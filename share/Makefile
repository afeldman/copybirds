COMPFLAGS= -Wall -Wextra -std=gnu99 -pedantic -O2 -ggdb
LIBS=
CFLAGS=
COMPILER=gcc


all: together color

together:
	$(COMPILER) -c xmlhelper.c $(COMPFLAGS) -I/usr/include/libxml2
	$(COMPILER) -c dirtools.c $(COMPFLAGS)
	$(COMPILER) -c packagestates.c $(COMPFLAGS)
	$(COMPILER) -c hashhelper.c $(COMPFLAGS)

color: together
	$(COMPILER) -c tools.c $(COMPFLAGS)

nocolor: together
	$(COMPILER) -c tools.c $(COMPFLAGS)
	echo "If not already done, please define NOCOLOR in tools.h and recompile."

removebackups:
	rm -f *~


clean:
	rm -f  *.o
