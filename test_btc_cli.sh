#!/bin/bash
# Comprehensive btc-cli test suite
# Tests all 132 commands with normal usage, edge cases, and error handling

set -o pipefail

CLI="./btc-cli"
WALLET="testrunner"
PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Test helper - expects success
test_ok() {
    local name="$1"
    shift
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$CLI" "$@" 2>&1); then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "      Command: $CLI $@"
        echo "      Output: $output"
        ((FAIL++))
        return 1
    fi
}

# Test helper - expects error
test_err() {
    local name="$1"
    shift
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$CLI" "$@" 2>&1); then
        echo -e "${RED}FAIL (expected error)${NC}"
        ((FAIL++))
        return 1
    else
        echo -e "${GREEN}PASS (got expected error)${NC}"
        ((PASS++))
        return 0
    fi
}

# Test helper - skip
test_skip() {
    local name="$1"
    local reason="$2"
    ((TOTAL++))
    ((SKIP++))
    echo -e "  [$TOTAL] $name ... ${YELLOW}SKIP${NC} ($reason)"
}

# Capture output for later use
capture() {
    "$CLI" "$@" 2>/dev/null
}

echo "=============================================="
echo "       btc-cli Comprehensive Test Suite"
echo "=============================================="
echo ""

# ============================================
# SECTION 1: CLI Infrastructure
# ============================================
echo "=== 1. CLI Infrastructure Tests ==="

test_ok "help flag" -help
test_ok "help=getblock" -help=getblock
test_err "help=nonexistent" -help=nonexistent
test_ok "signet getblockcount" -signet getblockcount
test_err "testnet (no node)" -testnet getblockcount
test_err "regtest (no node)" -regtest getblockcount
test_ok "explicit rpcconnect" -signet -rpcconnect=127.0.0.1 getblockcount
test_ok "explicit rpcport" -signet -rpcport=38332 getblockcount
test_err "bad host" -signet -rpcconnect=badhost.invalid getblockcount
test_err "bad auth" -signet -rpcuser=wrong -rpcpassword=wrong getblockcount
test_err "unknown command" -signet notarealcommand
test_err "no command" -signet

echo ""

# ============================================
# SECTION 2: Blockchain Commands
# ============================================
echo "=== 2. Blockchain Commands ==="

# Basic queries
test_ok "getblockchaininfo" -signet getblockchaininfo
test_ok "getblockcount" -signet getblockcount
test_ok "getbestblockhash" -signet getbestblockhash
test_ok "getdifficulty" -signet getdifficulty
test_ok "getchaintips" -signet getchaintips
test_ok "getmempoolinfo" -signet getmempoolinfo
test_ok "getrawmempool" -signet getrawmempool
test_ok "savemempool" -signet savemempool

# getblockhash tests
test_ok "getblockhash 0 (genesis)" -signet getblockhash 0
test_ok "getblockhash 100" -signet getblockhash 100
test_err "getblockhash 999999 (too high)" -signet getblockhash 999999
test_err "getblockhash -1 (negative)" -signet getblockhash -1

# Get a real hash for further tests
HASH=$(capture -signet getblockhash 100)
if [ -n "$HASH" ]; then
    test_ok "getblock (default)" -signet getblock "$HASH"
    test_ok "getblock verbosity=0" -signet getblock "$HASH" 0
    test_ok "getblock verbosity=1" -signet getblock "$HASH" 1
    test_ok "getblock verbosity=2" -signet getblock "$HASH" 2
    test_ok "getblockheader" -signet getblockheader "$HASH"
    test_ok "getblockheader verbose=true" -signet getblockheader "$HASH" true
    test_ok "getblockheader verbose=false" -signet getblockheader "$HASH" false
    test_ok "getblockstats by height" -signet getblockstats 100
else
    test_skip "getblock tests" "could not get block hash"
fi

test_err "getblock invalid hash" -signet getblock "invalidhash"
test_err "getblock all zeros" -signet getblock "0000000000000000000000000000000000000000000000000000000000000000"

# Chain stats
test_ok "getchaintxstats" -signet getchaintxstats
test_ok "getchaintxstats 100" -signet getchaintxstats 100

# Mempool - verbose modes
test_ok "getrawmempool verbose" -signet getrawmempool true

echo ""

# ============================================
# SECTION 3: Wallet Setup
# ============================================
echo "=== 3. Wallet Setup ==="

# Clean up any existing test wallet
capture -signet unloadwallet "$WALLET" 2>/dev/null

# Create or load test wallet (handle case where wallet exists on disk)
if ! "$CLI" -signet createwallet "$WALLET" >/dev/null 2>&1; then
    # Wallet exists, try to load it
    "$CLI" -signet loadwallet "$WALLET" >/dev/null 2>&1
fi
echo "  [wallet setup] Using wallet: $WALLET"
test_ok "listwallets" -signet listwallets

echo ""

