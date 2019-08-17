#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include "life_helpers.c"

void print_state(char *state, int size);
void update_state(char *state, int size);


int main(int argc, char **argv) {

    if (argc != 3) {
    	fprintf(stderr, "Usage: USAGE: life initial n\n");
    	return 1;
    }

    int size = strlen(argv[1]);
 
    // TODO: complete the main function
    print_state(argv[1], size);
    for(int i = 0; i < strtol(argv[2], NULL, 10) - 1; i++){
        update_state(argv[1], size);
        print_state(argv[1], size);
    }
    return 0;
}