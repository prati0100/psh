CC = gcc
INCLUDES = -I .
CFLAGS = -Wall
DEPS = *.h
OBJDIR = objs
BINDIR = bin

OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))

default: debug

PHONY: default clean all release debug

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*.o
	@rm -f $(BINDIR)/*

debug: CFLAGS += -D PSH_DEBUG
debug: all

release: clean all

all: $(OBJS)
	@mkdir -p ${BINDIR}
	@$(CC) -o $(BINDIR)/psh $^ $(CLFAGS) $(INCLUDES)

$(OBJDIR)/%.o : %.c $(DEPS)
	@mkdir -p $(OBJDIR)
	@echo Building $<
	@$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)
