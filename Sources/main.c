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

#define SHELL_AMOUNT 5
#define COMMAND_AMOUNT 7
#ifdef __x86_64__
const char* endingOfFilename = "x86_64.AppImage";
#else
const char* endingOfFilename = "i386.AppImage";
#endif
/** @ERRORS
    - downloading Advanced_Rest_Client causes a free of invalid data in openssl...
**/
const char* ghLink = "https://github.com/";
const char* ghApiLink = "https://api.github.com/repos/";
const char* desktopFilesLocal = ".local/share/applications";
const char* iconsFolder = "shovel/icons";
const char* desktopFilesShared = "/usr/share/applications";
const char* exportPath = "export PATH=$PATH:";
const char* supportedTerminals[SHELL_AMOUNT] = {"bash","zsh","dash","fish","rc"};
const char* commands[COMMAND_AMOUNT] = {"install","uninstall or rm","update","info","search","list","help"};
const char* commandsTxt[COMMAND_AMOUNT] = {
                                            "Install an appimage based on the name supplied. i.e. shovel install vscodium",
                                            "Uninstall an appimage based on the name supplied. i.e. shovel rm vscodium",
                                            "Update the appimage based on the name supplied. Use the * wildcard to update all.",
                                            "Give the info related to the appimage if it exists",
                                            "Search for available appimages on the appimage hub. Can be a substring of the name.",
                                            "List the installed appimages on the system by shovel.",
                                            "Show this beautiful text to help you ☻"
};
char* home;

static const unsigned int sinus[] = {
  515704, 531394, 547052, 562664, 578214, 593687, 609068, 624341, 639491,
  654504, 669364, 684057, 698568, 712883, 726989, 740870, 754513, 767906,
  781034, 793885, 806445, 818704, 830647, 842265, 853545, 864476, 875047,
  885248, 895069, 904500, 913532, 922156, 930363, 938145, 945495, 952406,
  958870, 964881, 970434, 975522, 980141, 984286, 987954, 991139, 993840,
  996054, 997778, 999011, 999752, 999999, 999754, 999014, 997783, 996060,
  993848, 991148, 987964, 984298, 980154, 975536, 970449, 964898, 958888,
  952426, 945516, 938168, 930386, 922180, 913558, 904527, 895097, 885277,
  875077, 864507, 853577, 842299, 830682, 818739, 806482, 793922, 781072,
  767945, 754553, 740910, 727030, 712925, 698610, 684100, 669407, 654548,
  639536, 624386, 609113, 593733, 578260, 562710, 547098, 531440, 515751,
  500046, 484341, 468651, 452993, 437381, 421830, 406357, 390976, 375703,
  360552, 345539, 330679, 315985, 301474, 287158, 273052, 259170, 245525,
  232132, 219003, 206152, 193590, 181331, 169386, 157768, 146487, 135555,
  124983, 114781, 104959, 95526, 86493, 77868, 69660, 61876, 54525, 47613,
  41147, 35135, 29581, 24491, 19871, 15724, 12056, 8868, 6166, 3951, 2225,
  990, 248, 0, 244, 982, 2212, 3933, 6144, 8842, 12025, 15690, 19832, 24448,
  29534, 35084, 41092, 47554, 54462, 61809, 69589, 77794, 86415, 95445,
  104873, 114692, 124891, 135460, 146389, 157667, 169282, 181224, 193480,
  206039, 218888, 232015, 245406, 259048, 272928, 287032, 301346, 315856,
  330548, 345407, 360419, 375568, 390841, 406221, 421693, 437243, 452854,
  468513, 484202, 499907
};
const char dlchar = '=';
static void fly(struct ProgressData *bar, bool moved)
{
  char buf[256];
  int pos;
  int check = bar->width - 2;

  curl_msnprintf(buf, sizeof(buf), "%*s\r", bar->width-1, " ");
  memcpy(&buf[bar->bar], "-=O=-", 5);

  pos = sinus[bar->tick%200] / (1000000 / check);
  buf[pos] = dlchar;
  pos = sinus[(bar->tick + 5)%200] / (1000000 / check);
  buf[pos] = dlchar;
  pos = sinus[(bar->tick + 10)%200] / (1000000 / check);
  buf[pos] = dlchar;
  pos = sinus[(bar->tick + 15)%200] / (1000000 / check);
  buf[pos] = dlchar;

  fputs(buf, bar->out);
  bar->tick += 2;
  if(bar->tick >= 200)
    bar->tick -= 200;

  bar->bar += (moved?bar->barmove:0);
  if(bar->bar >= (bar->width - 6)) {
    bar->barmove = -1;
    bar->bar = bar->width - 6;
  }
  else if(bar->bar < 0) {
    bar->barmove = 1;
    bar->bar = 0;
  }
}

