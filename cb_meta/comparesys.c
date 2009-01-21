/*(c) 2008 by Malte Marwedel
This code may be used under the terms of the GPL version 2.
*/

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

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <GL/glut.h>

#include "../lib/trie.h"
#include "../lib/hash-table.h"

#include "../share/tools.h"
#include "../share/xmlhelper.h"
#include "../share/packagestates.h"
#include "../share/hashhelper.h"

#include "copymeta.h"

int test_gl = 0;

	/*first test if the child can run properly, as this is not the case on every
	 system. returns 1 if succeed, 0 if failed */
int test_init_glut(int argc, char *argv[]) {
	fflush(stdout); //otherwise the buffer might printed twice
	int child = fork();
	if (child == 0) {
		glutInit(&argc, argv);
		glutInitWindowSize(600,30);
		//glutInitWindowPosition(10,10);
		//glutInitDisplayMode(GLUT_RGB);
		glutCreateWindow(PGN" child dummy window");//otherwise glGetString returns null
		exit(0);
	}
	int status;
	waitpid(child, &status, 0);
	int rval = WEXITSTATUS(status);
	if (rval == 0) { //if all went well
		//now do it again by the parent
		glutInit(&argc, argv);
		glutInitWindowSize(600,30);
		//glutInitWindowPosition(10,10);
		//glutInitDisplayMode(GLUT_RGB);
		glutCreateWindow(PGN" dummy window");//otherwise glGetString returns null
		return 1;
	} else {
		message(1, "test_init_glut: Error: Could not initiate glut. Will not compare/sample openGL infos\n");
	}
	return 0;
}

/*
Will get information about the system. Including the graphic card.
Note: the lsb_release calls are slow and take a second each.
*/
void fetch_meta(int argc, char *argv[]) {
	test_gl = test_init_glut(argc, argv);
	message(1, "fetch_meta: Getting system information...\n");
	//getenv idea: http://www.linuxforums.org/forum/linux-programming-scripting/123517-get-data-shell-variable-into-gtk-2-0-a.html
	csys.loginname = getenv("LOGNAME");
	message(2, "fetch_meta: Loginname: '%s'\n", csys.loginname);
	csys.homedir = getenv("HOME");
	message(2, "fetch_meta: Home directory: '%s'\n", csys.homedir);
	csys.kernelver = get_cmd_output("uname", NULL, "-r", 0);
	message(2, "fetch_meta: Kernel Version: '%s'\n", csys.kernelver);
	//based on: http://ubuntuforums.org/showthread.php?t=796627
	csys.distro =  get_cmd_output("lsb_release", NULL, "-si", 0);
	message(2, "fetch_meta: Distribution: '%s'\n", csys.distro);
	csys.distroversion = get_cmd_output("lsb_release", NULL, "-sr", 0);
	message(2, "fetch_meta: Version of the distribution: '%s'\n", csys.distroversion);
	//get opengl info: http://www.google.com/codesearch?hl=en&q=%22opengl+driver:%22+show:77mw5SBNRM4:O0Uft0mLv2I:s-ppE9AAn3Y&sa=N&cd=1&ct=rc&cs_p=https://developer.nanohub.org&cs_f=projects/rappture/browser/trunk/gui/vizservers/nanovis/Nv.cpp%3Frev%3D580%26format%3Dtxt
	if (test_gl) {
		csys.graphicdriver = strdup((const char*)glGetString(GL_RENDERER));
		message(2, "fetch_meta: Graphic driver: '%s'\n", csys.graphicdriver);
		csys.graphiccard = strdup((const char*)glGetString(GL_VENDOR));
		message(2, "fetch_meta: Graphic card: '%s'\n", csys.graphiccard);
		csys.glversion = strdup((const char*)glGetString(GL_VERSION));
		message(2, "fetch_meta: GL Version: '%s'\n", csys.glversion);
		csys.glextensions = strdup((const char*)glGetString(GL_EXTENSIONS));
		message(2, "fetch_meta: GL Extensions: '%s'\n", csys.glextensions);
	}
	csys.displayvar = getenv("DISPLAY");
	message(2, "fetch_meta: Display variable: '%s'\n", csys.displayvar);
}

/*
Warn about everything osys.glextensions has, but not csys.glextensions*/
void compare_glext(void) {
	if (osys.glextensions == NULL) {
		message(1, "compare_glext: Warning: No informations about gl extensions \
found in the source file\n");
		return;
	}
	if (csys.glextensions == NULL) {
		message(1, "compare_glext: Warning: It was not possible to determine\
the gl extensions on the current system\n");
		return;
	}
	const char * delim = " ";
	//chunc csys.glextensions and put into a Tire
	char * temp = strdup(csys.glextensions);
	Trie * t = trie_new();
	char * token = strtok(temp, delim);
	int foo = 42; //has to be anything not equal to TRIE_NULL
	TrieValue val = &foo;
	while (token) {
		trie_insert(t, token, val);
		token = strtok(NULL, delim);
	}
	//now warn about everything which is not found
	temp = strdup(osys.glextensions);
	token = strtok(temp, delim);
	char * notthere = NULL;
	while (token) {
		if (trie_lookup(t, token) == TRIE_NULL) {
			char * a = merge(token, " ");
			char * b;
			if (notthere != NULL) {
				b = merge(notthere, a);
			} else
				b = strdup(a);
			free(a);
			free(notthere);
			notthere = b;
		}
		token = strtok(NULL, delim);
	}
	if (notthere != NULL) {
		message(1, "compare_glext: Warning: The following gl extensions are\
 unsupported: '%s'\n", notthere);
		free(notthere);
	} else
		message(1, "compare_glext: Info: All gl extensions are supported\n");
}


