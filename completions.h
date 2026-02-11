/* Shell completion script generation */

#ifndef COMPLETIONS_H
#define COMPLETIONS_H

/* Generate shell completion script.
 * shell: "bash", "zsh", or "fish"
 * Prints script to stdout.
 */
void completions_generate(const char *shell);

#endif
