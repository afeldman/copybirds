/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#include <string.h>
#include <stdlib.h>

#include "../lib/hash-table.h"
#include "../lib/arraylist.h"

#include "../share/tools.h"
#include "../share/hashhelper.h"


#include "datamanager.h"


//--------------------the file part --------------------------------------

HashTable * fanc; //files

int file_exists(const char * filename) {
	HashTableValue val = hash_table_lookup(fanc, (HashTableValue)filename);
	if (val == HASH_TABLE_NULL) //not found
		return 0;
	return 1;
}

int file_add(const char * filename) {
	if (file_exists(filename))
		return 1;
	hash_table_insert(fanc, strdup(filename), strdup(filename));
	return 0;
}

int file_count(void) {
	HashTableIterator hit;
	hash_table_iterate(fanc, &hit);
	int files = 0;
	while (hash_table_iter_has_more(&hit)) {
		hash_table_iter_next(&hit);
		files++;
	}
	return files;
}

HashTableIterator fit;

void file_start_iterate(void) {
	hash_table_iterate(fanc, &fit);
}

const char * file_get_next(void) {
	const char * filename = hash_table_iter_next(&fit);
	return filename;
}

int file_has_next(void) {
	if (hash_table_iter_has_more(&fit)) {
		return 1;
	}
	return 0;
}

//--------------------------the blacklist part--------------------------

ArrayList * banc; //blacklist
/*
If the speed is too slow, it could be improved by a sorted list */

/*
Returns 1 if check is part if base. Meaning that all files blacklisted by
the pattern of check will be blacklisted by the pattern of base too.
Returns 0 otherwise. */
int bl_partof(const char * base, const char * check) {
	int baselen = strlen(base), checklen = strlen(check);
	int basewild = 0, checkwild = 0;
	if (base[baselen-1] == '*')  basewild = 1;
	if (check[checklen-1] == '*') checkwild = 1;
	//compare without a possible wildcard
	baselen -= basewild;
	checklen -= checkwild;
	//are they equal?
	int eq = strncmp(base, check, imin(baselen, checklen));
	if (eq == 0) { //base is the same
		if ((baselen < checklen) && (basewild))
			return 1;
		if (baselen == checklen) //the strings are the same
			return 1;
	}
	return 0;
}

/*
Adds bad into the blacklist if not already a more general pattern is in it.
The first more specific pattern will be replaced by bad.
So removing more specific patterns is only a heuristic.
 */
int bl_add(const char * bad) {
	int i;
	int max = banc->length;
	for (i = 0; i < max; i++) {
		char * cbad = banc->data[i];
		if (cbad == NULL) break;
		if (bl_partof(cbad, bad) == 1) return 0; //already blacklisted
		if (bl_partof(bad, cbad)) { //update entry
			banc->data[i] = strdup(bad);
			free(cbad);
			return 1;
		}
	}
	//no match so far, add entry
	arraylist_append(banc, strdup(bad));
	return 1;
}

/*
Returns 1 if check is blacklisted, otherwise 0. */
int bl_check(const char * check) {
	int i;
	int max = banc->length;
	for (i = 0; i < max; i++) {
		char * cbad = banc->data[i];
		if (cbad == NULL) break;
		if (bl_partof(cbad, check)) {
			message(3, "bl_check: '%s' is blacklisted\n", check);
			return 1; //yes, its blacklisted
		}
	}
	//no, not blacklisted
	return 0;
}

//------------------the PID part -----------------------------------

pidstore_t * panc; //pids

void pid_add_new(int pid) {
	//add at the beginning is faster
	pidstore_t * n = smalloc(sizeof(pidstore_t));
	n->next = panc;
	panc = n;
	n->pid = pid;
}

int pid_exists(int pid) {
	pidstore_t * x = panc;
	while (x != NULL) {
		if (x->pid == pid)
			return 1;
		x = x->next;
	}
	return 0;
}

int pid_add(int pid) {
	if (pid_exists(pid))
		return 1;
	pid_add_new(pid);
	return 0;
}

int pid_count(void) {
	pidstore_t * x = panc;
	int pids = 0;
	while (x != NULL) {
		x = x->next;
		pids++;
	}
	return pids;
}

//--------------the device part--------------------

HashTable * devanc; //files

int device_exists(const char * filename) {
	HashTableValue val = hash_table_lookup(devanc, (HashTableValue)filename);
	if (val == HASH_TABLE_NULL) //not found
		return 0;
	return 1;
}

int device_add(const char * filename) {
	if (device_exists(filename))
		return 1;
	hash_table_insert(devanc, strdup(filename), strdup(filename));
	return 0;
}

HashTableIterator dit;

void device_start_iterate(void) {
	hash_table_iterate(devanc, &dit);
}

const char * device_get_next(void) {
	const char * filename = hash_table_iter_next(&dit);
	return filename;
}

int device_has_next(void) {
	if (hash_table_iter_has_more(&dit)) {
		return 1;
	}
	return 0;
}



//--------------the general part-------------------

/*
Inits the hashtable. This function has to be executed once, before
any add function may be called */
void datamanager_initmemory(void) {
	fanc = hash_table_new(hash_string_func, hash_string_equal_func);
	banc = arraylist_new(32);
	devanc = hash_table_new(hash_string_func, hash_string_equal_func);
}