int compare_string(const char * v1, const char * v2, const char * name) {
	if ((v1 != NULL) && (v2 != NULL)) {
		if (strcmp(v1, v2) == 0) {
			message(1, "compare_string: Info: %s are equal\n", name);
			return 1;
		} else {
			message(1, "compare_string: Warning: %s differ: '%s' vs '%s'\n", name,
			        v1, v2);
			return 0;
		}
	}
	message(1, "compare_string: Error: %s has at least one value not set\n", name);
	return -1;
}

/* Returns the distance to the beginning of the next digits in the text.
Returns the length of the text if none are found
 */
int nextdigits(const char * text) {
	int max = strlen(text);
	int len;
	for (len = 0; len < max; len++) {
		if ((text[len] < '0') || (text[len] > '9')) {
			break;
		}
	}
	return len;
}

/*
Returns the next number found in the text.
Negative numbers are handled as positive ones.
The text pointer will be updated to the position after the number.
text* = "0123.567\0"
after this the pointer moved to:
text* = ".567\0"
and after the second call:
text* = "\0"
If no next number is found, -1 is returned and the pointer stays the same
*/
int nextnumber(char ** text) {
	int value;
	int digits = 0;
	while((**text) != '\0') {
		digits = nextdigits(*text);
		if (digits > 0)
			break;
		(*text)++;
	}
	if (digits > 0) { //*text is still before the \0 ending
		char * temp = strndup(*text, digits);
		(*text) += digits; //may not go past the \0 ending
		value = atoi(temp);
		free(temp);
		return value;
	}
	return -1;
}

/*
This function works heuristic
Returns: the dest is:
0: same,
-1 slight older, -2 older, -3 much older, -4 much much older
1: slight younger, 2: younger, 3: much younger, 4: much much younger
-100: Comparison failed
100: It is guessed that the versions are equal.
some random version examples taken from /var/lib/dpkg/status
1.4.0-7
0.17-1
2.0.10-3+b1
1:4.2.1-15
4:3.5.5-1
2.6.24-6~etchnhalf.5

5.6+20071124-1ubuntu2
1.3 Mesa 7.0.3-rc2
Method: seek for delimiters: everything which is not a number.
Start with seeking on the first:
decrease importance on every delimiter.
*/

int compare_versions(char * source, char * dest) {
	if ((source == NULL) || (dest == NULL)) {
		return -100;
	}
	if (strcmp(source, dest) == 0) { //exact match
		return 0;
	}
	char * st;
	char * dt;
	char * s = strchr(source, ':');
	char * d = strchr(dest, ':');
	if (s != NULL) {
		st = s;
	} else
		st = source;
	if (d != NULL) {
		dt = d;
	} else
		dt = dest;
	int depth = 0;
	int sv, dv;
	do {
		depth++;
		sv = nextnumber(&st);
		dv = nextnumber(&dt);
	} while((sv == dv) && (sv >= 0) && (dv >= 0) && (depth < 100));
	//assemble the result together
	if (sv == dv) { //versions look equal
		return 100;
	}
	//set strength of mismatch
	int value = -100; //error, not expected to happen
	if (depth == 1) value = 4; //much much
	if (depth == 2) value = 3; //much
	if (depth == 3) value = 2; //somewhat
	if (depth > 3) value = 1; //slightly
	//set direction of mismatch
	if (sv > dv) { //dest is older (mostly the worser situation)
		if ((value < 50) && (value > -50)) { //avoid accidentally move -100 to 100
			value *= -1;
		} else
			message(1, "compare_versions: Error: Mysterious programming bug\n");
	}
	return value;
}

/*
The input comes from compare_versions()
0: Not serious -> info
1: Somewhat -> Warning
2: Problematic -> Error
Its guaranteed, that no other numbers are returned.
*/
int level_of_mismatch(int value) {
	if (value == 100) return 0;
	if ((value < 2) && (value > -1)) return 0;
	if ((value < 3) && (value > -2)) return 1;
	return 2;
}

