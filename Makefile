CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=
OBJDIR = build
BINDIR = bin
TARGET = $(BINDIR)/assembler

SRC = src/main.c src/assembler.c src/target.c
OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))

all: $(TARGET)

$(OBJDIR) $(BINDIR):
	mkdir -p $@

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

test: $(TARGET)
	sh tests/run.sh

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all test clean
