#
# Copyright (c) 2011 Bharath Ramesh <bramesh.dev@gmail.com>
#

# set compiler to be used
CC = gcc

# set compile time flags
ARCH = -m64
C99 = -std=c99
CFLAGS = -g -Wall -Wextra -O3
SOCFLAGS = -fPIC
GNU = -D_GNU_SOURCE
LDFLAGS = -Wl,-rpath ./
LIBRARY = -L ./ -lbrmalloc

# compilation step
.c.o:
	$(CC) $(ARCH) $(C99) $(CFLAGS) $(SOCFLAGS) $(GNU) -c $<

all: lib bin

bin: lib
	$(CC) $(ARCH) $(C99) $(CFLAGS) $(GNU) $(LDFLAGS) -o test test.c $(LIBRARY)

clean:
	rm test
	rm -f libbrmalloc.so
	rm -f *.o

lib: brmalloc.o
	$(CC) -shared -Wl,-soname,libbrmalloc.so -o libbrmalloc.so $<
