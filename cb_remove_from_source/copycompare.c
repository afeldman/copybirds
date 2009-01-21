
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#include "copycompare.h"
#include "../share/tools.h"
#include "../share/dirtools.h"

int allow_deletion = 0;

//only paths beginning with the sourcepathbase string are allowed to be deleted
//the variables were set in main and won't be modified later
char * sourcepathbase;
char * targetpathbase;


int setup_args(int argc, char *argv[]) {
	int myargs = 0;
	//interesting args
	char * arg = argv[1];
	while (arg[0] == '-') {
		myargs++;
		if (strcmp(arg,"-v") == 0) {
			set_chatting(PGN, 2);
		} else
		if (strcmp(arg,"-vv") == 0) {
			set_chatting(PGN, 3);
		} else
		if (strcmp(arg,"-q") == 0) {
			set_chatting(PGN, 0);
		} else
		if (strcmp(arg,"-d") == 0) {
			set_chatting(PGN, 4);
		} else
		if (strcmp(arg,"-dd") == 0) {
			set_chatting(PGN, 5);
		} else
		if (strcmp(arg,"-keep") == 0) {
			allow_deletion = 0;
		} else
		if (strcmp(arg,"-delete") == 0) {
			allow_deletion = 1;
		} else
		if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"-help") == 0) || (strcmp(arg,"--help") == 0)) {
			printf("License: GPL Version 2.0\n");
			printf("Deletes any files in the source dir, which exists already on the target directory\n");
			printf("SOURCEDIR TARGETDIR\n");
			printf("Normally the program returns: 0 if succeed, 1 if an error occurred\n");
			printf("You may use the following options:\n");
			printf("-v: Be verbose\n");
			printf("-vv: Be very verbose\n");
			printf("-d: Debug messages. Even more verbose than -vv\n");
			printf("-dd: Ultra debug messages. You will be overwhelmed by non understandable messages.\n");
			printf("-q: Be quiet\n");
			printf("-keep: Do not delete any files (default)\n");
			printf("-delete: Do the action (do not run as root or on a system without backups)\n");
			printf("-h, -help, --help: Show this screen\n");
			printf("--version: Shows the version\n");
			exit(0);
		} else
		if (strcmp(arg,"--version") == 0) {
			printf(PGN": Version "PGNVERSION"\n"); //Do not print PID
			exit(0);
		} else {
			message(1, "Error: Option '%s' is unknown\n", arg);
			exit(0);
		}
		if (1+myargs >= argc) //security out
			break;
		arg = argv[1+myargs];
	}
	return myargs;
}

const char typenames[6][32] = {
	"directory", "file", "link to a file", "link to a directory",
	"link to an unknown target", "unknown"};

/*finding remove function: http://bytes.com/forum/thread215843.html
Deletes the given file if certain security checks don't call 'stop'.
*/
void del_file(const char * path) {
	//Safetycheck if deleted file is within the source dir
	char * base = path_from_file(path);
	char * tbpath = truepath(base);
	message(4, "del_files: True base path of '%s' is: '%s'\n", path, tbpath);
	if ((beginswith(path, sourcepathbase) == 0) ||
	    (beginswith(tbpath, sourcepathbase) == 0)) {
		message(1, "del_file: Error: '%s' is not within '%s' this may be a\
 programming bug or the filesystem was changed by an other program.\
 Exiting for security reason.\n", path, sourcepathbase);
		exit(1);
	}
	free(base);
	free(tbpath);
	if (allow_deletion) {
		message(2, "Deleting file '%s'\n", path);
		if (remove(path)) {
			message(1, "del_file: Error: deletion of '%s' failed. This is most likely a programming bug,\n", path);
			message(1, "del_file: or not wished by the user. Exiting for security reason.\n", path);
			exit(1);
		}
	} else
		message(1, "del_file: Would have deleted '%s'\n", path);
}

/*
Compares two files binary and print a warning if they are not equal.
The source file will be deleted anyway.
*/
void h_compf(comparedata_t * comp) {
	const int bs = 256;
	FILE * s = fopen(comp->source, "r");
	if (s == NULL) {
		message(1, "h_compf: Error: Could not open source file '%s',\n", comp->source);
		message(1, "h_compf: This may not happen. Exiting\n");
		exit(1);
		return;
	}
	FILE * t = fopen(comp->target, "r");
	if (t == NULL) {
		message(1, "h_compf: Error could not open target file '%s'\n", comp->target);
		fclose(s);
		return;
	}
	char * sm = (char *)smalloc(sizeof(char)*bs);
	char * tm = (char *)smalloc(sizeof(char)*bs);
	int equals = 1;
	int sr, tr;
	do {
		sr = fread(sm, sizeof(char), bs, s);
		tr = fread(tm, sizeof(char), bs, t);
		if (memcmp(sm, tm, sr) != 0) {
			equals = 0;
		}
		if (sr != tr) {
			equals = 0;
		}
	} while ((equals) && (sr > 0) && (tr > 0));
	free(sm);
	free(tm);
	fclose(s);
	fclose(t);
	if (equals == 0) {
		message(1, "h_compf: Warning: Source '%s' and target '%s' files are not binary equal\n",
		        comp->source, comp->target);
	} else
		message(3, "h_compf: Source '%s' and target '%s' are binary equal\n",
		        comp->source, comp->target);
	del_file(comp->source);
}

