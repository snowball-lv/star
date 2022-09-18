#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <star/mem.h>
#include <star/util.h>
#include <star/star.h>

#define OPS(OP) OP(NONE) OP(RET) OP(CONS) OP(NIL) OP(NEW) \
        OP(ADD) OP(SUB) OP(MUL) OP(DIV) \
        OP(NEG) \
        OP(PRINT) \
        OP(POP) \
        OP(GET_LOCAL) OP(SET_LOCAL) \
        OP(GET_FIELD) OP(SET_FIELD) \
        OP(CJMP) OP(JMP) \
        OP(NOP) \
        OP(NOT) \
        OP(LT)

#define OBJS(O) O(NONE) O(STR) O(TAB)

enum {
#define OP(name) OP_ ## name,
    OPS(OP)
#undef OP
};

enum {
#define O(name) OBJ_ ## name,
    OBJS(O)
#undef V
};

Chunk *newchunk() {
    Chunk *c = xmalloc(sizeof(Chunk));
    memset(c, 0, sizeof(Chunk));
    c->ins = newarray(sizeof(Ins));
    c->cons = newarray(sizeof(Value));
    return c;
}

Vm *newvm() {
    Vm *vm = xmalloc(sizeof(Vm));
    memset(vm, 0, sizeof(Vm));
    vm->stack = newarray(sizeof(Value));
    return vm;
}

static void *allocobj(int size) {
    Obj *o = xmalloc(size);
    return o;
}

static void freeobj(Obj *o) {
    if (o->type == OBJ_STR)
        xfree(((ObjString *)o)->str);
    xfree(o);
}

void freevm(Vm *vm) {
    freearray(vm->stack);
    xfree(vm);
}

void freechunk(Chunk *c) {
    freearray(c->ins);
    for (int i = 0; i < c->ncons; i++) {
        if (c->cons[i].type != V_OBJ) continue;
        freeobj(c->cons[i].as.obj);
    }
    freearray(c->cons);
    xfree(c);
}

static ObjTab *alloctab() {
    ObjTab *o = allocobj(sizeof(ObjTab));
    o->hdr.type = OBJ_TAB;
    o->fields = newvaltab();
    return o;
}

static ObjString *allocstr(char *str) {
    ObjString *o = allocobj(sizeof(ObjString));
    o->hdr.type = OBJ_STR;
    o->len = strlen(str);
    o->str = xmalloc(o->len + 1);
    strcpy(o->str, str);
    o->hash = strhash(str);
    return o;
}

Value numval(double num) {
    return (Value){V_NUM, {.num = num}};
}

Value boolval(char boolean) {
    return (Value){V_BOOL, {.boolean = !!boolean}};
}

Value nilval() {
    return (Value){V_NIL};
}

Value strval(char *str) {
    return (Value){V_OBJ, {.obj = (Obj *)allocstr(str)}};
}

int addcons(Chunk *c, Value v) {
    int idx = c->ncons++;
    c->cons = arraygrow(c->cons, c->ncons);
    c->cons[idx] = v;
    return idx;
}

int emit(Chunk *c, Ins i) {
    int idx = c->nins++;
    c->ins = arraygrow(c->ins, c->nins);
    c->ins[idx] = i;
    return idx;
}

int emitcons(Chunk *c, int consid) {
    return emit(c, (Ins){OP_CONS, consid});
}

int emitadd(Chunk *c) {
    return emit(c, (Ins){OP_ADD});
}

int emitsub(Chunk *c) {
    return emit(c, (Ins){OP_SUB});
}

int emitmul(Chunk *c) {
    return emit(c, (Ins){OP_MUL});
}

int emitdiv(Chunk *c) {
    return emit(c, (Ins){OP_DIV});
}

int emitneg(Chunk *c) {
    return emit(c, (Ins){OP_NEG});
}

int emitret(Chunk *c) {
    return emit(c, (Ins){OP_RET});
}

int emitprint(Chunk *c) {
    return emit(c, (Ins){OP_PRINT});
}

int emitpop(Chunk *c) {
    return emit(c, (Ins){OP_POP});
}

int emitgetlocal(Chunk *c, int slot) {
    return emit(c, (Ins){OP_GET_LOCAL, slot});
}

int emitsetlocal(Chunk *c, int slot) {
    return emit(c, (Ins){OP_SET_LOCAL, slot});
}

int emitgetfield(Chunk *c, int consid) {
    return emit(c, (Ins){OP_GET_FIELD, consid});
}

int emitsetfield(Chunk *c, int consid) {
    return emit(c, (Ins){OP_SET_FIELD, consid});
}

