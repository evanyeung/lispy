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

// forward declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/**
 * lval definitions
 */
// lval types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

char* ltype_name(int t) {
    switch (t) {
        case LVAL_NUM:   return "Number";
        case LVAL_ERR:   return "Error";
        case LVAL_SYM:   return "Symbol";
        case LVAL_FUN:   return "Function";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default:         return "Unknown";
    }
}

// function pointer for builtin functions
typedef lval*(*lbuiltin)(lenv*, lval*);

#define LASSERT(args, cond, fmt, ...) \
    if (!cond) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_NUM_ARGS(func_name, args, num_args) \
    if (args->count != num_args) { \
        lval* err = lval_err("Function '%s' passed incorrect number of " \
                             "arguments. Got %i, expected %i.", \
                             func_name, args->count, num_args); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_ARG_TYPE(func_name, args, arg_num, arg_type) \
    if (args->cell[arg_num]->type != arg_type) { \
        lval* err = lval_err("Function '%s' passed incorrect type for " \
                             "argument %i. Got %s, expected %s.", \
                             func_name, \
                             arg_num, \
                             ltype_name(args->cell[arg_num]->type), \
                             ltype_name(arg_type)); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_NOT_EMPTY(func_name, args, arg_num) \
    if (args->cell[arg_num]->count == 0) { \
        lval* err = lval_err("Function '%s' passed {} for argument %i", \
                             func_name, arg_num); \
        lval_del(args); \
        return err; \
    }

// new lval struct. The result of an eval.
struct lval {
    int type;

    // basic
    long num;     // for numeric values
    char* err;    // for error types
    char* sym;    // for symbols

    // function
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    // expression
    int count;
    struct lval** cell; // list of lvals
};

/**
 * lval constructor
 */
// construct pointer to new number lval
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}
// construct pointer to new error lval
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    // create a va_list and initialize it
    va_list va;
    va_start(va, fmt);

    // allocate 512 bytes of space (hopefully the error message is shorter!)
    v->err = malloc(512);

    // printf the error string with a maximum of 511 characters
    vsnprintf(v->err, 511, fmt, va);
    v->err = realloc(v->err, strlen(v->err) + 1); // realloc to actual length

    // cleanup the va list
    va_end(va);

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
// construct pointer to new function lval
lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
}
// construct pointer to new lambda function lval
lenv* lenv_new(void);
lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    v->builtin = NULL;
    v->env = lenv_new();
    v->formals = formals;
    v->body = body;
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
// construct pointer to new qexpression lval
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/**
 * lval utility
 */
void lenv_del(lenv* e);
void lval_del(lval* v) {
    switch(v->type) {
        // do nothing special for num
        case LVAL_NUM: break;

        // free string data for error or sym
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        // free env and data if not builtin
        case LVAL_FUN:
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;

        // recursively free all elements inside sexpr/qexpr
        case LVAL_SEXPR:
        case LVAL_QEXPR:
           for (int i = 0; i < v->count; i++) {
               lval_del(v->cell[i]);
           }
           // also free memory for pointers
           free(v->cell);
           break;
    }
    // free memory for lval struct itself
    free(v);
}

lenv* lenv_copy(lenv* e);
lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        // copy numbers and functions directly
        case LVAL_NUM:
            x->num = v->num;
            break;
        case LVAL_FUN:
            if (v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;

        // copy strings for err and sym
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        // copy lists by recursively copying each sub expression
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * v->count);
            for (int i = 0; i < v->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

void lval_print(lval* v); // used in lval_expr_print
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
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
        case LVAL_FUN:
                         if (v->builtin) {
                             printf("<builtin>");
                         } else {
                             printf("\\"); lval_print(v->formals);
                             putchar(' '); lval_print(v->body); putchar(')');
                         }
                         break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')');  break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}');  break;
    }
}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_pop(lval* v, int i) {
    // find item at i (Note: v->cell is array of pointers to lvals)
    lval* x = v->cell[i];

    // shift memory after item "i" back
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));
    v->count--;

    // reallocate memory used
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);

    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

/**
 * Read lvals
 */
lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
        ? lval_num(x)
        : lval_err("Invalid number '%s'.", t->contents);
}

