/* Bitcoin Core JSON-RPC client */

#ifndef RPC_H
#define RPC_H

#include <stddef.h>

typedef struct {
	char host[256];
	int port;
	char auth[512];
	char wallet[256];  /* Wallet name for -rpcwallet */
	int sock;
} RpcClient;

void rpc_init(RpcClient *client, const char *host, int port);
int rpc_auth_cookie(RpcClient *client, const char *cookie_path);
void rpc_auth_userpass(RpcClient *client, const char *user, const char *pass);
int rpc_auth_auto(RpcClient *client, const char *datadir);
void rpc_set_wallet(RpcClient *client, const char *wallet);
int rpc_connect(RpcClient *client);
char *rpc_call(RpcClient *client, const char *method, const char *params);
void rpc_disconnect(RpcClient *client);

#endif
