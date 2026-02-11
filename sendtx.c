/* Robust sendrawtransaction with retry + mempool confirmation */

#include "sendtx.h"
#include "json.h"
#include "methods.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_RETRIES 3

/* Build JSON params: ["hex"] or ["hex", feerate] */
static char *sendtx_build_params(const char *hexstring, const char *maxfeerate)
{
	size_t len = strlen(hexstring) + 64;
	if (maxfeerate)
		len += strlen(maxfeerate);

	char *params = malloc(len);
	if (!params)
		return NULL;

	if (maxfeerate)
		snprintf(params, len, "[\"%s\",%s]", hexstring, maxfeerate);
	else
		snprintf(params, len, "[\"%s\"]", hexstring);

	return params;
}

/* Call getmempoolentry to verify tx is in local mempool */
static int sendtx_verify_mempool(RpcClient *rpc, const char *txid)
{
	char params[128];
	char *response;
	int error_code;
	char *result;

	snprintf(params, sizeof(params), "[\"%s\"]", txid);
	response = rpc_call(rpc, "getmempoolentry", params);
	if (!response)
		return 0;

	result = method_extract_result(response, &error_code);
	free(response);
	free(result);

	return (error_code == 0) ? 1 : 0;
}

/* Call decoderawtransaction to extract txid from hex (for -27 case) */
static int sendtx_get_txid_from_hex(RpcClient *rpc, const char *hexstring,
                                     char *txid_out, size_t txid_size)
{
	size_t plen = strlen(hexstring) + 16;
	char *params = malloc(plen);
	char *response;
	int error_code;
	char *result;

	if (!params)
		return -1;

	snprintf(params, plen, "[\"%s\"]", hexstring);
	response = rpc_call(rpc, "decoderawtransaction", params);
	free(params);

	if (!response)
		return -1;

	result = method_extract_result(response, &error_code);
	free(response);

	if (error_code != 0 || !result) {
		free(result);
		return -1;
	}

	/* Extract txid from the decoded transaction JSON */
	if (json_get_string(result, "txid", txid_out, txid_size) <= 0) {
		free(result);
		return -1;
	}

	free(result);
	return 0;
}

/* Sleep with exponential backoff: 1s, 2s, 4s */
static void sendtx_sleep_ms(int attempt)
{
	unsigned int secs = 1u << attempt;  /* 1, 2, 4 */
	sleep(secs);
}

int sendtx_submit(RpcClient *rpc, const char *hexstring,
                  const char *maxfeerate, SendTxResult *result)
{
	char *params;
	char *response;
	char *extracted;
	int error_code;
	int attempt;

	memset(result, 0, sizeof(SendTxResult));

	params = sendtx_build_params(hexstring, maxfeerate);
	if (!params) {
		snprintf(result->error_msg, sizeof(result->error_msg),
		         "Failed to build parameters");
		return -1;
	}

	for (attempt = 0; attempt < MAX_RETRIES; attempt++) {
		if (attempt > 0) {
			fprintf(stderr, "Retry %d/%d after backoff...\n",
			        attempt, MAX_RETRIES - 1);
			sendtx_sleep_ms(attempt - 1);
			/* Reconnect dead socket */
			rpc_disconnect(rpc);
			if (rpc_connect(rpc) < 0) {
				fprintf(stderr, "Warning: reconnect failed\n");
				continue;
			}
		}

		response = rpc_call(rpc, "sendrawtransaction", params);

		if (!response) {
			/* True network failure — retryable */
			fprintf(stderr, "Warning: network failure (attempt %d/%d)\n",
			        attempt + 1, MAX_RETRIES);
			continue;
		}

		extracted = method_extract_result(response, &error_code);
		free(response);

		if (error_code == -27) {
			/* Already in mempool — treat as success */
			free(extracted);
			if (sendtx_get_txid_from_hex(rpc, hexstring,
			                              result->txid,
			                              sizeof(result->txid)) == 0) {
				result->in_local_mempool = 1;
			}
			free(params);
			return 0;
		}

		if (error_code != 0) {
			/* Real RPC error — NOT retryable */
			result->rpc_error_code = error_code;
			if (extracted) {
				strncpy(result->error_msg, extracted,
				        sizeof(result->error_msg) - 1);
				free(extracted);
			}
			free(params);
			return -1;
		}

		/* Success: got txid back */
		if (extracted) {
			strncpy(result->txid, extracted, sizeof(result->txid) - 1);
			free(extracted);
		}

		/* Mempool confirmation (informational, not fatal) */
		if (result->txid[0])
			result->in_local_mempool = sendtx_verify_mempool(rpc, result->txid);

		free(params);
		return 0;
	}

	/* All retries exhausted */
	free(params);
	snprintf(result->error_msg, sizeof(result->error_msg),
	         "sendrawtransaction failed after %d attempts (network error)",
	         MAX_RETRIES);
	return -1;
}
