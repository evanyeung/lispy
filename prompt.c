#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

// fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy(strlen(cpy) - 1, '\0');
    return cpy;
}

// automatically done in windows cmdline
void add_history(char* unused) {}

#else
#include <editline/readline.h>
#endif

// Declare buffer for user input of size 2048
/*static char input[2048];*/

int main(int argc, char *argv[]) {
    // Print version and exit information
    puts("Lispy version 0.0.0.0.1");
    puts("Press Ctrl-c to Exit\n");

    // never ending while loop
    while(1) {

        // output to prompt and get input
        char* input = readline("Lispy> ");

        // add input to history
        add_history(input);

        // echo back to user
        printf("No you're a %s\n", input);

        free(input);
    }

    return 0;
}
