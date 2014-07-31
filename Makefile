LD_LIBRARY_PATH = /usr/local/lib

all: 
	gcc -o test httpclient.c -lz -lm -ljansson -std=gnu99