#if (SIZEOF_CURL_OFF_T == 4)
#  define CURL_OFF_T_MAX CURL_OFF_T_C(0x7FFFFFFF)
#else
   /* assume SIZEOF_CURL_OFF_T == 8 */
#  define CURL_OFF_T_MAX CURL_OFF_T_C(0x7FFFFFFFFFFFFFFF)
#endif

int progressbar_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow){
  struct ProgressData *bar = clientp;
  curl_off_t total;
  curl_off_t point;

  /* Calculate expected transfer size. initial_size can be less than zero
     when indicating that we are expecting to get the filesize from the
     remote */
  if(bar->initial_size < 0 ||
     ((CURL_OFF_T_MAX - bar->initial_size) < (dltotal + ultotal)))
    total = CURL_OFF_T_MAX;
  else
    total = dltotal + ultotal + bar->initial_size;

  /* Calculate the current progress. initial_size can be less than zero when
     indicating that we are expecting to get the filesize from the remote */
  if(bar->initial_size < 0 ||
     ((CURL_OFF_T_MAX - bar->initial_size) < (dlnow + ulnow)))
    point = CURL_OFF_T_MAX;
  else
    point = dlnow + ulnow + bar->initial_size;

  if(bar->calls) {
    /* after first call... */
    if(total) {
      /* we know the total data to get... */
      if(bar->prev == point)
        /* progress didn't change since last invoke */
        return 0;

    }
    else {
        fly(bar, point != bar->prev);
    }
  }

  /* simply count invokes */
  bar->calls++;

  if((total > 0) && (point != bar->prev)) {
    char line[MAX_BARLENGTH + 3];
    char format[40];
    double frac;
    double percent;
    int barwidth;
    int num;
    if(point > total)
      /* we have got more than the expected total! */
      total = point;

    frac = (double)point / (double)total;
    percent = frac * 100.0;
    if(percent >= 25 && bar->prev == 0){
        return 0;
    }
    barwidth = bar->width - 9;
    num = (int) (((double)barwidth) * frac);
    if(num > MAX_BARLENGTH)
      num = MAX_BARLENGTH;
    memset(line, ' ', MAX_BARLENGTH +3);
    memset(line, dlchar, num);
    line[0] = '[';
    if(num != barwidth)
        line[num] = '>';
    else
        line[num] = dlchar;
    line[barwidth+1] = ']';
    line[barwidth+2] = '\0';
    curl_msnprintf(format, sizeof(format), "\r%%-%ds %%5.1f%%%%", barwidth);
    fprintf(bar->out, format, line, percent);
  }
  fflush(bar->out);
  bar->prev = point;

  return 0;
}

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

