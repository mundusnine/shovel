#pragma once

#include <sys/types.h>
#include <stdbool.h>

#define PATHNAME_MAX 256
#define uint32_t u_int32_t

bool directoryExists(const char* path);
bool fileExists(char* path);
int getExtension(char* filename,char* extension);
int getFilename(char* path,char* filename);
uint32_t readDir(const char *path, bool with_extensions, char **out);
int recursiveMkdir(char *path, int mode);
/**
    Recursivaly remove the directory and it's contents.
    @param path A path to a directory
    @return  if it succeeded, else errno will contain the error message
**/
bool recursiveRmDir(char *path);
bool rmFile(char *path);
bool copyFile(char *from, char *to);
bool copyDir(char *from, char *to);

