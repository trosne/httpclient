all: 
	gcc -o test main.c httpclient.c -lz -lm -std=gnu99
