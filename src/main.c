#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <star/mem.h>
#include <star/util.h>
#include <star/star.h>

static char *OPTS[] = {
    "-i:interactive (REPL)",
    0,
};

static void usage() {
    printf("Usage: star [options] file\n");
    printf("Options:\n");
    for (char **opt = OPTS; *opt; opt++) {
        char *sep = strchr(*opt, ':');
        printf("    %-7.*s %s\n", (int)(sep - *opt), *opt, sep + 1);
    }
    exit(1);
}

static char *readfile(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) exit(1);
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    char *src = xmalloc(size + 1);
    rewind(fp);
    fread(src, size, 1, fp);
    src[size] = 0;
    fclose(fp);
    return src;
}

static int emptyline(char *line) {
    for (; *line; line++)
        if (!isspace(*line)) return 0;
    return 1;
}

int main(int argc, char **argv) {
    char line[1024];
    int repl = 0;
    char *file = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) repl = 1;
        else if (!file) file = argv[i];
        else printf("*** unknown option %s\n", argv[i]);
    }
    if (repl) {
        Vm *vm = newvm();
        printf("star repl\n");
        for (;;) {
            printf("> ");
            if (!fgets(line, sizeof(line), stdin)) break;
            if (emptyline(line)) continue;
            ObjFunc *fn = compile(line);
            printchunk(fn->chunk);
            runchunk(vm, fn->chunk);
            printstack(vm);
            freechunk(fn->chunk);
        }
        freevm(vm);
    }
    else if (!file) {
        usage();
    }
    else {
        printmem();
        char *src = readfile(file);
        ObjFunc *fn = compile(src);
        xfree(src);
        Vm *vm = newvm();
        printchunk(fn->chunk);
        runchunk(vm, fn->chunk);
        printstack(vm);
        freechunk(fn->chunk);
        freevm(vm);
        printmem();
    }
    return 0;
}
