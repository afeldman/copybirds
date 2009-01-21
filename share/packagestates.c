//(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>


#include "../lib/hash-table.h"

#include "packagestates.h"

#include "tools.h"
#include "hashhelper.h"

HashTable * packstates = NULL;

/*
Generates a proper struct and returns a pointer to it */
static pstore_t * pstore_make(const char * package, const char * version,
                       int installed) {
	pstore_t * ps = (pstore_t *)smalloc(sizeof(pstore_t));
	ps->name = strdup(package);
	if (version != NULL) {
		ps->version = strdup(version);
	} else
		ps->version = NULL;
	ps->installed = installed;
	return ps;
}

/*
Returns the struct associated with the package. If the parameter is wrong or
no package with this name is found, NULL is returned.
Returned structs should not be freed!
 */
pstore_t * package_get(const char * package) {
	if (package == NULL) {
		message(1, "package_get: Error: parameter is a null pointer\n");
		return NULL;
	}
	if (packstates == NULL) {
		message(1, "package_get: Error: hash-table is not initialized.\n");
		return NULL;
	}
	HashTableValue got = hash_table_lookup(packstates, (HashTableKey)package);
	if (got == HASH_TABLE_NULL)
		return NULL;
	return (pstore_t *)got;
}

/*
Adds a package to the internal structure.
package: The name of the package
version: The version of the package (may be NULL).
installed: 1: yes, 0: no.
*/
pstore_t * package_add(const char * package, const char * version,
                       int installed) {
	if (package == NULL) {
		message(1, "package_add: Error: 'package' is a null pointer\n");
		return NULL;
	}
	if (packstates == NULL) {
		message(1, "package_add: Error: hash-table is not initialized.\n");
		return NULL;
	}
	pstore_t * dupl = package_get(package);
	if (dupl != NULL) {
		message(1, "package_add: Warning: dpkg database contains two entries for\
 the package '%s'. Using the second one.\n", package);
	}
	pstore_t * ps = pstore_make(package, version, installed);
	hash_table_insert(packstates, (HashTableKey)strdup(package),
	                  (HashTableValue)ps);
	return ps;
}

/*Returns the new state based on the old state and what type of information was
found next */
static int read_package_states_smachine(int ostate, int found) {
	/* Parsing is realized as a state machine with 6 states and 4 possibilities
	   in each node:
	   State 0 ist the starting state.
	   state 0 -> 0: empty line found
	   state 0 -> 1: Package name found
	   state 1 -> 3: Status information found
	   state 1 -> 2: Version information found
	   state 2 -> 4: Status information found
	   state 3 -> 4: Version information found
	   state 3 -> 4: Empty line found
	   state 4 -> 0: always (runs evaluation of foud data)
	   state 5 -> 0: always (prints error message)
	   all non displayed states go to state 5 (the error state)
	   the array is: x: found information
	   with 0: empty line, 1: packagename, 2: version, 3: status
	   y: the current state
 */
	const int smachine[6][4] = {
	 {0,1,5,5},
	 {5,5,2,3},
	 {5,5,5,4},
	 {4,5,4,5},
	 {0,0,0,0},
	 {0,0,0,0}
	};
	if ((found >= 0) && (found < 4) && (ostate >= 0) && (ostate < 6)) {
		//message(1, "statechange: from %i by %i to %i\n",
 //ostate, found, smachine[ostate][found]);
		return smachine[ostate][found];
	}
	message(1, "read_package_states_smachine: Error: State machine got invalid\
 input [%i][%i]\n", ostate, found);
	return 0;
}

/*
Reads in the package states from the dpkg database file
The recommend parameter is therefore: "/var/lib/dpkg/status"
returns 1 if opening the file was a success, otherwise 0 is returned.
*/
int package_states_read(const char * filepath) {
	if (filepath == NULL) {
		message(1, "package_states_read: Error: parameter is a null pointer\n");
		return 0;
	}
	message(1, "read_package_states: Reading...\n");
	FILE * packagemeta = fopen(filepath, "r");
	if (packagemeta != NULL) {
		int length = 0;
		char * pname = NULL;
		char * status = NULL;
		char * version = NULL;
		int state = 0;
		do {
			unsigned int buflen = INITIAL_BUF_LEN;
			int info = -1;
			char * line = NULL;
			length = getline(&line, &buflen, packagemeta);
			if (length == 0) break;
			replace_chars(line, '\n', '\0'); //no newlines
			if (strlen(line) == 0) {
				info = 0;
			}
			if (beginswith(line, "Package:")) {
				if (pname != NULL) {
					free(pname);
				}
				pname = text_get_between(line, "Package: ", NULL);
				info = 1;
			}
			if (beginswith(line, "Version:")) {
				if (version != NULL) {
					free(version);
				}
				version = text_get_between(line, "Version: ", NULL);
				info = 2;
			}
			if (beginswith(line, "Status:")) {
				if (status != NULL) {
					free(status);
				}
				status = text_get_between(line, "Status: ", NULL);
				info = 3;
			}
			if (info != -1) { //if there were changes
				state = read_package_states_smachine(state, info);
			}
			if (state == 5) {
				message(1, "read_package_states: Error: Invalid input data: '%s' '%s'\
 '%s'\n", pname, status, version);
				//second parameter is unimportant
				state = read_package_states_smachine(state, 0);
			}
			if (state == 4) {
				if (strstr(status, "not-installed") != NULL) { //if not "not-installed"
					package_add(pname, version, 0);
					message(4, "read_package_states: '%s' is not installed\n", pname);
				} else if (strstr(status, "installed") != NULL) {
					message(4, "read_package_states: '%s' is installed\n", pname);
					//part of "not-installed"...
					package_add(pname, version, 1);
				} else if (strstr(status, "config-files") != NULL) { //
					message(4, "read_package_states: '%s' has config-files\n", pname);
					package_add(pname, version, 0);
				} else if (strstr(status, "deinstall") != NULL) { //
					message(4, "read_package_states: '%s' is deinstalled\n", pname);
					package_add(pname, version, 0);
				} else {
					message(4, "read_package_states: '%s' has an unknown status\n", pname);
				}
				//second parameter is unimportant
				state = read_package_states_smachine(state, 0);
			}
			free(line);
		} while (length > 0);
		free(pname);
		free(status);
		free(version);
		fclose(packagemeta);
	} else {
		message(1, "read_package_states: Error: Could not open input file '%s'.\n",
		        filepath);
		return 0;
	}
	return 1;
}

/*
Inits the hash-table. Run before using any of the other functions in this file*/
void package_states_initmemory(void) {
	packstates = hash_table_new(hash_string_func, hash_string_equal_func);
}
