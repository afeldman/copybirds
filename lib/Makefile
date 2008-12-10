all:
	make -C pathconvert/lib/LGPL
	ln -sf pathconvert/lib/LGPL/libpathconvertLGPL.a libpathconvert.a

#use
#make -C c-algorithms-1.1.0
#instead, if the directory contains the original c-algorithms library
	make -C c-algorithms-1.1.0/src
	ln -sf c-algorithms-1.1.0/src/arraylist.h arraylist.h
	ln -sf c-algorithms-1.1.0/src/arraylist.o arraylist.o
	ln -sf c-algorithms-1.1.0/src/hash-table.h hash-table.h
	ln -sf c-algorithms-1.1.0/src/hash-table.o hash-table.o
	ln -sf c-algorithms-1.1.0/src/trie.h trie.h
	ln -sf c-algorithms-1.1.0/src/trie.o trie.o

clean:
	make -C pathconvert/lib/LGPL clean
#use
#make -C c-algorithms-1.1.0 clean
#instead, if the directory contains the original c-algorithms library
	make -C c-algorithms-1.1.0/src clean
	rm -f hash-table.o
	rm -f trie.o
	rm -f arraylist.o
	rm -f libpathconvert.a
	rm -f trie.h
	rm -f arraylist.h
	rm -f hash-table.h

removebackups:
	rm -f *~
	make -C  pathconvert/lib/LGPL removebackups
#disable if the original  c-algorithms library package is used
	make -C  c-algorithms-1.1.0/src removebackups