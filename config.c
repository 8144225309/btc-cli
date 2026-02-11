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
		const char *val = arg + 12;
		const char *colon = strrchr(val, ':');
		if (colon && colon != val) {
			/* Always extract host before the colon */
			size_t hostlen = colon - val;
			if (hostlen >= sizeof(cfg->host))
				hostlen = sizeof(cfg->host) - 1;
			memcpy(cfg->host, val, hostlen);
			cfg->host[hostlen] = '\0';
			/* Parse port if digits present and valid */
			if (*(colon + 1)) {
				int port = atoi(colon + 1);
				if (port > 0 && port <= 65535 && !cfg->port_set) {
					cfg->port = port;
					cfg->port_set = 1;
				}
			}
		} else {
			strncpy(cfg->host, val, sizeof(cfg->host) - 1);
		}
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
	(void)prog;
	printf(
	"Bitcoin Core RPC client version v30.2.0\n"
	"\n"
	"The bitcoin-cli utility provides a command line interface to interact with a Bitcoin Core RPC server.\n"
	"\n"
	"It can be used to query network information, manage wallets, create or broadcast transactions, and control the Bitcoin Core server.\n"
	"\n"
	"Use the \"help\" command to list all commands. Use \"help <command>\" to show help for that command.\n"
	"The -named option allows you to specify parameters using the key=value format, eliminating the need to pass unused positional parameters.\n"
	"\n"
	"Usage: bitcoin-cli [options] <command> [params]\n"
	"or:    bitcoin-cli [options] -named <command> [name=value]...\n"
	"or:    bitcoin-cli [options] help\n"
	"or:    bitcoin-cli [options] help <command>\n"
	"\n"
	"\n"
	"Options:\n"
	"\n"
	"  -color=<when>\n"
	"       Color setting for CLI output (default: auto). Valid values: always, auto\n"
	"       (add color codes when standard output is connected to a terminal\n"
	"       and OS is not WIN32), never. Only applies to the output of\n"
	"       -getinfo.\n"
	"\n"
	"  -conf=<file>\n"
	"       Specify configuration file. Relative paths will be prefixed by datadir\n"
	"       location. (default: bitcoin.conf)\n"
	"\n"
	"  -datadir=<dir>\n"
	"       Specify data directory\n"
	"\n"
	"  -help\n"
	"       Print this help message and exit (also -h or -?)\n"
	"\n"
	"  -named\n"
	"       Pass named instead of positional arguments (default: false)\n"
	"\n"
	"  -rpcclienttimeout=<n>\n"
	"       Timeout in seconds during HTTP requests, or 0 for no timeout. (default:\n"
	"       900)\n"
	"\n"
	"  -rpcconnect=<ip>\n"
	"       Send commands to node running on <ip> (default: 127.0.0.1)\n"
	"\n"
	"  -rpccookiefile=<loc>\n"
	"       Location of the auth cookie. Relative paths will be prefixed by a\n"
	"       net-specific datadir location. (default: data dir)\n"
	"\n"
	"  -rpcpassword=<pw>\n"
	"       Password for JSON-RPC connections\n"
	"\n"
	"  -rpcport=<port>\n"
	"       Connect to JSON-RPC on <port> (default: 8332, testnet: 18332, testnet4:\n"
	"       48332, signet: 38332, regtest: 18443)\n"
	"\n"
	"  -rpcuser=<user>\n"
	"       Username for JSON-RPC connections\n"
	"\n"
	"  -rpcwait\n"
	"       Wait for RPC server to start\n"
	"\n"
	"  -rpcwaittimeout=<n>\n"
	"       Timeout in seconds to wait for the RPC server to start, or 0 for no\n"
	"       timeout. (default: 0)\n"
	"\n"
	"  -rpcwallet=<walletname>\n"
	"       Send RPC for non-default wallet on RPC server (needs to exactly match\n"
	"       corresponding -wallet option passed to bitcoind). This changes\n"
	"       the RPC endpoint used, e.g.\n"
	"       http://127.0.0.1:8332/wallet/<walletname>\n"
	"\n"
	"  -stdin\n"
	"       Read extra arguments from standard input, one per line until EOF/Ctrl-D\n"
	"       (recommended for sensitive information such as passphrases). When\n"
	"       combined with -stdinrpcpass, the first line from standard input\n"
	"       is used for the RPC password.\n"
	"\n"
	"  -stdinrpcpass\n"
	"       Read RPC password from standard input as a single line. When combined\n"
	"       with -stdin, the first line from standard input is used for the\n"
	"       RPC password. When combined with -stdinwalletpassphrase,\n"
	"       -stdinrpcpass consumes the first line, and -stdinwalletpassphrase\n"
	"       consumes the second.\n"
	"\n"
	"  -stdinwalletpassphrase\n"
	"       Read wallet passphrase from standard input as a single line. When\n"
	"       combined with -stdin, the first line from standard input is used\n"
	"       for the wallet passphrase.\n"
	"\n"
	"  -version\n"
	"       Print version and exit\n"
	"\n"
	"Debugging/Testing options:\n"
	"\n"
	"Chain selection options:\n"
	"\n"
	"  -chain=<chain>\n"
	"       Use the chain <chain> (default: main). Allowed values: main, test,\n"
	"       testnet4, signet, regtest\n"
	"\n"
	"  -signet\n"
	"       Use the signet chain. Equivalent to -chain=signet. Note that the network\n"
	"       is defined by the -signetchallenge parameter\n"
	"\n"
	"  -signetchallenge\n"
	"       Blocks must satisfy the given script to be considered valid (only for\n"
	"       signet networks; defaults to the global default signet test\n"
	"       network challenge)\n"
	"\n"
	"  -signetseednode\n"
	"       Specify a seed node for the signet network, in the hostname[:port]\n"
	"       format, e.g. sig.net:1234 (may be used multiple times to specify\n"
	"       multiple seed nodes; defaults to the global default signet test\n"
	"       network seed node(s))\n"
	"\n"
	"  -testnet\n"
	"       Use the testnet3 chain. Equivalent to -chain=test. Support for testnet3\n"
	"       is deprecated and will be removed in an upcoming release.\n"
	"       Consider moving to testnet4 now by using -testnet4.\n"
	"\n"
	"  -testnet4\n"
	"       Use the testnet4 chain. Equivalent to -chain=testnet4.\n"
	"\n"
	"CLI Commands:\n"
	"\n"
	"  -addrinfo\n"
	"       Get the number of addresses known to the node, per network and total,\n"
	"       after filtering for quality and recency. The total number of\n"
	"       addresses known to the node may be higher.\n"
	"\n"
	"  -generate\n"
	"       Generate blocks, equivalent to RPC getnewaddress followed by RPC\n"
	"       generatetoaddress. Optional positional integer arguments are\n"
	"       number of blocks to generate (default: 1) and maximum iterations\n"
	"       to try (default: 1000000), equivalent to RPC generatetoaddress\n"
	"       nblocks and maxtries arguments. Example: bitcoin-cli -generate 4\n"
	"       1000\n"
	"\n"
	"  -getinfo\n"
	"       Get general information from the remote server. Note that unlike\n"
	"       server-side RPC calls, the output of -getinfo is the result of\n"
	"       multiple non-atomic requests. Some entries in the output may\n"
	"       represent results from different states (e.g. wallet balance may\n"
	"       be as of a different block from the chain state reported)\n"
	"\n"
	"  -netinfo\n"
	"       Get network peer connection information from the remote server. An\n"
	"       optional argument from 0 to 4 can be passed for different peers\n"
	"       listings (default: 0). If a non-zero value is passed, an\n"
	"       additional \"outonly\" (or \"o\") argument can be passed to see\n"
	"       outbound peers only. Pass \"help\" (or \"h\") for detailed help\n"
	"       documentation.\n"
	"\n"
	"btc-cli Extensions:\n"
	"\n"
	"  -verify\n"
	"       Verify transaction propagation via P2P peers\n"
	"\n"
	"  -verify-peers=<n>\n"
	"       Number of peers to check for verification (default: 3, max: 10)\n"
	"\n"
	"  -fallback-mempool-space\n"
	"       Broadcast via mempool.space API\n"
	"\n"
	"  -fallback-blockstream\n"
	"       Broadcast via blockstream.info API\n"
	"\n"
	"  -fallback-blockchair\n"
	"       Broadcast via blockchair.com API\n"
	"\n"
	"  -fallback-blockchain-info\n"
	"       Broadcast via blockchain.info API\n"
	"\n"
	"  -fallback-blockcypher\n"
	"       Broadcast via blockcypher.com API\n"
	"\n"
	"  -fallback-esplora=<url>\n"
	"       Broadcast via Esplora API at specified URL\n"
	"\n"
	"  -fallback-p2p=<n>\n"
	"       Broadcast to N peers via P2P protocol\n"
	"\n"
	"  -fallback-all\n"
	"       Enable all fallback broadcast methods\n"
	"\n"
	"  -help=<command>\n"
	"       Show help for a specific RPC command\n"
	);
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
