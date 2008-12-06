
/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

//xml part
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "copyxmler.h"
#include "../share/tools.h"
#include "filemanager.h"
#include "pidmanager.h"
#include "connectionmanager.h"
#include "preprocessing_file.h"
#include "string_helper.h"


char * outputfile;
char * stracefile;
char * wdirfile;

int linecount = 0;

/*if one, files will be saved with the pid = 1. this makes things a lot faster
	if the target program has many childs */
int ignorePIDs = 0;

/*
Sets up the optional parameters given for the program
Returns the number of optional parameters (for using as offset later)
*/
int setup_args(int argc, char *argv[]) {
	int myargs = 0;
	//interesting args
	if (argc < 2) return 0; //nothing to parse
	char * arg = argv[1];
	while (arg[0] == '-') {
		myargs++;
		if (strcmp(arg,"-v") == 0) {
			set_chatting(PGN, 2);
		} else
		if (strcmp(arg,"-vv") == 0) {
			set_chatting(PGN, 3);
		} else
		if (strcmp(arg,"-q") == 0) {
			set_chatting(PGN, 0);
		} else
		if (strcmp(arg,"-d") == 0) {
			set_chatting(PGN, 4);
		} else
		if (strcmp(arg,"-dd") == 0) {
			set_chatting(PGN, 5);
		} else
		if (strcmp(arg,"-noPIDs") == 0) {
			ignorePIDs = 1;
		} else
		if (strcmp(arg, "-o") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -o without spacing.\n");
			exit(2);
		} else
		if (strcmp(arg, "-s") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -s without spacing.\n");
			exit(2);
		} else
		if (strcmp(arg, "-p") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -p without spacing.\n");
			exit(2);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-o", 2) == 0)) {
			outputfile = get_arg_filename(arg);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-s", 2) == 0)) {
			stracefile = get_arg_filename(arg);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-p", 2) == 0)) {
			wdirfile = get_arg_filename(arg);
		} else
		if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"-help") == 0) || (strcmp(arg,"--help") == 0)) {
			printf("Usage: copyxmler [option]\n");
			printf("You may use the following options:\n");
			printf("-v: Be verbose\n");
			printf("-vv: Be very verbose\n");
			printf("-d: Debug messages. Even more verbose than -vv\n");
			printf("-dd: Ultra debug messages. You will be overwhelmed by non understandable messages.\n");
			printf("-q: Be quiet\n");
			printf("-oFILENAME: writes the recorded file accesses to this file, default is: 'record.xml'\n");
			printf("-sFILENAME: reads the strace data from FILENAME, default is: 'strace_temp'\n");
			printf("-pFILENAME: uses FILENAME as beginning working directory, default is: 'path_temp'\n");
			printf("-noPIDs: The in the resulting xml file, can't distinguish between different programs. This improves speed.\n");
			printf("-h, -help, --help: Show this screen\n");
			printf("--version: Shows the version\n");
			exit(0);
		} else
		if (strcmp(arg,"--version") == 0) {
			printf(PGN": Version "PGNVERSION"\n"); //Do not print PID
			exit(0);
		} else {
			message(1, "Error: Option '%s' is unknown\n", arg);
			exit(2);
		}
		if (1+myargs >= argc) //security out
			break;
		arg = argv[1+myargs];
	}
	return myargs;
}

/* Writes out the internal data into a xml file.
Based on:
http://www.xmlsoft.org/examples/testWriter.c
Returns 0 if succeed, otherwise an error code suitable for debugging
*/
int writexml(void) {
	int result;
	message(3, "writexml: starting to save results\n");
	xmlTextWriterPtr writer;
	writer = xmlNewTextWriterFilename(outputfile, 0);
	xmlTextWriterSetIndent(writer, 1); //results in good linebreaks
	if (writer == NULL) return 1;
	result = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);
	if (result < 0) return 2;
	result = xmlTextWriterStartElement(writer, BAD_CAST "content"); //start content tag
	if (result < 0) return 3;
	result = xmlTextWriterStartElement(writer, BAD_CAST "filelist"); //start filelist tag
	if (result < 0) return 4;
	result = files_savexml(writer);
	if (result < 0) return 5;
	result = xmlTextWriterEndElement(writer); // end filelist tag
	if (result < 0) return 6;
	result = xmlTextWriterStartElement(writer, BAD_CAST "metainfo"); //start metainfo tag
	if (result < 0) return 7;
	result = xmlTextWriterStartElement(writer, BAD_CAST "connections"); //start connections tag
	if (result < 0) return 8;
	result = connection_savexml(writer);
	if (result < 0) return 9;
	result = xmlTextWriterEndElement(writer); // end connections tag
	if (result < 0) return 10;
	result = xmlTextWriterEndElement(writer); // end meta tag
	if (result < 0) return 11;
	result = xmlTextWriterEndElement(writer); // end content tag
	if (result < 0) return 12;
	result = xmlTextWriterEndDocument(writer);
	if (result < 0) return 13;
	xmlFreeTextWriter(writer);
	return 0;
}

