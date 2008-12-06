
/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "tools.h"

int chatting = 1; //0: silent, 1: normal, 2: talk about everything, 4: debug, 5: super debug
const char * pgn = "Program";

/*Unless otherwise noted, all functions with char * as return values returning
NULL only, if one of the parameters is NULL. An error message is printed. */

/*
Returns a printf message if the level parameter is the same or lower than
the mode set by set_chatting. The format of the other parameters
is exactly the same than for printf. (However the compiler does not
warn if there are formatting mistakes).
Based on: http://bytes.com/forum/thread220856.html */
void message(int level, const char * format, ...) {
	if (level > chatting) { //nothing to say
		return;
	}
	va_list ap;
	char * strnew = smalloc(strlen(format)+100);
#ifndef NOCOLOR
	if (strstr(format, " Warning: ") != 0) {
		/*color output based on: http://linuxgazette.net/issue65/padala.html
		*/
		 //may not exceed 99 bytes
		sprintf(strnew, "\033[01;33;40m%s[%i]:\033[00m ", pgn, getpid());
	} else
	if (strstr(format, " Error: ") != 0) {
		 //may not exceed 99 bytes
		sprintf(strnew, "\033[01;31;40m%s[%i]:\033[00m ", pgn, getpid());
	} else
		sprintf(strnew, "%s[%i]: ", pgn, getpid()); //may not exceed 99 bytes
#else
	sprintf(strnew, "%s[%i]: ", pgn, getpid()); //may not exceed 99 bytes
#endif
	strcat(strnew, format);
	va_start(ap, format); /* Initialize the va_list */
	vprintf(strnew, ap); /* Call vprintf */
	va_end(ap); /* Cleanup the va_list */
	free (strnew);
	return;
}

/*
makes a new string consisting of both strigs. new memory is allocated and has
to be freed after use.
 */
char * merge(const char * str1, const char * str2) {
	if ((str1 == NULL) || (str2 == NULL)) {
		message(1, "merge: Error: One or both paramerers are null pointers\n");
		return NULL;
	}
	int len1 = strlen(str1);
	int len2 = strlen(str2);
	int biglen = len1+len2+1;
	char * bigone = smalloc(sizeof(char)*biglen);
	memcpy(bigone, str1, len1);
	memcpy(bigone+len1, str2, len2);
	bigone[biglen-1] = '\0';
	return bigone;
}

/*
Replaces all multiple '/' by a single one.
Example: "/usr//local//////bin/foo"
will return "usr/local/bin/foo".
The returning memory has to be freed after usage. */
char * nodoubleslash(const char * path) {
	if (path == NULL) {
		message(1, "nodoubleslash: Error: Paramerer is a null pointer\n");
		return NULL;
	}
	int rlen = strlen(path);
	char * newpath = smalloc(sizeof(path)*(rlen+1));
	int rptr = 0;
	int wptr = 0;
	int lastslash = 0; //if 1, do not copy '/' signs
	while (rptr <= rlen) {
		if ((lastslash == 0) || (path[rptr] != '/')) {
			newpath[wptr] = path[rptr];
			if (path[rptr] == '/') {
				lastslash = 1;
			}	else
				lastslash = 0;
			wptr++;
		}
		rptr++;
	}
	newpath[wptr] = '\0';
	return newpath;
}

/*
Makes a new string consisting of both strigs.
There will be exactly one '/' between the two strings in the
result. All double "//" (or more) will be replaced by a single
"/" in the result.
New memory is allocated and has to be freed after use.*/
char * dirmerge(const char * str1, const char * str2) {
	//possible nullpointers are handeled by the sub functions
	char * m0 = merge(str1, "/");
	char * m1 = merge(m0, str2);
	free(m0);
	char * m2 = nodoubleslash(m1);
	free(m1);
	return m2;
}

/*
Returns 1 if the path does not begin with a '/'.
Otherwise a 0 is returned (also if the path has a zero length)*/
int path_is_relative(const char * path) {
	if (path == NULL) {
		message(1, "path_is_relative: Error: Parameter is a null pointer\n");
		return 0;
	}
	if (strlen(path) > 0) {
		if (path[0] == '/') {
			return 0;
		} else
			return 1;
	}
	message(1, "path_is_relative: Error: path has zero lenght\n");
	return 0;
}

/*
Returns 1 if the path does begin with a '/'.
Otherwise a 0 is returned (also if the path has a zero length)
Note: This is similar to path_is_relative, but you can decide between
error and an absolute path
*/
int path_is_absolute(const char * path) {
	if (path == NULL) {
		message(1, "path_is_absolute: Error: Parameter is a null pointer\n");
		return 0;
	}
	if (strlen(path) > 0) {
		if (path[0] == '/') {
			return 1;
		} else
			return 0;
	}
	message(1, "path_is_relative: Error: path has zero lenght\n");
	return 0;
}


