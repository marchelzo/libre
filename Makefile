all: re

re: re.h re.c
	cc -std=c11 -O2 -c -o libre.a re.c

install:
	sudo cp re.h /usr/local/include/re.h
	sudo cp libre.a /usr/local/lib/libre.a
