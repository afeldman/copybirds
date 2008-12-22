//(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2

#ifndef TOOLS_H
#define TOOLS_H

#include <string.h>

//enable use if the special terminal characters make problems
//#define NOCOLOR

#define PGNVERSION "0.8.6"


void message(int level, const char * format, ...);
char * merge(const char * str1, const char * str2);
char * nodoubleslash(const char * path);
char * dirmerge(const char * str1, const char * str2);
int path_is_relative(const char * path);
int path_is_absolute(const char * path);
char * path_from_file(const char * file);
void set_chatting(const char * newpgn, int mode);
void * smalloc(size_t size);
char * itoa(int val);
int imin(int a, int b);
int endswith(const char * text, const char * end);
int beginswith(const char * text, const char * begin);
char * get_cmd_output(const char * command, const char * wdir, const char * parameter, int multiline);
char * get_arg_filename(char * arg);
void replace_chars(char * text, char find, char replacement);
char * text_get_between(const char * text, const char * before, const char * after);
int path_exists(const char * path);
int path_is_dir(const char * path);
char * haveslashending_free(char * path);
char * readfile(char * filename);

#endif
