# btc-cli

Drop-in replacement for `bitcoin-cli`, written in pure C with zero dependencies.

## What is this

A lightweight, standalone reimplementation of Bitcoin Core's `bitcoin-cli`. Same RPC interface, same flags, same output — but compiles to a single ~200KB binary with no dependencies beyond libc.

Useful when you want to talk to a Bitcoin Core node without pulling in the full Core build, or when you want faster cookie-auth workflows.

## Features

- **Full RPC coverage**: 169 methods covering blockchain, wallet, mining, network, mempool, raw transactions, PSBTs, and utility commands
- **All the CLI flags you'd expect**: network selection (`-regtest`, `-testnet`, `-signet`), auth (`-rpcuser`, `-rpccookiefile`), wallets (`-rpcwallet`), named params (`-named`), stdin input, and more
- **Convenience commands**: `-getinfo`, `-netinfo`, `-addrinfo`, `-generate` — all matching bitcoin-cli's behavior
- **Fast cookie auth**: reads the `.cookie` file once and holds it, rather than re-reading on every call
- **Parity-tested**: 215 automated tests compare btc-cli output byte-for-byte against bitcoin-cli

## btc-cli extensions

Things btc-cli does that bitcoin-cli doesn't.

**Transaction verification** — after broadcasting a transaction, confirm it actually propagated:

```
./btc-cli -verify sendrawtransaction <hex>
./btc-cli -verify -verify-peers=5 sendrawtransaction <hex>
```

Connects to peers via P2P and checks their mempools.

**Fallback broadcasting** — if your node can't broadcast, try external APIs:

```
./btc-cli -fallback-mempool-space sendrawtransaction <hex>
./btc-cli -fallback-all sendrawtransaction <hex>
```

Supports mempool.space, Blockstream, Blockchair, Blockchain.info, BlockCypher, custom Esplora instances, and direct P2P broadcast.

## Build

```
make
```

Requires `gcc` (or any C99 compiler) and standard POSIX headers. Works on Linux and macOS.

## Usage

```sh
./btc-cli getblockchaininfo
./btc-cli -getinfo
./btc-cli -rpcuser=alice -rpcpassword=secret getbalance
./btc-cli -regtest -rpcwallet=test getnewaddress
./btc-cli -named createwallet wallet_name=mywallet descriptors=true
```

## Testing

```
bash parity_scan.sh
```

Runs 215 tests that start a regtest node, exercise every supported RPC method and CLI flag, and diff the output against bitcoin-cli. Covers blockchain queries, wallet operations, transaction building, error handling, network flags, and the convenience commands.

## License

MIT
