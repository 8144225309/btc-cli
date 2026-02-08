/* CLI configuration and argument parsing */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default Bitcoin datadir */
const char *config_default_datadir(void)
{
	static char path[1024];

#ifdef _WIN32
	const char *appdata = getenv("APPDATA");
	if (appdata) {
		snprintf(path, sizeof(path), "%s\\Bitcoin", appdata);
		return path;
	}
#endif

	const char *home = getenv("HOME");
	if (!home)
		home = ".";

	snprintf(path, sizeof(path), "%s/.bitcoin", home);
	return path;
}

const char *config_network_subdir(Network net)
{
	switch (net) {
	case NET_TESTNET:  return "testnet3";
	case NET_TESTNET4: return "testnet4";
	case NET_SIGNET:   return "signet";
	case NET_REGTEST:  return "regtest";
	default:           return "";
	}
}

static int parse_option(Config *cfg, const char *arg)
{
	/* Network selection */
	if (strcmp(arg, "-signet") == 0) {
		cfg->network = NET_SIGNET;
		return 1;
	}
	if (strcmp(arg, "-testnet") == 0) {
		cfg->network = NET_TESTNET;
		return 1;
	}
	if (strcmp(arg, "-regtest") == 0) {
		cfg->network = NET_REGTEST;
		return 1;
	}
	if (strcmp(arg, "-mainnet") == 0) {
		cfg->network = NET_MAINNET;
		return 1;
	}

	/* Flags */
	if (strcmp(arg, "-named") == 0) {
		cfg->named = 1;
		return 1;
	}
	if (strcmp(arg, "-stdin") == 0) {
		cfg->stdin_rpc = 1;
		return 1;
	}
	if (strcmp(arg, "-help") == 0 || strcmp(arg, "-h") == 0 ||
	    strcmp(arg, "--help") == 0) {
		cfg->help = 1;
		return 1;
	}
	if (strcmp(arg, "-getinfo") == 0) {
		cfg->getinfo = 1;
		return 1;
	}
	if (strcmp(arg, "-netinfo") == 0) {
		cfg->netinfo = 0;  /* Level 0 (summary) */
		return 1;
	}
	if (strncmp(arg, "-netinfo=", 9) == 0) {
		cfg->netinfo = atoi(arg + 9);
		if (cfg->netinfo < 0) cfg->netinfo = 0;
		if (cfg->netinfo > 4) cfg->netinfo = 4;
		return 1;
	}
	if (strcmp(arg, "-addrinfo") == 0) {
		cfg->addrinfo = 1;
		return 1;
	}
	if (strcmp(arg, "-generate") == 0) {
		cfg->generate = 1;
		return 1;
	}
	if (strcmp(arg, "-version") == 0 || strcmp(arg, "--version") == 0) {
		cfg->version = 1;
		return 1;
	}
	if (strcmp(arg, "-rpcwait") == 0) {
		cfg->rpcwait = 1;
		return 1;
	}
	if (strncmp(arg, "-rpcwaittimeout=", 16) == 0) {
		cfg->rpcwait_timeout = atoi(arg + 16);
		return 1;
	}
	if (strcmp(arg, "-stdinrpcpass") == 0) {
		cfg->stdinrpcpass = 1;
		return 1;
	}
	if (strcmp(arg, "-stdinwalletpassphrase") == 0) {
		cfg->stdinwalletpassphrase = 1;
		return 1;
	}
	if (strcmp(arg, "-testnet4") == 0) {
		cfg->network = NET_TESTNET4;
		return 1;
	}
	if (strncmp(arg, "-chain=", 7) == 0) {
		const char *chain = arg + 7;
		if (strcmp(chain, "main") == 0)
			cfg->network = NET_MAINNET;
		else if (strcmp(chain, "test") == 0)
			cfg->network = NET_TESTNET;
		else if (strcmp(chain, "testnet4") == 0)
			cfg->network = NET_TESTNET4;
		else if (strcmp(chain, "signet") == 0)
			cfg->network = NET_SIGNET;
		else if (strcmp(chain, "regtest") == 0)
			cfg->network = NET_REGTEST;
		else {
			fprintf(stderr, "error: Unknown chain: %s\n", chain);
			return 0;
		}
		return 1;
	}
	if (strncmp(arg, "-rpcclienttimeout=", 18) == 0) {
		cfg->rpc_timeout = atoi(arg + 18);
		return 1;
	}
	if (strncmp(arg, "-signetchallenge=", 17) == 0) {
		strncpy(cfg->signetchallenge, arg + 17,
		        sizeof(cfg->signetchallenge) - 1);
		return 1;
	}
	if (strncmp(arg, "-signetseednode=", 16) == 0) {
		strncpy(cfg->signetseednode, arg + 16,
		        sizeof(cfg->signetseednode) - 1);
		return 1;
	}
	if (strcmp(arg, "-color") == 0 || strcmp(arg, "-color=auto") == 0) {
		cfg->color = COLOR_AUTO;
		return 1;
	}
	if (strcmp(arg, "-color=always") == 0) {
		cfg->color = COLOR_ALWAYS;
		return 1;
	}
	if (strcmp(arg, "-color=never") == 0) {
		cfg->color = COLOR_NEVER;
		return 1;
	}
	if (strcmp(arg, "-verify") == 0) {
		cfg->verify = 1;
		return 1;
	}
	if (strncmp(arg, "-verify-peers=", 14) == 0) {
		cfg->verify_peers = atoi(arg + 14);
		if (cfg->verify_peers < 1) cfg->verify_peers = 1;
		if (cfg->verify_peers > 10) cfg->verify_peers = 10;
		return 1;
	}

	/* Fallback broadcast flags */
	if (strcmp(arg, "-fallback-mempool-space") == 0) {
		cfg->fallback.mempool_space = 1;
		return 1;
	}
	if (strcmp(arg, "-fallback-blockstream") == 0) {
		cfg->fallback.blockstream = 1;
		return 1;
	}
	if (strcmp(arg, "-fallback-blockchair") == 0) {
		cfg->fallback.blockchair = 1;
		return 1;
	}
	if (strcmp(arg, "-fallback-blockchain-info") == 0) {
		cfg->fallback.blockchain_info = 1;
		return 1;
	}
	if (strcmp(arg, "-fallback-blockcypher") == 0) {
		cfg->fallback.blockcypher = 1;
		return 1;
	}
	if (strncmp(arg, "-fallback-esplora=", 18) == 0) {
		strncpy(cfg->fallback.esplora_url, arg + 18,
		        sizeof(cfg->fallback.esplora_url) - 1);
		return 1;
	}
	if (strncmp(arg, "-fallback-p2p=", 14) == 0) {
		cfg->fallback.p2p_peers = atoi(arg + 14);
		if (cfg->fallback.p2p_peers < 1) cfg->fallback.p2p_peers = 1;
		if (cfg->fallback.p2p_peers > 50) cfg->fallback.p2p_peers = 50;
		return 1;
	}
	if (strcmp(arg, "-fallback-all") == 0) {
		cfg->fallback.mempool_space = 1;
		cfg->fallback.blockstream = 1;
		cfg->fallback.blockchair = 1;
		cfg->fallback.blockchain_info = 1;
		cfg->fallback.blockcypher = 1;
		cfg->fallback.p2p_peers = 10;
		return 1;
	}

	/* -help=command */
	if (strncmp(arg, "-help=", 6) == 0) {
		cfg->help = 1;
		strncpy(cfg->help_cmd, arg + 6, sizeof(cfg->help_cmd) - 1);
		return 1;
	}

	/* Key=value options */
	if (strncmp(arg, "-rpcconnect=", 12) == 0) {
		strncpy(cfg->host, arg + 12, sizeof(cfg->host) - 1);
		return 1;
	}
	if (strncmp(arg, "-rpcport=", 9) == 0) {
		cfg->port = atoi(arg + 9);
		cfg->port_set = 1;
		return 1;
	}
	if (strncmp(arg, "-rpcuser=", 9) == 0) {
		strncpy(cfg->user, arg + 9, sizeof(cfg->user) - 1);
		cfg->user_set = 1;
		return 1;
	}
	if (strncmp(arg, "-rpcpassword=", 13) == 0) {
		strncpy(cfg->password, arg + 13, sizeof(cfg->password) - 1);
		cfg->password_set = 1;
		return 1;
	}
	if (strncmp(arg, "-rpccookiefile=", 15) == 0) {
		strncpy(cfg->cookie_file, arg + 15, sizeof(cfg->cookie_file) - 1);
		return 1;
	}
	if (strncmp(arg, "-rpcwallet=", 11) == 0) {
		strncpy(cfg->wallet, arg + 11, sizeof(cfg->wallet) - 1);
		return 1;
	}
	if (strncmp(arg, "-datadir=", 9) == 0) {
		strncpy(cfg->datadir, arg + 9, sizeof(cfg->datadir) - 1);
		return 1;
	}
	if (strncmp(arg, "-conf=", 6) == 0) {
		strncpy(cfg->conf_file, arg + 6, sizeof(cfg->conf_file) - 1);
		return 1;
	}

	return 0;  /* Not recognized as option */
}

