
#ifndef COPYMETA_H
	#define COPYMETA_H

#define PGN "cb_meta_check"

typedef struct sysinfo {
	char *loginname, *homedir, *kernelver, *distro, *distroversion, *displayvar,
  *graphicdriver, *graphiccard, *glversion, *glextensions;
} sysinfo_t;

extern sysinfo_t csys; //current system
extern sysinfo_t osys; //system readed from the xml file

extern xmlNodePtr devicesp;
extern xmlNodePtr packagesp;
#endif
