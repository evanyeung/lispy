#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

// readline and history are default in windows cmdline
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

// lval types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

// new lval struct. The result of an eval.
typedef struct lval {
    int type;
    long num;
    char* err;
    char* sym;
    // count and pointer to a list of lval*
    int count;
    struct lval** cell;
} lval;

// construct pointer to new number lval
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}
// construct pointer to new error lval
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}
// construct pointer to new symbol lval
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}
// construct pointer to new sexpression lval
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch(v->type) {
        // do nothing special for num
        case LVAL_NUM: break;

        // free string data for error or sym
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        // recursively free all elements inside sexpr
        case LVAL_SEXPR:
           for(int i = 0; i < v->count; i++) {
               lval_del(v->cell[i]);
           }
           // also free memory for pointers
           free(v->cell);
           break;
    }
    // free memory for lval struct itself
    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

void lval_print(lval* v); // used in lval_expr_print
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for(int i = 0; i < v->count; i++) {
        // print value in cell
        lval_print(v->cell[i]);
        if (i < v->count - 1) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_NUM:   printf("%li", v->num);         break;
        case LVAL_ERR:   printf("Error: %s\n", v->err); break;
        case LVAL_SYM:   printf("%s", v->sym);          break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')');  break;
    }
}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
        ? lval_num(x)
        : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    // if symbol or number, create lval of that type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // if root (>) or sexpr then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) {
        x = lval_sexpr();
    }

    // fill list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

/*
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
*/

int main(int argc, char *argv[]) {
    // set up parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                          \
            number: /-?[0-9]+/;                        \
            symbol: '+' | '-' | '*' | '/' | '%' | '^'; \
            sexpr:  '(' <expr>* ')';                   \
            expr:   <number> | <symbol> | <sexpr>;     \
            lispy:  /^/ <expr>* /$/;                   \
            ", Number, Symbol, Sexpr, Expr, Lispy
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
            lval* x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefine and delete parsers
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
    return 0;
}
