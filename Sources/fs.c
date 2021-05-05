#include "fs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#define __USE_XOPEN_EXTENDED
#include <ftw.h>
#include <errno.h>

#include "log.h"

#define PATH_MAX 4096
#define MAX_FILE_COUNT 128




bool directoryExists(const char* path) {
	DIR* dir = opendir(path);
	bool out = dir != NULL;
	if(out)
        closedir(dir);
	return out;
}
bool fileExists(char* path){
    return access( path, F_OK ) == 0;
}

int getExtension(char* filename,char* extension) {
	size_t strLength = strlen(filename);
	for (int i = strLength - 1; i > 0 ; --i) {
		if (filename[i] == '.' && i != (strLength - 1)) {
			strcpy(extension, &filename[i]);
			return 1;
		}
		else if (strLength - i > 8) {
			return 0;
		}
	}
	return 0;
}

int getFilename(char* path,char* filename){
    size_t strLength = strlen(path);
	for (int i = strLength - 1; i > 0 ; --i) {
		if (path[i] == '/' && i != (strLength - 1)) {
			strcpy(filename, &path[i+1]);
			return 1;
		}
	}
	return 0;
}
uint32_t readDir(const char *path, bool with_extensions, char **out) {
	if (!directoryExists(path)) goto EXIT;
	#if _WIN32
	int len = strlen(path) + 4;
	char *fixedPath = (char *)malloc(sizeof(char) * (len));
	assert(fixedPath != NULL);
	strcpy(fixedPath, path);
	strcat(fixedPath, "\\*");
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFileA(fixedPath, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) goto EXIT;
	uint32_t count = 0;
	while (FindNextFileA(hFind, &ffd))
	{
		if (strcmp(ffd.cFileName, "..") == 0) continue;
		char ext[8];
		if (!with_extensions && getExtension(ffd.cFileName,ext)) {
			size_t size = strlen(ffd.cFileName);
			char *endName = out[count];
			strcpy(endName, ffd.cFileName);
			endName[size - strlen(ext)] = 0;
		}
		else {
			strcpy(out[count], ffd.cFileName);
		}
		count++;
	}
	return count;
	#else
	uint32_t count = 0;
	DIR* dir = opendir(path);
	struct dirent* dirinfo = readdir(dir);
	for(; dirinfo != NULL; dirinfo = readdir(dir)){
        if(dirinfo->d_type != DT_DIR){
            strcpy(out[count], dirinfo->d_name);
            if(!with_extensions){
                char ext[8] = {0};
                getExtension(out[count],ext);
                out[count][strlen(out[count])-strlen(ext)] = 0;
            }
            count++;
        }
	}
	return count;
	#endif
EXIT:
	kinc_log(KINC_LOG_LEVEL_ERROR, "The path %s isn't a directory or it couldn't be found", path);
	exit(1);

}
int recursiveMkdir(char *path, int mode) {
	int success = 0;// 0 Is true because mkdir reasons...
	size_t strLen = strlen(path);
	char tpath[PATHNAME_MAX] = {0};
	memcpy(tpath,path,sizeof(char) * strLen);
	char* token = strtok(tpath, "/");
	char* out = (char*)malloc(sizeof(char) * strLen);
	assert(out != NULL);
	memset(out, 0, sizeof(char) * strLen);
	if(path[0] == '/')
        out[0] = '/';
	while (token != NULL && strlen(token) != strLen) {
		if (token != NULL) {
			strcat(out, token);
			if (!directoryExists(out)) {
				success = mkdir(out, mode);
				if (success != 0) {
					kinc_log(KINC_LOG_LEVEL_ERROR, "Path %s was not found.", out);
					return success;
				}
			}
			strcat(out, "/");
		}
		token = strtok(NULL, "/");

	}
	free(out);
	return success;
}
static int visit(const char* path,const struct stat* s, int flag,struct FTW *__info){
    if(S_ISDIR(s->st_mode)){
        int out = rmdir(path);
        return out;
    }
    else if(S_ISREG(s->st_mode)){
        return unlink(path);
    }
    return 0;
}

