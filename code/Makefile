CC := cc
CFLAGS := -Wall -Wextra -Iinclude -O1 

SRCDIR := src
INCDIR := include
BINDIR := bin
BUILDDIR := build

PROGRAM := $(BINDIR)/ocsh

SRCS := $(wildcard $(SRCDIR)/*.c)

OBJS := $(SRCS:.c=.o)

all: $(PROGRAM)


$(PROGRAM): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(PROGRAM)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -m=755 -p $(BINDIR)

run: all
	PATH="$(CURDIR)/$(BINDIR):$$PATH" ./$(PROGRAM)

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(PROGRAM)

install:
	install -d -m 755 ~/bin
	mv ./bin/ocsh ~/bin

.PHONY: all run clean install

