//(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2
#ifndef READPACKAGESTATES_H
#define READPACKAGESTATES_H

#define INITIAL_BUF_LEN 128

typedef struct pstore {
	char * name;
	char * version;
	int installed;
} pstore_t;

//Please init before use!
void package_states_initmemory(void);

pstore_t * package_add(const char * package, const char * version, int installed);
pstore_t * package_get(const char *package);

int package_states_read(const char * filepath);

#endif
