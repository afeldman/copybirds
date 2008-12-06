
#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <libxml/xmlwriter.h>

#define FO_READ 0x01
#define FO_WRITE 0x02
#define FO_EXEC 0x04

typedef struct faccess {
	char * filename;
	int pid;
	int readed;
	int written;
	int executed;
	char * version;
	char * shasum;
} faccess_t;

typedef struct faccessk {
	const char * filename;
	int pid;
} faccessk_t;



void files_initmemory(void);
int file_add(const char * filename, int acctype, int pid);
int files_savexml(xmlTextWriterPtr writer);


#endif
