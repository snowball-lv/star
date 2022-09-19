#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <star/mem.h>
#include <star/util.h>
#include <star/star.h>

#define TOKS(T) T(NONE) T(EOF) T(NUM) T(NIL) T(STR) \
        T(ADD) T(SUB) T(MUL) T(DIV) \
        T(DOT) \
        T(COMMA) \
        T(LPAREN) T(RPAREN) \
        T(LBRACE) T(RBRACE) \
        T(ASSIGN) \
        T(ID) \
        T(VAR) T(PRINT) T(IF) T(ELSE) T(WHILE) T(FUNC) \
        T(LT) \
        T(RET)

enum {
#define T(name) T_ ## name,
    TOKS(T)
#undef T
};

typedef struct {
    char type;
    char *_start;
    int len;
    char *str;
} Tok;

typedef struct {
    Tok name;
    int depth;
} Local;

typedef struct Function Function;
struct Function {
    ObjFunc *obj;
    Local *locals;
    int nlocals;
    int depth;
    Function *parent;
};

typedef struct {
    char *src;
    Tok prev;
    Tok next;
    Tab *kws;
    char **tokstrs;
    int ntokstrs;
    Function *func;
} Parser;

static const char *tname(int type) {
    switch (type) {
#define T(name) case T_ ## name: return #name;
    TOKS(T)
#undef T
    }
    printf("*** unknown token type %i\n", type);
    exit(1);
}

static Tok nexttok(Parser *p) {
    char *start;
    while (isspace(*p->src))
        p->src++;
    start = p->src;
    switch (*start) {
    case 0: return (Tok){T_EOF};
    case '+': p->src++; return (Tok){T_ADD, start, 1};
    case '-': p->src++; return (Tok){T_SUB, start, 1};
    case '*': p->src++; return (Tok){T_MUL, start, 1};
    case '/': p->src++; return (Tok){T_DIV, start, 1};
    case '(': p->src++; return (Tok){T_LPAREN, start, 1};
    case ')': p->src++; return (Tok){T_RPAREN, start, 1};
    case '{': p->src++; return (Tok){T_LBRACE, start, 1};
    case '}': p->src++; return (Tok){T_RBRACE, start, 1};
    case '=': p->src++; return (Tok){T_ASSIGN, start, 1};
    case '<': p->src++; return (Tok){T_LT, start, 1};
    case '.': p->src++; return (Tok){T_DOT, start, 1};
    case ',': p->src++; return (Tok){T_COMMA, start, 1};
    case '"': p->src++; goto str;
    }
    if (isdigit(*start)) goto num;
    if (isalpha(*start) || *start == '_') goto id;
    printf("*** unexpected char %c\n", *start);
    exit(1);
num:
    while (isdigit(*p->src))
        p->src++;
    return (Tok){T_NUM, start, p->src - start};
id:
    while (isalnum(*p->src) || *p->src == '_')
        p->src++;
    return (Tok){T_ID, start, p->src - start};
str:
    while (*p->src && *p->src != '"')
        p->src++;
    if (!*p->src) {
        printf("*** untermianted string\n");
        exit(1);
    }
    p->src++;
    return (Tok){T_STR, start + 1, p->src - start - 2};
}

static void intern(Parser *p, Tok *t) {
    if (!t->len) t->_start = "";
    for (int i = 0; i < p->ntokstrs; i++) {
        char *str = p->tokstrs[i];
        if (strlen(str) != t->len) continue;
        if (strncmp(str, t->_start, t->len) != 0) continue;
        t->str = str;
        t->_start = 0;
        return;
    }
    t->str = xmalloc(t->len + 1);
    memcpy(t->str, t->_start, t->len);
    t->str[t->len] = 0;
    t->_start = 0;
    p->ntokstrs++;
    p->tokstrs = arraygrow(p->tokstrs, p->ntokstrs);
    p->tokstrs[p->ntokstrs - 1] = t->str;
    // printf("new str %s\n", t->str);

}

static void advance(Parser *p) {
    p->prev = p->next;
    p->next = nexttok(p);
    intern(p, &p->next);
    if (p->next.type == T_ID && tabhas(p->kws, p->next.str))
        p->next.type = tabget(p->kws, p->next.str);
}

static int match(Parser *p, int type) {
    if (p->next.type != type) return 0;
    advance(p);
    return 1;
}

