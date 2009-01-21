
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "dirtools.h"
#include "tools.h"


/*
Returns 1 if base concaternaded with name is a directory, otherwise 0 will be
 returned */
static int is_directory(const char * base, const char * name) {
	int is = 0;
	char * path = dirmerge(base, name);
	if (path_is_dir(path)) {
		is = 1;
	}
	free(path);
	return is;
}

/*
Starts at the path given as base. For every entry, excluding . and .., the
handler function is called.
If dorecursive is set to one, the function calls itself on this directory.
Somedata can be used for directing information to the next deeper directory.
For deeper directories somedata contains the return value which the handler
function returned for this directory before.

idea of opendir: http://bytes.com/forum/thread62886.html
howto use: http://www.willemer.de/informatik/cpp/fileop.htm
*/
void recursive_dir(const char * base, pathfunc handler, void * somedata,
                   int dorecursive) {
	static int depth = 0;
	if (depth > DEPT_CUTOFF) {
		message(1, "recurse_dir: Warning: Looks like we followed an endless\
 recursive link, exiting at depth %i\n", depth);
		return;
	}
	if (base == NULL) {
		message(1, "recursive_dir: Error: parameter 'base' is a null pointer\n");
		return;
	}
	depth++;
	//get a list of all files in the current source directory
	DIR * dirstream;
	struct dirent *entry;
	dirstream = opendir(base);
	if (dirstream != NULL) {
		do {
			entry = readdir(dirstream);
			if (entry) {
				char * filename = entry->d_name;
				message(4, "recurse_dir: Found: '%s'\n", filename);
				/*Avoid endless loops, by not processing the superiour dir and the
				 current twice */
				if ((strcmp(filename, ".") != 0) && (strcmp(filename, "..") != 0)) {
					void * subdata = handler(base, filename, somedata);
					//possible bug: char * newbase = dirmerge(base, filename); missing
					if ((dorecursive) && (is_directory(base, filename))) {
						recursive_dir(filename, handler, subdata, dorecursive); //use newbase instead of filename?
					}
				}
			}
		} while (entry);
		closedir(dirstream);
	} else
		message(1, "recurse_dir: Error: could not open source dir '%s'\n", base);
	depth--;
}

/*
Acts like readlink, but will put a \0 on the ending and makes sure, the buffer
is big enough to fit.
The returning value has to be freed after usage. In the case of an error,
NULL is returned. */
char * sreadlink(const char * name) {
	if (name == NULL) {
		message(1, "sreadlink: Error: Parameter is a null pointer\n");
		return NULL;
	}
	int blen = 512;
	do {
		char * buf = (char *)smalloc(sizeof(char)*blen);
		int tlen = readlink(name, buf, blen);
		if (tlen < 0) {
			message(1, "sreadlink: Error: Could not read the link\n");
			return NULL;
		}
		if (tlen < blen) {
			buf[tlen] = '\0';
			return buf;
		}
		free(buf);
		blen *= 2;
	} while (blen > 0);
	message(1, "sreadlink: Error: Link seems to long\n");
	return NULL; //error
}

/*
The given path may contain links, so that the "true" path may be something else.
This function returns the true path. The String has to be freed after usage.
In the case of an error, NULL is returned.
*/
char * truepath(const char * path) {
	/* howto: crunch down the path from the root to the leaves. whenever a part is
	   a link, replace the content of the path with the new link and restart until
	   nothing changes */
	if (path == NULL) {
		message(1, "truepath: Error: parameter is a null pointer\n");
		return NULL;
	}
	int has_changed = 1;
	char * newpath = nodoubleslash(path);
	message(4, "truepath: checking '%s'\n", path);
	while (has_changed) {
		has_changed = 0;
		const char * index = newpath;
		if (strlen(newpath) > 0) {
			//inner cutting loop
			do {
				index = strchr(index+1, '/');
				int cutter = index-newpath;
				if (index == NULL) //we are at the leave now
					cutter = strlen(newpath);
				if (cutter > 0) {
					char * subhigherpath = strndup(newpath, cutter);//without ending slash
					char * subdeeperpath = strdup(newpath+cutter); //with leading slash
					struct stat stats;
					if (lstat(subhigherpath, &stats) == 0) {
						if (S_ISLNK(stats.st_mode)) {
							has_changed = 1;
							char * target = sreadlink(subhigherpath);
							char * abstarget;
							if (path_is_relative(target)) {
								/* As we started from the root, we can be sure that the
								   everyting withing sublowerpath is free of any links */
								char * parent_pre = path_from_file(subhigherpath);
								char * parent = nodoubleslash(parent_pre);
								free(parent_pre);
								//now make absolute to relative link
								abstarget = (char *)smalloc(sizeof(char)*2048);
								rel2abs(target, parent, abstarget, 2048);
								free(parent);
								free(target); //in this case abstarget != target
							} else { //if absolute, its easy
								abstarget = target;
							}
							free(newpath); //not needed anymore, we will get a new path
							newpath = dirmerge(abstarget, subdeeperpath);
							free(abstarget); //may be the same as target
						}
					}
					free(subhigherpath);
					free(subdeeperpath);
				}
			} while (index != NULL);
			//end inner cutting loop
		}
	}
	return newpath;
}

/*
Give the path to a linkname and a string showing the absolute and true path
of the linktarget is returned. Free after usage.
In the case of an error, NULL is returned.
 */
char * reveal_true_abs_link_target(const char * linkname) {
	if (linkname == NULL) {
		message(1, "reveal_true_abs_link_target: Error: parameter is a null pointer\n");
		return NULL;
	}
	//get the linktarget
	char * linktarget = sreadlink(linkname);
	if (linktarget == NULL) {
		message(1, "reveal_true_abs_link_target: Error: could not get link target\n");
		return NULL;
	}
	char * linkabstarget;
	if (path_is_absolute(linktarget)) { //the easy part
		linkabstarget = strdup(linktarget);
	} else { //otherwise it is relative
		//first get the true base where relative links are based on
		char * namebase = path_from_file(linkname);
		char * nametruebase = truepath(namebase);
		if (nametruebase != NULL) {
			linkabstarget = (char *)smalloc(sizeof(char)*2048);
			rel2abs(linktarget, nametruebase, linkabstarget, 2048);
		} else {
			message(1, "reveal_true_abs_link_target: Error: could not get base of\
 linkname\n");
			linkabstarget = NULL;
		}
		free(namebase);
		free(nametruebase);
	}
	char * linktrueabstarget = truepath(linkabstarget);
	free(linkabstarget);
	return linktrueabstarget;
}