int config_parse_args(Config *cfg, int argc, char **argv)
{
	int i;

	/* Initialize defaults */
	memset(cfg, 0, sizeof(Config));
	cfg->network = NET_MAINNET;
	cfg->netinfo = -1;
	cfg->rpc_timeout = 900;
	cfg->verify_peers = 3;
	strncpy(cfg->host, "127.0.0.1", sizeof(cfg->host) - 1);
	strncpy(cfg->datadir, config_default_datadir(), sizeof(cfg->datadir) - 1);
	cfg->cmd_index = -1;

	/* Parse arguments */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* Support --option by passing to parse_option as -option */
			const char *opt = argv[i];
			if (opt[0] == '-' && opt[1] == '-')
				opt = opt + 1;  /* Skip first dash: --foo → -foo */

			if (!parse_option(cfg, opt)) {
				fprintf(stderr, "error: Unknown option: %s\n", argv[i]);
				return -1;
			}
		} else {
			/* First non-option is the command */
			cfg->cmd_index = i;
			break;
		}
	}

	return 0;
}

void config_apply_network_defaults(Config *cfg)
{
	/* Set default port if not set by CLI or config file */
	if (!cfg->port_set && !cfg->port_from_conf) {
		switch (cfg->network) {
		case NET_MAINNET:  cfg->port = PORT_MAINNET;  break;
		case NET_TESTNET:  cfg->port = PORT_TESTNET;  break;
		case NET_TESTNET4: cfg->port = PORT_TESTNET4; break;
		case NET_SIGNET:   cfg->port = PORT_SIGNET;   break;
		case NET_REGTEST:  cfg->port = PORT_REGTEST;  break;
		}
	}
}

