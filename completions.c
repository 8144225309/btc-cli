/* Shell completion script generation */

#define _GNU_SOURCE
#include "completions.h"
#include "methods.h"
#include <stdio.h>
#include <string.h>

static void generate_bash(void)
{
	int count, i;
	const char **names = method_list_names(&count);

	printf("# btc-cli bash completion\n");
	printf("# Usage: eval \"$(btc-cli -completions=bash)\"\n\n");
	printf("_btc_cli() {\n");
	printf("    local cur prev commands\n");
	printf("    COMPREPLY=()\n");
	printf("    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n");
	printf("    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n\n");
	printf("    commands=\"");
	for (i = 0; i < count; i++) {
		if (i > 0) printf(" ");
		printf("%s", names[i]);
	}
	printf("\"\n\n");
	printf("    if [[ ${COMP_CWORD} -eq 1 ]]; then\n");
	printf("        if [[ \"$cur\" == -* ]]; then\n");
	printf("            local opts=\"-getinfo -netinfo -addrinfo -generate -named -stdin ");
	printf("-rpcconnect= -rpcport= -rpcuser= -rpcpassword= -rpcwallet= ");
	printf("-regtest -testnet -signet -testnet4 -datadir= -conf= ");
	printf("-field= -format= -sats -empty -human -batch -completions= ");
	printf("-verify -color= -rpcwait -help -version\"\n");
	printf("            COMPREPLY=( $(compgen -W \"$opts\" -- \"$cur\") )\n");
	printf("        else\n");
	printf("            COMPREPLY=( $(compgen -W \"$commands\" -- \"$cur\") )\n");
	printf("        fi\n");
	printf("    fi\n");
	printf("    return 0\n");
	printf("}\n\n");
	printf("complete -F _btc_cli btc-cli\n");
}

static void generate_zsh(void)
{
	int count, i;
	const char **names = method_list_names(&count);

	printf("#compdef btc-cli\n");
	printf("# btc-cli zsh completion\n");
	printf("# Usage: eval \"$(btc-cli -completions=zsh)\"\n\n");
	printf("_btc_cli() {\n");
	printf("    local -a commands\n");
	printf("    commands=(\n");
	for (i = 0; i < count; i++)
		printf("        '%s'\n", names[i]);
	printf("    )\n\n");
	printf("    _arguments \\\n");
	printf("        '-getinfo[Get general node info]' \\\n");
	printf("        '-netinfo[Get network peer info]' \\\n");
	printf("        '-addrinfo[Get address counts]' \\\n");
	printf("        '-generate[Generate blocks]' \\\n");
	printf("        '-named[Use named parameters]' \\\n");
	printf("        '-field=[Extract JSON field]:field path' \\\n");
	printf("        '-format=[Output format]:format:(table csv)' \\\n");
	printf("        '-sats[Show amounts in satoshis]' \\\n");
	printf("        '-empty[Show note for null results]' \\\n");
	printf("        '-human[Human-friendly output]' \\\n");
	printf("        '-batch[Batch mode from stdin]' \\\n");
	printf("        '-rpcconnect=[RPC host]:host' \\\n");
	printf("        '-rpcport=[RPC port]:port' \\\n");
	printf("        '-rpcuser=[RPC user]:user' \\\n");
	printf("        '-rpcpassword=[RPC password]:password' \\\n");
	printf("        '-rpcwallet=[Wallet name]:wallet' \\\n");
	printf("        '-regtest[Use regtest]' \\\n");
	printf("        '-testnet[Use testnet]' \\\n");
	printf("        '-signet[Use signet]' \\\n");
	printf("        '-help[Show help]' \\\n");
	printf("        '-version[Show version]' \\\n");
	printf("        '1:command:($commands)' \\\n");
	printf("        '*:args'\n");
	printf("}\n\n");
	printf("_btc_cli \"$@\"\n");
}

static void generate_fish(void)
{
	int count, i;
	const char **names = method_list_names(&count);

	printf("# btc-cli fish completion\n");
	printf("# Usage: btc-cli -completions=fish | source\n\n");

	/* Subcommands */
	printf("set -l commands ");
	for (i = 0; i < count; i++) {
		if (i > 0) printf(" ");
		printf("%s", names[i]);
	}
	printf("\n\n");

	printf("complete -c btc-cli -f\n");
	printf("complete -c btc-cli -n '__fish_use_subcommand' -a \"$commands\"\n\n");

	/* Options */
	printf("complete -c btc-cli -l getinfo -d 'Get general node info'\n");
	printf("complete -c btc-cli -l netinfo -d 'Get network peer info'\n");
	printf("complete -c btc-cli -l addrinfo -d 'Get address counts'\n");
	printf("complete -c btc-cli -l generate -d 'Generate blocks'\n");
	printf("complete -c btc-cli -l named -d 'Use named parameters'\n");
	printf("complete -c btc-cli -l sats -d 'Show amounts in satoshis'\n");
	printf("complete -c btc-cli -l empty -d 'Show note for null results'\n");
	printf("complete -c btc-cli -l human -d 'Human-friendly output'\n");
	printf("complete -c btc-cli -l batch -d 'Batch mode from stdin'\n");
	printf("complete -c btc-cli -l regtest -d 'Use regtest'\n");
	printf("complete -c btc-cli -l testnet -d 'Use testnet'\n");
	printf("complete -c btc-cli -l signet -d 'Use signet'\n");
	printf("complete -c btc-cli -l help -d 'Show help'\n");
	printf("complete -c btc-cli -l version -d 'Show version'\n");
	printf("complete -c btc-cli -l rpcconnect -d 'RPC host' -r\n");
	printf("complete -c btc-cli -l rpcport -d 'RPC port' -r\n");
	printf("complete -c btc-cli -l rpcuser -d 'RPC user' -r\n");
	printf("complete -c btc-cli -l rpcpassword -d 'RPC password' -r\n");
	printf("complete -c btc-cli -l rpcwallet -d 'Wallet name' -r\n");
	printf("complete -c btc-cli -l field -d 'Extract JSON field' -r\n");
	printf("complete -c btc-cli -l format -d 'Output format' -ra 'table csv'\n");
}

void completions_generate(const char *shell)
{
	if (strcmp(shell, "bash") == 0)
		generate_bash();
	else if (strcmp(shell, "zsh") == 0)
		generate_zsh();
	else if (strcmp(shell, "fish") == 0)
		generate_fish();
	else
		fprintf(stderr, "error: Unknown shell: %s (supported: bash, zsh, fish)\n", shell);
}
