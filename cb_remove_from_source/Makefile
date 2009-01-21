#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copycompare
PGN=cb_remove_from_source

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -O2 -ggdb
LIBS=../lib/libpathconvert.a
CFLAGS=
FILES=$(PGMAIN).o ../share/tools.o ../share/dirtools.o
COMPILER=gcc

all:
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAGS)
	../createdir.sh ../bin
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
