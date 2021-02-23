#include<iostream>
#include<set>
#include<list>
#include<string>
#include<forward_list>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"
#include "set.h"
#include <unistd.h>
 
#define MAX_LEN 16
#define SLEEP_T 1

struct batch{
    char *cw;
    int  cwl;
    float *p;
    int num;
};
typedef struct batch batch;
typedef batch *batchPtr;
//#define BATCH_SIZE 10
#define BATCH_SIZE 4096
#define MAX_BUFFER 2048
#define WORD_BATCH_DELIMITER '\t'
#define WORD_BATCH_DELIMITER_STRING "\t"
#define DEFAULT_SERVER_PORT 56780
#define DEFAULT_SERVER_HOST "127.0.0.1"

#define DEBUG 0


void printSet(set<char *> test){
  set<char *>::iterator it;
	for(it=test.begin(); it!=test.end(); it++){
        printf("LABEL: %s\n", *it);
	}
}

  struct cstrless {
    bool operator()(const char* a, const char* b) {
      return strcmp(a, b) < 0;
    }
  };


int loadLabelSetGetNSets(const char* path, set<char *, cstrless> * st, uint64_t *N){
    
    char termBuf[BUF_SIZE];
    uint64_t i = 0;

    FILE *fin = Fopen(path,"r");
    MyListPtr head = NULL;
    fprintf(stderr,"[INFO]: creating binary tree of the label set (%s) \n", path);
    while( NULL != fgets(termBuf,BUF_SIZE,fin)){
        int n = strlen(termBuf);
        termBuf[n-1] = '\0';
        //printf("LABEL: %s\n", termBuf);
        char * t =strdup((const char*) termBuf);
        st->insert(t);
        //fprintf(stderr,"[DEBUG]: insert %s size %ld\n",termBuf,st->size());
        i++;
    }
    fprintf(stderr,"[INFO]: the binary tree created: %lu\n", i);
    fclose(fin);
    
    *N = i;
   
   return 0;
}

//----------------------//----------------------//----------------------//----------------------//----------------------

