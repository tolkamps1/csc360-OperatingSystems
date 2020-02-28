CC := gcc
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror

all: diningOut

clean:
	rm -rf diningOut diningOut.dSYM

diningOut: diningOut.c
	$(CC) $(CFLAGS) -o diningOut diningOut.c -lpthread
