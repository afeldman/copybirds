
#ifndef DATAMANAGER_H
 #define DATAMANAGER_H

struct pidstore {
	struct pidstore * next;
	int pid;
};

#define fstore_t struct fstore

#define pidstore_t struct pidstore

int pid_add(int pid);
int pid_count(void);

int bl_add(const char * bad);
int bl_check(const char * check);

int file_add(const char * filename);
int file_count(void);
int file_exists(const char * filename);
void file_start_iterate(void);
const char * file_get_next(void);
int file_has_next(void);

int device_add(const char * filename);
void device_start_iterate(void);
const char * device_get_next(void);
int device_has_next(void);

void datamanager_initmemory(void);



#endif
