
#ifndef DATAMANAGER_H
 #define DATAMANAGER_H

typedef struct pfstore {
	pstore_t * package;
	int refcount;
} pfstore_t;

int file_add(char * filename);
int file_count(void);
int file_exists(const char * filename);
void file_start_iterate(void);
const char * file_get_next(void);
int file_has_next(void);

void package_file_add(const char * package, const char * filename);
pfstore_t * package_file_get(const char *filename);

void findings_add(pstore_t * ps);
void findings_start_iterate(void);
pstore_t * findings_get_next(void);
int findings_has_next(void);

void files_initmemory(void);
#endif
