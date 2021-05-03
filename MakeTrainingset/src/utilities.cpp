#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <set>

#define DIRPERM S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

void *Malloc(size_t sz, const char *name) {
    void *ptr;

    ptr = malloc(sz);
    if (!ptr) {
        fprintf(stderr, "Cannot allocate %zu bytes for %s\n\n", sz, name);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *Calloc(size_t nmemb, size_t size, const char *name){
    void *ptr;

    ptr=calloc(nmemb,size);
    if (!ptr) {
        fprintf(stderr, "Cannot allocate %zu bytes for %s\n\n", (nmemb*size), name);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

FILE *Fopen(const char *path, const char *mode){
    FILE *fp = NULL;
    fp = fopen(path, mode);
    if (!fp) {
        fprintf(stderr, "Cannot open file %s...\n", path);
        exit(EXIT_FAILURE);
    }
    return fp;
}

void Mkdir(char * path, mode_t mode){
    int status = 0;
    status = mkdir(path,mode);
    if( status != 0 && errno != EEXIST){
        fprintf(stderr,"Cannot create dir %s\n",path);
	    exit(EXIT_FAILURE);
    }
}
char *printDate(){

    time_t now;
    time(&now);
    char *timeString = ctime(&now);
    int l = strlen(timeString);
    timeString[l-1] ='\0';
    return timeString;
}