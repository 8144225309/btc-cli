/* Interactive REPL (Read-Eval-Print Loop) with line editing */

#define _GNU_SOURCE
#include "repl.h"
#include "methods.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

#define REPL_LINE_MAX 4096
#define REPL_HISTORY_MAX 100

/* Pretty print JSON with indentation (duplicated from btc-cli.c to avoid dependency) */
static void repl_print_json(const char *json)
{
	const char *p = json;
	int in_string = 0;
	int level = 0;
	int i;

	while (*p) {
		if (*p == '"' && (p == json || *(p-1) != '\\')) {
			fputc(*p, stdout);
			in_string = !in_string;
		} else if (in_string) {
			fputc(*p, stdout);
		} else if (*p == '{' || *p == '[') {
			const char *peek = p + 1;
			while (*peek == ' ' || *peek == '\t' || *peek == '\n' || *peek == '\r')
				peek++;
			char closing = (*p == '{') ? '}' : ']';
			if (*peek == closing) {
				fputc(*p, stdout);
				fputc(closing, stdout);
				p = peek;
			} else {
				fputc(*p, stdout);
				fputc('\n', stdout);
				level++;
				for (i = 0; i < level * 2; i++) fputc(' ', stdout);
			}
		} else if (*p == '}' || *p == ']') {
			fputc('\n', stdout);
			level--;
			for (i = 0; i < level * 2; i++) fputc(' ', stdout);
			fputc(*p, stdout);
		} else if (*p == ',') {
			fputc(*p, stdout);
			fputc('\n', stdout);
			for (i = 0; i < level * 2; i++) fputc(' ', stdout);
		} else if (*p == ':') {
			fputc(*p, stdout);
			fputc(' ', stdout);
		} else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			/* Skip whitespace */
		} else {
			fputc(*p, stdout);
		}
		p++;
	}
	fputc('\n', stdout);
}

/* Tab completion: find methods matching prefix */
static int find_completions(const char *prefix, const char **matches, int max_matches)
{
	int count;
	const char **names = method_list_names(&count);
	int n = 0;
	int i;
	size_t plen = strlen(prefix);

	for (i = 0; i < count && n < max_matches; i++) {
		if (strncmp(names[i], prefix, plen) == 0)
			matches[n++] = names[i];
	}
	return n;
}

#ifndef _WIN32

