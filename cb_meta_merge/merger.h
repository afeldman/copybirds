/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#ifndef MERGER_H
	#define MERGER_H

#include <libxml/tree.h>

#include "../lib/hash-table.h"


#define IS_HASH 1
#define IS_STRING 2

typedef struct metavalue {
	char * name;
	int type; //tells from which type the pointer data is
	void * data; //pointer to string or hashmap
	char * sourcefile;
} metavalue_t;


typedef struct metasubvalue {
	xmlNodePtr data;
	char * sourcefile;
} metasubvalue_t;

void merger_initmemory(void);
void merger_write_xml(xmlNodePtr meta);
int merger_add_entry(const char * nodename, const char * value,
	 const char * filename);
HashTable * merger_have_complex_entry(const char * nodename,
	 const char * sourcename);
void merger_add_subentries(HashTable * subnodes, xmlNodePtr rootvalue,
	 const char * sourcename);
#endif
