/* Output formatting extensions: -field, -sats, -format=table/csv */

#define _GNU_SOURCE
#include "format.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── -field=path ───────────────────────────────────────────────────── */

char *format_extract_field(const char *json, const char *path)
{
	char segment[256];
	const char *p = path;
	const char *current = json;

	/* Skip leading whitespace */
	while (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r')
		current++;

	while (*p) {
		/* Extract next path segment (before '.' or end) */
		const char *dot = strchr(p, '.');
		size_t seg_len = dot ? (size_t)(dot - p) : strlen(p);
		if (seg_len >= sizeof(segment)) seg_len = sizeof(segment) - 1;
		memcpy(segment, p, seg_len);
		segment[seg_len] = '\0';

		/* Check if segment is an array index */
		int is_index = 1;
		size_t si;
		for (si = 0; si < seg_len; si++) {
			if (!isdigit((unsigned char)segment[si])) { is_index = 0; break; }
		}

		if (is_index && *current == '[') {
			/* Array indexing */
			int target = atoi(segment);
			int idx = 0;
			const char *elem = current + 1;
			while (*elem) {
				while (*elem == ' ' || *elem == '\n' || *elem == '\r' || *elem == '\t')
					elem++;
				if (*elem == ']') { current = NULL; break; }
				if (idx == target) {
					current = elem;
					break;
				}
				/* Skip this element */
				if (*elem == '{' || *elem == '[') {
					const char *end = json_find_closing(elem);
					if (!end) { current = NULL; break; }
					elem = end + 1;
				} else if (*elem == '"') {
					elem++;
					while (*elem && *elem != '"') {
						if (*elem == '\\') elem++;
						elem++;
					}
					if (*elem == '"') elem++;
				} else {
					while (*elem && *elem != ',' && *elem != ']')
						elem++;
				}
				while (*elem == ' ' || *elem == '\n' || *elem == '\r' || *elem == '\t')
					elem++;
				if (*elem == ',') elem++;
				idx++;
			}
			if (!current) return NULL;
		} else if (*current == '{') {
			/* Object key lookup */
			const char *val = json_find_value(current, segment);
			if (!val) return NULL;
			current = val;
		} else {
			return NULL;
		}

		p += seg_len;
		if (*p == '.') p++;
	}

	if (!current) return NULL;

	/* Extract the value at current position */
	while (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r')
		current++;

	if (*current == '"') {
		/* String — extract without quotes */
		const char *start = current + 1;
		const char *end = start;
		while (*end && *end != '"') {
			if (*end == '\\') end++;
			end++;
		}
		size_t len = end - start;
		char *result = malloc(len + 1);
		if (!result) return NULL;
		memcpy(result, start, len);
		result[len] = '\0';
		return result;
	} else if (*current == '{' || *current == '[') {
		const char *end = json_find_closing(current);
		if (!end) return NULL;
		size_t len = end - current + 1;
		char *result = malloc(len + 1);
		if (!result) return NULL;
		memcpy(result, current, len);
		result[len] = '\0';
		return result;
	} else if (strncmp(current, "null", 4) == 0) {
		return strdup("null");
	} else {
		/* Number or boolean */
		const char *end = current;
		while (*end && *end != ',' && *end != '}' && *end != ']' &&
		       *end != ' ' && *end != '\n' && *end != '\r')
			end++;
		size_t len = end - current;
		char *result = malloc(len + 1);
		if (!result) return NULL;
		memcpy(result, current, len);
		result[len] = '\0';
		return result;
	}
}

/* ─── -sats ─────────────────────────────────────────────────────────── */

/* Check if a numeric string has exactly 8 decimal places (BTC amount) */
static int is_btc_amount(const char *s)
{
	const char *dot = strchr(s, '.');
	if (!dot) return 0;

	/* Check all chars are digits (allow leading minus) */
	const char *p = s;
	if (*p == '-') p++;
	while (p < dot) {
		if (!isdigit((unsigned char)*p)) return 0;
		p++;
	}

	/* Count decimal places */
	p = dot + 1;
	int decimals = 0;
	while (*p && isdigit((unsigned char)*p)) {
		decimals++;
		p++;
	}
	/* Must be exactly 8 decimal places and nothing after */
	return decimals == 8 && (*p == '\0' || *p == ',' || *p == '}' ||
	       *p == ']' || *p == ' ' || *p == '\n' || *p == '\r');
}

char *format_sats(const char *json)
{
	size_t len = strlen(json);
	size_t bufsize = len * 2 + 1;
	char *buf = malloc(bufsize);
	if (!buf) return NULL;

	const char *p = json;
	size_t pos = 0;
	int in_string = 0;
	int after_colon = 0;

	while (*p) {
		/* Grow buffer if needed */
		if (pos + 32 > bufsize) {
			bufsize *= 2;
			buf = realloc(buf, bufsize);
			if (!buf) return NULL;
		}

		if (*p == '"' && (p == json || *(p-1) != '\\')) {
			in_string = !in_string;
			buf[pos++] = *p++;
			after_colon = 0;
			continue;
		}

		if (in_string) {
			buf[pos++] = *p++;
			continue;
		}

		if (*p == ':') {
			buf[pos++] = *p++;
			after_colon = 1;
			continue;
		}

		/* Check for BTC amount (number after colon or in array) */
		if ((*p == '-' || isdigit((unsigned char)*p)) &&
		    (after_colon || (pos > 0 && (buf[pos-1] == '[' || buf[pos-1] == ',')))) {
			/* Capture the number */
			const char *numstart = p;
			if (*p == '-') p++;
			while (isdigit((unsigned char)*p)) p++;
			if (*p == '.') {
				p++;
				while (isdigit((unsigned char)*p)) p++;
			}
			size_t numlen = p - numstart;
			char numbuf[64];
			if (numlen < sizeof(numbuf)) {
				memcpy(numbuf, numstart, numlen);
				numbuf[numlen] = '\0';
				if (is_btc_amount(numbuf)) {
					/* Convert to satoshis */
					double btc = strtod(numbuf, NULL);
					long long sats = (long long)(btc * 100000000.0 + (btc >= 0 ? 0.5 : -0.5));
					pos += snprintf(buf + pos, bufsize - pos, "%lld", sats);
				} else {
					memcpy(buf + pos, numstart, numlen);
					pos += numlen;
				}
			} else {
				memcpy(buf + pos, numstart, numlen);
				pos += numlen;
			}
			after_colon = 0;
			continue;
		}

		if (*p == ',' || *p == '{' || *p == '[')
			after_colon = 0;

		buf[pos++] = *p++;
	}

	buf[pos] = '\0';
	return buf;
}

/* ─── -format=table ─────────────────────────────────────────────────── */

/* Max columns and max column width */
#define TABLE_MAX_COLS 32
#define TABLE_MAX_WIDTH 60

/* Extract a simple value from JSON (string, number, bool, null) as a string */
static void extract_value(const char *val, char *out, size_t out_size)
{
	if (!val || !*val) { out[0] = '\0'; return; }

	while (*val == ' ' || *val == '\t' || *val == '\n' || *val == '\r')
		val++;

	if (*val == '"') {
		/* String */
		val++;
		size_t i = 0;
		while (*val && *val != '"' && i < out_size - 1) {
			if (*val == '\\' && *(val+1)) {
				val++;
				out[i++] = *val;
			} else {
				out[i++] = *val;
			}
			val++;
		}
		out[i] = '\0';
	} else if (*val == '{' || *val == '[') {
		/* Complex value — show truncated */
		const char *end = json_find_closing(val);
		size_t len = end ? (size_t)(end - val + 1) : strlen(val);
		if (len >= out_size) len = out_size - 1;
		memcpy(out, val, len);
		out[len] = '\0';
	} else {
		/* Number, boolean, null */
		size_t i = 0;
		while (*val && *val != ',' && *val != '}' && *val != ']' &&
		       *val != '\n' && i < out_size - 1) {
			out[i++] = *val++;
		}
		out[i] = '\0';
	}

	/* Truncate long values */
	if (strlen(out) > TABLE_MAX_WIDTH) {
		out[TABLE_MAX_WIDTH - 3] = '.';
		out[TABLE_MAX_WIDTH - 2] = '.';
		out[TABLE_MAX_WIDTH - 1] = '.';
		out[TABLE_MAX_WIDTH] = '\0';
	}
}

int format_table(FILE *out, const char *json)
{
	/* Skip whitespace */
	while (*json == ' ' || *json == '\t' || *json == '\n' || *json == '\r')
		json++;

	if (*json != '[') return -1;

	/* Collect keys from first object */
	char keys[TABLE_MAX_COLS][128];
	int widths[TABLE_MAX_COLS];
	int ncols = 0;

	const char *first_obj = strchr(json + 1, '{');
	if (!first_obj) return -1;

	/* Parse keys from first object */
	{
		const char *end = json_find_closing(first_obj);
		if (!end) return -1;
		const char *p = first_obj + 1;
		while (p < end && ncols < TABLE_MAX_COLS) {
			/* Find next key */
			const char *kstart = strchr(p, '"');
			if (!kstart || kstart >= end) break;
			kstart++;
			const char *kend = strchr(kstart, '"');
			if (!kend || kend >= end) break;

			size_t klen = kend - kstart;
			if (klen >= sizeof(keys[0])) klen = sizeof(keys[0]) - 1;
			memcpy(keys[ncols], kstart, klen);
			keys[ncols][klen] = '\0';
			widths[ncols] = (int)klen;
			ncols++;

			/* Skip past value */
			p = kend + 1;
			while (p < end && *p != ':') p++;
			if (p < end) p++;
			while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
			if (p < end) {
				if (*p == '{' || *p == '[') {
					const char *vend = json_find_closing(p);
					if (vend) p = vend + 1;
				} else if (*p == '"') {
					p++;
					while (p < end && *p != '"') {
						if (*p == '\\') p++;
						p++;
					}
					if (p < end) p++;
				} else {
					while (p < end && *p != ',' && *p != '}') p++;
				}
			}
			while (p < end && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
				p++;
		}
	}

	if (ncols == 0) return -1;

	/* First pass: compute column widths from all rows */
	{
		const char *arr_end = json_find_closing(json);
		if (!arr_end) return -1;
		const char *obj = first_obj;
		while (obj && obj < arr_end) {
			const char *obj_end = json_find_closing(obj);
			if (!obj_end) break;

			/* Extract each column's value width */
			size_t obj_len = obj_end - obj + 1;
			char *obj_copy = malloc(obj_len + 1);
			if (obj_copy) {
				int c;
				memcpy(obj_copy, obj, obj_len);
				obj_copy[obj_len] = '\0';
				for (c = 0; c < ncols; c++) {
					const char *val = json_find_value(obj_copy, keys[c]);
					char valbuf[TABLE_MAX_WIDTH + 4];
					extract_value(val, valbuf, sizeof(valbuf));
					int vlen = (int)strlen(valbuf);
					if (vlen > widths[c]) widths[c] = vlen;
				}
				free(obj_copy);
			}

			obj = strchr(obj_end + 1, '{');
		}
	}

	/* Cap widths */
	{
		int c;
		for (c = 0; c < ncols; c++) {
			if (widths[c] > TABLE_MAX_WIDTH) widths[c] = TABLE_MAX_WIDTH;
		}
	}

	/* Print header */
	{
		int c;
		for (c = 0; c < ncols; c++) {
			if (c > 0) fprintf(out, "  ");
			fprintf(out, "%-*s", widths[c], keys[c]);
		}
		fprintf(out, "\n");
		/* Separator */
		for (c = 0; c < ncols; c++) {
			int w;
			if (c > 0) fprintf(out, "  ");
			for (w = 0; w < widths[c]; w++) fputc('-', out);
		}
		fprintf(out, "\n");
	}

	/* Print rows */
	{
		const char *arr_end = json_find_closing(json);
		if (!arr_end) return -1;
		const char *obj = first_obj;
		while (obj && obj < arr_end) {
			const char *obj_end = json_find_closing(obj);
			if (!obj_end) break;

			size_t obj_len = obj_end - obj + 1;
			char *obj_copy = malloc(obj_len + 1);
			if (obj_copy) {
				int c;
				memcpy(obj_copy, obj, obj_len);
				obj_copy[obj_len] = '\0';
				for (c = 0; c < ncols; c++) {
					const char *val = json_find_value(obj_copy, keys[c]);
					char valbuf[TABLE_MAX_WIDTH + 4];
					extract_value(val, valbuf, sizeof(valbuf));
					if (c > 0) fprintf(out, "  ");
					fprintf(out, "%-*s", widths[c], valbuf);
				}
				fprintf(out, "\n");
				free(obj_copy);
			}

			obj = strchr(obj_end + 1, '{');
		}
	}

	return 0;
}

/* ─── -format=csv ───────────────────────────────────────────────────── */

/* Escape a value for CSV output (quote if contains comma, quote, or newline) */
static void csv_write_value(FILE *out, const char *val)
{
	int needs_quote = 0;
	const char *p;

	for (p = val; *p; p++) {
		if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
			needs_quote = 1;
			break;
		}
	}

	if (needs_quote) {
		fputc('"', out);
		for (p = val; *p; p++) {
			if (*p == '"') fputc('"', out);  /* Escape quotes by doubling */
			fputc(*p, out);
		}
		fputc('"', out);
	} else {
		fputs(val, out);
	}
}

int format_csv(FILE *out, const char *json)
{
	/* Skip whitespace */
	while (*json == ' ' || *json == '\t' || *json == '\n' || *json == '\r')
		json++;

	if (*json != '[') return -1;

	/* Collect keys from first object */
	char keys[TABLE_MAX_COLS][128];
	int ncols = 0;

	const char *first_obj = strchr(json + 1, '{');
	if (!first_obj) return -1;

	/* Parse keys from first object */
	{
		const char *end = json_find_closing(first_obj);
		if (!end) return -1;
		const char *p = first_obj + 1;
		while (p < end && ncols < TABLE_MAX_COLS) {
			const char *kstart = strchr(p, '"');
			if (!kstart || kstart >= end) break;
			kstart++;
			const char *kend = strchr(kstart, '"');
			if (!kend || kend >= end) break;

			size_t klen = kend - kstart;
			if (klen >= sizeof(keys[0])) klen = sizeof(keys[0]) - 1;
			memcpy(keys[ncols], kstart, klen);
			keys[ncols][klen] = '\0';
			ncols++;

			/* Skip past value */
			p = kend + 1;
			while (p < end && *p != ':') p++;
			if (p < end) p++;
			while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
			if (p < end) {
				if (*p == '{' || *p == '[') {
					const char *vend = json_find_closing(p);
					if (vend) p = vend + 1;
				} else if (*p == '"') {
					p++;
					while (p < end && *p != '"') {
						if (*p == '\\') p++;
						p++;
					}
					if (p < end) p++;
				} else {
					while (p < end && *p != ',' && *p != '}') p++;
				}
			}
			while (p < end && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
				p++;
		}
	}

	if (ncols == 0) return -1;

	/* Print header row */
	{
		int c;
		for (c = 0; c < ncols; c++) {
			if (c > 0) fputc(',', out);
			csv_write_value(out, keys[c]);
		}
		fputc('\n', out);
	}

	/* Print data rows */
	{
		const char *arr_end = json_find_closing(json);
		if (!arr_end) return -1;
		const char *obj = first_obj;
		while (obj && obj < arr_end) {
			const char *obj_end = json_find_closing(obj);
			if (!obj_end) break;

			size_t obj_len = obj_end - obj + 1;
			char *obj_copy = malloc(obj_len + 1);
			if (obj_copy) {
				int c;
				memcpy(obj_copy, obj, obj_len);
				obj_copy[obj_len] = '\0';
				for (c = 0; c < ncols; c++) {
					const char *val = json_find_value(obj_copy, keys[c]);
					char valbuf[1024];
					extract_value(val, valbuf, sizeof(valbuf));
					if (c > 0) fputc(',', out);
					csv_write_value(out, valbuf);
				}
				fputc('\n', out);
				free(obj_copy);
			}

			obj = strchr(obj_end + 1, '{');
		}
	}

	return 0;
}
