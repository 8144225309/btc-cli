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
#endif

#include "config.h"
#include "methods.h"
#include "rpc.h"
#include "json.h"

#define BTC_CLI_VERSION "1.0.0"

/* ANSI color codes */
#define C_RESET   "\033[0m"
#define C_KEY     "\033[36m"   /* Cyan for keys */
#define C_STRING  "\033[32m"   /* Green for strings */
#define C_NUMBER  "\033[33m"   /* Yellow for numbers */
#define C_BOOL    "\033[35m"   /* Magenta for true/false/null */
#define C_BRACE   "\033[1m"    /* Bold for {} [] */

/* Global color setting */
static int use_color = 0;

/* Pretty print JSON output with optional color */
static void print_json_pretty(const char *json, int indent)
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
					printf("%s", is_key ? C_KEY : C_STRING);
			}
			putchar(*p);
			if (in_string) {
				if (use_color)
					printf("%s", C_RESET);
				is_key = 0;
			}
			in_string = !in_string;
		} else if (in_string) {
			putchar(*p);
		} else if (*p == '{' || *p == '[') {
			if (use_color)
				printf("%s", C_BRACE);
			putchar(*p);
			if (use_color)
				printf("%s", C_RESET);
			putchar('\n');
			level++;
			for (i = 0; i < level * 2; i++) putchar(' ');
		} else if (*p == '}' || *p == ']') {
			putchar('\n');
			level--;
			for (i = 0; i < level * 2; i++) putchar(' ');
			if (use_color)
				printf("%s", C_BRACE);
			putchar(*p);
			if (use_color)
				printf("%s", C_RESET);
		} else if (*p == ',') {
			putchar(*p);
			putchar('\n');
			for (i = 0; i < level * 2; i++) putchar(' ');
		} else if (*p == ':') {
			putchar(*p);
			putchar(' ');
		} else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			/* Skip whitespace - we add our own */
		} else {
			/* Numbers, booleans, null */
			if (use_color) {
				if (*p == 't' || *p == 'f' || *p == 'n')
					printf("%s", C_BOOL);  /* true/false/null */
				else if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
					printf("%s", C_NUMBER);
			}
			putchar(*p);
			/* Check if next char ends the token */
			char next = *(p + 1);
			if (use_color && (next == ',' || next == '}' || next == ']' ||
			    next == ' ' || next == '\n' || next == '\0'))
				printf("%s", C_RESET);
		}
		p++;
	}
	putchar('\n');
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

/* Handle -getinfo: combined node information */
static int handle_getinfo(RpcClient *rpc)
{
	char *blockchain = NULL;
	char *network = NULL;
	char *wallet = NULL;
	char buf[256];
	int chain_height = 0;
	int connections = 0;
	double difficulty = 0;
	double balance = 0;
	int has_wallet = 0;

	/* Get blockchain info */
	blockchain = rpc_call(rpc, "getblockchaininfo", "[]");
	if (blockchain) {
		chain_height = (int)json_get_int(blockchain, "blocks");
		difficulty = json_get_double(blockchain, "difficulty");
		json_get_string(blockchain, "chain", buf, sizeof(buf));
		printf("Chain: %s\n", buf);
		printf("Blocks: %d\n", chain_height);
		printf("Difficulty: %.4g\n", difficulty);

		/* Verification progress */
		double progress = json_get_double(blockchain, "verificationprogress");
		if (progress > 0 && progress < 0.9999) {
			printf("Sync: %.2f%%\n", progress * 100);
		}
		free(blockchain);
	}

	/* Get network info */
	network = rpc_call(rpc, "getnetworkinfo", "[]");
	if (network) {
		connections = (int)json_get_int(network, "connections");
		json_get_string(network, "subversion", buf, sizeof(buf));
		printf("Connections: %d\n", connections);
		printf("Node: %s\n", buf);
		free(network);
	}

	/* Try to get wallet info (may fail if no wallet loaded) */
	wallet = rpc_call(rpc, "getbalances", "[]");
	if (wallet) {
		/* Try to get trusted balance from mine.trusted */
		const char *mine = json_find_object(wallet, "mine");
		if (mine) {
			balance = json_get_double(mine, "trusted");
			has_wallet = 1;
		}
		free(wallet);
	}

	if (has_wallet) {
		printf("Balance: %.8f BTC\n", balance);
	}

	return 0;
}

