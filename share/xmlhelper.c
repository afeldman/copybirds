/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "xmlhelper.h"
#include "tools.h"

/*
In theory all functions should be safe to use with nullpointers as parameters,
however, in this case they will most likely produce error messages.

*/

/*
Tries to determine the encoding, if nothing similar to iso_8859_1 is found,
utf8 is assumed.
Returns: 1: utf8, 2: iso_8859_1 */
int get_encoding(void) {
	char * var = getenv("LC_CTYPE");
	int enc = 1;
	if (var != NULL) {
		if (strcasestr(var, "iso_8859_1")) {
			enc = 2;
		}
	} else {
		var = getenv("LANG");
		if (var != NULL) {
			if (strcasestr(var, "iso_8859_1")) {
				enc = 2;
			}
		}
	}
	message(4, "get_encoding: Type is: %i\n", enc);
	return enc;
}

/*Converts the given string to utf-8, if possible and returns a copy, which has
to be freed after usage. */
xmlChar * xml_cleanutf8(const char * key) {
	if (key == NULL)
		return NULL;
	if (!xmlCheckUTF8((unsigned char * )key)) {
		message(4, "xml_cleanutf8: Warning: '%s' is not utf-8 encoded\n", key);
		int ilen = strlen(key);
		int olen = ilen*4; //four bits for one char should be the worst case
		unsigned char * outbuf = (unsigned char *) calloc(olen+1, sizeof(char));
		isolat1ToUTF8(outbuf, &olen, (unsigned char *) key, &ilen);
		if (!xmlCheckUTF8(outbuf)) {
			message(1, "xml_cleanutf8: Error: Converting '%s' to utf-8 failed\n", key);
		}
		return (xmlChar *)outbuf;
	}
	return (xmlChar *)strdup(key);
}

/* Converts the given xmlChar text into the locale settings, if possible.
xtext will be freed after converting. The returning value has to be freed after
usage. */
char * xmlchar_to_char(xmlChar * xtext) {
	if (xtext == NULL)
		return NULL;
	char * text;
	if (get_encoding() == 2) {	//convert to iso
		message(4, "xmlchar_to_char Converting sting to iso\n");
		int ilen = xmlStrlen(xtext);
		int olen = ilen;
		text = (char *)calloc(olen+1, sizeof(char));
		UTF8Toisolat1((unsigned char *) text, &olen, xtext, &ilen);
	} else
		text = strdup((char *)xtext);
	xmlFree(xtext);
	return text;
}

/*
Returns the value of the attribute key in the node.
Returns NULL if something went wrong. */
char * xml_get_attribute(xmlNodePtr node, const char * key) {
	xmlChar * keyu = xml_cleanutf8(key);
	xmlChar * strattr = xmlGetProp(node, keyu);
	xmlFree(keyu);
	if (strattr == NULL) {
		message(1, "get_attribute: Error: no attribute with the name '%s' found\n",
		 key);
		return NULL;
	}
	return xmlchar_to_char(strattr);
}

/*
Returns the value of he attribute key in the node, converted to a decimal
number, returns 0 if something goes bad.
*/
int xml_get_number_attr(xmlNodePtr node, const char * key) {
	char * strattr = xml_get_attribute(node, key);
	int value = 0;
	if (strattr != NULL) {
		value = atoi(strattr);
	} else
		message(1, "get_number_attr: Error: No valid attribute '%s' found\n", key);
	free(strattr);
	return value;
}


/*
Returns 1 if the type of the node is an element (and not commentary or spaces or
something other curious). Otherwise 0 is returned.
*/
int xml_is_usefulnode(xmlNodePtr node) {
	if (node == NULL) {
		message(1, "xml_is_usefulnode: Error: got a null pointer as node\n");
		return 0;
	}
	if (node->type == XML_ELEMENT_NODE) {
		return 1;
	} else {
		message(4, "xml_is_usefulnode: Found not fitting node of type %i\n",
		        node->type);
		xml_print_debug(node);
	}
	return 0;
}

