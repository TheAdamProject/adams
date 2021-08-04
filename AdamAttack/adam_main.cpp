/**
 * Authors......:
 * License.....: MIT
 */

/**
 * Author......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */

#include "utilities.h"
#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"
#include "wordlist.h"
#include "adam_attack.h"
#include "hashcat_utils.h"

#define MAX_CMD_LEN 2048
#define ADAM_ATTACK_MODE 9
#define CHARMAP_PATH "./charmap.txt"

#define USAGE "Usage: \n"\
"-r RULES-FILE\n"\
"-w WORDLIST-FILE\n"\
"--hashes-file HASH-FILE\n"\
"[--max-guesses-pow MAX-GUESSES (10^MAX-GUESSES)]\n"\
"[-a ATTACK-MODE {0=standard, 9=adams} (default=adams)]\n"\
"\nif adams:\n"\
"\t--config-dir DIR-PATH\n"\
"\t[--model-path MODEL-PATH]\n"\
"\t[--budget BUDGET]\n"\
"The option --config-dir specifies the directory containg the model, the rules, and the budget.\n"\
"The options -r, --model-path, and --budget will overwrite the --config-dir values.\n"\
"If -r,--model-path, and --budget are all provided by the cmdline, --config-dir can be omitted\n"

int main(int argc, char * argv[]) {

  setvbuf(stdout, NULL, _IONBF, 0);

  uint32_t usage_view = USAGE_VIEW;
  uint32_t version_view = VERSION_VIEW;
  uint32_t cache_size = CACHE_SIZE;
  uint32_t attack_mode = ADAM_ATTACK_MODE;
  uint32_t max_guess_pow = 0;
  uint64_t max_guesses = -1;
  uint64_t TOT_GUESSES = 0;
  char * file_rules = NULL;
  char * file_wordlist = NULL;
  char * file_hashes = NULL;
  char * output_dir = NULL;
  char * daemon_path = NULL;
  char * config_dir = NULL;
  char * out_guess_path = NULL;
  char * model_path = NULL;
  char rule_buf[BUFSIZ];
  char cmd[MAX_CMD_LEN];
  pid_t daemon_pid = 0;
  float budget = 0.0;
  FILE * out_guess_file = NULL;
  FILE * fp = NULL;
  int status = 0;
  int rule_len = 0;
  int rc = 0;
  int c = 0;
  int option_index = 0;
  /*
   * preinit: getopt
   */

  #define IDX_HELP 'h'
  #define IDX_VERSION 'V'
  #define IDX_RULES_FILE 'r'
  #define IDX_WORDLIST_FILE 'w'
  #define IDX_ATTACK_MODE 'a'
  #define IDX_HASH_FILE 9001
  #define IDX_DYNAMIC 9002
  #define IDX_ADR 9004
  #define IDX_MODEL_PATH 9005
  #define IDX_ADAM_BUDG 9006
  #define IDX_OUT_G_PATH 9007
  #define IDX_CONFIG_DIR 9008
  #define IDX_MAX_GUESS 9009

  struct option long_options[] = {
    {
      "help",
      no_argument,
      0,
      IDX_HELP
    },
    {
      "version",
      no_argument,
      0,
      IDX_VERSION
    },
    {
      "rules-file",
      required_argument,
      0,
      IDX_RULES_FILE
    },
    {
      "wordlist-file",
      required_argument,
      0,
      IDX_WORDLIST_FILE
    },
    {
      "attack-mode",
      required_argument,
      0,
      IDX_ATTACK_MODE
    },
    {
      "hashes-file",
      required_argument,
      0,
      IDX_HASH_FILE
    },
    {
      "config-dir",
      required_argument,
      0,
      IDX_CONFIG_DIR
    },
    {
      "model-path",
      required_argument,
      0,
      IDX_MODEL_PATH
    },
    {
      "output-guessed-file",
      required_argument,
      0,
      IDX_OUT_G_PATH
    },
    {
      "budget",
      required_argument,
      0,
      IDX_ADAM_BUDG
    },
    {
      "max-guesses-pow",
      required_argument,
      0,
      IDX_MAX_GUESS
    },
    {
      0,
      0,
      0,
      0
    }
  };

  while ((c = getopt_long(argc, argv, "hVr:w:l:p:a:m:o:t:g:", long_options, & option_index)) != -1) {
    switch (c) {
    case IDX_HELP:
      usage_view = 1;
      break;
    case IDX_VERSION:
      version_view = 1;
      break;
    case IDX_RULES_FILE:
      file_rules = strdup(optarg);
      break;
    case IDX_WORDLIST_FILE:
      file_wordlist = strdup(optarg);
      break;
    case IDX_HASH_FILE:
      file_hashes = strdup(optarg);
      break;
    case IDX_ATTACK_MODE:
      attack_mode = atoi(optarg);
      break;
    case IDX_MODEL_PATH:
      model_path = strdup(optarg);
      break;
    case IDX_CONFIG_DIR:
      config_dir = strdup(optarg);
      break;
    case IDX_OUT_G_PATH:
      out_guess_path = strdup(optarg);
      break;
    case IDX_ADAM_BUDG:
      budget = atof(optarg);
      break;
    case IDX_MAX_GUESS:
      max_guess_pow = atoi(optarg);
      break;
    default:
      exit(EXIT_FAILURE);
    }
  }

  if (usage_view || version_view) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
  }

  if (NULL == file_wordlist) {
    fprintf(stderr, "[ERROR]: wordlist-file is mandatory\n");
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  if (NULL == file_hashes) {
    fprintf(stderr, "[ERROR]: hashes-file is mandatory\n");
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  out_guess_file = stdout;
  if (out_guess_path) {
    out_guess_file = Fopen(out_guess_path, "w+");
  }

  if (max_guess_pow > 0) {
    max_guesses = POW_BASE;
    for (int i = 1; i < max_guess_pow; i++) {
      max_guesses *= POW_BASE;
    }
  }

  if (NULL == config_dir) {
    if ((NULL == model_path) || (NULL == file_rules) || (0.0 == budget)) {
      fprintf(stderr, "[ERROR]: Only if -r, --model-path, and --budget are provided the option --config-dir can be omitted\n");
      fprintf(stderr, "%s", USAGE);
      exit(EXIT_FAILURE);
    }
  } else {
    if (budget) {
      float _budget;
      getModelFromDir(config_dir, & model_path, & file_rules, & _budget);
    } else {
      getModelFromDir(config_dir, & model_path, & file_rules, & budget);
    }
  }
    
  bool dynamic, semantic;
  if(attack_mode == 9){
    dynamic = true;
    semantic = false;
  }else if(attack_mode == 10){
    dynamic = true;
    semantic = true;
  }else if(attack_mode == 0){
    dynamic = false;
    semantic = false;
    budget = 1.;
  }

  attack_mode == 9;

  // init hashcat legacy and load setups ================================================

  // init hashcat
  db_t * db = initBuffers_hashcat();
  // load rules
  loadRulesFromFile(file_rules, db -> rules);

  // wordlist
  fprintf(stderr, "[INFO]: Reading wordlist...\n");
  wordlist_t wordlist = loadWordlistFromFile_list(file_wordlist);

  rules_t * rules = db -> rules;

  uint64_t tot_guesses = (uint64_t) wordlist.size() * (uint64_t) rules -> rules_cnt;
  int tg_m = (int) log10(tot_guesses);
  fprintf(stderr, "[INFO]: Wordcount %lu\n", wordlist.size());
  fprintf(stderr, "[INFO]: Rulecount %lu\n", rules -> rules_cnt);
    
  fprintf(stderr, "[INFO]: Reading target set...\n");
  target_t target = loadTarget(file_hashes);
  uint64_t target_size = target.size();
  fprintf(stderr, "[INFO]: Target size %lu\n", target_size);
    
  if(dynamic){
    uint64_t max_tot_guesses = (uint64_t) (wordlist.size() + target.size()) * (uint64_t) rules -> rules_cnt;
    int mtg_m = (int) log10(max_tot_guesses);
    fprintf(stderr, "[INFO]: TOT GUESSES %lu - %lu (from 10^%d to 10^%d)\n", tot_guesses, max_tot_guesses, tg_m, mtg_m);
  }else
    fprintf(stderr, "[INFO]: TOT GUESSES %lu (10^%d)\n", tot_guesses, tg_m);

  
  //======================================================================================

  // load neural network =================================================================

  // init model context
  model_context ctx;
  cppflow::model model(model_path);
  ctx.model = &model;

  // char map
  char_map_t char_map;
  load_char_map(CHARMAP_PATH, & (char_map));
  ctx.char_map = &char_map;
  ctx.max_len = MAX_LEN;
  ctx.batch_size = BATCH_SIZE;

  // memory pool 
  int64_t batch_max_size = ctx.batch_size * ctx.max_len;
  ctx.batch_encoded = std::vector < encoded_t > (batch_max_size);

  //======================================================================================
    
  int output_type = 1;
  // run
  status = runAttackAdam(&ctx, rules, &wordlist, &target, budget, dynamic, semantic, output_type, max_guesses);

}