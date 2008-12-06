/*
(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#include <string.h>
#include <stdlib.h>

#include <libxml/tree.h>

#include "../lib/hash-table.h"

#include "../share/tools.h"
#include "../share/xmlhelper.h"
#include "../share/hashhelper.h"

#include "merger.h"



//-----------the part for the top level-----------------
HashTable * manc; //metadata anchor

void merger_initmemory(void) {
	manc = hash_table_new(hash_string_func, hash_string_equal_func);
}

/*Compares to values (if they are from the type IS_STING). Returns
1 if equal, 0 if not*/
int vequal_func(metavalue_t * value1, metavalue_t * value2) {
	if (strcmp(value1->name, value2->name) == 0) {
		if ((value1->type == IS_STRING) && (value2->type == IS_STRING)) {
			char * data1 = value1->data;
			char * data2 = value2->data;
			if (strcmp(data1, data2) == 0) {
				return 1; //equal
			}
		}
	}
	return 0;
}


metavalue_t * node_get(const char * nodename) {
	HashTableValue val = hash_table_lookup(manc, (HashTableValue)nodename);
	if (val == HASH_TABLE_NULL) //not found
		return NULL;
	return val;
}

metavalue_t * node_make(const char * nodename, const char * value,
	 const char * sourcefile) {
	metavalue_t * n = smalloc(sizeof(metavalue_t));
	n->name = strdup(nodename);
	n->type = IS_STRING;
	n->data = strdup(value);
	n->sourcefile = strdup(sourcefile);
	return n;
}


void node_destroy(metavalue_t * node) {
	free(node->name);
	free(node->data);
	free(node->sourcefile);
	free(node);
}

/*
Makes an entry from the parameters an adds it to the hash-table if useful.
Returns 0: same element existed, 1: added as new, -1 conflict with existing
element */
int merger_add_entry(const char * nodename, const char * value,
	 const char * filename) {
	message(3, "merger_add_entry: %s is '%s'\n", nodename, value);
	//look if the nodename already exists
	metavalue_t * existing = node_get(nodename);
	metavalue_t * newnode = node_make(nodename, value, filename);
	if (existing != NULL) { //possible conflict?
		if (vequal_func(existing, newnode)) {
			//good, same existed
			node_destroy(newnode); //we do not need it
			return 0;
		} else {
			/*bad, we (most likely) have a conflict
				note: if no content/value is given the type of the node cant be decided
			*/
			if (strlen(value) > 0) {
				message(1, "merger_add_entry: Error: '%s' has two different values\
 defined in the files '%s' and '%s. Will use the first one'\n",
				 nodename, existing->sourcefile, newnode->sourcefile);
			}
			node_destroy(newnode);
			return -1;
		}
	} else {//add new one
		hash_table_insert(manc, strdup(nodename), newnode);
		return 1;
	}
}

//----------the part for sub entries----------------


/*
The hash function to use for sub entries*/
unsigned long shash_func(HashTableKey value) {
	xmlNodePtr key = (xmlNodePtr)value;
	if (key->name != NULL) {
		return hash_of_string((char *)(key->name)); //may be bad hashing here
	}
	message(1, "shash_func: Error: the xmlNode does not have a name set\n");
	return 0;
}

/*
Compares two subhash keys. They are equal if their name and all of their
properties are equal
returns 1 if equal, 0 if not */
int sequal_func(HashTableKey key1, HashTableKey key2) {
	xmlNodePtr k1 = (xmlNodePtr)key1;
	xmlNodePtr k2 = (xmlNodePtr)key2;
	if (xmlStrcmp(k1->name, k2->name) == 0) { //are equal?
		//check if the amount of attributes was equal
		if (xml_count_properties(k1) == xml_count_properties(k2)) {
			/*Compare the value of each property. The documentation of the libxml2
			 is very poor here and i can only guess how it works. I did not found
			 suitable functions to operate on the structures. :-( */
			xmlAttr * attrib = k1->properties;
			while(attrib != NULL) {
				//fetch attribute name
				const xmlChar * attribname = attrib->name;
				if (attribname != NULL) {
					xmlChar * val1 = xmlGetProp(k1, attribname);
					xmlChar * val2 = xmlGetProp(k2, attribname);
					if ((val1 != NULL) && (val2 != NULL)) {
						if (xmlStrcmp(val1, val2)) {
							return 0; //not equal
						}
					}
				} else {
					message(1, "sequal_fun: Error: Strange, an property with a a null\
 pointer\n");
				}
				attrib = attrib->next;
			}
			return 1; //are equal
		}
	}
	return 0;
}

/*
Go trough the children and compare their values, returns 0 if not equal and
1 if equal. This means, that there are the same amount of childrens,
that the childrens have the same name and value.
*/
int svequal_func(metasubvalue_t * value1, metasubvalue_t * value2) {
	//assuming that the key part is already equal
	xmlNodePtr val1 = value1->data;
	xmlNodePtr val2 = value2->data;
	if ((val1 == NULL) || (val2 == NULL)) {
		message(1, "svequal_func: Error, got nullpointers, do not proceed\n");
		return 0;
	}
	if (xml_count_children(val1) == xml_count_children(val2)) {
		xmlNodePtr cur_node = val1->children;
		while (cur_node != NULL) {
			if (xml_is_usefulnode(cur_node)) {
				char * key = xml_get_nodename(cur_node);
				char * con1 = xml_get_nodecontent(cur_node);
				char * con2 = xml_get_content(val2, key);
				if ((con1 == NULL) || (con2 == NULL)) {
					return 0;
				}
				if (strcmp(con1, con2)) {
					return 0;
				}
			}
			cur_node = cur_node->next;
		}
		return 1;
	}
	return 0;
}