/*
Uses the which program to determine the path of a called
executable. The returned string has to be freed after use.
If something failed, a copy of command is returned */
char * abs_prog_path(int callerpid, const char * command) {
	if (path_is_relative(command)) {
		char * wdir = pid_get_wdir(callerpid);
		message(5, "abs_prog_path: Call 'which' with wdir: '%s' and file: '%s'\n", wdir, command);
		char * bywhich = get_cmd_output("which", wdir, command, 0);
		free(wdir);
		if (bywhich == NULL) {
			message(1, "abs_prog_path: Error: Calling 'which' failed\n");
			return strdup(command);
		}
		char * bypidpath;
		if (strlen(bywhich) == 0) {
			//maybe the file was deleted or which did a mistake, try with the wdir.
			message(1, "Warning: 'which' was not able to get the absolute path for '%s'\n", command);
			bypidpath = pid_get_absolute_path(callerpid, command);
		} else if (path_is_relative(bywhich)) {
			//was relative and/or not in the PATH environment
			bypidpath = pid_get_absolute_path(callerpid, bywhich);
		}	else {
			return bywhich;
		}
		free(bywhich);
		return bypidpath;
	} else
		return strdup(command); //is already absolute
}

/*
Processes each line of the already preprocessed strace output */
void process_line(const char * line, char ** firstwdir) {
	if (strstr(line, "<unfinished ...>") != NULL) {
		message(1, "main: Warning: Processing begin incomplete line '%s'\n", line);
	}
	if (strstr(line, "resumed> ") != NULL) {
		message(1, "main: Warning: Processing end of incomplete line '%s'\n", line);
	}
	//Determine pid and the name of the syscall
	int pid = extract_pid(line);
	char * syscall = extract_syscall(line);
	//Handle each syscall in their own way
	if (strcmp(syscall, "clone") == 0) { //if true: add the child
		int childpid = get_retvalue(line);
		message(2, "main: clone called by %i, new pid is: %i\n",pid , childpid);
		pid_add_child(pid, childpid);
		//as this is a syscall which does not use extract_filename()
		goto end_free;
	}
	if (strcmp(syscall, "vfork") == 0) { //if true: add the child
		int childpid = get_retvalue(line);
		message(2, "main: vfork called by %i, new pid is: %i\n",pid , childpid);
		pid_add_child(pid, childpid);
		//as this is a syscall which does not use extract_filename()
		goto end_free;
	}
	if (strcmp(syscall, "connect") == 0) { //if true: add connection to hash-table
		char * type = extract_connectiontype(line);
		if (type != NULL) {
			if (strcmp(type, "AF_INET") == 0) {
				char * host = extract_inetaddr(line);
				int port = extract_connport(line);
				//message(1, "main: connect called by %i, addr: %s, port: %i\n",pid, host, port);
				connection_add(pid, host, port);
			} else if (strcmp(type, "AF_FILE") == 0) {
				//not important, are sockets in /var/run or /tmp, which are not copied
			} else if  (strcmp(type, "AF_INET6") == 0) {
				char * host = extract_inet6addr(line);
				int port = extract_conn6port(line);
				connection_add(pid, host, port);
			} else
				message(1, "process_line: Warning: '%s' connection type not handled\n", type);
		} else
			message(1, "process_line: Error: No connection type found in '%s'\n", line);
		//as this is a syscall which does not use extract_filename()
		free(type);
		goto end_free;
	}
	//now all syscalls which are using filenames
	char * filename = extract_filename(line);
	if (strcmp(syscall, "open") == 0) { //it true: add filename to hash-table
		const char * ending = filename_ending(line);
		char * ro = strstr(ending, "O_RDONLY");
		char * wo = strstr(ending, "O_WRONLY");
		char * rw = strstr(ending, "O_RDWR");
		int mode = 0;
		if (ro != NULL) mode |= FO_READ;
		if (wo != NULL) mode |= FO_WRITE;
		if (rw != NULL) mode |= FO_READ | FO_WRITE;
		char * absfilename = pid_get_absolute_path(pid, filename);
		file_add(absfilename, mode, pid);
		free(absfilename);
	} else
	if ((strcmp(syscall, "stat64") == 0) || (strcmp(syscall, "lstat64") == 0) ||
		 (strcmp(syscall, "readlink") == 0) || (strcmp(syscall, "getcwd") == 0)) {
		//all those four syscalls can be treated equal: add used filename
		if (strlen(filename) > 0) {
			char * absfilename = pid_get_absolute_path(pid, filename);
			file_add(absfilename, FO_READ, pid); //read may be not correct, but fits best
			free(absfilename);
		} else
			message(1, "process_line: Warning: '%s' called, but filename has a length"
			           " of zero, line: %i\n", syscall, get_linecount());
	} else
	if (strcmp(syscall, "access") == 0) { //if true: add filename with used mode
		const char * ending = filename_ending(line);
		char * exis = strstr(ending, "F_OK");
		char * ro = strstr(ending, "R_OK");
		char * wo = strstr(ending, "W_OK");
		char * xo = strstr(ending, "X_OK");
		int mode = 0;
		if (exis != NULL) mode |= FO_READ; //not exact, but we have to read for copying
		if (ro != NULL) mode |= FO_READ;
		if (wo != NULL) mode |= FO_WRITE;
		if (xo != NULL) mode |= FO_EXEC;
		message(3, "main: %i, %s\n",pid , filename);
		char * absfilename = pid_get_absolute_path(pid, filename);
		file_add(absfilename, mode, pid);
		free(absfilename);
	} else
	if (strcmp(syscall, "execve") == 0) {
		//gets absolute file path and adds it
		if (*firstwdir != NULL) {
			/*Has to be done before the abs_prog_path() call
			because the first command may be relative */
			pid_add_entry(pid, *firstwdir);
			free(*firstwdir);
			*firstwdir = NULL;
		}
		char * executed = abs_prog_path(pid, filename);
		message(2, "main: %i, %s\n",pid , executed);
		file_add(executed, FO_EXEC, pid);
		free(executed);
	} else
	if (strcmp(syscall, "chdir") == 0) {
		//follows successful chdir.
		int succ = get_retvalue(line);
		message(2, "main: chdir called by %i, changes %s, returned: %i\n", pid,
						filename, succ);
		if (succ == 0) {
			pid_update_path(pid, filename);
		}
	}
	free(filename);
end_free:
	free(syscall);
}


