#by Malte Marwedel

COMPFLAGS= -Wall -Wextra -O2 -ggdb

all:
	gcc -c arraylist.c $(COMPFLAGS)
	gcc -c hash-table.c $(COMPFLAGS)
	gcc -c trie.c $(COMPFLAGS)

clean:
	rm -f *.o

removebackups:
	rm -f *~
