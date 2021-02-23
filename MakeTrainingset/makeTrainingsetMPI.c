/**
 * Author......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */

#define LOG_FQ 100000

#include <mpi.h>

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

#include "common.h"
#include "tsearch.h"
#include "rp.h"
#include "engine.h"
#include "set.h"


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
#define BENCHMARK       0
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

const char *PROMPT = "[s]tatus [p]ause [r]esume [b]ypass [q]uit => ";

#define NUM_DEFAULT_BENCHMARK_ALGORITHMS 72


int mpi_rank = 0;
int mpi_size = 0;
int fgetl (FILE *fp, char *line_buf)
{
  if (fgets (line_buf, BUFSIZ, fp) == NULL) return (-1);

  int line_len = strlen (line_buf);

  if (line_buf[line_len - 1] == '\n')
  {
    line_len--;

    line_buf[line_len] = '\0';
  }

  if (line_buf[line_len - 1] == '\r')
  {
    line_len--;

    line_buf[line_len] = '\0';
  }

  return (line_len);
}

int compare_string (const void *p1, const void *p2)
{
  const char *s1 = (const char *) p1;
  const char *s2 = (const char *) p2;

  return strcmp (s1, s2);
}

int compare_stringptr (const void *p1, const void *p2)
{
  const char **s1 = (const char **) p1;
  const char **s2 = (const char **) p2;

  return strcmp (*s1, *s2);
}

int compare_salt_ikepsk (const void *p1, const void *p2)
{
  const salt_t *s1 = (const salt_t *) p1;
  const salt_t *s2 = (const salt_t *) p2;

  return memcmp (s1->ikepsk, s2->ikepsk, sizeof (ikepsk_t));
}

int compare_salt_netntlm (const void *p1, const void *p2)
{
  const salt_t *s1 = (const salt_t *) p1;
  const salt_t *s2 = (const salt_t *) p2;

  return memcmp (s1->netntlm, s2->netntlm, sizeof (netntlm_t));
}

char **scan_directory (const char *path)
{
  char *tmp_path = mystrdup (path);

  size_t tmp_path_len = strlen (tmp_path);

  while (tmp_path[tmp_path_len - 1] == '/' || tmp_path[tmp_path_len - 1] == '\\')
  {
    tmp_path[tmp_path_len - 1] = '\0';

    tmp_path_len = strlen (tmp_path);
  }

  char **files = NULL;

  int num_files = 0;

  DIR *d;

  if ((d = opendir (tmp_path)) != NULL)
  {
    struct dirent *de;

    while ((de = readdir (d)) != NULL)
    {
      if ((strcmp (de->d_name, ".") == 0) || (strcmp (de->d_name, "..") == 0)) continue;

      int path_file_len = strlen (tmp_path) + 1 + strlen (de->d_name) + 1;

      char *path_file =(char*) malloc ( path_file_len );

      snprintf (path_file, path_file_len , "%s/%s", tmp_path, de->d_name);

      DIR *d_test;

      if ((d_test = opendir (path_file)) != NULL)
      {
        closedir (d_test);

        myfree (path_file);
      }
      else
      {
        files = (char**) myrealloc (files, (num_files + 1) * sizeof (char *));

        files[num_files] = path_file;

        num_files++;
      }
    }

    closedir (d);

    qsort (files, num_files, sizeof (char *), compare_stringptr);
  }
  else if (errno == ENOTDIR)
  {
    files = (char**) myrealloc (files, (num_files + 1) * sizeof (char *));

    files[num_files] = mystrdup (path);

    num_files++;
  }
  else
  {
    fprintf(stderr,"%s: %s", path, strerror (errno));

    exit (-1);
  }

  files = (char**)myrealloc (files, (num_files + 1) * sizeof (char *));

  files[num_files] = NULL;

  myfree (tmp_path);

  return (files);
}

void incr_rules_buf (rules_t *rules)
{
  if (rules->rules_cnt == rules->rules_avail)
  {
    rules->rules_avail += INCR_RULES_PTR;

    rules->rules_buf =(char**) myrealloc (rules->rules_buf, rules->rules_avail * sizeof (char *));

    rules->rules_len = (uint32_t*)myrealloc (rules->rules_len, rules->rules_avail * sizeof (uint32_t));
  }
}

