COMPFLAGS= -Wall -Wextra -O2 -ggdb
LIBS=
CFLAGS=

all: together color

together:
	gcc -c xmlhelper.c $(COMPFLAGS) -I/usr/include/libxml2
	gcc -c dirtools.c $(COMPFLAGS)
	gcc -c packagestates.c $(COMPFLAGS)
	gcc -c hashhelper.c $(COMPFLAGS)

color: together
	gcc -c tools.c $(COMPFLAGS)

nocolor: together
	gcc -c tools.c $(COMPFLAGS)
	echo "If not already done, please define NOCOLOR in tools.h and recompile."

removebackups:
	rm -f *~


clean:
	rm -f  *.o
