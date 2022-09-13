
BIN = bin/star
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=out/%.o)
DEPS = $(SRCS:src/%.c=out/%.d)

CFLAGS = -g -c -MMD -I inc -Wall

all: $(BIN)

-include $(DEPS)

out:
	mkdir out

out/%.o: src/%.c | out
	$(CC) $(CFLAGS) $< -o $@

bin:
	mkdir bin

$(BIN): $(OBJS) $(AOBJS) | bin
	$(CC) $^ -o $@

clean:
	rm -rf out bin

test: all
	$(BIN) main.sr