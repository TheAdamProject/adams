// SETTINGS -------------------------------------------------------------
// define to enable SEMANTIC MANGLING RULES
#undef SEMANTIC_RULES
// define to log HITS SOURCE TRACING 
#undef HITS_SOURCE_TRACING
// ----------------------------------------------------------------------

#include "adam_attack.h"
#define LOG_FQ 50000



#ifdef SEMANTIC_RULES
#define SOURCE_ID 0 // wordlist identity 
#define SOURCE_WR 1 // wordlist + mangling rule;
#define SOURCE_DYN 2 // dynamic
#define SOURCE_SEM 3 // semantic (identity)
#define SOURCE_SEMR 4 // semantic + mangling rule
#endif

#ifdef SEMANTIC_RULES

struct threadArgs{
    wordlist_t *matched_list_ptr;
    wordlist_t *wordlist;
    #ifdef SEMANTIC_RULES
    std::list<int> *source_wordlist;
    #endif  
};

pthread_mutex_t matched_list_mutex;
pthread_mutex_t word_list_mutex;
pthread_cond_t cond_available_in_matched_list = PTHREAD_COND_INITIALIZER;
volatile bool MAIN_ATTACK_STOP = false;
#include "semantic_deamon.cpp"

// filters out matched passwords that are not suited for semantic mangling rules e.g., numeric strings
bool is_for_semantic_exp(const *char){
    // todo
    return true;
}
#endif

/*
output_type 1 -> #guess matched
output_type 2 -> matched
output_type 3 -> log
output_type 4 -> guesses
*/

