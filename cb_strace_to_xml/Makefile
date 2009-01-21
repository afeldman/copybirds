#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copyxmler
PGN=cb_strace_to_xml

COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -O2 -ggdb -I/usr/include/libxml2
LIBS=`xml2-config --libs` ../lib/libpathconvert.a
CFLAGS=`xml2-config --cflags`
FILES=$(PGMAIN).o filemanager.o ../share/tools.o pidmanager.o preprocessing_file.o dlist.o string_helper.o ../lib/hash-table.o connectionmanager.o ../share/hashhelper.o
COMPILER=gcc

all:
	$(COMPILER) -c $(PGMAIN).c $(COMPFLAGS)
	$(COMPILER) -c filemanager.c $(COMPFLAGS)
	$(COMPILER) -c string_helper.c -Wall $(COMPFLAGS)
	$(COMPILER) -c pidmanager.c $(COMPFLAGS)
	$(COMPILER) -c preprocessing_file.c $(COMPFLAGS)
	$(COMPILER) -c dlist.c $(COMPFLAGS)
	$(COMPILER) -c connectionmanager.c $(COMPFLAGS)
	$(COMPILER) -o $(PGN) $(FILES) $(LIBS) $(CFLAG)
	../createdir.sh ../bin
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f  *.o
	rm -f $(PGN)
