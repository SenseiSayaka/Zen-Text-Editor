zen: zen.c
	gcc zen.c -o zen -Wall -Wextra -pedantic -std=c99
	./zen
clear:
	rm zen
