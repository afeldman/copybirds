#(c) 2009 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=sizecheck
PGN=sizecheck

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -Wno-long-long -O2 -ggdb
LIBS=
CFLAGS=
FILES=$(PGMAIN).o
COMPILER=gcc

all:
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAGS)

removebackups:
	rm -f *~

clean:
	rm -f *.o
	rm -f $(PGN)
