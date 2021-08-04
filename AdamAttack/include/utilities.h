#include <iostream>
#include <experimental/filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <set>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define DIRPERM S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

#define MODEL "model"
#define RULES "rules.txt"
#define PARAMS "params.txt"
#define BUFFER_SIZE 10000

void getModelFromDir(const char* _home, char **modelpath, char **rulespath, float *budget);
void *Malloc(size_t sz, const char *name);
void *Calloc(size_t nmemb, size_t size, const char *name);
FILE *Fopen(const char *path, const char *mode);
void Mkdir(char * path, mode_t mode);
char *printDate();

ssize_t Read(int fd, void *buf, size_t count);
ssize_t Write(int fd, const void *buf, size_t count);