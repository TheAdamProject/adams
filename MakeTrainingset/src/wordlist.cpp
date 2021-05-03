#include <list>
#include <vector>
#include <string>
#include <set>

typedef std::vector<const char*> wordlist_t;
typedef std::vector<const char*>::iterator it_wordlist_t;

#define BUF_SIZE 4096
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


//-----__-----__------------------------------------_____--_---_----

struct cstrless {
    bool operator()(const char* a, const char* b) {
        return strcmp(a, b) < 0;
    }
};

typedef std::set<char *, cstrless> target_t;

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
