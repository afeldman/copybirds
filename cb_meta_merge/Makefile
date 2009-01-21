#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=metamerge
PGN=cb_meta_merge

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -O2 -ggdb -I/usr/include/libxml2
LIBS=`xml2-config --libs` ../lib/libpathconvert.a
CFLAGS=`xml2-config --cflags`
FILES=$(PGMAIN).o ../share/tools.o merger.o ../lib/hash-table.o ../share/xmlhelper.o ../share/hashhelper.o
COMPILER=gcc

all:
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -c merger.c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAGS)
	../createdir.sh ../bin
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
