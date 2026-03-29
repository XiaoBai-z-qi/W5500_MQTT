#include "jsmn_port.h" 
#include "debug_uart.h"
int json_parse(const char *json, jsmntok_t *tokens, int max_tokens)
{
    jsmn_parser parser;
    jsmn_init(&parser);

    int ret = jsmn_parse(&parser, json, strlen(json), tokens, max_tokens);

    if (ret < 0) {
        Debug_Printf("JSON parse failed: %d\r\n", ret);
        return -1;
    }

    if (ret < 1 || tokens[0].type != JSMN_OBJECT) {
        Debug_Printf("JSON root is not object\r\n");
        return -1;
    }

    return ret; // 返回token数量
}

jsmntok_t* json_find_value(const char *json, jsmntok_t *tokens, int token_cnt, const char *key) {
    for (int i = 0; i < token_cnt; i++) {
        if (tokens[i].type == JSMN_STRING &&
            (int)strlen(key) == (tokens[i].end - tokens[i].start) &&
            strncmp(json + tokens[i].start, key, tokens[i].end - tokens[i].start) == 0) {
            // 找到了 key，下一个 token 就是 value
            if (i + 1 < token_cnt) {
                return &tokens[i + 1];
            }
        }
    }
    return NULL;
}
int json_token_to_int(const char *json, jsmntok_t *tok) 
{
    int num = 0;
    int sign = 1;
    int i = tok->start;
    
    // 先处理符号
    if (i < tok->end && json[i] == '-') {
        sign = -1;
        i++;
    }
    
    // 再处理数字
    for (; i < tok->end; i++) {
        if (json[i] >= '0' && json[i] <= '9') {
            num = num * 10 + (json[i] - '0');
        }
    }
    return sign * num;
}

float json_token_to_float(const char *json, jsmntok_t *tok) {
    float result = 0.0f;
    float frac = 0.0f;
    float divisor = 1.0f;
    int sign = 1;
    int i = tok->start;
    int is_frac = 0;  // 是否小数部分
    
    // 符号
    if (i < tok->end && json[i] == '-') {
        sign = -1;
        i++;
    }
    
    // 整数部分 + 小数部分
    for (; i < tok->end; i++) {
        if (json[i] >= '0' && json[i] <= '9') {
            if (!is_frac) {
                result = result * 10 + (json[i] - '0');
            } else {
                divisor *= 10;
                frac = frac * 10 + (json[i] - '0');
            }
        } else if (json[i] == '.') {
            is_frac = 1;  // 遇到小数点，切换模式
        }
    }
    
    return sign * (result + frac / divisor);
}

bool json_token_to_bool(const char *json, jsmntok_t *tok) {
    int len = tok->end - tok->start;
    if (len == 4 && strncmp(json + tok->start, "true", 4) == 0) {
        return true;   // 或 1
    }
    return false;      // "false" 或其他都返回 false
}
int json_token_to_str(const char *json, jsmntok_t *tok, char *dst, int max_len) {
    int len = tok->end - tok->start;
    
    // 检查是否有引号
    if (json[tok->start] != '\"' || json[tok->end - 1] != '\"') {
        // 不是字符串类型，直接复制原始内容
        if (len >= max_len) return -1;
        memcpy(dst, json + tok->start, len);
        dst[len] = '\0';
        return len;
    }
    
    // 去掉首尾引号
    int content_len = len - 2;  // 去掉两个引号
    if (content_len < 0) content_len = 0;
    
    if (content_len >= max_len) return -1;
    
    // 复制内容（暂时不处理转义，见下方进阶版）
    memcpy(dst, json + tok->start + 1, content_len);
    dst[content_len] = '\0';
    
    return content_len;
}
int json_printf(char *buf, int size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, size, fmt, args);  // 带长度限制
    va_end(args);
    return (n >= size) ? -1 : n;  // -1 表示溢出
}
