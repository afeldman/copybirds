#ifndef XMLHELPER_H
#define XMLHELPER_H

typedef void (*nodefunc) (xmlNodePtr node);


char * xml_get_attribute(xmlNodePtr node, const char * key);

int xml_get_number_attr(xmlNodePtr node, const char * key);

xmlNodePtr xml_find_node(xmlNodePtr parent, const char * nodename);

char * xml_get_content(xmlNodePtr cur, const char * nodename);

char * xml_get_nodecontent(xmlNodePtr cur);

char * xml_get_nodename(xmlNodePtr cur);

xmlNodePtr xml_have_content(xmlNodePtr parent, const char * nodename,
  const char * newcontent);

xmlNodePtr xml_have_node(xmlNodePtr parent, const char * nodename);

xmlNodePtr xml_have_infonode(xmlNodePtr cur, const char * name,
  const char * attribute, const char * value);

xmlNodePtr xml_open_file(const char * filename, const char * rootname,
  xmlDocPtr * doc);

xmlNodePtr xml_new_file(const char * rootname, xmlDocPtr * doc);

void xml_close_file(xmlDocPtr doc);

xmlNodePtr xml_have_file(const char * filename, const char * rootname,
 xmlDocPtr * doc);

int xml_count_properties(xmlNodePtr node);

int xml_count_children(xmlNodePtr node);

int xml_is_usefulnode(xmlNodePtr node);

void xml_print_debug(xmlNodePtr node);

int xml_child_handler(xmlNodePtr parent, const char * name, nodefunc handler);

#endif
