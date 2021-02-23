#include <iostream>
#include <experimental/filesystem>
#include <string.h>
#define MODEL "model.h5"
#define RULES "rules.txt"
#define PARAMS "params.txt"
#define BUFFER_SIZE 10000

void getModelFromDir(const char* _home, char **modelpath, char **rulespath, float *budget);
