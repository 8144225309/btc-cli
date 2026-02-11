/* Fallback broadcast: send transactions via public APIs and P2P */

#ifndef FALLBACK_H
#define FALLBACK_H

#include "config.h"

#define MAX_FALLBACK_RESULTS 10

typedef struct {
	const char *source;     /* "mempool.space", "blockstream", etc. */
	int success;            /* 1 if broadcast succeeded */
	char txid[65];          /* txid if returned by API */
	char error[256];        /* error message if failed */
} FallbackResult;

/* Check if any fallback is configured */
int fallback_has_any(const FallbackConfig *cfg);

/* Broadcast transaction via all configured fallback methods.
 * hex: raw transaction hex string
 * net: network (for endpoint URLs and P2P magic)
 * results: array of MAX_FALLBACK_RESULTS to fill
 * num_results: output — number of results written
 * Returns: number of successful broadcasts
 */
int fallback_broadcast(const FallbackConfig *cfg, const char *hex,
                       Network net, FallbackResult *results, int *num_results);

#endif
