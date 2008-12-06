#ifndef RECURSIVEDIR_H
#define RECURSIVEDIR_H

#define DEPT_CUTOFF 60

//header for abs2rel and rel2abs
char * abs2rel(const char *path, const char *base, char *result, const size_t size);
char * rel2abs(const char *path, const char *base, char *result, const size_t size);


typedef void * (*pathfunc) (const char *, const char *, void * );

void recursive_dir(const char * base, pathfunc handler, void * somedata, int dorecursive);

char * sreadlink(const char * name);

char * truepath(const char * path);

char * reveal_true_abs_link_target(const char * linkname);

#endif
