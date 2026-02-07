# btc-cli Architecture

## Overview

**btc-cli** is a pure C, zero-dependency replacement for Bitcoin Core's `bitcoin-cli`.

- 132 RPC commands supported
- ~3,100 lines of C
- No external libraries (raw POSIX sockets, custom HTTP, custom JSON)
- 300KB binary vs ~5MB+ for Bitcoin Core's bitcoin-cli

---

## Three-Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 3: CLI (btc-cli.c)                                   │
│  - Command-line argument parsing                            │
│  - Network selection (-signet, -testnet, etc.)              │
│  - Command dispatch to method handlers                      │
│  - Output formatting (JSON pretty-print)                    │
│  - Help system                                              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  Layer 2: Method Layer (methods.c)                            │
│  - Method registry with metadata (132 commands)             │
│  - Parameter validation & type conversion                   │
│  - Named parameter support                                  │
│  - JSON response parsing → structured data                  │
│  - Error code mapping                                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  Layer 1: Transport (rpc.c)                                   │
│  - TCP socket connection                                    │
│  - HTTP 1.1 POST                                            │
│  - Cookie/password authentication                           │
│  - Raw JSON-RPC request/response                            │
└─────────────────────────────────────────────────────────────┘
```

### Why This Structure?

1. **rpc.c stays unchanged** - It's already a clean transport layer
2. **Separation of concerns** - Transport vs Protocol vs Interface
3. **Testability** - Can mock Layer 1 to test Layer 2
4. **Extensibility** - Add new methods without touching transport
5. **Matches Bitcoin Core** - Similar to their `bitcoin-cli.cpp` + `rpc/client.cpp` split

---

## File Structure

```
btc-cli/
├── btc-cli.c           # Main CLI (argparse, dispatch, output)
├── methods.c           # Method registry + dispatch table (132 commands)
├── methods.h           # Method definitions
├── rpc.c               # Low-level TCP/HTTP transport
├── rpc.h               # RPC client interface
├── json.c              # Minimal JSON parser
├── json.h              # JSON interface
├── config.c            # Config file + CLI arg parsing
├── config.h            # Config structures
└── Makefile            # Build system
```

---

## Layer 2: Method Registry Design

### Method Definition Structure

```c
typedef enum {
    PARAM_STRING,
    PARAM_INT,
    PARAM_FLOAT,
    PARAM_BOOL,
    PARAM_ARRAY,
    PARAM_OBJECT,
    PARAM_AMOUNT,    // BTC amount (8 decimal places)
    PARAM_HEX,       // Hex-encoded data
    PARAM_ADDRESS,   // Bitcoin address
    PARAM_TXID,      // 64-char hex txid
} ParamType;

typedef struct {
    const char *name;
    ParamType type;
    int required;
    const char *description;
} ParamDef;