void h_warn(comparedata_t * comp) {
	message(1, "Warning: Source '%s' is from the type '%s', but target is from the type '%s'\n",
	        comp->source, typenames[comp->typesource], typenames[comp->typetarget]);
}

void h_warn_del(comparedata_t * comp) {
	h_warn(comp);
	del_file(comp->source);
}

void h_del(comparedata_t * comp) {
	//be sure about what you do!
	del_file(comp->source);
}

void h_keep(comparedata_t * comp) {
	//nothing to do
	message(3, "h_keep: Nothing to do with source %s\n", comp->source);
}

/*
Compares the target of the two links and warns if there are differences.
Deletes the source anyway.
*/
void h_link_link(comparedata_t * comp) {
	/*As we may have gone already through link directories, reveal the true path
	  once again (for the current table, only target may contain links, but this
	  change one day)
	  We can expect: source and path are absolute and are pointing to directories
	*/
	char * srctar = haveslashending_free(reveal_true_abs_link_target(comp->source));
	char * tartar = haveslashending_free(reveal_true_abs_link_target(comp->target));
	/* There may be two situations now:
		Either they were absolute targets or relative ones. This makes no difference
		for the source but for the target.
		If they are absolute, a string comparison will do everything.
		if they should be equal if the base is removed */
	if ((srctar != NULL) && (tartar != NULL)) {
		if (strcmp(srctar, tartar) != 0) {
			//either they are different or one was relative
			char * srcnobasetar = text_get_between(srctar, sourcepathbase, NULL);
			char * tarnobasetar = text_get_between(tartar, targetpathbase, NULL);
			if ((srcnobasetar != NULL) && (tarnobasetar != NULL)) {
				if (strcmp(srcnobasetar, tarnobasetar) != 0) {
					message(1, "h_link_link: Warning: targets of '%s' and '%s' are\
 pointing to different directories\n", comp->source, comp->target);
				}
			} else {
				/*Not expected to happen often, as there must be a mismatch AND
					the targets are outside of the sources.
				*/
				message(1, "h_link_link: Warning: targets of '%s' and '%s' are\
 pointing to different directories and at least one is outside their base\
 directory\n", comp->source, comp->target);
			}
		}
	} else {
		message(1, "h_link_link: Warning: targets of '%s' and '%s' could not be\
 compared\n", comp->source, comp->target);
	}
	free(srctar);
	free(tartar);
	del_file(comp->source);
}

void h_sourceerr(comparedata_t * comp) {
	message(1, "h_sourceerr: Error: Could not access the source file '%s'\n",
	        comp->source);
}

/*target:   dir       file      linkfile       linkdir        none
  source:
  dir       recurse   warn+del  warn           recurse        recurse
  file      warn      compare   comparetarget  warn+del       keep
  linkfile  warn+del  warn+del  compare        warn+del       keep
  linkdir   warn+del  warn+del  warn+del       comparetargets keep
*/

#define IS_DIR 0
#define IS_FILE 1
#define IS_LINKFILE 2
#define IS_LINKDIR 3
#define IS_LINKUNKNOWN 4
#define IS_NONE 5

#define NUMS_ALLOWED 6

/*
Determines the type of the target if path is a symbolic link.
*/
int linktype(const char * path) {
	int w = IS_NONE;
	struct stat stats;
	if (stat(path, &stats) == 0) {
		if (S_ISDIR(stats.st_mode)) {
			w = IS_LINKDIR;
		} else if (S_ISREG(stats.st_mode)) { //normal file
			w = IS_LINKFILE;
		} else if (S_ISLNK(stats.st_mode)) { // a link
			w = IS_NONE;
			message(1, "linktype: Debugme: stat '%s' returned a link\n", path);
		}
	} else {
		message(2, "linktype: Warning did not get stats about '%s', perhaps the destination is missing.\n", path);
		w = IS_LINKUNKNOWN;
	}
	return w;
}

//functionpointer howto: http://www.newty.de/fpt/fpt.html#r_value

typedef void (*handlefunc) (comparedata_t *);
// v-source -->target
handlefunc handletable[NUMS_ALLOWED][NUMS_ALLOWED] = {
{rec_dir,     h_warn,      h_warn,      rec_dir,      h_warn,   rec_dir},
{h_warn,      h_compf,     h_compf,     h_warn,       h_warn,   h_keep},
{h_warn_del,  h_warn_del,  h_compf,     h_warn_del,   h_warn,   h_keep},
{h_warn_del,  h_warn_del,  h_warn_del,  h_link_link,  h_warn,   h_keep},
{h_warn_del,  h_warn_del,  h_del,       h_del,        h_warn,   h_keep},
{h_sourceerr, h_sourceerr, h_sourceerr, h_sourceerr,  h_sourceerr, h_sourceerr}
};

