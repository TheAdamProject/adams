#include "utilities.h"

namespace fs = std::experimental::filesystem;

void getModelFromDir(const char* _home, char **modelpath, char **rulespath, float *budget){
    fs::path home = fs::path(_home);
    home /= MODEL;
    
    //sprintf(modelpath, "%s", home.string().c_str());
    if( *modelpath == NULL){
        *modelpath=strdup(home.string().c_str());
    }
    
    home = fs::path(_home);
    home /= RULES;
    
    //sprintf(rulespath, "%s", home.string().c_str());
    if (*rulespath == NULL){
        *rulespath=strdup(home.string().c_str());
    }
    home = fs::path(_home);
    home /= PARAMS;

    if( *budget == 0.0){
        FILE *f;
        if ((f = fopen(home.c_str(), "r")) == NULL){
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
