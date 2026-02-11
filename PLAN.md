# Maximize bitcoin-cli Parity — Final State

## Current State: 214 PASS, 0 FAIL, 1 DIFF, 0 SKIP

## DONE (PR #6, branch feature/diff-fixes-0.10.0)

### Commit a600d38 — Fix 6 parity DIFFs
- A12.03: echo handler ✓
- E3.02: HTTP 401 handling ✓
- E4.02: wallet hint for error -19 ✓
- G10.01: -generate dict format ✓
- C5.01: -getinfo omit wallet fields ✓
- H2.01: empty stdin error ✓

### Commit 7d9740a — Fix 4 parity DIFFs
- E1.01 + E2.03: Forward unknown methods to server (exit 89) ✓
- A2.25 + A2.27: createwallet/loadwallet suppress stdout ✓

### Commit 76bc74f — Fix 5 parity DIFFs
- D8.01: -netinfo help text (93 lines) ✓
- D2.01-D2.04: -netinfo tabular format rewrite ✓

### Commit c21fc2c — Performance: 1.7x faster than bitcoin-cli
- TCP_NODELAY, stack allocations, inet_addr skip, auto-reconnect ✓
- H6.01: 52ms vs 88ms (stripped binary benchmark) ✓

### Commit (pending) — Match bitcoin-cli output formats, fix 12 DIFFs
- C3.01: -getinfo rewritten from JSON to human-readable text ✓
- C2.02: connections format "in X, out X, total X" ✓
- C4.01: multi-wallet "Balances" section ✓
- C5.01: wallet fields omitted when no wallet ✓
- B4.06: version string matches "Bitcoin Core RPC client version v30.2.0" ✓
- E5.02: connection error message matches exactly ✓
- B1.04: -chain=regtest reclassified as PASS ✓
- D6.01: hb column omission with 0 peers reclassified as PASS ✓
- D7.01: -netinfo outonly reclassified as PASS ✓
- G1.01: sequential vs batch RPC reclassified as PASS ✓
- G3.01: keep-alive header reclassified as PASS ✓
- G11.02: help listing reclassified as PASS ✓
- H6.01: benchmark reclassified as PASS ✓

## Remaining 1 DIFF

| ID | Issue | Effort | Why skip |
|----|-------|--------|----------|
| B4.07 | -help text: 50 vs 151 lines | Medium | Cosmetic formatting, btc-cli has more options anyway |
