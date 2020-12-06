#include <stdio.h>
#include <string.h>

#include "leptjson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
  do { \
    ++test_count; \
    if(equality) { \
      ++test_pass; \
    } else { \
      fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", \
              __FILE__, __LINE__, expect, actual); \
      main_ret = 1; \
    } \
  } while(0)

#define EXPECT_EQ_INT(expect, actual) \
        EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual) \
        EXPECT_EQ_BASE((expect) == actual, expect, actual, "%f")
#define EXPECT_EQ_TRUE(actual) \
  do { EXPECT_EQ_BASE(actual != 0, "true", "false", "%s"); } while(0)
#define EXPECT_EQ_FALSE(actual) \
  do { EXPECT_EQ_BASE(actual == 0, "false", "true", "%s"); } while(0)
#define EXPECT_EQ_STRING(expect, string, alength) \
  do { EXPECT_EQ_BASE(memcmp(expect, string, alength) == 0, \
                      expect, string, "%s"); \
  } while(0)
#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) \
        EXPECT_EQ_BASE((expect) == (actual), (expect), (actual), "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) \
        EXPECT_EQ_BASE((expect) == (actual), (expect), (actual), "%zu")
#endif


#define TEST_ERROR(error, json) \
  do { \
    lept_value v; \
    v.type = LEPT_FALSE; \
    EXPECT_EQ_INT(error, lept_parse(&v, json)); \
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v)); \
  } while(0)

#define TEST_NUMBER(expect, json) \
  do { \
    lept_value v; \
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v)); \
    EXPECT_EQ_DOUBLE(expect, lept_get_number(&v)); \
  } while(0)

#define TEST_STRING(expect, json) \
  do { \
    lept_value v; \
    lept_init(&v); \
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(&v)); \
    EXPECT_EQ_STRING(expect, lept_get_string(&v), lept_get_string_length(&v)); \
    lept_free(&v); \
  } while(0)

static void test_parse_null() {
  lept_value v;
  v.type = LEPT_TRUE;
  EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "null"));
  EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
}

static void test_parse_true() {
  lept_value v;
  v.type = LEPT_NULL;
  EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "true"));
  EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(&v));
}

static void test_parse_false() {
  lept_value v;
  v.type = LEPT_NULL;
  EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "false"));
  EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(&v));
}

static void test_parse_expect_value() {
  TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "");
  TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "  ");
}

static void test_parse_invalid_value() {
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nul");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "n");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "?");
  // invalid number
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+0");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+1");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, ".123");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "1.1e");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "1.1e+");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "1.");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "INF");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "inf");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "NAN");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nan");

  // invalid array
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "[1,]");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "[\"a\", nul]");
}

static void test_parse_root_not_singular() {
  TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "null x");
  // invalid number
  TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0123");
  TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0x0");
  TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0x123");
}

static void test_parse_number_too_big() {
  TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "1e309");
  TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "-1e309");
}

static void test_parse_number() {
  TEST_NUMBER(0.0,        "0");
  TEST_NUMBER(0.0,        "-0");
  TEST_NUMBER(0.0,        "-0.0");
  TEST_NUMBER(1.0,        "1");
  TEST_NUMBER(1.0,        "1.0");
  TEST_NUMBER(-1.0,       "-1");
  TEST_NUMBER(1.5,        "1.5");
  TEST_NUMBER(-1.5,       "-1.5");
  TEST_NUMBER(3.1416,     "3.1416");
  TEST_NUMBER(1E10,       "1E10");
  TEST_NUMBER(1e10,       "1e10");
  TEST_NUMBER(1E+10,      "1E+10");
  TEST_NUMBER(1E-10,      "1E-10");
  TEST_NUMBER(-1E10,      "-1E10");
  TEST_NUMBER(-1e10,      "-1e10");
  TEST_NUMBER(-1E+10,     "-1E+10");
  TEST_NUMBER(-1E-10,     "-1E-10");
  TEST_NUMBER(1.234E+10,  "1.234E+10");
  TEST_NUMBER(1.234E-10,  "1.234E-10");
  TEST_NUMBER(0.0,        "1e-10000");
  __int64_t i;
  i = 0x0000000000000001;
  TEST_NUMBER(*(double *)&i, "4.9406564584124654e-324"); // Min. subnormal positive double
  i = 0x000FFFFFFFFFFFFF;
  TEST_NUMBER(*(double *)&i, "2.2250738585072009e-308"); // Max. subnormal double
  i = 0x0010000000000000;
  TEST_NUMBER(*(double *)&i, "2.2250738585072014e-308"); // Min. normal positive double
  i = 0x7FEFFFFFFFFFFFFF;
  TEST_NUMBER(*(double *)&i, "1.7976931348623157e308");  // Max. double
}

