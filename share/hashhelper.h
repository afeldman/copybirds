/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#ifndef HASHHELPER_H
#define HASHHELPER_H

unsigned long hash_of_string(const char * text);

unsigned long hash_string_func(HashTableKey value);

int hash_string_equal_func(HashTableKey value1, HashTableKey value2);


#endif

