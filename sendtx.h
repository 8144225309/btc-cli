/* Robust sendrawtransaction with retry + mempool confirmation */

#ifndef SENDTX_H
#define SENDTX_H

#include "rpc.h"

typedef struct {
	char txid[65];           /* 64 hex + null */
	int in_local_mempool;    /* 1 if confirmed via getmempoolentry */
	int rpc_error_code;      /* 0 on success */
	char error_msg[512];     /* Error details if failed */
} SendTxResult;

/* Submit raw transaction with retry logic and mempool confirmation.
 * Returns 0 on success, -1 on failure.
 * hexstring: signed transaction hex (required)
 * maxfeerate: maximum fee rate string, or NULL for default
 */
int sendtx_submit(RpcClient *rpc, const char *hexstring,
                  const char *maxfeerate, SendTxResult *result);

#endif
