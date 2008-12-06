#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <libxml/xmlwriter.h>

typedef struct connection {
	const char * host;
	int pid;
	int port;
} connection_t;

void connection_initmemory(void);
int connection_add(int pid, const char * host, int port);
int connection_savexml(xmlTextWriterPtr writer);

#endif