static void expect(Parser *p, int type) {
    if (match(p, type)) return;
    printf("*** expected %s got %s:%s\n",
            tname(type), tname(p->next.type), p->next.str);
    exit(1);
}

static Chunk *curchunk(Parser *p) {
    return p->func->obj->chunk;
}

static int getlocal(Parser *p, Tok name, int depth) {
    Function *fn = p->func;
    for (int i = fn->nlocals - 1; i >= 0; i--) {
        Local *l = &fn->locals[i];
        if (l->depth < depth) return -1;
        if (strcmp(l->name.str, name.str) != 0) continue;
        return i;
    }
    return -1;
}

static int haslocal(Parser *p, Tok name) {
    return getlocal(p, name, p->func->depth) != -1;
}

static void resolve(Parser *p, Tok name) {
    int slot = getlocal(p, name, 0);
    if (slot == -1) {
        printf("*** undeclared identifier %s\n", name.str);
        exit(1);
    }
    emitgetlocal(curchunk(p), slot);
}

static void expr(Parser *p);

static void object(Parser *p) {
    emitnew(curchunk(p));
}

static void primary(Parser *p) {
    if (match(p, T_STR)) {
        emitcons(curchunk(p), addcons(curchunk(p), strval(p->prev.str)));
    }
    else if (match(p, T_NIL)) {
        emitnil(curchunk(p));
    }
    else if (match(p, T_NUM)) {
        emitcons(curchunk(p), addcons(curchunk(p), numval(atof(p->prev.str))));
    }
    else if (match(p, T_ID)) {
        resolve(p, p->prev);
    }
    else if (match(p, T_LPAREN)) {
        expr(p);
        expect(p, T_RPAREN);
    }
    else if (match(p, T_LBRACE)) {
        object(p);
        expect(p, T_RBRACE);
    }
    else {
        printf("*** unexpected token %s:%s\n", tname(p->next.type), p->next.str);
        exit(1);
    }
}

static void callexpr(Parser *p) {
    primary(p);
    while (match(p, T_LPAREN)) {
        int nargs = 0;
        while (!match(p, T_RPAREN)) {
            expr(p);
            match(p, T_COMMA); // optional
            nargs++;
        }
        emitcall(curchunk(p), nargs);
    }
}

static void dotexpr(Parser *p) {
    callexpr(p);
    while (match(p, T_DOT)) {
        expect(p, T_ID);
        int name = addcons(curchunk(p), strval(p->prev.str));
        emitgetfield(curchunk(p), name);
    }
}

static void unary(Parser *p) {
    if (match(p, T_SUB)) {
        unary(p);
        emitneg(curchunk(p));
    }
    else {
        dotexpr(p);
    }
}

static void factor(Parser *p) {
    unary(p);
}

static void term(Parser *p) {
    factor(p);
    while (match(p, T_MUL) || match(p, T_DIV)) {
        int op = p->prev.type;
        factor(p);
        if (op == T_MUL) emitmul(curchunk(p));
        else if (op == T_DIV) emitdiv(curchunk(p));
    }
}

static void mathexpr(Parser *p) {
    term(p);
    while (match(p, T_ADD) || match(p, T_SUB)) {
        int op = p->prev.type;
        term(p);
        if (op == T_ADD) emitadd(curchunk(p));
        else if (op == T_SUB) emitsub(curchunk(p));
    }
}

static void relexpr(Parser *p) {
    mathexpr(p);
    while (match(p, T_LT)) {
        mathexpr(p);
        emitlt(curchunk(p));
    }
}

static void assignment(Parser *p) {
    relexpr(p);
    if (match(p, T_ASSIGN)) {
        int getop = getip(curchunk(p)) - 1;
        assignment(p);
        fixassign(curchunk(p), getop);
    }
}

static void expr(Parser *p) {
    assignment(p);
}

static void stm(Parser *p);

static void block(Parser *p) {
    int nlocals = p->func->nlocals;
    p->func->depth++;
    while (!match(p, T_RBRACE))
        stm(p);
    p->func->depth--;
    while (p->func->nlocals > nlocals) {
        p->func->nlocals--;
        emitpop(curchunk(p));
    }
}

