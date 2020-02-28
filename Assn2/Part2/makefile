CC := gcc
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror

all: dataPar

clean:
	rm -rf dataPar dataPar.dSYM

dataPar: main.c
	$(CC) $(CFLAGS) -o dataPar main.c -lpthread
