#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include<set>
using namespace std; 

#define BUF_SIZE 4096
#define CHECKPOINT 10000

#define DIRPERM S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

struct MyList {
    char *term;
    struct MyList *left;
    struct MyList *right;
};

typedef struct MyList MyList;
typedef struct MyList * MyListPtr;

MyListPtr insertElem(MyListPtr head, char *term);
int searchElem(MyListPtr head, char *term);
MyListPtr createElem(char *term);
void freeTree(MyListPtr head);
int mycompare(char *t1, char *t2);

MyListPtr* loadLabelSets(const char** paths, int numOfLabelSet);
MyListPtr loadLabelSetGetN(const char* paths, uint64_t *N);
int loadLabelSetGetNSets(const char* path, set<char *> *st, uint64_t *N);
char *Itoa(int n);
