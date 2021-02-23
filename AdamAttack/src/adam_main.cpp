/**
 * Authors......:
 * License.....: MIT
 */

/**
 * Author......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */

#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"
#include "adam_attack.h"
#include "utilities.h"

#include "hashcat_utils.h"

#define DAEMON_ARGC 5
#define DAEMON_BASH_IDX 0
#define DAEMON_NAME_IDX 0
#define DAEMON_MODEL_IDX 1
#define DAEMON_CHARMAP_IDX 2
#define DAEMON_PORT_IDX 3

#define INFO_FILENAME "info.txt"
#define RULESOUT_FILENAME "rules.txt"
#define XOUT_FILENAME "X.txt"
#define LABELX_FILENAME "labelX.bin"
#define LABELR_FILENAME "labelR.bin"
#define META_FILENAME "meta.bin"

#define MAX_CMD_LEN 2048
#define ADAM_ATTACK_MODE 9
#define DEFAULT_DAEMON_PORT 12358

#define USAGE "Usage: \n"\
"-r RULES-FILE\n"\
"-w WORDLIST-FILE\n"\
"--hashes-file HASH-FILE\n"\
"[--output-guessed-file OUT-GUESSES-FILE (default stdout)]\n"\
"[--max-guesses-pow MAX-GUESSES (10^MAX-GUESSES)]\n"\
"[-a ATTACK-MODE {0=standard, 9=adams} (default=adams)]\n"\
"\nif adams:\n"\
"\t--config-dir DIR-PATH\n"\
"\t[--daemon-port PORT (default 12358)]\n"\
"\t[--model-path MODEL-PATH]\n"\
"\t[--budget BUDGET]\n"\
"The option --config-dir specifies the directory containg the model, the rules, and the budget.\n"\
"The options -r, --model-path, and --budget will overwrite the --config-dir values.\n"\
"If -r,--model-path, and --budget are all provided by the cmdline, --config-dir can be omitted\n"
int main (int argc, char *argv[]){


  setvbuf (stdout, NULL, _IONBF, 0);

  uint32_t usage_view       = USAGE_VIEW;
  uint32_t version_view     = VERSION_VIEW;
  uint32_t cache_size       = CACHE_SIZE;
  uint32_t attack_mode      = ADAM_ATTACK_MODE;
  uint32_t daemon_port      = DEFAULT_DAEMON_PORT;
  uint32_t max_guess_pow    = 0;
  uint64_t max_guesses      = -1;
  uint64_t TOT_GUESSES      = 0;
  char    *file_rules       = NULL;
  char    *file_wordlist    = NULL;
  char    *file_hashes    = NULL;
  char    *output_dir       = NULL;
  char    *daemon_path      = NULL;
  char    *config_dir       = NULL;
  char    *out_guess_path   = NULL;
  char    *model_path       = NULL;
  char    rule_buf[BUFSIZ];
  char    cmd[MAX_CMD_LEN];
  pid_t   daemon_pid        = 0;
  float   budget            = 0.0;
  FILE    *out_guess_file    = NULL;
  FILE    *fp               = NULL;
  int      status           = 0;
  int      rule_len         = 0;
  int      rc               = 0;
  int      c                = 0;
  int      option_index     = 0;
  /*
   * preinit: getopt
   */

  #define IDX_HELP            'h'
  #define IDX_VERSION         'V'
  #define IDX_RULES_FILE      'r'
  #define IDX_WORDLIST_FILE   'w'
  #define IDX_ATTACK_MODE     'a'
  #define IDX_HASH_FILE   9001
  #define IDX_DYNAMIC         9002
  #define IDX_DAEMON_PORT     9003
  #define IDX_ADR             9004
  #define IDX_MODEL_PATH      9005
  #define IDX_ADAM_BUDG       9006
  #define IDX_OUT_G_PATH      9007
  #define IDX_CONFIG_DIR    9008
  #define IDX_MAX_GUESS       9009

  struct option long_options[] =
  {
    {"help",            no_argument,       0, IDX_HELP},
    {"version",         no_argument,       0, IDX_VERSION},
    {"rules-file",      required_argument, 0, IDX_RULES_FILE},
    {"wordlist-file",   required_argument, 0, IDX_WORDLIST_FILE},
    {"attack-mode",     required_argument, 0, IDX_ATTACK_MODE},
    {"hashes-file",     required_argument, 0, IDX_HASH_FILE},
    {"daemon-port",     required_argument, 0, IDX_DAEMON_PORT},
    {"config-dir",      required_argument, 0, IDX_CONFIG_DIR},
    {"model-path",      required_argument, 0, IDX_MODEL_PATH},
    {"output-guessed-file",required_argument, 0, IDX_OUT_G_PATH},
    {"budget",         required_argument, 0,  IDX_ADAM_BUDG},
    {"max-guesses-pow", required_argument, 0, IDX_MAX_GUESS},
    {0, 0, 0, 0}
  };


  while ((c = getopt_long (argc, argv, "hVr:w:l:p:a:m:o:t:g:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
      case IDX_HELP:            usage_view         = 1;               break;
      case IDX_VERSION:         version_view       = 1;               break;
      case IDX_RULES_FILE:      file_rules         = strdup(optarg);  break;
      case IDX_WORDLIST_FILE:   file_wordlist      = strdup(optarg);  break;
      case IDX_HASH_FILE:       file_hashes        = strdup(optarg);  break;
      case IDX_ATTACK_MODE:     attack_mode        = atoi(optarg);    break; 
      case IDX_DAEMON_PORT:     daemon_port        = atoi(optarg);    break;
      case IDX_MODEL_PATH:      model_path         = strdup(optarg);  break;
      case IDX_CONFIG_DIR:      config_dir         = strdup(optarg);  break;
      case IDX_OUT_G_PATH:      out_guess_path     = strdup(optarg);  break;
      case IDX_ADAM_BUDG:       budget             = atof(optarg);    break;
      case IDX_MAX_GUESS:       max_guess_pow      = atoi(optarg);    break;
      default: exit (EXIT_FAILURE);
    }
  }


  if(usage_view || version_view){
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_SUCCESS);
  }


  if( NULL == file_wordlist){
	fprintf(stderr,"[ERROR]: wordlist-file is mandatory\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
  }

  if( NULL == file_hashes){
	fprintf(stderr,"[ERROR]: hashes-file is mandatory\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
  }

  out_guess_file = stdout;
  if ( out_guess_path ){
    out_guess_file = Fopen(out_guess_path,"w+");
  }

  if(max_guess_pow > 0){
    max_guesses=POW_BASE;
    for(int i=1 ; i < max_guess_pow; i++){
        max_guesses *= POW_BASE;
    }
  }
  if( NULL == config_dir){
    if( (NULL == model_path) || ( NULL == file_rules) || ( 0.0 == budget)){
        fprintf(stderr,"[ERROR]: Only if -r, --model-path, and --budget are provided the option --config-dir can be omitted\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
    }
  }else {
     if(budget){
        float _budget;
        getModelFromDir(config_dir, &model_path, &file_rules, &_budget);
     }else{
        getModelFromDir(config_dir, &model_path, &file_rules, &budget);
     }
  }
  //printf("%s %s %f\n", model_path, file_rules, budget);

   /*
   * init main buffers
   */

  engine_parameter_t *engine_parameter = init_new_engine_parameter ();
  engine_parameter->hash_type      = HASH_TYPE_PLAIN;
  engine_parameter->salt_type      = SALT_TYPE_NONE;
  engine_parameter->plain_size_max = PLAIN_SIZE_PLAIN;


  rules_t *rules = init_new_rules ();

  words_t *words = init_new_words ();

  words->cache_avail = cache_size * 1024 * 1024;

  words->cache_buf = (char*)mymalloc (words->cache_avail);

  db_t *db = init_new_db ();

  db->rules = rules;

  db->words = words;

  /*
   * rules
   */

  if (file_rules != NULL){
      if ((fp = fopen (file_rules, "rb")) != NULL){
	  while ((rule_len = fgetl (fp, rule_buf)) != -1){
          if (rule_len == 0){
	      continue;
	  }
	  if (rule_buf[0] == '#'){
	      continue;
	  }
	  rc = add_rule (rule_buf, rule_len, rules);
	  if (rc == 0){
		/* all ok */
	  } else if (rc == -1){
              fprintf(stderr,"[WARN]: Skipping rule: %s (syntax error)", rule_buf);
	  } else if (rc == -3){
	      fprintf(stderr,"[WARN]: Skipping rule: %s (duplicate rule)", rule_buf);
	  } else if (rc == -4){
	      fprintf(stderr,"[WARN]: Skipping rule: %s (duplicate result)", rule_buf);
	  }
    }
    fclose (fp);
    } else {
        fprintf(stderr,"[ERROR]: %s: %s", file_rules, strerror (errno));
        exit(EXIT_FAILURE);
    }
 } else {
    fprintf(stderr,"[ERROR]: rules file is missing\n");
    exit(EXIT_FAILURE);
 }

  if (file_wordlist != NULL){
      if ((fp = fopen (file_wordlist, "rb")) != NULL){
          /*    char rule_buf[RP_RULE_BUFSIZ]; */
	  load_words (fp, words, engine_parameter);
	  fclose (fp);
      } else {
	  fprintf(stderr,"[ERROR]: %s: %s", file_wordlist, strerror (errno));
	  exit (EXIT_FAILURE);
      }
  } else {
      fprintf(stderr,"[ERROR]: wordlist file is missing\n");
       exit(EXIT_FAILURE);
  }

  TOT_GUESSES = (uint64_t) words->words_cnt * (uint64_t) rules->rules_cnt;
 #ifndef BENCHMARK
  fprintf(stderr,"[INFO]: Wordcount %lu\n",words->words_cnt);
  fprintf(stderr,"[INFO]: Rulecount %lu\n",rules->rules_cnt);
  fprintf(stderr,"[INFO]: TOT GUESSES %lu\n", TOT_GUESSES);
  #endif
    


  if(attack_mode == ADAM_ATTACK_MODE){
    pid_t pid = fork();
    if(pid == 0){
        snprintf(cmd,MAX_CMD_LEN, "python daemon.py -model %s -p %d", model_path, daemon_port);
        return system(cmd);
    }else{
        status = runAttackAdam(rules, words, file_hashes, budget, daemon_port, out_guess_file,max_guesses);
        return status;
        }
  }else{
     return runAttackStandard(rules, words, file_hashes,out_guess_file, max_guesses);
  }

}