/* Compares two version information and prints out a message how different they
are. The strength of the difference is the return value
*/
int compare_stringversions(char * source, char * dest,
                           const char * name, int reportlevel) {
	int res = compare_versions(source, dest);
	int level = level_of_mismatch(res);
	//direction
	char * direct;
	if (res > 0) direct = "newer";
	if (res < 0) direct = "older";
	if (res == 100) direct = "equal";
	if (res == 0) direct = "equal";
	if (res == -100) direct = "not equal";
	//description
	char * descr;
	int tmp = res;
	if (tmp < 0) tmp *= -1;
	switch(tmp) {
		case 0: descr = ""; break;
		case 1: descr = " slightly";break;
		case 2: descr = ""; break;
		case 3: descr = " much"; break;
		case 4: descr = " very much"; break;
		default : descr = "";
	}
	//on normal or only on verbose output?
	int messlevel;
	if (level >= reportlevel) {
		messlevel = 1;
	} else {
		messlevel = 2;
	}
	//now print it
	/*the Info, Warning, Error, cant be used as single string, because coloring
		of the output would not work if they are given as parameter */
	if (res != 0) {
		char * basestring; //be careful with %s and parameters!!!
		switch (level) {
			case 0: basestring = "compare_stringversions: Info: The versions of\
 '%s' are%s %s. '%s' vs '%s'\n"; break;
			case 1: basestring = "compare_stringversions: Warning: The versions of\
 '%s' are%s %s. '%s' vs '%s'\n"; break;
			default: basestring = "compare_stringversions: Error: The versions of\
 '%s' are%s %s. '%s' vs '%s'\n";
		}
		message(messlevel, basestring, name, descr, direct, dest, source);
	} else {
		char * basestring; //be careful with %s and parameters!!!
		switch (level) {
			case 0: basestring = "compare_stringversions: Info: The versions of\
 '%s' are%s %s\n"; break;
			case 1: basestring = "compare_stringversions: Warning: The versions of\
 '%s' are%s %s\n"; break;
			default: basestring = "compare_stringversions: Error: The versions of\
 '%s' are%s %s\n";
		}
		message(messlevel, basestring, name, descr, direct);
	}
	return res;
}

/*
Checks for the package name and version number if they are available on the
current system */
void package_found(xmlNodePtr child) {
	message(4, "package_found: entering\n");
	char * pname = xml_get_attribute(child, "name");
	pstore_t * pack = package_get(pname);
	if (pname != NULL) {
		//now get version from XML
		char * xversion = xml_get_content(child, "version");
		if (xversion != NULL) {
			//Get version from database
			if (pack != NULL) {
				if (pack->version != NULL) {
					compare_stringversions(xversion, pack->version, pname, 1);
				} else {
					message(1, "compare_packages: Warning: '%s' has no version\
 information stored in the dpkg database\n", pname);
				}
			}
			free(xversion);
		} else {
			message(1, "compare_packages: Warning: Package '%s' has no version \
information stored in the XML file.\n", pname);
		}
		if (pack != NULL) {
			if (pack->installed != 1) {
				message(1, "compare_packages: Warning: Package '%s' is not installed \
right now.\n", pname);
			}
		} else {
			message(1, "compare_packages: Error: Needed package '%s' does not\
 exists on the current system\n", pname);
		}
	}
	free(pname);
	message(4, "package_found: leaving\n");
}

/*
Initiates reading the states of all packages and compares them with the
dependencies from the XML file */
void compare_packages(void) {
	char * dbfile = "/var/lib/dpkg/status";
	if (access(dbfile, R_OK) != 0) {
		message(1, "compare_packages: Warning: Package DB is not accessible\n");
		return;
	}
	if (packagesp != NULL) {
		package_states_initmemory();
		package_states_read(dbfile);
		//now go through each package in the xmlNodePtr packagesp
		xml_child_handler(packagesp, "package", package_found);
	} else {
		message(1, "compare_packages: No package information found in XML file\n");
	}
}

/*
Checks if the device (stored in child) is available on the current system. */
void device_found(xmlNodePtr child) {
	char * pname = xml_get_attribute(child, "name");
	if (pname != NULL) {
		//check if the device exists
		if (access(pname, F_OK)) {
			message(1, "device_found: Warning: The device '%s', used by the archived\
 application, is not avariable\n", pname);
		} else
			message(2, "device_found: The device '%s' is available\n", pname);
	}
	free(pname);
}

/*
Calls device_found for each device named in the XML file */
void compare_devices(void) {
	if (devicesp != NULL) {
		//now go through each package in the xmlNodePtr devicesp
		xml_child_handler(devicesp, "device", device_found);
	} else {
		message(1, "compare_packages: Warning: No device information found in\
 XML file\n");
	}
}

/*
Tries to compare the current system against the information stored in the XML
file */
void comparesys(void) {
	/*the login name is not needed for comparison
	(and is compared by the homedir anyway) */
	compare_string(csys.homedir, osys.homedir, "Homefolders");
	if (compare_string(csys.distro, osys.distro, "Distros") == 1) {
		compare_string(csys.distroversion, osys.distroversion, "Distro versions");
	}
	if (test_gl) {
		compare_string(csys.graphicdriver, osys.graphicdriver, "Graphic drivers");
		compare_string(csys.graphiccard, osys.graphiccard, "Graphic cards");
		compare_stringversions(osys.glversion, csys.glversion, "GL", 0);
		compare_glext();
	}
	compare_stringversions(osys.kernelver, csys.kernelver, "the Kernel", 0);
//TODO: displayvar
	compare_packages();
	compare_devices();
}