bool recursiveRmDir(char *path){

    int out = nftw(path,visit,256,FTW_DEPTH);
    if(out < 0){
        if(errno == EACCES){
            kinc_log(KINC_LOG_LEVEL_ERROR,"Search permission is denied for any component of path or read permission is denied for path: %s",path);
        }
        else if(errno == ENAMETOOLONG){
            kinc_log(KINC_LOG_LEVEL_ERROR,"The length of a pathname exceeds %i characters",PATH_MAX);
        }
        else if(errno == ENOENT){
            kinc_log(KINC_LOG_LEVEL_ERROR,"A component of path does not name an existing file or path is an empty string");
        }
        else if(errno == ENOTDIR){
            kinc_log(KINC_LOG_LEVEL_ERROR,"A component of path names an existing file that is neither a directory nor a symbolic link to a directory.");
        }
        else if (errno == EOVERFLOW){
            kinc_log(KINC_LOG_LEVEL_ERROR,"A field in the stat structure cannot be represented correctly in the current programming environment for one or more files found in the file hierarchy.");
        }
        else if(errno == EMFILE){
            kinc_log(KINC_LOG_LEVEL_ERROR,"All file descriptors available to the process are currently open.");
        }
        else if(errno == ENFILE){
            kinc_log(KINC_LOG_LEVEL_ERROR,"Too many files are currently open in the system.");
        }
        else {
            char* errmsg = strerror(errno);
            kinc_log(KINC_LOG_LEVEL_ERROR,"%s",errmsg);
        }
    }
    return out == 0;
}
bool rmFile(char *path){
    return unlink(path) > -1;
}
bool copyFile(char *from, char *to) {
	FILE *srcFile;
	FILE *destFile;

	srcFile = fopen(from, "rb");
	assert(srcFile != NULL);
	fseek(srcFile, 0, SEEK_END);
	size_t size = ftell(srcFile);
	fseek(srcFile, 0, SEEK_SET);

	char *txt = (char *)malloc(size + 1);
	assert(txt != NULL);
	fread(txt, 1, size, srcFile);
	txt[size] = 0;

	destFile = fopen(to, "wb");
	assert(destFile != NULL);
	size_t writeAmount = 0;
	writeAmount = fwrite(txt, sizeof(char), size, destFile);

	fclose(srcFile);
	fclose(destFile);

	return writeAmount == size;
}
bool copyDir(char *from, char *to) {
	char *out[MAX_FILE_COUNT];
	for (int i = 0; i < MAX_FILE_COUNT; ++i) {
		out[i] = (char *)malloc(sizeof(char) * PATHNAME_MAX);
		assert(out[i] != NULL);
	}
	uint32_t size = readDir(from, true, out);
	bool succeded = true;
	char srcPath[PATHNAME_MAX];
	char destPath[PATHNAME_MAX];
	for (int i = 0; i < size && succeded; ++i) {
		memset(srcPath, 0, PATHNAME_MAX);
		memset(destPath, 0, PATHNAME_MAX);
		strcpy(srcPath, from);
		strcat(srcPath, "/");
		strcat(srcPath, out[i]);

		strcpy(destPath, to);
		strcat(destPath, "/");
		strcat(destPath, out[i]);
		bool isSrcDir = directoryExists(srcPath);

		if (isSrcDir && !directoryExists(destPath)) {
			int success = mkdir(destPath, S_IRWXU);
			if (success != 0) {
				kinc_log(KINC_LOG_LEVEL_ERROR, "Couldn't create folder at path %s", destPath);
				exit(1);
			}
		}

		if (isSrcDir) {
			succeded = copyDir(srcPath, destPath);
		}
		else {
			succeded = copyFile(srcPath, destPath);
		}
	}
	return succeded;
}