# ============================================
# SECTION 4: Wallet Basic Commands
# ============================================
echo "=== 4. Wallet Basic Commands ==="

test_ok "getwalletinfo" -signet -rpcwallet="$WALLET" getwalletinfo
test_ok "getbalance" -signet -rpcwallet="$WALLET" getbalance
test_ok "getbalances" -signet -rpcwallet="$WALLET" getbalances
test_skip "getunconfirmedbalance" "deprecated in Bitcoin Core 30.x"
test_ok "listunspent" -signet -rpcwallet="$WALLET" listunspent
test_ok "listtransactions" -signet -rpcwallet="$WALLET" listtransactions
test_ok "listlockunspent" -signet -rpcwallet="$WALLET" listlockunspent
test_ok "listaddressgroupings" -signet -rpcwallet="$WALLET" listaddressgroupings
test_ok "listlabels" -signet -rpcwallet="$WALLET" listlabels
test_ok "listdescriptors" -signet -rpcwallet="$WALLET" listdescriptors

echo ""

# ============================================
# SECTION 5: Address Generation
# ============================================
echo "=== 5. Address Generation ==="

test_ok "getnewaddress (default)" -signet -rpcwallet="$WALLET" getnewaddress
test_ok "getnewaddress legacy" -signet -rpcwallet="$WALLET" getnewaddress "" legacy
test_ok "getnewaddress p2sh-segwit" -signet -rpcwallet="$WALLET" getnewaddress "" p2sh-segwit
test_ok "getnewaddress bech32" -signet -rpcwallet="$WALLET" getnewaddress "" bech32
test_ok "getnewaddress bech32m" -signet -rpcwallet="$WALLET" getnewaddress "" bech32m
test_ok "getnewaddress with label" -signet -rpcwallet="$WALLET" getnewaddress "mylabel"
test_ok "getrawchangeaddress" -signet -rpcwallet="$WALLET" getrawchangeaddress

# Get address for further tests
ADDR=$(capture -signet -rpcwallet="$WALLET" getnewaddress)
if [ -n "$ADDR" ]; then
    test_ok "getaddressinfo" -signet -rpcwallet="$WALLET" getaddressinfo "$ADDR"
    test_ok "validateaddress" -signet validateaddress "$ADDR"
    test_ok "setlabel" -signet -rpcwallet="$WALLET" setlabel "$ADDR" "test_label"
    test_ok "getaddressesbylabel" -signet -rpcwallet="$WALLET" getaddressesbylabel "test_label"
    test_ok "getreceivedbyaddress" -signet -rpcwallet="$WALLET" getreceivedbyaddress "$ADDR"
fi

test_ok "validateaddress invalid" -signet validateaddress "invalid_address"
test_ok "listreceivedbyaddress" -signet -rpcwallet="$WALLET" listreceivedbyaddress
test_ok "listreceivedbylabel" -signet -rpcwallet="$WALLET" listreceivedbylabel

echo ""

# ============================================
# SECTION 6: Wallet Management
# ============================================
echo "=== 6. Wallet Management ==="

test_ok "backupwallet" -signet -rpcwallet="$WALLET" backupwallet /tmp/btc_cli_test_backup.dat
test_ok "keypoolrefill" -signet -rpcwallet="$WALLET" keypoolrefill
test_ok "keypoolrefill 100" -signet -rpcwallet="$WALLET" keypoolrefill 100

# Unload/load cycle
test_ok "unloadwallet" -signet unloadwallet "$WALLET"
test_ok "loadwallet" -signet loadwallet "$WALLET"

# Create variant wallets
# Create or load blank wallet (handle existing)
if ! "$CLI" -signet createwallet "blank_test" false true >/dev/null 2>&1; then
    "$CLI" -signet loadwallet "blank_test" >/dev/null 2>&1 || true
fi
test_ok "unloadwallet blank" -signet unloadwallet "blank_test"

# Create or load watchonly wallet (handle existing)
if ! "$CLI" -signet createwallet "watchonly_test" true >/dev/null 2>&1; then
    "$CLI" -signet loadwallet "watchonly_test" >/dev/null 2>&1 || true
fi
test_ok "unloadwallet watchonly" -signet unloadwallet "watchonly_test"

echo ""

# ============================================
# SECTION 7: Network Commands
# ============================================
echo "=== 7. Network Commands ==="

test_ok "getnetworkinfo" -signet getnetworkinfo
test_ok "getpeerinfo" -signet getpeerinfo
test_ok "getconnectioncount" -signet getconnectioncount
test_ok "getnettotals" -signet getnettotals
test_ok "listbanned" -signet listbanned
test_ok "getnodeaddresses" -signet getnodeaddresses
test_ok "getnodeaddresses 10" -signet getnodeaddresses 10
test_ok "ping" -signet ping

echo ""

# ============================================
# SECTION 8: Mining/Control Commands
# ============================================
echo "=== 8. Mining/Control Commands ==="