lval* lval_read(mpc_ast_t* t) {
    // if symbol or number, create lval of that type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // if root (>) or sexpr then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

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

/**
 * lisp environment
 */

// new lenv struct
struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        free(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

// get value for symbol of k in lenv
lval* lenv_get(lenv* e, lval* k) {
    // iterate over items in the environment
    for (int i = 0; i < e->count; i++) {
        // if stored symbol matches symbol of k, return copy of value
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    // if no symbol k->sym in lenv, check parent env, otherwise error
    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("Symbol '%s' not defined.", k->sym);
    }
}

// put new value v for symbol k into lenv
void lenv_put(lenv* e, lval* k, lval* v) {
    // iterate over items in environment to see if it already exists
    for (int i = 0; i < e->count; i++) {
        // change value for symbol if it exists
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // if no existing entry, allocate space for new variable
    e->count++;
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);

    // copy symbol and lval into new locations
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
    e->vals[e->count - 1] = lval_copy(v);
}

// put new value in global scope
void lenv_def(lenv* e, lval* k, lval* v) {
    // iterate until no parent
    while (e->par) { e = e->par; }
    lenv_put(e, k, v);
}

/**
 * builtin functions
 */

lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM_ARGS("head", a, 1);
    LASSERT_ARG_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);

    lval* v = lval_take(a, 0); // take the first arg (a qexpr)
    while(v->count > 1) {
        lval_del(lval_pop(v, 1)); // delete tail
    }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM_ARGS("tail", a, 1);
    LASSERT_ARG_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    lval* v = lval_take(a, 0); // take the first arg (a qexpr)
    lval_del(lval_pop(v, 0)); // delete head
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* lval_join(lval* x, lval* y) {
    while(y->count) {
        lval_add(x, lval_pop(y, 0));
    }
    lval_del(y);
    return x;
}

lval* lval_eval(lenv* e, lval* v);
lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM_ARGS("eval", a, 1);
    LASSERT_ARG_TYPE("eval", a, 0, LVAL_QEXPR);

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_ARG_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while(a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_ARG_TYPE(func, a, 0, LVAL_QEXPR);

    // first argument is symbol list
    lval* syms = a->cell[0];

    // ensure all elements of first list are symbols
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
                "Function '%s' cannot define non-symbol. "
                "Got %s, expected %s.",
                func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    // check correct number of symbols and values
    LASSERT(a, (syms->count == a->count - 1),
            "Function '%s' cannot define incorrect number of values to "
            "symbols. Got %i symbols and %i values.",
            func, syms->count, a->count - 1);

    // assign copies of values to symbols
    for (int i = 0; i < syms->count; i++) {
        // if 'def' define globally if 'put' define locally
        if (strcmp("def", func) == 0) {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }
        else if (strcmp("=", func) == 0){
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* builtin_lambda(lenv* e, lval* a) {
    LASSERT_NUM_ARGS("\\", a, 2);
    LASSERT_ARG_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_ARG_TYPE("\\", a, 1, LVAL_QEXPR);

    // ensure all elements of first list are symbols
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
                "Function '\' cannot define non-symbol. "
                "Got %s, expected %s.",
                ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    // pop args and return new lval_lambda
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    // ensure all arguments are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval* err = lval_err("Function '%s' passed incorrect type for "
                                 "argument %i. Got %s, expected %s.",
                                 op, i,
                                 ltype_name(a->cell[i]->type),
                                 ltype_name(LVAL_NUM));
            lval_del(a);
            return err;
        }
    }

    // pop the first element
    lval* x = lval_pop(a, 0);

    // if no arguments and sub the perform unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    // while there are still arguments remaining
    while(a->count > 0) {
        // pop the next element
        lval* y = lval_pop(a, 0);
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Function '/' caused division by zero.");
                break;
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Function '%%' caused division by zero.");
                break;
            }
            x->num = x->num % y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "%");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

// adds builtin functions to environment
void lenv_add_builtins(lenv* e) {
    // variable functions
    lenv_add_builtin(e, "\\",  builtin_lambda);
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=",   builtin_put);

    // list functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    // mathematical functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
}

/**
 * Evaluate lval
 */
lval* lval_eval(lenv* e, lval* v);
lval* lval_call(lenv* e, lval* f, lval* a);
lval* lval_eval_sexpr(lenv* e, lval* v) {
    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    // error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // empty expression
    if (v->count == 0) { return v; }
    // single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // ensure first element is function after evaluation
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err("Incorrect type for first element. "
                             "Got %s, expected %s.",
                             ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        lval_del(v);
        return err;
    }

    // call function with arguments
    lval* result = lval_call(e, f, v);
    lval_del(f);

    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}

lval* lval_call(lenv* e, lval* f, lval* a) {
    // if builtin, just call it
    if (f->builtin) { return f->builtin(e, a); }

    // record argument counts
    int given = a->count;
    int total = f->formals->count;

    // while arguments still remain to be processed
    while (a->count) {
        // if ran out of formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function passed too many arguments."
                            "Got %i, expected %i.",
                            given, total);
        }

        // put copy of next symbol and argument into function's environment
        lval* sym = lval_pop(f->formals, 0);

        // special case for variable number of arguments
        if (strcmp(sym->sym, "&") == 0) {
            // ensure "&" is followed by one other symbol
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid."
                                "Symbol '&' not followed by single symbol");
            }

            // next formal should be bound to remaining arguments
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));

            lval_del(sym);
            lval_del(nsym);
            break;
        }

        lval* val = lval_pop(a, 0);
        lenv_put(f->env, sym, val);

        lval_del(sym);
        lval_del(val);
    }

    // argument list is now bound so can be cleaned up
    lval_del(a);

    // if '&' remains in formal list bind to empty list
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
        // check to make sure '&' is not passed invalidly
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. Symbol '&' not "
                            "followed by a singe symbol.");
        }

        // pop and delete '&' symbol
        lval_del(lval_pop(f->formals, 0));

        // pop next symbol and create empty list
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        // bind to environment and delete
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);

    }

    // if all formals have been bound, evaluate
    if (f->formals->count == 0) {
        // set up parent env
        f->env->par = e;

        // evaluate the body
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        // otherwise return partially evaluated function
        return lval_copy(f);
    }
}

int main(int argc, char *argv[]) {
    // set up parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                   \
        number: /-?[0-9]+/;                                 \
        symbol: /[a-zA-Z0-9_+\\-*\\/%\\\\=<>!&]+/;           \
        sexpr:  '(' <expr>* ')';                            \
        qexpr:  '{' <expr>* '}';                            \
        expr:   <number> | <symbol> | <sexpr> | <qexpr>;    \
        lispy:  /^/ <expr>* /$/;                            \
        ", Number, Symbol, Sexpr, Qexpr, Expr, Lispy
    );

    // Print version and exit information
    puts("Lispy version 0.0.0.0.1");
    puts("Press Ctrl-c to Exit\n");

    // set up environment
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    // never ending while loop
    while(1) {
        // output to prompt and get input
        char* input = readline("Lispy> ");

        // add input to history
        add_history(input);

        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)) {
            /*mpc_ast_print(r.output);*/
            lval* x = lval_eval(e, lval_read(r.output));
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

    lenv_del(e);
    return 0;
}
