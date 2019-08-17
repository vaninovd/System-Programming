#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_state(char arr[], int size){
    for(int i = 0; i < size; i++){
        printf("%c", arr[i]);
    }
    printf("\n");
}

void update_state(char arr[], int size){
    char arr_copy[size]; 

    for(int i = 0; i < size; i++){
        arr_copy[i] = arr[i];
    }

    for(int b = 1; b < size -1; b++){
        if((arr_copy[b-1] == '.'  && arr_copy[b+1] == '.') || (arr_copy[b-1] == 'X'  && arr_copy[b+1] == 'X')) {
            arr[b] = '.';
        }
        else if((arr_copy[b-1] == '.'  && arr_copy[b+1] == 'X') || (arr_copy[b-1] == 'X'  && arr_copy[b+1] == '.')){
            arr[b] = 'X';
        }
    }
}