void toLower(char* txt){
    int len = strlen(txt);
    for(int i = 0; i < len; ++i){
        txt[i] = tolower(txt[i]);
    }
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
    toLower(otherName);
    while(i < len){
        cJSON* data = cJSON_GetArrayItem(apps,i);
        cJSON* out = cJSON_GetObjectItem(data,"name");
        char* curAppName = cJSON_Print(out);
        fixText('\"',curAppName);
        char otherAppName[64] = {0};
        strcpy(otherAppName,curAppName);
        toLower(otherAppName);
        if(strstr(curAppName, subname) != NULL || strstr(otherAppName, otherName) != NULL){
            out_apps[count] = data;
            count++;
        }
        ++i;
    }
    return count;
}
/**
 Get the version of the app based on the links and put the version string in OutVersion. If no download link is present we fail and don't change the outVersion parameter value.
**/
int getVersion(cJSON* app,char*outVersion){
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
        int success = par_easycurl_to_memory(lastUrl,&t_data,&temp,NULL);
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
        success = version != NULL ? 1 : 0;

        if(success)
            strcpy(outVersion,version);

        cJSON_Delete(urlData);
        free(t_data);

        return success;
}
void status(cJSON* apps) {
    char manifestsPath[PATHNAME_MAX] = {0};
    strcat(manifestsPath,home);
    strcat(manifestsPath,"/");
    strcat(manifestsPath,"shovel");
    strcat(manifestsPath,"/");
    strcat(manifestsPath,"manifests");

    char* names[256] = {0};
    for(int i = 0; i < 256;++i){names[i] = (char*)malloc(sizeof(char) *PATHNAME_MAX);}

    uint32_t dirsize = readDir(manifestsPath,false,names);
    int first = 0;
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

        cJSON* app = searchFullname(names[i],apps);
        char webVersion[256] = {0};
        getVersion(app,webVersion);

        if( webVersion[0] != '\0' && strcmp(version, webVersion) != 0){
            if(!first){
                kinc_log(KINC_LOG_LEVEL_INFO,"Apps with available updates:\n");
                first++;
            }
            kinc_log(KINC_LOG_LEVEL_INFO,"%s (%s -> %s)\n",names[i],version,webVersion);
        }
    }
    if(!first)
        kinc_log(KINC_LOG_LEVEL_INFO,"No apps to update\n");

    for(int i = 0; i < 256;++i){free(names[i]);}
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
        int success = par_easycurl_to_memory(lastUrl,&t_data,&temp,NULL);
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
    int success = par_easycurl_to_memory(lastUrl,&t_data,&temp,NULL);
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
        kinc_log(KINC_LOG_LEVEL_ERROR,"App can't be installed, app manifest for %s has no links, file an issue at https://github.com/AppImage/appimage.github.io/issues",name);
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
    int success = par_easycurl_to_memory(lastUrl,&t_data,&temp,NULL);
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
            char* dlfile = cJSON_Print(cJSON_GetObjectItem(item,"browser_download_url"));
            fixText('\"',type);
            fixText('\"',dlfile);
            if((strcmp(type,"application/x-iso9660-appimage") == 0 || strcmp(type,"application/octet-stream") == 0) && (strstr(dlfile,endingOfFilename) != NULL || strstr(dlfile,".AppImage"))){
                dlLink = dlfile;
                fixText('\"',dlLink);
                break;

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

    kinc_log(KINC_LOG_LEVEL_INFO,"Downloading %s:",name);
    success = par_easycurl_to_memory(dlLink,&t_data,&temp,progressbar_callback);
    char fname[PATHNAME_MAX] = {0};
    int found = getFilename(dlLink,fname);

    bool wasInstalled = false;
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

        char symbolPath[PATHNAME_MAX] = {0};
        char symbolName[PATHNAME_MAX] = {0};
        strcpy(symbolName,desktop);
        symbolName[strlen(desktop)-8] = 0;
        toLower(symbolName);
        sprintf(symbolPath,"%s/shovel/symbols/%s",home,symbolName);
        if(symlink(path,symbolPath) == 0){
            kinc_log(KINC_LOG_LEVEL_INFO,"Successfully created a symlink %s for %s.You can start the app from your shell.",symbolName,name);
        }
        else {
            kinc_log(KINC_LOG_LEVEL_INFO,"Coulnd't create a symlink for %s.",name);
        }

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
        wasInstalled = true;
    }

    if(wasInstalled)
        kinc_log(KINC_LOG_LEVEL_INFO,"Successfully installed %s (%s)\n",name,version);
    cJSON_Delete(urlData);
    if(success)
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

    rmFile(manifestPath);

    char* path[PATHNAME_MAX] = {0};
    strcat(path,home);
    strcat(path,"/");
    strcat(path,"shovel");
    strcat(path,"/");
    strcat(path,"apps");
    strcat(path,"/");
    strcat(path,name);

    char symbolPath[PATHNAME_MAX] = {0};
    char symbolName[PATHNAME_MAX] = {0};
    getFilename(desktopPath,symbolName);
    symbolName[strlen(symbolName)-8] = 0;
    toLower(symbolName);
    sprintf(symbolPath,"%s/shovel/symbols/%s",home,symbolName);
    if(rmFile(symbolPath)){
        kinc_log(KINC_LOG_LEVEL_INFO,"Successfully removed symlink %s from PATH.\n",symbolName);
    }

    recursiveRmDir(path);

    kinc_log(KINC_LOG_LEVEL_INFO,"Successfully uninstalled %s .\n",name);

    cJSON_Delete(manifest);
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
        int success = par_easycurl_to_memory(lastUrl,&t_data,&temp,NULL);
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

