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

#include "connectionmanager.h"
#include "copyxmler.h"

HashTable * conn_data;

/* For remember:
typedef struct connection {
	char * host;
	int pid;
	int port;
} connection_t;
*/

/*
The hash function to use */
unsigned long conn_hash_func(HashTableKey value) {
	connection_t * content = value;
	unsigned long key = content->pid+content->port;
	const char * str = content->host;
	if (str == NULL)
		message(1, "hash_func: Error: Got nullpointer as filename\n");
	key += hash_of_string(str);
	return key;
}

/*
Compares two hash keys.
returns 1 if equal, 0 if not */
int conn_equal_func(HashTableKey value1, HashTableKey value2) {
	connection_t * content1 = value1;
	connection_t * content2 = value2;
	if (content1->pid == content2->pid) {
		if (content1->port == content2->port) {
			if (strcmp(content1->host, content2->host) == 0) {
				return 1; //equal
			}
		}
	}
	return 0;
}

/*
Inits the hashtable. This function has to be executed once, before
any add function may be called */
void connection_initmemory(void) {
	conn_data = hash_table_new(conn_hash_func, conn_equal_func);
}

/*
Searches in the hash table, for a given key. Returns a pointer to the content
if found, NULL if not found */
connection_t * conn_seek(int pid, const char * host, int port) {
	connection_t key;
	key.pid = pid;
	key.host = host;
	key.port = port;
	HashTableValue val = hash_table_lookup(conn_data, &key);
	if (val == HASH_TABLE_NULL) //not found
		return NULL;
	return val;
}

/*
Adds an entry to the hash table */
connection_t * conn_add_entry(int pid, const char * host, int port) {
	message(4, "add_entry: %s adding for %i\n", host, pid);
	connection_t * n = smalloc(sizeof(connection_t));
	connection_t * k = smalloc(sizeof(connection_t));
	n->pid = pid;
	k->pid = pid;
	n->host = host;
	k->host = host;
	n->port = port;
	k->port = port;
	hash_table_insert(conn_data, k, n);
	return n;
}

/*
Adds an entry to the hash table if it does not already exists.*/
int connection_add(int pid, const char * host, int port) {
	if (host == NULL) {
		message(1, "connection_add: Error: Host is a null pointer, line: %i\n", get_linecount());
		return 1;
	}
	if (get_ignorepids()) //will speed things up
		pid = 1;
	int wasnew = 0;
	connection_t * x = conn_seek(pid, host, port);
	if (x == NULL) { //generate new entry
		x = conn_add_entry(pid, strdup(host), port);
		wasnew = 1;
	}
	return wasnew;
}

/*
Writes the acc parameter as xml in the writer pointer.
Returns 1 if succeed, 0 if something went bad */
int conn_savexml(xmlTextWriterPtr writer, connection_t * co) {
	int res;
	res = xmlTextWriterStartElement(writer, BAD_CAST "connection");
	if (res < 0) return 0;
	res = xmlTextWriterWriteAttribute(writer, BAD_CAST "pid",
          BAD_CAST (itoa(co->pid)));
	if (res < 0) return 0;
	res = xmlTextWriterWriteAttribute(writer, BAD_CAST "host",
	        BAD_CAST (co->host));
	if (res < 0) return 0;
	res = xmlTextWriterWriteAttribute(writer, BAD_CAST "port",
	        BAD_CAST (itoa(co->port)));
	if (res < 0) return 0;
	res = xmlTextWriterEndElement(writer); // end connection tag
	if (res < 0) return 0;
	return 1;
}

/*
Saves all files to the given xml pointer.
Returns 1 if succeed, 0 if failed */
int connection_savexml(xmlTextWriterPtr writer) {
	HashTableIterator hit;
	hash_table_iterate(conn_data, &hit);
	connection_t * x;
	while (hash_table_iter_has_more(&hit)) {
		x = hash_table_iter_next(&hit);
		if (conn_savexml(writer, x) == 0) { //saving was not successful
			return 0;
		}
	}
	return 1;
}

