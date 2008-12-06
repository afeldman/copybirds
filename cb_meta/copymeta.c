
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <GL/glut.h>

#include "copymeta.h"
#include "../share/tools.h"
#include "../share/xmlhelper.h"

#include "../share/tools.h"
#include "../share/xmlhelper.h"

#include "comparesys.h"

sysinfo_t csys; //current system
sysinfo_t osys; //system read from the xml file

char * usercomment;

xmlNodePtr devicesp;
xmlNodePtr packagesp;
xmlNodePtr commandsp;
xmlNodePtr connectionsp;

char * command_file;

//the mode of operation
int mode = 0;

int setup_args(int argc, char *argv[]) {
	if (argc < 2) return 0;
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
		if (strcmp(arg,"-update") == 0) {
			mode = 1;
		} else
		if (strcmp(arg,"-printpackages") == 0) {
			mode = 2;
		} else
		if (strcmp(arg,"-printcomment") == 0) {
			mode = 3;
		} else
		if (strcmp(arg,"-printuserhome") == 0) {
			mode = 4;
		} else
		if (strcmp(arg,"-printcommands") == 0) {
			mode = 5;
		} else
		if (strcmp(arg,"-printconnections") == 0) {
			mode = 6;
		} else
		if (strcmp(arg, "-c") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -c without spacing.\n");
			exit(1);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-c", 2) == 0)) {
			command_file = get_arg_filename(arg);
		} else
		if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"-help") == 0) ||
			  (strcmp(arg,"--help") == 0)) {
			printf("License: GPL Version 2.0\n");
			printf("[options] [xmlfile]\n");
			printf("If a xml file is given it display differences to the current system\n");
			printf("Normally the program returns: 0 if succeed, 1 if an error occurred\n");
			printf("You may use the following options:\n");
			printf("-v: Be verbose\n");
			printf("-vv: Be very verbose\n");
			printf("-d: Debug messages. Even more verbose than -vv\n");
			printf("-dd: Ultra debug messages. You will be overwhelmed by non understandable messages.\n");
			printf("-q: Be quiet\n");
			printf("-update: Writes the meta info into this file. Existing meta informations will be updated\n");
			printf("-cFILE: Adds a command from the file which has been run to capture the data, use with -update\n");
			printf("-printpackages: Prints out the depended packagenames from the given xml file\n");
			printf("-printcomment: Prints out the user comment, if any.\n");
			printf("-printuserhome: Prints out the original user home directory, if any.\n");
			printf("-printcommands: Prints out the captured commands.\n");
			printf("-printconnections: Prints out network connections made by te captured program.\n");
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

/* Parsing based on:
http://xmlsoft.org/examples/parse1.c
Reads in all relevant system informations from the XML filename.
*/
void readxml(const char * filename) {
	//read out all current file info
	message(2, "readxml: Processing '%s'...\n", filename);
	xmlDocPtr doc;
	xmlNodePtr root = xml_open_file(filename, "content", &doc);
	//get files tag
	xmlNodePtr meta = xml_find_node(root, "metainfo");
	if (meta != NULL) {
		//fetch info
		osys.loginname = xml_get_content(meta, "loginname");
		osys.homedir = xml_get_content(meta, "homedir");
		osys.kernelver = xml_get_content(meta, "kernelver");
		osys.distro = xml_get_content(meta, "distro");
		osys.distroversion = xml_get_content(meta, "distroversion");
		osys.displayvar = xml_get_content(meta, "displayvar");
		osys.graphicdriver = xml_get_content(meta, "graphicdriver");
		osys.graphiccard = xml_get_content(meta, "graphiccard");
		osys.glversion = xml_get_content(meta, "glversion");
		osys.glextensions = xml_get_content(meta, "glextensions");
		usercomment = xml_get_content(meta, "usercomment");
		if (usercomment == NULL) {
			usercomment = strdup("<User did not add any comment>");
		}
		//save packages and device node pointers
		xmlNodePtr tmp = xml_find_node(meta, "devices");
		if (tmp != NULL) {
			devicesp = xmlCopyNode(tmp, 1);//1 means recursive copy
		}
		tmp = xml_find_node(meta, "depend");
		if (tmp != NULL) {
			packagesp = xmlCopyNode(tmp, 1);//1 means recursive copy
		}
		tmp = xml_find_node(meta, "commands");
		if (tmp != NULL) {
			commandsp = xmlCopyNode(tmp, 1);//1 means recursive copy
		}
		tmp = xml_find_node(meta, "connections");
		if (tmp != NULL) {
			connectionsp= xmlCopyNode(tmp, 1);//1 means recursive copy
		}
	} else
		message(3, "readxml: File does not contain a <metainfo> tag\n");
	xml_close_file(doc);
}

char * get_filecommand(void) {
	message(4, "get_filecommand: Started, with file '%s'\n", command_file);
	char * output;
	if (command_file != NULL) {
		output = readfile(command_file);
		message(4, "get_filecommand: Done\n");
	} else {
		message(3, "get_filecommand: Warning: Requested info, but no file was given\n");
		output = NULL;
	}
	return output;
}

/*The found meta information are now written to filename. Existing
informations will be updated
*/
void writexml(const char * filename) {
	xmlKeepBlanksDefault(0); //results in good linebreaks -> better human readable
	xmlDocPtr doc = NULL;
	xmlNodePtr base = NULL;
	base = xml_have_file(filename, "content", &doc);
	if (base == NULL) {
		message(1, "writexml: Error: Nothing to write on. Aborting\n");
		return;
	}
	//find /make metainfo entry
	xmlNodePtr meta = xml_have_node(base, "metainfo");
	//update content
	xml_have_content(meta, "loginname", csys.loginname);
	xml_have_content(meta, "homedir", csys.homedir);
	xml_have_content(meta, "kernelver", csys.kernelver);
	xml_have_content(meta, "distro", csys.distro);
	xml_have_content(meta, "distroversion", csys.distroversion);
	xml_have_content(meta, "displayvar", csys.displayvar);
	xml_have_content(meta, "graphicdriver", csys.graphicdriver);
	xml_have_content(meta, "graphiccard", csys.graphiccard);
	xml_have_content(meta, "glversion", csys.glversion);
	xml_have_content(meta, "glextensions", csys.glextensions);
	xmlNodePtr comn = xml_have_node(meta, "commands");
	char * commandstr = get_filecommand();
	if (commandstr != NULL) {
		xml_have_infonode(comn, "command", "name", commandstr);
		free(commandstr);
	}
	//save the file
	xmlSaveFormatFile(filename, doc, 1);
}

void compare(const char * xmlfilename, int argc, char *argv[]) {
	fetch_meta(argc, argv);
	readxml(xmlfilename);
	comparesys();
}

void update(const char * xmlfilename, int argc, char *argv[]) {
	fetch_meta(argc, argv);
	writexml(xmlfilename);
}

void printmetainfo(xmlNodePtr base, const char * basename, int multiline) {
	xmlNodePtr cur = base->children;
	while (cur != NULL) {
		if (xml_is_usefulnode(cur)) {
			char * nodename = xml_get_nodename(cur);
			if (strcmp(nodename, basename) == 0) {
				char * pname = xml_get_attribute(cur, "name");
				if (pname != NULL) {
					printf("%s ", pname);
					free(pname);
					if (multiline)
						printf("\n");
				} else
					message(1, "printmetainfo: Error: '%s' entry has no name\n", basename);
				}
				free(nodename);
			}
			cur = cur->next;
		}
	printf("\n");
}

void printpackages(const char * xmlfilename) {
	readxml(xmlfilename);
	if (packagesp != NULL) {
		printmetainfo(packagesp, "package", 0);
	} else
		message(1, "printpackages: Error: No package information found\n");
	printf("\n");
}

void printcommands(const char * xmlfilename) {
	readxml(xmlfilename);
	message(1, "printcommands: Showing all commands including parameters which were captured\n");
	if (commandsp != NULL) {
			printmetainfo(commandsp, "command", 1);
	} else
		message(1, "printcommands: Warning: No commands information found\n");
}

void printcomment(const char * xmlfilename) {
	readxml(xmlfilename);
	if (usercomment != NULL) {
		printf("%s\n", usercomment);
	} else	{
		message(1, "printhomedir: Error: Unable to get user comment\n");
		exit(1);
	}
}

void printhomedir(const char * xmlfilename) {
	readxml(xmlfilename);
	if (osys.homedir != NULL) {
		printf("%s\n", osys.homedir);
	} else {  //not expected to happen
		message(1, "printhomedir: Error: No home directory set\n");
		exit(1);
	}
}

void connection_found(xmlNodePtr child) {
	char * pid = xml_get_attribute(child, "pid");
	char * host = xml_get_attribute(child, "host");
	char * port = xml_get_attribute(child, "port");
	if ((pid != NULL) && (host != NULL) && (port != NULL)) {
		//print it
		printf("PID %s: to: %s on port: %s\n", pid, host, port);
	}
	free(pid);
	free(host);
	free(port);
}

void printconnections(const char * xmlfilename) {
	readxml(xmlfilename);
	if (connectionsp != NULL) {
		message(1, "connection_found: The archived application made the following\
 connections:\n");
		xml_child_handler(connectionsp, "connection", connection_found);
	} else {  //not expected to happen
		message(1, "printconnections: Error: No connections node found\n");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	set_chatting(PGN, 1);
	int argoffset = setup_args(argc, argv);
	message(1, "Welcome to '%s'\n", argv[0]);
	//set filename
	char * xmlfilename;
	if (argc == argoffset+2) {
		xmlfilename = argv[argoffset+1];
	} else if (argc > argoffset+2) {
		message(1, "main: Error: Multiple XML files are not allowed as parameter\n");
		exit(1);
	} else
		xmlfilename = "sysinfo.xml"; //default filename
	//if file was given as parameter
	if ((argc > argoffset+1) || (mode == 1)) {
		switch (mode) {
			case 0: compare(xmlfilename, argc, argv); break;
			case 1: update(xmlfilename, argc, argv); break;
			case 2: printpackages(xmlfilename); break;
			case 3: printcomment(xmlfilename); break;
			case 4: printhomedir(xmlfilename); break;
			case 5: printcommands(xmlfilename); break;
			case 6: printconnections(xmlfilename); break;
			default: break; //not expected to happen
		}
	} else { //no file given
		message(1, "main: Nothing to do. See options\n");
	}
	message(1, "main: Done\n");
	return 0;
}
