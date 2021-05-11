// EASYCURL :: https://github.com/prideout/par
// Wrapper around libcurl for performing simple synchronous HTTP requests.
//
// Distributed under the MIT License, see bottom of file.

// -----------------------------------------------------------------------------
// BEGIN PUBLIC API
// -----------------------------------------------------------------------------

#ifndef PAR_EASYCURL_H
#define PAR_EASYCURL_H

#include <curl/system.h>
#include <curl/curl.h>

typedef struct ProgressData {
  int         calls;
  curl_off_t  prev;
  int         width;
  FILE       *out;  /* where to write everything to */
  curl_off_t  initial_size;
  unsigned int tick;
  int bar;
  int barmove;
} ProgressData_t;


#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char par_byte;

// Call this before calling any other easycurl function.  The flags are
// currently unused, so you can just pass 0.
void par_easycurl_init(unsigned int flags,struct ProgressData* bardata);

// Allocates a memory buffer and downloads a data blob into it.
// Returns 1 for success and 0 otherwise.  The byte count should be
// pre-allocated.  The caller is responsible for freeing the returned data.
// This does not do any caching!
int par_easycurl_to_memory(char const* url, par_byte** data, int* nbytes,curl_xferinfo_callback progress_callback);

// Downloads a file from the given URL and saves it to disk.  Returns 1 for
// success and 0 otherwise.
int par_easycurl_to_file(char const* srcurl, char const* dstpath);

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
// END PUBLIC API
// -----------------------------------------------------------------------------

#ifdef PAR_EASYCURL_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <curl/easy.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#define MAX_BARLENGTH 256

int load_cacertfile(const char *filename, void **filedata, size_t *filesize)
{
  size_t datasize = 0;
  void *data = NULL;
  if(filename) {
    FILE *fInCert = fopen(filename, "rb");

    if(fInCert) {
      long cert_tell = 0;
      bool continue_reading = fseek(fInCert, 0, SEEK_END) == 0;
      if(continue_reading)
        cert_tell = ftell(fInCert);
      if(cert_tell < 0)
        continue_reading = 0;
      else
        datasize = (size_t)cert_tell;
      if(continue_reading)
         continue_reading = fseek(fInCert, 0, SEEK_SET) == 0;
      if(continue_reading)
          data = malloc(datasize + 1);
      if((!data) ||
          ((int)fread(data, datasize, 1, fInCert) != 1))
          continue_reading = 0;
      fclose(fInCert);
      if(!continue_reading) {
        free(data);
        datasize = 0;
        data = NULL;
      }
   }
  }
  *filesize = datasize;
  *filedata = data;
  return data ? 1 : 0;
}

void progressbarinit(struct ProgressData *bar,FILE* out)
{
  char *colp;
  memset(bar, 0, sizeof(struct ProgressData));

  colp = curl_getenv("COLUMNS");
  if(colp) {
    char *endptr;
    long num = strtol(colp, &endptr, 10);
    if((endptr != colp) && (endptr == colp + strlen(colp)) && (num > 20) &&
       (num < 10000))
      bar->width = (int)num;
    curl_free(colp);
  }

  if(!bar->width) {
    int cols = 0;
    if(cols > 20)
      bar->width = cols;
  }

  if(!bar->width)
    bar->width = 79;
  else if(bar->width > MAX_BARLENGTH)
    bar->width = MAX_BARLENGTH;

  bar->out = out == NULL ? stderr : out;
  bar->tick = 150;
  bar->barmove = 1;
}

static int _ready = 0;
static struct ProgressData* bar = NULL;
//static struct curl_blob* blob; Add me back when Blob loading is available in curl stable
void par_easycurl_init(unsigned int flags,struct ProgressData* bardata)
{
    if (!_ready) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        _ready = 1;
    }
    /*blob = malloc(sizeof(struct curl_blob));
    size_t certsize;
    void *certdata;
    if(!load_cacertfile("cacert.pem",&certdata,&certsize)){
        printf("Couldn't load cacert file. Https won't work.");
        free(blob);
    }
    blob->data = certdata;
    blob->len = certsize;
    blob->flags = CURL_BLOB_COPY;*/
    bar = bardata;
}

void par_easycurl_shutdown()
{
    if (_ready) {
        curl_global_cleanup();
    }
    /*if(blob != NULL){
        if(blob->data != NULL)
            free(blob->data);
        free(blob);
    }*/
    bar = NULL;
}

static size_t onheader(void* v, size_t size, size_t nmemb)
{
    size_t n = size * nmemb;
    char* h = (char*) v;
    if (n > 14 && !strncasecmp("Last-Modified:", h, 14)) {
        char const* s = h + 14;
        time_t r = curl_getdate(s, 0);
        if (r != -1) {
            // TODO handle last-modified
        }
    } else if (n > 5 && !strncasecmp("ETag:", h, 5)) {
        // TODO handle etag
    }
    return n;
}

