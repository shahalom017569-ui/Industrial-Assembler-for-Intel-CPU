CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude -D_POSIX_C_SOURCE=200809L
OBJDIR = build
BINDIR = bin
TARGET = $(BINDIR)/assembler

SRC = \
	src/main.c \
	src/assembler.c \
	src/lexer.c \
	src/parser.c \
	src/symbol_table.c \
	src/target.c \
	src/targets/i8008.c \
	src/targets/i8085.c \
	src/targets/i8086.c \
	src/targets/i386.c \
	src/targets/x86_64.c

OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))

all: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean
