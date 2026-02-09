/* btc-cli - Bitcoin CLI replacement in pure C */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <signal.h>
#endif

#include "config.h"
#include "methods.h"
#include "rpc.h"
#include "json.h"
#include "fallback.h"

#define BTC_CLI_VERSION "0.9.0"

/* ANSI color codes */
#define C_RESET   "\033[0m"
#define C_KEY     "\033[36m"   /* Cyan for keys */
#define C_STRING  "\033[32m"   /* Green for strings */
#define C_NUMBER  "\033[33m"   /* Yellow for numbers */
#define C_BOOL    "\033[35m"   /* Magenta for true/false/null */
#define C_BRACE   "\033[1m"    /* Bold for {} [] */

/* Global color setting */
static int use_color = 0;

/* Pretty print JSON output with optional color to specified stream */
static void fprint_json_pretty(FILE *out, const char *json, int indent)
{
	const char *p = json;
	int in_string = 0;
	int is_key = 0;
	int level = indent;
	int i;

	while (*p) {
		if (*p == '"' && (p == json || *(p-1) != '\\')) {
			if (!in_string) {
				/* Starting a string - check if it's a key */
				const char *ahead = p + 1;
				while (*ahead && *ahead != '"')
					ahead++;
				if (*ahead == '"') {
					ahead++;
					while (*ahead == ' ' || *ahead == '\t')
						ahead++;
					is_key = (*ahead == ':');
				}
				if (use_color)
					fprintf(out, "%s", is_key ? C_KEY : C_STRING);
			}
			fputc(*p, out);
			if (in_string) {
				if (use_color)
					fprintf(out, "%s", C_RESET);
				is_key = 0;
			}
			in_string = !in_string;
		} else if (in_string) {
			fputc(*p, out);
		} else if (*p == '{' || *p == '[') {
			/* Peek ahead for empty container {} or [] */
			const char *peek = p + 1;
			while (*peek == ' ' || *peek == '\t' || *peek == '\n' || *peek == '\r')
				peek++;
			char closing = (*p == '{') ? '}' : ']';
			if (*peek == closing) {
				/* Empty container — print compacted on same line */
				if (use_color)
					fprintf(out, "%s", C_BRACE);
				fputc(*p, out);
				fputc(closing, out);
				if (use_color)
					fprintf(out, "%s", C_RESET);
				p = peek;  /* Skip to closing bracket (loop will advance past it) */
			} else {
				if (use_color)
					fprintf(out, "%s", C_BRACE);
				fputc(*p, out);
				if (use_color)
					fprintf(out, "%s", C_RESET);
				fputc('\n', out);
				level++;
				for (i = 0; i < level * 2; i++) fputc(' ', out);
			}
		} else if (*p == '}' || *p == ']') {
			fputc('\n', out);
			level--;
			for (i = 0; i < level * 2; i++) fputc(' ', out);
			if (use_color)
				fprintf(out, "%s", C_BRACE);
			fputc(*p, out);
			if (use_color)
				fprintf(out, "%s", C_RESET);
		} else if (*p == ',') {
			fputc(*p, out);
			fputc('\n', out);
			for (i = 0; i < level * 2; i++) fputc(' ', out);
		} else if (*p == ':') {
			fputc(*p, out);
			fputc(' ', out);
		} else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			/* Skip whitespace - we add our own */
		} else {
			/* Numbers, booleans, null */
			if (use_color) {
				if (*p == 't' || *p == 'f' || *p == 'n')
					fprintf(out, "%s", C_BOOL);  /* true/false/null */
				else if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
					fprintf(out, "%s", C_NUMBER);
			}
			fputc(*p, out);
			/* Check if next char ends the token */
			char next = *(p + 1);
			if (use_color && (next == ',' || next == '}' || next == ']' ||
			    next == ' ' || next == '\n' || next == '\0'))
				fprintf(out, "%s", C_RESET);
		}
		p++;
	}
	fputc('\n', out);
}

/* Convenience: print JSON to stdout */
static void print_json_pretty(const char *json, int indent)
{
	fprint_json_pretty(stdout, json, indent);
}

/* Get cookie path for network */
static void get_cookie_path(char *path, size_t size, const char *datadir, Network net)
{
	const char *subdir = config_network_subdir(net);
	if (subdir[0])
		snprintf(path, size, "%s/%s/.cookie", datadir, subdir);
	else
		snprintf(path, size, "%s/.cookie", datadir);
}

/* Print version and exit */
static void print_version(void)
{
	printf("btc-cli version %s\n", BTC_CLI_VERSION);
	printf("Pure C Bitcoin CLI - no external dependencies\n");
}

/* Read password from stdin without echo */
static void read_password_stdin(char *buf, size_t size)
{
#ifdef _WIN32
	/* Windows: use _getch() which doesn't echo */
	size_t i = 0;
	int c;
	while (i < size - 1) {
		c = _getch();
		if (c == '\r' || c == '\n')
			break;
		buf[i++] = c;
	}
	buf[i] = '\0';
#else
	/* Unix: disable terminal echo */
	struct termios old_term, new_term;
	if (tcgetattr(STDIN_FILENO, &old_term) == 0) {
		new_term = old_term;
		new_term.c_lflag &= ~ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
	}

	if (fgets(buf, size, stdin)) {
		char *nl = strchr(buf, '\n');
		if (nl)
			*nl = '\0';
	}

	/* Restore terminal */
	tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
#endif
}

/* Handle -addrinfo: address count by network type */
static int handle_addrinfo(RpcClient *rpc)
{
	char *response;
	const char *p;
	int ipv4 = 0, ipv6 = 0, onion = 0, i2p = 0, cjdns = 0, total = 0;

	/* getnodeaddresses 0 = return all known addresses */
	response = rpc_call(rpc, "getnodeaddresses", "[0]");
	if (!response) {
		fprintf(stderr, "error: Could not get node addresses\n");
		return 1;
	}

	/* Find result array within JSON-RPC response */
	const char *result_arr = json_find_value(response, "result");
	if (!result_arr) result_arr = response;
	while (*result_arr && *result_arr != '[') result_arr++;
	const char *arr_end = NULL;
	if (*result_arr == '[')
		arr_end = json_find_closing(result_arr);

	/* Iterate address objects within the result array */
	p = result_arr;
	while (*p && (!arr_end || p < arr_end)) {
		const char *obj = strchr(p, '{');
		if (!obj || (arr_end && obj >= arr_end)) break;
		const char *end = json_find_closing(obj);
		if (!end) break;

		size_t len = end - obj + 1;
		char *entry = malloc(len + 1);
		if (entry) {
			char net[32] = {0};
			memcpy(entry, obj, len);
			entry[len] = '\0';

			if (json_get_string(entry, "network", net, sizeof(net)) > 0) {
				total++;
				if (strcmp(net, "ipv4") == 0) ipv4++;
				else if (strcmp(net, "ipv6") == 0) ipv6++;
				else if (strcmp(net, "onion") == 0) onion++;
				else if (strcmp(net, "i2p") == 0) i2p++;
				else if (strcmp(net, "cjdns") == 0) cjdns++;
			}
			free(entry);
		}
		p = end + 1;
	}
	free(response);

	printf("{\"addresses_known\":{");
	printf("\"ipv4\":%d,\"ipv6\":%d,\"onion\":%d,\"i2p\":%d,\"cjdns\":%d,\"total\":%d",
	       ipv4, ipv6, onion, i2p, cjdns, total);
	printf("}}\n");

	return 0;
}

/* Handle -generate: convenience block generator */
static int handle_generate(RpcClient *rpc, int argc, char **argv, int cmd_index)
{
	char *response;
	char params[512];
	char address[256] = {0};
	int nblocks = 1;
	int maxtries = 1000000;
	int error_code;

	/* Parse optional positional args after -generate */
	if (cmd_index >= 0 && cmd_index < argc)
		nblocks = atoi(argv[cmd_index]);
	if (cmd_index >= 0 && cmd_index + 1 < argc)
		maxtries = atoi(argv[cmd_index + 1]);

	if (nblocks < 1) nblocks = 1;

	/* Step 1: get a fresh address */
	response = rpc_call(rpc, "getnewaddress", "[]");
	if (!response) {
		fprintf(stderr, "error: getnewaddress failed (is a wallet loaded?)\n");
		return 1;
	}

	char *addr_result = method_extract_result(response, &error_code);
	free(response);

	if (error_code != 0 || !addr_result) {
		fprintf(stderr, "error: getnewaddress: %s\n", addr_result ? addr_result : "failed");
		free(addr_result);
		return 1;
	}
	strncpy(address, addr_result, sizeof(address) - 1);
	free(addr_result);

	/* Step 2: generatetoaddress */
	snprintf(params, sizeof(params), "[%d,\"%s\",%d]", nblocks, address, maxtries);

	response = rpc_call(rpc, "generatetoaddress", params);
	if (!response) {
		fprintf(stderr, "error: generatetoaddress failed\n");
		return 1;
	}

	char *result = method_extract_result(response, &error_code);
	free(response);

	if (result) {
		const char *p = result;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;
		if (*p == '{' || *p == '[')
			print_json_pretty(result, 0);
		else
			printf("%s\n", result);
		free(result);
	}

	return error_code != 0 ? 1 : 0;
}

/* Handle -getinfo: combined node information (full dashboard) */
static int handle_getinfo(RpcClient *rpc, const char *wallet_name)
{
	char *blockchain = NULL;
	char *network = NULL;
	char *walletinfo = NULL;
	char *walletlist = NULL;
	char buf[256];

	/* Collect data */
	blockchain = rpc_call(rpc, "getblockchaininfo", "[]");
	network = rpc_call(rpc, "getnetworkinfo", "[]");

	printf("{\n");

	/* Version from getnetworkinfo (display first, like bitcoin-cli) */
	if (network) {
		int version = (int)json_get_int(network, "version");
		char subversion[256] = {0};
		json_get_string(network, "subversion", subversion, sizeof(subversion));
		printf("  \"version\": %d,\n", version);
		if (subversion[0])
			printf("  \"subversion\": \"%s\",\n", subversion);
	}

	/* Chain info */
	if (blockchain) {
		json_get_string(blockchain, "chain", buf, sizeof(buf));
		printf("  \"chain\": \"%s\",\n", buf);
		printf("  \"blocks\": %d,\n", (int)json_get_int(blockchain, "blocks"));
		printf("  \"headers\": %d,\n", (int)json_get_int(blockchain, "headers"));
		printf("  \"verificationprogress\": %.10g,\n",
		       json_get_double(blockchain, "verificationprogress"));
		printf("  \"difficulty\": %.10g,\n", json_get_double(blockchain, "difficulty"));
	}

	/* Network info */
	if (network) {
		printf("  \"timeoffset\": %d,\n", (int)json_get_int(network, "timeoffset"));

		int conn_in = (int)json_get_int(network, "connections_in");
		int conn_out = (int)json_get_int(network, "connections_out");
		int conn_total = (int)json_get_int(network, "connections");
		printf("  \"connections\": {\"in\": %d, \"out\": %d, \"total\": %d},\n",
		       conn_in, conn_out, conn_total);

		/* Proxy */
		const char *networks_arr = json_find_array(network, "networks");
		if (networks_arr) {
			const char *first_net = strchr(networks_arr, '{');
			if (first_net) {
				const char *first_end = json_find_closing(first_net);
				if (first_end) {
					size_t nlen = first_end - first_net + 1;
					char *net_obj = malloc(nlen + 1);
					if (net_obj) {
						char proxy[256] = {0};
						memcpy(net_obj, first_net, nlen);
						net_obj[nlen] = '\0';
						json_get_string(net_obj, "proxy", proxy, sizeof(proxy));
						printf("  \"proxy\": \"%s\",\n", proxy);
						free(net_obj);
					}
				}
			}
		}

		printf("  \"relayfee\": %.8f,\n", json_get_double(network, "relayfee"));

		char warnings[1024] = {0};
		json_get_string(network, "warnings", warnings, sizeof(warnings));
		printf("  \"warnings\": \"%s\",\n", warnings);
	}

	/* Wallet info — multi-wallet support */
	int multi_wallet = 0;
	walletlist = rpc_call(rpc, "listwallets", "[]");
	if (walletlist) {
		/* Find the result array: locate the [ ... ] within "result" */
		const char *arr_start = json_find_value(walletlist, "result");
		if (!arr_start) arr_start = walletlist;
		/* Skip to the opening [ */
		while (*arr_start && *arr_start != '[') arr_start++;

		if (*arr_start == '[') {
			const char *arr_end = json_find_closing(arr_start);
			if (!arr_end) arr_end = arr_start;

			/* Count wallet names within the array bounds */
			int wallet_count = 0;
			const char *q = arr_start + 1;
			while (q < arr_end) {
				if (*q == '"') {
					wallet_count++;
					q++;
					while (q < arr_end && *q != '"') {
						if (*q == '\\') q++;
						q++;
					}
				}
				if (q < arr_end) q++;
			}

			if (wallet_count > 1 && !wallet_name[0]) {
				/* Multiple wallets, no -rpcwallet specified: show all balances */
				printf("  \"balances\": {\n");
				multi_wallet = 1;

				/* Parse wallet names within array bounds only */
				const char *p = arr_start + 1;
				int first = 1;
				while (p < arr_end) {
					const char *start = strchr(p, '"');
					if (!start || start >= arr_end) break;
					start++;
					const char *end = strchr(start, '"');
					if (!end || end >= arr_end) break;

					size_t namelen = end - start;
					char wname[256] = {0};
					if (namelen < sizeof(wname)) {
						memcpy(wname, start, namelen);
						wname[namelen] = '\0';

						/* Set wallet and get balance */
						rpc_set_wallet(rpc, wname);
						rpc_disconnect(rpc);
						if (rpc_connect(rpc) == 0) {
							char *wb = rpc_call(rpc, "getbalances", "[]");
							if (wb) {
								const char *mine = json_find_object(wb, "mine");
								double bal = mine ? json_get_double(mine, "trusted") : 0;
								if (!first) printf(",\n");
								printf("    \"%s\": %.8f", wname, bal);
								first = 0;
								free(wb);
							}
						}
					}
					p = end + 1;
				}
				printf("\n  }\n");
			}
		}
		free(walletlist);
	}

	if (!multi_wallet) {
		/* Single wallet or specific -rpcwallet */
		walletinfo = rpc_call(rpc, "getbalances", "[]");
		if (walletinfo) {
			const char *mine = json_find_object(walletinfo, "mine");
			if (mine) {
				double balance = json_get_double(mine, "trusted");
				printf("  \"balance\": %.8f,\n", balance);
			}
			free(walletinfo);
		}

		/* Wallet details */
		char *winfo = rpc_call(rpc, "getwalletinfo", "[]");
		if (winfo) {
			char wname[256] = {0};
			json_get_string(winfo, "walletname", wname, sizeof(wname));
			printf("  \"walletname\": \"%s\",\n", wname);

			int64_t unlocked = json_get_int(winfo, "unlocked_until");
			if (unlocked > 0)
				printf("  \"unlocked_until\": %lld,\n", (long long)unlocked);

			printf("  \"keypoolsize\": %d,\n", (int)json_get_int(winfo, "keypoolsize"));
			printf("  \"paytxfee\": %.8f\n", json_get_double(winfo, "paytxfee"));
			free(winfo);
		} else {
			/* Remove trailing comma from warnings line if no wallet */
			/* Already printed, just close cleanly */
			printf("  \"wallet\": null\n");
		}
	}

	printf("}\n");

	free(blockchain);
	free(network);
	return 0;
}

/* Peer info struct for netinfo table */
typedef struct {
	int is_inbound;
	char conn_type[32];
	char network[16];
	double minping;
	double pingtime;
	int64_t lastsend;
	int64_t lastrecv;
	int64_t last_transaction;
	int64_t last_block;
	int64_t conntime;
	int bip152_hb_from;
	int bip152_hb_to;
	char addr[256];
	char subver[256];
} PeerRow;

/* Handle -netinfo: network peer summary with detail levels 0-4 */
static int handle_netinfo(RpcClient *rpc, int level, int outonly)
{
	char *peers_json = NULL;
	char *net_json = NULL;
	PeerRow peers[256];
	int peer_count = 0;
	int total = 0, inbound = 0, outbound = 0, block_relay = 0;
	int ipv4 = 0, ipv6 = 0, onion = 0, i2p = 0, cjdns = 0;
	const char *p;
	int i;
	time_t now = time(NULL);

	peers_json = rpc_call(rpc, "getpeerinfo", "[]");
	if (!peers_json) {
		fprintf(stderr, "error: Could not get peer info\n");
		return 1;
	}

	/* Find the result array within the JSON-RPC response */
	const char *result_arr = json_find_value(peers_json, "result");
	if (!result_arr) result_arr = peers_json;
	/* Skip to opening [ */
	while (*result_arr && *result_arr != '[') result_arr++;
	const char *arr_end = NULL;
	if (*result_arr == '[')
		arr_end = json_find_closing(result_arr);

	/* Parse peer objects into structs */
	p = result_arr;
	while (*p && peer_count < 256 && (!arr_end || p < arr_end)) {
		const char *obj = strchr(p, '{');
		if (!obj || (arr_end && obj >= arr_end)) break;
		const char *end = json_find_closing(obj);
		if (!end) break;

		size_t len = end - obj + 1;
		char *pj = malloc(len + 1);
		if (!pj) { p = end + 1; continue; }
		memcpy(pj, obj, len);
		pj[len] = '\0';

		PeerRow *pr = &peers[peer_count];
		memset(pr, 0, sizeof(PeerRow));

		/* Direction */
		json_get_string(pj, "connection_type", pr->conn_type, sizeof(pr->conn_type));
		if (strcmp(pr->conn_type, "inbound") == 0) {
			pr->is_inbound = 1;
			inbound++;
		} else {
			const char *inb = json_find_value(pj, "inbound");
			if (inb && strncmp(inb, "true", 4) == 0) {
				pr->is_inbound = 1;
				inbound++;
			} else {
				outbound++;
			}
		}

		if (strcmp(pr->conn_type, "block-relay-only") == 0)
			block_relay++;

		/* Network */
		json_get_string(pj, "network", pr->network, sizeof(pr->network));
		if (strcmp(pr->network, "ipv4") == 0) ipv4++;
		else if (strcmp(pr->network, "ipv6") == 0) ipv6++;
		else if (strcmp(pr->network, "onion") == 0) onion++;
		else if (strcmp(pr->network, "i2p") == 0) i2p++;
		else if (strcmp(pr->network, "cjdns") == 0) cjdns++;

		/* Timing */
		pr->minping = json_get_double(pj, "minping");
		pr->pingtime = json_get_double(pj, "pingtime");
		pr->lastsend = json_get_int(pj, "lastsend");
		pr->lastrecv = json_get_int(pj, "lastrecv");
		pr->last_transaction = json_get_int(pj, "last_transaction");
		pr->last_block = json_get_int(pj, "last_block");
		pr->conntime = json_get_int(pj, "conntime");

		/* BIP152 high-bandwidth */
		{
			const char *hb_from = json_find_value(pj, "bip152_hb_from");
			const char *hb_to = json_find_value(pj, "bip152_hb_to");
			pr->bip152_hb_from = (hb_from && strncmp(hb_from, "true", 4) == 0) ? 1 : 0;
			pr->bip152_hb_to = (hb_to && strncmp(hb_to, "true", 4) == 0) ? 1 : 0;
		}

		/* Address and version */
		json_get_string(pj, "addr", pr->addr, sizeof(pr->addr));
		json_get_string(pj, "subver", pr->subver, sizeof(pr->subver));

		total++;
		peer_count++;
		free(pj);
		p = end + 1;
	}

	/* Level 1-4: print peer table */
	if (level >= 1) {
		/* Header */
		printf("Peer connections:\n");
		printf("  %3s  %3s  %-16s %-6s", "#", "<->", "type", "net");

		printf("  %6s  %6s  %5s  %5s  %5s  %5s  %2s  %8s", "mping", "ping", "send", "recv", "txn", "blk", "hb", "age");
		if (level == 2 || level == 4)
			printf("  %-21s", "addr");
		if (level == 3 || level == 4)
			printf("  %-24s", "version");
		printf("\n");

		for (i = 0; i < peer_count; i++) {
			PeerRow *pr = &peers[i];

			/* outonly filter */
			if (outonly && pr->is_inbound)
				continue;

			const char *dir = pr->is_inbound ? "in" : "out";
			int mping_ms = (int)(pr->minping * 1000);
			int ping_ms = (int)(pr->pingtime * 1000);

			/* Time since last send/recv/tx/block */
			int send_ago = pr->lastsend ? (int)(now - pr->lastsend) : -1;
			int recv_ago = pr->lastrecv ? (int)(now - pr->lastrecv) : -1;
			int tx_ago = pr->last_transaction ? (int)(now - pr->last_transaction) : -1;
			int blk_ago = pr->last_block ? (int)(now - pr->last_block) : -1;

			/* Connection age */
			int age_secs = pr->conntime ? (int)(now - pr->conntime) : 0;
			char age_str[16];
			if (age_secs >= 86400)
				snprintf(age_str, sizeof(age_str), "%dd", age_secs / 86400);
			else if (age_secs >= 3600)
				snprintf(age_str, sizeof(age_str), "%dh", age_secs / 3600);
			else if (age_secs >= 60)
				snprintf(age_str, sizeof(age_str), "%dm", age_secs / 60);
			else
				snprintf(age_str, sizeof(age_str), "%ds", age_secs);

			printf("  %3d  %3s  %-16s %-6s", i + 1, dir, pr->conn_type, pr->network);
			printf("  %6d  %6d", mping_ms, ping_ms);

			if (send_ago >= 0) printf("  %5d", send_ago); else printf("  %5s", "-");
			if (recv_ago >= 0) printf("  %5d", recv_ago); else printf("  %5s", "-");
			if (tx_ago >= 0) printf("  %5d", tx_ago); else printf("  %5s", "-");
			if (blk_ago >= 0) printf("  %5d", blk_ago); else printf("  %5s", "-");

			/* hb column: compact block relay status
			 * "." = we selected peer as high-bandwidth (hb_to)
			 * "*" = peer selected us as high-bandwidth (hb_from) */
			{
				char hb_str[3] = "  ";
				if (pr->bip152_hb_to) hb_str[0] = '.';
				if (pr->bip152_hb_from) hb_str[1] = '*';
				printf("  %2s", hb_str);
			}
			printf("  %8s", age_str);

			if (level == 2 || level == 4)
				printf("  %-21s", pr->addr);
			if (level == 3 || level == 4)
				printf("  %-24s", pr->subver);
			printf("\n");
		}
		printf("\n");
	}

	/* Summary (all levels) */
	printf("Peer connections: %d (in %d, out %d", total, inbound, outbound);
	if (block_relay > 0) printf(", block-relay %d", block_relay);
	printf(")\n");

	printf("\nBy network:");
	if (ipv4 > 0) printf("  ipv4 %d", ipv4);
	if (ipv6 > 0) printf("  ipv6 %d", ipv6);
	if (onion > 0) printf("  onion %d", onion);
	if (i2p > 0) printf("  i2p %d", i2p);
	if (cjdns > 0) printf("  cjdns %d", cjdns);
	printf("\n");

	/* Local addresses from getnetworkinfo */
	net_json = rpc_call(rpc, "getnetworkinfo", "[]");
	if (net_json) {
		const char *local = json_find_array(net_json, "localaddresses");
		if (local) {
			const char *la = strchr(local, '{');
			if (la) {
				printf("\nLocal addresses:");
				while (la) {
					const char *la_end = json_find_closing(la);
					if (!la_end) break;
					size_t llen = la_end - la + 1;
					char *la_obj = malloc(llen + 1);
					if (la_obj) {
						char addr[256] = {0};
						memcpy(la_obj, la, llen);
						la_obj[llen] = '\0';
						json_get_string(la_obj, "address", addr, sizeof(addr));
						int port = (int)json_get_int(la_obj, "port");
						int score = (int)json_get_int(la_obj, "score");
						printf("  %s:%d (score %d)", addr, port, score);
						free(la_obj);
					}
					la = strchr(la_end + 1, '{');
				}
				printf("\n");
			}
		}
		free(net_json);
	}

	free(peers_json);
	return 0;
}

/* Connect with retry for -rpcwait, including warmup wait (error -28) */
static int rpc_connect_wait(RpcClient *rpc, int timeout_secs)
{
	time_t start = time(NULL);
	int attempt = 0;

	while (1) {
		attempt++;
		if (rpc_connect(rpc) == 0) {
			/* TCP connected — now check if node is warmed up
			 * by making a test RPC call */
			char *response = rpc_call(rpc, "getnetworkinfo", "[]");
			if (response) {
				/* Check for warmup error (code -28) */
				int error_code = 0;
				char *result = method_extract_result(response, &error_code);
				free(result);
				free(response);

				if (error_code == -28) {
					/* Node is warming up, disconnect and retry */
					if (attempt == 1)
						fprintf(stderr, "Waiting for server warmup...\n");
					rpc_disconnect(rpc);
					goto wait_and_retry;
				}
				/* Node is ready */
				return 0;
			}
			/* No response — node might be mid-startup, retry */
			rpc_disconnect(rpc);
			goto wait_and_retry;
		}

wait_and_retry:
		/* Check timeout */
		if (timeout_secs > 0) {
			time_t elapsed = time(NULL) - start;
			if (elapsed >= timeout_secs) {
				fprintf(stderr, "error: Timeout waiting for RPC server after %d seconds\n", timeout_secs);
				return -1;
			}
		}

		/* Wait before retry */
		if (attempt == 1) {
			fprintf(stderr, "Waiting for RPC server...\n");
		}
		sleep(1);
	}
}

