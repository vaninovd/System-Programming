#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
typedef int bool;
#define true 1
#define false 0

/* Reads a trace file produced by valgrind and an address marker file produced
 * by the program being traced. Outputs only the memory reference lines in
 * between the two markers
 */

int main(int argc, char **argv) {
    
    if(argc != 3) {
         fprintf(stderr, "Usage: %s tracefile markerfile\n", argv[0]);
         exit(1);
    }

    // Addresses should be stored in unsigned long variables
    // unsigned long start_marker, end_marker;
    FILE *marker_file = fopen(argv[2], "r");
    if(marker_file == NULL){
        printf("No such file!\n");
        return 1;
    }

    unsigned long int start_marker; 
    unsigned long int end_marker; 

    fscanf(marker_file, "%lx %lx\n", &start_marker, &end_marker); 	
    /* For printing output, use this exact formatting string where the
     * first conversion is for the type of memory reference, and the second
     * is the address
     */
    // printf("%c,%#lx\n", VARIABLES TO PRINT GO HERE);

    FILE *trace_file = fopen(argv[1], "r");
    if(trace_file == NULL){
        printf("No such file!\n");
        return 1;
    }

    char type;
    unsigned long int hex_address; 
    int size;
    bool go = false;
    while(fscanf(trace_file, " %c %lx,%d\n", &type, &hex_address, &size) != EOF){
	if(end_marker == hex_address){
		go = false;
	}
	if(go){
		printf("%c,%#lx\n", type, hex_address);
	}
	if(start_marker == hex_address){
		go = true;	
	}
    }

    return 0;
}
