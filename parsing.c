#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

#ifdef _WIN32
#include <string.h>

// Declare buffer for user input of size 2048
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

long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0 ) { return x + y; }
    if (strcmp(op, "-") == 0 ) { return x - y; }
    if (strcmp(op, "*") == 0 ) { return x * y; }
    if (strcmp(op, "/") == 0 ) { return x / y; }
    if (strcmp(op, "%") == 0 ) { return x % y; }
    if (strcmp(op, "^") == 0 ) {
        long result = x;
        for(int i = 1; i < y; i++) {
            result *= x;
        }
        return result;
    }
    return 0;
}

long eval(mpc_ast_t* t) {
    // if tagged as number, return it
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    // is an expression. operator is always second child
    char* op = t->children[1]->contents;

    // store value of third child in x
    long x = eval(t->children[2]);

    // iterate remaining children and combine
    for(int i = 3; strstr(t->children[i]->tag, "expr"); i++) {
        x = eval_op(x, op, eval(t->children[i]));
    }

    return x;
}

int main(int argc, char *argv[]) {
    // set up parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                  \
            number:   /-?[0-9]+/;                              \
            operator: '+' | '-' | '*' | '/' | '%' | '^';       \
            expr:     <number> | '(' <operator> <expr>+ ')';   \
            lispy:    /^/ <operator> <expr>+ /$/;              \
            ", Number, Operator, Expr, Lispy
    );

    // Print version and exit information
    puts("Lispy version 0.0.0.0.1");
    puts("Press Ctrl-c to Exit\n");

    // never ending while loop
    while(1) {
        // output to prompt and get input
        char* input = readline("Lispy> ");

        // add input to history
        add_history(input);

        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)) {
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefine and delete parsers
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}
