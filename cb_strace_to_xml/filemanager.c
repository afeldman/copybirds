/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libxml/xmlwriter.h>
#include <libxml/encoding.h>


#include "../lib/hash-table.h"

#include "../share/tools.h"
#include "../share/hashhelper.h"

#include "filemanager.h"
#include "copyxmler.h"

HashTable * file_data;

/* For remember:
	char * filename;
	int pid;
	int readed;
	int written;
	int executed;
	char * version;
	char * shasum;
*/

/*
The hash function to use */
unsigned long hash_func(HashTableKey value) {
	faccessk_t * content = (faccessk_t *)value;
	unsigned long key = content->pid*100000;
	const char * str = content->filename;
	//message(1, "Will hash %p, '%c%c%c%c'\n", str, (char)str>>24, (char)str>>16, (char)str>>8, (char)str);
	if (str == NULL)
		message(1, "hash_func: Error: Got nullpointer as filename\n");
	key += hash_of_string(str);
	return key;
}

/*
Compares two hash keys.
returns 1 if equal, 0 if not */
int equal_func(HashTableKey value1, HashTableKey value2) {
	faccessk_t * content1 = (faccessk_t *)value1;
	faccessk_t * content2 = (faccessk_t *)value2;
	if (content1->pid == content2->pid) {
		if (strcmp(content1->filename, content2->filename) == 0)
			return 1; //equal
	}
	return 0;
}

/*
Inits the hashtable. This function has to be executed once, before
any add function may be called */
void files_initmemory(void) {
	file_data = hash_table_new(hash_func, equal_func);
}

/*
Searches in the hash table, for a given key. Returns a pointer to the content
if found, NULL if not found */
faccess_t * seek_file(const char * filename, int pid) {
	faccessk_t key;
	key.filename = filename;
	key.pid = pid;
	HashTableValue val = hash_table_lookup(file_data, &key);
	if (val == HASH_TABLE_NULL) //not found
		return NULL;
	return (faccess_t *)val;
}

/*
Adds an entry to the hash table */
faccess_t * add_entry(char * filename, int pid) {
	//add at the beginning is faster
	message(4, "add_entry: %s adding for %i\n", filename, pid);
	faccess_t * n = (faccess_t *)smalloc(sizeof(faccess_t));
	faccessk_t * k = (faccessk_t *)smalloc(sizeof(faccessk_t));
	n->pid = pid;
	k->pid = pid;
	n->filename = filename;
	k->filename = filename;
	n->readed = 0;
	n->written = 0;
	n->executed = 0;
	n->version = NULL;
	n->shasum = NULL;
	hash_table_insert(file_data, k, n);
	return n;
}

/*
Adds an entry to the hash table if it does not already exists. If it exists,
only the read, write, execute counters will be updated.
Returns 1 if the data were new, 0 if they had already exists.*/
int file_add(const char * filename, int acctype, int pid) {
	if (filename == NULL) {
		message(1, "file_add: Error: Filename is a null pointer, line: %i\n", get_linecount());
		return 1;
	}
	if (get_ignorepids()) //will speed things up
		pid = 1;
	//printf("name %s\n,", filename);
	//printf("len: %i\n,", strlen(filename));
	message(5, "e1\n");
	int wasnew = 0;
	faccess_t * x = seek_file(filename, pid);
	message(5, "e2\n");
	if (x == NULL) { //generate new entry
		x = add_entry(strdup(filename), pid);
		wasnew = 1;
	}
	message(5, "e3\n");
	//which access did we have?
	if (acctype & FO_READ) {
		x->readed++;
	}
	if (acctype & FO_WRITE) {
		x->written++;
	}
	if (acctype & FO_EXEC) {
		x->executed++;
	}
	return wasnew;
}

/*
Writes the acc parameter as xml int in the writer pointer.
Returns 1 if succeed, 0 if something went bad */
int file_savexml(xmlTextWriterPtr writer, faccess_t * acc) {
	int res;
	res = xmlTextWriterStartElement(writer, BAD_CAST "file");
	if (res < 0) return 0;
	res = xmlTextWriterWriteAttribute(writer, BAD_CAST "name",
	        BAD_CAST (acc->filename));
	if (res < 0) return 0;
	res = xmlTextWriterWriteAttribute(writer, BAD_CAST "pid",
          BAD_CAST (itoa(acc->pid)));
	if (res < 0) return 0;
	if (acc->readed > 0) {
		res = xmlTextWriterWriteFormatElement(writer, BAD_CAST "readed", "%i",
		        acc->readed);
		if (res < 0) return 0;
	}
	if (acc->written > 0) {
		res = xmlTextWriterWriteFormatElement(writer, BAD_CAST "written", "%i",
		        acc->written);
		if (res < 0) return 0;
	}
	if (acc->executed > 0) {
		res = xmlTextWriterWriteFormatElement(writer, BAD_CAST "executed", "%i",
		        acc->executed);
		if (res < 0) return 0;
	}
	if (acc->version != NULL) {
		res = xmlTextWriterWriteElement(writer, BAD_CAST "version",
		        BAD_CAST (acc->version));
		if (res < 0) return 0;
	}
	if (acc->shasum != NULL) {
		res = xmlTextWriterWriteElement(writer, BAD_CAST "shasum",
		             BAD_CAST (acc->shasum));
		if (res < 0) return 0;
	}
	res = xmlTextWriterEndElement(writer);
	if (res < 0) return 0;
	return 1;
}

/*
Saves all files to the given xml pointer.
Returns 1 if succeed, 0 if failed */
int files_savexml(xmlTextWriterPtr writer) {
	HashTableIterator hit;
	hash_table_iterate(file_data, &hit);
	faccess_t * x;
	while (hash_table_iter_has_more(&hit)) {
		x = (faccess_t *)hash_table_iter_next(&hit);
		if (file_savexml(writer, x) == 0) { //saving was not successful
			return 0;
		}
	}
	return 1;
}

