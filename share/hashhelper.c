
/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#include <stdlib.h>

#include "../lib/hash-table.h"

#include "tools.h"

/*
Returns the hash value of a string.
If text is NULL, an error message will be shown and 0 is returned */

unsigned long hash_of_string(const char * text) {
	if (text == NULL) {
		message(1, "hash_of_string: Error: Got NULL pointer as text\n");
		return 0;
	}
	unsigned int i = 0;
	/*
	The multiplication increases the speed a lot, as the hash is more chaotic.
	On two million entries, this increases the speed from one minute down
	to six seconds. */
	unsigned long key = 0;
	while (text[i] != '\0') {
		key += text[i]*i;
		i++;
	}
	return key;
}

/*
The hash function to use, if the key is a string */
unsigned long hash_string_func(HashTableKey value) {
	const char * content = value;
	return hash_of_string(content);
}

/*
Compares two hash keys if they are strings.
returns 1 if equal, 0 if not */
int hash_string_equal_func(HashTableKey value1, HashTableKey value2) {
	char * content1 = value1;
	char * content2 = value2;
	if ((content1 == NULL) || (content2 == NULL)) {
		message(1, "hash_string_equal_func: Error: Got nullpointer as parameter\n");
		return 0;
	}
	if (strcmp(content1, content2) == 0)
		return 1; //equal
	return 0;
}