void config_print_usage(const char *prog)
{
	printf("Usage: %s [options] <command> [params]\n\n", prog);
	printf("Options:\n");
	printf("  -signet              Use signet network (port %d)\n", PORT_SIGNET);
	printf("  -testnet             Use testnet3 (port %d)\n", PORT_TESTNET);
	printf("  -testnet4            Use testnet4 (port %d)\n", PORT_TESTNET4);
	printf("  -chain=<name>        Select chain (main|test|testnet4|signet|regtest)\n");
	printf("  -regtest             Use regtest (port %d)\n", PORT_REGTEST);
	printf("  -rpcconnect=<ip>     Connect to node at <ip> (default: 127.0.0.1)\n");
	printf("  -rpcport=<port>      Connect to port <port>\n");
	printf("  -rpcuser=<user>      RPC username\n");
	printf("  -rpcpassword=<pw>    RPC password\n");
	printf("  -rpccookiefile=<f>   Cookie file path\n");
	printf("  -rpcwallet=<wallet>  Wallet name for wallet RPCs\n");
	printf("  -datadir=<dir>       Bitcoin data directory\n");
	printf("  -conf=<file>         Config file path (default: datadir/bitcoin.conf)\n");
	printf("  -named               Use named parameters\n");
	printf("  -stdin               Read extra args from stdin\n");
	printf("  -rpcwait             Wait for server to start\n");
	printf("  -rpcwaittimeout=<n>  Timeout for -rpcwait in seconds (default: 0=forever)\n");
	printf("  -rpcclienttimeout=<n> RPC timeout in seconds (default: 900)\n");
	printf("  -stdinrpcpass        Read RPC password from stdin (no echo)\n");
	printf("  -stdinwalletpassphrase  Read wallet passphrase from stdin\n");
	printf("  -signetchallenge=<hex>  Custom signet challenge script\n");
	printf("  -signetseednode=<h:p>   Custom signet seed node\n");
	printf("  -color=<when>        Colorize JSON output (auto, always, never)\n");
	printf("  -getinfo             Get general info from node\n");
	printf("  -netinfo[=<level>]   Get network peer info (level 0-4, default 0)\n");
	printf("  -addrinfo            Get address counts by network type\n");
	printf("  -generate [n] [max]  Generate n blocks (default: 1)\n");
	printf("  -verify              Verify tx propagation via P2P peers\n");
	printf("  -verify-peers=<n>    Number of peers to check (default: 3, max: 10)\n");
	printf("  -fallback-mempool-space    Broadcast via mempool.space API (requires TLS)\n");
	printf("  -fallback-blockstream      Broadcast via blockstream.info API (requires TLS)\n");
	printf("  -fallback-blockchair       Broadcast via blockchair.com API (requires TLS)\n");
	printf("  -fallback-blockchain-info  Broadcast via blockchain.info API (requires TLS)\n");
	printf("  -fallback-blockcypher      Broadcast via blockcypher.com API (requires TLS)\n");
	printf("  -fallback-esplora=<url>    Broadcast via Esplora API at URL (plain HTTP OK)\n");
	printf("  -fallback-p2p=<n>        Broadcast to N peers via P2P protocol\n");
	printf("  -fallback-all            Enable all fallback methods\n");
	printf("  -version             Show version and exit\n");
	printf("  -help                Show this help\n");
	printf("  -help=<command>      Show help for specific command\n");
	printf("\n");
	printf("Examples:\n");
	printf("  %s -signet getblockchaininfo\n", prog);
	printf("  %s -signet -getinfo\n", prog);
	printf("  %s -signet -netinfo\n", prog);
	printf("  %s -signet -rpcwait getblockcount\n", prog);
	printf("  %s -signet -named sendtoaddress address=tb1q... amount=0.1\n", prog);
}

