#include <iostream>
#include <map>
#include <vector>

#include "cppflow/ops.h"
#include "cppflow/model.h"


typedef int encoded_t;
typedef std::map<char, encoded_t> char_map_t;


struct model_context{
    cppflow::model *model;
    int max_len;
    int64_t batch_size;
    // memory-pool
    std::vector<encoded_t> batch_encoded;
    char_map_t *char_map;  
};


char_map_t* load_char_map(const char *path, char_map_t *char_map);
void encode_batch(char_map_t *char_map, uint64_t batch_size, char **batch, encoded_t *batch_encoded, int max_len);
std::vector<float> adaptive_inference_batch(model_context* ctx, char **batch, uint32_t batch_size);


//init example:

/*
// init model context
model_context ctx;
cppflow::model model(model_path);
ctx.model = &model;
// char map
char_map_t char_map;
load_char_map(charmap_path, &(char_map));
ctx.char_map = &char_map;
// dimensions
ctx.max_len = 16;
ctx.batch_size = 4096;
// memory pool   
int64_t batch_max_size = ctx.batch_size * ctx.max_len;    
ctx.batch_encoded = std::vector<encoded_t>(batch_max_size);
*/