bool verifyShell(char* term){
    for(int i = 0; i < SHELL_AMOUNT;++i){
        if(strcmp(term,supportedTerminals[i]) == 0){
            return true;
        }
    }
    return false;
}
void initialize(char* term){
    char path[PATHNAME_MAX] = {0};
    char exportLine[PATHNAME_MAX] = {0};
    sprintf(exportLine,"%s%s/%s",exportPath,home,"shovel/symbols");
    sprintf(path,"%s/.%src",home,term);
    FILE* f = fopen(path,"a");
    fprintf(f,"%s",exportLine);
    fclose(f);
    sprintf(path,"%s/shovel/symbols",home);
    recursiveMkdir(path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

}
void printHelp(){
    printf("Available commands\n");
    for(int i = 0; i < COMMAND_AMOUNT; ++i){
        printf("%s:\n",commands[i]);
        printf("\t%s\n",commandsTxt[i]);
    }
}
/**
    available actions are:
        - install [DONE]
        - info [DONE]
        - search [DONE]
        - list [DONE]
        - uninstall/rm [DONE]
        - update [DONE]
        - status [DONE]
**/
int main(int argc, char** argv) {

    home = getenv("HOME");
    ProgressData_t* pbdata = malloc(sizeof(ProgressData_t));
    par_easycurl_init(0,pbdata);
    progressbarinit(pbdata,stdout);

    FILE *srcFile;

	srcFile = fopen("feed.json", "rb");
	int len = 4096;
    par_byte* data = (par_byte*)malloc(sizeof(par_byte)*len);
    int success = par_easycurl_to_memory("https://appimage.github.io/feed.json",&data,&len,NULL);
	if(srcFile == NULL || success){
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
	}
	free(data);
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
	if(argc > 2 &&  strcmp(argv[1],"init") != 0){
        char manifestPath[PATHNAME_MAX] = {0};
        strcat(manifestPath,home);
        strcat(manifestPath,"/");
        strcat(manifestPath,"shovel");
        strcat(manifestPath,"/");
        strcat(manifestPath,"manifests");
        if(strcmp(argv[1],"install") == 0){
            for(int i = 2; i < argc;++i){
                cJSON* app = searchFullname(argv[i],apps);
                char manifestP[PATHNAME_MAX] = {0};
                strcpy(manifestP,manifestPath);
                strcat(manifestP,"/");
                strcat(manifestP,argv[i]);
                strcat(manifestP,".json");
                bool installed = fileExists(manifestP);
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
        }
        else if(strcmp(argv[1],"uninstall") == 0 || strcmp(argv[1],"rm") == 0){
            strcat(manifestPath,"/");
            strcat(manifestPath,argv[2]);
            strcat(manifestPath,".json");
            if(fileExists(manifestPath)){
                uninstall(manifestPath,argv[2]);
            }
            else {
                goto NOTINSTALLED;
            }
        }
        else if(strcmp(argv[1],"update") == 0){
            if(strcmp(argv[2],"*") == 0){
                char* names[256] = {0};
                for(int i = 0; i < 256;++i){names[i] = (char*)malloc(sizeof(char) *PATHNAME_MAX);}

                uint32_t dirsize = readDir(manifestPath,false,names);
                for(int i = 0; i < dirsize; ++i){
                    char manifestP[PATHNAME_MAX] = {0};
                    strcpy(manifestP,manifestPath);
                    strcat(manifestP,"/");
                    strcat(manifestP,names[i]);
                    strcat(manifestP,".json");
                    cJSON* app = searchFullname(names[i],apps);
                    if(app != NULL)
                        update(manifestP,app);
                }

                for(int i = 0; i < 256;++i){free(names[i]);}
            }
            else {
                for(int i = 2; i < argc;++i){
                    cJSON* app = searchFullname(argv[i],apps);
                    char manifestP[PATHNAME_MAX] = {0};
                    strcpy(manifestP,manifestPath);
                    strcat(manifestP,"/");
                    strcat(manifestP,argv[i]);
                    strcat(manifestP,".json");
                    bool installed = fileExists(manifestP);
                    if(installed){
                        update(manifestPath,app);
                    }
                    else {
                        goto NOTINSTALLED;
                    }
                }
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
                kinc_log(KINC_LOG_LEVEL_INFO,"App %s isn't on the AppImage Hub",argv[2]);
            }
        }
        else if(strcmp(argv[1],"info") == 0){
            strcat(manifestPath,"/");
            strcat(manifestPath,argv[2]);
            strcat(manifestPath,".json");
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
        if(argc > 1 && strcmp(argv[1],"init") == 0){
            char* term = "bash";
            bool isValid = argc > 2 ? verifyShell(argv[2]) : true;
            if(argc > 2 && isValid){
                term = argv[2];
            }
            else if(argc > 2 && !isValid){
                kinc_log(KINC_LOG_LEVEL_WARNING,"The shell app passed %s isn't officialy supported since it's for special snowflakes as yourself ;)\nYou can make a PR if you would like to use it with shovel.\nFor now, we will default to bash if it's installed.",argv[2]);
            }
            else {
                kinc_log(KINC_LOG_LEVEL_INFO,"No shell app passed to init command.For now, we will default to bash if it's installed.");
            }
            initialize(term);
        }
        else if(argc > 1 && strcmp(argv[1],"list") == 0){
            listInstalled();
        }
        else if(argc > 1 && strcmp(argv[1],"status") == 0){
            status(apps);
        }
        else {
            printHelp();
        }
	}

    free(txt);
	cJSON_Delete(parsed);

	free(pbdata);

    if(0){ EXIT: kinc_log(KINC_LOG_LEVEL_WARNING,"App %s isn't on the AppImage Hub\n",argv[2]);}
    if(0){ NOTINSTALLED: kinc_log(KINC_LOG_LEVEL_WARNING,"App %s isn't installed\n",argv[2]);}
	return 0;
}