/*
Returns the name of the node, the string has to be freed after usage.
*/
char * xml_get_nodename(xmlNodePtr cur) {
	if (cur == NULL) {
		message(1, "xml_get_nodename: Error: Node is a NULl pointer\n");
		return NULL;
	}
	const xmlChar * name = cur->name;
	if (name == NULL) {
			message(1, "xml_get_nodename: Error: Node has no name\n");
		return NULL;
	}
	return xmlchar_to_char(xmlStrdup(name));
}

/* Once again, where as simple thing has been realized in a difficult way
Based on: http://xmlsoft.org/tutorial/ar01s04.html
Searches through all children nodes of parent and return the first where the
name
fits. If noe is found, NULL is returned.
 */
xmlNodePtr xml_find_node(xmlNodePtr parent, const char * nodename) {
	if (parent == NULL) {
		message(1, "xml_find_node: Error: Parent node to search in is a\
 nullpointer\n");
		return NULL;
	}
	if (nodename == NULL) {
		message(1, "xml_find_node: Error: Name to search for is a nullpointer\n");
		return NULL;
	}
	xmlNodePtr cur = parent->children;
	while (cur != NULL) {
		message(4, "xml_find_node: found '%s'\n", cur->name);
		char * thename = xml_get_nodename(cur);
		if (thename == NULL) //should not happen if the file is valid
			break;
		int res = strcmp(thename, nodename);
		free(thename);
		if (!res) { //found
			return cur;
		}
		cur = cur->next;
	}
	return NULL; //not found
}

/*
Returns the content of the node, if any.
Otherwise NULL is returned */
char * xml_get_nodecontent(xmlNodePtr cur) {
	if (cur == NULL) {
		message(1, "xml_get_nodename: Error: Node is a NULL pointer\n");
		return NULL;
	}
	xmlChar * t = xmlNodeGetContent(cur);
	if (t == NULL) {
		message(1, "xml_get_nodename: Error: Content is a NULL pointer\n");
		return NULL;
	}
	return xmlchar_to_char(t);
}

/*
Returns the content of the first child from parent, which matches the nodename.
If no child with a proper name is found, or if the content is NULL, NULL
is returned */
char * xml_get_content(xmlNodePtr parent, const char * nodename) {
	xmlNodePtr tmp = xml_find_node(parent, nodename);
	if (tmp != NULL) {
		return  xml_get_nodecontent(tmp);
	}
	return NULL;
}

/*
Returns a child of parent with the nodename. If none is found, one will be created.
So as long as the parameters are valid and no internal error occured you can
expect that the returned pointer is a valid and useable one */
xmlNodePtr xml_have_node(xmlNodePtr parent, const char * nodename) {
	if (parent == NULL) {
		message(1, "xml_have_node: Error: Got NULL as parent node\n");
		return NULL;
	}
	if (nodename == NULL) {
		message(1, "xml_have_node: Error: Got NULL as nodename\n");
		return NULL;
	}
	xmlNodePtr tmp = xml_find_node(parent, nodename);
	if (tmp == NULL) {
		message(4, "xml_have_node: New node '%s'\n",nodename);
		xmlChar * nodenameu = xml_cleanutf8(nodename);
		tmp = xmlNewTextChild(parent, NULL, nodenameu, NULL);
		xmlFree(nodenameu);
		if (tmp == NULL) {
			message(1, "xml_have_node: Error: Created child, but got a NULL\
 pointer\n");
		}
	}
	return tmp;
}

