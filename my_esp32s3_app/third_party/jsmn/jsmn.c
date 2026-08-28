/**
 * @file jsmn.c
 * @brief JSMN JSON parser implementation for ESP-IDF
 */

#include "jsmn.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "JSMN";

JSMN_API void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   const size_t num_tokens) {
  jsmntok_t *tok;
  if (parser->toknext >= num_tokens) {
    return NULL;
  }
  tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
#ifdef JSMN_PARENT_LINKS
  tok->parent = -1;
#endif
  return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                            const int start, const int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                const size_t len, jsmntok_t *tokens,
                                const size_t num_tokens) {
  jsmntok_t *token;
  int start;

  start = parser->pos;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    switch (js[parser->pos]) {
#ifndef JSMN_STRICT
    /* In strict mode primitive must be followed by "," or "}" or "]" */
    case ':':
#endif
    case '\t':
    case '\r':
    case '\n':
    case ' ':
    case ',':
    case ']':
    case '}':
      goto found;
    default:
      break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }
#ifdef JSMN_STRICT
  /* In strict mode primitive must be followed by a comma/object/array */
  parser->pos = start;
  return JSMN_ERROR_PART;
#endif

found:
  if (tokens == NULL) {
    parser->pos--;
    return 0;
  }
  token = jsmn_alloc_token(parser, tokens, num_tokens);
  if (token == NULL) {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
  token->parent = parser->toksuper;
#endif
  parser->pos--;
  return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             const size_t len, jsmntok_t *tokens,
                             const size_t num_tokens) {
  jsmntok_t *token;
  int start = parser->pos;

  /* Skip starting quote */
  parser->pos++;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c = js[parser->pos];

    /* Quote: end of string */
    if (c == '\"') {
      if (tokens == NULL) {
        return 0;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
      token->parent = parser->toksuper;
#endif
      return 0;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\' && parser->pos + 1 < len) {
      int i;
      parser->pos++;
      switch (js[parser->pos]) {
      case '\"':
      case '/':
      case '\\':
      case 'b':
      case 'f':
      case 'r':
      case 'n':
      case 't':
        break;
      case 'u':
        parser->pos++;
        for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
          if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||
                (js[parser->pos] >= 65 && js[parser->pos] <= 70) ||
                (js[parser->pos] >= 97 && js[parser->pos] <= 102))) {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
          }
          parser->pos++;
        }
        parser->pos--;
        break;
      default:
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens) {
  int r;
  int i;
  jsmntok_t *token;
  int count = parser->toknext;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c;
    jsmntype_t type;

    c = js[parser->pos];
    switch (c) {
    case '{':
    case '[':
      count++;
      if (tokens == NULL) {
        break;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        return JSMN_ERROR_NOMEM;
      }
      if (parser->toksuper != -1) {
        jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
        if (t->type == JSMN_OBJECT) {
          return JSMN_ERROR_INVAL;
        }
#endif
        t->size++;
#ifdef JSMN_PARENT_LINKS
        token->parent = parser->toksuper;
#endif
      }
      token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
      token->start = parser->pos;
      parser->toksuper = parser->toknext - 1;
      break;
    case '}':
    case ']':
      if (tokens == NULL) {
        break;
      }
      type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
      if (parser->toknext < 1) {
        return JSMN_ERROR_INVAL;
      }
      token = &tokens[parser->toknext - 1];
      for (;;) {
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          token->end = parser->pos + 1;
          parser->toksuper = token->parent;
          break;
        }
        if (token->parent == -1) {
          if (token->type != type || parser->toksuper == -1) {
            return JSMN_ERROR_INVAL;
          }
          break;
        }
        token = &tokens[token->parent];
      }
#else
      for (i = parser->toknext - 1; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          parser->toksuper = -1;
          token->end = parser->pos + 1;
          break;
        }
      }
      if (i == -1) {
        return JSMN_ERROR_INVAL;
      }
      for (; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          parser->toksuper = i;
          break;
        }
      }