metasubvalue_t * node_subget(HashTable * subnodes, xmlNodePtr nodename) {
	HashTableValue val = hash_table_lookup(subnodes, (HashTableValue)nodename);
	if (val == HASH_TABLE_NULL) //not found
		return NULL;
	return val;
}


metasubvalue_t * node_submake(xmlNodePtr value, const char * sourcefile) {
	metasubvalue_t * n = smalloc(sizeof(metasubvalue_t));
	n->data = xmlCopyNode(value, 1);//1 means recursive copy
	n->sourcefile = strdup(sourcefile);
	return n;
}


void node_subdestroy(metasubvalue_t * node) {
	free(node->sourcefile);
	xmlFreeNode(node->data);
	free(node);
}

/*
Adds value to subnodes if there is no conflicting one.
Returns 0: same element existed, 1: added as new, -1 conflict with existing
element */
int merger_add_subentry(HashTable * subnodes, xmlNodePtr value,
	 const char * sourcename) {
	//look if there is already one existing
	metasubvalue_t * existing = node_subget(subnodes, value);
	metasubvalue_t * newnode = node_submake(value, sourcename);
	if (existing != NULL) { //possible conflict?
		if (svequal_func(existing, newnode)) {
			//good, same existed
			node_subdestroy(newnode); //we do not need it
			return 0;
		} else {
			//bad, we have a conflict
			message(1, "merger_add_subentry: Error: '%s' and '%s' have the same keys \
for a subnode within '%s' but with different values. Will use the first one.\n",
			 existing->sourcefile, newnode->sourcefile, xml_get_nodename(value));
			node_subdestroy(newnode);
			return -1;
		}
	} else {//add new one
		hash_table_insert(subnodes, xmlCopyNode(value, 1), newnode);
		return 1;
	}
}

/*
Adds every children from rootvalue to subnodes */
void merger_add_subentries(HashTable * subnodes, xmlNodePtr rootvalue,
	 const char * sourcename) {
	xmlNodePtr cur_node = rootvalue->children;
	while (cur_node != NULL) {
		if (xml_is_usefulnode(cur_node)) {
			merger_add_subentry(subnodes, cur_node, sourcename);
		}
		cur_node = cur_node->next;
	}
}

//---------------connection between top and sublevel---------


metavalue_t * node_make_complex(const char * nodename, const char * sourcefile){
	metavalue_t * n = smalloc(sizeof(metavalue_t));
	n->name = strdup(nodename);
	n->type = IS_HASH;
	n->data = hash_table_new(shash_func, sequal_func);
	n->sourcefile = strdup(sourcefile);
	return n;
}

/*
returns the hash-table for the complex entry with the name 'nodename'.
If some exists, this will be returned
If none exists one will be created
If one exists but it is not compatible to the complex type, an error message
will be thrown an the program exits with status 1.
*/
HashTable * merger_have_complex_entry(const char * nodename,
	 const char * sourcefile) {
	metavalue_t * node = node_get(nodename);
	if (node == NULL) {
		//make a new one and insert
		node = node_make_complex(nodename, sourcefile);
		hash_table_insert(manc, strdup(nodename), node);
	} else {
		//just check if data is a HASHMAP and not a STRING pointer
		if (node->type != IS_HASH) {
			//maybe it is still an empty node like </connections> and we can merge
			if (node->type == IS_STRING) {
				if (node->data == NULL) {
					message(1, "merger_have_complex_entry: Error: data is null. Exiting.\
\n");
				}
				if (strlen(node->data) == 0) {
					//ok, we can merge, as the node is virtually empty
					message(2, "merger_have_complex_entry: Warning: Empty node '%s' from\
 file '%s' will be replaced by complex node by file '%s'\n",
					         nodename, node->sourcefile, sourcefile);
					//remove an free old node
					hash_table_remove(manc, (HashTableValue)nodename);
					node_destroy(node);
					//make a new one and insert
					node = node_make_complex(nodename, sourcefile);
					hash_table_insert(manc, strdup(nodename), node);
				} else {
					//serious problem, abort merging
					message(1, "merger_have_complex_entry: Error: Node '%s' is used in a\
 different way by the files '%s' and '%s'. Can not merge. Exiting.\n",
					nodename, node->sourcefile, sourcefile);
					exit(1);
				}
			} else {
				//serious problem, abort merging
				message(1, "merger_have_complex_entry: Error: Node '%s' is used in an\
 unknown way by the file '%s'. Can not merge. Exiting.\n",
			 nodename, node->sourcefile);
				exit(1);
			}
		}
	}
	return node->data;
}

void merger_write_value(xmlNodePtr meta, metavalue_t * value) {
	xml_have_content(meta, value->name, value->data);
}

/*
Writes the entries from value as childs into the meta pointer */
void merger_write_complex(xmlNodePtr meta, metavalue_t * value) {
	xmlNodePtr subbase = xml_have_node(meta, value->name);
	HashTableIterator sit;
	hash_table_iterate(value->data, &sit);
	while (hash_table_iter_has_more(&sit)) {
		metasubvalue_t * subvalue = hash_table_iter_next(&sit);
		xmlNodePtr subvalnode = subvalue->data;
		xmlAddChild(subbase, subvalnode);
	}
}

/*
Writes all data from the hash-tables into the given meta pointer */
void merger_write_xml(xmlNodePtr meta) {
	HashTableIterator mit;
	hash_table_iterate(manc, &mit);
	while (hash_table_iter_has_more(&mit)) {
		metavalue_t * value = hash_table_iter_next(&mit);
		if (value->type == IS_STRING) {
			merger_write_value(meta, value);
		} else if (value->type == IS_HASH) {
			merger_write_complex(meta, value);
		} else
			message(1, "merger_write_xml: Error: Entry does not have a data type\n");
	}
}
