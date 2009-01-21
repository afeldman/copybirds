/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdlib.h>

#include "../lib/hash-table.h"

#include "../share/packagestates.h"
#include "../share/tools.h"
#include "../share/hashhelper.h"

#include "datamanager.h"


HashTable * fanc; //files

//--------------------the file part, storing the files from the xml structure---

int file_add(char * filename) {
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

int file_exists(const char * filename) {
	HashTableValue val = hash_table_lookup(fanc, (HashTableValue)filename);
	if (val == HASH_TABLE_NULL) //not found
		return 0;
	return 1;
}

HashTableIterator fit;

void file_start_iterate(void) {
	hash_table_iterate(fanc, &fit);
}

const char * file_get_next(void) {
	const char * filename = (const char *)hash_table_iter_next(&fit);
	return filename;
}

int file_has_next(void) {
	if (hash_table_iter_has_more(&fit)) {
		return 1;
	}
	return 0;
}


//--------------------------the package to version part-----------------------
//This part has been moved to the shared directory, as packagestates.c because
//cb_meta_check needs to use exactly the same functions

//--------------------------the package name to packagefilenames part-----------
HashTable * packfiles;

pfstore_t * package_file_get(const char *filename) {
	HashTableValue got = hash_table_lookup(packfiles, (HashTableKey)filename);
	if (got == HASH_TABLE_NULL)
		return NULL;
	return (pfstore_t *)got;
}

/*
If existing is installed -> use it, otherwise use new
Returns: 1 if the new should be added, 0 if not
*/
int package_duplicate_check(const char * filename) {
	pfstore_t * dupl = package_file_get(filename);
	if (dupl != NULL) {
		if (dupl->package->installed) {
			message(3, "package_duplicate_check: Duplicate: '%s'\n", dupl->package->name);
			(dupl->refcount)++;
			return 0;
		}
	}
	return 1;
}

/*
Adds a filename for a package name into the hashtable. Checks for filenames
which are part of multiple packages*/
void package_file_add(const char * package, const char * filename) {
	pstore_t * ps = package_get(package);
	if (ps == NULL) {
		ps = package_add(package, NULL, 0);
		message(1, "package_file_add: Warning: package '%s' does not contain status\
 informations\n", package);
	}
	message(5, "package_file_add: package name: '%s'\n", ps->name);
	pfstore_t * pf = (pfstore_t *)smalloc(sizeof(pfstore_t));
	pf->package = ps;
	pf->refcount = 1;
	//message(1, "File: '%s' package: '%s'\n", filename, package);
	//check if there is already an entry
	if (package_duplicate_check(filename)) {
		hash_table_insert(packfiles, strdup(filename), pf);
	}
}

//--------------------------the package with a hit part-----------------------
HashTable * findings;

/*Add a package including additional informations if a file from the package was
used */
void findings_add(pstore_t * ps) {
	if (ps == NULL) {
		message(1, "findings_add: Error: Parameter is a null pointer\n");
		return;
	}
	if (ps->name == NULL) {
		message(1, "findings_add: Error: Package name is a null pointer\n");
		return;
	}
	hash_table_insert(findings, (HashTableKey)strdup(ps->name), (HashTableValue)ps);
}

HashTableIterator findingsit;

void findings_start_iterate(void) {
	hash_table_iterate(findings, &findingsit);
}

pstore_t * findings_get_next(void) {
	pstore_t * ps = (pstore_t *)hash_table_iter_next(&findingsit);
	return ps;
}

int findings_has_next(void) {
	if (hash_table_iter_has_more(&findingsit)) {
		return 1;
	}
	return 0;
}

//--------------the general part-------------------

/*
Inits the hashtable. This function has to be executed once, before
any add function may be called */
void files_initmemory(void) {
	fanc = hash_table_new(hash_string_func, hash_string_equal_func);
	packfiles = hash_table_new(hash_string_func, hash_string_equal_func);
	package_states_initmemory();
	findings = hash_table_new(hash_string_func, hash_string_equal_func);
}
