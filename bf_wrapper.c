// /*
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <stddef.h>  // For NULL definition

// Declare the assembly Brainfuck main function
extern void bf_main(char *tape);

int main() {
    // Allocate memory for the Brainfuck tape, aligned to 16 bytes for SSE instructions
    char *tape;
    if (posix_memalign((void **)&tape, 16, 50000 * sizeof(char)) != 0) {
        perror("Failed to allocate aligned memory for the tape");
        return 1;
    }

    // Ensure that tape allocation was successful
    if (tape == NULL) {
        perror("Failed to allocate memory for the tape");
        return 1;
    }
    // After allocating the tape
    memset(tape, 0, 50000 * sizeof(char));

    // Call the Brainfuck main function implemented in assembly
    bf_main(tape);

    // Free the allocated tape memory
    // free(tape);
    return 0;
}
// */

/*
#include <stdio.h>
#include <stdlib.h>

// Declare the assembly Brainfuck main function
extern void bf_main(char *tape);

int main() {
    // Allocate memory for the Brainfuck tape
    char *tape = (char *)calloc(50000, sizeof(char));
    if (tape == NULL) {
        perror("Failed to allocate memory for the tape");
        return 1;
    }

    // Call the Brainfuck main function implemented in assembly
    bf_main(tape);

    // Free the allocated tape memory
    free(tape);
    return 0;
}
*/