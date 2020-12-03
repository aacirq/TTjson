#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#include "leptjson.h"

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)      do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)        ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)    ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)        do { *(char *)lept_context_push(c, sizeof(char)) = (ch); } while(0)
#define STRING_ERROR(ret)  do { c->top = head; return ret; } while(0)

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
} lept_context;

static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    c->json = p;
}

static int lept_parse_literal(lept_context *c, lept_value *v, const char *literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for(i = 0; literal[i + 1]; ++i)
        if(c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context *c, lept_value *v) {
    const char *p = c->json;
    if(*p == '-') ++p;
    if(*p == '0') ++p;
    else {
        if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(++p; ISDIGIT(*p); ++p) ; /* no code segment */
    }
    if(*p == '.') {
        ++p;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(++p; ISDIGIT(*p); ++p) ; /* no code segment */
    }
    if(*p == 'e' || *p == 'E') {
        ++p;
        if(*p == '+' || *p == '-') ++p;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(++p; ISDIGIT(*p); ++p) ; /* no code segment */
    }

    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if(errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

void lept_free(lept_value *v) {
    assert(v != NULL);
    if(v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

int lept_get_boolean(const lept_value *v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type - LEPT_FALSE;
}

void lept_set_boolean(lept_value *v, int b) {
    assert(v != NULL);
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value *v) {
    assert(v != NULL && (v->type == LEPT_NUMBER));
    return v->u.n;
}

void lept_set_number(lept_value *v, double n){
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_NUMBER;
    v->u.n = n;
}

void lept_set_string(lept_value *v, const char *s, size_t len) {
    assert((v != NULL) && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char *)malloc(sizeof(char) * (len + 1));
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

const char *lept_get_string(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

static void *lept_context_push(lept_context *c, size_t size) {
    void *ret;
    assert(size > 0);
    if(c->top + size > c->size) {
        if(c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        else
            while(c->top + size > c->size)
                c->size += c->size >> 2;
        c->stack = (char *)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void *lept_context_pop(lept_context *c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static const char *lept_parse_hex4(const char *p, unsigned *u) {
    *u = 0;
    for(size_t i = 0; i < 4; ++i) {
        char ch = *p++;
        *u <<= 4;
        if     ('0' <= ch && ch <= '9') *u += ch - '0';
        else if('A' <= ch && ch <= 'F') *u += ch - 'A' + 10;
        else if('a' <= ch && ch <= 'f') *u += ch - 'a' + 10;
        else return NULL;
    }
    return p;
}

static void lept_encode_utf8(lept_context *c, unsigned u) {
    assert (u <= 0x10FFFF);
    if (u <= 0x7F)
        PUTC(c, u);
    else if(u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0x1F));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if(u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0x0F));
        PUTC(c, 0X80 | ((u >>  6) & 0x3F));
        PUTC(c, 0X80 | ( u        & 0x3F));
    }
    else  {
        PUTC(c, 0xF0 | ((u >> 18) & 0x07));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

static int lept_parse_string(lept_context *c, lept_value *v) {
    size_t head = c->top, len;
    const char *p;
    unsigned u;
    EXPECT(c, '\"');
    p = c->json;
    while(1) {
        const char ch = *(p++);
        switch(ch) {
            case '\"':
                len = c->top - head;
                lept_set_string(v, (char *)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            case '\\':
                switch(*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/'); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default:    STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                    case 'u':
                        if(!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if(u >= 0xD800 && u <= 0xDBFF) {
                            if(!(p[0] == '\\' && p[1] == 'u'))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            p += 2;
                            unsigned l = 0;
                            if(!(p = lept_parse_hex4(p, &l)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if(!(l >= 0xDC00 && l <= 0xDFFF))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00);
                        }
                        lept_encode_utf8(c, u);
                        break;
                }
                break;
            default:
                if((unsigned char)ch < 0x20)
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int lept_parse_value(lept_context *c, lept_value *v) {
    switch(*c->json) {
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\"': return lept_parse_string(c, v);
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value *v, const char *json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if(*c.json != '\0') {
            v->type = LEPT_NULL;
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}