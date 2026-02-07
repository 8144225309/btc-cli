/* Minimal JSON parser for RPC responses */

#include "json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

const char *json_skip_ws(const char *p)
{
	while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	return p;
}

const char *json_find_closing(const char *p)
{
	char open, close;
	int depth = 1;

	if (*p == '{') { open = '{'; close = '}'; }
	else if (*p == '[') { open = '['; close = ']'; }
	else return NULL;

	p++;
	while (*p && depth > 0) {
		if (*p == '"') {
			p++;
			while (*p && *p != '"') {
				if (*p == '\\' && *(p+1)) p++;
				p++;
			}
			if (*p == '"') p++;
		} else if (*p == open) {
			depth++; p++;
		} else if (*p == close) {
			depth--;
			if (depth == 0) return p;
			p++;
		} else {
			p++;
		}
	}
	return NULL;
}

const char *json_find_value(const char *json, const char *key)
{
	char pattern[512];
	const char *p;

	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = json;
	while ((p = strstr(p, pattern)) != NULL) {
		p += strlen(pattern);
		p = json_skip_ws(p);
		if (*p == ':') {
			p++;
			return json_skip_ws(p);
		}
	}
	return NULL;
}

int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
	const char *val = json_find_value(json, key);
	const char *p;
	size_t i = 0;

	if (!val || *val != '"') {
		if (out_size > 0) out[0] = 0;
		return -1;
	}
	p = val + 1;
	while (*p && *p != '"' && i < out_size - 1) {
		if (*p == '\\' && *(p+1)) {
			p++;
			switch (*p) {
			case 'n': out[i++] = '\n'; break;
			case 'r': out[i++] = '\r'; break;
			case 't': out[i++] = '\t'; break;
			case '"': out[i++] = '"'; break;
			case '\\': out[i++] = '\\'; break;
			case '/': out[i++] = '/'; break;
			default: out[i++] = *p; break;
			}
		} else {
			out[i++] = *p;
		}
		p++;
	}
	out[i] = 0;
	return (int)i;
}

int64_t json_get_int(const char *json, const char *key)
{
	const char *val = json_find_value(json, key);
	if (!val) return 0;
	return strtoll(val, NULL, 10);
}

double json_get_double(const char *json, const char *key)
{
	const char *val = json_find_value(json, key);
	if (!val) return 0.0;
	return strtod(val, NULL);
}

int json_is_null(const char *json, const char *key)
{
	const char *val = json_find_value(json, key);
	if (!val) return 0;
	return (strncmp(val, "null", 4) == 0);
}

const char *json_find_array(const char *json, const char *key)
{
	char pattern[512];
	const char *p;

	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = json;
	while ((p = strstr(p, pattern)) != NULL) {
		p += strlen(pattern);
		p = json_skip_ws(p);
		if (*p == ':') {
			p++;
			p = json_skip_ws(p);
			if (*p == '[')
				return p;
		}
	}
	return NULL;
}

const char *json_find_object(const char *json, const char *key)
{
	char pattern[512];
	const char *p;

	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = json;
	while ((p = strstr(p, pattern)) != NULL) {
		p += strlen(pattern);
		p = json_skip_ws(p);
		if (*p == ':') {
			p++;
			p = json_skip_ws(p);
			if (*p == '{')
				return p;
		}
	}
	return NULL;
}

const char *json_array_next(const char *pos, const char **end)
{
	const char *p, *elem_start;

	if (!pos) return NULL;
	p = json_skip_ws(pos);

	if (*p == '[' || *p == ',') {
		p++;
		p = json_skip_ws(p);
	}

	if (*p == ']' || *p == 0) return NULL;

	elem_start = p;
	if (*p == '{' || *p == '[') {
		const char *closing = json_find_closing(p);
		if (closing)
			*end = closing + 1;
		else {
			*end = p;
			return NULL;
		}
	} else if (*p == '"') {
		p++;
		while (*p && *p != '"') {
			if (*p == '\\' && *(p+1)) p++;
			p++;
		}
		if (*p == '"') p++;
		*end = p;
	} else {
		while (*p && *p != ',' && *p != ']' && *p != '}' && !isspace(*p))
			p++;
		*end = p;
	}
	return elem_start;
}

int json_array_count(const char *arr)
{
	const char *pos = arr;
	const char *elem, *end;
	int count = 0;

	while ((elem = json_array_next(pos, &end)) != NULL) {
		count++;
		pos = end;
	}
	return count;
}

char *json_element_copy(const char *elem, const char *elem_end, char *buf, size_t buf_size)
{
	size_t len = elem_end - elem;
	if (len >= buf_size) return NULL;
	memcpy(buf, elem, len);
	buf[len] = 0;
	return buf;
}
