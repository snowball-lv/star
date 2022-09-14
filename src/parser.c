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
        T(LPAREN) T(RPAREN) \
        T(LBRACE) T(RBRACE) \
        T(ASSIGN) \
        T(ID) \
        T(VAR) T(PRINT) T(IF) T(ELSE) T(WHILE) T(FUNC) \
        T(LT) \

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

typedef struct Scope Scope;
struct Scope {
    Tab *locals;
    int nlocals;
    Scope *parent;
};

typedef struct {
    char *src;
    Tok prev;
    Tok next;
    Chunk *c;
    Tab *kws;
    Scope *scope;
    int nlocals;
    char **tokstrs;
    int ntokstrs;
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

static void resolve(Parser *p, Tok id) {
    for (Scope *s = p->scope; s; s = s->parent) {
        if (!tabhas(s->locals, id.str)) continue;
        int slot = tabget(s->locals, id.str);
        emitgetlocal(p->c, slot);
        return;
    }
    printf("*** undeclared identifier %s\n", id.str);
    exit(1);
}

static void expr(Parser *p);

static void object(Parser *p) {
    emitnew(p->c);
}

static void primary(Parser *p) {
    if (match(p, T_STR)) {
        emitcons(p->c, addcons(p->c, strval(p->prev.str)));
    }
    else if (match(p, T_NIL)) {
        emitnil(p->c);
    }
    else if (match(p, T_NUM)) {
        emitcons(p->c, addcons(p->c, numval(atof(p->prev.str))));
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
    if (match(p, T_LPAREN))
        expect(p, T_RPAREN);
}

static void dotexpr(Parser *p) {
    callexpr(p);
    while (match(p, T_DOT)) {
        expect(p, T_ID);
        int name = addcons(p->c, strval(p->prev.str));
        emitgetfield(p->c, name);
    }
}

static void unary(Parser *p) {
    if (match(p, T_SUB)) {
        unary(p);
        emitneg(p->c);
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
        if (op == T_MUL) emitmul(p->c);
        else if (op == T_DIV) emitdiv(p->c);
    }
}

static void mathexpr(Parser *p) {
    term(p);
    while (match(p, T_ADD) || match(p, T_SUB)) {
        int op = p->prev.type;
        term(p);
        if (op == T_ADD) emitadd(p->c);
        else if (op == T_SUB) emitsub(p->c);
    }
}

static void relexpr(Parser *p) {
    mathexpr(p);
    while (match(p, T_LT)) {
        mathexpr(p);
        emitlt(p->c);
    }
}

static void assignment(Parser *p) {
    relexpr(p);
    if (match(p, T_ASSIGN)) {
        int getop = getip(p->c) - 1;
        assignment(p);
        fixassign(p->c, getop);
    }
}

static void expr(Parser *p) {
    assignment(p);
}

static void pushscope(Parser *p) {
    Scope *s = xmalloc(sizeof(Scope));
    memset(s, 0, sizeof(Scope));
    s->locals = newtab();
    s->parent = p->scope;
    p->scope = s;
}

static void popscope(Parser *p) {
    Scope *s = p->scope;
    p->scope = p->scope->parent;
    while (s->nlocals--)
        emitpop(p->c);
    freetab(s->locals);
    xfree(s);
}

static void stm(Parser *p);

static void block(Parser *p) {
    pushscope(p);
    while (!match(p, T_RBRACE))
        stm(p);
    popscope(p);
}

static void definelocal(Parser *p, Tok name) {
    int slot = p->nlocals++;
    p->scope->nlocals++;
    tabset(p->scope->locals, name.str, slot);
}

static void ifstm(Parser *p) {
    expect(p, T_LPAREN);
    expr(p);
    expect(p, T_RPAREN);
    emitnot(p->c);
    int elsejmp = emitcjmp(p->c);
    stm(p);
    int endjmp = emitjmp(p->c);
    patchjmp(p->c, elsejmp);
    if (match(p, T_ELSE))
        stm(p);
    patchjmp(p->c, endjmp);
}

static void whilestm(Parser *p) {
    int ip = getip(p->c);
    expect(p, T_LPAREN);
    expr(p);
    expect(p, T_RPAREN);
    emitnot(p->c);
    int endjmp = emitcjmp(p->c);
    stm(p);
    emitjmp2(p->c, ip - getip(p->c));
    patchjmp(p->c, endjmp);
}

static void stm(Parser *p) {
    if (match(p, T_IF)) {
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
        if (tabhas(p->scope->locals, name.str)) {
            printf("*** variable %s already declared\n", name.str);
            exit(1);
        }
        if (match(p, T_ASSIGN))
            expr(p);
        else
            emitnil(p->c);
        definelocal(p, name);
    }
    else if (match(p, T_PRINT)) {
        expr(p);
        emitprint(p->c);
    }
    else if (match(p, T_FUNC)) {
        expect(p, T_ID);
        Tok name = p->prev;
        if (tabhas(p->scope->locals, name.str)) {
            printf("*** variable %s already declared\n", name.str);
            exit(1);
        }
        definelocal(p, name);
        expect(p, T_LPAREN);
        expect(p, T_RPAREN);
        expect(p, T_LBRACE);
        block(p);
    }
    else {
        expr(p);
        emitpop(p->c);
    }
}

static void parsefile(Parser *p) {
    advance(p);
    pushscope(p);
    while (!match(p, T_EOF))
        stm(p);
    popscope(p);
    emitret(p->c);
}

static void definekws(Parser *p) {
    tabset(p->kws, "var", T_VAR);
    tabset(p->kws, "print", T_PRINT);
    tabset(p->kws, "if", T_IF);
    tabset(p->kws, "else", T_ELSE);
    tabset(p->kws, "while", T_WHILE);
    tabset(p->kws, "nil", T_NIL);
    tabset(p->kws, "function", T_FUNC);
}

Chunk *compile(char *src) {
    Parser p = {0};
    p.src = src;
    p.c = newchunk();
    p.kws = newtab();
    p.tokstrs = newarray(sizeof(char *));
    definekws(&p);
    parsefile(&p);
    freetab(p.kws);
    for (int i = 0; i < p.ntokstrs; i++)
        xfree(p.tokstrs[i]);
    freearray(p.tokstrs);
    return p.c;
}