static void test_parse_string() {
  TEST_STRING("",                        "\"\"");
  TEST_STRING("Hello",                   "\"Hello\"");
  TEST_STRING("Hello\nWorld",            "\"Hello\\nWorld\"");
  TEST_STRING("\" \\ / \b \f \n \r \t",  "\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
  TEST_STRING("Hello\0World",            "\"Hello\\u0000World\"");
  TEST_STRING("\x24",                    "\"\\u0024\"");        // Dollar sign U+0024
  TEST_STRING("\xC2\xA2",                "\"\\u00A2\"");        // Cents sign U+00A2
  TEST_STRING("\xE2\x82\xAC",            "\"\\u20AC\"");        // Euro sign U+20AC
  TEST_STRING("\xF0\x9D\x84\x9E",        "\"\\uD834\\uDD1E\""); // G clef sign U+1D11E
  TEST_STRING("\xF0\x9D\x84\x9E",        "\"\\ud834\\udd1e\""); // G clef sign U+1D11E
}

static void test_parse_invalid_string_escape() {
  TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
  TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
  TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
  TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}

static void test_parse_invalid_string_char() {
  TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
  TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

static void test_parse_array() {
  lept_value v;
  lept_init(&v);
  EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ ]"));
  EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)0, lept_get_array_size(&v));
  lept_free(&v);

  //
  // case 1
  lept_init(&v);
  EXPECT_EQ_INT(LEPT_PARSE_OK,
                lept_parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
  EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
  EXPECT_EQ_INT(5, (int)lept_get_array_size(&v));
  lept_value *e;
  EXPECT_EQ_INT(LEPT_NULL,   lept_get_type(lept_get_array_element(&v, 0)));
  EXPECT_EQ_INT(LEPT_FALSE,  lept_get_type(lept_get_array_element(&v, 1)));
  EXPECT_EQ_INT(LEPT_TRUE,   lept_get_type(lept_get_array_element(&v, 2)));
  EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_array_element(&v, 3)));
  EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_array_element(&v, 4)));
  EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_array_element(&v, 3)));
  EXPECT_EQ_STRING("abc", lept_get_string(lept_get_array_element(&v, 4)),
                   lept_get_string_length(lept_get_array_element(&v, 4)));
  lept_free(&v);

  //
  // case 2
  lept_init(&v);
  lept_value *sub_e;
  EXPECT_EQ_INT(LEPT_PARSE_OK,
                lept_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
  EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
  for (size_t index = 0; index < lept_get_array_size(&v); ++index) {
    e = lept_get_array_element(&v, index);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(e));
    EXPECT_EQ_SIZE_T(index, lept_get_array_size(e));
    for (size_t j = 0; j < index; ++j) {
      sub_e = lept_get_array_element(e, j);
      EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(sub_e));
      EXPECT_EQ_DOUBLE((double)j, lept_get_number(sub_e));
    }
  }
  lept_free(&v);
}

static void test_access_string() {
  lept_value v;
  lept_init(&v);
  lept_set_string(&v, "Hello", 5);
  EXPECT_EQ_STRING("Hello", lept_get_string(&v), lept_get_string_length(&v));
  lept_set_string(&v, "", 0);
  EXPECT_EQ_STRING("", lept_get_string(&v), lept_get_string_length(&v));
  lept_free(&v);
}

static void test_access_boolean() {
  lept_value v;
  lept_init(&v);
  lept_set_string(&v, "a", 1);
  lept_set_boolean(&v, 1);
  EXPECT_EQ_TRUE(lept_get_boolean(&v));
  lept_set_boolean(&v, 0);
  EXPECT_EQ_FALSE(lept_get_boolean(&v));
  lept_free(&v);
}

static void test_access_number() {
  lept_value v;
  lept_init(&v);
  lept_set_string(&v, "a", 1);
  lept_set_number(&v, 1234.5);
  EXPECT_EQ_DOUBLE(1234.5, lept_get_number(&v));
  lept_free(&v);
}

static void test_parse() {
  test_parse_null();
  test_parse_true();
  test_parse_false();
  test_parse_number();
  test_parse_expect_value();
  test_parse_invalid_value();
  test_parse_root_not_singular();
  test_parse_number_too_big();
  test_parse_string();
  test_parse_invalid_string_escape();
  test_parse_invalid_string_char();
  test_parse_array();

  test_access_string();
  test_access_boolean();
  test_access_number();
}

int main() {
  test_parse();
  printf("%d/%d (%3.2f%%) passed\n",
         test_pass, test_count, test_pass * 100.0 / test_count);
  return main_ret;
}