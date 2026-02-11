/* Interactive REPL (Read-Eval-Print Loop) */

#ifndef REPL_H
#define REPL_H

#include "rpc.h"

/* Run interactive REPL. Returns 0 on clean exit. */
int repl_run(RpcClient *rpc);

#endif
