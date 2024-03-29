#include <stdlib.h>
#include <stdio.h>
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
#define STRING_ERROR(ret)  do { c->top = head; return ret; } while(0)
#define PUTC(c, ch) \
    do { \
        *(char *)lept_context_push(c, sizeof(char)) = (ch); \
    } while(0)

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
} lept_context;

static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    c->json = p;
}

static int lept_parse_literal(lept_context *c, lept_value *v,
                              const char *literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; ++i)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context *c, lept_value *v) {
    const char *p = c->json;
    if (*p == '-') ++p;
    if (*p == '0') {
        ++p;
    } else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (++p; ISDIGIT(*p); ++p) ; /* no code segment */
    }
    if (*p == '.') {
        ++p;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (++p; ISDIGIT(*p); ++p) ; /* no code segment */
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-') ++p;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (++p; ISDIGIT(*p); ++p) ; /* no code segment */
    }

    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

void lept_free(lept_value *v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING) {
        free(v->u.s.s);
    } else if (v->type == LEPT_ARRAY) {
        for (size_t index = 0; index < lept_get_array_size(v); ++index)
            lept_free(lept_get_array_element(v, index));
        free(v->u.a.e);
    } else if (v->type == LEPT_OBJECT) {
        for (size_t index = 0; index < lept_get_object_size(v); ++index) {
            free((v->u.o.m + index) -> k);
            lept_free(lept_get_object_value(v, index));
        }
        free(v->u.o.m);
    }
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
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        else
            while (c->top + size > c->size)
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
        if      ('0' <= ch && ch <= '9') *u += ch - '0';
        else if ('A' <= ch && ch <= 'F') *u += ch - 'A' + 10;
        else if ('a' <= ch && ch <= 'f') *u += ch - 'a' + 10;
        else    return NULL;
    }
    return p;
}

