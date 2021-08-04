#include "wordlist.h"

wordlist_t loadWordlistFromFile_list(const char *path){
    char buf[BUF_SIZE];
    uint64_t i = 0;
    
    wordlist_t word_list;
    
    FILE *fin = Fopen(path, "r");

    while( NULL != fgets(buf, BUF_SIZE,fin)){
        int n = strlen(buf);
        char *s = strdup(buf);
        s[n-1] = '\0';
        word_list.push_back(s);
        i++;
    }
    fclose(fin);
    return word_list;
}

target_t loadTarget(const char* path){
    
    target_t st;
    char buf[BUF_SIZE];
    uint64_t i = 0;
    
    FILE *fin = Fopen(path,"r");
    while( NULL != fgets(buf, BUF_SIZE, fin)){
        int n = strlen(buf);
        char *s = strdup(buf);
        s[n-1] = '\0';
        st.insert(s);
        i++;
    }
    fclose(fin);
    
   return st;
}
