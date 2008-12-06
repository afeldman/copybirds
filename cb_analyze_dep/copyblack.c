
/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "../lib/hash-table.h"

#include "../share/tools.h"
#include "../share/dirtools.h"
#include "../share/xmlhelper.h"
#include "../share/packagestates.h"

#include "copyblack.h"

#include "datamanager.h"

char * outputfile;

int setup_args(int argc, char *argv[]) {
	int myargs = 0;
	//interesting args
	if (argc < 2)
		return 0;
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
		if (strcmp(arg, "-o") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -o without spacing.\n");
			exit(2);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-o", 2) == 0)) {
			outputfile = get_arg_filename(arg);
		} else
		if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"-help") == 0) || (strcmp(arg,"--help") == 0)) {
			printf("License: GPL Version 2.0\n");
			printf("[option] XMLFILE [more xmlfiles]\n");
			printf("Normally the program returns: 0 if succeed, 1 if an error occurred\n");
			printf("You may use the following options:\n");
			printf("-v: Be verbose\n");
			printf("-vv: Be very verbose\n");
			printf("-d: Debug messages. Even more verbose than -vv\n");
			printf("-dd: Ultra debug messages. You will be overwhelmed by non understandable messages.\n");
			printf("-q: Be quiet\n");
			printf("-o: give name of output blacklist file\n");
			printf("-h, -help, --help: Show this screen\n");
			printf("--version: Shows the version\n");
			exit(0);
		} else
		if (strcmp(arg,"--version") == 0) {
			printf(PGN": Version "PGNVERSION"\n"); //Do not print PID
			exit(0);
		} else {
			message(1, "Error: Option '%s' is unknown\n", arg);
			exit(0);
		}
		if (1+myargs >= argc) //security out
			break;
		arg = argv[1+myargs];
	}
	return myargs;
}


void add_if_ok(const char * filename) {
	if (file_exists(filename) == 0) { //new file
		//does it exsist on the file system?
		int state = access(filename, F_OK); //access is a syscall
		if (state == 0) { //yes it does
			char * filenameclean = nodoubleslash(filename);
			file_add(filenameclean);
			free(filenameclean);
		} else
			message(2, "readxml: Warning: File %s not found\n", filename);
	}
}

void * rec_handler(const char * base, const char * name, void * data) {
	//avoid compiler warning ;-)
	if (data != NULL)
		message(4, "rec_handler: Warning: I got data but expected nothing\n");
	char * newsource =  dirmerge(base, name);
	//check if file or directory
	struct stat stats;
	if (lstat(newsource, &stats) == 0) {
		if (S_ISLNK(stats.st_mode)) { // a link
			add_if_ok(newsource);
		} else if (S_ISREG(stats.st_mode)) { //normal file
			add_if_ok(newsource);
		}
	}
	free(newsource);
	return NULL;
}

/*
Reads in all xml files and adds all files which exists in the internal storage.
All found pids get added into the storage too. In order to avoid collisions of
pids by the same pid in multiple files, each pid gets an offset of
filenum*2^15 */

/* Parsing based on:
http://xmlsoft.org/examples/parse1.c
*/
void readxml(char * filename) {
	//put all copyable files into the list
	message(2, "readxml: Procesing '%s'...\n", filename);
	xmlDocPtr doc;
	xmlNodePtr root_element = xml_open_file(filename, "content", &doc);
 	if (root_element == NULL) {
		message(1, "readxml: Aborting function due to the previous errors.\n", filename);
		return;
	}
	//get files tag
	xmlNodePtr files = xml_find_node(root_element, "filelist");
	if (files != NULL) {
		//go through files
		xmlNodePtr cur_node = NULL;
		for (cur_node = files->children; cur_node; cur_node = cur_node->next) {
			if (xml_is_usefulnode(cur_node)) { //found file node
				char * filename =  xml_get_attribute(cur_node, "name");
				message(4, "readxml: filename: %s\n", filename);
				if (endswith(filename, "/*")) { //handle wildcards
					filename[strlen(filename)-1] = '\0'; //remove '*' char
					recursive_dir(filename, rec_handler, NULL, 1);
				}	else
					add_if_ok(filename);
				free(filename);
			}
		}
	} else
		message(3, "readxml: File does not contain a <filelist> tag\n");
	xml_close_file(doc);
}


