#ifndef __HASHCAT_UTILS_HEADER
#define __HASHCAT_UTILS_HEADERH
/**
 * Author......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */


#ifdef FREEBSD
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/ttydefaults.h>
#endif

#ifdef OSX
#include <sys/sysctl.h>
#endif

#define _FILE_OFFSET_BITS 64
#define _CRT_SECURE_NO_WARNINGS

#include <unistd.h>
#include <sys/types.h>
#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"
#include <sys/wait.h>

// for interactive status prompt
#if defined OSX || defined FREEBSD
#include <termios.h>
#include <sys/ioctl.h>
#endif

#if defined LINUX
#include <termio.h>
#endif

#include <string.h>

#define USAGE_VIEW      0
#define VERSION_VIEW    0
#define QUIET           0
#define STDOUT_MODE     0
#define STATUS          0
#define STATUS_TIMER    10
#define STATUS_AUTOMAT  0
#define REMOVE          0
#define POTFILE_DISABLE 0
#define OUTFILE_FORMAT  3
#define OUTFILE_AUTOHEX 1
#define HEX_SALT        0
#define HEX_CHARSET     0
#define RUNTIME         0
#define ATTACK_MODE     0
#define HASH_MODE       0
#define DEBUG_MODE      0
#define NUM_THREADS     8
#define CACHE_SIZE      256
#define WORDS_SKIP      0
#define WORDS_LIMIT     0
#define SEPARATOR       ':'
#define USERNAME        0
#define SHOW            0
#define LEFT            0
#define RP_GEN          0
#define RP_GEN_FUNC_MIN 1
#define RP_GEN_FUNC_MAX 4
#define RP_GEN_SEED     0
#define TOGGLE_MIN      1
#define TOGGLE_MAX      16
#define PERM_MIN        2
#define PERM_MAX        10
#define TABLE_MIN       2
#define TABLE_MAX       15
#define INCREMENT       0
#define INCREMENT_MIN   IN_LEN_MIN
#define INCREMENT_MAX   IN_LEN_MAX
#define PW_MIN          1
#define PW_MAX          16
#define WL_DIST_LEN     0
#define WL_MAX          10000000
#define CASE_PERMUTE    0
#define MIN_FUNCS       1 
#define MAX_FUNCS       16
#define MAX_CUT_ITER    4

#define TIMESPEC_SUBTRACT(a,b) ((uint64_t) ((a).tv_sec - (b).tv_sec) * 1000000 + (a).tv_usec - (b).tv_usec)


#define NUM_DEFAULT_BENCHMARK_ALGORITHMS 72
#define POW_BASE 10


int fgetl (FILE *fp, char *line_buf);
int compare_string (const void *p1, const void *p2);
int compare_stringptr (const void *p1, const void *p2);
int compare_salt_ikepsk (const void *p1, const void *p2);
int compare_salt_netntlm (const void *p1, const void *p2);
char **scan_directory (const char *path);
void incr_rules_buf (rules_t *rules);
void incr_words_buf (words_t *words);
words_t *init_new_words (void);
rules_t *init_new_rules (void);
digest_t *init_new_digest (void);
index_t *init_new_index (void);
salt_t *init_new_salt (void);
db_t *init_new_db (void);
engine_parameter_t *init_new_engine_parameter (void);
void add_word (char *word_pos, uint32_t word_len, words_t *words, engine_parameter_t *engine_parameter);
int add_rule (char *rule_buf, uint32_t rule_len, rules_t *rules);
void load_words (FILE *fp, words_t *words, engine_parameter_t *engine_parameter);

db_t* initBuffers_hashcat();
void loadRulesFromFile(const char* file_rules, rules_t *rules);
void loadWordlist(const char *file_wordlist, words_t *words);
#endif
