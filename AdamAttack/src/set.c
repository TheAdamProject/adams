#include"set.h"
#include<iostream>
MyListPtr insertElem(MyListPtr head, char *term){
    if( NULL == head){
        return createElem(term);
    }
    MyListPtr cur = head;
    int test = 0;
    while( cur != NULL){
       test = mycompare(term, cur->term);
       if( 0 == test){
           cur = NULL;
       } else if( 1 == test){
           if( NULL == cur->left){
               cur->left = createElem(term);
           }
           cur = cur->left;
       } else {
           if( NULL == cur->right){
               cur->right = createElem(term);
           }
           cur = cur->right;
       }
    }

    return head;
}


int searchElem(MyListPtr head, char *term){
    if( NULL == head){
        return 0;
    }
    MyListPtr cur = head;

    int test = 0;
    while( cur != NULL){
       test = mycompare(term, cur->term);
       if( 0 == test){
          return 1;
       } else if( 1 == test){
           cur = cur->left;
       } else {
           cur = cur->right;
       }
    }
    return 0;
}

MyListPtr createElem(char *term){

        MyListPtr elem = (MyListPtr)Malloc(sizeof(MyList),"[createElem]: Malloc elem");
        size_t term_len = strlen(term) + 1;
        elem->term = (char*)Malloc( term_len * sizeof(char),"[createElem]: Malloc elem->term");
        memcpy(elem->term, term, term_len *sizeof(char));
        elem->left = NULL;
        elem->right = NULL;
        return elem;
}

void freeTree(MyListPtr head){
    if( NULL == head){
        return;
    }

    if(head->term != NULL){
        free(head->term);
    }

    if( head->left != NULL){
        freeTree(head->left);
    } 
    if( head->right != NULL){
        freeTree(head->right);
    }

    free(head);
    return;
}
// It returns 
//      * 0 if t1 == t2
//      * 1 if t1 < t2
//      * -1 if t1 > t2 
int mycompare(char *t1, char *t2){
    size_t t1_len, t2_len;
    size_t i, j;

    t1_len = strlen(t1);
    t2_len = strlen(t2);

    for( i = 0, j = 0; i < t1_len && j < t2_len ; i++, j++){
       if(t1[i] == t2[j]){
           continue;
       } else if ( t1[i] < t2[j]){
           return 1;
       } else {
           return -1;
       }
    }
    if ( i == t1_len){
        if ( j == t2_len){
            return 0;
        }
        return 1;
    }

    return -1;
}

//------------------

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

//-----------------

MyListPtr* loadLabelSets(const char** paths, int numOfLabelSet){
    
    char termBuf[BUF_SIZE];
    int i;
    MyListPtr * headsVector = (MyListPtr *)Malloc(sizeof(MyListPtr)*numOfLabelSet,"[main]: Malloc headsVector");
    for(i = 0; i < numOfLabelSet; i++){
        
        FILE *fin = Fopen(paths[i],"r");
        MyListPtr head = NULL;
        fprintf(stderr,"[INFO][%s]: creating binary tree of %d-th label set (%s) \n", printDate(), i, paths[i]);
        while( NULL != fgets(termBuf,BUF_SIZE,fin)){
            int n = strlen(termBuf);
            termBuf[n-1] = '\0';
            //printf("%s\n", termBuf);
            head = insertElem(head,termBuf);
        }
        fprintf(stderr,"[INFO][%s]: %d-th binary tree created\n",printDate(),i);
        fclose(fin);
        headsVector[i] = head;
   }
   
   return headsVector;
}


MyListPtr loadLabelSetGetN(const char* path, uint64_t *N){
    
    char termBuf[BUF_SIZE];
    uint64_t i = 0;

    FILE *fin = Fopen(path,"r");
    MyListPtr head = NULL;
    fprintf(stderr,"[INFO][%s]: creating binary tree of the label set (%s) \n", printDate(), path);
    while( NULL != fgets(termBuf,BUF_SIZE,fin)){
        int n = strlen(termBuf);
        termBuf[n-1] = '\0';
        head = insertElem(head,termBuf);
        i++;
    }
    fprintf(stderr,"[INFO][%s]: the binary tree created\n",printDate());
    fclose(fin);
    
    *N = i;
   
   return head;
}

#define INSERT_ELEM(st,s) st->insert(s)
#define REMOVE_ELEM(st,s) st->erase(s)
#define SEARCH_ELEM(st,s) (st->find(s) != st->end() ?true :false)
int loadLabelSetGetNSets(const char* path, set<char *> * st, uint64_t *N){
    
    char termBuf[BUF_SIZE];
    uint64_t i = 0;

    FILE *fin = Fopen(path,"r");
    MyListPtr head = NULL;
    fprintf(stderr,"[INFO][%s]: creating binary tree of the label set (%s) \n", printDate(), path);
    while( NULL != fgets(termBuf,BUF_SIZE,fin)){
        int n = strlen(termBuf);
        termBuf[n-1] = '\0';
        //printf("LABEL: %s\n", termBuf);
        char * t =strdup((const char*) termBuf);
        st->insert(t);
        //fprintf(stderr,"[DEBUG]: insert %s size %ld\n",termBuf,st->size());
        i++;
    }
    fprintf(stderr,"[INFO][%s]: the binary tree created: %lu\n",printDate(), i);
    fclose(fin);
    
    *N = i;
   
   return 0;
}

char *Itoa(int n){
    char buf[BUF_SIZE];
    char *t = NULL;
    snprintf(buf, BUF_SIZE, "%d",n);
    t=strdup(buf);
    return t;
}
