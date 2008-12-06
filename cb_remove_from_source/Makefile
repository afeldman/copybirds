#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copycompare
PGN=cb_remove_from_source

COMPFLAGS= -Wall -Wextra -O2 -ggdb
LIBS=../lib/libpathconvert.a
CFLAGS=
FILES=$(PGMAIN).o ../share/tools.o ../share/dirtools.o

all:
	gcc -c $(PGMAIN).c $(COMPFLAGS)
	gcc -o $(PGN) $(FILES) $(LIBS) $(CFLAGS)
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
