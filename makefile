all:
	gcc -c -pedantic-errors -Wall mine.c
	gcc -c -pedantic-errors -Wall miner.c
	gcc -c -pedantic-errors -Wall mylib.h mylib.c
	gcc -o mine mine.o mylib.o -pthread -lrt
	gcc -o miner miner.o mylib.o -pthread -lrt
	rm mine.o
	rm miner.o
	rm mylib.o
