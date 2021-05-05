#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>

#include "log.h"
#include "fs.h"

#include "cJSON.h"
#define PAR_EASYCURL_IMPLEMENTATION
#include "par_easycurl.h"


const char* ghLink = "https://github.com/";
const char* ghApiLink = "https://api.github.com/repos/";
const char* desktopFilesLocal = ".local/share/applications";
const char* iconsFolder = "shovel/icons";
const char* desktopFilesShared = "/usr/share/applications";
char* home;

void fixText(char toRemove,char* from){
    int len = strlen(from);
    char* out = (char*)malloc(sizeof(char) * len);
    memset(out,0,len);
    int count = 0;
    for(int i = 0; i < len; ++i){
        if(toRemove != from[i]){
            out[count] = from[i];
            count++;
        }
    }
    out[count++] = 0;
    memcpy(from,out,count);
    free(out);
}

int strCmpi (char *haystack,char *needle)
{
    for(int i=0; haystack[i]!='\0'; ++i)
    {
        if( toupper(haystack[i])!=toupper(needle[i]) )
            return 1;
    }
    return 0;
}

void listInstalled(){
    char manifestsPath[PATHNAME_MAX] = {0};
    strcat(manifestsPath,home);
    strcat(manifestsPath,"/");
    strcat(manifestsPath,"shovel");
    strcat(manifestsPath,"/");
    strcat(manifestsPath,"manifests");

    char* names[256] = {0};
    for(int i = 0; i < 256;++i){names[i] = (char*)malloc(sizeof(char) *PATHNAME_MAX);}

    uint32_t dirsize = readDir(manifestsPath,false,names);
    if(dirsize > 0)
        kinc_log(KINC_LOG_LEVEL_INFO,"Installed Apps:\n");
    else
        kinc_log(KINC_LOG_LEVEL_INFO,"No Apps installed.");
    char txt[4096] = {0};
    for(uint32_t i = 0 ; i < dirsize;++i){
        char path[PATHNAME_MAX] = {0};

        strcat(path,manifestsPath);
        strcat(path,"/");
        strcat(path,names[i]);
        strcat(path,".json");

        FILE* srcFile = fopen(path,"rb");
        assert(srcFile != NULL);
        fseek(srcFile, 0, SEEK_END);
        size_t size = ftell(srcFile);
        fseek(srcFile, 0, SEEK_SET);

        memset(txt,0, strlen(txt));
        fread(txt, 1, size, srcFile);
        txt[size] = 0;

        cJSON* manifest = cJSON_Parse(txt);
        char* version = cJSON_GetStringValue(cJSON_GetObjectItem(manifest,"version"));

        kinc_log(KINC_LOG_LEVEL_INFO,"%s (%s)\n",names[i],version);
    }

    for(int i = 0; i < 256;++i){free(names[i]);}
}

cJSON* searchFullname(const char* name,cJSON* apps){
    int len  = cJSON_GetArraySize(apps);
    int count = 0;
    while(count < len){
        cJSON* data = cJSON_GetArrayItem(apps,count);
        cJSON* out = cJSON_GetObjectItem(data,"name");
        char* curAppName = cJSON_Print(out);
        fixText('\"',curAppName);
        if(strcmp(curAppName, name) == 0 || strCmpi(curAppName,name) == 0){
            return data;
        }
        count++;
    }
    return NULL;
}

int search(char* subname, cJSON* apps, cJSON** out_apps ){
    int len  = cJSON_GetArraySize(apps);
    int i = 0;
    int count = 0;
    char otherName[64] = {0};
    memcpy(otherName,subname,strlen(subname)+1);
    if(isupper(subname[0])){
        otherName[0] = tolower(otherName[0]);
    }
    else {
        otherName[0] = toupper(otherName[0]);
    }
    while(i < len){
        cJSON* data = cJSON_GetArrayItem(apps,i);
        cJSON* out = cJSON_GetObjectItem(data,"name");
        char* curAppName = cJSON_Print(out);
        fixText('\"',curAppName);
        if(strstr(curAppName, subname) != NULL || strstr(curAppName, otherName) != NULL){
            out_apps[count] = data;
            count++;
        }
        ++i;
    }
    return count;
}