void incr_words_buf (words_t *words)
{
  if (words->words_cnt == words->words_avail)
  {
    words->words_avail += INCR_WORDS_PTR;

    words->words_buf = (char**)myrealloc (words->words_buf, words->words_avail * sizeof (char *));

    words->words_len = (uint32_t*)myrealloc (words->words_len, words->words_avail * sizeof (uint32_t));
  }
}


words_t *init_new_words (void)
{
  words_t *words = (words_t*)mymalloc (sizeof (words_t));

  memset (words, 0, sizeof (words_t));

  return words;
}

rules_t *init_new_rules (void)
{
  rules_t *rules = (rules_t*) mymalloc (sizeof (rules_t));

  memset (rules, 0, sizeof (rules_t));

  return rules;
}

digest_t *init_new_digest (void)
{
  digest_t *digest = (digest_t*)mymalloc (sizeof (digest_t));

  memset (digest, 0, sizeof (digest_t));

  return digest;
}

index_t *init_new_index (void)
{
  index_t *index = (index_t*)mymalloc (sizeof (index_t));

  memset (index, 0, sizeof (index_t));

  return index;
}

salt_t *init_new_salt (void)
{
  salt_t *salt = (salt_t*)mymalloc (sizeof (salt_t));

  memset (salt, 0, sizeof (salt_t));

  return salt;
}

db_t *init_new_db (void)
{
  db_t *db = (db_t*)mymalloc (sizeof (db_t));

  memset (db, 0, sizeof (db_t));

  return db;
}

engine_parameter_t *init_new_engine_parameter (void)
{
  engine_parameter_t *engine_parameter = (engine_parameter_t*)mymalloc (sizeof (engine_parameter_t));

  memset (engine_parameter, 0, sizeof (engine_parameter_t));

  return engine_parameter;
}

#define SPACE 256
int cnt = 0;

void *root_cs = NULL;

void add_word (char *word_pos, uint32_t word_len, words_t *words, engine_parameter_t *engine_parameter)
{
  if (word_len > 0) if (word_pos[word_len - 1] == '\r') word_len--;

  // this conversion must be done before the length check


  if (word_len > engine_parameter->plain_size_max) return;


  incr_words_buf (words);

  words->words_buf[words->words_cnt] = word_pos;
  words->words_len[words->words_cnt] = word_len;

  words->words_cnt++;

  return;

  /* disabled, confuses users */

  /* add trimmed variant */

  if (word_pos[word_len - 1] == ' ' || word_pos[word_len - 1] == '\t')
  {
    if (word_len == 1) return;

    word_len--;

    while (word_pos[word_len - 1] == ' ' || word_pos[word_len - 1] == '\t')
    {
      if (word_len == 1) return;

      word_len--;
    }

    add_word (word_pos, word_len, words, engine_parameter);
  }

  if (word_pos[0] == ' ' || word_pos[0] == '\t')
  {
    if (word_len == 1) return;

    word_pos++;
    word_len--;

    while (word_pos[0] == ' ' || word_pos[0] == '\t')
    {
      if (word_len == 1) return;

      word_pos++;
      word_len--;
    }

    add_word (word_pos, word_len, words, engine_parameter);
  }
}

int add_rule (char *rule_buf, uint32_t rule_len, rules_t *rules)
{
  if (__hc_tfind (rule_buf, &rules->root_rule, compare_string) != NULL) return (-3);

  char in[BLOCK_SIZE];
  char out[BLOCK_SIZE];

  memset (in,  0, BLOCK_SIZE);
  memset (out, 0, BLOCK_SIZE);

  int result = apply_rule (rule_buf, rule_len, in, 1, out);

  if (result == -1) return (-1);

  char *next_rule = mystrdup (rule_buf);

  __hc_tsearch (next_rule, &rules->root_rule, compare_string);

  incr_rules_buf (rules);

  rules->rules_buf[rules->rules_cnt] = next_rule;

  rules->rules_len[rules->rules_cnt] = rule_len;

  rules->rules_cnt++;

  return (0);
}



