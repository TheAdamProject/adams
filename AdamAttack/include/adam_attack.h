#ifndef __ADAM_ATTACK_HEADER__
#define __ADAM_ATTACK_HEADER__

#include <time.h> 
#include<iostream>
#include<set>
#include<list>
#include<string>
#include<forward_list>
#include <unistd.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"

#include "adaptive_model.h"
#include "hashcat_utils.h"
#include "wordlist.h"

#define MAX_LEN 16
#define SLEEP_T 5
#define MAX_CONNECT_RETRY 12

#define LOG_FQ 10000
#define BATCH_SIZE 4096
#define MAX_BUFFER 256
#define WORD_BATCH_DELIMITER '\t'
#define WORD_BATCH_DELIMITER_STRING "\t"
#define DEBUG 0


#define TIMER_DEF     struct timeval temp_1, temp_2

#define TIMER_START   gettimeofday(&temp_1, (struct timezone*)0)

#define TIMER_STOP    gettimeofday(&temp_2, (struct timezone*)0)

#define TIMER_ELAPSED ((temp_2.tv_sec-temp_1.tv_sec)*1.e6+(temp_2.tv_usec-temp_1 .tv_usec))

#define MT_INCR         1
#define MT_NORM         2
#define MT_NORM_MIN_MAX 3

#define MAX_DELTA 0.1
#define MIN_TH 0.1
#define MAX_TH 0.9


int runAttackAdam(
    model_context *ctx,
    rules_t * rules,
    wordlist_t *wordlist,
    target_t *target,
    float budget,
    bool dynamic,
    bool semantic,
    int output_type,
    uint64_t max_guess);

#endif
