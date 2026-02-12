/* Output formatting extensions: -field, -sats, -human, -format=table/csv */

#define _GNU_SOURCE
#include "format.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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

/* ─── -human ────────────────────────────────────────────────────────── */

/* Key categories for humanization */
typedef enum {
	HUMAN_NONE,
	HUMAN_TIMESTAMP,
	HUMAN_BYTES,
	HUMAN_DURATION,
	HUMAN_LARGE_NUMBER,
	HUMAN_PROGRESS
} HumanCategory;

static HumanCategory classify_key(const char *key, size_t len)
{
	/* Timestamps */
	static const char *ts_keys[] = {
		"time", "blocktime", "timereceived", "mediantime",
		"startingtime", "conntime", "lastsend", "lastrecv",
		"last_transaction", "last_block", "ban_created",
		"banned_until", NULL
	};
	/* Byte sizes */
	static const char *byte_keys[] = {
		"size_on_disk", "totalbytesrecv", "totalbytessent", NULL
	};
	/* Durations */
	static const char *dur_keys[] = {
		"uptime", NULL
	};
	/* Large numbers */
	static const char *large_keys[] = {
		"difficulty", "networkhashps", NULL
	};
	/* Progress */
	static const char *prog_keys[] = {
		"verificationprogress", NULL
	};

	const char **list;
	for (list = ts_keys; *list; list++)
		if (strlen(*list) == len && memcmp(*list, key, len) == 0)
			return HUMAN_TIMESTAMP;
	for (list = byte_keys; *list; list++)
		if (strlen(*list) == len && memcmp(*list, key, len) == 0)
			return HUMAN_BYTES;
	for (list = dur_keys; *list; list++)
		if (strlen(*list) == len && memcmp(*list, key, len) == 0)
			return HUMAN_DURATION;
	for (list = large_keys; *list; list++)
		if (strlen(*list) == len && memcmp(*list, key, len) == 0)
			return HUMAN_LARGE_NUMBER;
	for (list = prog_keys; *list; list++)
		if (strlen(*list) == len && memcmp(*list, key, len) == 0)
			return HUMAN_PROGRESS;
	return HUMAN_NONE;
}

/* Format a number according to its category, writing a quoted JSON string.
 * Returns number of chars written, or 0 if no transformation. */
