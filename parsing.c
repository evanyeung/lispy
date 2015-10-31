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

enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// new lval struct. The result of an eval.
typedef struct {
    int type;
    long num;
    int err;
} lval;

// create new number type lval
lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}
// create new error type lval
lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(lval v) {
    switch(v.type) {
        case LVAL_NUM:
            printf("%li", v.num);
            break;
        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: Division by zero!");
            }
            else if (v.err == LERR_BAD_OP) {
                printf("Error: Invalid operator!");
            }
            else if (v.err == LERR_BAD_NUM) {
                printf("Error: Invalid number!");
            }
            break;
    }
}

void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

lval eval_op(lval x, char* op, lval y) {
    // if either values is an error, return it
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0 ) { return lval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0 ) { return lval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0 ) { return lval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0 ) {
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }
    if (strcmp(op, "%") == 0 ) {
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num % y.num);
    }
    if (strcmp(op, "^") == 0 ) {
        long result = x.num;
        for(int i = 1; i < y.num; i++) {
            result *= x.num;
        }
        return lval_num(result);
    }
    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    // if tagged as number, return it
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // is an expression. operator is always second child
    char* op = t->children[1]->contents;

    // store value of third child in x
    lval x = eval(t->children[2]);

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
            lval result = eval(r.output);
            lval_println(result);
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