static void definelocal(Parser *p, Tok name) {
    Function *fn = p->func;
    int idx = fn->nlocals++;
    fn->locals = arraygrow(fn->locals, fn->nlocals);
    fn->locals[idx].name = name;
    fn->locals[idx].depth = p->func->depth;
}

static void ifstm(Parser *p) {
    expect(p, T_LPAREN);
    expr(p);
    expect(p, T_RPAREN);
    emitnot(curchunk(p));
    int elsejmp = emitcjmp(curchunk(p));
    stm(p);
    int endjmp = emitjmp(curchunk(p));
    patchjmp(curchunk(p), elsejmp);
    if (match(p, T_ELSE))
        stm(p);
    patchjmp(curchunk(p), endjmp);
}

static void whilestm(Parser *p) {
    int ip = getip(curchunk(p));
    expect(p, T_LPAREN);
    expr(p);
    expect(p, T_RPAREN);
    emitnot(curchunk(p));
    int endjmp = emitcjmp(curchunk(p));
    stm(p);
    emitjmp2(curchunk(p), ip - getip(curchunk(p)));
    patchjmp(curchunk(p), endjmp);
}

static void stm(Parser *p) {
    if (match(p, T_RET)) {
        expr(p);
        emitret(curchunk(p));
    }
    else if (match(p, T_IF)) {
        ifstm(p);
    }
    else if (match(p, T_WHILE)) {
        whilestm(p);
    }
    else if (match(p, T_LBRACE)) {
        block(p);
    }
    else if (match(p, T_VAR)) {
        expect(p, T_ID);
        Tok name = p->prev;
        if (haslocal(p, name)) {
            printf("*** variable %s already declared\n", name.str);
            exit(1);
        }
        if (match(p, T_ASSIGN))
            expr(p);
        else
            emitnil(curchunk(p));
        definelocal(p, name);
    }
    else if (match(p, T_PRINT)) {
        expr(p);
        emitprint(curchunk(p));
    }
    else if (match(p, T_FUNC)) {
        expect(p, T_ID);
        Tok name = p->prev;
        if (haslocal(p, name)) {
            printf("*** variable %s already declared\n", name.str);
            exit(1);
        }
        definelocal(p, name);
        Function child = {0};
        child.obj = newfunc();
        child.locals = newarray(sizeof(Local));
        child.parent = p->func;
        p->func = &child;
        expect(p, T_LPAREN);
        int nparams = 0;
        while (!match(p, T_RPAREN)) {
            expect(p, T_ID);
            Tok name = p->prev;
            if (haslocal(p, name)) {
                printf("*** parameter %s already declared\n", name.str);
                exit(1);
            }
            definelocal(p, name);
            nparams++;
            match(p, T_COMMA); // optional
        }
        p->func->obj->arity = nparams;
        expect(p, T_LBRACE);
        block(p);
        emitnil(curchunk(p));
        emitret(curchunk(p));
        printchunk(curchunk(p));
        p->func = p->func->parent;
        emitcons(curchunk(p), addcons(curchunk(p), OBJVAL(child.obj)));
    }
    else {
        expr(p);
        emitpop(curchunk(p));
    }
}

static ObjFunc *parsefile(Parser *p) {
    Function main = {0};
    main.obj = newfunc();
    main.locals = newarray(sizeof(Local));
    p->func = &main;
    advance(p);
    while (!match(p, T_EOF))
        stm(p);
    while (main.nlocals--)
        emitpop(curchunk(p));
    emitret(curchunk(p));
    return main.obj;
}

static void definekws(Parser *p) {
    tabset(p->kws, "var", T_VAR);
    tabset(p->kws, "print", T_PRINT);
    tabset(p->kws, "if", T_IF);
    tabset(p->kws, "else", T_ELSE);
    tabset(p->kws, "while", T_WHILE);
    tabset(p->kws, "nil", T_NIL);
    tabset(p->kws, "function", T_FUNC);
    tabset(p->kws, "return", T_RET);
}

ObjFunc *compile(char *src) {
    Parser p = {0};
    p.src = src;
    p.kws = newtab();
    p.tokstrs = newarray(sizeof(char *));
    definekws(&p);
    ObjFunc *func = parsefile(&p);
    freetab(p.kws);
    for (int i = 0; i < p.ntokstrs; i++)
        xfree(p.tokstrs[i]);
    freearray(p.tokstrs);
    return func;
}