void printAppNameVersion(cJSON* app, bool isInstalled){
    if(isInstalled){

    }
    else {
        cJSON* links = cJSON_GetObjectItem(app,"links");
        if(cJSON_IsNull(links)) return;
        int len = cJSON_GetArraySize(links);
        char* url;
        for(int i =0; i < len; ++i){
            cJSON* item = cJSON_GetArrayItem(links,i);
            char* type = cJSON_Print(cJSON_GetObjectItem(item,"type"));
            fixText('\"',type);
            if(strcmp(type,"Download") == 0){
                url = cJSON_Print(cJSON_GetObjectItem(item,"url"));
                fixText('\"',url);
            }
        }

        char* substr = strstr(url,ghLink) + strlen(ghLink);
        char lastUrl[256] = {0};
        strcat(lastUrl,ghApiLink);
        strcat(lastUrl,substr);
        strcat(lastUrl,"/latest");
        assert(strlen(lastUrl) != 256);
        int temp = 4096;
        par_byte* t_data = (par_byte*)malloc(sizeof(par_byte)*temp);
        int success = par_easycurl_to_memory(lastUrl,&t_data,&temp);
        char* version;
        cJSON* urlData = NULL;
        if(success){
            urlData = cJSON_Parse(t_data);
            version = cJSON_Print(cJSON_GetObjectItem(urlData,"tag_name"));
            fixText('\"',version);
        }
        else {
            kinc_log(KINC_LOG_LEVEL_ERROR,"The download link isn't available: %s\n",url);
            return;
        }
        char* name = cJSON_Print(cJSON_GetObjectItem(app,"name"));
        fixText('\"',name);

        printf("%s (%s)\n",name,version);
        cJSON_Delete(urlData);
        free(t_data);

    }
}

void printInfo(cJSON* app, bool isInstalled, char* manifestPath){
    cJSON* links = cJSON_GetObjectItem(app,"links");
    if(cJSON_IsNull(links)) return;
    int len = cJSON_GetArraySize(links);
    char* url;
    char website[256] = {0};
    memcpy(website,ghLink,strlen(ghLink)+1);

    for(int i =0; i < len; ++i){
        cJSON* item = cJSON_GetArrayItem(links,i);
        char* type = cJSON_Print(cJSON_GetObjectItem(item,"type"));
        fixText('\"',type);
        if(strcmp(type,"Download") == 0){
            url = cJSON_Print(cJSON_GetObjectItem(item,"url"));
            fixText('\"',url);
        }
        else if(strcmp(type,"GitHub") == 0){
            char* t_url = cJSON_Print(cJSON_GetObjectItem(item,"url"));
            fixText('\"',t_url);
            strcat(website,t_url);
        }
    }

    char* substr = strstr(url,ghLink) + strlen(ghLink);
    char lastUrl[256] = {0};
    strcat(lastUrl,ghApiLink);
    strcat(lastUrl,substr);
    strcat(lastUrl,"/latest");
    assert(strlen(lastUrl) != 256);
    int temp = 4096;
    par_byte* t_data = (par_byte*)malloc(sizeof(par_byte)*temp);
    int success = par_easycurl_to_memory(lastUrl,&t_data,&temp);
    char* version;
    cJSON* urlData = NULL;
    char* name = cJSON_Print(cJSON_GetObjectItem(app,"name"));
    fixText('\"',name);
    if(success){
        urlData = cJSON_Parse(t_data);
        version = cJSON_Print(cJSON_GetObjectItem(urlData,"tag_name"));
        fixText('\"',version);
    }
    else {
        kinc_log(KINC_LOG_LEVEL_ERROR,"The download link isn't available and the application %s can't be installed, ergo we won't show the app info for: %s\n",name,url);
        return;
    }

    char* desc = "";
    cJSON* d = cJSON_GetObjectItem(app,"description");
    if(!cJSON_IsNull(d)){
        desc = cJSON_Print(d);
        fixText('\"',desc);
    }

    char* license = "None";
    cJSON* l = cJSON_GetObjectItem(app,"license");
    if(!cJSON_IsNull(l)){
        license = cJSON_Print(l);
        fixText('\"',license);
    }

    printf("Name: %s\n",name);
    printf("Description: %s\n",desc);
    printf("Version: %s\n",version);
    printf("Project Link: %s\n",website);
    printf("License: %s\n",license);
    printf("Installed:");
    if(isInstalled){
        FILE* srcFile = fopen(manifestPath,"rb");
        assert(srcFile != NULL);
        fseek(srcFile, 0, SEEK_END);
        size_t size = ftell(srcFile);
        fseek(srcFile, 0, SEEK_SET);

        char *txt = (char *)malloc(size + 1);
        assert(txt != NULL);
        fread(txt, 1, size, srcFile);
        txt[size] = 0;


        cJSON* manifest = cJSON_Parse(txt);
        cJSON* obj  = cJSON_GetObjectItem(manifest,"desktopFile");

        fclose(srcFile);
        free(txt);

        char* desktopPath = cJSON_GetStringValue(obj);
        srcFile = fopen(desktopPath,"rb");
        char content[256] = {0};
        while(fgets(content,sizeof(content),srcFile)){

          char* pos = strstr(content,"Exec=");
          if(pos != NULL)
          {
            pos = strstr(content,"=");
            strcpy(content,pos+1);
            break;
          }
        }
        fclose(srcFile);
        printf("\n%s",content);
        cJSON_Delete(manifest);
    }
    else {
        printf(" No\n");
    }

    cJSON_Delete(urlData);
    free(t_data);
}

