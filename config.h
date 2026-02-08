/* CLI configuration and argument parsing */

#ifndef CONFIG_H
#define CONFIG_H

/* Network types */
typedef enum {
	NET_MAINNET = 0,
	NET_TESTNET,
	NET_SIGNET,
	NET_REGTEST
} Network;

/* Default ports by network */
#define PORT_MAINNET  8332
#define PORT_TESTNET  18332
#define PORT_SIGNET   38332
#define PORT_REGTEST  18443

/* Color output modes */
#define COLOR_AUTO   0
#define COLOR_ALWAYS 1
#define COLOR_NEVER  2

/* Fallback broadcast configuration */
typedef struct {
	int mempool_space;      /* -fallback-mempool-space */
	int blockstream;        /* -fallback-blockstream */
	int blockchair;         /* -fallback-blockchair */
	int blockchain_info;    /* -fallback-blockchain-info */
	int blockcypher;        /* -fallback-blockcypher */
	char esplora_url[512];  /* -fallback-esplora=URL */
	int p2p_peers;          /* -fallback-p2p=N (0 = disabled) */
} FallbackConfig;

/* Configuration from CLI args + config file */
typedef struct {
	/* Network selection */
	Network network;

	/* Connection */
	char host[256];
	int port;
	int port_set;  /* Was port explicitly set? */

	/* Authentication */
	char user[256];
	char password[256];
	char cookie_file[1024];
	char datadir[1024];

	/* Wallet */
	char wallet[256];

	/* Flags */
	int named;       /* Use named parameters */
	int help;        /* Show help */
	int stdin_rpc;   /* Read params from stdin */
	int getinfo;     /* Show combined node info */
	int netinfo;     /* Show network peer summary */
	int version;     /* Show version and exit */
	int rpcwait;     /* Wait for RPC server to start */
	int rpcwait_timeout;  /* Timeout for rpcwait (seconds, 0=forever) */
	int stdinrpcpass;  /* Read RPC password from stdin */
	int color;         /* Color output mode */
	int verify;        /* -verify: P2P tx propagation check */
	int verify_peers;  /* -verify-peers=N: peers to check (default 3) */
	FallbackConfig fallback;  /* Fallback broadcast settings */

	/* Help for specific command */
	char help_cmd[64];

	/* Config file */
	char conf_file[1024];  /* Explicit -conf= path */

	/* Command and arguments (indices into argv) */
	int cmd_index;   /* Index of command in argv */
} Config;

/* Parse command-line arguments
 * Returns 0 on success, -1 on error
 * Sets config->cmd_index to first non-option argument
 */
int config_parse_args(Config *cfg, int argc, char **argv);

/* Apply network defaults (port numbers) */
void config_apply_network_defaults(Config *cfg);

/* Get default datadir for current platform */
const char *config_default_datadir(void);

/* Get network subdirectory name */
const char *config_network_subdir(Network net);

/* Print usage */
void config_print_usage(const char *prog);

/* Parse config file (bitcoin.conf format)
 * Returns 0 on success, -1 on error (file not found)
 * CLI args take priority over config file values
 */
int config_parse_file(Config *cfg, const char *path);

#endif