void * readpackagelist(const char * base, const char * filename, void * fileno) {
	/* If the filename ends with .list, the filename is opened and all containing
		filenames were added to the hash-table.
		The name of the package is extracted from the filename.
	*/
	if (endswith(filename, ".list")) {
		int c = *(int *) fileno;
		c++;
		*(int *)fileno = c;
		message(1, "readpackagelist: Reading file %i\r", c);
		char * package = text_get_between(filename, NULL, ".list");
		char * filepath = dirmerge(base, filename);
		FILE * packagefile = fopen(filepath, "r");
		if (packagefile == NULL) {
			message(1, "readpackagelist: Error: Could not open '%s'. Exiting\n", filename);
			exit(1);
		}
		int length = 0;
		do {
			unsigned int buflen = INITIAL_BUF_LEN;
			char * name = NULL;
			length = getline(&name, &buflen, packagefile);
			if (length == 0) break;
			replace_chars(name, '\n', '\0'); //no newlines
			if (length > 1) {
				package_file_add(package, name);
			}
			free(name);
		} while (length > 0);
		free(package);
		fclose(packagefile);
	}
	return fileno;
}

void get_packages_and_write(const char * filename) {
	/* Checks for each file from the input xml files, if they are
		somewhere in the read .list files too. If this happens, the file is
		blacklisted and the name of the package is added to the output file.
	*/
	//prepare xml file
	xmlKeepBlanksDefault(0); //results in good linebeaks -> better human readable
	xmlDocPtr doc = NULL;
	xmlNodePtr base = NULL;
	if (access(filename, F_OK) == 0) {
		base = xml_open_file(filename, "content", &doc);
		message(1, "get_packages_and_write: Update existing file\n");
	} else { //we need a new file
		message(1, "get_packages_and_write: Creating a new output file\n");
		base = xml_new_file("content", &doc);
	}
	if (base == NULL) {
		message(1, "get_packages_and_write: Error: Nothing to write on. Aborting\n");
		return;
	}
	//find /make blacklist entry
	xmlNodePtr blacklist = xml_have_node(base, "blacklist");
	file_start_iterate();
	while (file_has_next()) {
		const char * filename = file_get_next();
		if (path_is_dir(filename) == 0) {
			/* Directories are trivial things which may be created directly AND
			are provided by any package which stores files in them, resulting in
			some curious packages which are not needed*/
			pfstore_t * pf = package_file_get(filename);
			if (pf != NULL) {
				pstore_t * ps = pf->package;
				message(2, "get_packages_and_write: Found '%s' for file '%s'\n",
				        ps->name, filename);
				//add blacklist
				xml_have_infonode(blacklist, "file", "name", filename);
				//add package (unifies)
				findings_add(ps);
				if (ps->installed == 0) {
					message(1, "get_packages_and_write: Warning: the file '%s' from the \
uninstalled package '%s' is used\n", filename, pf->package->name);
				}
				if (pf->refcount > 1) {
					message(1, "get_packages_and_write: Warning: file '%s' is provided by\
 %i different packages\n", filename, pf->refcount);
				}
			}
		}
	}
	//update package meta informations
	xmlNodePtr metainfo = xml_have_node(base, "metainfo");
	xmlNodePtr depend = xml_have_node(metainfo, "depend");
	findings_start_iterate();
	while (findings_has_next()) {
		pstore_t * ps = findings_get_next();
		if (ps != NULL) {
			xmlNodePtr pn = xml_have_infonode(depend, "package", "name", ps->name);
			if (ps->version != NULL) {
				xml_have_content(pn, "version", ps->version);
			}
		}
	}
	//close file
	xmlSaveFormatFile(filename, doc, 1);
}

int main(int argc, char *argv[]) {
	set_chatting(PGN, 1);
	files_initmemory();
	int argoffset = setup_args(argc, argv);
	if (argc < 2) {
		message(1, "main: Error: give parameters: [option] XMLFILE [more xmlfiles], or try -h\n");
		return 1;
	}
	if (argc > 32000) {
		message(1, "main: Error: %i program arguments are too much. 32000 is the maximum.\n", argc);
		return 1;
	}
	message(1, "Welcome to '%s'\n", argv[0]);
	/*Parse all xml files from 1+argoffset to argc-1 (without an output directory)
	 or argc-2 (if there is an output directory)
	*/
	message(1, "main: Parsing xml input file(s)...\n");
	int i = 0;
	int files = argc-argoffset-1;
	if (files <= 0)	{
		message(1, "main: Error no input file\n");
		return 1;
	}
	for (i = 0; i < files; i++) {
		readxml(argv[1+argoffset+i]);
	}
	//Open package meta info file and put into hashtable
	package_states_read("/var/lib/dpkg/status");
	//Open file lists of packages and put into hashtable
	message(1, "main: Reading package files...\n");
	int filecounter = 0;
	recursive_dir("/var/lib/dpkg/info", readpackagelist, &filecounter, 0);
	//prepare output file for writing information into it
	if (outputfile == NULL)
		outputfile = "packagemeta.xml";
	//Generate package list and write
	get_packages_and_write(outputfile);
	return 0;
}
