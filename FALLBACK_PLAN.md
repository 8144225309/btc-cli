# Robust sendrawtransaction Fallback Architecture

## Three Tiers of Broadcast Redundancy

```
Tier 1: Local RPC node      (your infrastructure, most reliable)
Tier 2: Public HTTP APIs     (someone else's node, easy to add)
Tier 3: Raw P2P broadcast    (no intermediary at all, nuclear option)
```

---

## Tier 2: Public APIs — 5 Supported

| API | Endpoint | Body format | Response | TLS Status |
|-----|----------|-------------|----------|------------|
| **mempool.space** | `POST /api/tx` | Raw hex (plain text) | txid string | Stubbed |
| **blockstream.info** | `POST /api/tx` | Raw hex (plain text) | txid string | Stubbed |
| **blockchair** | `POST /bitcoin/push/transaction` | `data=<hex>` (form) | JSON with `transaction_hash` | Stubbed |
| **blockchain.info** | `POST /pushtx` | `tx=<hex>` (form) | "Transaction Submitted" | Stubbed |
| **blockcypher** | `POST /v1/btc/main/txs/push` | `{"tx":"<hex>"}` (JSON) | JSON with `hash` field | Stubbed |

All 5 require HTTPS. TLS is stubbed — will be wired up with BearSSL or similar.

Additionally, `-fallback-esplora=URL` supports any Esplora-compatible endpoint. Plain HTTP works now (no TLS needed for self-hosted instances on LAN).

### Esplora API (mempool.space + blockstream.info)

Both run **Esplora** (Blockstream's open-source block explorer backend). Same API:
- **Mainnet**: `POST /api/tx`
- **Testnet**: `POST /testnet/api/tx`
- **Signet**: `POST /signet/api/tx`
- Body: raw hex string, Content-Type: text/plain
- Response: txid as plain text (64 chars)
- No auth required
- Self-hostable (Umbrel, Start9, RaspiBlitz, etc.)

### Blockchair

- **Mainnet**: `POST https://api.blockchair.com/bitcoin/push/transaction`
- **Testnet**: `POST https://api.blockchair.com/bitcoin/testnet/push/transaction`
- Body: `data=<hex>`, Content-Type: application/x-www-form-urlencoded
- Response: JSON with `data.transaction_hash` on success
- No auth for basic usage

### blockchain.info

- **Endpoint**: `POST https://blockchain.info/pushtx`
- Body: `tx=<hex>`, Content-Type: application/x-www-form-urlencoded
- Response: "Transaction Submitted" on success
- No auth required

### BlockCypher

- **Mainnet**: `POST https://api.blockcypher.com/v1/btc/main/txs/push`
- **Testnet**: `POST https://api.blockcypher.com/v1/btc/test3/txs/push`
- Body: `{"tx":"<hex>"}`, Content-Type: application/json
- Response: JSON with `hash` field on success
- Free tier, no API key needed

---

## Tier 3: P2P Direct Broadcast

```
DNS seed -> connect -> handshake -> send "tx" message -> done
```

The Bitcoin P2P `tx` message payload is just the raw serialized transaction bytes (hex decoded to binary). Broadcasting to 10-20 random peers from DNS seeds is essentially what Bitcoin Core does.

### Characteristics:
- Zero dependency on any API or service
- Fire-and-forget: no way to know if peer accepted (BIP 61 reject removed since Core v0.20)
- Connection survives invalid tx (0 misbehavior for deserialization failure)
- Most sovereign option — speaking the Bitcoin protocol directly
- 100ms sleep after send before disconnect (prevents TCP RST race)

### Performance:
- DNS seeds return 25-50+ IPs each, ~7 seeds = hundreds of IPs
- Each peer: connect (~1s) + handshake (~1s) + send tx (<1ms) + 100ms flush = ~2s
- 10 peers sequentially: ~20s. Failures skip fast.

---

## CLI Flags

```bash
# Individual API fallbacks (all require TLS — stubbed for now)
btc-cli sendrawtransaction <hex> -fallback-mempool-space
btc-cli sendrawtransaction <hex> -fallback-blockstream
btc-cli sendrawtransaction <hex> -fallback-blockchair
btc-cli sendrawtransaction <hex> -fallback-blockchain-info
btc-cli sendrawtransaction <hex> -fallback-blockcypher

# Custom Esplora instance (plain HTTP works now)
btc-cli sendrawtransaction <hex> -fallback-esplora=http://my-node.local/api/tx

# P2P direct broadcast to N peers
btc-cli sendrawtransaction <hex> -fallback-p2p=10

# Shorthand: all 5 APIs + P2P (10 peers)
btc-cli sendrawtransaction <hex> -fallback-all
```

All fallbacks are **off by default**. With no flags, btc-cli behaves identically to bitcoin-cli.

---

## Execution Flow

```
1. Local RPC (always first — fastest, most private)
   - 3 retries with exponential backoff (1s, 2s, 4s)
   - Reconnects dead sockets between retries
   - Error -27 "already in mempool" treated as success
   |
   v
2. Fallback broadcast (all configured methods fire, sequential)
   - HTTPS APIs: mempool.space, blockstream, blockchair, blockchain.info, blockcypher
   - Custom esplora (plain HTTP or HTTPS)
   - P2P broadcast to N peers (runs last — slowest but most resilient)
   |
   v
3. P2P verification (opt-in with -verify)
   - Connect to random DNS seed peers
   - Send "mempool" request, scan "inv" for txid
   - Reports N/M peers confirmed
```

If RPC fails but any fallback succeeds: exit 0, output "broadcast via fallback".
If RPC fails and all fallbacks fail: exit 1, output error.

### No-node mode

If bitcoind is unreachable but fallbacks are configured, btc-cli warns and continues:
```
warning: Could not connect to 127.0.0.1:8332 — using fallbacks
```
The handler runs, RPC fails immediately, fallbacks take over.

---

## Source Files

| File | Lines | Purpose |
|------|-------|---------|
| `sendtx.c/h` | ~210 | RPC send with retry, backoff, reconnect, error classification |
| `p2p.c/h` | ~670 | Pure C Bitcoin P2P: SHA-256, DNS seeds, TCP connect, handshake, tx/mempool/inv |
| `verify.c/h` | ~150 | P2P peer verification orchestration |
| `fallback.c/h` | ~700 | URL parser, HTTP POST client, TLS stub, 5 API handlers, P2P broadcast |
| `config.c/h` | modified | FallbackConfig struct, 10 new CLI flags |
| `methods.c/h` | modified | Custom sendrawtransaction handler (3-layer flow) |
| `btc-cli.c` | modified | No-node mode, fallback config wiring |
| `rpc.c` | modified | HTTP 500 body parsing (fixes error reporting for all commands) |

---

## TLS Status

All 5 HTTPS APIs are stubbed with:
```c
static int https_post(...) {
    /* TLS not wired up yet — will be added with BearSSL or similar */
    return -1;
}
```

When TLS is wired up, replace `https_post()` in `fallback.c` with a real implementation. The API handlers, URL parsing, response parsing, and error detection are all implemented and tested against documented API specs.
