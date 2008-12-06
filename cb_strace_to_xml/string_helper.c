
/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "../share/tools.h"
#include "copyxmler.h"

/*
Gets the pid from a line generated by strace. On error, a -1 is returned */
int extract_pid(const char * text) {
	char * pidstr = smalloc(sizeof(char)*6);
	if (strlen(text) > 5) {
		memcpy(pidstr, text, 5);
		pidstr[5] = '\0';
		int pid = atoi(pidstr);
		free(pidstr);
		return pid;
	}
	message(4, "extract_pid: Warning: no pid found in '%s', line: %i\n", text, get_linecount());
	return -1;
}

/*
Gets the name (null terminated) of the syscall from a line generated by strace.
The returning value should be freed after usage*/
char * extract_syscall(const char * text) {
	char * end = strchr(text, '(');
	if (end == NULL)
		message(1, "extract_syscall: Error: No syscall found in '%s' line: %i\n", text, get_linecount());
	int len = end-text-6;
	if (len <= 0)	{
		message(2, "extract_syscall: Error: No syscall found in '%s', line: %i\n", text, get_linecount());
		char * empty = smalloc(sizeof(char));
		empty[0] = '\0';
		return empty;
	}
	char * sysstr = smalloc(sizeof(char)*len+1);
	memcpy(sysstr, text+6, len);
	sysstr[len] = '\0';
	return sysstr;
}

/*
Gets the string parameter, if the line generated by strace
contains one. On all relevant syscalls, the meaning of the string is a filename.
Return the filename (null terminated) which should be freed after usage.
NULL if no string was found.
*/
char * extract_filename(const char * text) {
	char * filename = text_get_between(text, "\"", "\"");
	if (filename == NULL) {
		message(1, "extract_filename: Error: Could not extract filename from %s\n", text);
	}
	return filename;
}

/*
Returns a pointer to the first char after the first '=' sign in the input text.
No copy of the string is made. If no '=' is found, text is returned */
const char * filename_ending(const char * text) {
	char * begin = index(text, '"');
	if (begin != NULL) {
		begin++; //because the " is not needed
		char * end = index(begin, '"');
		if (end != NULL) {
			return end;
		}
	}
	message(1, "filename_ending: Error: Could not find filename end %s\n", text);
	return text;
}

/*
Gets the returning value of a syscall line of strace. This will be the number
after the last '=' sign in the input line. If no '= is found or if the text
after this is less than 3 chars, -1 is returned.
*/
int get_retvalue(const char * text) {
	char * equalsign = strrchr(text, '=');
	if (equalsign == NULL) return -1;
	int reaminglen = strlen(equalsign);
	if (reaminglen < 3) return -1;
	int result = atoi(equalsign+2);
	message(4, "get_retvalue: Value of %s is: %i\n", text, result);
	return result;
}

char * extract_connectiontype(const char *text) {
	return text_get_between(text, "sa_family=", ",");
}

char * extract_inetaddr(const char *text) {
	return text_get_between(text, " sin_addr=inet_addr(\"", "\"");;
}

char * extract_inet6addr(const char *text) {
	char * pton = text_get_between(text, "inet_pton(", ")");
	char * addr = text_get_between(pton, ", \"", "\"");
	free(pton);
	return addr;
}

int extract_connport(const char *text) {
	char * port = text_get_between(text, "sin_port=htons(", ")");
	if (port != NULL) {
		int val = atoi(port);
		free(port);
		return val;
	}
	return -1;
}

int extract_conn6port(const char *text) {
	char * port = text_get_between(text, "sin6_port=htons(", ")");
	if (port != NULL) {
		int val = atoi(port);
		free(port);
		return val;
	}
	return -1;
}