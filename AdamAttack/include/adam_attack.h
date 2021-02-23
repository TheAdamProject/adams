#ifndef __ADAM_ATTACK_HEADER__
#define __ADAM_ATTACK_HEADER__

#include <time.h> 
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
#define SLEEP_T 5
#define MAX_CONNECT_RETRY 12


#define BATCH_SIZE 4096
#define MAX_BUFFER 2048
#define WORD_BATCH_DELIMITER '\t'
#define WORD_BATCH_DELIMITER_STRING "\t"
#define DEFAULT_SERVER_PORT 56780
#define DEFAULT_SERVER_HOST "127.0.0.1"
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


//int runAttackAdaptiveRule(rules_t * rules, words_t *words, char *file_wordlist, char *file_rules, char *file_labelset, bool dynamic, float threshold, int daemon_port, uint64_t max_guess);

int runAttackAdam(rules_t * rules, words_t *words, char *file_hashes, float budget, int daemon_port, FILE *orf, uint64_t max_guess);

int runAttackStandard(rules_t * rules, words_t *words, char *file_hashes, FILE *orf, uint64_t max_guess);
#endif
