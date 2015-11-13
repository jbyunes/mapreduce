UNAME=$(shell uname)
CC = gcc
CFLAGS = -O3 -I.
LDFLAGS =
LDLIBS =
ifeq ($(UNAME),Linux)
LDLIBS += -lpthread
endif
OBJS = main.o treelib.o
EXEC = mapred

.PHONY=all test status

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

main.o: main.c treelib.h
	$(CC) $(CFLAGS) -c $<

treelib.o: treelib.c treelib.h
	$(CC) $(CFLAGS) -c $<

test: $(EXEC)
	test.sh $(EXEC)

clean:
	rm $(OBJS) $(EXEC)

all: $(EXEC) test

status:
	git status