void load_words (FILE *fp, words_t *words, engine_parameter_t *engine_parameter)
{
  size_t size;

  if ((size = fread (words->cache_buf, 1, words->cache_avail - 0x1000, fp)) > 0)
  {
    while (words->cache_buf[size - 1] != '\n')
    {
      if (feof (fp))
      {
        words->cache_buf[size] = '\n';

        size++;

        continue;
      }

      words->cache_buf[size] = fgetc (fp);

      if (size == words->cache_avail - 1)
      {
        if (words->cache_buf[size] != '\n') continue;
      }

      size++;
    }

    words->cache_cnt = size;

    char *pos = &words->cache_buf[0];

    while (pos != &words->cache_buf[size])
    {
      uint32_t i = 0;

      while ((&pos[i + 1] != &words->cache_buf[size]) && (pos[i] != '\n')) i++;

      add_word (pos, i, words, engine_parameter);

      pos += i + 1;
    }

    incr_words_buf (words);

    words->words_buf[words->words_cnt + 0] = words->cache_buf;
    words->words_len[words->words_cnt + 0] = 0;
    words->words_buf[words->words_cnt + 1] = words->cache_buf;
    words->words_len[words->words_cnt + 1] = 0;
    words->words_buf[words->words_cnt + 2] = words->cache_buf;
    words->words_len[words->words_cnt + 2] = 0;
    words->words_buf[words->words_cnt + 3] = words->cache_buf;
    words->words_len[words->words_cnt + 3] = 0;
  }
}


#define INFO_FILENAME "info.txt"
#define RULESOUT_FILENAME "rules.txt"
#define XOUT_FILENAME "X.txt"
//#define LABELX_FILENAME "labelX.bin"
#define LABELX_FILENAME "hits.bin"
//#define LABELR_FILENAME "labelR.bin"
#define LABELR_FILENAME "index_hits.bin"
#define META_FILENAME "meta.bin"

#define ISMASTER mpi_rank == 0

#define WC_t uint32_t
#define RC_t uint32_t

