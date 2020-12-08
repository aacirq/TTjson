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

  // invalid object
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "{\"n\" : nul}");
  TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "{\"t\" : true, \"f\" : fals}");
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

static void test_parse_object() {
  lept_value v;

  lept_init(&v);
  EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "{ }"));
  EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)0, lept_get_object_size(&v));
  lept_free(&v);

  lept_init(&v);
  char *json_txt = "{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}";
  // char *json_txt = " { "
  //                  "    \"n\" : null , "
  //                  "    \"f\" : false , "
  //                  "    \"t\" : true , "
  //                  "    \"i\" : 123 , "
  //                  "    \"s\" : \"abc\", "
  //                  "    \"a\" : [ 1, 2, 3 ],"
  //                  "    \"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
  //                  " } ";
  EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json_txt));
  EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)7, lept_get_object_size(&v));
  // no.0 member "n"
  EXPECT_EQ_STRING("n", lept_get_object_key(&v, 0),
                   lept_get_object_key_length(&v, 0));
  EXPECT_EQ_INT(LEPT_NULL, lept_get_type(lept_get_object_value(&v, 0)));
  // no.1 member "f"
  EXPECT_EQ_STRING("f", lept_get_object_key(&v, 1),
                   lept_get_object_key_length(&v, 1));
  EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(lept_get_object_value(&v, 1)));
  // no.2 member "t"
  EXPECT_EQ_STRING("t", lept_get_object_key(&v, 2),
                   lept_get_object_key_length(&v, 2));
  EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(lept_get_object_value(&v, 2)));
  // no.3 member "i"
  EXPECT_EQ_STRING("i", lept_get_object_key(&v, 3),
                   lept_get_object_key_length(&v, 3));
  EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_object_value(&v, 3)));
  EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_object_value(&v, 3)));
  // no.4 member "s"
  EXPECT_EQ_STRING("s", lept_get_object_key(&v, 4),
                   lept_get_object_key_length(&v, 4));
  EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_object_value(&v, 4)));
  EXPECT_EQ_STRING("abc", lept_get_string(lept_get_object_value(&v, 4)),
                   lept_get_string_length(lept_get_object_value(&v, 4)));
  // no.5 member "a"
  EXPECT_EQ_STRING("a", lept_get_object_key(&v, 5),
                   lept_get_object_key_length(&v, 5));
  lept_value *value_in_json = lept_get_object_value(&v, 5);
  EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(value_in_json));
  EXPECT_EQ_SIZE_T((size_t)3, lept_get_array_size(value_in_json));
  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ_DOUBLE(1.0 + i,
                     lept_get_number(lept_get_array_element(value_in_json, i)));
  // no.6 member "o"
  EXPECT_EQ_STRING("o", lept_get_object_key(&v, 6),
                   lept_get_object_key_length(&v, 6));
  value_in_json = lept_get_object_value(&v, 6);
  EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(value_in_json));
  EXPECT_EQ_SIZE_T((size_t)3, lept_get_object_size(value_in_json));
  char sub_key[] = "0";
  for (size_t i = 0; i < 3; ++i) {
    sub_key[0] = '1' + i;
    EXPECT_EQ_STRING(sub_key, lept_get_object_key(value_in_json, i),
                     lept_get_object_key_length(value_in_json, i));
    EXPECT_EQ_DOUBLE(1.0 + i,
                     lept_get_number(lept_get_object_value(value_in_json, i)));
  }
  lept_free(&v);
}

static void test_parse_miss_key() {
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{1:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{true:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{false:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{null:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{[]:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{{}:1,");
  TEST_ERROR(LEPT_PARSE_MISS_KEY, "{\"a\":1,");
}

static void test_parse_miss_colon() {
  TEST_ERROR(LEPT_PARSE_MISS_COLON, "{\"a\"}");
  TEST_ERROR(LEPT_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket() {
  TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
  TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
  TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
  TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
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
  test_parse_miss_key();
  test_parse_miss_colon();
  test_parse_miss_comma_or_curly_bracket();
  test_parse_object();

  test_access_string();
  test_access_boolean();
  test_access_number();
}

#define TEST_ROUNDTRIP(json) \
  do { \
    lept_value v; \
    lept_init(&v); \
    char *json2; \
    size_t len; \
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
    EXPECT_EQ_INT(LEPT_STRINGIFY_OK, lept_stringify(&v, &json2, &len)); \
    EXPECT_EQ_STRING(json, json2, len); \
    lept_free(&v); \
  } while(0)

static void test_stringify_number() {
  TEST_ROUNDTRIP("0");
  TEST_ROUNDTRIP("-0");
  TEST_ROUNDTRIP("1");
  TEST_ROUNDTRIP("-1");
  TEST_ROUNDTRIP("1.5");
  TEST_ROUNDTRIP("-1.5");
  TEST_ROUNDTRIP("3.25");
  TEST_ROUNDTRIP("1e+20");
  TEST_ROUNDTRIP("1.234e+20");
  TEST_ROUNDTRIP("1.234e-20");

  TEST_ROUNDTRIP("1.0000000000000002"); /* the smallest number > 1 */
  TEST_ROUNDTRIP("4.9406564584124654e-324"); /* minimum denormal */
  TEST_ROUNDTRIP("-4.9406564584124654e-324");
  TEST_ROUNDTRIP("2.2250738585072009e-308");  /* Max subnormal double */
  TEST_ROUNDTRIP("-2.2250738585072009e-308");
  TEST_ROUNDTRIP("2.2250738585072014e-308");  /* Min normal positive double */
  TEST_ROUNDTRIP("-2.2250738585072014e-308");
  TEST_ROUNDTRIP("1.7976931348623157e+308");  /* Max double */
  TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

static void test_stringify_array() {
  TEST_ROUNDTRIP("[]");
  TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

static void test_stringify_object() {
  TEST_ROUNDTRIP("{}");
  TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

static void test_stringify_string() {
  TEST_ROUNDTRIP("\"\"");
  TEST_ROUNDTRIP("\"Hello\"");
  TEST_ROUNDTRIP("\"Hello\\nWorld\"");
  TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
  TEST_ROUNDTRIP("\"Hello\\u0000World\"");
}

static void test_stringify() {
  TEST_ROUNDTRIP("null");
  TEST_ROUNDTRIP("true");
  TEST_ROUNDTRIP("false");
  test_stringify_number();
  test_stringify_string();
  test_stringify_array();
  test_stringify_object();
}

int main() {
  test_parse();
  test_stringify();
  printf("%d/%d (%3.2f%%) passed\n",
         test_pass, test_count, test_pass * 100.0 / test_count);
  return main_ret;
}