CC = gcc
INCLUDES = -I .
CFLAGS = -Wall
DEPS = *.h
OBJDIR = objs
BINDIR = bin

OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))

PHONY: clean all release release_build

clean:
	@rm -f $(OBJDIR)/*.o

all: CFLAGS += -D PSH_DEBUG
all: $(OBJS)
	$(CC) -o $(BINDIR)/psh $^ $(CLFAGS) $(INCLUDES)

release: clean release_build

release_build: $(OBJS)
	$(CC) -o $(BINDIR)/psh $^ $(CLFAGS) $(INCLUDES)

$(OBJDIR)/%.o : %.c $(DEPS)
	@echo Building $<
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)
