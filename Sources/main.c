#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>

#define PAR_EASYCURL_IMPLEMENTATION
#include "par_easycurl.h"

#include "cJSON.h"

void list(cJSON* apps){
    int len  = cJSON_GetArraySize(apps);
    int count = 0;
    while(count < len){
        cJSON* data = cJSON_GetArrayItem(apps,count);
        cJSON t_data = *data;
        if(cJSON_IsObject(data)){
            cJSON* out = cJSON_GetObjectItem(data,"name");
            printf("%s\n",cJSON_Print(out));
        }
        count++;
    }
}
/**
    available actions are:
        - install
        - info
        - list
        - uninstall/rm
        - update
**/
int main(int argc, char** argv) {

    par_easycurl_init(0);

    FILE *srcFile;

	srcFile = fopen("feed.json", "rb");
	if(srcFile == NULL){
        int len = 4096;
        par_byte* data = (par_byte*)alloca(sizeof(par_byte)*len);
        int success = par_easycurl_to_memory("https://appimage.github.io/feed.json",&data,&len);
        if(!success){
            printf("Could not download the feed.json file and it couldn't be found on your system");
        }
        else {
            srcFile = fopen("feed.json", "wb");
            cJSON_Minify(data);
            size_t txt_len = strlen(data);
            size_t size = fwrite(data,sizeof(par_byte),txt_len,srcFile);
            assert(size == txt_len);
            fclose(srcFile);
            srcFile = fopen("feed.json", "rb");
        }
	}
	assert(srcFile != NULL);
	fseek(srcFile, 0, SEEK_END);
	size_t size = ftell(srcFile);
	fseek(srcFile, 0, SEEK_SET);

	char *txt = (char *)alloca(size + 1);
	assert(txt != NULL);
	fread(txt, 1, size, srcFile);
	txt[size] = 0;

    fclose(srcFile);
    cJSON_Hooks hooks = {0};
    hooks.malloc_fn = malloc;
    hooks.free_fn = free;
    cJSON_InitHooks(&hooks);

	cJSON* parsed = cJSON_Parse(txt);
	cJSON* apps = cJSON_GetObjectItem(parsed,"items");
	if(argc > 1){
        if(strcmp(argv[1],"list") == 0){
            list(apps);
        }
	}

	return 0;
}