void install(cJSON* app){
    cJSON* links = cJSON_GetObjectItem(app,"links");
    char* name = cJSON_Print(cJSON_GetObjectItem(app,"name"));
    fixText('\"',name);
    if(cJSON_IsNull(links)){
        kinc_log(KINC_LOG_LEVEL_ERROR,"App can't be installed, app manifest for $s has no links, file an issue at https://github.com/AppImage/appimage.github.io/issues",name);
        return;
    }
    int len = cJSON_GetArraySize(links);
    char* url;
    for(int i =0; i < len; ++i){
        cJSON* item = cJSON_GetArrayItem(links,i);
        char* type = cJSON_Print(cJSON_GetObjectItem(item,"type"));
        fixText('\"',type);
        if(strcmp(type,"Download") == 0){
            url = cJSON_Print(cJSON_GetObjectItem(item,"url"));
            fixText('\"',url);
        }
    }

    char* substr = strstr(url,ghLink) + strlen(ghLink);
    char lastUrl[256] = {0};
    strcat(lastUrl,ghApiLink);
    strcat(lastUrl,substr);
    strcat(lastUrl,"/latest");
    assert(strlen(lastUrl) != 256);
    int temp = 4096;
    par_byte* t_data = (par_byte*)malloc(sizeof(par_byte)*temp);
    int success = par_easycurl_to_memory(lastUrl,&t_data,&temp);
    char* version;
    char* dlLink;
    cJSON* urlData = NULL;
    if(success){
        urlData = cJSON_Parse(t_data);
        version = cJSON_Print(cJSON_GetObjectItem(urlData,"tag_name"));
        fixText('\"',version);

        cJSON* assets = cJSON_GetObjectItem(urlData,"assets");
        int len = cJSON_GetArraySize(assets);
        for(int i =0; i < len; ++i){
            cJSON* item = cJSON_GetArrayItem(assets,i);
            char* type = cJSON_Print(cJSON_GetObjectItem(item,"content_type"));
            fixText('\"',type);
            if(strcmp(type,"application/x-iso9660-appimage") == 0){
                dlLink = cJSON_Print(cJSON_GetObjectItem(item,"browser_download_url"));
                fixText('\"',dlLink);
            }
        }

    }
    else {
        kinc_log(KINC_LOG_LEVEL_ERROR,"The download link isn't available: %s\n",url);
        return;
    }

    char path[PATHNAME_MAX] = {0};
    strcat(path,home);
    strcat(path,"/");
    strcat(path,"shovel");
    strcat(path,"/");
    strcat(path,"apps");
    strcat(path,"/");
    strcat(path,name);
    strcat(path,"/");
    strcat(path,version);
    int mkSuccess = recursiveMkdir(path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    char iconsPath[PATHNAME_MAX] = {0};
    strcat(iconsPath,home);
    strcat(iconsPath,"/");
    strcat(iconsPath,iconsFolder);
    if(!directoryExists(iconsPath)){
        recursiveMkdir(iconsPath,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

    success = par_easycurl_to_memory(dlLink,&t_data,&temp);
    char fname[PATHNAME_MAX] = {0};
    int found = getFilename(dlLink,fname);

    if(found && success && mkSuccess == 0){
        cJSON* manifest = cJSON_CreateObject();
        FILE* f = fopen(fname,"wb");
        size_t size = fwrite(t_data,sizeof(par_byte),temp,f);
        assert(size == temp);
        fclose(f);
        if(chmod(fname, S_IRWXU)){
            kinc_log(KINC_LOG_LEVEL_ERROR,"Couldn't set file as executable for %s, manually call chmod +x on the file",fname);
        }

        char cwd[PATHNAME_MAX] = {0};
        getcwd(cwd,sizeof(char) * PATHNAME_MAX);
        strcat(cwd,"/");
        strcat(cwd,fname);
        strcat(cwd, " ");
        strcat(cwd,"--appimage-extract >/dev/null");
        int succeded = system(cwd);

        char* names[256] = {0};
        for(int i = 0; i < 256;++i){names[i] = (char*)malloc(sizeof(char) *PATHNAME_MAX);}

        uint32_t dirsize = readDir("squashfs-root",true,names);
        char desktop[PATHNAME_MAX] = {0};
        char image[PATHNAME_MAX] = {0};
        for(int i = 0; i < dirsize; ++i){
            if(strstr(names[i],".png") != NULL){
                strcat(image,"squashfs-root");
                strcat(image,"/");
                strcat(image,names[i]);
                copyFile(image,names[i]);
                strcpy(image,names[i]);
            }
            if(strstr(names[i],".desktop") != NULL){
                strcat(desktop,"squashfs-root");
                strcat(desktop,"/");
                strcat(desktop,names[i]);
                copyFile(desktop,names[i]);
                strcpy(desktop,names[i]);
            }
        }

        strcat(path,"/");
        strcat(path,fname);
        rename(fname,path);

        strcat(iconsPath,"/");
        strcat(iconsPath,image);

        if(rename(image,iconsPath) < 0){
            kinc_log(KINC_LOG_LEVEL_ERROR,"Couldn't move %s, removing it.",image);
            remove(image);
        }

        char content[256] = {0};
        FILE* dfile = fopen(desktop,"rb");
        FILE *output = fopen("temp.txt", "wb");
        while(fgets(content,sizeof(content),dfile)){

          char* pos = strstr(content,"Exec=");
          char* iconpos = strstr(content,"Icon=");
          if(pos != NULL)
          {
            //@TODO: Instead of removing the custom args we should only add the path to the executable and keep the optional args
            // as this will replace for TryExec and the Exec of actions.
            char newline[256] = {0};
            strcat(newline,"Exec=");
            strcat(newline,path);
            strcat(newline,"\n");
            fputs(newline,output);
          }
          else if(iconpos != NULL){
            char newline[256] = {0};
            strcat(newline,"Icon=");
            strcat(newline,iconsPath);
            strcat(newline,"\n");
            fputs(newline,output);
          }
          else {
            fputs(content,output);
          }
        }
        fclose(dfile);
        fclose(output);

        rename("temp.txt",desktop);

        memset(path,0, strlen(path));
        strcat(path,home);
        strcat(path,"/");
        strcat(path,desktopFilesLocal);
        strcat(path,"/");
        strcat(path,desktop);

        if(rename(desktop,path) < 0){
            kinc_log(KINC_LOG_LEVEL_ERROR,"Couldn't move %s, removing it.",desktop);
            remove(desktop);
        }

        cJSON_AddStringToObject(manifest,"desktopFile",path);
        cJSON_AddStringToObject(manifest,"version",version);

        memset(path,0, strlen(path));
        getcwd(path,sizeof(char) * PATHNAME_MAX);
        strcat(path,"/");
        strcat(path,"squashfs-root");
        recursiveRmDir(path);

        memset(path,0, strlen(path));
        strcat(path,home);
        strcat(path,"/");
        strcat(path,"shovel");
        strcat(path,"/");
        strcat(path,"manifests");
        if(!directoryExists(path))
            recursiveMkdir(path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        strcat(path,"/");
        strcat(path,name);
        strcat(path,".json");
        FILE* man = fopen(path,"wb");
        fprintf(man,"%s",cJSON_Print(manifest));
        fclose(man);

        cJSON_free(manifest);
        for(int i = 0; i < 256;++i){ free(names[i]);}
    }


    printf("%s (%s)\n",name,version);
    cJSON_Delete(urlData);
    free(t_data);
}
void uninstall(char* manifestPath,char* name){
    FILE* srcFile = fopen(manifestPath,"rb");
    assert(srcFile != NULL);
    fseek(srcFile, 0, SEEK_END);
    size_t size = ftell(srcFile);
    fseek(srcFile, 0, SEEK_SET);

    char *txt = (char *)malloc(size + 1);
    assert(txt != NULL);
    fread(txt, 1, size, srcFile);
    txt[size] = 0;


    cJSON* manifest = cJSON_Parse(txt);
    cJSON* obj  = cJSON_GetObjectItem(manifest,"desktopFile");

    fclose(srcFile);
    free(txt);

    char* desktopPath = cJSON_GetStringValue(obj);
    srcFile = fopen(desktopPath,"rb");
    char content[256] = {0};
    while(fgets(content,sizeof(content),srcFile)){

      char* pos = strstr(content,"Icon=");
      if(pos != NULL)
      {
        pos = strstr(content,"=");
        strcpy(content,pos+1);
        content[strlen(content) - 1] = 0;
        rmFile(content);
      }
    }
    rmFile(desktopPath);

    fclose(srcFile);
    cJSON_Delete(manifest);

    rmFile(manifestPath);

    char* path[PATHNAME_MAX] = {0};
    strcat(path,home);
    strcat(path,"/");
    strcat(path,"shovel");
    strcat(path,"/");
    strcat(path,"apps");
    strcat(path,"/");
    strcat(path,name);

    recursiveRmDir(path);
}

void update(char* manifestPath, cJSON* app){
    if(app != NULL){
        FILE* srcFile = fopen(manifestPath,"rb");
        assert(srcFile != NULL);
        fseek(srcFile, 0, SEEK_END);
        size_t size = ftell(srcFile);
        fseek(srcFile, 0, SEEK_SET);

        char *txt = (char *)malloc(size + 1);
        assert(txt != NULL);
        fread(txt, 1, size, srcFile);
        txt[size] = 0;


        cJSON* manifest = cJSON_Parse(txt);
        char* installedVersion = cJSON_GetStringValue(cJSON_GetObjectItem(manifest,"version"));
        cJSON* links = cJSON_GetObjectItem(app,"links");
        char* name = cJSON_Print(cJSON_GetObjectItem(app,"name"));
        fixText('\"',name);

        int len = cJSON_GetArraySize(links);
        char* url;
        for(int i =0; i < len; ++i){
            cJSON* item = cJSON_GetArrayItem(links,i);
            char* type = cJSON_Print(cJSON_GetObjectItem(item,"type"));
            fixText('\"',type);
            if(strcmp(type,"Download") == 0){
                url = cJSON_Print(cJSON_GetObjectItem(item,"url"));
                fixText('\"',url);
            }
        }

        char* substr = strstr(url,ghLink) + strlen(ghLink);
        char lastUrl[256] = {0};
        strcat(lastUrl,ghApiLink);
        strcat(lastUrl,substr);
        strcat(lastUrl,"/latest");
        assert(strlen(lastUrl) != 256);
        int temp = 4096;
        par_byte* t_data = (par_byte*)malloc(sizeof(par_byte)*temp);
        int success = par_easycurl_to_memory(lastUrl,&t_data,&temp);
        char* version = NULL;
        cJSON* urlData = NULL;
        if(success){
            urlData = cJSON_Parse(t_data);
            version = cJSON_Print(cJSON_GetObjectItem(urlData,"tag_name"));
            fixText('\"',version);
        }
        else{
            kinc_log(KINC_LOG_LEVEL_WARNING,"While trying to check the project current version, we couldn't reach %s, is it down ?",lastUrl);
        }

        if(version != NULL && strcmp(version, installedVersion) != 0 ){
            kinc_log(KINC_LOG_LEVEL_INFO,"Updating %s from %s to %s\n",name,installedVersion,version);
            install(app);
        }
    }
    else{
        kinc_log(KINC_LOG_LEVEL_WARNING,"App isn't available on the appimage hub anymore, and can't be updated :(");
    }
}
/**
    available actions are:
        - install
        - info
        - search
        - list
        - uninstall/rm
        - update
**/
int main(int argc, char** argv) {

    home = getenv("HOME");
    par_easycurl_init(0);

    FILE *srcFile;

	srcFile = fopen("feed.json", "rb");
	if(srcFile == NULL){
        int len = 4096;
        par_byte* data = (par_byte*)malloc(sizeof(par_byte)*len);
        int success = par_easycurl_to_memory("https://appimage.github.io/feed.json",&data,&len);
        if(!success){
            kinc_log(KINC_LOG_LEVEL_ERROR,"Could not download the feed.json file and it couldn't be found on your system");
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
        free(data);
	}
	assert(srcFile != NULL);
	fseek(srcFile, 0, SEEK_END);
	size_t size = ftell(srcFile);
	fseek(srcFile, 0, SEEK_SET);

	char *txt = (char *)malloc(size + 1);
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
	if(argc > 2){
        char manifestPath[PATHNAME_MAX] = {0};
        strcat(manifestPath,home);
        strcat(manifestPath,"/");
        strcat(manifestPath,"shovel");
        strcat(manifestPath,"/");
        strcat(manifestPath,"manifests");
        strcat(manifestPath,"/");
        strcat(manifestPath,argv[2]);
        strcat(manifestPath,".json");
        if(strcmp(argv[1],"install") == 0){
            cJSON* app = searchFullname(argv[2],apps);
            bool installed = fileExists(manifestPath);
            if(app != NULL && !installed){
                install(app);
            }
            else if(installed){
                kinc_log(KINC_LOG_LEVEL_INFO,"App %s already installed, to update do `shovel update %s`",argv[2],argv[2]);
            }
            else {
                goto EXIT;
            }
        }
        else if(strcmp(argv[1],"uninstall") == 0 || strcmp(argv[1],"rm") == 0){
            if(fileExists(manifestPath)){
                uninstall(manifestPath,argv[2]);
            }
            else {
                goto NOTINSTALLED;
            }
        }
        else if(strcmp(argv[1],"update") == 0){
            cJSON* app = searchFullname(argv[2],apps);
            bool installed = fileExists(manifestPath);
            if(installed){
                update(manifestPath,app);
            }
            else {
                goto NOTINSTALLED;
            }
        }
        else if(strcmp(argv[1],"search") == 0){
            cJSON* app[256] = {NULL};
            int amount = search(argv[2],apps,app);
            if(app[0] != NULL){
                for(int i =0; i < amount; ++i){
                    printAppNameVersion(app[i],false);
                }

            }
            else {
                printf("App %s isn't on the AppImage Hub",argv[2]);
            }
        }
        else if(strcmp(argv[1],"info") == 0){
            cJSON* app = searchFullname(argv[2],apps);
            if(app != NULL){

                printInfo(app,fileExists(manifestPath),manifestPath);
            }
            else {
                goto EXIT;
            }
        }

	}
	else {
        if(strcmp(argv[1],"list") == 0){
            listInstalled();
        }
        else {
            //printHelp();
        }
	}

    free(txt);
	cJSON_Delete(parsed);

    if(0){ EXIT: kinc_log(KINC_LOG_LEVEL_WARNING,"App %s isn't on the AppImage Hub\n",argv[2]);}
    if(0){ NOTINSTALLED: kinc_log(KINC_LOG_LEVEL_WARNING,"App %s isn't installed\n",argv[2]);}
	return 0;
}
