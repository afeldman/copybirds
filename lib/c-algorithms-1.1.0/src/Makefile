#by Malte Marwedel

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -O2 -ggdb
COMPILER=gcc

all:
	$(COMPILER) -c arraylist.c $(COMPFLAGS)
	$(COMPILER) -c hash-table.c $(COMPFLAGS)
	$(COMPILER) -c trie.c $(COMPFLAGS)

clean:
	rm -f *.o

removebackups:
	rm -f *~