int emitcjmp(Chunk *c) {
    return emit(c, (Ins){OP_CJMP});
}

int emitjmp(Chunk *c) {
    return emit(c, (Ins){OP_JMP});
}

int emitjmp2(Chunk *c, int ip) {
    return emit(c, (Ins){OP_JMP, ip});
}

int emitnot(Chunk *c) {
    return emit(c, (Ins){OP_NOT});
}

int emitlt(Chunk *c) {
    return emit(c, (Ins){OP_LT});
}

int emitnil(Chunk *c) {
    return emit(c, (Ins){OP_NIL});
}

int emitnew(Chunk *c) {
    return emit(c, (Ins){OP_NEW});
}

void patchjmp(Chunk *c, int ip) {
    c->ins[ip].arg = c->nins - ip;
}

static const char *valname(int type) {
    switch (type) {
#define V(name) case V_ ## name: return #name;
    VALS(V)
#undef V
    }
    printf("*** unknown value type %i\n", type);
    exit(1);
}

static const char *opname(int op) {
    switch (op) {
#define OP(name) case OP_ ## name: return #name;
    OPS(OP)
#undef OP
    }
    printf("*** unknown op %i\n", op);
    exit(1);
}

int getip(Chunk *c) {
    return c->nins;
}

void fixassign(Chunk *c, int ip) {
    Ins i = c->ins[ip];
    switch (i.op) {
    case OP_GET_LOCAL:
        c->ins[ip].op = OP_NOP;
        emitsetlocal(c, i.arg);
        return;
    case OP_GET_FIELD:
        c->ins[ip].op = OP_NOP;
        emitsetfield(c, i.arg);
        return;
    }
    printf("*** left-hand side not an lvalue\n");
    exit(1);
}

void printchunk(Chunk *c) {
    printf("--- Chunk ---\n");
    printf("Constants:\n");
    for (int i = 0; i < c->ncons; i++) {
        Value cons = c->cons[i];
        printf("%3i: ", i);
        switch (cons.type) {
        case V_NUM: printf("%f", cons.as.num); break;
        case V_NIL: printf("nil"); break;
        case V_OBJ:
            switch (cons.as.obj->type) {
            case OBJ_STR:
                printf("\"%s\"", ((ObjString *)cons.as.obj)->str);
                break;
            default: printf("???"); break;
            }
            break;
        default: printf("???"); break;
        }
        printf("\n");
    }
    printf("Code:\n");
    for (int k = 0; k < c->nins; k++) {
        Ins i = c->ins[k];
        printf("%3i: ", k);
        switch (i.op) {
        case OP_RET:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_PRINT:
        case OP_NEG:
        case OP_POP:
        case OP_NOP:
        case OP_NOT:
        case OP_LT:
        case OP_NIL:
        case OP_NEW:
            printf("%s", opname(i.op));
            break;
        case OP_CONS:
        case OP_GET_LOCAL: case OP_SET_LOCAL:
        case OP_GET_FIELD: case OP_SET_FIELD:
        case OP_CJMP:
        case OP_JMP:
            printf("%s %i", opname(i.op), i.arg);
            break;
        default: printf("???"); break;
        }
        printf("\n");
    }
}

static void push(Vm *vm, Value v) {
    int idx = vm->nstack++;
    vm->stack = arraygrow(vm->stack, vm->nstack);
    vm->stack[idx] = v;
}

static Value pop(Vm *vm) {
    if (!vm->nstack) {
        printf("*** stack underflow\n");
        exit(1);
    }
    return vm->stack[--vm->nstack];
}

static int istrue(Value v) {
    switch (v.type) {
    case V_NUM: return v.as.num != 0.0;
    case V_BOOL: return v.as.boolean;
    }
    return 0;
}

static int isvstr(Value v) {
    return v.type == V_OBJ && v.as.obj->type == OBJ_STR;
}

static void printval(Value v) {
    switch (v.type) {
    case V_NIL: printf("nil"); return;
    case V_BOOL: printf(v.as.boolean ? "true" : "false"); return;
    case V_NUM: printf("%f", v.as.num); return;
    case V_OBJ: {
        switch (v.as.obj->type) {
        case OBJ_STR: printf("\"%s\"", ((ObjString *)v.as.obj)->str); return;
        case OBJ_TAB: printf("{Object Table}"); return;
        }
    }
    }
    printf("???");
}

void printstack(Vm *vm) {
    printf("--- Stack ---\n");
    for (int i = 0; i < vm->nstack; i++) {
        Value v = vm->stack[i];
        printf("%3i: %s ", i, valname(v.type));
        printval(v);
        printf("\n");
    }
}

