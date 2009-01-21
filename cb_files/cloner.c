/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#include "cloner.h"
#include "../share/tools.h"
#include "copyowl.h"
#include "datamanager.h"
#include "../share/dirtools.h"


void run_if_device(const char * path);
void run_if_file(const char * path, const char * root);
void run_if_dir(const char * path, const char * root);
void run_if_link(const char * path, const char * root, int isbl);
void create_link(const char * source, const char * dest);


/*
goes through each level of the given path. For each level it will be checked
which type the path is and what to do with it.
Example:
path = /home/ubuntu/readme.txt will be checked as
/home -> handle directory
/home/ubuntu -> handle directory
/home/ubuntu/readme.txt -> handle file
In the case a link is followed, the source is at least blacklisted if the target
is it */
void cloner_tokenize_bl(const char * path, const char * root, int isbl) {
	isbl |= bl_check(path);
	const char * index = path;
	if (strlen(path) > 0) {
		do {
			index = strchr(index+1, '/');
			int len = index-path;
			if (index == NULL)
				len = strlen(path);
			if (len > 0) {
				char * subpath = strndup(path, len);
				struct stat stats;
				if (lstat(subpath, &stats) == 0) {
					//printf("is telling to %s : %i\n", subpath, S_ISDIR(stats.st_mode));
					if (S_ISLNK(stats.st_mode)) { // is a link
						run_if_link(subpath, root, isbl);
					} else if (S_ISCHR(stats.st_mode)) { //is a character device
						run_if_device(subpath);
					} else if (S_ISBLK(stats.st_mode)) { //is a blockdevice
						run_if_device(subpath);
					} else if (isbl == 0) { //if not blacklisted
						if (S_ISDIR(stats.st_mode)) { //is a directory
							run_if_dir(subpath, root);
						} else if (S_ISREG(stats.st_mode)) {	//is a file
							run_if_file(subpath, root);
						} else if (S_ISSOCK(stats.st_mode)) {	//is a socket
							message(1, "cloner_tokenize: Warning: Not processing socket '%s'\n");
						} else if (S_ISFIFO(stats.st_mode)) {	//is a fifo
							message(1, "cloner_tokenize: Warning: Not processing FIFO '%s'\n");
						} else
							message(1, "cloner_tokenize: Error: Type of '%s' can't be determined\n", subpath);
					}
				}
				free(subpath);
			}
		} while (index != NULL);
	}
}

void cloner_tokenize(const char * path, const char * root) {
	cloner_tokenize_bl(path, root, 0);
}

void run_if_device(const char * path) {
	device_add(path);
}

/*
Copies the file, if root/path exists
*/
void run_if_file(const char * path, const char * root) {
	message(2, "run_if_file: '%s' is a file\n", path);
	char * destpath = dirmerge(root, path);
	if (access(destpath, F_OK) != 0) {
		int cpid = fork();
		if (cpid == 0) { //the child
			//printf("I would do: 'cp %s %s'\n", path, destpath);
			execlp("cp", "cp", path, destpath, NULL);
			exit(-1);
		}
		int status;
		waitpid(cpid, &status, 0);
	}
	free(destpath);
}

/*
Generates the directory if root/path exists
*/
void run_if_dir(const char * path, const char * root) {
	message(2, "run_if_dir: '%s' is a directory\n", path);
	//create target directory
	char * newdir = dirmerge(root, path);
	if (access(newdir, F_OK) != 0) {
		if (newdir != NULL) {
			int cpid = fork();
			if (cpid == 0) {
				//printf("I would do : 'mkdir -p %s\n", newdir);
				execlp("mkdir", "mkdir", "-p", newdir, NULL);
				exit(-1);
			}
			int status;
			waitpid(cpid, &status, 0);
		}
	}
	free(newdir);
}

/*
Generates the link root/path of not blacklisted and follows the target of the
link recursively */
void run_if_link(const char * path, const char * root, int isbl) {
/*
  linkname: the name of the link which will be created
  linktarget: the name of the directory where linkname is pointing to
  filepath: the directory where linkname is in
  abslinktarget: the absolute target path of the link, which needs to be copied to
  rellinktarget: the relative link target where the new path will point to
*/
	message(2, "run_if_link: '%s' is a link\n", path);
	char * linkname = dirmerge(root, path);
	if (access(linkname, F_OK) != 0) {
		char * linktarget = sreadlink(path);
		char * filepath = path_from_file(path);
		char * abslinktarget;
		char * rellinktarget;
		if (path_is_relative(linktarget)) {
			abslinktarget = dirmerge(filepath, linktarget);
			rellinktarget = strdup(linktarget);
		} else { //is absolute
			abslinktarget = strdup(linktarget);
			rellinktarget = (char *)smalloc(sizeof(char) *2048);
			/*The problem is that the filepath may be a part of a symlink. In order to
			 make correct relative symlinks, the relative path has to be relative to
			 the true path were thesymlink will be really stored in and has to be
			 correct. */
			char * truefilepath = truepath(filepath);
			abs2rel(abslinktarget, truefilepath, rellinktarget, 2047);
			free(truefilepath);
		}
		/* Observe the blacklist, for linkname and abslinktarget.
		  Now there are four possible situations as eiter of them may be blacklisted
		  or not. We follow the abslinktargets anyway because they may point to
		  devices.
		  If none are blacklisted -> create link and go recursive to the target
		  If the linkname is blacklisted -> go recursive to the source
		  If the target is blacklisted -> warn and go recursive to the source
		  If both are blacklisted -> go recursive to the source.
		  Note: any of the two is blacklisted, all targets found recursive will
		  be handled as blacklisted too.
		*/
		message(2,"run_if_link Recursive with '%s'\n", abslinktarget);
		cloner_tokenize_bl(abslinktarget, root, isbl);
		int targetbl = bl_check(abslinktarget);
		if (isbl == 0) { //if linkname is not blacklisted
			if (targetbl == 0) {
				create_link(rellinktarget, linkname);
			} else
				message(1, "run_if_link: Warning: Target for link '%s' is blacklisted and may not exists on the target system\n", linkname);
		}
		free(abslinktarget);
		free(rellinktarget);
		free(linktarget);
		free(filepath);
	}
	free(linkname);
}

/*
Generates a symbolic link
*/
void create_link(const char * source, const char * linkname) {
	int child = fork();
	if (child == 0) { // the child
		execlp("ln", "ln", "-s", source, linkname, NULL);
	}
	int status;
	/* The wait() is very important here. If it is not waited, the next command
		may start copying on the before the link was created, resulting in making a
		directory with the name of the link. Then ln comes in and complains, that
		it could not create the link. I seen this scenario in real. */
	waitpid(child, &status, 0);
	int exitstat = WEXITSTATUS(status);
	if (exitstat != 0) {
		message(1, "create_link: Error: Command 'ln -s %s %s' failed\n", source,
		        linkname);
	}
}
