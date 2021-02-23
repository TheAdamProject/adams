/**
 * Author......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */

#include "hashcat_utils.h"
const char *PROMPT = "[s]tatus [p]ause [r]esume [b]ypass [q]uit => ";


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