/*
Returns an new null terminated string, representing the content
before and including the last '/' found in file.
Useful to get the parent folder form a folder or file.
Returns NULL if there were no '/' in the parameter or an error occured.
The returning string should be freed after usage.
 */
char * path_from_file(const char * file) {
	if (file == NULL) {
		message(1, "path_from_file: Error: Parameter is a null pointer\n");
		return NULL;
	}
	const char * lastslash = strrchr(file, '/');
	if (lastslash == NULL) {
		message(1, "path_from_file: Error: No path to extract\n");
		return NULL;
	}
	int len = lastslash - file;
	return strndup(file, len+1);
}

/*
Safe malloc.
Checks if size is >= 0, otherwise the program exits with 1.
Checks if the malloc succeed, otherwise the program exits with 1.
Returns the value, malloc gave us.
*/
void * smalloc(size_t size) {
	if (size > 1000000000) { //1GB
		message(1, "smalloc: Error: tried to allocate more than 1GB (%i bytes),\
		   this may indicate a negative number with a wrong cast. Exiting\n", size);
		exit(1);
	}
	if (size == 0) {
		/*as malloc is often used for strings and we depend on null-terminated ones
		 there sould be at least one useable accessible character*/
		message(1, "smalloc: Warning: Tried to allocate 0 byte, replacing with 1\
 byte\n");
		size = 1;
	}
	void * mem = malloc(size);
	if (mem == NULL) {
		message(1, "smalloc: Error: could not allocate %i bytes with malloc.\
 Exiting\n", size);
		exit(1);
	}
	return mem;
}

/*
Sets the mode on which message() should really print out messages.
newpgm should be initialized with the program name at the beginning,
and will be used for the message() function. */
void set_chatting(const char * newpgn, int mode) {
	chatting = mode;
	pgn = newpgn;
}

/*
Returns a decimal string representation of val.
(Not included in the common glibc, its horrible that everyone has to write such
 trivial things again and again, and agian....)
The string has to be freed after usage.
*/
char * itoa(int val) {
	/*if int would be 64 bit, the max value would be 2^64=1,8*10^19 unsigned or
	2^63=9*10^18 and  negative sign. adding one char for the ending \0 makes
	21 chars.
	*/
	char * blub = smalloc(21);
	sprintf(blub,"%i",val);
	return blub;
}

/*
Returns the smaller one of the parameters */
int imin(int a, int b) {
	if (a < b) return a;
	return b;
}

/* Returns 1 if end is the last part of text, otherwise 0 */
int endswith(const char * text, const char * end) {
	if ((text == NULL) || (text == NULL)) {
		message(1, "endswith: Error: One or both parameter are null pointer\n");
		return 0;
	}
	int lt = strlen(text);
	int le = strlen(end);
	if (le > lt) return 0;
	if (strcmp(text+lt-le, end) == 0) return 1;
	return 0;
}

/* Returns 1 if begin is the first part of text, otherwise 0 */
int beginswith(const char * text, const char * begin) {
	if ((text == NULL) || (text == NULL)) {
		message(1, "beginswith: Error: One or both parameter are null pointer\n");
		return 0;
	}
	int lt = strlen(text);
	int lb = strlen(begin);
	if (lb > lt) return 0;
	if (strncmp(text, begin, lb) == 0) return 1;
	return 0;
}

/*
Returns the content the runned program returned
like "echo $DISPLAY" returns ":0"
wdir is the working directory the executed program starts in. If this is NULL
the current working directory will be used.
if multiline is 0 the command returns after the first readed line and
the ending \n char will be replaced by \0.
Note: No more than the 4095 first bytes are returned.
The returned string has to be freed after usage.
 */
char * get_cmd_output(const char * command, const char * wdir,
                      const char * parameter, int multiline) {
	if ((command == NULL) || (parameter == NULL)) {
		message(1, "get_cmd_output: Error: Some parameters were null pointers\n");
		return NULL;
	}
	int	commpipe[2];
	if (pipe(commpipe)) {
		message(1,"get_cmd_output: Error: Pipe generation failed\n");
		return NULL;
	}
	if (fork()) { //the parent
		message(3,"get_cmd_output: I am the parent: to the command '%s'\
 with parameter '%s'\n", command, parameter);
		close(commpipe[1]); //Close unused side of pipe (in side)
		const int buflen = 4096;
		char * readed = smalloc(sizeof(char)*buflen);
		int i = 0;
		for (i = 0; i < buflen-1; i++) {
			if (read(commpipe[0], readed+i, 1) == 0) { //nothing to read
				readed[i] = '\0';
				break;
			}
			if (readed[i] == '\0') {
				break;
			}
			if ((readed[i] == '\n') && (multiline == 0)) { //read newline
				readed[i] = '\0';
				break;
			}
		}
		if (i >= buflen-1) {
			message(1, "get_cmd_output: Error: String was very long\n");
			readed[i-1] = '\0';
		}
		//Possible: wait for the child to exit, but this would take time
		return readed;
	} else { //the child
		close(commpipe[0]); //Close unused side of pipe (out side)
		dup2(commpipe[1], 1);  //Replace stdout with out side of the pipe
		if (wdir != NULL)
			chdir(wdir);
		execlp(command, command, parameter, NULL);
		message(1, "get_cmd_output: Error: execlp '%s' failed\n", command);
		exit(1);
	}
	message(1, "get_cmd_output: Error: Something completely unexpected happened\n");
	return strdup(command);
}