/* Handle -netinfo: network peer summary */
static int handle_netinfo(RpcClient *rpc)
{
	char *peers = NULL;
	int total = 0, inbound = 0, outbound = 0;
	int ipv4 = 0, ipv6 = 0, onion = 0, i2p = 0;
	const char *p;

	peers = rpc_call(rpc, "getpeerinfo", "[]");
	if (!peers) {
		fprintf(stderr, "error: Could not get peer info\n");
		return 1;
	}

	/* Count peers by parsing JSON array */
	p = peers;
	while (*p) {
		/* Find each peer object */
		const char *peer_start = strchr(p, '{');
		if (!peer_start) break;

		const char *peer_end = json_find_closing(peer_start);
		if (!peer_end) break;

		total++;

		/* Check inbound field */
		char inbound_str[16] = {0};
		/* Extract just this peer's JSON for parsing */
		size_t peer_len = peer_end - peer_start + 1;
		char *peer_json = malloc(peer_len + 1);
		if (peer_json) {
			memcpy(peer_json, peer_start, peer_len);
			peer_json[peer_len] = '\0';

			if (json_get_string(peer_json, "connection_type", inbound_str, sizeof(inbound_str)) > 0) {
				if (strcmp(inbound_str, "inbound") == 0)
					inbound++;
				else
					outbound++;
			} else {
				/* Fallback: check inbound boolean */
				const char *inb = json_find_value(peer_json, "inbound");
				if (inb && strncmp(inb, "true", 4) == 0)
					inbound++;
				else
					outbound++;
			}

			/* Check network type */
			char net_type[32] = {0};
			if (json_get_string(peer_json, "network", net_type, sizeof(net_type)) > 0) {
				if (strcmp(net_type, "ipv4") == 0) ipv4++;
				else if (strcmp(net_type, "ipv6") == 0) ipv6++;
				else if (strcmp(net_type, "onion") == 0) onion++;
				else if (strcmp(net_type, "i2p") == 0) i2p++;
			}

			free(peer_json);
		}

		p = peer_end + 1;
	}

	printf("Peer connections:\n");
	printf("  Total:    %d\n", total);
	printf("  Inbound:  %d\n", inbound);
	printf("  Outbound: %d\n", outbound);
	printf("\nBy network:\n");
	if (ipv4 > 0) printf("  IPv4:     %d\n", ipv4);
	if (ipv6 > 0) printf("  IPv6:     %d\n", ipv6);
	if (onion > 0) printf("  Onion:    %d\n", onion);
	if (i2p > 0) printf("  I2P:      %d\n", i2p);

	free(peers);
	return 0;
}

/* Connect with retry for -rpcwait */
static int rpc_connect_wait(RpcClient *rpc, int timeout_secs)
{
	time_t start = time(NULL);
	int attempt = 0;

	while (1) {
		attempt++;
		if (rpc_connect(rpc) == 0) {
			return 0;  /* Success */
		}

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
			printf("\n");
			method_list_all();
		}
		return 0;
	}

	/* Check for special info commands that don't need a command argument */
	int need_command = 1;
	if (cfg.getinfo || cfg.netinfo) {
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

	/* Set wallet if specified */
	if (cfg.wallet[0])
		rpc_set_wallet(&rpc, cfg.wallet);

	/* Read password from stdin if requested */
	if (cfg.stdinrpcpass) {
		fprintf(stderr, "RPC password: ");
		read_password_stdin(cfg.password, sizeof(cfg.password));
		fprintf(stderr, "\n");
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
			fprintf(stderr, "error: Could not connect to %s:%d\n", cfg.host, cfg.port);
			fprintf(stderr, "Is bitcoind running?\n");
			return 1;
		}
	}

	/* Handle special info commands */
	if (cfg.getinfo) {
		ret = handle_getinfo(&rpc);
		rpc_disconnect(&rpc);
		return ret;
	}
	if (cfg.netinfo) {
		ret = handle_netinfo(&rpc);
		rpc_disconnect(&rpc);
		return ret;
	}

	/* Set named parameter mode if requested */
	if (cfg.named)
		method_set_named_mode(1);

	/* Build and execute command */
	int cmd_argc = argc - cfg.cmd_index - 1;
	char **cmd_argv = &argv[cfg.cmd_index + 1];

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

				/* Parse stdin into arguments (whitespace separated) */
				stdin_args = malloc(sizeof(char *) * 64);
				if (stdin_args) {
					char *tok = strtok(stdin_buf, " \t\n\r");
					while (tok && stdin_count < 64) {
						stdin_args[stdin_count++] = tok;
						tok = strtok(NULL, " \t\n\r");
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

	/* Output result */
	if (result) {
		/* Check if result looks like JSON */
		const char *p = result;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;

		if (*p == '{' || *p == '[') {
			/* Pretty print JSON */
			print_json_pretty(result, 0);
		} else {
			/* Plain output */
			printf("%s\n", result);
		}
		free(result);
	}

	/* Cleanup */
	rpc_disconnect(&rpc);

	return ret;
}