/*
Sets the first child of parent, which matches the nodename, with newcontent.
If no fitting node with a proper name is found, one is created.
the found or created node is returned. If an error happened, NULL is returned.
*/
xmlNodePtr xml_have_content(xmlNodePtr parent, const char * nodename,
                              const char * newcontent) {
	if (parent == NULL) {
		message(1, "xml_have_content: Error: Got NULL as parent node\n");
		return NULL;
	}
	if (nodename == NULL) {
		message(1, "xml_have_content: Error: Got NULL as nodename\n");
		return NULL;
	}
	if (newcontent == NULL) {
		message(1, "xml_have_content: Error: Got NULL as content\n");
		return NULL;
	}
	xmlNodePtr tmp = xml_have_node(parent, nodename);
	if (tmp == NULL) {
		message(1, "xml_have_content: Error: Created child, but got a NULL\
 pointer\n");
		return NULL;
	}
	message(4, "xml_have_content: Updateing '%s'\n", nodename);
	//xmlNodeSetContent(tmp, BAD_CAST newcontent); does not encapsulate
	xmlNodeSetContentLen(tmp, BAD_CAST "", 0);
	xmlChar * newcontentu = xml_cleanutf8(newcontent);
	xmlNodeAddContent(tmp, newcontentu); //does encapsulate special chars
	xmlFree(newcontentu);
	return tmp;
}

/*Searches within the childs of parent for node with a fitting name, attribute
and value of the attribute. The first match is returned. If none is found a
child, matching all these criteria, is created an the pointer to the new node
is returned.
NULL is returned only, if some error happened.
*/
xmlNodePtr xml_have_infonode(xmlNodePtr parent, const char * name,
 const char * attribute, const char * value) {
	if (parent == NULL) {
		message(1, "xml_have_infonode: Error: Node to search in is a nullpointer\n");
		return NULL;
	}
	xmlChar * nameu = xml_cleanutf8(name);
	xmlChar * attributeu = xml_cleanutf8(attribute);
	xmlChar * valueu = xml_cleanutf8(value);
	xmlNodePtr winner = NULL; //the one who will be returned
	xmlNodePtr child = parent->children;
	while ((child != NULL) && (winner == NULL)) {
		if (xml_is_usefulnode(child)) {
			//test if the name fits
			char * cname = xml_get_nodename(child);
			if (cname != NULL) {
				if (!strcmp(cname, name)) {
					//test if attribute name fits
					char * strattrc = xmlchar_to_char(xmlGetProp(child, attributeu));
					if (strattrc != NULL) {
						//test if property is the same
						if (!strcmp(strattrc, value)) { //if we found an existing
							message(4, "xml_have_infonode: Found fitting node\n");
							winner = child;
						}
						free(strattrc);
					}
				}
			}
		}
		child = child->next;
	}
	if (winner == NULL) { //if not existing, create
		message(4, "xml_have_infonode: Make new node\n");
		winner = xmlNewTextChild(parent, NULL, nameu, NULL);
		xmlNewProp(winner, attributeu, valueu);
	}
	if (winner == NULL) {
		message(1, "xml_have_infonode: Error: Could not generate node, please\
 debug.\n");
	}
	xmlFree(nameu);
	xmlFree(attributeu);
	xmlFree(valueu);
	return winner;
}

/* Opens the filename and saves the result of the opening in doc.
In the opened file, rootname is searched and the pointer returned.
In the case something went bad, NULL is returned.
*/
xmlNodePtr xml_open_file(const char * filename, const char * rootname,
                         xmlDocPtr * doc) {
	LIBXML_TEST_VERSION
	*doc = xmlReadFile(filename, NULL, 0);
	if (*doc == NULL) {
		message(1, "xml_open_file: Error: Could not open input file '%s'\n",
		        filename);
		return NULL;
	}
	xmlNodePtr root_element = xmlDocGetRootElement(*doc);
	if (root_element == NULL) {
		message(1, "xml_open_file: Error: file '%s' is not a valid xml file\n",
		        filename);
		return NULL;
	}
	if (root_element->name == NULL) {
		message(1, "xml_open_file: Error: The root_element in '%s' has no name\n",
		        filename);
		return NULL;
	}
	xmlChar * rootnameu = xml_cleanutf8(rootname);
	if (xmlStrcmp(root_element->name, rootnameu)) {
		message(1, "xml_open_file: Error: file '%s' does not have a '%s' tag\n",
		        rootname, filename);
		xmlFree(rootnameu);
		return NULL;
	}
	xmlFree(rootnameu);
	return root_element;
}

