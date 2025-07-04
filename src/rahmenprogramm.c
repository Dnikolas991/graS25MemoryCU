#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "rahmenprogramm.h"

#define DEFAULT_CYCLES 100000
#define DEFAULT_LATENCY_ROM 1
#define DEFAULT_ROM_SIZE 0x100000
#define DEFAULT_BLOCK_SIZE 0x1000 // Both examples from pdf data

void print_help(const char* prog_name) {
    fprintf(stderr, "Usage: %s [options] <input_file>\n\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --cycles <num>           Number of cycles (default: %d)\n", DEFAULT_CYCLES);
    fprintf(stderr, "  --tf <string>            Path to the Tracefile\n");
    fprintf(stderr, "  --latency-rom <num>      Latency of ROM (default: %d)\n", DEFAULT_LATENCY_ROM);
    fprintf(stderr, "  --rom-size <num>         Size of ROM in Bytes (default: %#x)\n", DEFAULT_ROM_SIZE);
    fprintf(stderr, "  --block-size <num>       Size of Memory-block in Bytes (default: %#x)\n", DEFAULT_BLOCK_SIZE);
    fprintf(stderr, "  --rom-content <string>   Path to the content of ROM\n");
    fprintf(stderr, "  --help                   Show this help message\n");
}

int parse_arguments(int argc, char* argv[], MemConfig *config) {

    int opt;
    int option_index=0;
    
    static struct option long_options[] = {
        {"cycles", required_argument, 0, 'c'},
        {"tf", required_argument, 0, 't'},
        {"latency-rom", required_argument, 0, 'l'},
        {"rom-size", required_argument, 0, 's'},
        {"block-size", required_argument, 0, 'b'},
        {"rom-content", required_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Set default values
    config->cycles=DEFAULT_CYCLES;
    config->block_size=DEFAULT_BLOCK_SIZE;
    config->inputfile=NULL;
    config->latency_rom=DEFAULT_LATENCY_ROM;
    config->rom_content_file=NULL;
    config->rom_size=DEFAULT_ROM_SIZE;
    config->tracefile=NULL;
    
    while ((opt = getopt_long(argc, argv, "c:t:l:s:b:r:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config->cycles=atoi(optarg);
                break;
            case 't':
                config->tracefile=atoi(optarg);
                break;
            case 'l':
                config->latency_rom=atoi(optarg);
                break;
            case 's':
                config->rom_size=atoi(optarg);
                break;
            case 'b':
                config->block_size=atoi(optarg);
                break;
            case 'r':
                config->rom_content_file=atoi(optarg);
                break;
            case 'h':
                print_help(argv[0]);
                exit(0);
            default:
                print_help(argv[0]);
                exit(0);
        }
    }
    
    if (optind < argc) {
        config->inputfile=argv[optind];
        if(strlen(config->inputfile)<4 || strcmp(config->inputfile + strlen(config->inputfile)-4, ".csv")!=0){
            // Check whether the name or type of inputfile is valid
            fprintf(stderr, "Input file invalid!\n");
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Input file: %s\n", config->inputfile);
    } else {
        fprintf(stderr, "Input file required!\n");
        return 1;
    }
    
    return 0;
}

int parse_number(const char* str, uint32_t* value) {
    // Transform values Hexdezimal -> Dezimal
    char* endptr;
    long val;
    if(strncmp(str, "0x", 2)==0 || strncmp(str, "0X", 2)==0){
        val=strtol(str, &endptr, 16);
    }else{
        val=strtol(str, &endptr, 10);
    }
    if (endptr==str || *endptr!='\0' || val<0 || val>UINT32_MAX) {
        return 1;
    }
    *value=(uint32_t)val;
    return 0;
}

uint32_t* load_rom_content(const char* filename, uint32_t rom_size, uint32_t* actual_size) {
    FILE* file=fopen(filename, "r");
    if(!file){
        fprintf(stderr, "Cannot open ROM-content file: %s\n", filename);
        return NULL;
    }
    uint32_t max_entries=rom_size / sizeof(uint32_t);
    uint32_t* content=calloc(max_entries, sizeof(uint32_t));
    if(!content){
        fclose(file);
        return NULL;
    }
    char line[256];
    uint32_t count=0;
    while(fgets(line, sizeof(line), file) && count<max_entries) {
        uint32_t value;
        if (parse_number(line, &value)==0) {
            content[count++]=value;
        }
    }
    if(count>max_entries){
        fprintf(stderr, "Error: Content in ROM has greater size than ROM size!\n");
        free(content);
        fclose(file);
        return NULL;
    }
    *actual_size = count;
    fclose(file);
    return content;
}

int parse_csv_file(const char* filename, struct Request** requests, uint32_t* num_requests) {
    FILE* file=fopen(filename, "r");
    if(!file){
        fprintf(stderr, "Cannot open CSV file: %s\n", filename);
        return NULL;
    }
    char line[256];
    if(!fgets(line, sizeof(line), file)){
        fprintf(stderr, "Error: CSV file empty!\n");
        fclose(file);
        return 1;
    }
    
    uint32_t line_count=0;
    while(fgets(line, sizeof(line), file)){ // Count total number of rows
        line_count++;
    }
    rewind(file);
    fgets(line, sizeof(line), file); // Skip the table head (First row)
    
    *requests=malloc(line_count * sizeof(struct Request));
    if(!*requests){
        fclose(file);
        return 1;
    }
    *num_requests = 0;
    while(fgets(line, sizeof(line), file) && *num_requests<line_count){
        char type, wide_char;
        uint32_t addr, data, user;
        if (sscanf(line, "%c,%x,%x,%d,%c", &type, &addr, &data, &user, &wide_char)>=4) {
            (*requests)[*num_requests].addr = addr;
            (*requests)[*num_requests].data = data;
            (*requests)[*num_requests].w = (type == 'W' || type == 'w') ? 1 : 0;
            (*requests)[*num_requests].user = (uint8_t)user;
            (*requests)[*num_requests].wide = (wide_char == 'T' || wide_char == 't') ? 1 : 0;
            (*num_requests)++;
        }
    }
    fclose(file);
    return 0;
}

/*
struct Result run_simulation{
    //TODO (Wait for SystemC)
};
*/
