#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

PGMAIN=copyxmler
PGN=cb_strace_to_xml

COMPFLAGS= -Wall -Wextra -O2 -ggdb -I/usr/include/libxml2
LIBS=`xml2-config --libs` ../lib/libpathconvert.a
CFLAGS=`xml2-config --cflags`
FILES=$(PGMAIN).o filemanager.o ../share/tools.o pidmanager.o preprocessing_file.o dlist.o string_helper.o ../lib/hash-table.o connectionmanager.o ../share/hashhelper.o

all:
	gcc -c $(PGMAIN).c $(COMPFLAGS)
	gcc -c filemanager.c $(COMPFLAGS)
	gcc -c string_helper.c -Wall $(COMPFLAGS)
	gcc -c pidmanager.c $(COMPFLAGS)
	gcc -c preprocessing_file.c $(COMPFLAGS)
	gcc -c dlist.c $(COMPFLAGS)
	gcc -c connectionmanager.c $(COMPFLAGS)
	gcc -o $(PGN) $(FILES) $(LIBS) $(CFLAG)
	cp $(PGN) ../bin/$(PGN)

removebackups:
	rm -f *~

clean:
	rm -f  *.o
	rm -f $(PGN)
