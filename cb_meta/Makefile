#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copymeta
PGN=cb_meta_check

COMPFLAGS= -Wall -Wextra -O2 -ggdb -I/usr/include/libxml2
LIBS=`xml2-config --libs` -lGLU -lGL -lglut
CFLAGS=`xml2-config --cflags`
FILES=$(PGMAIN).o comparesys.o ../share/tools.o ../share/packagestates.o ../share/xmlhelper.o ../share/hashhelper.o ../lib/hash-table.o ../lib/trie.o
COMPILER=gcc

all:
	$(COMPILER) -c comparesys.c $(COMPFLAGS)
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAGS)
	../createdir.sh ../bin
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
