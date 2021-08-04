#include "adaptive_model.h"


char_map_t* load_char_map(const char *path, char_map_t *char_map){
    FILE *fin = fopen(path, "r");
    int v, k;
    while (!feof(fin)){  
       if(fscanf(fin, "%d %d\n", &v, &k) == EOF)
           break;
       //printf("%c %d\n", v, k); 
       (*char_map)[(char)v] = k;
    }
    fclose(fin);
}


void encode_batch(char_map_t *char_map, uint64_t batch_size, char **batch, encoded_t *batch_encoded, int max_len){
    uint64_t i = 0; 
    for(uint64_t _i=0; _i<batch_size; _i++){
        for(int _j=0; _j<max_len; _j++, i++){
           if(_j > strlen(batch[_i]))
               batch_encoded[i] = 0;
            else
               batch_encoded[i] = (*char_map)[batch[_i][_j]];
        }
    }
}

std::vector<float> adaptive_inference_batch(model_context* ctx, char **batch, uint32_t batch_size){
    //preprocess input 
    encode_batch(ctx->char_map, batch_size, batch, ctx->batch_encoded.data(), ctx->max_len);
    
    /*
    for(int i=0; i<batch_size * ctx->max_len; i++){
        printf("%d ", ctx->batch_encoded.data()[i]);
    }
    printf("\n");
    */
    
    std::vector<int64_t> shape = {batch_size, ctx->max_len};
    auto input = cppflow::tensor(ctx->batch_encoded, shape);
    //inference
    // todo use pre-allocated out buffer
    auto output = (*(ctx->model))(input);
    std::vector<float> values = output.get_data<float>();
    return values;
}

