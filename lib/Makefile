all:
	make -C pathconvert/lib
	ln -sf pathconvert/lib/libpathconvert.a libpathconvert.a

	make -C c-algorithms-1.1.0
	ln -sf c-algorithms-1.1.0/src/arraylist.h arraylist.h
	ln -sf c-algorithms-1.1.0/src/arraylist.o arraylist.o
	ln -sf c-algorithms-1.1.0/src/hash-table.h hash-table.h
	ln -sf c-algorithms-1.1.0/src/hash-table.o hash-table.o
	ln -sf c-algorithms-1.1.0/src/trie.h trie.h
	ln -sf c-algorithms-1.1.0/src/trie.o trie.o

clean:
	make -C pathconvert/lib clean
	make -C c-algorithms-1.1.0 clean
	rm -f hash-table.o
	rm -f trie.o
	rm -f arraylist.o
	rm -f libpathconvert.a
	rm -f trie.h
	rm -f arraylist.h
	rm -f hash-table.h
