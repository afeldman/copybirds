
#ifndef COPYXMLER_H

#define COPYXMLER_H

#define PGN "cb_strace_to_xml"

#define MY_ENCODING "UTF-8"

#define INITIAL_BUF_LEN 128
//smaller needs less ram, bigger will work mort likely without problems

#define MAX_PATH_LEN 2048

extern int get_linecount(void);
extern int get_ignorepids();

#endif
