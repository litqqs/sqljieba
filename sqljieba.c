/* Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <stdlib.h>
#include <ctype.h>
#include <my_config.h>
#include <m_ctype.h>
#include <mysql/plugin_ftparser.h>
#include "jieba.h"

static const char* DICT_PATH      = "/usr/share/dict/jieba.dict.utf8";
static const char* MODEL_PATH     = "/usr/share/dict/hmm_model.utf8";
static const char* USER_DICT_PATH = "/usr/share/dict/user.dict.utf8";
static const char* IDF_PATH       = "/usr/share/dict/idf.utf8";
static const char* STOPWORD_PATH  = "/usr/share/dict/stop_words.utf8";

static void* jieba_hanlde = NULL;

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

static int sqljieba_plugin_init(void *arg __attribute__((unused)))
{
  jieba_hanlde = NewJieba(DICT_PATH, MODEL_PATH, USER_DICT_PATH, IDF_PATH, STOPWORD_PATH);
  return(0);
}

static int sqljieba_plugin_deinit(void *arg __attribute__((unused)))
{
  FreeJieba(jieba_hanlde);
  return(0);
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
  return(0);
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
  return(0);
}


/*
  Pass a word back to the server.

  SYNOPSIS
    add_word()
      param              parsing context of the plugin
      word               a word
      len                word length

  DESCRIPTION
    Fill in boolean metadata for the word (if parsing in boolean mode)
    and pass the word to the server.  The server adds the word to
    a full-text index when parsing for indexing, or adds the word to
    the list of search terms when parsing a search string.
*/

static void add_word(MYSQL_FTPARSER_PARAM *param, char *word, size_t len)
{
  /* It is difficult to get the position value, so we use 0 as a placeholder.
     Fortunately bool_info is not used in the MyISAM engine. */
  const int position = 0;
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info =
    { FT_TOKEN_WORD, 0, 0, 0, 0, position, ' ', 0 };

  /* Since word will later be freed, we need to tell MySQL
     to make a copy of the word */
  param->flags = MYSQL_FTFLAGS_NEED_COPY;
  param->mysql_add_word(param, word, len, &bool_info);
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
  /* If Jieba is not found, use the built-in parser */
  if (!jieba_hanlde) {
    return param->mysql_parse(param, param->doc, param->length);
  }

  CJiebaWordCollection* collection = CutForSearch(jieba_hanlde, param->doc, param->length);
  for (size_t i = 0; i < collection->nwords; i++) {
    add_word(param, collection->words[i].word, collection->words[i].len);
  }
  FreeWords(collection);
  return(0);
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_ftparser sqljieba_descriptor =
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  sqljieba_parse,                   /* parsing function       */
  sqljieba_parser_init,             /* parser init function   */
  sqljieba_parser_deinit            /* parser deinit function */
};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(sqljieba)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &sqljieba_descriptor,       /* descriptor                      */
  "sqljieba",                 /* name                            */
  "github.com/yanyiwu",       /* author                          */
  "Jieba Full-Text Parser",   /* description                     */
  PLUGIN_LICENSE_GPL,
  sqljieba_plugin_init,       /* init function (when loaded)     */
  sqljieba_plugin_deinit,     /* deinit function (when unloaded) */
  0x0001,                     /* version                         */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;
