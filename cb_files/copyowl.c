/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

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

#include "copyowl.h"
#include "../share/tools.h"
#include "../share/dirtools.h"
#include "../share/xmlhelper.h"
#include "datamanager.h"
#include "cloner.h"

/*
Refs:
Code based on:
http://ezxml.sourceforge.net/
*/

#define ALWAYSLINK_C 6
const char alwayslink[ALWAYSLINK_C][32] = {"/dev", "/proc", "/tmp", "/bin", "/sbin", "/sys"};


int mode = 0;
int makelinks = 0;
int defaultblacklist = 1;
char * xmldevices;

int setup_args(int argc, char *argv[]) {
	int myargs = 0;
	//interesting args
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
		if (strcmp(arg,"-list") == 0) {
			mode = 1;
		} else
		if (strcmp(arg,"-blacklist") == 0) {
			mode = 3;
		} else
		if (strcmp(arg,"-stats") == 0) {
			mode = 2;
		} else
		if (strcmp(arg,"-imagesize") == 0) {
			mode = 4;
		} else
		if (strcmp(arg,"-makelinks") == 0) {
			makelinks = 1;
		} else
		if (strcmp(arg,"-nodefaultblacklist") == 0) {
			defaultblacklist = 0;
		} else
		if (strcmp(arg, "-x") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -x without spacing.\n");
			exit(2);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-x", 2) == 0)) {
			xmldevices = get_arg_filename(arg);
		} else
		if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"-help") == 0) || (strcmp(arg,"--help") == 0)) {
			printf("License: GPL Version 2.0\n");
			printf("[option] XMLFILE [more xmlfiles] [outputdirectory]\n");
			printf("Normally the program returns: 0 if succeed, -1 if an error occured\n");
			printf("You may use the following options:\n");
			printf("-v: Be verbose\n");
			printf("-vv: Be very verbose\n");
			printf("-d: Debug messages. Even more verbose than -vv\n");
			printf("-dd: Ultra debug messages. You will be overwhelmed by non understandable messages.\n");
			printf("-q: Be quiet\n");
			printf("-makelinks: Make some default links, suitable to run with fakechroot\n");
			printf("-nodefaultblacklist: Does even copy the content of general not copied directories\n");
			printf("-xFILENAME: Writes/Updates all accessed devices into the given XML file\n");
			printf("-list: Prints out all filenames, which are avariable on the filesystem\n");
			printf("-blacklist: Prints out all patterns, which should not be copied\n");
			printf("-imagesize: Returns the total size of all files in megabytes (rounded up)\n");
			printf("-stats: Shows a lot of informations about the content of the xml files\n");
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

/*
Prints all files which would be copied */
void list(void) {
	file_start_iterate();
	while (file_has_next()) {
		printf("%s\n", file_get_next());
	}
}

/*
Source about getting the file size:
http://c-faq.com/osdep/filesize.html
*/
/*
Set onlyblacks to 1 on order to get the size of all blacklisted files.
Use 0 to get the size of all files which should be copied
*/
long long get_files_size(int onlyblacks) {
	/*TODO: If a file is referenced directly and by a link this results in
	 counting the file twice */
	long long fsizes = 0;
	message(1, "stats: Calculating size of all files...\n");
	file_start_iterate();
	while (file_has_next()) {
		const char * filename = file_get_next();
		if (bl_check(filename) == onlyblacks) {
			//does it exsists? (whe had checked this already, but we do it once again)
			int state = access(filename, F_OK);
			if (state == 0) {
				struct stat stats;
				if (stat(filename, &stats) == 0) {
					if (stats.st_size >= 0) {
						fsizes += stats.st_size;
					} else
						message(1, "get_file_size: Error: '%s' reported a negative file size. Ignoring\n", filename);
				} else
					message(1, "stats: Error: Could not determine the size of the file '%s'\n", filename);
			} else
			message(2, "stats: Warning: File '%s' not found\n", filename);
		}
	}
	return fsizes;
}

void stats(void) {
	long long fsizes = get_files_size(0);
	long long fsizesall = get_files_size(1);
	fsizesall += fsizes;
	printf("Different pids %i\n", pid_count());
	printf("Existing files: %i\n", file_count());
	printf("Size of all files which would be copied: %lli KB = %lli MB\n", fsizes/1024, fsizes/1024/1024);
	printf("Size of all files including blacklisted: %lli KB = %lli MB\n", fsizesall/1024, fsizesall/1024/1024);
}

/*
Gives the returns the size of all files as program exit code in megabyte */
void ret_fsizes(void) {
	long fsizes = get_files_size(0);
	fsizes--; //we round up to full MB (in the base of 1000)
	fsizes /= 1000*1000;
	fsizes++;
	exit(fsizes);
}

/*
target will get the rootdir infront
source not. so it may point outside of the rootdir or can be relative to the target */
void link_entry(const char * source, const char * target, const char * rootdir) {
	char * linkname = dirmerge(rootdir, target);
	if (path_exists(linkname) == 0) {
		if (fork() == 0) { // the child
			execlp("ln", "ln", "-s", source, linkname, NULL);
		}
	} else
		message(1, "link_entry: Warning: Linkname for general link '%s' exists already\n", linkname);
	free(linkname);
}

/* Tries to copy all files stored in the hash-table into the rootdir.
*/
void copycontent(char * rootdir) {
	/*Check if not something unexpected, like a file, has to be done before adding
	  a possible slash.
	  Note: This does not catch if the user added already a '/' but part of the
	  path is a file or something invalid.
	  But it does work if the user runs *.xml as parameter and forgets the target
	  directory as this would result otherwise in trying to use the last xml file
	  as target directory, which would result in tons of error messages.
	*/
	if ((path_exists(rootdir)) && (path_is_dir(rootdir) == 0)) {
		message(1, "copycontent: Error: Target '%s' exists but is not a directory. Exiting.\n", rootdir);
		exit(1);
	}
	//add / if not there
	if (rootdir[strlen(rootdir)-1] != '/')
		rootdir = dirmerge(rootdir, "/");
	//now copy
	message(1, "copycontent: Copying files...\n");
	int i = 0;
	file_start_iterate();
	while (file_has_next()) {
		const char * filename = file_get_next();
		if (path_is_relative(filename) == 0) {
			cloner_tokenize(filename, rootdir);
		} else
			message(1, "copycontent: Error: Not processing '%s'. Path is relative to what?\n", filename);
	}
	if (makelinks) {
		//make some links
		message(1, "copycontent: Making some general links\n");
		for (i = 0; i < ALWAYSLINK_C; i++) {
			const char * source = alwayslink[i];
			link_entry(source, source, rootdir);
		}
	}
}

/*
Adds the file to the internal table if accessing the file works and is not
alreay in the hash-table.*/
void add_if_ok(const char * filename) {
	if (file_exists(filename) == 0) { //not existing -> new file
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

/*
Is used to resolve wildcards in xml input data. Gets called for every entry
found in the directory */
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
All found pids get added into the storage too. In order to avoid collosions of
pids by the same pid in multiple files, each pid gets an offset of
filenum*2^15 */

/* Parsing based on:
http://xmlsoft.org/examples/parse1.c
*/
void readxml(char * filename, int filenum) {
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
				char * filename = xml_get_attribute(cur_node, "name");
				int pid = xml_get_number_attr(cur_node, "pid");
				//filenum*65536: avoid conflicting with same pids in other xml files
				pid_add(pid+filenum*65536);
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
	//get blacklist tag
	xmlNodePtr blacklist = xml_find_node(root_element, "blacklist");
	if (blacklist != NULL) {
		//go through files
		xmlNodePtr cur_node = NULL;
		for (cur_node = blacklist->children; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE) { //found file node
				char * filename =  xml_get_attribute(cur_node, "name");
				message(4, "readxml: filename: %s\n", filename);
				char * filenameclean = nodoubleslash(filename);
				bl_add(filenameclean);
				free(filenameclean);
				free(filename);
			}
		}
	} else
		message(3, "readxml: File does not contain a <filelist> tag\n");
	xml_close_file(doc);
}

/*
Updates the names of all found devices into the given filename */
void update_device_xml(char * filename) {
	message(1, "update_device_xml: Writing used devices into xml file...\n");
	xmlKeepBlanksDefault(0); //results in good linebeaks -> better human readable
	xmlDocPtr doc = NULL;
	xmlNodePtr base = NULL;
	base = xml_have_file(filename, "content", &doc);
	if (base == NULL) {
		message(1, "writexml: Error: Nothing to write on. Aborting\n");
		return;
	}
	xmlNodePtr meta = xml_have_node(base, "metainfo");
	xmlNodePtr devices = xml_have_node(meta, "devices");
	device_start_iterate();
	while (device_has_next()) {
		const char * devicepath = device_get_next();
		xml_have_infonode(devices, "device", "name", devicepath);
	}
	xmlSaveFormatFile(filename, doc, 1);
}

int main(int argc, char *argv[]) {
	set_chatting(PGN, 1);
	datamanager_initmemory();
	if (argc < 2) {
		message(1, "main: Error: give paramaeters: [option] XMLFILE [more xmlfiles] [outputdirectory], or try -h\n");
		return -1;
	}
	if (argc > 32000) {
		message(1, "main: Error: %i program arguments are too much. 32000 is the maximum.\n", argc);
		return -1;
	}
	int argoffset = setup_args(argc, argv);
	message(1, "Welcome to '%s'\n", argv[0]);
	//switch to the true path, removing symlinks
	char * wdir = smalloc(BUF_SIZE);
	if (getcwd(wdir, BUF_SIZE) != NULL) {
		message(2, "main: Working dir: '%s'\n", wdir);
		char * twdir = truepath(wdir);
		if (strcmp(wdir, twdir) != 0) {
			message(1, "main: The true working directory is: '%s'\n", twdir);
			chdir(twdir);
		}
		free(wdir);
		free(twdir);
	}
	//load default blacklist if wished
	if (defaultblacklist) {
		int i;
		for (i = 0; i < ALWAYSLINK_C; i++) {
			char * tmp = merge(alwayslink[i], "*");
			bl_add(tmp);
			free(tmp);
		}
	}
	/*Parse all xml files from 1+argoffset to argc-1 (without an output directory)
	 or argc-2 (if there is an output directory)
	*/
	message(1, "main: Parsing xml input file(s)...\n");
	int i = 0;
	int files = argc-argoffset-1;
	if (mode == 0) files--;
	if (files <= 0)	{
		message(1, "main: Error no input file\n");
		return -1;
	}
  LIBXML_TEST_VERSION
	for (i = 0; i < files; i++) {
		readxml(argv[1+argoffset+i], i);
	}
	message(2, "main: Parsing done\n");
	//run wished operation
	if (mode == 1) {
		list();
	} else if (mode == 2) {
		stats();
	} else if (mode == 4) {
		ret_fsizes();
	} else if (mode == 0) {
		if (argc > argoffset+2) {
			copycontent(argv[argc-1]);
			if (xmldevices != NULL) {
				update_device_xml(xmldevices);
			}
		} else
			message(1, "main: Error: Please add a target parameter for copying\n");
	} else
		message(1, "main: Error: Feature not implemented yet\n");
	return 0;
}