handlefunc getfunc(int typesource, int typetarget) {
	if ((typetarget >= 0) && (typetarget < NUMS_ALLOWED) &&
    (typesource >= 0) && (typesource < NUMS_ALLOWED)) {
		return handletable[typesource][typetarget];
	}
	message(1, "getfunc: Internal Error: Index out of bounds, please debug\n");
	exit(1);
}

/*
Determines the type of source and target and calls a proper situation for
handling each combination.
*/
void * handle_paths(const char * base, const char * name, void * parentcomp) {
	if ((base == NULL) || (name == NULL) || (parentcomp == NULL)) {
		message(1, "handle_paths: Error: One of the parameters was a null pointer.\
 Please Debug. Exiting.\n");
		exit(1);
	}
	comparedata_t comp;
	comparedata_t * parentcompp = (comparedata_t *)parentcomp;
	if (strcmp(parentcompp->source, base)) {	//some safety check
		message(1, "handle_paths: Error: base '%s' and internal base '%s' mismatch.\
 Please Debug. Exiting.\n", base, parentcompp->source);
		exit(1);
	}
	char * newsource = dirmerge(parentcompp->source, name);
	char * newtarget = dirmerge(parentcompp->target, name);
	comp.source = newsource;
	comp.target = newtarget;
	comp.typesource = IS_NONE;
	comp.typetarget = IS_NONE;
	//check if file or directory
	struct stat stats;
	if (lstat(comp.source, &stats) == 0) {
		if (S_ISLNK(stats.st_mode)) { // a link
			comp.typesource = linktype(comp.source);
			message(4, "handle_paths: %s is a link, %i\n", comp.source, comp.typesource);
		} else if (S_ISDIR(stats.st_mode)) {
			message(4, "handle_paths: %s is a directory\n", comp.source);
			comp.typesource = IS_DIR;
		} else if (S_ISREG(stats.st_mode)) { //normal file
			message(4, "handle_paths: %s is a file\n", comp.source);
			comp.typesource = IS_FILE;
		}
	}
	if (lstat(comp.target, &stats) == 0) {
		if (S_ISLNK(stats.st_mode)) { // a link
			comp.typetarget = linktype(comp.target);
		} else if (S_ISDIR(stats.st_mode)) {
			comp.typetarget = IS_DIR;
		} else if (S_ISREG(stats.st_mode)) { //normal file
			comp.typetarget = IS_FILE;
		}
	}
	getfunc(comp.typesource, comp.typetarget)(&comp);
	free(newsource);
	free(newtarget);
	return NULL; //as we do not set the dorecursive parameter to one.
}

/*
Recursive go through the next deeper level.
*/
void rec_dir(comparedata_t * parentcomp) {
	if (parentcomp == NULL)	 {
		message(1, "recurse_dir: Error: Got a null pointer. This may not happen,\
		        please debug. Exiting.\n");
		exit(1);
	}
	recursive_dir(parentcomp->source, handle_paths, parentcomp, 0);
}

int main(int argc, char *argv[]) {
	set_chatting(PGN, 1);
	if (argc < 2) {
		message(1, "main: Error: give paramaeters: SOURCEDIR TARGETDIR, or try -h\n");
		return 1;
	}
	int argoffset = setup_args(argc, argv);
	message(1, "Welcome to '%s'\n", argv[0]);
	if (argc-argoffset < 3) {
		message(1, "main: Error: give paramaeters: SOURCEDIR TARGETDIR, or try -h\n");
		return 1;
	}
	comparedata_t comp;
	if ((path_is_absolute(argv[argoffset+1]) != 1) ||
      (path_is_absolute(argv[argoffset+2]) != 1)){
		message(1, "main: Error: Only absolute paths are allowed as parameter\n");
		exit(1);
	}
	//convert paths to true ones with tailing slash
	comp.source = haveslashending_free(truepath(argv[argoffset+1]));
	sourcepathbase = strdup(comp.source);
	comp.target = haveslashending_free(truepath(argv[argoffset+2]));
	targetpathbase = strdup(comp.target);
	message(4, "Abs source: '%s', Abs target: '%s'\n", comp.source, comp.target);
	//check if the parameters make make sense
	if (strcmp(comp.source, "/") == 0) {
		message(1, "main: Error: Source dir may not be / as this would be too dangerous\n");
		exit(1);
	}
	if (strcmp(comp.source, comp.target) == 0) {
		message(1, "main: Error: Both parameters may not be equal, as this would delete everything within the directory\n");
		exit(1);
	}
	//all right, lets go
	recursive_dir(comp.source, handle_paths, &comp, 0);
	return 0;
}