typedef struct {
    par_byte* data;
    int nbytes;
} par_easycurl_buffer;

static size_t onwrite(char* contents, size_t size, size_t nmemb, void* udata)
{
    size_t realsize = size * nmemb;
    par_easycurl_buffer* mem = (par_easycurl_buffer*) udata;
    mem->data = (par_byte*) realloc(mem->data, mem->nbytes + realsize + 1);
    if (!mem->data) {
        return 0;
    }
    memcpy(mem->data + mem->nbytes, contents, realsize);
    mem->nbytes += realsize;
    mem->data[mem->nbytes] = 0;
    return realsize;
}

#if IOS_EXAMPLE
bool curlToMemory(char const* url, uint8_t** data, int* nbytes)
{
    NSString* nsurl =
    [NSString stringWithCString:url encoding:NSASCIIStringEncoding];
    NSMutableURLRequest* request =
    [NSMutableURLRequest requestWithURL:[NSURL URLWithString:nsurl]];
    [request setTimeoutInterval: TIMEOUT_SECONDS];
    NSURLResponse* response = nil;
    NSError* error = nil;
    // Use the simple non-async API because we're in a secondary thread anyway.
    NSData* nsdata = [NSURLConnection sendSynchronousRequest:request
        returningResponse:&response
        error:&error];
    if (error == nil) {
        *nbytes = (int) [nsdata length];
        *data = (uint8_t*) malloc([nsdata length]);
        memcpy(*data, [nsdata bytes], [nsdata length]);
        return true;
    }
    BLAZE_ERROR("%s\n", [[error localizedDescription] UTF8String]);
    return false;
}

#endif

int par_easycurl_to_memory(char const* url, par_byte** data, int* nbytes,curl_xferinfo_callback progress_callback)
{
    char errbuf[CURL_ERROR_SIZE] = {0};
    par_easycurl_buffer buffer = {(par_byte*) malloc(1), 0};
    long code = 0;
    long status = 0;
    CURL* handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 8);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1);
    if(progress_callback != NULL && bar != NULL){
        progressbarinit(bar,bar->out);
        curl_easy_setopt(handle,CURLOPT_NOPROGRESS,0);
        curl_easy_setopt(handle,CURLOPT_XFERINFOFUNCTION,progress_callback);
        curl_easy_setopt(handle,CURLOPT_XFERINFODATA,bar);
    }
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, onwrite);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, onheader);
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
    curl_easy_setopt(handle, CURLOPT_TIMEVALUE, 0);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, 0);
    //curl_easy_setopt(handle, CURLOPT_HEADER, 1);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 100);
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(handle, CURLOPT_USERAGENT,"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.103 Safari/537.36");
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1);
    //curl_easy_setopt(handle, CURLOPT_CAINFO, blob);
    curl_easy_setopt(handle, CURLOPT_CAINFO, "cacert.pem");
    CURLcode res = curl_easy_perform(handle);
    if(bar != NULL && bar->calls)
        fputs("\n",bar->out);
    if (res != CURLE_OK) {
        printf("CURL Error: %s\n", errbuf);
        return 0;
    }
    curl_easy_getinfo(handle, CURLINFO_CONDITION_UNMET, &code);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    if (status == 304 || status >= 400) {
        return 0;
    }
    *data = buffer.data;
    *nbytes = buffer.nbytes;
    curl_easy_cleanup(handle);
    return 1;
}

int par_easycurl_to_file(char const* srcurl, char const* dstpath)
{
    long code = 0;
    long status = 0;
    FILE* filehandle = fopen(dstpath, "wb");
    if (!filehandle) {
        printf("Unable to open %s for writing.\n", dstpath);
        return 0;
    }
    CURL* handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_ENCODING, "gzip, deflate");
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 8);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, filehandle);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, onheader);
    curl_easy_setopt(handle, CURLOPT_URL, srcurl);
    curl_easy_setopt(handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
    curl_easy_setopt(handle, CURLOPT_TIMEVALUE, 0);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, 0);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_perform(handle);
    curl_easy_getinfo(handle, CURLINFO_CONDITION_UNMET, &code);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    fclose(filehandle);
    if (status == 304 || status >= 400) {
        remove(dstpath);
        return 0;
    }
    curl_easy_cleanup(handle);
    return 1;
}

#endif // PAR_EASYCURL_IMPLEMENTATION
#endif // PAR_EASYCURL_H

// par_easycurl is distributed under the MIT license:
//
// Copyright (c) 2019 Philip Rideout
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