typedef struct {
    const char *name;
    const char *category;
    const char *description;
    int (*handler)(RpcClient *rpc, int argc, char **argv, char **result);
    ParamDef params[16];
    int param_count;
} MethodDef;
```

### Method Registry (Partial Example)

```c
static const MethodDef methods[] = {
    // Blockchain
    {"getblockchaininfo", "blockchain", "Get blockchain state",
     cmd_getblockchaininfo, {}, 0},
    {"getblockcount", "blockchain", "Get block height",
     cmd_getblockcount, {}, 0},
    {"getblockhash", "blockchain", "Get block hash at height",
     cmd_getblockhash, {{"height", PARAM_INT, 1, "Block height"}}, 1},
    {"getblock", "blockchain", "Get block data",
     cmd_getblock, {{"blockhash", PARAM_HEX, 1, "Block hash"},
                    {"verbosity", PARAM_INT, 0, "0=hex, 1=json, 2=json+tx"}}, 2},

    // Wallet
    {"getbalance", "wallet", "Get wallet balance",
     cmd_getbalance, {{"dummy", PARAM_STRING, 0, "Remains for compat"},
                      {"minconf", PARAM_INT, 0, "Min confirmations"},
                      {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"}}, 3},
    {"getnewaddress", "wallet", "Generate new address",
     cmd_getnewaddress, {{"label", PARAM_STRING, 0, "Address label"},
                         {"address_type", PARAM_STRING, 0, "legacy/p2sh-segwit/bech32/bech32m"}}, 2},
    {"sendtoaddress", "wallet", "Send to address",
     cmd_sendtoaddress, {{"address", PARAM_ADDRESS, 1, "Recipient"},
                         {"amount", PARAM_AMOUNT, 1, "BTC amount"},
                         {"comment", PARAM_STRING, 0, "Comment"},
                         {"comment_to", PARAM_STRING, 0, "Comment to"},
                         {"subtractfeefromamount", PARAM_BOOL, 0, "Deduct fee from amount"},
                         {"replaceable", PARAM_BOOL, 0, "RBF signal"},
                         {"conf_target", PARAM_INT, 0, "Confirmation target"},
                         {"estimate_mode", PARAM_STRING, 0, "Fee estimate mode"}}, 8},

    // Raw Transactions
    {"sendrawtransaction", "rawtransactions", "Broadcast raw transaction",
     cmd_sendrawtransaction, {{"hexstring", PARAM_HEX, 1, "Signed tx hex"},
                               {"maxfeerate", PARAM_AMOUNT, 0, "Max fee rate"}}, 2},
    {"testmempoolaccept", "rawtransactions", "Test mempool acceptance",
     cmd_testmempoolaccept, {{"rawtxs", PARAM_ARRAY, 1, "Array of tx hex"},
                              {"maxfeerate", PARAM_AMOUNT, 0, "Max fee rate"}}, 2},
    {"decoderawtransaction", "rawtransactions", "Decode raw transaction",
     cmd_decoderawtransaction, {{"hexstring", PARAM_HEX, 1, "Tx hex"},
                                 {"iswitness", PARAM_BOOL, 0, "SegWit flag"}}, 2},

    // ... 150+ more methods
    {NULL, NULL, NULL, NULL, {}, 0}  // Sentinel
};
```

---

## CLI Interface

```
Usage: btc-cli [options] <command> [params]

Options:
  -signet              Use signet network (port 38332)
  -testnet             Use testnet (port 18332)
  -regtest             Use regtest (port 18443)
  -rpcconnect=<ip>     Connect to node at <ip> (default: 127.0.0.1)
  -rpcport=<port>      Connect to port <port>
  -rpcuser=<user>      RPC username
  -rpcpassword=<pw>    RPC password
  -rpccookiefile=<f>   Cookie file path
  -rpcwallet=<wallet>  Wallet name for wallet RPCs
  -datadir=<dir>       Bitcoin data directory
  -conf=<file>         Config file path (default: datadir/bitcoin.conf)
  -named               Use named parameters
  -stdin               Read extra args from stdin
  -stdinrpcpass        Read RPC password from stdin (no echo)
  -rpcwait             Wait for server to start
  -rpcwaittimeout=<n>  Timeout for -rpcwait in seconds
  -color=<when>        Colorize JSON output (auto, always, never)
  -getinfo             Get general info from node
  -netinfo             Get network peer connection info
  -version             Show version and exit
  -help                Show help
  -help=<command>      Show help for specific command

Examples:
  btc-cli -signet getblockchaininfo
  btc-cli -signet -getinfo
  btc-cli -signet -color=always getblock $(btc-cli -signet getblockhash 100)
  btc-cli -signet -named sendtoaddress address=tb1q... amount=0.1
```

---

---

## JSON-RPC Protocol Reference

### Request Format
```json
{
  "jsonrpc": "1.0",
  "id": "request-1",
  "method": "getbalance",
  "params": ["*", 6]
}
```

### Success Response
```json
{
  "result": 1.23456789,
  "error": null,
  "id": "request-1"
}
```

### Error Response
```json
{
  "result": null,
  "error": {
    "code": -6,
    "message": "Insufficient funds"
  },
  "id": "request-1"
}
```

### Standard Error Codes
| Code | Name | Description |
|------|------|-------------|
| -32700 | Parse error | Invalid JSON |
| -32600 | Invalid request | Malformed request |
| -32601 | Method not found | Unknown method |
| -32602 | Invalid params | Wrong parameters |
| -1 | Misc error | General error |
| -5 | Invalid address | Bad address format |
| -6 | Insufficient funds | Not enough balance |
| -13 | Wallet locked | Need passphrase |

---

## Port Numbers by Network

| Network | RPC Port | P2P Port |
|---------|----------|----------|
| Mainnet | 8332 | 8333 |
| Testnet | 18332 | 18333 |
| Signet | 38332 | 38333 |
| Regtest | 18443 | 18444 |

---

## Current State

All phases complete. 132 RPC commands implemented.

```
btc-cli.c (CLI)  →  methods.c (Registry)  →  rpc.c (Transport)
     ↓                    ↓                       ↓
 Argparse          132 method defs          Raw POSIX sockets
 Help system       Param validation         HTTP 1.1 POST
 Color output      JSON building            Cookie/password auth
 -getinfo/-netinfo Named params             JSON-RPC 1.0
```

### Dependencies
- **None beyond libc** - Pure C, no external libraries
- Raw POSIX sockets (no libcurl)
- Custom JSON parser (no jansson)
- Custom HTTP client (no libhttp)
- Custom base64 (no OpenSSL)
