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

#include <ctype.h>

#include "config.h"
#include "methods.h"
#include "rpc.h"
#include "json.h"
#include "fallback.h"

#define BTC_CLI_VERSION "0.10.0"

/* ANSI color codes */
#define C_RESET   "\033[0m"
#define C_KEY     "\033[36m"   /* Cyan for keys */
#define C_STRING  "\033[32m"   /* Green for strings */
#define C_NUMBER  "\033[33m"   /* Yellow for numbers */
#define C_BOOL    "\033[35m"   /* Magenta for true/false/null */
#define C_BRACE   "\033[1m"    /* Bold for {} [] */

/* Global color setting */
static int use_color = 0;

/* Build raw JSON params array from argv with type inference (for unknown methods) */
static char *build_raw_params(int argc, char **argv)
{
	char *buf;
	size_t bufsize = 4096;
	size_t pos = 0;
	int i;

	buf = malloc(bufsize);
	if (!buf) return NULL;

	buf[pos++] = '[';

	for (i = 0; i < argc; i++) {
		const char *arg = argv[i];
		size_t arglen = strlen(arg);

		while (pos + arglen + 64 > bufsize) {
			bufsize *= 2;
			buf = realloc(buf, bufsize);
			if (!buf) return NULL;
		}

		if (i > 0) buf[pos++] = ',';

		/* Type inference */
		if (strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0 ||
		    strcmp(arg, "null") == 0) {
			pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
		} else if (arg[0] == '[' || arg[0] == '{') {
			/* Already JSON */
			pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
		} else {
			/* Check if numeric */
			const char *s = arg;
			int is_num = 1;
			if (*s == '-') s++;
			if (!*s) is_num = 0;
			while (*s) {
				if (!isdigit(*s) && *s != '.') { is_num = 0; break; }
				s++;
			}
			if (is_num)
				pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
			else
				pos += snprintf(buf + pos, bufsize - pos, "\"%s\"", arg);
		}
	}

	buf[pos++] = ']';
	buf[pos] = '\0';
	return buf;
}

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
	printf("Bitcoin Core RPC client version v30.2.0\n");
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
		/* Wrap result in {"address": "<addr>", "blocks": [...]} */
		printf("{\n  \"address\": \"%s\",\n  \"blocks\": ", address);
		const char *p = result;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;
		if (*p == '{' || *p == '[')
			fprint_json_pretty(stdout, result, 1);
		else
			printf("%s\n", result);
		printf("}\n");
		free(result);
	}

	return error_code != 0 ? 1 : 0;
}

