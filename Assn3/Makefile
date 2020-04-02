CC:=gcc
CFLAGS:=-g -Wall -Werror 

TESTFILES := $(wildcard apps/test*.c) 
$(info TESTFILES are $(TESTFILES))

TESTS := $(TESTFILES:apps/%.c=%)
$(info TESTS are $(TESTS))

all: $(TESTS) drivers.o file.o

test%: apps/test%.c io/File.c io/File.h disk/drivers.c disk/drivers.h
	$(CC) $(CFLAGS) -o apps/$@ $^

drivers.o: disk/drivers.c disk/drivers.h
	$(CC) $(CFLAGS) -c -o $@ $<

file.o: io/File.c io/File.h 
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -rf *.o
	find apps -type f -not -name '*.c' -print0 | xargs -0 rm --