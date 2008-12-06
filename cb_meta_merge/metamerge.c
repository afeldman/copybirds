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

#include "metamerge.h"
#include "../share/tools.h"
#include "../share/xmlhelper.h"
#include "merger.h"

char * outputxml;

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
		if (strcmp(arg, "-o") == 0) { //filename is next param
			message(1, "Error: Please put the filename directly after -o without\
 spacing.\n");
			exit(2);
		} else
		if ((strlen(arg) > 2) && (strncmp(arg, "-o", 2) == 0)) {
			outputxml = get_arg_filename(arg);
		} else
		if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"-help") == 0) ||
			  (strcmp(arg,"--help") == 0)) {
			printf("License: GPL Version 2.0\n");
			printf("This program merges the meta informations from multiple xml\
 files\n");
			printf("[option] XMLFILE [more xmlfiles]\n");
			printf("Normally the program returns: 0 if succeed, 1 if an error\
 occurred\n");
			printf("You may use the following options:\n");
			printf("-v: Be verbose\n");
			printf("-vv: Be very verbose\n");
			printf("-d: Debug messages. Even more verbose than -vv\n");
			printf("-dd: Ultra debug messages. You will be overwhelmed by non\
 understandable messages.\n");
			printf("-q: Be quiet\n");
			printf("-oFILENAME: Writes the metainformation into this file. The\
 metainfo in existing files will be replaced\n");
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
Reads in all xml files and tries to understand what they mean, in the hope
that this program can merge future (currently unknown) meta information without
any modification too */
void readxml(char * filename) {
	//put all copyable files into the list
	message(2, "readxml: Procesing '%s'...\n", filename);
	xmlDocPtr doc;
	xmlNodePtr root_element = xml_open_file(filename, "content", &doc);
 	if (root_element == NULL) {
		message(1, "readxml: Aborting function due to the previous errors.\n",
		        filename);
		return;
	}
	//get meta tag
	xmlNodePtr metainfo = xml_find_node(root_element, "metainfo");
	if (metainfo != NULL) {
		//go through the nodes
		xmlNodePtr cur_node = NULL;
		for (cur_node = metainfo->children; cur_node; cur_node = cur_node->next) {
			char * nodename = xml_get_nodename(cur_node);
			if (xml_is_usefulnode(cur_node)) {
				if (xml_count_children(cur_node) == 0) {
					//message(1, "Simple node\n");
					merger_add_entry(nodename, xml_get_nodecontent(cur_node), filename);
				} else {
					//message(1, "Complex node\n");
					HashTable * subnodes = merger_have_complex_entry(nodename, filename);
					merger_add_subentries(subnodes, cur_node, filename);
				}
			}
		}
	} else
		message(3, "readxml: File '%s' does not contain a <filelist> tag\n",
		        filename);
	xml_close_file(doc);
}

/*
Writes the merged meta data to the given filename. Existing metadata from an
existing file will be replaced. */

void write_meta_xml(char * filename) {
	message(1, "write_meta_xml: Writing results into file '%s'...\n", filename);
	xmlKeepBlanksDefault(0); //results in good linebeaks -> better human readable
	xmlDocPtr doc = NULL;
	xmlNodePtr base = NULL;
	base = xml_have_file(filename, "content", &doc);
	if (base == NULL) {
		message(1, "write_meta_xml: Error: Nothing to write on. Aborting\n");
		return;
	}
	//free old metainfo to prevent conflicts (we have already merged)
	xmlNodePtr meta = xml_find_node(base, "metainfo");
	if (meta != NULL) {
		xmlUnlinkNode(meta);
		xmlFreeNode(meta);
	}
	//write new info
	meta = xml_have_node(base, "metainfo");
	merger_write_xml(meta);
	/*usin newlines in the output found by:
	 http://mail.gnome.org/archives/xml/2002-March/msg00198.html
	*/
	xmlSaveFormatFile(filename, doc, 1);
}

int main(int argc, char *argv[]) {
	set_chatting(PGN, 1);
	merger_initmemory();
	if (argc < 2) {
		message(1, "main: Error: give parameters: [option] XMLFILE [more xmlfiles]\
, or try -h\n");
		return 1;
	}
	if (argc > 32000) {
		message(1, "main: Error: %i program arguments are too much. 32000 is the\
 maximum.\n", argc);
		return 1;
	}
	int argoffset = setup_args(argc, argv);
	message(1, "Welcome to '%s'\n", argv[0]);
	/*Parse all xml files from 1+argoffset to argc-1
	*/
	message(1, "main: Parsing and merging xml input file(s)...\n");
	int i = 0;
	int files = argc-argoffset-1;
	if (files <= 0)	{
		message(1, "main: Error: no input file\n");
		return 1;
	}
  LIBXML_TEST_VERSION
	for (i = 0; i < files; i++) {
		readxml(argv[1+argoffset+i]);
	}
	//write output
	if (outputxml == NULL)
		outputxml = "metainfo.xml";
	write_meta_xml(outputxml);
	return 0;
}
