#include "utilities.h"


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

namespace fs = std::experimental::filesystem;

void getModelFromDir(const char* home, char **modelpath, char **rulespath, float *budget){
    char buf[BUFFER_SIZE];
    
    sprintf(buf, "%s/%s", home, MODEL);
    
    if(*modelpath == NULL)
        *modelpath = strdup(buf);
    
    sprintf(buf, "%s/%s", home, RULES);
    
    if (*rulespath == NULL)
        *rulespath = strdup(buf);
    
    sprintf(buf, "%s/%s", home, PARAMS);

    if(*budget == 0.0){
        FILE *f;
        if ((f = fopen(buf, "r")) == NULL){
            printf("[ERROR] opening PARAMS\n");
            exit(1);
        }
        if(!fscanf(f,"%f", budget)){
            printf("[ERROR] reading PARAMS\n");
            exit(1);
        }
        fclose(f);
    }
}



ssize_t Read(int fd, void *buf, size_t count){
	if(count==0){return 0;}
	ssize_t bs = recv(fd,buf,count,MSG_WAITALL);

	if(bs == -1){
		fprintf(stderr,"[ERROR]: read failed with error %d\n",errno);
		exit(EXIT_FAILURE);
	}
	if(bs != count){
		fprintf(stderr,"[WARN]: read returned %lu\n",count);
	}
	return bs;
}


ssize_t Write(int fd, const void *buf, size_t count){
	if (count == 0){return 0;}
	ssize_t bs=send(fd,buf,count,0);
	if(bs == -1){
		fprintf(stderr,"[ERROR]: write failed with error %d\n",errno);
		exit(EXIT_FAILURE);
	}
	if(bs != count){
		fprintf(stderr,"[WARN]: write returned %lu\n",count);
	}
	return bs;
}