static int humanize_number(char *out, size_t outsize, const char *numstr,
                           size_t numlen, HumanCategory cat)
{
	char tmp[64];
	if (numlen >= sizeof(tmp)) return 0;
	memcpy(tmp, numstr, numlen);
	tmp[numlen] = '\0';

	if (cat == HUMAN_TIMESTAMP) {
		long long val = strtoll(tmp, NULL, 10);
		if (val <= 0) return 0;
		time_t t = (time_t)val;
		struct tm *tm = gmtime(&t);
		if (!tm) return 0;
		return snprintf(out, outsize, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	}

	if (cat == HUMAN_BYTES) {
		double val = strtod(tmp, NULL);
		if (val < 0) return 0;
		if (val >= 1073741824.0)
			return snprintf(out, outsize, "\"%.1f GB\"", val / 1073741824.0);
		if (val >= 1048576.0)
			return snprintf(out, outsize, "\"%.1f MB\"", val / 1048576.0);
		if (val >= 1024.0)
			return snprintf(out, outsize, "\"%.1f KB\"", val / 1024.0);
		return snprintf(out, outsize, "\"%.0f B\"", val);
	}

	if (cat == HUMAN_DURATION) {
		long long secs = strtoll(tmp, NULL, 10);
		if (secs < 0) return 0;
		int d = (int)(secs / 86400);
		int h = (int)((secs % 86400) / 3600);
		int m = (int)((secs % 3600) / 60);
		return snprintf(out, outsize, "\"%dd %dh %dm\"", d, h, m);
	}

	if (cat == HUMAN_LARGE_NUMBER) {
		double val = strtod(tmp, NULL);
		double abs_val = val < 0 ? -val : val;
		if (abs_val >= 1e12)
			return snprintf(out, outsize, "\"%.2fT\"", val / 1e12);
		if (abs_val >= 1e9)
			return snprintf(out, outsize, "\"%.2fB\"", val / 1e9);
		if (abs_val >= 1e6)
			return snprintf(out, outsize, "\"%.2fM\"", val / 1e6);
		if (abs_val >= 1e3)
			return snprintf(out, outsize, "\"%.2fK\"", val / 1e3);
		return 0; /* Small number, leave as-is */
	}

	if (cat == HUMAN_PROGRESS) {
		double val = strtod(tmp, NULL);
		if (val >= 0.9999)
			return snprintf(out, outsize, "\"Synced\"");
		return snprintf(out, outsize, "\"%.2f%%\"", val * 100.0);
	}

	return 0;
}

char *format_human(const char *json)
{
	size_t len = strlen(json);
	size_t bufsize = len * 2 + 256;
	char *buf = malloc(bufsize);
	if (!buf) return NULL;

	const char *p = json;
	size_t pos = 0;
	int in_string = 0;

	/* Track the last key we saw */
	const char *last_key = NULL;
	size_t last_key_len = 0;
	int expect_value = 0; /* 1 = next non-ws token is the value for last_key */

	while (*p) {
		/* Grow buffer if needed */
		if (pos + 128 > bufsize) {
			bufsize *= 2;
			buf = realloc(buf, bufsize);
			if (!buf) return NULL;
		}

		if (*p == '"' && (p == json || *(p-1) != '\\')) {
			if (!in_string) {
				/* Opening quote — find closing quote */
				const char *end = p + 1;
				while (*end && !(*end == '"' && *(end-1) != '\\'))
					end++;

				if (expect_value) {
					/* This is a string value, not a number — copy as-is */
					expect_value = 0;
					size_t span = end - p + 1;
					if (pos + span + 1 > bufsize) {
						bufsize = pos + span + 256;
						buf = realloc(buf, bufsize);
						if (!buf) return NULL;
					}
					memcpy(buf + pos, p, span);
					pos += span;
					p = end + 1;
					continue;
				}

				/* Check if this is a key (followed by ':') */
				const char *after = end + 1;
				while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r')
					after++;
				if (*after == ':') {
					/* It's a key */
					last_key = p + 1;
					last_key_len = end - p - 1;
					/* Copy the key string as-is */
					size_t span = end - p + 1;
					memcpy(buf + pos, p, span);
					pos += span;
					p = end + 1;
					continue;
				}
			}
			in_string = !in_string;
			buf[pos++] = *p++;
			continue;
		}

		if (in_string) {
			buf[pos++] = *p++;
			continue;
		}

		if (*p == ':') {
			buf[pos++] = *p++;
			if (last_key)
				expect_value = 1;
			continue;
		}

		/* Check for a number that might need transformation */
		if (expect_value && (*p == '-' || (*p >= '0' && *p <= '9'))) {
			expect_value = 0;

			HumanCategory cat = HUMAN_NONE;
			if (last_key)
				cat = classify_key(last_key, last_key_len);

			if (cat != HUMAN_NONE) {
				/* Capture the full number */
				const char *numstart = p;
				if (*p == '-') p++;
				while (*p >= '0' && *p <= '9') p++;
				if (*p == '.') {
					p++;
					while (*p >= '0' && *p <= '9') p++;
				}
				/* Handle scientific notation */
				if (*p == 'e' || *p == 'E') {
					p++;
					if (*p == '+' || *p == '-') p++;
					while (*p >= '0' && *p <= '9') p++;
				}
				size_t numlen = p - numstart;

				char human_buf[128];
				int hlen = humanize_number(human_buf, sizeof(human_buf),
				                           numstart, numlen, cat);
				if (hlen > 0) {
					if (pos + (size_t)hlen + 1 > bufsize) {
						bufsize = pos + hlen + 256;
						buf = realloc(buf, bufsize);
						if (!buf) return NULL;
					}
					memcpy(buf + pos, human_buf, hlen);
					pos += hlen;
				} else {
					/* No transformation — copy original */
					memcpy(buf + pos, numstart, numlen);
					pos += numlen;
				}
				last_key = NULL;
				continue;
			}

			/* No category match — copy number as-is */
			last_key = NULL;
			buf[pos++] = *p++;
			continue;
		}

		/* Reset expect_value on non-whitespace that isn't a number */
		if (expect_value && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
			expect_value = 0;
			last_key = NULL;
		}

		buf[pos++] = *p++;
	}

	buf[pos] = '\0';
	return buf;
}
