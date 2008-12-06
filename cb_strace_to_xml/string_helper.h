
#ifndef STRING_HELPER_H
#define STRING_HELPER_H

int extract_pid(const char * text);
char * extract_syscall(const char * text);
char * extract_filename(const char * text);
const char * filename_ending(const char * text);
int get_retvalue(const char * text);
char * extract_connectiontype(const char *text);
char * extract_inetaddr(const char *text);
char * extract_inet6addr(const char *text);
int extract_connport(const char *text);
int extract_conn6port(const char *text);
#endif
