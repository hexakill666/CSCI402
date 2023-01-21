warmup2: my402list.o warmup2.o
	gcc -g my402list.o warmup2.o -lpthread -lm -o warmup2

warmup2.o: warmup2.c my402list.h mypacket.h
	gcc -g -c -Wall warmup2.c

my402list.o: my402list.c my402list.h cs402.h
	gcc -g -c -Wall my402list.c

test: test.c
	gcc -g -Wall test.c -lpthread -lm -o test

clean:
	rm -f *.o *.gch warmup2 test