#endif
      break;
    case '\"':
      r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      break;
    case ':':
      parser->toksuper = parser->toknext - 1;
      break;
    case ',':
      if (tokens != NULL && parser->toksuper != -1 &&
          tokens[parser->toksuper].type != JSMN_ARRAY &&
          tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
        parser->toksuper = tokens[parser->toksuper].parent;
#else
        for (i = parser->toknext - 1; i >= 0; i--) {
          if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
            if (tokens[i].start != -1 && tokens[i].end == -1) {
              parser->toksuper = i;
              break;
            }
          }
        }
#endif
      }
      break;
#ifdef JSMN_STRICT
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 't':
    case 'f':
    case 'n':
      if (tokens != NULL && parser->toksuper != -1) {
        const jsmntok_t *t = &tokens[parser->toksuper];
        if (t->type == JSMN_OBJECT ||
            (t->type == JSMN_STRING && t->size != 0)) {
          return JSMN_ERROR_INVAL;
        }
      }
#else
    default:
#endif
      r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;

#ifdef JSMN_STRICT
    default:
      return JSMN_ERROR_INVAL;
#endif
    }
  }

  if (tokens != NULL) {
    for (i = parser->toknext - 1; i >= 0; i--) {
      if (tokens[i].start != -1 && tokens[i].end == -1) {
        return JSMN_ERROR_PART;
      }
    }
  }

  return count;
}

/* ==================== HELPER FUNCTIONS ==================== */

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  int key_len = tok->end - tok->start;
  int s_len = strlen(s);
  if (tok->type == JSMN_STRING && s_len == key_len &&
      memcmp(json + tok->start, s, key_len) == 0) {
    return 0;
  }
  return -1;
}

int get_pos_key(const char *json, jsmntok_t *tok, int num_token,
                const char *key) {
  for (int i = 1; i < num_token; i++) {
    if (jsoneq(json, &tok[i], key) == 0) {
      return i + 1;
    }
  }
  return -1;
}

int str2int(const char *str) {
  if (!str) return 0;
  return atoi(str);
}

int json_get_int(const char *json, jsmntok_t *tok, int num_token,
                 const char *key) {
  int pos = get_pos_key(json, tok, num_token, key);
  if (pos == -1) {
    return -1;
  }

  char data[32] = {0};
  int len = tok[pos].end - tok[pos].start;
  if (len >= sizeof(data)) len = sizeof(data) - 1;
  memcpy(data, json + tok[pos].start, len);
  data[len] = '\0';
  return str2int(data);
}

int json_get_str(const char *json, jsmntok_t *tok, int num_token,
                 const char *key, char *buf, size_t max_buf_len) {
  if (!buf || max_buf_len == 0) return 0;
  buf[0] = '\0';

  int pos = get_pos_key(json, tok, num_token, key);
  if (pos == -1) {
    return 0;
  }

  int len = tok[pos].end - tok[pos].start;
  if (len >= max_buf_len) len = max_buf_len - 1;
  memcpy(buf, json + tok[pos].start, len);
  buf[len] = '\0'; // Luôn kết thúc chuỗi bằng null terminator
  return len;
}

int json_get_arr_obj(const char *json, jsmntok_t *tok, int num_token,
                     const char *key, int index, char *buf, size_t max_buf_len) {
  if (!buf || max_buf_len == 0) return 0;
  buf[0] = '\0';

  int pos = get_pos_key(json, tok, num_token, key);
  if (pos == -1) {
    return 0;
  }

  pos -= 1;
  jsmntok_t *g = &tok[pos + index + 2];
  int len = g->end - g->start;
  if (len >= max_buf_len) len = max_buf_len - 1;
  snprintf(buf, max_buf_len, "%.*s", len, json + g->start);
  return strlen(buf);
}

int json_parser(const char *json, jsmn_parser *parser, jsmntok_t *token,
                int num_token) {
  if (!json || !parser || !token || num_token <= 0) return -1;
  jsmn_init(parser);
  int r = jsmn_parse(parser, json, strlen(json), token, num_token);
  if (r < 0) {
    ESP_LOGE(TAG, "Lỗi phân tích cú pháp JSON: mã lỗi %d", r);
    return -1;
  }

  if (r < 1 || token[0].type != JSMN_OBJECT) {
    ESP_LOGE(TAG, "JSON mong đợi là dạng Object {...}");
    return -1;
  }

  return r;
}