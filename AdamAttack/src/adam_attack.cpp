#include "adam_attack.h"


//#define REMOVE_IDENTITY
//#define MULTI_TH
#define VERBOSE 1
#define LOG_FQ 10000000

struct batch{
    char *cw;
    int  cwl;
    float *p;
    int num;
};
typedef struct batch batch;
typedef batch *batchPtr;

//----------------------//----------------------//----------------------//----------------------//----------------------
void printSet(set<char *> test){
  set<char *>::iterator it;
	for(it=test.begin(); it!=test.end(); it++){
        fprintf(stderr, "[INFO]: LABEL: %s\n", *it);
	}
}

//----------------------//----------------------//----------------------//----------------------//----------------------
struct cstrless {
    bool operator()(const char* a, const char* b) {
        return strcmp(a, b) < 0;
    }
};

//----------------------//----------------------//----------------------//----------------------//----------------------

int loadLabelSetGetNSets(const char* path, set<char *, cstrless> * st, uint64_t *N){

    int n;
    char *t;
    char termBuf[BUF_SIZE];
    uint64_t i = 0;

    FILE *fin = Fopen(path,"r");
    fprintf(stderr,"[INFO]: creating binary tree of the label set (%s) \n", path);
    while( NULL != fgets(termBuf,BUF_SIZE,fin)){
        n = strlen(termBuf);
        termBuf[n-1] = '\0';
        t =strdup((const char*) termBuf);
        st->insert(t);
        i++;
    }
    fprintf(stderr,"[INFO]: the binary tree created: %lu\n", i);
    fclose(fin);

    *N = i;

    return 0;
}