/*
Based on: http://xmlsoft.org/library.html
Generates a new xml xml structure with the rootname.
The DocPtr is stored in doc and the pointer to the rootelement is returned.
May return NULL if something completely unexpected happened. */
xmlNodePtr xml_new_file(const char * rootname, xmlDocPtr * doc) {
	LIBXML_TEST_VERSION
	*doc = xmlNewDoc(BAD_CAST  "1.0");
	(*doc)->children = xmlNewDocNode(*doc, NULL, BAD_CAST  rootname, NULL);
	if ((*doc)->children == NULL)
		message(1, "xml_new_file: Error: xmlNewDocNode failed\n");
	return (*doc)->children;
}

/*
Close the given doc and cleans up the parser.*/
void xml_close_file(xmlDocPtr doc) {
	xmlFreeDoc(doc);
	xmlCleanupParser();
}

/*
will return a pointer to the given rootname. If existing, filename will be
opened, otherwise a new file will be created.
May return NULL if something completely unexpected happened. */
xmlNodePtr xml_have_file(const char * filename, const char * rootname,
	                       xmlDocPtr * doc) {
	if (path_exists(filename)) {
		message(1, "xml_have_file: Updateing existing file\n");
		return xml_open_file(filename, rootname, doc);
	} else { //we need a new file
		message(1, "xml_have_file: Creating a new output file\n");
		return  xml_new_file(rootname, doc);
	}
}

/*
Returns the number of propertiers a node has. 0 if node is NULL */
int xml_count_properties(xmlNodePtr node) {
	if (node == NULL) {
		message(1, "xml_count_properites: Error: got a null pointer as node\n");
		return 0;
	}
	int num = 0;
	xmlAttr * attrib = node->properties;
	while(attrib != NULL) {
		num++;
		attrib = attrib->next;
	}
	return num;
}

/*
Returns the number of useful children for the given node. If node is NULL,
an error message is printed and 0 is returned.
*/
int xml_count_children(xmlNodePtr node) {
	if (node == NULL) {
		message(1, "xml_count_children: Error: got a null pointer as node\n");
		return 0;
	}
	int num = 0;
	xmlNodePtr child = node->children;
	while (child != NULL) {
		if (xml_is_usefulnode(child))
			num++;
		child = child->next;
	}
	return num;
}

/*
Prints name, type and content of the given node */
void xml_print_debug(xmlNodePtr node) {
	if (node == NULL) {
		message(4, "xml_print_debug: Node is a null pointer\n");
	return;
	}
	message(4, "xml_print_debug: Name: '%s', type: '%i', content: '%s'\n",
	       (char *)(node->name), node->type, (char *)(node ->content));
}

/*
Runs handler for every child of parent where the name of the child matches
the name parameter.
Returns 0 on success or -1 if parent is a NULL pointer.
*/
int xml_child_handler(xmlNodePtr parent, const char * name, nodefunc handler) {
	if (parent != NULL) {
		xmlNodePtr cur = parent->children;
		while (cur != NULL) {
			if (xml_is_usefulnode(cur)) {
				char * nodename = xml_get_nodename(cur);
				if (nodename != NULL) {
					if (strcmp(nodename, name) == 0) {
						handler(cur);
					}
					free(nodename);
				} else
					message(1, "xml_child_handler: Error: Child node did not have a name\n");
			}
			cur = cur->next;
		}
	} else {
		message(1, "xml_child_handler: Error: Parent is a null pointer\n");
		return -1;
	}
	return 0;
}
