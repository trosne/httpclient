all: 
	gcc -o example example.c httpclient.c -lz -lm -std=gnu99 -ggdb