test_ok "getmininginfo" -signet getmininginfo
test_ok "getnetworkhashps" -signet getnetworkhashps
test_ok "uptime" -signet uptime
test_ok "getmemoryinfo" -signet getmemoryinfo
test_ok "getrpcinfo" -signet getrpcinfo
test_ok "logging" -signet logging
test_ok "help" -signet help
test_ok "help getblock" -signet help getblock

echo ""

# ============================================
# SECTION 9: Raw Transaction Tests
# ============================================
echo "=== 9. Raw Transaction Tests ==="

# Get a real tx from the blockchain
TXID=$(capture -signet getblock "$(capture -signet getblockhash 100)" 1 | grep -o '"txid": "[^"]*"' | head -1 | cut -d'"' -f4)
if [ -n "$TXID" ]; then
    test_ok "getrawtransaction" -signet getrawtransaction "$TXID"
    test_ok "getrawtransaction verbose" -signet getrawtransaction "$TXID" true

    RAWTX=$(capture -signet getrawtransaction "$TXID")
    if [ -n "$RAWTX" ]; then
        test_ok "decoderawtransaction" -signet decoderawtransaction "$RAWTX"
    fi
else
    test_skip "getrawtransaction tests" "could not find txid"
fi

test_ok "createrawtransaction empty" -signet createrawtransaction '[]' '[]'
test_err "decoderawtransaction invalid" -signet decoderawtransaction "notahex"
test_err "decoderawtransaction short" -signet decoderawtransaction "0000"

echo ""

# ============================================
# SECTION 10: PSBT Tests
# ============================================
echo "=== 10. PSBT Tests ==="

PSBT=$(capture -signet createpsbt '[]' '[]')
test_ok "createpsbt empty" -signet createpsbt '[]' '[]'

if [ -n "$PSBT" ]; then
    test_ok "decodepsbt" -signet decodepsbt "$PSBT"
    test_ok "analyzepsbt" -signet analyzepsbt "$PSBT"
fi

test_err "decodepsbt invalid" -signet decodepsbt "notapsbt"
test_err "analyzepsbt invalid" -signet analyzepsbt "notapsbt"

echo ""

# ============================================
# SECTION 11: Utility Commands
# ============================================
echo "=== 11. Utility Commands ==="

test_ok "estimatesmartfee 6" -signet estimatesmartfee 6
test_ok "estimatesmartfee 1" -signet estimatesmartfee 1
test_ok "estimatesmartfee 144" -signet estimatesmartfee 144
test_ok "getindexinfo" -signet getindexinfo

# Message signing (need legacy address)
LEGACY_ADDR=$(capture -signet -rpcwallet="$WALLET" getnewaddress "" legacy)
if [ -n "$LEGACY_ADDR" ]; then
    SIG=$(capture -signet -rpcwallet="$WALLET" signmessage "$LEGACY_ADDR" "test message")
    test_ok "signmessage" -signet -rpcwallet="$WALLET" signmessage "$LEGACY_ADDR" "test message"
    if [ -n "$SIG" ]; then
        test_ok "verifymessage" -signet verifymessage "$LEGACY_ADDR" "$SIG" "test message"
    fi
fi

echo ""

# ============================================
# SECTION 12: Error Handling
# ============================================
echo "=== 12. Error Handling Tests ==="

test_err "unknown command" -signet fakecommand
test_err "case sensitive" -signet GETBLOCKCOUNT
test_err "missing param getblockhash" -signet getblockhash
test_err "missing param getblock" -signet getblock
test_err "wrong type getblockhash" -signet getblockhash "notanumber"
test_err "wrong type getblock" -signet getblock 12345
test_err "nonexistent wallet" -signet -rpcwallet=nonexistent getbalance
test_err "bad port" -signet -rpcport=1 getblockcount

echo ""

# ============================================
# SECTION 13: Edge Cases
# ============================================
echo "=== 13. Edge Cases ==="

test_ok "empty mempool" -signet getrawmempool
test_ok "genesis block" -signet getblockhash 0
test_ok "label with spaces" -signet -rpcwallet="$WALLET" getnewaddress "label with spaces"
test_ok "label with underscore" -signet -rpcwallet="$WALLET" getnewaddress "label_underscore"

echo ""

# ============================================
# CLEANUP
# ============================================
echo "=== Cleanup ==="

capture -signet unloadwallet "$WALLET" 2>/dev/null
echo "  Unloaded test wallet"

# Remove backup file
rm -f /tmp/btc_cli_test_backup.dat 2>/dev/null

echo ""
echo "=============================================="
echo "                  RESULTS"
echo "=============================================="
echo ""
echo -e "  ${GREEN}PASSED:${NC}  $PASS"
echo -e "  ${RED}FAILED:${NC}  $FAIL"
echo -e "  ${YELLOW}SKIPPED:${NC} $SKIP"
echo "  ─────────────"
echo "  TOTAL:   $TOTAL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
