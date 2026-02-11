/* Output formatting extensions: -field, -sats, -format=table/csv */

#ifndef FORMAT_H
#define FORMAT_H

#include <stdio.h>

/* Extract a JSON field by dotted path (e.g., "softforks.taproot.active")
 * Returns malloc'd string with the extracted value, or NULL if not found.
 * Caller frees the returned string.
 */
char *format_extract_field(const char *json, const char *path);

/* Convert BTC amounts (8-decimal floats) to satoshis in JSON output.
 * Returns malloc'd string with converted output. Caller frees.
 */
char *format_sats(const char *json);

/* Render JSON array of objects as aligned ASCII table.
 * Prints directly to out. Returns 0 on success, -1 if not an array.
 */
int format_table(FILE *out, const char *json);

/* Render JSON array of objects as CSV.
 * Prints directly to out. Returns 0 on success, -1 if not an array.
 */
int format_csv(FILE *out, const char *json);

#endif
