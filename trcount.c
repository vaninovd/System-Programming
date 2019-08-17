#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Constants that determine that address ranges of different memory regions

#define GLOBALS_START 0x400000
#define GLOBALS_END   0x700000
#define HEAP_START   0x4000000
#define HEAP_END     0x8000000
#define STACK_START 0xfff000000

int main(int argc, char **argv) {
    
    FILE *fp = NULL;

    if(argc == 1) {
        fp = stdin;

    } else if(argc == 2) {
        fp = fopen(argv[1], "r");
        if(fp == NULL){
            perror("fopen");
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: %s [tracefile]\n", argv[0]);
        exit(1);
    }

    /* Complete the implementation */


    /* Use these print statements to print the ouput. It is important that 
     * the output match precisely for testing purposes.
     * Fill in the relevant variables in each print statement.
     * The print statements are commented out so that the program compiles.  
     * Uncomment them as you get each piece working.
     */
    /*
    printf("Reference Counts by Type:\n");
    printf("    Instructions: %d\n", );
    printf("    Modifications: %d\n", );
    printf("    Loads: %d\n", );
    printf("    Stores: %d\n", );
    printf("Data Reference Counts by Location:\n");
    printf("    Globals: %d\n", );
    printf("    Heap: %d\n", );
    printf("    Stack: %d\n", );
    */
    int num_instructions = 0;
    int num_modifications = 0;
    int num_loads = 0; 
    int num_stores = 0;
    int num_global = 0;
    int num_heap = 0;
    int num_stack = 0;

    char type;
    unsigned long int hex_address; 
    while(fscanf(fp, "%c,%lx\n", &type, &hex_address) != EOF){
        if (type == 'I'){
            num_instructions++;
        }
        else if (type == 'M'){
            num_modifications++;
        }
        else if (type == 'L'){
            num_loads++;
        }
        else if (type == 'S'){
            num_stores++;
        }
        if(type != 'I'){
            if (GLOBALS_START < hex_address && hex_address < GLOBALS_END){
                num_global++;
            }
            else if (HEAP_START < hex_address && hex_address < HEAP_END){
                num_heap++;
            }
            else if (STACK_START < hex_address){
                num_stack++;
            }
        }
    }

    printf("Reference Counts by Type:\n");
    printf("    Instructions: %d\n", num_instructions);
    printf("    Modifications: %d\n", num_modifications);
    printf("    Loads: %d\n", num_loads);
    printf("    Stores: %d\n", num_stores);
    printf("Data Reference Counts by Location:\n");
    printf("    Globals: %d\n", num_global);
    printf("    Heap: %d\n", num_heap);
    printf("    Stack: %d\n", num_stack);

    return 0;
}
