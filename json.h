/* Minimal JSON parser */

#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include <stdint.h>

const char *json_find_value(const char *json, const char *key);
int json_get_string(const char *json, const char *key, char *out, size_t out_size);
int64_t json_get_int(const char *json, const char *key);
double json_get_double(const char *json, const char *key);
int json_is_null(const char *json, const char *key);
const char *json_find_array(const char *json, const char *key);
const char *json_find_object(const char *json, const char *key);
const char *json_array_next(const char *pos, const char **end);
int json_array_count(const char *arr);
const char *json_skip_ws(const char *p);
const char *json_find_closing(const char *p);
char *json_element_copy(const char *elem, const char *elem_end, char *buf, size_t buf_size);

#endif
