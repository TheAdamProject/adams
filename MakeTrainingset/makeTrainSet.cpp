/**
 * Authors......:
 * License.....: MIT
 */

/**
 * Author......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */

#include <math.h> 
#include <iostream>
#include<thread>
#include<vector>

#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"

#include "hashcat_utils.h"

#include "src/utilities.cpp"
#include "src/wordlist.cpp"

#define INFO_FILENAME "info.txt"
#define RULESOUT_FILENAME "rules.txt"
#define XOUT_FILENAME "X.txt"
#define LABELX_FILENAME "hits.bin"
#define LABELR_FILENAME "index_hits.bin"
#define META_FILENAME "meta.bin"

#define LOG_FQ 100000

#define RC_t uint32_t

void makeTrainSet(int rank, const char *output_dir, rules_t *rules, wordlist_t *wordlist, target_t *target, uint64_t *matched_per_th){
    
    fprintf(stderr,"[INFO]: Thread %d starts (%lu)\n", rank, wordlist->size());

    FILE *rules_out, *dict_out, *index_out, *hits_out, *meta_out;
    rules_out = dict_out = index_out = meta_out = hits_out = NULL;
    
    uint64_t guesses, noop_guesses, matched, wc, n_targets, log, rc, cwl, crl;
    guesses = noop_guesses = matched = wc = n_targets = 0;
    log = rc = cwl = crl = 0;
    int ret = 0;
    //cw = cr = NULL;

    // open output files -----------------------------------------------
    char out[BUF_SIZE];
    snprintf(out, BUF_SIZE, "%s/%s_%d", output_dir, INFO_FILENAME, rank);
    
    // open files
    if(rank == 0){
        snprintf(out,BUF_SIZE,"%s/%s",output_dir,RULESOUT_FILENAME);
        rules_out = Fopen(out, "w");
    }
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir,XOUT_FILENAME, rank);
    dict_out = Fopen(out, "w");

    //bins
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir, LABELX_FILENAME, rank);
    hits_out = Fopen(out, "wb");
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir,LABELR_FILENAME, rank);
    index_out = Fopen(out, "wb");
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir,META_FILENAME, rank);
    meta_out = Fopen(out, "wb");
    // ----------------------------------------------------------------


    // run ----------------------------------------------------------------
    RC_t matched_per_word;
    char *cw = NULL, *cr = NULL;
    it_wordlist_t it;
    for(it=wordlist->begin(); it != wordlist->end(); ++it){
        
        matched_per_word = 0;

        if(log == LOG_FQ){
            fprintf(stderr,"[LOG %d]: [%lu] %lf%%\n", rank, wc, (double) wc / (double) wordlist->size());
            log = 0;
        }

        log += 1;

        rc=0;
        cw = (char*)*it;
        cwl = strlen(cw);

        //print dict
        fprintf(dict_out, "%s\n", cw);

        for(rc = 0; rc < rules->rules_cnt; rc++){
            cr = rules->rules_buf[rc];
            crl = rules->rules_len[rc];
            
            ret = apply_rule(cr, crl, cw, cwl, out);

            //print rule
            if(rank == 0 && wc == 0){
                fprintf(rules_out, "%s\n", cr);
            }
            

            if(ret != -1){
                //printf("%s\n", out);
                guesses += 1;

                if(strcmp(cw, out) == 0){
                    // noop rule
                    noop_guesses += 1;
                    continue;
                }

                // check match
                if(target->find(out) != target->end()){
                    //matched
                    matched++;
                    matched_per_word++;
                    //print match rule
                    fwrite(&rc, sizeof(RC_t), 1, hits_out);
                }

            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(1);
            }
        }
        // write number of matched password rule rc
        fwrite(&matched_per_word, sizeof(RC_t), 1, index_out);
        
        wc++;
    } 

    float ratio= (float) noop_guesses / (float) guesses;
    fprintf(stderr, "[LOG %d] \nGuesses: %ld\tNoop_guessed: %ld(%f%%)\tMatched: %ld\n", rank, guesses, noop_guesses, ratio, matched);

    fwrite(&matched, sizeof(uint64_t), 1, meta_out);
    fwrite(&noop_guesses, sizeof(uint64_t), 1, meta_out);
    fwrite(&guesses, sizeof(uint64_t), 1, meta_out);

    if(rank == 0){
        fclose(rules_out);
    }
    
    fclose(dict_out);
    fclose(hits_out);
    fclose(index_out);
    fclose(meta_out);    
    //matched_per_th[rank] = matched;
}

//-_______------______---------_______-------_____-----------______--------____----------_____-------_____--
#define USAGE "USAGE: wordlist rules_set target output_dir num_thread\n"
#define ARGC 6
int main (int argc, char *argv[]){

    setvbuf (stdout, NULL, _IONBF, 0);
    
    if(argc < ARGC){
        printf(USAGE);
        exit(1);
    }
    
    const char *wordlist_path = argv[1];
    const char *rules_path = argv[2];
    const char *target_path = argv[3];
    const char *output_dir = argv[4];
    int n_th = atoi(argv[5]);
    
    Mkdir( (char*)output_dir, DIRPERM);
    
    // init hashcat
    db_t *db = initBuffers_hashcat();
    // load rules
    loadRulesFromFile(rules_path, db->rules);
    
    // wordlist
    fprintf(stderr,"[INFO]: Reading wordlist...\n");
    wordlist_t wordlist =loadWordlistFromFile_list(wordlist_path);
    //loadWordlist(wordlist_path, db->words);
    
    rules_t *rules = db->rules;
    //words_t *words = db->words;
    
    uint64_t tot_guesses = (uint64_t) wordlist.size() * (uint64_t) rules->rules_cnt;
    int tg_m = (int)log10(tot_guesses);
    fprintf(stderr,"[INFO]: Wordcount %lu\n", wordlist.size());
    fprintf(stderr,"[INFO]: Rulecount %lu\n",rules->rules_cnt);
    fprintf(stderr,"[INFO]: TOT GUESSES %lu (10^%d)\n", tot_guesses, tg_m);

    
    fprintf(stderr,"[INFO]: Reading target set...\n");
    target_t target = loadTarget(target_path);
    uint64_t target_size = target.size();
    fprintf(stderr,"[INFO]: Target size %lu\n", target_size);
    
    
    uint64_t *matched_per_th;
    matched_per_th = (uint64_t*) Malloc(n_th*sizeof(uint64_t), "");
    
    if( n_th == 0){
        makeTrainSet(0, output_dir, rules, &wordlist, &target, matched_per_th);
    }else{ 
        // run ============================================
        
        std::vector<std::thread> threads;

        uint64_t words_per_th = wordlist.size() / n_th;
        wordlist_t wordlist_batchs[n_th];
        it_wordlist_t start, stop;
        
        for(int i=0; i < n_th; i++){
            uint64_t shift = words_per_th * i;
            start = wordlist.begin() + shift;
            // last thread
            if(i == n_th-1)
                stop = wordlist.end();
            else
                stop = wordlist.begin() + (shift + words_per_th);
            
            wordlist_batchs[i] = wordlist_t(start, stop);
            threads.push_back(std::thread(makeTrainSet, i, output_dir, rules, &wordlist_batchs[i], &target, matched_per_th));
        }

        for (auto &th : threads) {
            th.join();
        }
        //=================================================
    }
    
    
    /*
    uint64_t matched = 0;
    for(int i=0; i < n_th; i++)
        matched += matched_per_th[i];
    fprintf(stderr,"[INFO]: Total Matched %lu\n", matched);
    */
    free(matched_per_th);
    return 0;
}

