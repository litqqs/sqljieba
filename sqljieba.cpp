#include <vector>
#include <string>
#include <my_config.h>
#include <m_ctype.h>
#include <mysql/plugin_ftparser.h>
#include <mysql_com.h>
#include <cppjieba/Jieba.hpp>
#include <stdio.h>

using cppjieba::Jieba;
using cppjieba::Word;

#define DICT_FILE "jieba.dict.utf8"
#define MODEL_FILE "hmm_model.utf8"
#define USER_DICT_FILE "user.dict.utf8"
#define IDF_FILE "idf.utf8"
#define STOPWORD_FILE "stop_words.utf8"


#ifdef _DEBUG
static FILE *log_filehandle = NULL;

#ifdef WIN32
#define LOG_FILE "C:/jieba_cut2.log"
#else
#define LOG_FILE "/tmp/jieba_cut.log"
#endif

#endif

#ifdef WIN32
#include <libloaderapi.h>
#else
#define DICT_DIR "/usr/share/dict/"
static const char *DICT_PATH      = DICT_DIR DICT_FILE;
static const char *MODEL_PATH     = DICT_DIR MODEL_FILE;
static const char *USER_DICT_PATH = DICT_DIR USER_DICT_FILE;
static const char *IDF_PATH       = DICT_DIR IDF_FILE;
static const char *STOPWORD_PATH  = DICT_DIR STOPWORD_FILE;
#endif

static const Jieba *jieba_handle = NULL;

/*
  sqljieba interface functions:

  Plugin declaration functions:
  - sqljieba_plugin_init()
  - sqljieba_plugin_deinit()

  Parser descriptor functions:
  - sqljieba_parse()
  - sqljieba_parser_init()
  - sqljieba_parser_deinit()
*/

/*
  Initialize the plugin at server start or plugin installation.
  NOTICE: when the DICT_PATH, HMM_MODEL, USER_DICT_PATH not found, NewJieba would log the error and exit without return anything .

  SYNOPSIS
    sqljieba_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int sqljieba_plugin_init(void *arg)
{
#ifdef _DEBUG
  if (0 == access(LOG_FILE, 0))
    log_filehandle = fopen(LOG_FILE, "a");
#endif

#ifdef WIN32
  TCHAR DICT_DIR[256], DICT_PATH[256], MODEL_PATH[256], USER_DICT_PATH[256], IDF_PATH[256], STOPWORD_PATH[256];
  GetModuleFileName(NULL, DICT_DIR, 256);
  for (int n = strlen(DICT_DIR) - 1, i = 0; n >= 0; n--)
  {
    if ('\\' == DICT_DIR[n])
    {
      DICT_DIR[n] = 0;
      if (++i >= 2)
        break;
    }
  }
  strcat(DICT_DIR, "\\lib\\plugin\\dict\\");
  //--
  strcpy(DICT_PATH, DICT_DIR);
  strcpy(MODEL_PATH, DICT_DIR);
  strcpy(USER_DICT_PATH, DICT_DIR);
  strcpy(IDF_PATH, DICT_DIR);
  strcpy(STOPWORD_PATH, DICT_DIR);

  strcat(DICT_PATH, DICT_FILE);
  strcat(MODEL_PATH, MODEL_FILE);
  strcat(USER_DICT_PATH, USER_DICT_FILE);
  strcat(IDF_PATH, IDF_FILE);
  strcat(STOPWORD_PATH, STOPWORD_FILE);
#endif
  jieba_handle = new Jieba(DICT_PATH, MODEL_PATH, USER_DICT_PATH, IDF_PATH, STOPWORD_PATH);
  return 0;
}

static int sqljieba_plugin_deinit(void *arg)
{
  if (jieba_handle)
  {
    delete jieba_handle;
  }
#ifdef _DEBUG
  if (log_filehandle)
    fclose(log_filehandle);
#endif
  return 0;
}

/*
  Initialize the parser on the first use in the query

  SYNOPSIS
    sqljieba_parser_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int sqljieba_parser_init(MYSQL_FTPARSER_PARAM *param)
{
  return 0;
}

/*
  Terminate the parser at the end of the query

  SYNOPSIS
    sqljieba_parser_deinit()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int sqljieba_parser_deinit(MYSQL_FTPARSER_PARAM *param)
{
  return 0;
}

/*
  Parse a document or a search query.

  SYNOPSIS
    sqljieba_parse()
      param              parsing context

  DESCRIPTION
    This is the main plugin function which is called to parse
    a document or a search query. The call mode is set in
    param->mode.  This function simply splits the text into words
    and passes every word to the MySQL full-text indexing engine.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int sqljieba_parse(MYSQL_FTPARSER_PARAM *param)
{
  // If Jieba is not found, use the built-in parser
  if (!jieba_handle || MYSQL_FTPARSER_FULL_BOOLEAN_INFO == param->mode)
  {
    return param->mysql_parse(param, param->doc, param->length);
  }

  // Convert C const char* to C++ string
  std::string doc(param->doc, param->length);
  // Segmented words
  // cpjieba::Word contains the offset information
  std::vector<Word> words;
  jieba_handle->CutForSearch(doc, words, true);

  // Used in boolean mode
  char c = ' ';
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info = {FT_TOKEN_WORD, 0, 0, 0, 0, 0, &c};
  //{ FT_TOKEN_WORD, 0, 0, 0, 0, 0, ' ', 0 };
  for (size_t i = 0; i < words.size(); i++)
  {
    // Skip white spaces
    if (words[i].word[0] == ' ')
      continue;

    // According to some use cases the MySQL will crash if we add
    // a word that is too long. Hence we manually remove words that are
    // more than 100 bytes
    if (words[i].word.size() > 100)
      continue;
#ifdef _DEBUG
    if (log_filehandle)
    {
      fwrite(param->doc + words[i].offset, words[i].word.size(), 1, log_filehandle);
      fwrite(" ", 1, 1, log_filehandle);
    }
#endif
    // Set offset and add the word
    // bool_info.position = words[i].offset;
    param->mysql_add_word(param, param->doc + words[i].offset, words[i].word.size(), &bool_info);
  }
#ifdef _DEBUG
  if (log_filehandle)
    fwrite("\n", 1, 1, log_filehandle);
#endif
  return 0;
}

/*
  Plugin type-specific descriptor
*/

static struct st_mysql_ftparser sqljieba_descriptor = {
    MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
    sqljieba_parse,                   /* parsing function       */
    sqljieba_parser_init,             /* parser init function   */
    sqljieba_parser_deinit            /* parser deinit function */
};

/*
  Plugin library descriptor
*/

maria_declare_plugin(sqljieba){
    MYSQL_FTPARSER_PLUGIN,    /* type                            */
    &sqljieba_descriptor,     /* descriptor                      */
    "sqljieba",               /* name                            */
    "github.com/litqqs",      /* author                          */
    "Jieba Full-Text Parser", /* description                     */
    PLUGIN_LICENSE_GPL,
    sqljieba_plugin_init,           /* init function (when loaded)     */
    sqljieba_plugin_deinit,         /* deinit function (when unloaded) */
    0x0100,                         /* version                         */
    NULL,                           /* status variables                */
    NULL,                           /* system variables                */
    NULL,                           /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE, /* maturity */
} maria_declare_plugin_end;