/* Raw terminal mode REPL with line editing */
static int repl_raw(RpcClient *rpc)
{
	struct termios orig, raw;
	char history[REPL_HISTORY_MAX][REPL_LINE_MAX];
	int hist_count = 0;
	int hist_pos = -1;
	char line[REPL_LINE_MAX];
	int pos, len;

	if (tcgetattr(STDIN_FILENO, &orig) < 0) return -1;
	raw = orig;
	raw.c_lflag &= ~(ICANON | ECHO | ISIG);
	raw.c_iflag &= ~(IXON | ICRNL);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) return -1;

	printf("btc-cli shell (type 'exit' or Ctrl-D to quit)\r\n");

	while (1) {
		/* Print prompt */
		printf("btc> ");
		fflush(stdout);

		pos = 0;
		len = 0;
		hist_pos = hist_count;
		memset(line, 0, sizeof(line));

		while (1) {
			char c;
			if (read(STDIN_FILENO, &c, 1) != 1) {
				/* EOF (Ctrl-D) */
				printf("\r\n");
				goto done;
			}

			if (c == 4) {  /* Ctrl-D */
				if (len == 0) {
					printf("\r\n");
					goto done;
				}
				continue;
			}

			if (c == 3) {  /* Ctrl-C */
				printf("^C\r\n");
				pos = 0; len = 0; line[0] = '\0';
				break;
			}

			if (c == '\r' || c == '\n') {
				printf("\r\n");
				break;
			}

			if (c == 127 || c == 8) {  /* Backspace */
				if (pos > 0) {
					memmove(line + pos - 1, line + pos, len - pos);
					pos--;
					len--;
					line[len] = '\0';
					/* Redraw from cursor */
					printf("\033[D");  /* Move left */
					printf("%s ", line + pos);  /* Rewrite + clear last char */
					/* Move cursor back */
					int back = len - pos + 1;
					if (back > 0) printf("\033[%dD", back);
				}
				fflush(stdout);
				continue;
			}

			if (c == 9) {  /* Tab — completion */
				/* Find word start */
				int wstart = pos;
				while (wstart > 0 && line[wstart-1] != ' ')
					wstart--;
				char prefix[256];
				int plen = pos - wstart;
				if (plen > 0 && plen < (int)sizeof(prefix)) {
					const char *matches[32];
					int nmatch;
					memcpy(prefix, line + wstart, plen);
					prefix[plen] = '\0';
					nmatch = find_completions(prefix, matches, 32);
					if (nmatch == 1) {
						/* Complete the word */
						const char *completion = matches[0] + plen;
						size_t clen = strlen(completion);
						if (len + (int)clen < REPL_LINE_MAX) {
							memmove(line + pos + clen, line + pos, len - pos);
							memcpy(line + pos, completion, clen);
							len += clen;
							pos += clen;
							line[len] = '\0';
							printf("%s", completion);
							/* Add space after */
							if (len < REPL_LINE_MAX - 1) {
								memmove(line + pos + 1, line + pos, len - pos);
								line[pos] = ' ';
								len++;
								pos++;
								line[len] = '\0';
								printf(" ");
							}
						}
					} else if (nmatch > 1) {
						/* Show options */
						int mi;
						printf("\r\n");
						for (mi = 0; mi < nmatch; mi++)
							printf("  %s\r\n", matches[mi]);
						printf("btc> %s", line);
						/* Move cursor to position */
						if (pos < len) printf("\033[%dD", len - pos);
					}
				}
				fflush(stdout);
				continue;
			}

			if (c == 27) {  /* Escape sequence */
				char seq[3];
				if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
				if (seq[0] != '[') continue;
				if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

				if (seq[1] == 'A') {  /* Up arrow — history */
					if (hist_pos > 0) {
						hist_pos--;
						/* Clear line */
						printf("\r\033[K");
						printf("btc> ");
						strncpy(line, history[hist_pos], REPL_LINE_MAX - 1);
						len = strlen(line);
						pos = len;
						printf("%s", line);
					}
				} else if (seq[1] == 'B') {  /* Down arrow */
					if (hist_pos < hist_count - 1) {
						hist_pos++;
						printf("\r\033[K");
						printf("btc> ");
						strncpy(line, history[hist_pos], REPL_LINE_MAX - 1);
						len = strlen(line);
						pos = len;
						printf("%s", line);
					} else if (hist_pos == hist_count - 1) {
						hist_pos = hist_count;
						printf("\r\033[K");
						printf("btc> ");
						line[0] = '\0';
						len = 0;
						pos = 0;
					}
				} else if (seq[1] == 'C') {  /* Right arrow */
					if (pos < len) {
						pos++;
						printf("\033[C");
					}
				} else if (seq[1] == 'D') {  /* Left arrow */
					if (pos > 0) {
						pos--;
						printf("\033[D");
					}
				} else if (seq[1] == 'H') {  /* Home */
					if (pos > 0) {
						printf("\033[%dD", pos);
						pos = 0;
					}
				} else if (seq[1] == 'F') {  /* End */
					if (pos < len) {
						printf("\033[%dC", len - pos);
						pos = len;
					}
				}
				fflush(stdout);
				continue;
			}

			/* Ctrl-A (home) */
			if (c == 1) {
				if (pos > 0) { printf("\033[%dD", pos); pos = 0; }
				fflush(stdout);
				continue;
			}
			/* Ctrl-E (end) */
			if (c == 5) {
				if (pos < len) { printf("\033[%dC", len - pos); pos = len; }
				fflush(stdout);
				continue;
			}
			/* Ctrl-U (clear line) */
			if (c == 21) {
				printf("\r\033[K");
				printf("btc> ");
				line[0] = '\0';
				pos = 0; len = 0;
				fflush(stdout);
				continue;
			}
			/* Ctrl-K (kill to end) */
			if (c == 11) {
				printf("\033[K");
				line[pos] = '\0';
				len = pos;
				fflush(stdout);
				continue;
			}
			/* Ctrl-W (delete word) */
			if (c == 23) {
				int wstart = pos;
				while (wstart > 0 && line[wstart-1] == ' ') wstart--;
				while (wstart > 0 && line[wstart-1] != ' ') wstart--;
				int deleted = pos - wstart;
				if (deleted > 0) {
					memmove(line + wstart, line + pos, len - pos);
					len -= deleted;
					pos = wstart;
					line[len] = '\0';
					printf("\r\033[K");
					printf("btc> %s", line);
					if (pos < len) printf("\033[%dD", len - pos);
				}
				fflush(stdout);
				continue;
			}

			/* Regular character */
			if (c >= 32 && len < REPL_LINE_MAX - 1) {
				memmove(line + pos + 1, line + pos, len - pos);
				line[pos] = c;
				len++;
				pos++;
				line[len] = '\0';
				/* Print from cursor position */
				printf("%s", line + pos - 1);
				if (pos < len) printf("\033[%dD", len - pos);
				fflush(stdout);
			}
		}

		/* Process the line */
		line[len] = '\0';

		/* Skip empty lines */
		{
			char *trimmed = line;
			while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
			if (*trimmed == '\0') continue;

			/* Exit commands */
			if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0)
				break;

			/* Add to history */
			if (hist_count < REPL_HISTORY_MAX) {
				strncpy(history[hist_count], line, REPL_LINE_MAX - 1);
				hist_count++;
			} else {
				memmove(history[0], history[1],
				        sizeof(history[0]) * (REPL_HISTORY_MAX - 1));
				strncpy(history[REPL_HISTORY_MAX - 1], line, REPL_LINE_MAX - 1);
			}

			/* Parse: method arg1 arg2 ... */
			char *save_ptr = NULL;
			char *tok = strtok_r(trimmed, " \t", &save_ptr);
			if (!tok) continue;

			char method_name[256];
			strncpy(method_name, tok, sizeof(method_name) - 1);
			method_name[sizeof(method_name) - 1] = '\0';

			/* Built-in help */
			if (strcmp(method_name, "help") == 0) {
				tok = strtok_r(NULL, " \t", &save_ptr);
				if (tok) {
					const MethodDef *m = method_find(tok);
					if (m) method_print_help(m);
					else printf("Unknown command: %s\n", tok);
				} else {
					printf("Type a command (e.g., getblockcount, getblockchaininfo)\n");
					printf("Type 'help <command>' for details\n");
					printf("Type 'exit' or Ctrl-D to quit\n");
				}
				continue;
			}

			/* Collect args */
			char *args[64];
			int nargs = 0;
			while ((tok = strtok_r(NULL, " \t", &save_ptr)) != NULL && nargs < 64)
				args[nargs++] = tok;

			/* Execute command */
			const MethodDef *m = method_find(method_name);
			char *result = NULL;
			int error_code;

			if (m) {
				/* Known method */
				int rc = m->handler(rpc, nargs, args, &result);
				(void)rc;
			} else {
				/* Unknown method — build raw params and forward */
				char *params_buf = NULL;
				size_t pb_size = 256;
				size_t pb_pos = 0;
				int ai;
				params_buf = malloc(pb_size);
				if (params_buf) {
					params_buf[pb_pos++] = '[';
					for (ai = 0; ai < nargs; ai++) {
						size_t alen = strlen(args[ai]);
						while (pb_pos + alen + 64 > pb_size) {
							pb_size *= 2;
							params_buf = realloc(params_buf, pb_size);
							if (!params_buf) break;
						}
						if (!params_buf) break;
						if (ai > 0) params_buf[pb_pos++] = ',';
						/* Type inference */
						if (strcmp(args[ai], "true") == 0 || strcmp(args[ai], "false") == 0 ||
						    strcmp(args[ai], "null") == 0) {
							pb_pos += snprintf(params_buf + pb_pos, pb_size - pb_pos, "%s", args[ai]);
						} else if (args[ai][0] == '[' || args[ai][0] == '{') {
							pb_pos += snprintf(params_buf + pb_pos, pb_size - pb_pos, "%s", args[ai]);
						} else {
							const char *s = args[ai];
							int is_num = 1;
							if (*s == '-') s++;
							if (!*s) is_num = 0;
							while (*s) { if (!isdigit(*s) && *s != '.') { is_num = 0; break; } s++; }
							if (is_num)
								pb_pos += snprintf(params_buf + pb_pos, pb_size - pb_pos, "%s", args[ai]);
							else
								pb_pos += snprintf(params_buf + pb_pos, pb_size - pb_pos, "\"%s\"", args[ai]);
						}
					}
					if (params_buf) {
						params_buf[pb_pos++] = ']';
						params_buf[pb_pos] = '\0';
					}
				}

				char *response = rpc_call(rpc, method_name, params_buf ? params_buf : "[]");
				free(params_buf);

				if (response) {
					result = method_extract_result(response, &error_code);
					free(response);
				} else {
					printf("error: RPC call failed\n");
				}
			}

			/* Print result */
			if (result) {
				const char *rp = result;
				while (*rp == ' ' || *rp == '\t' || *rp == '\n') rp++;
				if (*rp == '{' || *rp == '[')
					repl_print_json(result);
				else
					printf("%s\n", result);
				free(result);
			}
		}
	}

done:
	tcsetattr(STDIN_FILENO, TCSANOW, &orig);
	return 0;
}

#endif /* !_WIN32 */

int repl_run(RpcClient *rpc)
{
#ifdef _WIN32
	/* Simple fallback for Windows */
	char line[REPL_LINE_MAX];
	printf("btc-cli shell (type 'exit' to quit)\n");
	while (1) {
		printf("btc> ");
		fflush(stdout);
		if (!fgets(line, sizeof(line), stdin)) break;
		char *nl = strchr(line, '\n');
		if (nl) *nl = '\0';
		char *trimmed = line;
		while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
		if (*trimmed == '\0') continue;
		if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) break;
		/* Would need full command parsing here — minimal for now */
		printf("(Windows REPL not fully implemented)\n");
	}
	return 0;
#else
	if (!isatty(STDIN_FILENO)) {
		fprintf(stderr, "error: shell requires an interactive terminal\n");
		return 1;
	}
	return repl_raw(rpc);
#endif
}