static void lept_encode_utf8(lept_context *c, unsigned u) {
    assert(u <= 0x10FFFF);
    if (u <= 0x7F) {
        PUTC(c, u);
    } else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0x1F));
        PUTC(c, 0x80 | ( u       & 0x3F));
    } else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0x0F));
        PUTC(c, 0X80 | ((u >>  6) & 0x3F));
        PUTC(c, 0X80 | ( u        & 0x3F));
    } else {
        PUTC(c, 0xF0 | ((u >> 18) & 0x07));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

static int lept_parse_string_raw(lept_context *c, char **str, size_t *len) {
    size_t head = c->top;
    const char *p;
    unsigned u;
    EXPECT(c, '\"');
    p = c->json;
    while (1) {
        const char ch = *(p++);
        switch (ch) {
            case '\"':
                *len = c->top - head;
                *str = (char *)lept_context_pop(c, *len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/'); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default:   STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                    case 'u':
                        if (!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) {
                            if (!(p[0] == '\\' && p[1] == 'u'))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            p += 2;
                            unsigned l = 0;
                            if (!(p = lept_parse_hex4(p, &l)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (!(l >= 0xDC00 && l <= 0xDFFF))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00);
                        }
                        lept_encode_utf8(c, u);
                        break;
                }
                break;
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int lept_parse_string(lept_context *c, lept_value *v) {
    int ret;
    char *s;
    size_t len;
    if((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK) {
        lept_set_string(v, s, len);
        // s = NULL;
    }
    return ret;
}

static int lept_parse_value(lept_context *c, lept_value *v);

static int lept_parse_array(lept_context *c, lept_value *v) {
    EXPECT(c, '[');
    size_t size = 0;
    int ret;
    lept_parse_whitespace(c);
    if (*c->json == ']') {
        ++(c->json);
        v->type = LEPT_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return LEPT_PARSE_OK;
    }
    while (1) {
        lept_value e;
        lept_init(&e);
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK)
            break;
        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
        ++size;
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            ++(c->json);
            lept_parse_whitespace(c);
        } else if (*c->json == ']') {
            ++(c->json);
            v->type = LEPT_ARRAY;
            v->u.a.size = size;
            size *= sizeof(lept_value);
            memcpy(v->u.a.e = malloc(size), lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        } else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    for(size_t i = 0; i < size; ++i)
        lept_free((lept_value *)lept_context_pop(c, sizeof(lept_value)));
    return ret;
}

static int lept_parse_object(lept_context *c, lept_value *v) {
    EXPECT(c, '{');
    size_t size = 0;
    int ret;
    lept_member m;
    lept_parse_whitespace(c);
    if (*c->json == '}') {
        ++c->json;
        v->type = LEPT_OBJECT;
        v->u.o.size = 0;
        v->u.o.m = NULL;
        return LEPT_PARSE_OK;
    }
    m.k = NULL;
    while (1) {
        char *str;
        lept_init(&m.v);
        if (*c->json != '\"') {
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)
            break;
        memcpy(m.k = (char *)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        ++c->json;
        lept_parse_whitespace(c);
        if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK)
            break;
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
        ++size;
        m.k = NULL;
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            ++c->json;
            lept_parse_whitespace(c);
        } else if (*c->json == '}') {
            ++c->json;
            v->type = LEPT_OBJECT;
            v->u.o.size = size;
            size *= sizeof(lept_member);
            memcpy(v->u.o.m = (lept_member *)malloc(size),
                         lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        } else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    free(m.k);
    for (size_t i = 0; i < size; ++i) {
        lept_member *free_m = (lept_member *)lept_context_pop(c,
                                                              sizeof(lept_member));
        free(free_m->k);
    }
    v->type = LEPT_NULL;
    return ret;
}

static int lept_parse_value(lept_context *c, lept_value *v) {
    switch (*c->json) {
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\"': return lept_parse_string(c, v);
        case '[':  return lept_parse_array(c, v);
        case '{':  return lept_parse_object(c, v);
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
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
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

size_t lept_get_array_size(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}

lept_value *lept_get_array_element(const lept_value *v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.a.size);
    return v->u.a.e + index;
}

size_t lept_get_object_size(const lept_value *v) {
    assert(v != NULL);
    return v->u.o.size;
}

const char *lept_get_object_key(const lept_value *v, size_t index) {
    assert(v != NULL);
    return (v->u.o.m + index) -> k;
}

size_t lept_get_object_key_length(const lept_value *v, size_t index) {
    assert(v != NULL);
    return (v->u.o.m + index) -> klen;
}

lept_value *lept_get_object_value(const lept_value *v, size_t index) {
    assert(v != NULL);
    return &(v->u.o.m + index) -> v;
}

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

#define PUTS(c, s, len) memcpy(lept_context_push(c, len), s, len)

# if 0

static int lept_stringify_string(lept_context *c, const char *str, size_t len) {
    PUTC(c, '\"');
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = str[i];
        switch (ch) {
            case '\"': PUTC(c, '\\'); PUTC(c, '\"'); break;
            case '\\': PUTC(c, '\\'); PUTC(c, '\\'); break;
            case '\b': PUTC(c, '\\'); PUTC(c, 'b');  break;
            case '\f': PUTC(c, '\\'); PUTC(c, 'f');  break;
            case '\n': PUTC(c, '\\'); PUTC(c, 'n');  break;
            case '\r': PUTC(c, '\\'); PUTC(c, 'r');  break;
            case '\t': PUTC(c, '\\'); PUTC(c, 't');  break;
            default:
                if (ch < 0x20) {
                    PUTS(c, "\\u00", 4);
                    PUTC(c, '0' + ch / 10);
                    PUTC(c, '0' + ch % 10);
                } else {
                    PUTC(c, ch);
                }
                break;
        }
    }
    PUTC(c, '\"');
    return LEPT_STRINGIFY_OK;
}

#else
// optimized
static int lept_stringify_string(lept_context *c, const char *str, size_t len) {
    assert(str != NULL);
    size_t size;
    char *p = lept_context_push(c, size = len * 6 + 2);
    char *head = p;
    *p++ = '\"';
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = str[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = '0' + ch / 10;
                    *p++ = '0' + ch % 10;
                } else {
                    *p++ = str[i];
                }
                break;
        }
    }
    *p++ = '\"';
    c->top -= size - (p - head);
    return LEPT_STRINGIFY_OK;
}

#endif

static int lept_stringify_value(lept_context *c, const lept_value *v) {
    switch (v->type) {
        case LEPT_NULL : PUTS(c, "null",  4); break;
        case LEPT_TRUE : PUTS(c, "true",  4); break;
        case LEPT_FALSE: PUTS(c, "false", 5); break;
        case LEPT_NUMBER:
            c->top -= 32 - sprintf(lept_context_push(c, 32), "%.17g", v->u.n);
            break;
        case LEPT_STRING:
            lept_stringify_string(c, v->u.s.s, v->u.s.len);
            break;
        case LEPT_ARRAY:
            PUTC(c, '[');
            for (size_t i = 0; i < v->u.a.size; ++i) {
                if (i > 0) PUTC(c, ',');
                lept_stringify_value(c, v->u.a.e + i);
            }
            PUTC(c, ']');
            break;
        case LEPT_OBJECT:
            PUTC(c, '{');
            for (size_t i = 0; i < v->u.o.size; ++i) {
                if (i > 0) PUTC(c, ',');
                lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                lept_stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: break;
    }
    return LEPT_STRINGIFY_OK;
}

int lept_stringify(const lept_value *v, char **json, size_t *length) {
    lept_context c;
    int ret;
    assert(v != NULL);
    assert(json != NULL);
    c.stack = (char *)malloc(sizeof(char) * LEPT_PARSE_STRINGIFY_INIT_SIZE);
    c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE;
    c.top = 0;
    if ((ret = lept_stringify_value(&c, v)) != LEPT_STRINGIFY_OK) {
        free(c.stack);
        *json = NULL;
        return ret;
    }
    if (length)
        *length = c.top;
    PUTC(&c, '\0');
    *json = c.stack;
    return LEPT_STRINGIFY_OK;
}

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

size_t lept_find_object_index(const lept_value *v, const char *key, size_t klen) {
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    for (size_t i = 0; i < v->u.o.size; ++i)
        if (v->u.o.m[i].klen == klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
            return i;
    return LEPT_KEY_NOT_EXIST;
}

lept_value *lept_find_object_value(lept_value *v, const char *key, size_t klen) {
    size_t index = lept_find_object_index(v, key, klen);
    return index != LEPT_KEY_NOT_EXIST ? &v->u.o.m[index].v : NULL;
}

int lept_is_equal(const lept_value *lhs, const lept_value *rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type)
        return 0;
    switch (lhs->type) {
        case LEPT_STRING:
            return lhs->u.s.len == rhs->u.s.len &&
                         memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
        case LEPT_NUMBER:
            return lhs->u.n == rhs->u.n;
        case LEPT_ARRAY:
            if (lhs->u.a.size != rhs->u.a.size)
                return 0;
            for (size_t i = 0; i < lhs->u.a.size; ++i)
                if (lept_is_equal(lhs->u.a.e + i, rhs->u.a.e + i) == 0)
                    return 0;
            return 1;
        case LEPT_OBJECT:
            if (lhs->u.o.size != rhs->u.o.size)
                return 0;
            for (size_t i = 0; i < lhs->u.o.size; ++i) {
                if (lhs->u.o.m[i].klen != rhs->u.o.m[i].klen)
                    return 0;
                if (memcmp(lhs->u.o.m[i].k, rhs->u.o.m[i].k, lhs->u.o.m[i].klen) != 0)
                    return 0;
                if (lept_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[i].v) == 0)
                    return 0;
            }
            return 1;
        default:
            return 1;
    }
}

void lept_copy(lept_value *dst, const lept_value *src) {
    assert(dst != NULL && src != NULL);
    switch (src->type) {
        case LEPT_STRING:
            lept_set_string(dst, src->u.s.s, src->u.s.len);
            break;
        case LEPT_ARRAY:
            // TODO
            break;
        case LEPT_OBJECT:
            // TODO
            break;
        default:
            lept_free(dst);
            memcpy(dst, src, sizeof(lept_value));
            break;
    }
}