/* Handle -getinfo: combined node information (matches bitcoin-cli text format) */
static int handle_getinfo(RpcClient *rpc, const char *wallet_name)
{
	char *blockchain = NULL;
	char *network = NULL;
	char *walletlist = NULL;
	char buf[256];

	/* Collect data */
	blockchain = rpc_call(rpc, "getblockchaininfo", "[]");
	network = rpc_call(rpc, "getnetworkinfo", "[]");

	/* Chain info */
	if (blockchain) {
		json_get_string(blockchain, "chain", buf, sizeof(buf));
		printf("Chain: %s\n", buf);
		printf("Blocks: %d\n", (int)json_get_int(blockchain, "blocks"));
		printf("Headers: %d\n", (int)json_get_int(blockchain, "headers"));
		printf("Verification progress: %.4f%%\n",
		       json_get_double(blockchain, "verificationprogress") * 100.0);
		printf("Difficulty: %.15g\n", json_get_double(blockchain, "difficulty"));
	}

	/* Network info */
	if (network) {
		int conn_in = (int)json_get_int(network, "connections_in");
		int conn_out = (int)json_get_int(network, "connections_out");
		int conn_total = (int)json_get_int(network, "connections");
		printf("\nNetwork: in %d, out %d, total %d\n", conn_in, conn_out, conn_total);
		printf("Version: %d\n", (int)json_get_int(network, "version"));
		printf("Time offset (s): %d\n", (int)json_get_int(network, "timeoffset"));

		/* Proxy */
		char proxy[256] = {0};
		const char *networks_arr = json_find_array(network, "networks");
		if (networks_arr) {
			const char *first_net = strchr(networks_arr, '{');
			if (first_net) {
				const char *first_end = json_find_closing(first_net);
				if (first_end) {
					size_t nlen = first_end - first_net + 1;
					char *net_obj = malloc(nlen + 1);
					if (net_obj) {
						memcpy(net_obj, first_net, nlen);
						net_obj[nlen] = '\0';
						json_get_string(net_obj, "proxy", proxy, sizeof(proxy));
						free(net_obj);
					}
				}
			}
		}
		printf("Proxies: %s\n", proxy[0] ? proxy : "n/a");
		printf("Min tx relay fee rate (BTC/kvB): %.8f\n",
		       json_get_double(network, "relayfee"));
	}

	/* Detect wallet situation */
	int no_wallet = 0;
	int wallet_count = 0;
	walletlist = rpc_call(rpc, "listwallets", "[]");
	if (walletlist) {
		const char *arr_start = json_find_value(walletlist, "result");
		if (!arr_start) arr_start = walletlist;
		while (*arr_start && *arr_start != '[') arr_start++;

		if (*arr_start == '[') {
			const char *arr_end_tmp = json_find_closing(arr_start);
			if (!arr_end_tmp) arr_end_tmp = arr_start;

			const char *q = arr_start + 1;
			while (q < arr_end_tmp) {
				if (*q == '"') {
					wallet_count++;
					q++;
					while (q < arr_end_tmp && *q != '"') {
						if (*q == '\\') q++;
						q++;
					}
				}
				if (q < arr_end_tmp) q++;
			}

			if (wallet_count == 0 && !wallet_name[0])
				no_wallet = 1;
		}
	}

	/* Multi-wallet: show balances list */
	if (wallet_count > 1 && !wallet_name[0]) {
		printf("\nBalances\n");

		const char *arr_start = json_find_value(walletlist, "result");
		if (!arr_start) arr_start = walletlist;
		while (*arr_start && *arr_start != '[') arr_start++;
		const char *arr_end = json_find_closing(arr_start);
		if (!arr_end) arr_end = arr_start;

		const char *p = arr_start + 1;
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

				rpc_set_wallet(rpc, wname);
				char *wb = rpc_call(rpc, "getbalances", "[]");
				if (wb) {
					const char *mine = json_find_object(wb, "mine");
					double bal = mine ? json_get_double(mine, "trusted") : 0;
					printf("%12.8f %s\n", bal, wname);
					free(wb);
				}
			}
			p = end + 1;
		}
	} else if (!no_wallet) {
		/* Single wallet or specific -rpcwallet */
		char *winfo = rpc_call(rpc, "getwalletinfo", "[]");
		if (winfo) {
			char wname[256] = {0};
			json_get_string(winfo, "walletname", wname, sizeof(wname));
			printf("\nWallet: %s\n", wname);
			printf("Keypool size: %d\n", (int)json_get_int(winfo, "keypoolsize"));
			printf("Transaction fee rate (-paytxfee) (BTC/kvB): %.8f\n",
			       json_get_double(winfo, "paytxfee"));
			free(winfo);
		}

		char *balances = rpc_call(rpc, "getbalances", "[]");
		if (balances) {
			const char *mine = json_find_object(balances, "mine");
			if (mine)
				printf("\nBalance: %.8f\n", json_get_double(mine, "trusted"));
			free(balances);
		}
	}
	free(walletlist);

	/* Warnings */
	if (network) {
		char warnings[1024] = {0};
		json_get_string(network, "warnings", warnings, sizeof(warnings));
		printf("\nWarnings: %s\n", warnings[0] ? warnings : "(none)");
	}

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
	int total = 0, inbound = 0, outbound = 0, block_relay = 0, manual = 0;
	int ipv4_in = 0, ipv6_in = 0, onion_in = 0, i2p_in = 0, cjdns_in = 0;
	int ipv4_out = 0, ipv6_out = 0, onion_out = 0, i2p_out = 0, cjdns_out = 0;
	int block_relay_out = 0;
	const char *p;
	int i;
	time_t now = time(NULL);

	/* Fetch getnetworkinfo first (needed for header banner and local info) */
	net_json = rpc_call(rpc, "getnetworkinfo", "[]");

	peers_json = rpc_call(rpc, "getpeerinfo", "[]");
	if (!peers_json) {
		fprintf(stderr, "error: Could not get peer info\n");
		free(net_json);
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

		if (strcmp(pr->conn_type, "block-relay-only") == 0) {
			block_relay++;
			if (!pr->is_inbound) block_relay_out++;
		}
		if (strcmp(pr->conn_type, "manual") == 0)
			manual++;

		/* Network - track per-direction counts */
		json_get_string(pj, "network", pr->network, sizeof(pr->network));
		if (pr->is_inbound) {
			if (strcmp(pr->network, "ipv4") == 0) ipv4_in++;
			else if (strcmp(pr->network, "ipv6") == 0) ipv6_in++;
			else if (strcmp(pr->network, "onion") == 0) onion_in++;
			else if (strcmp(pr->network, "i2p") == 0) i2p_in++;
			else if (strcmp(pr->network, "cjdns") == 0) cjdns_in++;
		} else {
			if (strcmp(pr->network, "ipv4") == 0) ipv4_out++;
			else if (strcmp(pr->network, "ipv6") == 0) ipv6_out++;
			else if (strcmp(pr->network, "onion") == 0) onion_out++;
			else if (strcmp(pr->network, "i2p") == 0) i2p_out++;
			else if (strcmp(pr->network, "cjdns") == 0) cjdns_out++;
		}

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

	/* Header banner from getnetworkinfo */
	{
		char subver[256] = {0};
		char chain[64] = {0};
		int protover = 0;
		/* Extract from net_json result */
		if (net_json) {
			const char *nr = json_find_value(net_json, "result");
			if (nr && *nr == '{') {
				size_t nlen;
				const char *nend = json_find_closing(nr);
				char *nobj;
				if (nend) {
					nlen = nend - nr + 1;
					nobj = malloc(nlen + 1);
					if (nobj) {
						memcpy(nobj, nr, nlen);
						nobj[nlen] = '\0';
						json_get_string(nobj, "subversion", subver, sizeof(subver));
						protover = (int)json_get_int(nobj, "protocolversion");
						free(nobj);
					}
				}
			}
		}
		/* Get chain from getblockchaininfo */
		{
			char *bc = rpc_call(rpc, "getblockchaininfo", "[]");
			if (bc) {
				const char *br = json_find_value(bc, "result");
				if (br && *br == '{') {
					const char *bend = json_find_closing(br);
					if (bend) {
						size_t blen = bend - br + 1;
						char *bobj = malloc(blen + 1);
						if (bobj) {
							memcpy(bobj, br, blen);
							bobj[blen] = '\0';
							json_get_string(bobj, "chain", chain, sizeof(chain));
							free(bobj);
						}
					}
				}
				free(bc);
			}
		}
		printf("btc-cli client v%s %s - server %d/%s/\n",
		       BTC_CLI_VERSION, chain[0] ? chain : "main",
		       protover, subver[0] ? subver : "unknown");
	}

	/* Level 1-4: print peer table (only when peers exist) */
	if (level >= 1 && peer_count > 0) {
		printf("\nPeer connections sorted by direction and min ping\n");
		printf(" <->   type   net  mping   ping send recv  txn  blk  hb");
		if (level == 2 || level == 4)
			printf("  addr");
		if (level == 3 || level == 4)
			printf("  version");
		printf("\n");

		for (i = 0; i < peer_count; i++) {
			PeerRow *pr = &peers[i];

			/* outonly filter */
			if (outonly && pr->is_inbound)
				continue;

			const char *dir = pr->is_inbound ? " in" : "out";
			int mping_ms = (int)(pr->minping * 1000);
			int ping_ms = (int)(pr->pingtime * 1000);

			/* Time since last send/recv in seconds */
			int send_ago = pr->lastsend ? (int)(now - pr->lastsend) : -1;
			int recv_ago = pr->lastrecv ? (int)(now - pr->lastrecv) : -1;
			/* Time since last tx/block in minutes */
			int tx_min = pr->last_transaction ? (int)((now - pr->last_transaction) / 60) : -1;
			int blk_min = pr->last_block ? (int)((now - pr->last_block) / 60) : -1;

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

			/* Map connection_type to short form */
			const char *type_str = "full";
			if (strcmp(pr->conn_type, "block-relay-only") == 0) type_str = "block";
			else if (strcmp(pr->conn_type, "manual") == 0) type_str = "manual";
			else if (strcmp(pr->conn_type, "feeler") == 0) type_str = "feeler";
			else if (strcmp(pr->conn_type, "addr-fetch") == 0) type_str = "addr";

			printf(" %3s  %6s  %4s", dir, type_str, pr->network);
			printf("  %5d  %5d", mping_ms, ping_ms);

			if (send_ago >= 0) printf("  %3d", send_ago); else printf("    .");
			if (recv_ago >= 0) printf("  %3d", recv_ago); else printf("    .");
			if (tx_min >= 0) printf("  %3d", tx_min); else printf("    *");
			if (blk_min >= 0) printf("  %3d", blk_min); else printf("    .");

			/* hb column */
			{
				char hb_str[3] = "  ";
				if (pr->bip152_hb_to) hb_str[0] = '.';
				if (pr->bip152_hb_from) hb_str[1] = '*';
				printf("  %2s", hb_str);
			}

			if (level == 2 || level == 4)
				printf("  %s", pr->addr);
			if (level == 3 || level == 4)
				printf("  %s", pr->subver);
			printf("\n");
		}
	}

	/* Network summary grid (all levels) */
	{
		int ipv4_total = ipv4_in + ipv4_out;
		int ipv6_total = ipv6_in + ipv6_out;
		int onion_total = onion_in + onion_out;
		int i2p_total = i2p_in + i2p_out;
		int cjdns_total = cjdns_in + cjdns_out;

		/* Determine which optional network columns to show */
		int show_onion = (onion_total > 0);
		int show_i2p = (i2p_total > 0);
		int show_cjdns = (cjdns_total > 0);

		/* Header row */
		printf("\n        ipv4    ipv6");
		if (show_onion) printf("   onion");
		if (show_i2p) printf("     i2p");
		if (show_cjdns) printf("   cjdns");
		printf("   total   block\n");

		/* In row */
		printf("in    %5d  %5d", ipv4_in, ipv6_in);
		if (show_onion) printf("  %5d", onion_in);
		if (show_i2p) printf("  %5d", i2p_in);
		if (show_cjdns) printf("  %5d", cjdns_in);
		printf("  %5d\n", inbound);

		/* Out row (includes block relay count) */
		printf("out   %5d  %5d", ipv4_out, ipv6_out);
		if (show_onion) printf("  %5d", onion_out);
		if (show_i2p) printf("  %5d", i2p_out);
		if (show_cjdns) printf("  %5d", cjdns_out);
		printf("  %5d  %5d\n", outbound, block_relay_out);

		/* Total row */
		printf("total %5d  %5d", ipv4_total, ipv6_total);
		if (show_onion) printf("  %5d", onion_total);
		if (show_i2p) printf("  %5d", i2p_total);
		if (show_cjdns) printf("  %5d", cjdns_total);
		printf("  %5d\n", total);
	}

	/* Local services from getnetworkinfo */
	if (net_json) {
		const char *nr = json_find_value(net_json, "result");
		if (nr && *nr == '{') {
			const char *services = json_find_array(nr, "localservicesnames");
			if (services) {
				printf("\nLocal services:");
				/* Parse array of strings */
				const char *sp = services;
				int first = 1;
				while (*sp) {
					const char *q = strchr(sp, '"');
					if (!q) break;
					q++;
					const char *qe = strchr(q, '"');
					if (!qe) break;
					size_t slen = qe - q;
					if (slen > 0) {
						printf("%s %.*s", first ? "" : ",", (int)slen, q);
						first = 0;
					}
					sp = qe + 1;
				}
				printf("\n");
			}
		}
	}

	/* Local addresses */
	if (net_json) {
		const char *local = json_find_array(net_json, "localaddresses");
		int has_addr = 0;
		if (local) {
			const char *la = strchr(local, '{');
			if (la) {
				has_addr = 1;
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
		if (!has_addr) {
			printf("\nLocal addresses: n/a\n");
		}
		free(net_json);
	} else {
		printf("\nLocal addresses: n/a\n");
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
		/* Unknown methods are forwarded to the server (like bitcoin-cli) */
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
				fprintf(stderr, "error: timeout on transient error: Could not connect to the server %s:%d\n\nMake sure the bitcoind server is running and that you are connecting to the correct RPC port.\nUse \"bitcoin-cli -help\" for more info.\n", cfg.host, cfg.port);
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
		/* Check for "help"/"h" or "outonly"/"o" in remaining args */
		int outonly = 0;
		if (cfg.cmd_index >= 0 && cfg.cmd_index < argc) {
			const char *arg = argv[cfg.cmd_index];
			if (strcmp(arg, "help") == 0 || strcmp(arg, "h") == 0) {
				printf(
					"-netinfo (level [outonly]) | help\n"
					"\n"
					"Returns a network peer connections dashboard with information from the remote server.\n"
					"This human-readable interface will change regularly and is not intended to be a stable API.\n"
					"Under the hood, -netinfo fetches the data by calling getpeerinfo and getnetworkinfo.\n"
					"An optional argument from 0 to 4 can be passed for different peers listings; values above 4 up to 255 are parsed as 4.\n"
					"If that argument is passed, an optional additional \"outonly\" argument may be passed to see outbound peers only.\n"
					"Pass \"help\" or \"h\" to see this detailed help documentation.\n"
					"If more than two arguments are passed, only the first two are read and parsed.\n"
					"Suggestion: use -netinfo with the Linux watch(1) command for a live dashboard; see example below.\n"
					"\n"
					"Arguments:\n"
					"1. level (integer 0-4, optional)  Specify the info level of the peers dashboard (default 0):\n"
					"                                  0 - Peer counts for each reachable network as well as for block relay peers\n"
					"                                      and manual peers, and the list of local addresses and ports\n"
					"                                  1 - Like 0 but preceded by a peers listing (without address and version columns)\n"
					"                                  2 - Like 1 but with an address column\n"
					"                                  3 - Like 1 but with a version column\n"
					"                                  4 - Like 1 but with both address and version columns\n"
					"2. outonly (\"outonly\" or \"o\", optional) Return the peers listing with outbound peers only, i.e. to save screen space\n"
					"                                        when a node has many inbound peers. Only valid if a level is passed.\n"
					"\n"
					"help (\"help\" or \"h\", optional) Print this help documentation instead of the dashboard.\n"
					"\n"
					"Result:\n"
					"\n"
					"* The peers listing in levels 1-4 displays all of the peers sorted by direction and minimum ping time:\n"
					"\n"
					"  Column   Description\n"
					"  ------   -----------\n"
					"  <->      Direction\n"
					"           \"in\"  - inbound connections are those initiated by the peer\n"
					"           \"out\" - outbound connections are those initiated by us\n"
					"  type     Type of peer connection\n"
					"           \"full\"   - full relay, the default\n"
					"           \"block\"  - block relay; like full relay but does not relay transactions or addresses\n"
					"           \"manual\" - peer we manually added using RPC addnode or the -addnode/-connect config options\n"
					"           \"feeler\" - short-lived connection for testing addresses\n"
					"           \"addr\"   - address fetch; short-lived connection for requesting addresses\n"
					"  net      Network the peer connected through (\"ipv4\", \"ipv6\", \"onion\", \"i2p\", \"cjdns\", or \"npr\" (not publicly routable))\n"
					"  serv     Services offered by the peer\n"
					"           \"n\" - NETWORK: peer can serve the full block chain\n"
					"           \"b\" - BLOOM: peer can handle bloom-filtered connections (see BIP 111)\n"
					"           \"w\" - WITNESS: peer can be asked for blocks and transactions with witness data (SegWit)\n"
					"           \"c\" - COMPACT_FILTERS: peer can handle basic block filter requests (see BIPs 157 and 158)\n"
					"           \"l\" - NETWORK_LIMITED: peer limited to serving only the last 288 blocks (~2 days)\n"
					"           \"2\" - P2P_V2: peer supports version 2 P2P transport protocol, as defined in BIP 324\n"
					"           \"u\" - UNKNOWN: unrecognized bit flag\n"
					"  v        Version of transport protocol used for the connection\n"
					"  mping    Minimum observed ping time, in milliseconds (ms)\n"
					"  ping     Last observed ping time, in milliseconds (ms)\n"
					"  send     Time since last message sent to the peer, in seconds\n"
					"  recv     Time since last message received from the peer, in seconds\n"
					"  txn      Time since last novel transaction received from the peer and accepted into our mempool, in minutes\n"
					"           \"*\" - we do not relay transactions to this peer (getpeerinfo \"relaytxes\" is false)\n"
					"  blk      Time since last novel block passing initial validity checks received from the peer, in minutes\n"
					"  hb       High-bandwidth BIP152 compact block relay\n"
					"           \".\" (to)   - we selected the peer as a high-bandwidth peer\n"
					"           \"*\" (from) - the peer selected us as a high-bandwidth peer\n"
					"  addrp    Total number of addresses processed, excluding those dropped due to rate limiting\n"
					"           \".\" - we do not relay addresses to this peer (getpeerinfo \"addr_relay_enabled\" is false)\n"
					"  addrl    Total number of addresses dropped due to rate limiting\n"
					"  age      Duration of connection to the peer, in minutes\n"
					"  asmap    Mapped AS (Autonomous System) number at the end of the BGP route to the peer, used for diversifying\n"
					"           peer selection (only displayed if the -asmap config option is set)\n"
					"  id       Peer index, in increasing order of peer connections since node startup\n"
					"  address  IP address and port of the peer\n"
					"  version  Peer version and subversion concatenated, e.g. \"70016/Satoshi:21.0.0/\"\n"
					"\n"
					"* The peer counts table displays the number of peers for each reachable network as well as\n"
					"  the number of block relay peers and manual peers.\n"
					"\n"
					"* The local addresses table lists each local address broadcast by the node, the port, and the score.\n"
					"\n"
					"Examples:\n"
					"\n"
					"Peer counts table of reachable networks and list of local addresses\n"
					"> btc-cli -netinfo\n"
					"\n"
					"The same, preceded by a peers listing without address and version columns\n"
					"> btc-cli -netinfo 1\n"
					"\n"
					"Full dashboard\n"
					"> btc-cli -netinfo 4\n"
					"\n"
					"Full dashboard, but with outbound peers only\n"
					"> btc-cli -netinfo 4 outonly\n"
					"\n"
					"Full live dashboard, adjust --interval or --no-title as needed (Linux)\n"
					"> watch --interval 1 --no-title btc-cli -netinfo 4\n"
					"\n"
					"See this help\n"
					"> btc-cli -netinfo help\n"
				);
				rpc_disconnect(&rpc);
				return 0;
			}
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

	/* Error on empty stdin input */
	if (cfg.stdin_rpc && stdin_count == 0) {
		fprintf(stderr, "error: Reading from stdin\n");
		if (stdin_buf) free(stdin_buf);
		if (stdin_args) free(stdin_args);
		if (wp_argv) free(wp_argv);
		rpc_disconnect(&rpc);
		return 1;
	}

	/* Execute command handler */
	if (method) {
		ret = method->handler(&rpc, all_argc, all_argv, &result);
	} else {
		/* Unknown method: forward to server (matches bitcoin-cli behavior) */
		char *params = build_raw_params(all_argc, all_argv);
		char *response = rpc_call(&rpc, command, params ? params : "[]");
		free(params);
		if (!response) {
			if (rpc.last_http_error == 401) {
				result = strdup("error: Authorization failed: Incorrect rpcuser or rpcpassword");
			} else {
				result = strdup("error: Could not connect to the server");
			}
			ret = 1;
		} else {
			int error_code;
			result = method_extract_result(response, &error_code);
			free(response);
			ret = error_code != 0 ? abs(error_code) : 0;
		}
	}

	/* Cleanup stdin resources */
	if (stdin_buf) free(stdin_buf);
	if (stdin_args) free(stdin_args);
	if (all_argv != cmd_argv) free(all_argv);
	if (wp_argv) free(wp_argv);

	/* Special output handling for createwallet/loadwallet:
	 * bitcoin-cli extracts just the "name" field, prints warning if any */
	if (ret == 0 && result && command &&
	    (strcmp(command, "createwallet") == 0 || strcmp(command, "loadwallet") == 0)) {
		const char *p = result;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;
		if (*p == '{') {
			char name[256] = {0};
			char warning[2048] = {0};
			json_get_string(result, "name", name, sizeof(name));
			json_get_string(result, "warning", warning, sizeof(warning));
			free(result);
			result = NULL;
			if (warning[0]) {
				result = strdup(warning);
			}
		}
	}

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