static void binop(Vm *vm, Value l, Value r, int op) {
    if (l.type == V_NUM && r.type == V_NUM) {
        switch (op) {
        case OP_ADD: push(vm, numval(l.as.num + r.as.num)); return;
        case OP_SUB: push(vm, numval(l.as.num - r.as.num)); return;
        case OP_MUL: push(vm, numval(l.as.num * r.as.num)); return;
        case OP_DIV: push(vm, numval(l.as.num / r.as.num)); return;
        case OP_LT: push(vm, boolval(l.as.num < r.as.num)); return;
        }
    }
    else if (isvstr(l) && isvstr(r) && op == OP_ADD) {
        ObjString *lstr = (void *)l.as.obj;
        ObjString *rstr = (void *)r.as.obj;
        int len = lstr->len + rstr->len;
        char *str = xmalloc(len + 1);
        strcpy(str, lstr->str);
        strcpy(str + lstr->len, rstr->str);
        Value v = strval(str);
        push(vm, v);
        xfree(str);
        return;
    }
    else if ((l.type == V_NUM && isvstr(r)) || (r.type == V_NUM && isvstr(l))) {
        int n = l.type == V_NUM ? l.as.num : r.as.num;
        Value str = isvstr(l) ? l : r;
        push(vm, strval(""));
        for (int i = 0; i < n; i++)
            binop(vm, pop(vm), str, OP_ADD);
        return;
    }
    printf("*** can't execute binop %s on ", opname(op));
    printval(l); printf(" and "); printval(r);
    printf("\n");
    exit(1);
}

static void pushfield(Vm *vm, Value vtab, Value vname) {
    if (vtab.type != V_OBJ || vtab.as.obj->type != OBJ_TAB) {
        printf("*** only tables have fields\n");
        exit(1);
    }
    if (vname.type != V_OBJ || vname.as.obj->type != OBJ_STR) {
        printf("*** field name has to be a string\n");
        exit(1);
    }
    ObjTab *tab = (ObjTab *)vtab.as.obj;
    ObjString *name = (ObjString *)vname.as.obj;
    Value tmp;
    if (valtabget(tab->fields, name, &tmp))
        push(vm, tmp);
    else
        push(vm, nilval());
}

void runchunk(Vm *vm, Chunk *c) {
    for (int ip = 0; ip < c->nins; ip++) {
        Ins i = c->ins[ip];
        switch (i.op) {
        case OP_NOP: break;
        case OP_RET: return;
        case OP_CONS: push(vm, c->cons[i.arg]); break;
        case OP_NIL: push(vm, nilval()); break;
        case OP_NEW: {
            Value v = (Value){V_OBJ, {.obj = (Obj *)alloctab()}};
            push(vm, v);
            break;
        }
        case OP_POP: pop(vm); break;
        case OP_GET_LOCAL: push(vm, vm->stack[i.arg]); break;
        case OP_SET_LOCAL: {
            Value v = pop(vm);
            vm->stack[i.arg] = v;
            push(vm, v);
            break;
        }
        case OP_GET_FIELD: {
            pushfield(vm, pop(vm), c->cons[i.arg]);
            break;
        }
        case OP_SET_FIELD:  {
            Value v = pop(vm);
            Value vtab = pop(vm);
            Value vname = c->cons[i.arg];
            ObjTab *tab = (ObjTab *)vtab.as.obj;
            ObjString *name = (ObjString *)vname.as.obj;
            valtabset(tab->fields, name, v);
            push(vm, v);
            break;
        }
        case OP_NEG: {
            Value v = pop(vm);
            if (v.type != V_NUM) {
                printf("*** can only negate numbers\n");
                exit(1);
            }
            push(vm, numval(-v.as.num));
            break;
        }
        case OP_NOT: {
            push(vm, boolval(!istrue(pop(vm))));
            break;
        }
        case OP_JMP: {
            ip += i.arg - 1; // account for ip++
            break;
        }
        case OP_CJMP: {
            Value v = pop(vm);
            if (istrue(v))
                ip += i.arg - 1; // account for ip++
            break;
        }
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV: 
        case OP_LT: {
            Value r = pop(vm);
            Value l = pop(vm);
            binop(vm, l, r, i.op);
            break;
        }
        case OP_PRINT: 
            printval(pop(vm));
            printf("\n");
            break;
        default:
            printf("*** can't execute %s\n", opname(i.op));
            exit(1);
        }
    }
}