int runAttackAdam(
    model_context *ctx,
    rules_t * rules,
    wordlist_t *wordlist,
    target_t *target,
    float budget,
    bool dynamic,
    bool semantic,
    int output_type,
    uint64_t max_guess){
    
    
    char out[BUF_SIZE];
    
    uint64_t guesses, matched, wc, n_targets, log, cwl, crl;
    int64_t rc;
    guesses = matched = wc = n_targets = 0;
    log = rc = cwl = crl = 0;
    int ret = 0;

        
    uint64_t matched_per_word;
    char *cw = NULL, *cr = NULL;
    
    uint64_t num_target_pass = target->size();
    
#ifdef HITS_SOURCE_TRACING
    std::list<int> source_wordlist(wordlist->size(), SOURCE_WR);
#endif
    
#ifdef SEMANTIC_RULES
    wordlist_t matched_list;
    // set args thread
    struct threadArgs *thread_args;
    pthread_t thread_id;
    
    if(semantic){
        // init mutex
        if( pthread_mutex_init(&word_list_mutex,NULL) != 0 ){
            fprintf(stderr,"[FATAL]: Unable to initialize word_list_mutex. Closing\n");
            exit(EXIT_FAILURE);
        }

        if( pthread_mutex_init(&matched_list_mutex,NULL) != 0 ){
            fprintf(stderr,"[FATAL]: Unable to initialize matched_list_mutex. Closing\n");
            exit(EXIT_FAILURE);
        }


        thread_args = (struct threadArgs *) Malloc(sizeof(struct threadArgs),"Malloc thread_args");
        thread_args->wordlist = wordlist;
        thread_args->matched_list_ptr = &matched_list;
      
    #ifdef HITS_SOURCE_TRACING
        thread_args->source_wordlist = &source_wordlist;
    #endif  
        
        // Start thread
        if( pthread_create(&thread_id, NULL, semantic_provider,(void*)thread_args) != 0){
            fprintf(stderr,"[FATAL]: Unable to start thread. Closing\n");
            exit(EXIT_FAILURE);
        }
    }
#else
    if(semantic){
        fprintf(stderr,"[FATAL]: Semantic Mangling Rules are not supported\n");
        exit(EXIT_FAILURE);
    }
#endif
    
    // run ----------------------------------------------------------------
    
    // allocate batch pool dictonary words
    char **batch_words = (char**) Malloc(sizeof(char*) * BATCH_SIZE, "Malloc batch pool dictonary words");
    float* _comp_scores = NULL;
    std::vector<float> comp_scores;
    
    bool end = false, identity = true;

    // number of strings in the batch (<= BATCH_SIZE)
    uint64_t ib, i;
    
    // dictonary start
    it_wordlist_t it = wordlist->begin();
    
#ifdef HITS_SOURCE_TRACING
    int *batch_source = (int*) Malloc(sizeof(int) * BATCH_SIZE, "Malloc batch pool sources");
    std::list<int>::iterator it_source = source_wordlist.begin();
#endif        
    
RERUN_MAIN_ATTACK:
    
    while(!end){
        
#ifdef SEMANTIC_RULES
        if(semantic)
            pthread_mutex_lock(&word_list_mutex);
#endif
    
        //fill a batch (also words longer than MAX_LEN are inserted)
#ifdef HITS_SOURCE_TRACING
        for(ib = 0; ib < BATCH_SIZE && it != wordlist->end(); it++, ib++, it_source++){
            batch_words[ib] = (char*) *it;
            batch_source[ib] = *it_source;
        }
#else
        for(ib = 0; ib < BATCH_SIZE && it != wordlist->end(); it++, ib++){
            batch_words[ib] = (char*) *it;
        }
#endif
        
#ifdef SEMANTIC_RULES
        if(semantic)
            pthread_mutex_unlock(&word_list_mutex);
#endif

        // wordlist is empty
        if(ib == 0){
            end = true;
        }
        

        if(budget < 1){
            // query the model. It applies on BATCH_SIZE input strings even if ib != BATCH_SIZE. Ignore outputs s.t. i>ib in that case
            // we compute compatibility scores even for words longer than MAX_LEN, but we ignore such scores later (rules are always for words longer than MAX_LEN)
            // todo pre allocate comp_scores
            comp_scores = adaptive_inference_batch(ctx, batch_words, BATCH_SIZE);
            _comp_scores = comp_scores.data();
        }

        // guess
        // for each word in the batch
        for(i=0; i<ib; i++){
            
             if(log == LOG_FQ){

                int tg_m = (int) log10(guesses);
                
                if(output_type == 3)
                    fprintf(stdout, "[LOG]: (#guesses: %lu |~10^%d|)\t(recovered: %f%%)\t(efficiency: %f%%)\n", guesses, tg_m, (float) matched / (float) num_target_pass, (float) matched / (float) guesses);


                fprintf(stderr, "[LOG]: (#guesses: %lu |~10^%d|)\t(recovered: %f%%)\t(efficiency: %f%%)\n", guesses, tg_m, (float) matched / (float) num_target_pass, (float) matched / (float) guesses);
                log = 0;
            }

            log += 1;
            
            // get word
            cw = batch_words[i];
            cwl = strlen(cw);
            
            // for each rule (rc = 0 is the identity rule)
            for(rc = 0; rc < rules->rules_cnt+1; rc++){
                
                
                // always perform identity guess
                if(rc > 0){
                
                    // get rule
                    cr = rules->rules_buf[rc-1];
                    crl = rules->rules_len[rc-1];

                    //printf("%f %f\n", _comp_scores[i*rules->rules_cnt+rc], (1-budget));

#ifdef MULTI_TH 
                    assert(false);
#else
                    // check compatibility
                    if(!( budget == 1 || cwl > MAX_LEN || _comp_scores[i*rules->rules_cnt+(rc-1)] >= (1-budget) ))
                        continue;
#endif

                    // apply the mangling rule on the word
                    ret = apply_rule(cr, crl, cw, cwl, out);
                    identity = false;
                }else{
                    sprintf(out, "%s", cw);
                    ret = 0;
                    identity = true;
                }

                if(ret != -1){
                    
                    guesses += 1;
                    
                    // print guesses
                    if(output_type == 4){
                        printf("%s\n", out);
                    }

                    // check match
                    if(target->erase(out)){
                        //matched
                        matched++;
                        
                        // print match
                        if(output_type == 1){
#ifdef HITS_SOURCE_TRACING
                            int source = batch_source[i];
                                
                            if(source == SOURCE_WR && identity)
                                source = SOURCE_ID;
                            else if (source == SOURCE_SEM && identity)
                                source = SOURCE_SEMR;
                                
                            printf("%lu\t%d\t%s\n", guesses, source, out);
#else
                            printf("%lu\t%s\n", guesses, out);
#endif
                        }else if(output_type == 2){
                            printf("%s\n", out);
                        }
                        
                        // dynamic attack
                        if(!identity && dynamic){
                            char *out_ = strdup(out);
                            
#ifdef SEMANTIC_RULES
                            if(semantic)
                                pthread_mutex_lock(&word_list_mutex);
#endif
                            wordlist->push_back(out_);
                            
#ifdef HITS_SOURCE_TRACING
                            source_wordlist.push_back(SOURCE_DYN);
#endif
                            
#ifdef SEMANTIC_RULES
                            if(semantic && is_for_semantic_exp(out_)){
                                pthread_mutex_unlock(&word_list_mutex);

                                // add matched password to the matched list
                                pthread_mutex_lock(&matched_list_mutex);
                                matched_list.push_back(out_);
                                pthread_mutex_unlock(&matched_list_mutex);

                                pthread_cond_signal(&cond_available_in_matched_list);
                            }
#endif
                        }
                    }

                }else{ 
                    fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                    exit(1);
                }
                
                if(guesses >= max_guess){
                    fprintf(stderr,"[LOG] Maximum number of guesses (i.e., %lu) reached!\n", max_guess);
                    goto exit;
                }
           }
        }
    }
    
 
    // handle consumer thread
#ifdef SEMANTIC_RULES
    if(MAIN_ATTACK_STOP == false){
        // check if the consumer has finished
        pthread_mutex_lock(&matched_list_mutex);
        MAIN_ATTACK_STOP = true;
        uint64_t len_matched_list = matched_list.size();
        
        // unlock consumer to finish a not-full batch 
        if(len_matched_list < SEMANTIC_BATCH_SIZE){
            for(uint32_t i=0; i<SEMANTIC_BATCH_SIZE-len_matched_list; i++){
                pthread_cond_signal(&cond_available_in_matched_list);
            }
        }
            
        pthread_mutex_unlock(&matched_list_mutex);

        fprintf(stderr,"[LOG] Remaining matched passwords in the semantic queue: %lu -> %f %%\n", len_matched_list, (float) len_matched_list / (float) matched);
        fprintf(stderr,"[LOG] Processing the queue .....\n");

        // consumer has not finished
        if(len_matched_list > 0){
            pthread_join(thread_id, NULL);
        }
        
        fprintf(stderr,"[LOG] Restarting the main attack loop .....\n");

        goto RERUN_MAIN_ATTACK;
    }
#endif
    
exit:
    
    fprintf(stderr,"[LOG] number of guesses: %lu number of guessed: %lu\n", guesses, matched);
    
    
    return 0;
}