int main(int argc, char **argv)
{
	Config cfg;
	RpcClient rpc;
	const MethodDef *method = NULL;
	char *result = NULL;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	int ret;
	char cookie_path[1024];

	/* Parse command-line arguments */
	if (config_parse_args(&cfg, argc, argv) < 0)
		return 1;

	/* Load config file (CLI args take priority) */
	if (cfg.conf_file[0]) {
		/* Explicit -conf= path */
		if (config_parse_file(&cfg, cfg.conf_file) < 0) {
			fprintf(stderr, "error: Could not read config file: %s\n", cfg.conf_file);
			return 1;
		}
	} else {
		/* Try default config file in datadir */
		char conf_path[1024];
		snprintf(conf_path, sizeof(conf_path), "%s/bitcoin.conf", cfg.datadir);
		config_parse_file(&cfg, conf_path);  /* Ignore failure - file optional */
	}

	/* Apply network defaults */
	config_apply_network_defaults(&cfg);

	/* Set up color output */
	if (cfg.color == COLOR_ALWAYS) {
		use_color = 1;
	} else if (cfg.color == COLOR_AUTO) {
		/* Auto: use color if stdout is a terminal */
		use_color = isatty(STDOUT_FILENO);
	}
	/* COLOR_NEVER: use_color stays 0 */

	/* Show version if requested */
	if (cfg.version) {
		print_version();
		return 0;
	}

	/* Show help if requested */
	if (cfg.help) {
		if (cfg.help_cmd[0]) {
			/* Help for specific command */
			method = method_find(cfg.help_cmd);
			if (method) {
				method_print_help(method);
			} else {
				fprintf(stderr, "Unknown command: %s\n", cfg.help_cmd);
				return 1;
			}
		} else {
			config_print_usage(argv[0]);
		}
		return 0;
	}

	/* Check for special info commands that don't need a command argument */
	int need_command = 1;
	if (cfg.getinfo || cfg.netinfo >= 0 || cfg.addrinfo || cfg.generate) {
		need_command = 0;
	}

	/* Need a command (unless using -getinfo or -netinfo) */
	if (need_command && (cfg.cmd_index < 0 || cfg.cmd_index >= argc)) {
		config_print_usage(argv[0]);
		return 1;
	}

	const char *command = NULL;

	/* Find command in registry (if not using special flags) */
	if (need_command) {
		command = argv[cfg.cmd_index];
		method = method_find(command);
		if (!method) {
			fprintf(stderr, "error: Unknown command: %s\n", command);
			fprintf(stderr, "Use -help for list of commands\n");
			return 1;
		}
	}

	/* Initialize RPC client */
	rpc_init(&rpc, cfg.host, cfg.port);
	rpc.timeout = cfg.rpc_timeout;

	/* Set wallet if specified */
	if (cfg.wallet[0])
		rpc_set_wallet(&rpc, cfg.wallet);

	/* Read password from stdin if requested */
	if (cfg.stdinrpcpass) {
		fprintf(stderr, "RPC password: ");
		read_password_stdin(cfg.password, sizeof(cfg.password));
		fprintf(stderr, "\n");
	}

	/* Read wallet passphrase from stdin if requested */
	char wallet_passphrase[256] = {0};
	if (cfg.stdinwalletpassphrase) {
		read_password_stdin(wallet_passphrase, sizeof(wallet_passphrase));
	}

	/* Set up authentication */
	if (cfg.cookie_file[0]) {
		/* Explicit cookie file */
		if (rpc_auth_cookie(&rpc, cfg.cookie_file) < 0) {
			fprintf(stderr, "error: Could not read cookie file: %s\n", cfg.cookie_file);
			return 1;
		}
	} else if (cfg.user[0] && cfg.password[0]) {
		/* Explicit user/password */
		rpc_auth_userpass(&rpc, cfg.user, cfg.password);
	} else {
		/* Try auto-detection: cookie first (for selected network), then config */
		get_cookie_path(cookie_path, sizeof(cookie_path), cfg.datadir, cfg.network);

		if (rpc_auth_cookie(&rpc, cookie_path) < 0) {
			/* Try config file auth */
			if (rpc_auth_auto(&rpc, cfg.datadir) < 0) {
				fprintf(stderr, "error: Could not find authentication\n");
				fprintf(stderr, "Tried: %s\n", cookie_path);
				fprintf(stderr, "Try: -rpcuser=<user> -rpcpassword=<password>\n");
				return 1;
			}
		}
	}

	/* Connect to node (with retry if -rpcwait) */
	if (cfg.rpcwait) {
		if (rpc_connect_wait(&rpc, cfg.rpcwait_timeout) < 0) {
			return 1;
		}
	} else {
		if (rpc_connect(&rpc) < 0) {
			if (fallback_has_any(&cfg.fallback)) {
				fprintf(stderr, "warning: Could not connect to %s:%d — using fallbacks\n",
				        cfg.host, cfg.port);
			} else {
				fprintf(stderr, "error: Could not connect to %s:%d\n", cfg.host, cfg.port);
				fprintf(stderr, "Is bitcoind running?\n");
				return 1;
			}
		}
	}

	/* Handle special info commands */
	if (cfg.getinfo) {
		ret = handle_getinfo(&rpc, cfg.wallet);
		rpc_disconnect(&rpc);
		return ret;
	}
	if (cfg.netinfo >= 0) {
		/* Check for "outonly"/"o" in remaining args */
		int outonly = 0;
		if (cfg.cmd_index >= 0 && cfg.cmd_index < argc) {
			const char *arg = argv[cfg.cmd_index];
			if (strcmp(arg, "outonly") == 0 || strcmp(arg, "o") == 0)
				outonly = 1;
		}
		ret = handle_netinfo(&rpc, cfg.netinfo, outonly);
		rpc_disconnect(&rpc);
		return ret;
	}
	if (cfg.addrinfo) {
		ret = handle_addrinfo(&rpc);
		rpc_disconnect(&rpc);
		return ret;
	}
	if (cfg.generate) {
		ret = handle_generate(&rpc, argc, argv, cfg.cmd_index);
		rpc_disconnect(&rpc);
		return ret;
	}

	/* Set named parameter mode if requested */
	if (cfg.named)
		method_set_named_mode(1);

	/* Set up P2P verification if requested */
	if (cfg.verify)
		method_set_verify(cfg.verify, cfg.verify_peers, cfg.network);

	/* Set up fallback broadcast if configured */
	if (fallback_has_any(&cfg.fallback))
		method_set_fallback(&cfg.fallback);

	/* Build and execute command */
	int cmd_argc = argc - cfg.cmd_index - 1;
	char **cmd_argv = &argv[cfg.cmd_index + 1];

	/* If -stdinwalletpassphrase and command is walletpassphrase,
	 * prepend the passphrase as first argument */
	char **wp_argv = NULL;
	if (cfg.stdinwalletpassphrase && wallet_passphrase[0] && command &&
	    strcmp(command, "walletpassphrase") == 0) {
		int wp_argc = cmd_argc + 1;
		wp_argv = malloc(sizeof(char *) * wp_argc);
		if (wp_argv) {
			int i;
			wp_argv[0] = wallet_passphrase;
			for (i = 0; i < cmd_argc; i++)
				wp_argv[i + 1] = cmd_argv[i];
			cmd_argv = wp_argv;
			cmd_argc = wp_argc;
		}
	}

	/* Read additional args from stdin if requested */
	char **all_argv = cmd_argv;
	int all_argc = cmd_argc;
	char *stdin_buf = NULL;
	char **stdin_args = NULL;
	int stdin_count = 0;

	if (cfg.stdin_rpc) {
		/* Read all of stdin */
		size_t buf_size = 4096;
		size_t buf_len = 0;
		stdin_buf = malloc(buf_size);
		if (stdin_buf) {
			int c;
			while ((c = getchar()) != EOF) {
				if (buf_len >= buf_size - 1) {
					buf_size *= 2;
					stdin_buf = realloc(stdin_buf, buf_size);
					if (!stdin_buf) break;
				}
				stdin_buf[buf_len++] = c;
			}
			if (stdin_buf) {
				stdin_buf[buf_len] = '\0';

				/* Parse stdin into arguments: one argument per line
				 * (matches bitcoin-cli behavior) */
				stdin_args = malloc(sizeof(char *) * 64);
				if (stdin_args) {
					char *line = stdin_buf;
					while (*line && stdin_count < 64) {
						/* Find end of line */
						char *eol = strchr(line, '\n');
						if (eol)
							*eol = '\0';

						/* Trim trailing \r */
						size_t llen = strlen(line);
						if (llen > 0 && line[llen - 1] == '\r')
							line[llen - 1] = '\0';

						/* Skip empty lines */
						if (line[0] != '\0')
							stdin_args[stdin_count++] = line;

						if (eol)
							line = eol + 1;
						else
							break;
					}

					/* Combine cmd_argv with stdin_args */
					if (stdin_count > 0) {
						all_argc = cmd_argc + stdin_count;
						all_argv = malloc(sizeof(char *) * all_argc);
						if (all_argv) {
							int i;
							for (i = 0; i < cmd_argc; i++)
								all_argv[i] = cmd_argv[i];
							for (i = 0; i < stdin_count; i++)
								all_argv[cmd_argc + i] = stdin_args[i];
						}
					}
				}
			}
		}
	}

	/* Execute command handler */
	ret = method->handler(&rpc, all_argc, all_argv, &result);

	/* Cleanup stdin resources */
	if (stdin_buf) free(stdin_buf);
	if (stdin_args) free(stdin_args);
	if (all_argv != cmd_argv) free(all_argv);
	if (wp_argv) free(wp_argv);

	/* Output result */
	if (result) {
		/* Check if result looks like JSON */
		const char *p = result;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;

		/* Errors go to stderr, normal output to stdout */
		FILE *dest = (ret != 0) ? stderr : stdout;

		if (*p == '{' || *p == '[') {
			/* Pretty print JSON */
			fprint_json_pretty(dest, result, 0);
		} else {
			/* Plain output */
			fprintf(dest, "%s\n", result);
		}
		free(result);
	}

	/* Cleanup */
	rpc_disconnect(&rpc);
	memset(wallet_passphrase, 0, sizeof(wallet_passphrase));

	return ret;
}
