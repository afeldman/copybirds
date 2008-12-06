
#ifndef PIDMANAGER_H
#define PIDMANAGER_H

struct spid {
	char * wdir;
	struct spid * next;
	int pid;
};

#define spid_t struct spid


spid_t * pid_add_entry(int pid, const char * firstwdir);
spid_t * pid_add_child(int parent, int child);
void pid_update_path(int pid, const char * newwdir);
char * pid_get_absolute_path(int pid, const char * filename);
char * pid_get_wdir(int pid);

#endif