int makeTrainSet(rules_t * rules, words_t *words, char *output_dir, char *file_wordlist, char *file_rules, char *file_labelset ){

    FILE     *info_out;
    FILE     *rules_out;
    FILE     *dict_out;
    FILE     *hits_out;
    FILE     *index_out;
    FILE     *meta_out;
    uint64_t guesses;
    uint64_t noop_guesses;
    uint64_t matched;
    WC_t wc;
    uint64_t n_targets;
    uint64_t n_words;
    uint64_t chunk_size;
    int      k;
    int      log;
    RC_t      rc;
    int      cwl;
    int      crl;
    int      ret;
    char     out[BUF_SIZE];
    char     *cw;
    char     *cr;
    double   ratio;
    MyListPtr  labelset;


    info_out = rules_out = dict_out = hits_out = index_out = meta_out = NULL;
    guesses = noop_guesses = matched = wc = n_targets = n_words = chunk_size = 0;
    k = log = rc = cwl = crl = ret = 0;
    cw = cr = NULL;
    ratio = 0;

    if ( NULL == rules){
        fprintf(stderr,"[ERROR]: invalid argument rules\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == words){
        fprintf(stderr,"[ERROR]: invalid argument words\n");
        return (EXIT_FAILURE);
    }

    if ( NULL == output_dir){
        fprintf(stderr,"[ERROR]: invalid argument output_dir\n");
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

    if(ISMASTER){
        snprintf(out,BUF_SIZE,"%s/%s",output_dir,INFO_FILENAME);
        info_out = Fopen(out, "w");
        fprintf(info_out, "%s\n%s\n%s", file_wordlist, file_rules, file_labelset);
        fclose(info_out);
    }

    // open files
    //text
    if(ISMASTER){
        snprintf(out,BUF_SIZE,"%s/%s",output_dir,RULESOUT_FILENAME);
        rules_out = Fopen(out, "w");
    }
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir,XOUT_FILENAME, mpi_rank);
    dict_out = Fopen(out, "w");

    //bins
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir, LABELX_FILENAME, mpi_rank);
    hits_out = Fopen(out, "wb");
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir,LABELR_FILENAME, mpi_rank);
    index_out = Fopen(out, "wb");
    snprintf(out,BUF_SIZE,"%s/%s_%d",output_dir,META_FILENAME, mpi_rank);
    meta_out = Fopen(out, "wb");
    //

    labelset = loadLabelSetGetN(file_labelset, &n_targets); // btw: in qst funzione c'e` un bug, utilizzi i ma nn e` inizializzata
    chunk_size = (words->words_cnt + 1) / mpi_size;
    wc = chunk_size * mpi_rank;

    if(mpi_rank == mpi_size-1)
        n_words = words->words_cnt;
    else
        n_words = wc + chunk_size;

    RC_t matched_per_word;
    for( ; wc < n_words; wc++){

        matched_per_word = 0;

        if(ISMASTER && log == LOG_FQ){
            fprintf(stderr,"[LOG]: [%u] %lf\n", wc, (double) wc / (double) words->words_cnt);
            log = 0;
        }

        log += 1;

        cw = words->words_buf[wc];
        // word len
        cwl = words->words_len[wc];
        cw[cwl] = '\0';

        //print dict
        fprintf(dict_out, "%s\n", cw);

        for(rc=0; rc < rules->rules_cnt; rc++){

            cr = rules->rules_buf[rc];
            crl = rules->rules_len[rc];
            ret = apply_rule(cr, crl, cw, cwl, out);

            if(ISMASTER && wc == 0){
                //print rule
                fprintf(rules_out, "%s\n", cr);
            }

            if(ret != -1){
                guesses += 1;

                if(strcmp(cw, out) == 0){
                    // noop rule
                    noop_guesses += 1;
                    continue;
                }

                // check match
                if(searchElem(labelset, out) == 1){
                    //matched
                    matched++;
                    matched_per_word++;
                    //print match rule
                    fwrite(&rc, sizeof(RC_t), 1, hits_out);                }
            } else {
                fprintf(stderr,"[ERROR]: skipping %s\t%s\n",cw,cr);
                exit(1);
            }
        }
        // write number of matched password rule rc
        fwrite(&matched_per_word, sizeof(RC_t), 1, index_out);
    }

    ratio=(double) noop_guesses / (double) guesses;
    fprintf(stderr, "\n%llu %llu %llu\n", guesses, noop_guesses, matched);
    fprintf(stderr, "NOOP %lf\n", ratio);

    fwrite(&matched, sizeof(uint64_t), 1, meta_out);
    fwrite(&noop_guesses, sizeof(uint64_t), 1, meta_out);
    fwrite(&guesses, sizeof(uint64_t), 1, meta_out);

    if(ISMASTER)
        fclose(rules_out);
    fclose(dict_out);
    fclose(hits_out);
    fclose(index_out);
    fclose(meta_out);
    freeTree(labelset);
    return 0;
}

