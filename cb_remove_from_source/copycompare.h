
#ifndef COPYCOMPARE_H
	#define COPYCOMPARE_H

#define PGN "cb_remove_from_source"


//typedef howto: http://www.tu-clausthal.de/~rzppk/wscript/node60.html

typedef struct comparedata {
	const char * source;
	const char * target;
	int typesource;
	int typetarget;
} comparedata_t;

void rec_dir(comparedata_t * parentcomp);

#endif
