#pragma once

#include <list>
#include <vector>
#include <string>
#include <set>
#include "utilities.h"

#define BUF_SIZE 4096

struct cstrless {
    bool operator()(const char* a, const char* b) {
        return strcmp(a, b) < 0;
    }
};

typedef std::list<const char*> wordlist_t;
typedef std::list<const char*>::iterator it_wordlist_t;
typedef std::set<char *, cstrless> target_t;


wordlist_t loadWordlistFromFile_list(const char *path);
target_t loadTarget(const char* path);