/*
Gets the value of a command line parameter. Thus meaning a null terminated
copy of arg without the first two chars. thus -"oFILE" would return FILE.
The string has to be freed after usage */
char * get_arg_filename(char * arg) {
	if (arg == NULL) {
		message(1, "get_arg_filename: Error: Paramerer is a null pointer\n");
		return NULL;
	}
	int flen = strlen(arg);
	char * filename = smalloc(sizeof(char)*(flen-1));
	memcpy(filename, arg+2, flen-1); //do not copy "-f" but copy "\0"
	return filename;
}

/*
Replaces all chars which are equal to find in text by replacement */
void replace_chars(char * text, char find, char replacement) {
	if (text == NULL) {
		message(1, "replace_chars: Error: Paramerer is a null pointer\n");
		return;
	}
	while (*text != '\0') {
		if ((*text) == find)
			*text = replacement;
		text++;
	}
}

/*
Returns a part of text which begins after the first occurence of before
and ends before the next occurence of the after string.
Before and after may be NULL, resulting starting at the beginning or go to the
ending.
Example:
text_get_between("Hello! I am Malte!", "am ", "!") will return "Malte".
text_get_between("Hello, I am Malte!", "am ", NULL) will return "Malte!".
If no occurence is found, NULL is returned.
*/

char * text_get_between(const char * text, const char * before,
                        const char * after) {
	int beforelen;
	const char * f1;
	if (text == NULL) {
		message(1, "text_get_between: Error: parameter 'text' is a null pointer\n");
		return NULL;
	}
	if (before != NULL) {
		beforelen = strlen(before);
		f1 = strstr(text, before);
	} else {
		beforelen = 0;
		f1 = text;
	}
	if (f1 != NULL) { //beginning found
		f1 += beforelen;
			const char * end;
		if (after != NULL) {
			end = strstr(f1, after);
		} else
			end = text+strlen(text);
		if (end != NULL) { //if ending found
			int anslen = end-f1;
			char * addr = strndup(f1, anslen);
			return addr;
		}
	}
	return NULL;
}

/*
Returns 1 if path exists. Otherwise 0.
*/

int path_exists(const char * path) {
	if (path == NULL) {
		message(1, "path_exists: Error: parameter is a null pointer\n");
		return 0;
	}
	return 1+access(path, F_OK); //access returns 0 or -1
}

/*
Returns 1 if path is an accessable directory on the filesystem, otherwise 0.
*/
int path_is_dir(const char * path) {
	if (path == NULL) {
		message(1, "path_is_dir: Error: parameter is a null pointer\n");
		return 0;
	}
	struct stat stats;
	if (stat(path, &stats) == 0) {
		if (S_ISDIR(stats.st_mode)) {
			return 1;
		}
		return 0;
	}
	message(1, "path_is_dir: Error: Could not get stats from '%s'\n", path);
	return 0;
}

/* Adds a slash to the ending if not there.
The given parameter will be freed and an pointer to the new string
will be returned
*/
char * haveslashending_free(char * path) {
	char * thenew = dirmerge(path, "/");
	free(path);
	return thenew;
}

/*Returns the content of the file as string. Reading is limmited to
to around 100MB
In the case of an error, NULL is returned.
 */

char * readfile(char * filename) {
	if (filename == NULL) {
		message(1, "readfile: Error: parameter is a null pointer\n");
		return NULL;
	}
	FILE * f = fopen(filename, "r");
	char * text = NULL;
	if (f == NULL) {
		message(1, "readfile: Error: could not open '%s'\n",  filename);
		return NULL;
	}
	int toread = 4096;
	int len = 0;
	int bufsize = toread+1;
	text = smalloc(sizeof(char)*bufsize);
	while (bufsize < 100000000) { //100MB
		if (bufsize <= len+toread) {
			bufsize = (len+toread)+bufsize/3*4+1;
			if (bufsize <= len+toread)	{
				message(1, "readfile: Error: Size overflow, exiting\n");
				exit(1);
			}
			text = realloc(text,sizeof(char)*bufsize);
			if (text == NULL) {
				message(1, "readfile: Error: realloc failed, exiting\n");
			}
		}
		int readed = fread(text+len, sizeof(char),toread, f);
		message(4, "Readed '%i' bytes\n", readed);
		len += readed;
		if (readed != toread) { //we got to the end of the file
			text[len] = '\0';
			fclose(f);
			return text;
		}
	}
	message(1, "Error: File '%s' is larger than 100MB, cutting the rest\n");
	text[len] = '\0';
	fclose(f);
	return text;
}
