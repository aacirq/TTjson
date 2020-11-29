#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "leptjson.h"

#define EXPECT(c, ch) do { assert(*c->json == (ch)); c->json++; } while(0)

#define ISDIGIT(ch)      ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)  ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char *json;
} lept_context;

static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    c->json = p;
}

#if 0
static int lept_parse_null(lept_context *c, lept_value *v) {
    EXPECT(c, 'n');
    if(c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l') 
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

static int lept_parse_true(lept_context *c, lept_value *v) {
    EXPECT(c, 't');
    if(c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context *c, lept_value *v) {
    EXPECT(c, 'f');
    if(c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}
#endif

static int lept_parse_literal(lept_context *c, lept_value *v) {
    char *literal[] = { "null", "false", "true" };
    int ind = -1;
    switch(*c->json) {
        case 'n': ind = 0; break;
        case 'f': ind = 1; break;
        case 't': ind = 2; break;
        default:  break;
    }
    assert(ind >= 0);
    ++c->json;
    char *p = literal[ind] + 1;
    int i = 0;
    while(*p && *p == c->json[i++])
        ++p;
    if(*p != '\0')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    switch(ind) {
        case 0:  v->type = LEPT_NULL;  break;
        case 1:  v->type = LEPT_FALSE; break;
        case 2:  v->type = LEPT_TRUE;  break;
        default: v->type = LEPT_NULL;  break;
    }
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context *c, lept_value *v) {
    char *end;
    /* validate number */
    int pos = 0;
    if(c->json[pos] == '-')
        ++pos;
    if(c->json[pos] == '0') {
        ++pos;
        if(c->json[pos] != '.' && c->json[pos] != 'e' && c->json[pos] != 'E'){
            v->type = LEPT_NUMBER;
            v->n = 0;
            c->json += pos;
            return LEPT_PARSE_OK;
        }
    }
    else if(ISDIGIT1TO9(c->json[pos])) {
        while(ISDIGIT(c->json[pos]))
            ++pos;
    }
    else
        return LEPT_PARSE_INVALID_VALUE;
    if(c->json[pos] == '.') {
        ++pos;
        if(!ISDIGIT(c->json[pos]))
            return LEPT_PARSE_INVALID_VALUE;
        while(ISDIGIT(c->json[pos]))
            ++pos;
    }
    if(c->json[pos] == 'e' || c->json[pos] == 'E') {
        ++pos;
        if(c->json[pos] == '+' || c->json[pos] == '-')
            ++pos;
        if(!ISDIGIT(c->json[pos]))
            return LEPT_PARSE_INVALID_VALUE;
    }
    else if(c->json[pos] != '\0')
        return LEPT_PARSE_INVALID_VALUE;

    v->n = strtod(c->json, &end);
    if(errno == ERANGE) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
        errno = 0;
    }
    if(c->json == end)
        return LEPT_PARSE_INVALID_VALUE;
    c->json = end;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context *c, lept_value *v) {
    switch(*c->json) {
        case 'n'://  return lept_parse_null(c, v);
        case 't'://  return lept_parse_true(c, v);
        case 'f':  return lept_parse_literal(c, v);
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
    return LEPT_PARSE_OK;
}

int lept_parse(lept_value *v, const char *json) {
    lept_context c;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    int ret;
    if((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if(*c.json != '\0') {
            v->type = LEPT_NULL;
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_value(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}