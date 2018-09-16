CC = gcc
INCLUDES = -I .
CFLAGS = -Wall
DEPS = *.h
OBJDIR = objs
BINDIR = bin

OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))

default: all

PHONY: default clean all release release_build

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*.o
	@rm -f $(BINDIR)/*

all: CFLAGS += -D PSH_DEBUG
all: $(OBJS)
	@$(CC) -o $(BINDIR)/psh $^ $(CLFAGS) $(INCLUDES)

release: clean release_build

release_build: $(OBJS)
	@$(CC) -o $(BINDIR)/psh $^ $(CLFAGS) $(INCLUDES)

$(OBJDIR)/%.o : %.c $(DEPS)
	@echo Building $<
	@$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)
