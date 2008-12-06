/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "pidmanager.h"
#include "../share/tools.h"
#include "copyxmler.h"
#include "../share/dirtools.h"

spid_t * first_spid;

/* For remember:
struct spid {
	struct faccess * next;
	int pid;
	char * wdir;
};
*/

/*
Returns a pointer to the struct with the correct pid. Returns NULL if not found.
*/
spid_t * pid_seek(int pid) {
	spid_t * x = first_spid;
	while (x != NULL) {
		if (x->pid == pid) {
			break;
		}
		x = x->next;
	}
	return x;
}

/*
Adds a pid to the list. Returns a pointer to the generated structure */
spid_t * pid_add_entry(int pid, const char * firstwdir) {
	//add at the beginning is faster
	spid_t * n = smalloc(sizeof(spid_t));
	n->next = first_spid;
	first_spid = n;
	n->pid = pid;
	n->wdir = strdup(firstwdir);
	return n;
}

/*
Adds a pid to the list, the initialized path is fetched from the parent pid */
spid_t * pid_add_child(int parent, int child) {
	spid_t * n = smalloc(sizeof(spid_t));
	n->next = first_spid;
	first_spid = n;
	n->pid = child;
	n->wdir = NULL;
	spid_t * temp = pid_seek(parent);
	if (temp) {
		const char * tempstr = temp->wdir;
		if (tempstr) {
			n->wdir = strdup(tempstr);
		} else
			message(1, "pid_add_child: Error: Parent '%i' had no working directory set\n", parent);
	} else
		message(1, "pid_add_child: Error: Parent pid %i was not found\n", parent);
	return n;
}

/*
Updates the path of the given pid. to match newwdir. newwdir may be an absolute
or relative path. The newwdir is copied and the parameter an safely be freed */
void pid_update_path(int pid, const char * newwdir) {
	message(2,"pid_update_path: %s\n", newwdir);
	spid_t * proc = pid_seek(pid);
	if (proc != NULL) {
		char * oldwdir = proc->wdir;
		if (path_is_relative(newwdir)) {
			if (oldwdir != NULL) {
				char * abspath = smalloc(MAX_PATH_LEN);
				rel2abs(oldwdir, newwdir, abspath, MAX_PATH_LEN-1);
				proc->wdir = abspath;
			} else
				message(1, "pid_update_path: Error the process %i has no working directory set\n", pid);
		} else {
			proc->wdir = strdup(newwdir);
		}
		if (oldwdir != NULL) {
			free(oldwdir);
		}
	} else
		message(1, "pid_update_path: Error no process found with ID %i\n", pid);
}

/*
Returns the absolute path of filename based on the current working directory
of the pid. If the pid os not found or does not have a filename set, a copy of
filename is returned. The returned string may be freed after usage. */
char * pid_get_absolute_path(int pid, const char * filename) {
	if (path_is_relative(filename)) {
		spid_t * proc = pid_seek(pid);
		if (proc != NULL) {
			char * wdir = proc->wdir;
			if (wdir != NULL) {
				char * abspath = smalloc(MAX_PATH_LEN);
				rel2abs(filename, proc->wdir, abspath, MAX_PATH_LEN-1);
				message(2, "pid_get_absolute_path: Abspath from %s is %s\n", filename, abspath);
				return abspath;
			} else
				message(1, "pid_get_absolute_path: Error the process %i has no working directory set\n", pid);
		} else
			message(1, "pid_get_absolute_path: Error no process found with ID %i\n", pid);
	}
	return strdup(filename);
}

/*
Returns a copy of the currently set working directory for the pid.
Returns NULL if the pid is not known or no working directory is set */
char * pid_get_wdir(int pid) {
	spid_t * proc = pid_seek(pid);
	if (proc != NULL) {
		char * wdir = proc->wdir;
		if (wdir != NULL) {
			return strdup(wdir);
		}
	}
	message(1, "pid_get_wdir: Warning: Did not found a working directory for pid %i\n", pid);
	return NULL;
}
