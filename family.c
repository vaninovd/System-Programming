#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "family.h"

/* Number of word pointers allocated for a new family.
   This is also the number of word pointers added to a family
   using realloc, when the family is full.
*/
static int family_increment = 0;


/* Set family_increment to size, and initialize random number generator.
   The random number generator is used to select a random word from a family.
   This function should be called exactly once, on startup.
*/
void init_family(int size) {
    family_increment = size;
    srand(time(0));
}


/* Given a pointer to the head of a linked list of Family nodes,
   print each family's signature and words.

   Do not modify this function. It will be used for marking.
*/
void print_families(Family* fam_list) {
    int i;
    Family *fam = fam_list;
    
    while (fam) {
        printf("***Family signature: %s Num words: %d\n",
               fam->signature, fam->num_words);
        for(i = 0; i < fam->num_words; i++) {
            printf("     %s\n", fam->word_ptrs[i]);
        }
        printf("\n");
        fam = fam->next;
    }
}


/* Return a pointer to a new family whose signature is 
   a copy of str. Initialize word_ptrs to point to 
   family_increment+1 pointers, numwords to 0, 
   maxwords to family_increment, and next to NULL.
*/
Family *new_family(char *str) {
    Family *new_fam = malloc(sizeof(struct fam));
    if (!new_fam) {
            perror("malloc");
            exit(1);
        }
	
    new_fam->signature = malloc(sizeof(char) * (strlen(str) +1));
    if (!new_fam->signature) {
            perror("malloc");
            exit(1);
        }
	
    strcpy(new_fam->signature, str);
    new_fam->word_ptrs = malloc(sizeof(char*) * (family_increment+1));
    if (!new_fam->word_ptrs) {
            perror("malloc");
            exit(1);
        }	
    new_fam->num_words = 0;
    new_fam->max_words = family_increment;
    new_fam->next = NULL; 

    return new_fam;
}


/* Add word to the next free slot fam->word_ptrs.
   If fam->word_ptrs is full, first use realloc to allocate family_increment
   more pointers and then add the new pointer.
*/
void add_word_to_family(Family *fam, char *word) {
    if (fam->num_words >= fam->max_words){
        fam->word_ptrs = realloc(fam->word_ptrs, (fam->max_words + family_increment + 1) * sizeof(char*));
        if (!fam->word_ptrs) {
            perror("realloc");
            exit(1);
        }
        fam->max_words = fam->max_words + family_increment;
        fam->word_ptrs[fam->max_words] = NULL;
    }

    fam->word_ptrs[fam->num_words] = word; 
    fam->num_words++;

    return;
}


/* Return a pointer to the family whose signature is sig;
   if there is no such family, return NULL.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_family(Family *fam_list, char *sig) {
    Family *curr = fam_list;
    while(curr != NULL){
        int result = strcmp(curr->signature, sig);
        if(result == 0){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}


/* Return a pointer to the family in the list with the most words;
   if the list is empty, return NULL. If multiple families have the most words,
   return a pointer to any of them.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_biggest_family(Family *fam_list) {
    Family *biggest = fam_list;
    Family *curr = fam_list->next;
    while(curr != NULL){
        if(curr->num_words > biggest->num_words){
            biggest = curr;
        }
        
        curr = curr->next;
    }
    return biggest;
}


/* Deallocate all memory rooted in the List pointed to by fam_list. */
void deallocate_families(Family *fam_list) {
    Family *curr = fam_list;
    while(curr != NULL){
        Family *next = curr->next;
        free(curr->signature);
        free(curr->word_ptrs);
        free(curr);
        curr = next;
    }
    free(curr);
    return;
}

/* Generate the signature of the word, using the letter. Then modify the empty 
   empty array accordingly
*/ 
void generate_signature(char *arr, char *word, char letter){
    int i;
    for(i = 0; i < strlen(word); i++){
        if(word[i] != letter){
            arr[i] = '-';
        }
        else{arr[i] = word[i];}
    }
    arr[strlen(word)] = '\0';
}

/* Generate and return a linked list of all families using words pointed to
   by word_list, using letter to partition the words.

   Implementation tips: To decide the family in which each word belongs, you
   will need to generate the signature of each word. Create only the families
   that have at least one word from the current word_list.
*/
Family *generate_families(char **word_list, char letter) {
    int i = 0;
    Family *famhead = NULL; 
    while(word_list[i] != NULL){
        char signature[strlen(word_list[i]) + 1];
        generate_signature(signature, word_list[i], letter); 
        if (find_family(famhead, signature) != NULL){
            add_word_to_family(find_family(famhead, signature), word_list[i]);
        }
        else{
            Family *new_famhead = new_family(signature);
            add_word_to_family(new_famhead, word_list[i]);
            new_famhead->next = famhead;
            famhead = new_famhead;
        }

        i++;
    }
    return famhead;
}

/* Return the signature of the family pointed to by fam. */
char *get_family_signature(Family *fam) {
    return fam->signature;
}


/* Return a pointer to word pointers, each of which
   points to a word in fam. These pointers should not be the same
   as those used by fam->word_ptrs (i.e. they should be independently malloc'd),
   because fam->word_ptrs can move during a realloc.
   As with fam->word_ptrs, the final pointer should be NULL.
*/
char **get_new_word_list(Family *fam) {
    char **new_words = malloc(sizeof(char*) * (fam->num_words +1));
    int i = 0;
    while(i < fam->num_words){
        new_words[i] = fam->word_ptrs[i];
        i++;
    }
    new_words[fam->num_words] = NULL;

    return new_words;
}


/* Return a pointer to a random word from fam. 
   Use rand (man 3 rand) to generate random integers.
*/
char *get_random_word_from_family(Family *fam) {
    time_t t;
    srand((unsigned) time(&t));
    int i = rand() % fam->num_words;
    return fam->word_ptrs[i];
}
