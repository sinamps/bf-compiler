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
