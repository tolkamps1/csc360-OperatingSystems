CC := gcc
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror

TESTS := test01 test02

all: $(TESTS)

clean:
	rm -rf $(TESTS) *.dSYM

test%: test%.c ../io/File.c
	$(CC) $(CFLAGS) -I.. -o $@ $< ../io/File.c
