#pragma once

#define VALS(V) V(NONE) V(NUM) V(BOOL) V(NIL) V(OBJ)

enum {
#define V(name) V_ ## name,
    VALS(V)
#undef V
};

typedef struct ValTab ValTab;

typedef struct  {
    char type;
} Obj;

typedef struct {
    Obj hdr;
    char *str;
    int len;
    unsigned hash;
} ObjString;

typedef struct {
    Obj hdr;
    ValTab *fields;
} ObjTab;

typedef struct {
    char type;
    union {
        double num;
        char boolean;
        Obj *obj;
    } as;
} Value;

typedef struct {
    char op;
    int arg;
} Ins;

typedef struct {
    Ins *ins;
    int nins;
    Value *cons;
    int ncons;
} Chunk;

typedef struct {
    Obj hdr;
    Chunk *chunk;
} ObjFunc;

typedef struct {
    Value *stack;
    int nstack;
} Vm;

#define OBJVAL(o) ((Value){.type = V_OBJ, {.obj = (Obj*)(o)}})

Chunk *newchunk();
void freechunk(Chunk *c);
Vm *newvm();
void freevm(Vm *vm);
ObjFunc *newfunc();

Value numval(double num);
Value boolval(char boolean);
Value nilval();
Value strval(char *str);
int addcons(Chunk *c, Value v);
int emit(Chunk *c, Ins i);

int emitcons(Chunk *c, int consid);
int emitadd(Chunk *c);
int emitsub(Chunk *c);
int emitmul(Chunk *c);
int emitdiv(Chunk *c);
int emitneg(Chunk *c);
int emitret(Chunk *c);
int emitprint(Chunk *c);
int emitpop(Chunk *c);
int emitgetlocal(Chunk *c, int slot);
int emitsetlocal(Chunk *c, int slot);
int emitgetfield(Chunk *c, int consid);
int emitcjmp(Chunk *c);
int emitjmp(Chunk *c);
int emitjmp2(Chunk *c, int ip);
int emitnot(Chunk *c);
int emitlt(Chunk *c);
int emitnil(Chunk *c);
int emitnew(Chunk *c);
int emitcall(Chunk *c);

void patchjmp(Chunk *c, int ip);
int getip(Chunk *c);
void fixassign(Chunk *c, int ip);

ValTab *newvaltab();
void freevaltab(ValTab *vt);
int valtabget(ValTab *vt, ObjString *key, Value *dst);
void valtabset(ValTab *vt, ObjString *key, Value v);

void printchunk(Chunk *c);
void printstack(Vm *vm);

void runchunk(Vm *vm, Chunk *c);

ObjFunc *compile(char *src);