#define USAGE "Usage: -r RULES-FILE -w WORDLIST-FILE -l LABELSET-FILE -o OUTPUT-DIR\n"
int main (int argc, char *argv[]){


  setvbuf (stdout, NULL, _IONBF, 0);

  uint32_t usage_view       = USAGE_VIEW;
  uint32_t version_view     = VERSION_VIEW;
  char    *file_rules       = NULL;
  char    *file_wordlist    = NULL;
  char    *file_labelset    = NULL;
  uint32_t cache_size       = CACHE_SIZE;
  char    *output_dir       = NULL;
  /*
   * preinit: getopt
   */

  #define IDX_HELP            'h'
  #define IDX_VERSION         'V'
  #define IDX_RULES_FILE      'r'
  #define IDX_WORDLIST_FILE   'w'
  #define IDX_LABELSET_FILE   'l'
  #define IDX_OUTPUT_DIR      'o'
  struct option long_options[] =
  {
    {"help",            no_argument,       0, IDX_HELP},
    {"version",         no_argument,       0, IDX_VERSION},
    {"rules-file",      required_argument, 0, IDX_RULES_FILE},
    {"wordlist-file",   required_argument, 0, IDX_WORDLIST_FILE},
    {"labelset-file",   required_argument, 0, IDX_LABELSET_FILE},
    {"output-dir",      required_argument, 0, IDX_OUTPUT_DIR},
    {0, 0, 0, 0}
  };

  int option_index = 0;

  if( MPI_SUCCESS != MPI_Init(&argc, &argv)){
    fprintf(stderr,"[ERROR]: unable to initialize MPI\n");
    exit(EXIT_FAILURE);
  }
  if( MPI_SUCCESS != MPI_Comm_size(MPI_COMM_WORLD, &mpi_size)){
    fprintf(stderr,"[ERROR]: unable to initialize MPI\n");
    exit(EXIT_FAILURE);
  }
  if( MPI_SUCCESS != MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank)){
    fprintf(stderr,"[ERROR]: unable to initialize MPI\n");
    exit(EXIT_FAILURE);
  }

  int c;
  while ((c = getopt_long (argc, argv, "hVr:w:l:o:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
      case IDX_HELP:            usage_view         = 1;               break;
      case IDX_VERSION:         version_view       = 1;               break;
      case IDX_RULES_FILE:      file_rules         = optarg;          break;
      case IDX_WORDLIST_FILE:   file_wordlist      = optarg;          break;
      case IDX_LABELSET_FILE:   file_labelset      = optarg;          break;
      case IDX_OUTPUT_DIR:      output_dir         = optarg;          break;
      default: exit (EXIT_FAILURE);
    }
  }

  if(usage_view || version_view){
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_SUCCESS);
  }

  if( NULL == file_rules){
	fprintf(stderr,"[ERROR]: rules-file is mandatory\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
  }

  if( NULL == file_wordlist){
	fprintf(stderr,"[ERROR]: wordlist-file is mandatory\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
  }

  if( NULL == file_labelset){
	fprintf(stderr,"[ERROR]: labelset-file is mandatory\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
  }

  if( NULL == output_dir){
	fprintf(stderr,"[ERROR]: output-dir is mandatory\n");
 	fprintf(stderr,"%s",USAGE);
	exit(EXIT_FAILURE);
  }
  if( mpi_rank == 0){
      Mkdir(output_dir, DIRPERM);
  }
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
      FILE *fp;
      if ((fp = fopen (file_rules, "rb")) != NULL){
          /*    char rule_buf[RP_RULE_BUFSIZ]; */
	  char rule_buf[BUFSIZ];
	  int rule_len;
	  while ((rule_len = fgetl (fp, rule_buf)) != -1){
          if (rule_len == 0){
	      continue;
	  }
	  if (rule_buf[0] == '#'){
	      continue;
	  }
	  int rc = add_rule (rule_buf, rule_len, rules);
	  if (rc == 0){
		/* all ok */
	  } else if (rc == -1){
              fprintf(stderr,"Skipping rule: %s (syntax error)", rule_buf);
	  } else if (rc == -3){
	      fprintf(stderr,"Skipping rule: %s (duplicate rule)", rule_buf);
	  } else if (rc == -4){
	      fprintf(stderr,"Skipping rule: %s (duplicate result)", rule_buf);
	  }
    }
    fclose (fp);
    } else {
        fprintf(stderr,"%s: %s", file_rules, strerror (errno));
        exit(EXIT_FAILURE);
    }
 } else {
    fprintf(stderr,"[ERROR]: rules file is missing\n");
    exit(EXIT_FAILURE);
 }

  if (file_wordlist != NULL){
      FILE *fp;
      if ((fp = fopen (file_wordlist, "rb")) != NULL){
          /*    char rule_buf[RP_RULE_BUFSIZ]; */
	  load_words (fp, words, engine_parameter);
	  fclose (fp);
      } else {
	  fprintf(stderr,"%s: %s", file_wordlist, strerror (errno));
	  exit (EXIT_FAILURE);
      }
  } else {
      fprintf(stderr,"[ERROR]: wordlist file is missing\n");
       exit(EXIT_FAILURE);
  }


  uint64_t TOT_GUESSES = (uint64_t) words->words_cnt * (uint64_t) rules->rules_cnt;
  fprintf(stderr,"[INFO]: Wordcount %llu\n",words->words_cnt);
  fprintf(stderr,"[INFO]: Rulecount %llu\n",rules->rules_cnt);
  fprintf(stderr,"[INFO]: TOT GUESSES %llu\n", TOT_GUESSES);

  c = makeTrainSet(rules, words, output_dir, file_wordlist, file_rules, file_labelset);

  MPI_Finalize();
  return c;
}