int runAttackAdam(rules_t * rules, words_t *words, char *file_hashes, float budget, int daemon_port, FILE *ogf, uint64_t max_guess){
    

    uint64_t            tot_guesses;
    uint64_t            guesses;
    uint64_t            noop_guesses;
    uint64_t            matched;
    uint64_t            wc;
    uint64_t            n_targets; 
    int 		        *rule_match;
    int                 k;
    int                 log;
    int                 rc;
    int                 cwl;
    int                 crl;
    int                 ret;
    int                 sock;
    int                 ib;
    int                 batchWord_count;
    int                 optval;
    int                 batchcount;
    int                 connect_retries;
    char                out[BUF_SIZE];
    char                *cw;
    char                *cr;
    char                *data_buf;
    batchPtr            batchWord;
    bool                loopFlag;
    bool                adam;
    bool                max_guess_flag;
    float               *th_array;
    float               match_ratio;
    float               *p_matrix;
    FILE                *adam_file;
    size_t              data_len;
    size_t              max_matrix_size;
    size_t              cur_matrix_size;
    ssize_t             bs;
    socklen_t           optlen;
    struct sockaddr_in  addr_in;
    char                buf[MAX_BUFFER];

    double              ratio;
    double              current_sum;
    double              initial_sum;
    
#if VERBOSE >= 1
    time_t ttime = time(NULL); 
#endif

    tot_guesses = guesses = noop_guesses = matched = wc = n_targets = 0;
    rule_match = NULL;
    k = log = rc = cwl = crl = ret = 0;
    sock = ib = batchWord_count = optval = batchcount = connect_retries = 0;
    cw = cr = NULL;
    data_buf = NULL;
    ratio = current_sum = 0.0;
    initial_sum = (1.0 - budget) * rules->rules_cnt;
    //initial_sum = threshold * 1.0 * rules->rules_cnt;
    
    set<char *, cstrless> st_label;
    set<char *, cstrless>::iterator it_label;

    batchWord = (batchPtr)Calloc(BATCH_SIZE, sizeof(batch),"Calloc batchWord");
    loopFlag = true;
    adam = false;
    max_guess_flag = true;
    th_array = NULL;
    match_ratio = 0;
    p_matrix = NULL;
    adam_file = NULL;
    data_len = max_matrix_size = cur_matrix_size = 0;
    bs = 0;
    optlen = sizeof(optval);

    loadLabelSetGetNSets(file_hashes, &st_label, &n_targets);
    std::forward_list<std::string> word_list;
    for(uint64_t i=words->words_cnt-1; i>=0; i--){
        cw = words->words_buf[i];
        cwl = words->words_len[i];
        if(cwl > MAX_LEN){ 
            continue;
        }
        cw[cwl] = '\0';
        std::string s(cw);
        word_list.push_front(s);
    
#ifdef REMOVE_IDENTITY
        if(st_label.erase(cw)){
            n_targets--;
        }
#endif
        if(i == 0){
          break;
        }
    }
    
    if ( NULL == rules){
        fprintf(stderr,"[ERROR]: invalid argument rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == words){
        fprintf(stderr,"[ERROR]: invalid argument words\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_hashes){
        fprintf(stderr,"[ERROR]: invalid argument file_hashes\n");
        return (EXIT_FAILURE);
    }

    
    // SETUP SOCKET FOR BATCH
    
    addr_in.sin_family=AF_INET;
    addr_in.sin_port=htons(daemon_port);
    inet_pton(addr_in.sin_family, DEFAULT_SERVER_HOST, &addr_in.sin_addr.s_addr);
    sock = socket(addr_in.sin_family, SOCK_STREAM,0);
    if( sock == -1){
        fprintf(stderr,"[ERROR]: unable to create socket\n");
        return (EXIT_FAILURE);
    }
    /* Check the status for the keepalive option */
    if(getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
        fprintf(stderr,"[ERROR]: unable to check KEEPALIVE option on socket\n");
        close(sock);
        return(EXIT_FAILURE);
    }

    if( !optval){
        /* Set the option active */
        optval = 1;
        optlen = sizeof(optval);
        if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
            fprintf(stderr,"[ERROR]: unable to set KEEPALIVE option on socket\n");
            close(sock);
            return(EXIT_FAILURE);
        }
    }

    while(true){
        if( -1 != connect(sock,(struct sockaddr*)&addr_in, sizeof(addr_in))){
            break;
        }
        fprintf(stderr,"[WARN]: unable to connect to %s on port %d\n", DEFAULT_SERVER_HOST, daemon_port);
        connect_retries++;
        sleep(SLEEP_T);
        if(connect_retries >= MAX_CONNECT_RETRY){
            fprintf(stderr,"[ERROR]: unable to connect to %s on port %d after %d retries. Shutting down the application\n",DEFAULT_SERVER_HOST, daemon_port, connect_retries);
            exit(EXIT_FAILURE);
        }
    }
    max_matrix_size = BATCH_SIZE * rules->rules_cnt * sizeof(float);
    p_matrix = (float*) Malloc(max_matrix_size,"Malloc matrix");
    batchWord_count=0;
    batchcount=0;
    //---------------------------------------------------------------------------------
    
    
#ifdef MULTI_TH    
    // initialize threshold array
    th_array = (float*)Malloc(sizeof(float) * rules->rules_cnt,"Malloc th_array");
    for(rc=0; rc < rules->rules_cnt; rc++){
    	th_array[rc] = 1.0 - budget;
    }
#endif


    // initialize rule_match array
    rule_match = (int*)Malloc(sizeof(int) * rules->rules_cnt,"Malloc rule_match ");

   
    //tot_guesses = rules->rules_cnt * words->words_cnt;
    //if(dynamic){
    //    tot_guesses = rules->rules_cnt * (words->words_cnt + n_targets);
    //}

    uint64_t dict_dyn_size = 0;

    tot_guesses = rules->rules_cnt * (words->words_cnt + n_targets);
   
    TIMER_DEF;
    TIMER_START;
    // main loop
    while(loopFlag){
    	memset(rule_match,0, sizeof(int)*rules->rules_cnt);
        std::string batchString;
        batchcount++;
        std::forward_list<std::string>::iterator it = word_list.begin();
        for(ib = 0; ib < BATCH_SIZE && it != word_list.end() ; it++, ib++){
            batchWord[ib].cw  = strdup((char*)(*it).c_str());
            batchWord[ib].cwl = (*it).size();
            batchString.append(batchWord[ib].cw);
            batchString.append(WORD_BATCH_DELIMITER_STRING);
        }
        batchString.pop_back();
        batchString.append("\0");
        batchWord_count=ib;
        for(ib = 0; ib < batchWord_count; ib++){
            word_list.pop_front();
        }
        cur_matrix_size=rules->rules_cnt * sizeof(float) * batchWord_count ;

        // SEND_BATCH to Python DAEMON
        data_buf = (char*)batchString.c_str();
        data_len = strlen(data_buf);

        bs = send(sock, (void*) &data_len,sizeof(size_t),0);
        bs = send(sock, (void*) data_buf,data_len,0);
        bs=recv(sock, p_matrix, cur_matrix_size,MSG_WAITALL);
        
        for(ib = 0; max_guess_flag && ib < batchWord_count; ib++){
            
            dict_dyn_size++;
            
            cw=batchWord[ib].cw;
            cwl=batchWord[ib].cwl;
            for(rc=0; max_guess_flag && rc < rules->rules_cnt; rc++){
                // adaptive mangling rule
                
#ifdef MULTI_TH    
                if(p_matrix[(ib*rules->rules_cnt) + rc] <= th_array[rc]){
                    continue;
                }
#else
                
                if(p_matrix[(ib*rules->rules_cnt) + rc] <= (1-budget)){
                    continue;
                }
#endif
                cr = rules->rules_buf[rc];
                crl = rules->rules_len[rc];
                ret = apply_rule(cr, crl, cw, cwl, out);

                if(ret != -1){
                    
#if VERBOSE == 1
                    if(guesses % LOG_FQ == 0){
                        match_ratio = (float)matched / (float)n_targets;
                        fprintf(stderr, "[LOG] #Guesses: %lu\t Matched: %f\n", guesses, match_ratio);
                    }
#endif
                    guesses += 1;
                    if(guesses >= max_guess){
                        max_guess_flag = false;
                    }

#ifdef REMOVE_IDENTITY
                    // count noop applications
                    if(strcmp(cw, out) == 0){
                        noop_guesses += 1;
                        continue;
                    }
#endif
                    // if matched
                    if(st_label.erase(out)){
                        rule_match[rc]++; 
                        //fprintf(stderr,"\n[MATCH][%s]\n",out);                    
                        char *tmpOut=strdup(out);
                        matched++;
#ifndef BENCHMARK
    #if PRINT_TYPE == 0
                        match_ratio = (float)matched / (float)n_targets;
                        fprintf(ogf, "%lu\t%lf\t%lu\t%lu\n", guesses, match_ratio,matched,n_targets);
    #elif PRINT_TYPE == 1
                        fprintf(ogf, "%s\n", tmpOut);
    #elif PRINT_TYPE == 2
                        
    #endif
#endif
                        //if(dynamic){
                        //    if(strlen(tmpOut) < MAX_LEN){
                        //        word_list.push_front(tmpOut);
                        //    }
                        //} 
                        if(strlen(tmpOut) < MAX_LEN){
                            word_list.push_front(tmpOut);
                        }
                    }

                } else {
                    fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                    return(EXIT_FAILURE);
                }
            }
            free(batchWord[ib].cw);
        }
        if(!max_guess_flag){
            break;
        }

#ifdef MULTI_TH    
    double delta_th = (1.0 / (double)(tot_guesses / guesses)) ;
    if(tot_guesses < guesses){
        delta_th=MAX_DELTA;
    }
    for(rc = 0; rc < rules->rules_cnt; rc++){
        if(th_array[rc] == MIN_TH || th_array[rc] == MAX_TH){ continue;}
        th_array[rc] = th_array[rc] - ( (100*rule_match[rc] / BATCH_SIZE)* delta_th); 
        current_sum +=th_array[rc];
    }
    for(rc = 0; rc < rules->rules_cnt; rc++){
        th_array[rc] = (th_array[rc] * initial_sum) / current_sum; 
        if (th_array[rc] < MIN_TH) { th_array[rc] = MIN_TH; }
        if (th_array[rc] > MAX_TH) { th_array[rc] = MAX_TH; }
    }
#endif
        
    if(it == word_list.end()){
        loopFlag = false;
    }
    }
    TIMER_STOP;

    fprintf(stderr,"Execution time: %f \n", (TIMER_ELAPSED / 1000000.0));
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    ratio = (double) noop_guesses / (double) guesses;
    match_ratio = (float)matched / (float)n_targets;
#ifdef REMOVE_IDENTITY 
    fprintf(stderr, "NOOP %lf\n", ratio);
#endif
    fprintf(stderr, "MATCH %lf\n", match_ratio);
    
    fprintf(stderr, "COMP FACTOR %lf\n", (double) guesses / (double) (dict_dyn_size*rules->rules_cnt) );

#ifdef MULTI_TH 
    free(th_array);
#endif
    free(rule_match);
    return 0;
}


//----------------------//----------------------//----------------------//----------------------//----------------------
int runAttackStandard(rules_t *rules, words_t *words, char *file_hashes, FILE * ogf, uint64_t max_guess){
    uint64_t            guesses;
    uint64_t            noop_guesses;
    uint64_t            matched;
    uint64_t            wc;
    uint64_t            n_targets; 
    char                out[BUF_SIZE];
    int                 k;
    int                 rc;
    int                 cwl;
    int                 crl;
    int                 ret;
    int                 sock;
    int                 ib;
    bool                max_guess_flag;
    char                *cw;
    char                *cr;
    char                *data_buf;
    double              ratio;
    float               match_ratio;
    size_t              data_len;

    std::forward_list<std::string> word_list;

    set<char *, cstrless> st_label;
    set<char *, cstrless>::iterator it_label;

    guesses = noop_guesses = matched = wc = n_targets = 0;
    k = rc = cwl = crl = ret = sock = ib = 0;
    max_guess_flag = true;
    cw = cr = NULL;
    data_buf = NULL;
    ratio = 0;

    match_ratio = 0;

    loadLabelSetGetNSets(file_hashes, &st_label, &n_targets);

    for(uint64_t i=words->words_cnt-1; i>=0; i--){
        cw = words->words_buf[i];

        cwl = words->words_len[i];
        if(cwl > MAX_LEN)
            continue;

        cw[cwl] = '\0';
        std::string s(cw);
        word_list.push_front(s);

#ifdef REMOVE_IDENTITY
        if(st_label.erase(cw)){
            n_targets--;
        }
#endif

        // i uint
        if(i == 0){
            break;
        }
    }
    std::forward_list<std::string>::iterator it = word_list.begin();

    if ( NULL == rules){
        fprintf(stderr,"[ERROR]: invalid argument rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == words){
        fprintf(stderr,"[ERROR]: invalid argument words\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_hashes){
        fprintf(stderr,"[ERROR]: invalid argument file_hashes\n");
        return (EXIT_FAILURE);
    }

    
    TIMER_DEF;
    TIMER_START;
    for(wc=0; max_guess_flag && it != word_list.end(); ++it, wc++){

        rc = 0;
        
        cw = (char*)(*it).c_str();
        cwl = (*it).size();
        
        for(rc = 0; max_guess_flag && rc < rules->rules_cnt; rc++){
            cr = rules->rules_buf[rc];
            crl = rules->rules_len[rc];
            ret = apply_rule(cr, crl, cw, cwl, out);

            if(ret != -1){                
#if VERBOSE == 1
                if(guesses % LOG_FQ == 0){
                    match_ratio = (float)matched / (float)n_targets;
                    fprintf(stderr, "[LOG] #Guesses: %lu\t Matched: %f\n", guesses, match_ratio);
                }
#endif
                
                guesses += 1;


                if( guesses >= max_guess){
                    max_guess_flag=false;
                }
   
#ifdef REMOVE_IDENTITY
                // count noop applications
                if(strcmp(cw, out) == 0){
                    noop_guesses += 1;
                    continue;
                }
#endif

                // check match
                if(st_label.erase(out)){
                    char *tmpOut=strdup(out);
#ifndef BENCHMARK
    #if PRINT_TYPE == 0
                    match_ratio = (float)matched / (float)n_targets;
                    fprintf(ogf, "%lu\t%lf\t%lu\t%lu\n", guesses, match_ratio,matched,n_targets);
    #elif PRINT_TYPE == 1
                    fprintf(ogf, "%s\n", tmpOut);
    #endif
#endif                   
                    matched++;
                }
                 
            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(EXIT_FAILURE);
            }
        }
    }

    TIMER_STOP;
    fprintf(stderr,"Execution time : %f \n", (TIMER_ELAPSED / 1000000.0));
    ratio = (double) noop_guesses / (double) guesses;
    match_ratio = (float)matched / (float)n_targets;
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
#ifdef REMOVE_IDENTITY 
    fprintf(stderr, "NOOP %lf\n", ratio);
#endif
    fprintf(stderr, "MATCH %lf\n", match_ratio);
        
    return 0;
}
