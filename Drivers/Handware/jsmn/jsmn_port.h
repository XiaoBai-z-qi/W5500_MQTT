#ifndef __JSMN_PORT_H__
#define __JSMN_PORT_H__
#define JSMN_STATIC 
#include "jsmn.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#define JSON_BOOL(x) ((x) ? "true" : "false")

int json_parse(const char *json, jsmntok_t *tokens, int max_tokens);
jsmntok_t* json_find_value(const char *json, jsmntok_t *tokens, int token_cnt, const char *key);
int json_token_to_int(const char *json, jsmntok_t *tok);
float json_token_to_float(const char *json, jsmntok_t *tok);
bool json_token_to_bool(const char *json, jsmntok_t *tok);
int json_token_to_str(const char *json, jsmntok_t *tok, char *dst, int max_len);
int json_printf(char *buf, int size, const char *fmt, ...);



#endif
