#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copyblack
PGN=cb_analyze_dep

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -O2 -ggdb -I/usr/include/libxml2
LIBS=`xml2-config --libs` ../lib/libpathconvert.a
CFLAGS=`xml2-config --cflags`
FILES=$(PGMAIN).o datamanager.o ../share/tools.o ../share/packagestates.o ../share/xmlhelper.o ../share/dirtools.o ../share/hashhelper.o ../lib/hash-table.o ../lib/trie.o
COMPILER=gcc

all:
	$(COMPILER) -c datamanager.c $(COMPFLAGS)
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAG)
	../createdir.sh ../bin
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