/*
The main function.
Opens files
Runs some pre-processing
Then handle each line
At last save everything as xml
*/
int main(int argc, char *argv[]) {
	set_chatting(PGN, 1);
	files_initmemory();
	connection_initmemory();
	if (argc < 1) {
		message(0, "main: Error, the first parameter should be the own program name, but no parameter was given\n");
		exit(1);
	}
	setup_args(argc, argv);
	message(1, "Welcome to '%s'\n", argv[0]);
	//check if compiled version fits with libxml on the system
	LIBXML_TEST_VERSION
	//setup filenames
	if (outputfile == NULL) outputfile = "record.xml";
	if (stracefile == NULL) stracefile = "strace_temp";
	if (wdirfile == NULL) wdirfile = "path_temp";
	//open input files
	FILE * stracedata = fopen(stracefile, "r");
	FILE * firstdir = fopen(wdirfile, "r");
	message(4, "main: %p, %p\n", stracedata, firstdir);
	if ((stracedata != NULL) && (firstdir != NULL)) {
		//some concatenations and removements
		stracedata = strace_preparation(stracedata);
		unsigned int buflen = INITIAL_BUF_LEN;
		char * line = smalloc(sizeof(char) * buflen);
		int length = 0;
		linecount = 0;
		//get first wdir
		char * firstwdir = smalloc(sizeof(char) * buflen);
		int firstwdirlen = getline(&firstwdir, &buflen, firstdir);
		if (firstwdir[firstwdirlen-1] == '\n') firstwdir[firstwdirlen-1] = '\0';
		//files
		message(1, "main: Processing files...\n");
		do {
			length = getline(&line, &buflen, stracedata);
			if (length < 1) break;
			message(3, "main: processing: %s\n", line);
			process_line(line, &firstwdir);
			if (linecount % 1000 == 0)
				message(1, "main: Line number: %i\r", linecount);
			linecount++;
		} while (length > 0);
		message(1, "main: %i lines proceeded        \n", linecount);
		message(1, "main: Writing xml...\n");
		int wretcode = writexml();
		if (wretcode) {
			message(1, "main: Error: writexml() failed with the error code %i\n",
			        wretcode);
		}
	} else
		message(1, "main: Error: One or more inputfiles could not be opened\n");
	if (stracedata != NULL)
		fclose(stracedata);
	if (firstdir != NULL)
		fclose(firstdir);
	message(1, "main: Done\n");
	return 0;
}

/*
Returns the current processed line. Useful for error messages */
int get_linecount() {
	return linecount;
}

/*
Returns 1 if the different pids should be ignored. Set by the optional
command line parameter.
*/
int get_ignorepids() {
	return ignorePIDs;
}
