CC = gcc
INCLUDES = -I .
CFLAGS = -Wall
DEPS = *.h
OBJDIR = objs
BINDIR = bin

OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))

$(OBJDIR)/%.o : %.c $(DEPS)
	@echo Building $<
	@$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)

all: $(OBJS)
	@$(CC) -o $(BINDIR)/psh $^ $(CLFAGS) $(INCLUDES)

.PHONY: clean

clean:
	@rm -f $(OBJDIR)/*.o
