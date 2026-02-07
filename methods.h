/* RPC Method registry and dispatch */

#ifndef METHODS_H
#define METHODS_H

#include "rpc.h"

/* Parameter types for validation */
typedef enum {
	PARAM_STRING,
	PARAM_INT,
	PARAM_FLOAT,
	PARAM_BOOL,
	PARAM_ARRAY,
	PARAM_OBJECT,
	PARAM_AMOUNT,    /* BTC amount (8 decimal places) */
	PARAM_HEX,       /* Hex-encoded data */
	PARAM_ADDRESS,   /* Bitcoin address */
	PARAM_TXID,      /* 64-char hex txid */
	PARAM_HEIGHT_OR_HASH  /* Block height (int) or hash (string) */
} ParamType;

/* Parameter definition */
typedef struct {
	const char *name;
	ParamType type;
	int required;
	const char *description;
} ParamDef;

/* Maximum parameters per method */
#define MAX_PARAMS 16

/* Method handler function type
 * Returns: 0 on success, non-zero on error
 * out: allocated string with result (caller frees)
 */
typedef int (*MethodHandler)(RpcClient *rpc, int argc, char **argv, char **out);

/* Method definition */
typedef struct {
	const char *name;
	const char *category;
	const char *description;
	MethodHandler handler;
	ParamDef params[MAX_PARAMS];
	int param_count;
} MethodDef;

/* Find method by name, returns NULL if not found */
const MethodDef *method_find(const char *name);

/* List all methods (for help) */
void method_list_all(void);

/* List methods by category */
void method_list_category(const char *category);

/* Print help for specific method */
void method_print_help(const MethodDef *method);

/* Build JSON params array from argv
 * Handles type conversion based on method definition
 * Returns allocated string (caller frees)
 */
char *method_build_params(const MethodDef *method, int argc, char **argv);

/* Build JSON params object for named parameters
 * Parses key=value format
 * Returns allocated string (caller frees)
 */
char *method_build_named_params(const MethodDef *method, int argc, char **argv);

/* Extract result from JSON-RPC response
 * Handles error checking
 * Returns allocated string (caller frees), or NULL on error
 */
char *method_extract_result(const char *response, int *error_code);

/* Set named parameter mode (for -named flag) */
void method_set_named_mode(int enabled);

#endif