int runAttackAdaptiveRule(rules_t * rules, words_t *words, char *file_wordlist, char *file_rules, char *file_labelset, bool dynamic, float threshold, int daemon_port){
    

    uint64_t            guesses;
    uint64_t            noop_guesses;
    uint64_t            matched;
    uint64_t            wc;
    uint64_t            n_targets; 
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
    char                out[BUF_SIZE];
    char                *cw;
    char                *cr;
    char                *data_buf;
    double              ratio;
    MyListPtr             labelset;
    MyListPtr             matched_psswd;
    batchPtr            batchWord;
    bool                loopFlag;
    bool                adam;
    float               TH;
    float               MATCH_RATIO;
    float               *p_matrix;
    FILE                *adam_file;
    size_t              data_len;
    size_t              max_matrix_size;
    size_t              cur_matrix_size;
    ssize_t             bs;
    socklen_t           optlen;
    struct sockaddr_in  addr_in;
    char                buf[MAX_BUFFER];

    guesses = noop_guesses = matched = wc = n_targets = 0;
    k = log = rc = cwl = crl = ret = 0;
    sock = ib = batchWord_count = optval = batchcount = 0;
    cw = cr = NULL;
    data_buf = NULL;
    ratio = 0;
    
    set<char *, cstrless> st_label;
    set<char *, cstrless>::iterator it_label;

    matched_psswd = NULL;
    batchWord = (batchPtr)Calloc(BATCH_SIZE, sizeof(batch),"Calloc batchWord");
    loopFlag=true;
    adam = false;
    MATCH_RATIO = 0;
    TH = threshold;
    p_matrix = NULL;
    adam_file = NULL;
    data_len = max_matrix_size = cur_matrix_size = 0;
    bs = 0;
    optlen = sizeof(optval);

    loadLabelSetGetNSets(file_labelset, &st_label, &n_targets);
    //cout << n_targets <<"\n";
    
    std::forward_list<std::string> word_list;
    for(uint64_t i=words->words_cnt-1; i>=0; i--){
        cw = words->words_buf[i];
        cwl = words->words_len[i];
        if(cwl > MAX_LEN)
            continue;
        cw[cwl] = '\0';
        std::string s(cw);
        word_list.push_front(s);
        
        if(st_label.erase(cw))
            n_targets--;
        
        // i uint
        if(i == 0){
          break;
        }
    }
    //std::forward_list<std::string>::iterator it = word_list.begin();
    
    if ( NULL == rules){
        fprintf(stderr,"[ERROR]: invalid argument rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == words){
        fprintf(stderr,"[ERROR]: invalid argument words\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_wordlist){
        fprintf(stderr,"[ERROR]: invalid argument file_wordlist\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_rules){
        fprintf(stderr,"[ERROR]: invalid argument file_rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_labelset){
        fprintf(stderr,"[ERROR]: invalid argument file_labelset\n");
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

    if(DEBUG)
        fprintf(stderr,"[DEBUG]: try to connect \n");
    
    while(true){
        if( -1 != connect(sock,(struct sockaddr*)&addr_in, sizeof(addr_in))){
            break;
            //fprintf(stderr,"[ERROR]: unable to connect to %s on port %d\n", DEFAULT_SERVER_HOST, daemon_port);
            //return(EXIT_FAILURE);
        }
        fprintf(stderr,"[ERROR]: unable to connect to %s on port %d\n", DEFAULT_SERVER_HOST, daemon_port);
        sleep(SLEEP_T);
    }
    max_matrix_size = BATCH_SIZE * rules->rules_cnt * sizeof(float);
    p_matrix = (float*) Malloc(max_matrix_size,"Malloc matrix");
    batchWord_count=0;
    batchcount=0;
    //---------------------------------------------------------------------------------
    
    
    // main loop
    while(loopFlag){
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
        if(DEBUG){
            fprintf(stderr,"[DEBUG]: batchWord_count %d\n",batchWord_count);
	}
        cur_matrix_size=rules->rules_cnt * sizeof(float) * batchWord_count ;
        
        // SEND_BATCH to Python DAEMON
        data_buf = (char*)batchString.c_str();
        data_len = strlen(data_buf);//izeof(data_buf);
        if(DEBUG){
            fprintf(stderr,"[DEBUG]: Sending [%s] %lu\n",data_buf, strlen(buf)+1);
	    fprintf(stderr,"[DEBUG]: Sending batchcount : %d\n", batchcount);
	}
        bs = send(sock, (void*) &data_len,sizeof(size_t),0);
        bs = send(sock, (void*) data_buf,data_len,0);
        if(bs != data_len){
            fprintf(stderr,"[WARN]: sent only %lu bytes of %lu\n",bs,data_len);
        }
        bs=recv(sock, p_matrix, cur_matrix_size,MSG_WAITALL);
        if(DEBUG){
            fprintf(stderr,"[DEBUG] %zu bytes received\n",bs);
        }
        for(ib = 0; ib < batchWord_count; ib++){
            cw=batchWord[ib].cw;
            cwl=batchWord[ib].cwl;
            //if(DEBUG)
            //    fprintf(stderr,"\n");
            for(rc=0; rc < rules->rules_cnt; rc++){
                if(DEBUG){
		            fprintf(stderr,"%f ", p_matrix[(ib*rules->rules_cnt) + rc]);
	    	    }
                
                // adaptive mangling rule
                if(p_matrix[(ib*rules->rules_cnt) + rc] <= TH){
                   // printf("%f\n", p_matrix[(ib*rules->rules_cnt) + rc]);
                   continue;
                }

                
                cr = rules->rules_buf[rc];
                crl = rules->rules_len[rc];
                ret = apply_rule(cr, crl, cw, cwl, out);

                if(ret != -1){
                    guesses += 1;
                    if (DEBUG){
                        fprintf(stderr,"[GUESS][%s]\n",out);                    
                    }
                    // count noop applications
                    if(strcmp(cw, out) == 0){
                        noop_guesses += 1;
                        continue;
                    }
                    
                    // if matched
                    if(st_label.erase(out)){
                        char *tmpOut=strdup(out);
                        matched++;
                        MATCH_RATIO = (float)matched / (float)n_targets;
                        fprintf(stdout, "%lu\t%lf\t%lu\t%lu\n", guesses, MATCH_RATIO,matched,n_targets);
                        if(dynamic){
                            if(strlen(tmpOut) < MAX_LEN)
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
        if(it == word_list.end()){
            loopFlag = false;
        }
    }
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    ratio = (double) noop_guesses / (double) guesses;
    fprintf(stderr, "NOOP %lf\n", ratio);
    fprintf(stderr, "MATCH %lf\n", MATCH_RATIO);


    return 0;
}


//----------------------//----------------------//----------------------//----------------------//----------------------


int runAttackAdaptiveRuleMT(rules_t * rules, words_t *words, char *file_wordlist, char *file_rules, char *file_labelset, bool dynamic, float threshold, int daemon_port, FILE *orf, int mthm){
    

    uint64_t            tot_guesses;
    uint64_t            guesses;
    uint64_t            noop_guesses;
    uint64_t            matched;
    uint64_t            wc;
    uint64_t            n_targets; 
    int 		*rule_match;
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
    char                out[BUF_SIZE];
    char                *cw;
    char                *cr;
    char                *data_buf;
    double              ratio;
    MyListPtr             labelset;
    MyListPtr             matched_psswd;
    batchPtr            batchWord;
    bool                loopFlag;
    bool                adam;
    float               *TH_ARRAY;
    float               MATCH_RATIO;
    float               *p_matrix;
    FILE                *adam_file;
    size_t              data_len;
    size_t              max_matrix_size;
    size_t              cur_matrix_size;
    ssize_t             bs;
    socklen_t           optlen;
    struct sockaddr_in  addr_in;
    char                buf[MAX_BUFFER];

    tot_guesses = guesses = noop_guesses = matched = wc = n_targets = 0;
    rule_match = NULL;
    k = log = rc = cwl = crl = ret = 0;
    sock = ib = batchWord_count = optval = batchcount = 0;
    cw = cr = NULL;
    data_buf = NULL;
    ratio = 0;
    
    set<char *, cstrless> st_label;
    set<char *, cstrless>::iterator it_label;

    matched_psswd = NULL;
    batchWord = (batchPtr)Calloc(BATCH_SIZE, sizeof(batch),"Calloc batchWord");
    loopFlag=true;
    adam = false;
    TH_ARRAY=NULL;
    MATCH_RATIO = 0;
    p_matrix = NULL;
    adam_file = NULL;
    data_len = max_matrix_size = cur_matrix_size = 0;
    bs = 0;
    optlen = sizeof(optval);

    loadLabelSetGetNSets(file_labelset, &st_label, &n_targets);
    //cout << n_targets <<"\n";
    
    std::forward_list<std::string> word_list;
    for(uint64_t i=words->words_cnt-1; i>=0; i--){
        cw = words->words_buf[i];
        cwl = words->words_len[i];
        if(cwl > MAX_LEN)
            continue;
        cw[cwl] = '\0';
        std::string s(cw);
        word_list.push_front(s);
        
        if(st_label.erase(cw))
            n_targets--;
        
        // i uint
        if(i == 0){
          break;
        }
    }
    //std::forward_list<std::string>::iterator it = word_list.begin();
    
    if ( NULL == rules){
        fprintf(stderr,"[ERROR]: invalid argument rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == words){
        fprintf(stderr,"[ERROR]: invalid argument words\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_wordlist){
        fprintf(stderr,"[ERROR]: invalid argument file_wordlist\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_rules){
        fprintf(stderr,"[ERROR]: invalid argument file_rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_labelset){
        fprintf(stderr,"[ERROR]: invalid argument file_labelset\n");
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

    if(DEBUG)
        fprintf(stderr,"[DEBUG]: try to connect \n");
    
    while(true){
        if( -1 != connect(sock,(struct sockaddr*)&addr_in, sizeof(addr_in))){
            break;
            //fprintf(stderr,"[ERROR]: unable to connect to %s on port %d\n", DEFAULT_SERVER_HOST, daemon_port);
            //return(EXIT_FAILURE);
        }
        fprintf(stderr,"[ERROR]: unable to connect to %s on port %d\n", DEFAULT_SERVER_HOST, daemon_port);
        sleep(SLEEP_T);
    }
    max_matrix_size = BATCH_SIZE * rules->rules_cnt * sizeof(float);
    p_matrix = (float*) Malloc(max_matrix_size,"Malloc matrix");
    batchWord_count=0;
    batchcount=0;
    //---------------------------------------------------------------------------------
    
    // initialize threshold array
    TH_ARRAY = (float*)Malloc(sizeof(float) * rules->rules_cnt,"Malloc TH_ARRAY");
    for(rc=0; rc < rules->rules_cnt; rc++){
    	TH_ARRAY[rc] = threshold;
        //fprintf(orf,"%f\t",TH_ARRAY[rc]);
    }
    //fprintf(orf,"\n");

    // initialize rule_match array
    rule_match = (int*)Malloc(sizeof(int) * rules->rules_cnt,"Malloc rule_match ");

   
    tot_guesses = rules->rules_cnt * words->words_cnt;
    if(dynamic){
        tot_guesses = rules->rules_cnt * (words->words_cnt + n_targets);
    }
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
        if(DEBUG){
            fprintf(stderr,"[DEBUG]: batchWord_count %d\n",batchWord_count);
	}
        cur_matrix_size=rules->rules_cnt * sizeof(float) * batchWord_count ;
        
        // SEND_BATCH to Python DAEMON
        data_buf = (char*)batchString.c_str();
        data_len = strlen(data_buf);//izeof(data_buf);
        if(DEBUG){
            fprintf(stderr,"[DEBUG]: Sending [%s] %lu\n",data_buf, strlen(buf)+1);
	    fprintf(stderr,"[DEBUG]: Sending batchcount : %d\n", batchcount);
	}
        bs = send(sock, (void*) &data_len,sizeof(size_t),0);
        bs = send(sock, (void*) data_buf,data_len,0);
        if(bs != data_len){
            fprintf(stderr,"[WARN]: sent only %lu bytes of %lu\n",bs,data_len);
        }
        bs=recv(sock, p_matrix, cur_matrix_size,MSG_WAITALL);
        if(DEBUG){
            fprintf(stderr,"[DEBUG] %zu bytes received\n",bs);
        }
        for(ib = 0; ib < batchWord_count; ib++){
            cw=batchWord[ib].cw;
            cwl=batchWord[ib].cwl;
            //if(DEBUG)
            //    fprintf(stderr,"\n");
            for(rc=0; rc < rules->rules_cnt; rc++){
                if(DEBUG){
		            fprintf(stderr,"%f ", p_matrix[(ib*rules->rules_cnt) + rc]);
	    	    }
                
                // adaptive mangling rule
                if(p_matrix[(ib*rules->rules_cnt) + rc] <= TH_ARRAY[rc]){
                   // printf("%f\n", p_matrix[(ib*rules->rules_cnt) + rc]);
                   continue;
                }


                cr = rules->rules_buf[rc];
                crl = rules->rules_len[rc];
                ret = apply_rule(cr, crl, cw, cwl, out);

                if(ret != -1){
                    guesses += 1;
                    if (DEBUG){
                        fprintf(stderr,"[GUESS][%s]\n",out);                    
                    }
                    // count noop applications
                    if(strcmp(cw, out) == 0){
                        noop_guesses += 1;
                        continue;
                    }
                    
                    // if matched
                    if(st_label.erase(out)){
                	rule_match[rc]++; 
                        char *tmpOut=strdup(out);
                        matched++;
                        MATCH_RATIO = (float)matched / (float)n_targets;
                        fprintf(stdout, "%lu\t%lf\t%lu\t%lu\n", guesses, MATCH_RATIO,matched,n_targets);
                        if(dynamic){
                            if(strlen(tmpOut) < MAX_LEN){
                                word_list.push_front(tmpOut);
			    }
                        } 
                    }

                } else {
                    fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                    return(EXIT_FAILURE);
                }
            }
            free(batchWord[ib].cw);
        }
    #define MAX_DELTA 0.1
	//double delta_th = MAX_DELTA ;
	double delta_th = (1.0/ (double)(tot_guesses / guesses)) ;
	//double delta_th = (1.0/ (double)(tot_guesses / guesses))/100.0 ;
    if(tot_guesses < guesses){
        delta_th=MAX_DELTA;
    }
	if (DEBUG){
	    fprintf(stderr,"delta:%lf \t tot_guess : %lu \t guess %lu\n",delta_th, tot_guesses, guesses);
	}
   
    #define MT_INCR         1
    #define MT_NORM         2
    #define MT_NORM_MIN_MAX 3
    
    #define MIN_TH 0.1
    #define MAX_TH 0.9

    if(mthm == MT_INCR){
	    for(rc = 0; rc < rules->rules_cnt; rc++){
	    	TH_ARRAY[rc] = TH_ARRAY[rc] - ( (rule_match[rc] / BATCH_SIZE)* delta_th); 
            //fprintf(orf,"%f\t",TH_ARRAY[rc]);
	    }
        //fprintf(orf,"\n");
    } else if ( mthm == MT_NORM){
        double current_sum = 0.0;
        double initial_sum = threshold * 1.0 * rules->rules_cnt;

	    for(rc = 0; rc < rules->rules_cnt; rc++){
	    	TH_ARRAY[rc] = TH_ARRAY[rc] - ( (rule_match[rc] / BATCH_SIZE)* delta_th); 
            current_sum +=TH_ARRAY[rc];
	    }
	    for(rc = 0; rc < rules->rules_cnt; rc++){
	    	TH_ARRAY[rc] = (TH_ARRAY[rc] * initial_sum) /current_sum; 
            if (TH_ARRAY[rc] < 0) { TH_ARRAY[rc] = 0;}
            if (TH_ARRAY[rc] > 1) { TH_ARRAY[rc] = 1;}
            //fprintf(orf,"%f\t",TH_ARRAY[rc]);
	    }
        //fprintf(orf,"\n");
        if(DEBUG){
            current_sum = 0.0;
            for(rc = 0; rc < rules->rules_cnt; rc++){
                current_sum += TH_ARRAY[rc]; 
            }
            fprintf(stderr,"[DEBUG]: check sum after update : %lf\n", current_sum);
        }
    } else if(mthm == MT_NORM_MIN_MAX){
	    double current_sum = 0.0;
	    double initial_sum = threshold * 1.0 * rules->rules_cnt;

	    for(rc = 0; rc < rules->rules_cnt; rc++){
		    if(TH_ARRAY[rc] == MIN_TH || TH_ARRAY[rc] == MAX_TH){ continue;}
		    TH_ARRAY[rc] = TH_ARRAY[rc] - ( (100*rule_match[rc] / BATCH_SIZE)* delta_th); 
		    current_sum +=TH_ARRAY[rc];
	    }
	    for(rc = 0; rc < rules->rules_cnt; rc++){
		    TH_ARRAY[rc] = (TH_ARRAY[rc] * initial_sum) /current_sum; 
		    if (TH_ARRAY[rc] < MIN_TH) { TH_ARRAY[rc] = MIN_TH; }
		    if (TH_ARRAY[rc] > MAX_TH) { TH_ARRAY[rc] = MAX_TH; }
		    //fprintf(orf,"%f\t",TH_ARRAY[rc]);
	    }
	    //fprintf(orf,"\n");
    }
    if(it == word_list.end()){
	    loopFlag = false;
    }
    }
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    ratio = (double) noop_guesses / (double) guesses;
    fprintf(stderr, "NOOP %lf\n", ratio);
    fprintf(stderr, "MATCH %lf\n", MATCH_RATIO);

    free(TH_ARRAY);
    free(rule_match);
    return 0;
}


//----------------------//----------------------//----------------------//----------------------//----------------------
int runAttack(rules_t * rules, words_t *words, char *file_wordlist, char *file_rules, char *file_labelset, bool dynamic){
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
	char                *cw;
	char                *cr;
	char                *data_buf;
	double              ratio;
	MyListPtr           labelset;
	float               MATCH_RATIO;
	size_t              data_len;

	guesses = noop_guesses = matched = wc = n_targets = 0;
	k = rc = cwl = crl = ret = 0;
	cw = cr = NULL;
	data_buf = NULL;
	ratio = 0;

	set<char *, cstrless> st_label;
	set<char *, cstrless>::iterator it_label;

	MATCH_RATIO = 0;

	loadLabelSetGetNSets(file_labelset, &st_label, &n_targets);

	std::forward_list<std::string> word_list;
	for(uint64_t i=words->words_cnt-1; i>=0; i--){
		cw = words->words_buf[i];

		cwl = words->words_len[i];
		if(cwl > MAX_LEN)
			continue;

		cw[cwl] = '\0';
		std::string s(cw);
		word_list.push_front(s);

		if(st_label.erase(cw))
			n_targets--;

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

    if ( NULL == file_wordlist){
        fprintf(stderr,"[ERROR]: invalid argument file_wordlist\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_rules){
        fprintf(stderr,"[ERROR]: invalid argument file_rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_labelset){
        fprintf(stderr,"[ERROR]: invalid argument file_labelset\n");
        return (EXIT_FAILURE);
    }

        
    for(wc=0; it != word_list.end(); ++it, wc++){
     
        rc = 0;
        
        cw = (char*)(*it).c_str();
        cwl = (*it).size();
        
        //printf("%s\n", cw);

        for(rc = 0; rc < rules->rules_cnt; rc++){
           
            cr = rules->rules_buf[rc];
            crl = rules->rules_len[rc];
            ret = apply_rule(cr, crl, cw, cwl, out);

            if(ret != -1){
                guesses += 1;

                if(DEBUG){
                    fprintf(stderr,"[GUESS][%s]\n",out);                    
                }
                if(strcmp(cw, out) == 0){
                    // noop rule
                    noop_guesses += 1;
                    continue;
                }

                // check match
                if(st_label.erase(out)){
                    char *tmpOut=strdup(out);
                    matched++;
                    MATCH_RATIO = (float)matched / (float)n_targets;
		    fprintf(stdout, "%lu\t%lf\t%lu\n", guesses, MATCH_RATIO,matched);
                    if(dynamic){
                        if(strlen(tmpOut) < MAX_LEN)
                            word_list.insert_after(it, tmpOut);
                    }
                }
                 
            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(1);
            }
        }
    }

    ratio = (double) noop_guesses / (double) guesses;
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    fprintf(stderr, "NOOP %lf\n", ratio);
    fprintf(stderr, "MATCH %lf\n", MATCH_RATIO);
        
    return 0;
}



int runAttack_rule_major(rules_t * rules, words_t *words, char *file_wordlist, char *file_rules, char *file_labelset, bool dynamic){
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
	char                *cw;
	char                *cr;
	char                *data_buf;
	double              ratio;
	MyListPtr           labelset;
	float               MATCH_RATIO;
	size_t              data_len;

	guesses = noop_guesses = matched = wc = n_targets = 0;
	k = rc = cwl = crl = ret = 0;
	cw = cr = NULL;
	data_buf = NULL;
	ratio = 0;

	set<char *, cstrless> st_label;
	set<char *, cstrless>::iterator it_label;

	MATCH_RATIO = 0;

	loadLabelSetGetNSets(file_labelset, &st_label, &n_targets);

	std::forward_list<std::string> word_list;
	for(uint64_t i=words->words_cnt-1; i>=0; i--){
		cw = words->words_buf[i];

		cwl = words->words_len[i];
		if(cwl > MAX_LEN)
			continue;

		cw[cwl] = '\0';
		std::string s(cw);
		word_list.push_front(s);

		if(st_label.erase(cw))
			n_targets--;

		// i uint
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

    if ( NULL == file_wordlist){
        fprintf(stderr,"[ERROR]: invalid argument file_wordlist\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_rules){
        fprintf(stderr,"[ERROR]: invalid argument file_rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_labelset){
        fprintf(stderr,"[ERROR]: invalid argument file_labelset\n");
        return (EXIT_FAILURE);
    }
        
    
     for(rc = 0; rc < rules->rules_cnt; rc++){
           
        cr = rules->rules_buf[rc];
        crl = rules->rules_len[rc];
         
        std::forward_list<std::string>::iterator it = word_list.begin();
     
        for(wc=0; it != word_list.end(); ++it, wc++){

            cw = (char*)(*it).c_str();
            cwl = (*it).size();
            
            ret = apply_rule(cr, crl, cw, cwl, out); 

            //printf("%s\n", cw);


            if(ret != -1){
                guesses += 1;

                if(DEBUG){
                    fprintf(stderr,"[GUESS][%s]\n",out);                    
                }
                if(strcmp(cw, out) == 0){
                    // noop rule
                    noop_guesses += 1;
                    continue;
                }

                // check match
                if(st_label.erase(out)){
                    char *tmpOut=strdup(out);
                    matched++;
                    MATCH_RATIO = (float)matched / (float)n_targets;
		    fprintf(stdout, "%lu\t%lf\t%lu\n", guesses, MATCH_RATIO,matched);
                    if(dynamic){
                        if(strlen(tmpOut) < MAX_LEN)
                            word_list.insert_after(it, tmpOut);
                    }
                }
                 
            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(1);
            }
        }
    }

    ratio = (double) noop_guesses / (double) guesses;
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    fprintf(stderr, "NOOP %lf\n", ratio);
    fprintf(stderr, "MATCH %lf\n", MATCH_RATIO);
        
    return 0;
}

/*

int runAttackOutpuTree(rules_t * rules, words_t *words, char *file_wordlist, char *file_rules, char *file_labelset, bool dynamic){
    
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
    char                *cw;
    char                *cr;
    char                *data_buf;
    double              ratio;
    MyListPtr           labelset;
    float               MATCH_RATIO;
    size_t              data_len;

    guesses = noop_guesses = matched = wc = n_targets = 0;
    k = rc = cwl = crl = ret = 0;
    cw = cr = NULL;
    data_buf = NULL;
    ratio = 0;
    
    set<char *, cstrless> st_label;
    set<char *, cstrless>::iterator it_label;

    MATCH_RATIO = 0;

    loadLabelSetGetNSets(file_labelset, &st_label, &n_targets);
    
    std::forward_list<std::string> word_list;
    for(uint64_t i=words->words_cnt-1; i>=0; i--){
        cw = words->words_buf[i];

        cwl = words->words_len[i];
        if(cwl > MAX_LEN)
            continue;
        
        cw[cwl] = '\0';
        std::string s(cw);
        word_list.push_front(s);
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

    if ( NULL == file_wordlist){
        fprintf(stderr,"[ERROR]: invalid argument file_wordlist\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_rules){
        fprintf(stderr,"[ERROR]: invalid argument file_rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == file_labelset){
        fprintf(stderr,"[ERROR]: invalid argument file_labelset\n");
        return (EXIT_FAILURE);
    }

        
    for(wc=0; it != word_list.end(); ++it, wc++){
     
        rc = 0;
        
        cw = (char*)(*it).c_str();
        cwl = (*it).size();
        
        //printf("%s\n", cw);

        for(rc = 0; rc < rules->rules_cnt; rc++){
           
            cr = rules->rules_buf[rc];
            crl = rules->rules_len[rc];
            ret = apply_rule(cr, crl, cw, cwl, out);

            if(ret != -1){
              != -1){
                guesses += 1;
                if(DEBUG){
                    fprintf(stderr,"[GUESS][%s]\n",out);                    
                }
                if(strcmp(cw, out) == 0){
                    // noop rule
                    noop_guesses += 1;
                    continue;
                }
                // check match
                if(st_label.erase(out)){
                    char *tmpOut=strdup(out);
                    matched++;
                    MATCH_RATIO = (float)matched / (float)n_targets;
                    fprintf(stderr, "%lu\t%lf\n", guesses, MATCH_RATIO);
                    if(dynamic){
                        if(strlen(tmpOut) < MAX_LEN)
                            word_list.insert_after(it, tmpOut);
                    }
                    fprintf(stdout, "%s\t%s\t%s\n", cw, tmpOut, cr);
                }
                 
            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(1);
            }
        }
    }
    ratio = (double) noop_guesses / (double) guesses;
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    fprintf(stderr, "NOOP %lf\n", ratio);
        
    return 0;
}
runAttack.cpp
   guesses += 1;

                if(DEBUG){
                    fprintf(stderr,"[GUESS][%s]\n",out);                    
                }
                if(strcmp(cw, out) == 0){
                    // noop rule
                    noop_guesses += 1;
                    continue;
                }

                // check match
                if(st_label.erase(out)){
                    char *tmpOut=strdup(out);
                    matched++;
                    MATCH_RATIO = (float)matched / (float)n_targets;
                    fprintf(stderr, "%lu\t%lf\n", guesses, MATCH_RATIO);
                    if(dynamic){
                        if(strlen(tmpOut) < MAX_LEN)
                            word_list.insert_after(it, tmpOut);
                    }
                    fprintf(stdout, "%s\t%s\t%s\n", cw, tmpOut, cr);
                }
                 
            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(1);
            }
        }
    }

    ratio = (double) noop_guesses / (double) guesses;
    fprintf(stderr, "\n%ld %ld %ld\n", guesses, noop_guesses, matched);
    fprintf(stderr, "NOOP %lf\n", ratio);
        
    return 0;
}

*/