/* Map section name to Network, returns -1 if not a network section */
static int section_to_network(const char *section)
{
	if (strcmp(section, "main") == 0) return NET_MAINNET;
	if (strcmp(section, "test") == 0) return NET_TESTNET;
	if (strcmp(section, "testnet4") == 0) return NET_TESTNET4;
	if (strcmp(section, "signet") == 0) return NET_SIGNET;
	if (strcmp(section, "regtest") == 0) return NET_REGTEST;
	return -1;
}

/* Internal recursive config parser with depth limit */
static int config_parse_file_internal(Config *cfg, const char *path, int depth);

int config_parse_file(Config *cfg, const char *path)
{
	return config_parse_file_internal(cfg, path, 0);
}

static int config_parse_file_internal(Config *cfg, const char *path, int depth)
{
	FILE *f;
	char line[1024];
	int current_section = -1;  /* -1 = root (no section), 0-4 = network */

	if (depth > 10)
		return -1;  /* Prevent infinite recursion */

	f = fopen(path, "r");
	if (!f)
		return -1;

	while (fgets(line, sizeof(line), f)) {
		/* Skip leading whitespace */
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;

		/* Skip comments and empty lines */
		if (*p == '#' || *p == '\n' || *p == '\0')
			continue;

		/* Check for [section] header */
		if (*p == '[') {
			char *end = strchr(p, ']');
			if (end) {
				*end = '\0';
				current_section = section_to_network(p + 1);
			}
			continue;
		}

		/* Skip settings in wrong section */
		if (current_section >= 0 && current_section != (int)cfg->network)
			continue;

		/* Parse key=value */
		char *eq = strchr(p, '=');
		if (!eq)
			continue;

		*eq = '\0';
		char *key = p;
		char *value = eq + 1;

		/* Trim newline and carriage return from value */
		char *nl = strchr(value, '\n');
		if (nl)
			*nl = '\0';
		nl = strchr(value, '\r');
		if (nl)
			*nl = '\0';

		/* Trim trailing whitespace from value */
		{
			size_t vlen = strlen(value);
			while (vlen > 0 && (value[vlen - 1] == ' ' || value[vlen - 1] == '\t'))
				value[--vlen] = '\0';
		}

		/* Handle includeconf directive */
		if (strcmp(key, "includeconf") == 0) {
			/* Resolve relative paths against datadir */
			char inc_path[1024];
			if (value[0] == '/') {
				strncpy(inc_path, value, sizeof(inc_path) - 1);
				inc_path[sizeof(inc_path) - 1] = '\0';
			} else {
				snprintf(inc_path, sizeof(inc_path), "%s/%s",
				         cfg->datadir, value);
			}
			config_parse_file_internal(cfg, inc_path, depth + 1);
			continue;
		}

		/* Map to config fields.
		 * Priority: CLI args > [section] values > root values.
		 * Section-specific values override root values,
		 * but CLI-set values are never overwritten. */
		int in_section = (current_section >= 0);

		if (strcmp(key, "rpcuser") == 0 &&
		    !cfg->user_set && (!cfg->user[0] || in_section))
			strncpy(cfg->user, value, sizeof(cfg->user) - 1);
		else if (strcmp(key, "rpcpassword") == 0 &&
		         !cfg->password_set && (!cfg->password[0] || in_section))
			strncpy(cfg->password, value, sizeof(cfg->password) - 1);
		else if (strcmp(key, "rpcconnect") == 0)
			strncpy(cfg->host, value, sizeof(cfg->host) - 1);
		else if (strcmp(key, "rpcport") == 0 &&
		         !cfg->port_set && (!cfg->port_from_conf || in_section)) {
			cfg->port = atoi(value);
			cfg->port_from_conf = 1;
		}
		else if (strcmp(key, "testnet") == 0 && atoi(value) && cfg->network == NET_MAINNET)
			cfg->network = NET_TESTNET;
		else if (strcmp(key, "signet") == 0 && atoi(value) && cfg->network == NET_MAINNET)
			cfg->network = NET_SIGNET;
		else if (strcmp(key, "regtest") == 0 && atoi(value) && cfg->network == NET_MAINNET)
			cfg->network = NET_REGTEST;
		else if (strcmp(key, "testnet4") == 0 && atoi(value) && cfg->network == NET_MAINNET)
			cfg->network = NET_TESTNET4;
		else if (strcmp(key, "chain") == 0 && cfg->network == NET_MAINNET) {
			if (strcmp(value, "main") == 0)
				cfg->network = NET_MAINNET;
			else if (strcmp(value, "test") == 0)
				cfg->network = NET_TESTNET;
			else if (strcmp(value, "testnet4") == 0)
				cfg->network = NET_TESTNET4;
			else if (strcmp(value, "signet") == 0)
				cfg->network = NET_SIGNET;
			else if (strcmp(value, "regtest") == 0)
				cfg->network = NET_REGTEST;
		}
		else if (strcmp(key, "rpcclienttimeout") == 0 && cfg->rpc_timeout == 900)
			cfg->rpc_timeout = atoi(value);
	}

	fclose(f);
	return 0;
}
