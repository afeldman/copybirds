# (c) by Malte Marwedel, terms of use for THIS file: LGPL

LIBNAME= libpathconvertLGPL.a

all:
	gcc -c abs2rel.c -Wall -O2
	gcc -c rel2abs.c -Wall -O2
	ar -cru libpathconvertLGPL.a abs2rel.o rel2abs.o

clean:
	rm -f *.o
	rm -f libpathconvertLGPL.a

removebackups:
	rm -f *~
