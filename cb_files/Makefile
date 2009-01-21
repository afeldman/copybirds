#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copyowl
PGN=cb_files

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -Wno-long-long -O2 -ggdb -I/usr/include/libxml2
LIBS=`xml2-config --libs` ../lib/libpathconvert.a
CFLAGS=`xml2-config --cflags`
FILES=$(PGMAIN).o ../share/tools.o datamanager.o cloner.o ../lib/hash-table.o ../lib/arraylist.o ../share/dirtools.o ../share/xmlhelper.o ../share/hashhelper.o
COMPILER=gcc

all:
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -c datamanager.c $(COMPFLAGS)
	$(COMPILER) -c cloner.c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAGS)
	../createdir.sh ../bin
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
