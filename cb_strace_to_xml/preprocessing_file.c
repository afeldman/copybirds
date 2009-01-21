/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "../share/tools.h"
#include "dlist.h"
#include "string_helper.h"
#include "copyxmler.h"

struct prepbuf {
	list_head_t head;
	char * line;
};

#define prepbuf_t struct prepbuf

list_head_t prepfifo;

/*
Adds a string to the internal FIFO */
static void add(char * line) {
	prepbuf_t * n = (prepbuf_t *)smalloc(sizeof(prepbuf_t));
	n->line = line;
	list_add(&(n->head),&prepfifo);
}

/*
Returns the string which is already the longest time in the FIFO.
The returned pointer is NOT a copy. So do not free it.
If the FIFO is empty, NULL is returned.
 */
static char * peek_oldest(void) {
	if (list_empty(&prepfifo) == 0)
		return ((prepbuf_t *)(prepfifo.prev))->line;
	message(1, "peek_oldest: Error: List is empty\n");
	return NULL;
}

/*
Returns the string which is already the longest time in the FIFO and removes
the entry from the fifo. The returned string should be freed after usage.
If the FIFO is empty, NULL is returned */
static char * get_oldest(void) {
	if (list_empty(&prepfifo) == 0) {
		char * text = peek_oldest();
		free(list_del(prepfifo.prev));
		return text;
	}
	message(1, "get_oldest: Error: List is empty\n");
	return NULL;
}

/*
Returns a pointer to the struct which is already the longest time in the FIFO.
*/
static prepbuf_t * peek_oldest_struct(void) {
	if (list_empty(&prepfifo) == 0) {
		return (prepbuf_t *)(prepfifo.prev);
	}
	message(1, "peek_oldest_struct: Error: List is empty\n");
	return NULL;
}

/*
Returns a pointer to the struct which has spend the least time in the FIFO.
*/
static prepbuf_t * peek_younger_struct(const prepbuf_t * older) {
	list_head_t * youngerhead = older->head.prev;
	if (youngerhead == &prepfifo) {
		message(4, "peek_younger_struct: Warning: No younger entry found\n");
	}
	return (prepbuf_t *)(youngerhead);
}

/*
Returns a string consisting of the first and second parameter.
The merging tries to find the last '<' in the first and the earliest '>' char
in the second parameter. The merging goes up to before the '<' and begins
after the '>' char.
Example "foo < incomplete >" and "< continue > bar" is merged to "foo  bar"
If one or both strings are missing their searched characters, a simple
concatenated string of both input strings is returned.
The returned string should be freed after usage.
*/
static char * fit_together(const char * first, const char * second) {
	char * usefulsecond = (strchr(second, '>')+1);
	if (usefulsecond != NULL) {
		char * uffirst_end = strrchr(first, '<');
		if (uffirst_end != NULL) {
			int uflen = uffirst_end-first;
			char * a = strndup(first, (uflen));
			char * result = merge(a, usefulsecond);
			free(a);
			return result;
		}
	}
	message(1, "fit_together: Error: Could not find useful < or > marks in strings\n");
	return merge(first, second);
}

/*
Reads from the fifo and tries to concatenate the strings from a strace
output. The concatenated strings are written to the outputfile.
If flush is '1' everything within the FIFO is written to the output file,
even if there are non concatenated lines in it. */
static void process_buffer(FILE * outputfile, int flush) {
	while (list_empty(&prepfifo) == 0) {
		char * line = peek_oldest();
		if (strstr(line, " resumed>") != NULL) { //this should not happen
			message(1, "process_buffer: Error: Got resumed as oldest element. This is most likely wrong. Removing line '%s'\n", line);
			line = get_oldest();
			free(line);
			continue;
		}
		char * sys = extract_syscall(line);
		if (strstr(line, " <unfinished ...>") != NULL) {
			//message(1, "found unfinished\n");
			int pid = extract_pid(line);
			//seek fitting <... sys resumed> with right pid string
			prepbuf_t * probe = peek_younger_struct(peek_oldest_struct());
			char * a = merge("<... ", sys);
			char * probeseek = merge(a, " resumed>");
			free(a);
			while ((list_head_t *)probe != &prepfifo) {
				char * probeline = probe->line;
				int probepid = extract_pid(probeline);
				if (probepid == pid) {
					if (strstr(probeline, probeseek) != NULL) { //looks good
						char * fittext = fit_together(line, probeline);
						//message(1, "merge '%s', with '%s', resulting in '%s'\n", line, probeline, fittext);
						fwrite(fittext, sizeof(char), strlen(fittext), outputfile);
						line = get_oldest();
						free(fittext);
						free(line);
						free(probeline);
						free(list_del(&(probe->head)));
						break;
					}
				}
				probe = peek_younger_struct(probe);
			}
			free(probeseek);
			free(sys);
			if ((flush) && (list_empty(&prepfifo) == 0)) {
				line = get_oldest();
				fwrite(line, sizeof(char), strlen(line), outputfile);
				free(line);
				continue;
			} else
				break;
		}
		if ((strcmp(sys, "open") == 0) || (strcmp(sys, "access") == 0) ||
		    (strcmp(sys, "execve") == 0) || (strcmp(sys, "clone") == 0) ||
		    (strcmp(sys, "chdir") == 0) || (strcmp(sys, "stat64") == 0) ||
		    (strcmp(sys, "lstat64") == 0) || (strcmp(sys, "readlink") == 0) ||
		    (strcmp(sys, "getcwd") == 0) || (strcmp(sys, "connect") == 0) ||
		    (strcmp(sys, "vfork") == 0)) {
			//got a normal line, just write to the file.
			line = get_oldest();
			fwrite(line, sizeof(char), strlen(line), outputfile);
			free(line);
		} else { //unknown data. removing from output
			message(2, "process_buffer: Warning: Removing unknown line '%s'\n", line);
			line = get_oldest();
			free(line);
		}
		free(sys);
	}
}


/*
Reads in the file and tries to concaterate syscalls which go over multiple
lines because an other process came between entering and returning from the
syscall. The Original file is closed and an opened temporary file with the
concatenated content is returned.
As side effect, unwanted syscalls and lines without a syscall name are filtered
out too.
 */
FILE * strace_preparation(FILE * runfile) {
	FILE * tempfile = fopen("strace_prepro_temp", "w");
	if (tempfile == NULL) {
		message(1, "strace_preparation: Error: Could not generate an output file\n");
		return runfile;
	}
	message(1, "strace_preparation: Preprocessing strace data...\n");
	//some init
	list_init(&prepfifo);
	int length = 0;
	do {
		unsigned int buflen = INITIAL_BUF_LEN;
		char * readbuffer = (char *)smalloc(sizeof(char)*buflen);
		length = getline(&readbuffer, &buflen, runfile); //uses realloc if needed
		if (length > 0) { //if there is an empty line as EOF, getline forgets the \0
			add(readbuffer);
		} else {
			//the strlen MAY result in a segfault...
			message(4, "strace_preparation: Warning: empty line has length %i\n", strlen(readbuffer));
			free(readbuffer);
		}
		process_buffer(tempfile, 0);
	} while (length > 0);
	process_buffer(tempfile, 1); //flush fifo
	//converting done
	fclose(tempfile);
	fclose(runfile);
	FILE * newfile = fopen("strace_prepro_temp", "r");
	return newfile;
}
