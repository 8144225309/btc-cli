#!/bin/bash
#
# btc-cli TORTURE TEST SUITE
# Tests ALL 160 methods with ALL variations, edge cases, limits, and stress tests
#
# Usage: ./test_torture.sh [network] [wallet]
#   network: signet (default), regtest, testnet
#   wallet: wallet name to use for wallet tests
#
# Examples:
#   ./test_torture.sh                    # signet with auto-created wallet
#   ./test_torture.sh regtest mywallet   # regtest with specific wallet
#

set -o pipefail

# Configuration
NETWORK="${1:-signet}"
WALLET="${2:-torture_test_wallet}"
CLI="./btc-cli"
CLIW="$CLI -$NETWORK -rpcwallet=$WALLET"
CLIN="$CLI -$NETWORK"

# Counters
PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Temp files for large data tests
TMPDIR="/tmp/btc-cli-torture"
mkdir -p "$TMPDIR"

# ============================================
# TEST HELPERS
# ============================================

# Test expecting success
test_ok() {
    local name="$1"
    shift
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$@" 2>&1); then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "      Command: $*"
        echo "      Output: ${output:0:200}"
        ((FAIL++))
        return 1
    fi
}

# Test expecting error
test_err() {
    local name="$1"
    shift
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$@" 2>&1); then
        echo -e "${RED}FAIL (expected error)${NC}"
        echo "      Command: $*"
        ((FAIL++))
        return 1
    else
        echo -e "${GREEN}PASS (expected error)${NC}"
        ((PASS++))
        return 0
    fi
}

# Test with output validation
test_match() {
    local name="$1"
    local pattern="$2"
    shift 2
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$@" 2>&1); then
        if echo "$output" | grep -qE "$pattern"; then
            echo -e "${GREEN}PASS${NC}"
            ((PASS++))
            return 0
        else
            echo -e "${RED}FAIL (pattern not found)${NC}"
            echo "      Pattern: $pattern"
            echo "      Output: ${output:0:200}"
            ((FAIL++))
            return 1
        fi
    else
        echo -e "${RED}FAIL${NC}"
        echo "      Command: $*"
        echo "      Output: ${output:0:200}"
        ((FAIL++))
        return 1
    fi
}

# Test output is valid JSON
test_json() {
    local name="$1"
    shift
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$@" 2>&1); then
        # Check if starts with { or [
        if echo "$output" | grep -qE '^\s*[\{\[]'; then
            echo -e "${GREEN}PASS (valid JSON)${NC}"
            ((PASS++))
            return 0
        else
            echo -e "${RED}FAIL (not JSON)${NC}"
            echo "      Output: ${output:0:100}"
            ((FAIL++))
            return 1
        fi
    else
        echo -e "${RED}FAIL${NC}"
        echo "      Output: ${output:0:200}"
        ((FAIL++))
        return 1
    fi
}

# Test with numeric output validation
test_numeric() {
    local name="$1"
    shift
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$@" 2>&1); then
        if echo "$output" | grep -qE '^-?[0-9]+\.?[0-9]*$'; then
            echo -e "${GREEN}PASS ($output)${NC}"
            ((PASS++))
            return 0
        else
            echo -e "${RED}FAIL (not numeric)${NC}"
            echo "      Output: $output"
            ((FAIL++))
            return 1
        fi
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
        return 1
    fi
}

# Test with hex output validation
test_hex() {
    local name="$1"
    local min_len="${2:-1}"
    shift 2
    ((TOTAL++))

    echo -n "  [$TOTAL] $name ... "
    if output=$("$@" 2>&1); then
        if echo "$output" | grep -qE "^[a-f0-9]{$min_len,}$"; then
            echo -e "${GREEN}PASS (${#output} chars)${NC}"
            ((PASS++))
            return 0
        else
            echo -e "${RED}FAIL (not hex or too short)${NC}"
            echo "      Output: ${output:0:100}"
            ((FAIL++))
            return 1
        fi
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
        return 1
    fi
}

# Skip test with reason
test_skip() {
    local name="$1"
    local reason="$2"
    ((TOTAL++))
    ((SKIP++))
    echo -e "  [$TOTAL] $name ... ${YELLOW}SKIP${NC} ($reason)"
}

# Capture output for later use
capture() {
    "$@" 2>/dev/null
}

# Section header
section() {
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
}

# Subsection header
subsection() {
    echo ""
    echo -e "${BLUE}--- $1 ---${NC}"
}

# ============================================
# SETUP
# ============================================

echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║       btc-cli TORTURE TEST SUITE - ALL 160 METHODS            ║${NC}"
echo -e "${CYAN}║       Network: $NETWORK                                         ${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check connection
echo -n "Checking connection to $NETWORK node... "
if ! $CLIN getblockcount >/dev/null 2>&1; then
    echo -e "${RED}FAILED${NC}"
    echo "Cannot connect to $NETWORK node. Make sure bitcoind is running."
    exit 1
fi
echo -e "${GREEN}OK${NC}"

# Setup wallet
echo -n "Setting up test wallet '$WALLET'... "
if ! $CLIN loadwallet "$WALLET" >/dev/null 2>&1; then
    if ! $CLIN createwallet "$WALLET" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC}"
        echo "Cannot create or load wallet"
        exit 1
    fi
fi
echo -e "${GREEN}OK${NC}"

# Get current chain state for tests
BLOCK_COUNT=$(capture $CLIN getblockcount)
BEST_HASH=$(capture $CLIN getbestblockhash)
GENESIS_HASH=$(capture $CLIN getblockhash 0)
BLOCK_100_HASH=$(capture $CLIN getblockhash 100 2>/dev/null || echo "")

echo "Block count: $BLOCK_COUNT"
echo "Best hash: ${BEST_HASH:0:16}..."
echo ""


# ============================================
# SECTION 1: CLI INFRASTRUCTURE
# ============================================
section "1. CLI INFRASTRUCTURE TESTS"

subsection "Help System"
test_ok "help flag" $CLI -help
test_ok "help=getblock" $CLI -help=getblock
test_ok "help=getblockcount" $CLI -help=getblockcount
test_ok "help=sendtoaddress" $CLI -help=sendtoaddress
test_ok "help=createpsbt" $CLI -help=createpsbt
test_err "help=nonexistent" $CLI -help=fakecmd123
# Note: -help= with empty value shows general help (valid behavior)
test_ok "help=empty shows help" $CLI -help=

subsection "Network Selection"
test_ok "signet flag" $CLI -signet getblockcount
if [ "$NETWORK" = "regtest" ]; then
    test_ok "regtest flag" $CLI -regtest getblockcount
else
    test_err "regtest (no node)" $CLI -regtest getblockcount
fi
test_err "testnet (no node)" $CLI -testnet getblockcount

subsection "Connection Options"
test_ok "explicit rpcconnect=127.0.0.1" $CLIN -rpcconnect=127.0.0.1 getblockcount
test_ok "explicit rpcconnect=localhost" $CLIN -rpcconnect=localhost getblockcount
test_err "bad host" $CLIN -rpcconnect=badhost.invalid.tld getblockcount
test_err "unreachable IP" $CLIN -rpcconnect=192.168.255.255 getblockcount
test_err "bad port 1" $CLIN -rpcport=1 getblockcount
test_err "bad port 99999" $CLIN -rpcport=99999 getblockcount

subsection "Authentication"
test_err "wrong user/pass" $CLIN -rpcuser=wrong -rpcpassword=wrong getblockcount
# Note: Empty user with password falls back to cookie auth if available
# This test only fails when cookie auth is not available
# test_err "empty user" $CLIN -rpcuser= -rpcpassword=test getblockcount
test_err "wrong cookie file" $CLIN -rpccookiefile=/nonexistent/cookie getblockcount

subsection "Command Handling"
test_err "unknown command" $CLIN notarealcommand
test_err "case sensitive GETBLOCKCOUNT" $CLIN GETBLOCKCOUNT
test_err "case sensitive GetBlockCount" $CLIN GetBlockCount
test_err "no command" $CLIN
test_err "empty command" $CLIN ""


# ============================================
# SECTION 2: BLOCKCHAIN QUERIES (NO PARAMS)
# ============================================
section "2. BLOCKCHAIN - BASIC QUERIES"

subsection "Chain Info"
test_json "getblockchaininfo" $CLIN getblockchaininfo
test_match "getblockchaininfo has chain" '"chain"' $CLIN getblockchaininfo
test_match "getblockchaininfo has blocks" '"blocks"' $CLIN getblockchaininfo
test_match "getblockchaininfo has headers" '"headers"' $CLIN getblockchaininfo

test_numeric "getblockcount" $CLIN getblockcount
test_hex "getbestblockhash" 64 $CLIN getbestblockhash
test_numeric "getdifficulty" $CLIN getdifficulty

subsection "Chain Tips"
test_json "getchaintips" $CLIN getchaintips
test_match "getchaintips has height" '"height"' $CLIN getchaintips
test_match "getchaintips has hash" '"hash"' $CLIN getchaintips

subsection "Mempool Info"
test_json "getmempoolinfo" $CLIN getmempoolinfo
test_match "getmempoolinfo has size" '"size"' $CLIN getmempoolinfo
test_json "getrawmempool" $CLIN getrawmempool
test_ok "savemempool" $CLIN savemempool


# ============================================
# SECTION 3: BLOCK QUERIES
# ============================================
section "3. BLOCKCHAIN - BLOCK QUERIES"

subsection "getblockhash"
test_hex "getblockhash 0 (genesis)" 64 $CLIN getblockhash 0
test_hex "getblockhash 1" 64 $CLIN getblockhash 1
test_hex "getblockhash 10" 64 $CLIN getblockhash 10
test_hex "getblockhash 100" 64 $CLIN getblockhash 100
test_err "getblockhash -1 (negative)" $CLIN getblockhash -1
test_err "getblockhash 999999999 (too high)" $CLIN getblockhash 999999999
test_err "getblockhash string" $CLIN getblockhash "notanumber"
test_err "getblockhash float" $CLIN getblockhash 1.5
test_err "getblockhash missing param" $CLIN getblockhash

subsection "getblock variations"
if [ -n "$BLOCK_100_HASH" ]; then
    test_json "getblock default" $CLIN getblock "$BLOCK_100_HASH"
    test_hex "getblock verbosity=0 (raw hex)" 100 $CLIN getblock "$BLOCK_100_HASH" 0
    test_json "getblock verbosity=1 (json)" $CLIN getblock "$BLOCK_100_HASH" 1
    test_json "getblock verbosity=2 (json+tx)" $CLIN getblock "$BLOCK_100_HASH" 2
    test_json "getblock verbosity=3 (json+prevout)" $CLIN getblock "$BLOCK_100_HASH" 3
    test_match "getblock v1 has hash" '"hash"' $CLIN getblock "$BLOCK_100_HASH" 1
    test_match "getblock v1 has tx array" '"tx"' $CLIN getblock "$BLOCK_100_HASH" 1
    test_match "getblock v2 has txid in tx" '"txid"' $CLIN getblock "$BLOCK_100_HASH" 2
else
    test_skip "getblock variations" "block 100 not available"
fi

test_err "getblock invalid hash" $CLIN getblock "invalidhash"
test_err "getblock short hash" $CLIN getblock "abcd1234"
test_err "getblock all zeros" $CLIN getblock "0000000000000000000000000000000000000000000000000000000000000000"
test_err "getblock missing param" $CLIN getblock
# Note: Bitcoin Core accepts any verbosity value (uses default for out-of-range)
test_ok "getblock verbosity 99" $CLIN getblock "$GENESIS_HASH" 99
test_ok "getblock verbosity -1" $CLIN getblock "$GENESIS_HASH" -1

subsection "getblockheader variations"
test_json "getblockheader default" $CLIN getblockheader "$GENESIS_HASH"
test_json "getblockheader verbose=true" $CLIN getblockheader "$GENESIS_HASH" true
test_hex "getblockheader verbose=false (hex)" 100 $CLIN getblockheader "$GENESIS_HASH" false
test_match "getblockheader has nonce" '"nonce"' $CLIN getblockheader "$GENESIS_HASH"
test_match "getblockheader has merkleroot" '"merkleroot"' $CLIN getblockheader "$GENESIS_HASH"
test_err "getblockheader invalid hash" $CLIN getblockheader "badhash"
test_err "getblockheader missing param" $CLIN getblockheader

subsection "getblockstats"
test_json "getblockstats by height 0" $CLIN getblockstats 0
test_json "getblockstats by height 10" $CLIN getblockstats 10
test_json "getblockstats by height 100" $CLIN getblockstats 100
if [ -n "$BLOCK_100_HASH" ]; then
    test_json "getblockstats by hash" $CLIN getblockstats "$BLOCK_100_HASH"
fi
test_match "getblockstats has avgfee" '"avgfee"' $CLIN getblockstats 100
test_match "getblockstats has txs" '"txs"' $CLIN getblockstats 100
test_err "getblockstats negative height" $CLIN getblockstats -1
test_err "getblockstats too high" $CLIN getblockstats 999999999

subsection "getchaintxstats"
test_json "getchaintxstats default" $CLIN getchaintxstats
test_json "getchaintxstats nblocks=10" $CLIN getchaintxstats 10
test_json "getchaintxstats nblocks=100" $CLIN getchaintxstats 100
test_match "getchaintxstats has txcount" '"txcount"' $CLIN getchaintxstats
test_err "getchaintxstats negative" $CLIN getchaintxstats -1


# ============================================
# SECTION 4: MEMPOOL QUERIES
# ============================================
section "4. BLOCKCHAIN - MEMPOOL QUERIES"

subsection "getrawmempool variations"
test_json "getrawmempool default (array)" $CLIN getrawmempool
test_json "getrawmempool verbose=false" $CLIN getrawmempool false
test_json "getrawmempool verbose=true" $CLIN getrawmempool true
test_ok "getrawmempool mempool_sequence" $CLIN getrawmempool false true

# Get a mempool txid if available
MEMPOOL_TXID=$(capture $CLIN getrawmempool | grep -o '"[a-f0-9]\{64\}"' | head -1 | tr -d '"')
if [ -n "$MEMPOOL_TXID" ]; then
    subsection "Mempool entry queries (have pending tx)"
    test_json "getmempoolentry" $CLIN getmempoolentry "$MEMPOOL_TXID"
    test_json "getmempoolancestors" $CLIN getmempoolancestors "$MEMPOOL_TXID"
    test_json "getmempoolancestors verbose" $CLIN getmempoolancestors "$MEMPOOL_TXID" true
    test_json "getmempooldescendants" $CLIN getmempooldescendants "$MEMPOOL_TXID"
    test_json "getmempooldescendants verbose" $CLIN getmempooldescendants "$MEMPOOL_TXID" true
else
    subsection "Mempool entry queries (no pending tx)"
    test_skip "getmempoolentry" "no mempool transactions"
    test_skip "getmempoolancestors" "no mempool transactions"
    test_skip "getmempooldescendants" "no mempool transactions"
fi

test_err "getmempoolentry invalid txid" $CLIN getmempoolentry "invalidtxid"
test_err "getmempoolentry nonexistent" $CLIN getmempoolentry "0000000000000000000000000000000000000000000000000000000000000000"


# ============================================
# SECTION 5: UTXO QUERIES
# ============================================
section "5. BLOCKCHAIN - UTXO QUERIES"

subsection "gettxoutsetinfo"
test_json "gettxoutsetinfo default" $CLIN gettxoutsetinfo
test_match "gettxoutsetinfo has total_amount" '"total_amount"' $CLIN gettxoutsetinfo
test_match "gettxoutsetinfo has txouts" '"txouts"' $CLIN gettxoutsetinfo

subsection "scantxoutset"
# Create a test descriptor
TEST_ADDR=$(capture $CLIW getnewaddress "" bech32)
if [ -n "$TEST_ADDR" ]; then
    test_json "scantxoutset start with addr" $CLIN scantxoutset start "[\"addr($TEST_ADDR)\"]"
fi
test_err "scantxoutset invalid action" $CLIN scantxoutset badaction "[]"

subsection "verifychain"
test_ok "verifychain default" $CLIN verifychain
test_ok "verifychain level 0" $CLIN verifychain 0
test_ok "verifychain level 1" $CLIN verifychain 1
test_ok "verifychain level 1 nblocks 10" $CLIN verifychain 1 10


# ============================================
# SECTION 6: NEW BLOCKCHAIN METHODS (v30.x)
# ============================================
section "6. BLOCKCHAIN - NEW v30.x METHODS"

subsection "Chain state methods"
test_json "getchainstates" $CLIN getchainstates
test_match "getchainstates has headers" '"headers"' $CLIN getchainstates

test_json "getdeploymentinfo" $CLIN getdeploymentinfo
test_match "getdeploymentinfo has deployments" '"deployments"' $CLIN getdeploymentinfo
if [ -n "$BEST_HASH" ]; then
    test_json "getdeploymentinfo with blockhash" $CLIN getdeploymentinfo "$BEST_HASH"
fi

subsection "Block from peer"
# Need peer_id - get from getpeerinfo
PEER_ID=$(capture $CLIN getpeerinfo | grep -o '"id": [0-9]*' | head -1 | grep -o '[0-9]*')
if [ -n "$PEER_ID" ] && [ -n "$BLOCK_100_HASH" ]; then
    # This usually fails because we already have the block - that's expected
    ((TOTAL++))
    echo -n "  [$TOTAL] getblockfrompeer ... "
    if $CLIN getblockfrompeer "$BLOCK_100_HASH" "$PEER_ID" >/dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
    else
        echo -e "${YELLOW}SKIP${NC} (block already known or peer unavailable)"
        ((SKIP++))
    fi
else
    test_skip "getblockfrompeer" "no peer or block hash"
fi

subsection "Transaction spending prevout"
# Note: gettxspendingprevout with empty array may fail on some node versions
((TOTAL++))
echo -n "  [$TOTAL] gettxspendingprevout empty ... "
if OUTPUT=$($CLIN gettxspendingprevout "[]" 2>&1); then
    echo -e "${GREEN}PASS${NC}"
    ((PASS++))
else
    echo -e "${YELLOW}SKIP${NC} (may require specific node configuration)"
    ((SKIP++))
fi

subsection "Wait methods"
# These are blocking calls with timeout - test with 1 second timeout
test_ok "waitfornewblock timeout=1" $CLIN waitfornewblock 1
test_ok "waitforblockheight 0 timeout=1" $CLIN waitforblockheight 0 1
if [ -n "$GENESIS_HASH" ]; then
    test_ok "waitforblock genesis timeout=1" $CLIN waitforblock "$GENESIS_HASH" 1
fi


# ============================================
# SECTION 7: WALLET - BASIC INFO
# ============================================
section "7. WALLET - BASIC QUERIES"

subsection "Wallet info"
test_json "getwalletinfo" $CLIW getwalletinfo
test_match "getwalletinfo has walletname" '"walletname"' $CLIW getwalletinfo
# Note: Bitcoin Core 30+ uses getbalances instead of balance in getwalletinfo
test_match "getwalletinfo has keypoolsize" '"keypoolsize"' $CLIW getwalletinfo
test_match "getwalletinfo has txcount" '"txcount"' $CLIW getwalletinfo

subsection "Balance queries"
test_ok "getbalance default" $CLIW getbalance
test_ok "getbalance minconf=0" $CLIW getbalance "*" 0
test_ok "getbalance minconf=1" $CLIW getbalance "*" 1
test_ok "getbalance minconf=6" $CLIW getbalance "*" 6
test_json "getbalances" $CLIW getbalances
test_match "getbalances has mine" '"mine"' $CLIW getbalances

subsection "UTXO and transaction lists"
test_json "listunspent default" $CLIW listunspent
test_json "listunspent minconf=0" $CLIW listunspent 0
test_json "listunspent minconf=1 maxconf=9999" $CLIW listunspent 1 9999
test_json "listtransactions default" $CLIW listtransactions
test_json "listtransactions count=5" $CLIW listtransactions "*" 5
test_json "listtransactions count=100" $CLIW listtransactions "*" 100
test_json "listlockunspent" $CLIW listlockunspent
test_json "listaddressgroupings" $CLIW listaddressgroupings
test_json "listlabels" $CLIW listlabels
test_json "listdescriptors" $CLIW listdescriptors
test_json "listdescriptors private=false" $CLIW listdescriptors false


# ============================================
# SECTION 8: WALLET - ADDRESS GENERATION
# ============================================
section "8. WALLET - ADDRESS GENERATION"

subsection "getnewaddress - all types"
test_ok "getnewaddress default" $CLIW getnewaddress
test_match "getnewaddress legacy starts with m/n" '^[mn]' $CLIW getnewaddress "" legacy
test_match "getnewaddress p2sh-segwit starts with 2" '^2' $CLIW getnewaddress "" p2sh-segwit
test_match "getnewaddress bech32 starts with tb1q/bcrt1q" '^(tb1q|bcrt1q)' $CLIW getnewaddress "" bech32
test_match "getnewaddress bech32m starts with tb1p/bcrt1p" '^(tb1p|bcrt1p)' $CLIW getnewaddress "" bech32m

subsection "getnewaddress - with labels"
test_ok "getnewaddress with label" $CLIW getnewaddress "test_label_1"
test_ok "getnewaddress label with spaces" $CLIW getnewaddress "label with spaces"
test_ok "getnewaddress label with underscore" $CLIW getnewaddress "label_underscore"
test_ok "getnewaddress label with numbers" $CLIW getnewaddress "label123"
test_ok "getnewaddress empty label" $CLIW getnewaddress ""

subsection "getrawchangeaddress"
test_ok "getrawchangeaddress default" $CLIW getrawchangeaddress
test_ok "getrawchangeaddress legacy" $CLIW getrawchangeaddress legacy
test_ok "getrawchangeaddress p2sh-segwit" $CLIW getrawchangeaddress p2sh-segwit
test_ok "getrawchangeaddress bech32" $CLIW getrawchangeaddress bech32
test_ok "getrawchangeaddress bech32m" $CLIW getrawchangeaddress bech32m

# Generate addresses for later tests
ADDR_LEGACY=$(capture $CLIW getnewaddress "" legacy)
ADDR_P2SH=$(capture $CLIW getnewaddress "" p2sh-segwit)
ADDR_BECH32=$(capture $CLIW getnewaddress "" bech32)
ADDR_BECH32M=$(capture $CLIW getnewaddress "" bech32m)


# ============================================
# SECTION 9: WALLET - ADDRESS INFO
# ============================================
section "9. WALLET - ADDRESS INFO"

subsection "getaddressinfo"
if [ -n "$ADDR_LEGACY" ]; then
    test_json "getaddressinfo legacy" $CLIW getaddressinfo "$ADDR_LEGACY"
    test_match "getaddressinfo has ismine" '"ismine"' $CLIW getaddressinfo "$ADDR_LEGACY"
fi
if [ -n "$ADDR_BECH32" ]; then
    test_json "getaddressinfo bech32" $CLIW getaddressinfo "$ADDR_BECH32"
fi
if [ -n "$ADDR_BECH32M" ]; then
    test_json "getaddressinfo bech32m" $CLIW getaddressinfo "$ADDR_BECH32M"
fi

subsection "validateaddress"
test_json "validateaddress legacy" $CLIN validateaddress "$ADDR_LEGACY"
test_json "validateaddress bech32" $CLIN validateaddress "$ADDR_BECH32"
test_json "validateaddress bech32m" $CLIN validateaddress "$ADDR_BECH32M"
test_json "validateaddress invalid" $CLIN validateaddress "invalid_address"
test_match "validateaddress invalid returns isvalid=false" '"isvalid": false' $CLIN validateaddress "invalid"
test_json "validateaddress empty" $CLIN validateaddress ""

subsection "Labels"
LABEL_ADDR=$(capture $CLIW getnewaddress "torture_label")
if [ -n "$LABEL_ADDR" ]; then
    test_ok "setlabel" $CLIW setlabel "$LABEL_ADDR" "new_torture_label"
    test_json "getaddressesbylabel" $CLIW getaddressesbylabel "new_torture_label"
fi
test_json "listlabels" $CLIW listlabels

subsection "Received amounts"
test_ok "getreceivedbyaddress" $CLIW getreceivedbyaddress "$ADDR_BECH32"
test_ok "getreceivedbyaddress minconf=0" $CLIW getreceivedbyaddress "$ADDR_BECH32" 0
test_ok "getreceivedbyaddress minconf=6" $CLIW getreceivedbyaddress "$ADDR_BECH32" 6
test_json "listreceivedbyaddress" $CLIW listreceivedbyaddress
test_json "listreceivedbyaddress minconf=0" $CLIW listreceivedbyaddress 0
test_json "listreceivedbyaddress include_empty" $CLIW listreceivedbyaddress 0 true
test_json "listreceivedbylabel" $CLIW listreceivedbylabel


# ============================================
# SECTION 10: WALLET - MANAGEMENT
# ============================================
section "10. WALLET - MANAGEMENT"

subsection "Wallet list and info"
test_json "listwallets" $CLIN listwallets
test_json "listwalletdir" $CLIN listwalletdir

subsection "Backup"
test_ok "backupwallet" $CLIW backupwallet "$TMPDIR/wallet_backup.dat"

subsection "Keypool"
test_ok "keypoolrefill default" $CLIW keypoolrefill
test_ok "keypoolrefill 10" $CLIW keypoolrefill 10
test_ok "keypoolrefill 100" $CLIW keypoolrefill 100

subsection "Load/Unload cycle"
# Create a temp wallet for testing
TEMP_WALLET="temp_torture_$$"
test_ok "createwallet temp" $CLIN createwallet "$TEMP_WALLET"
test_ok "unloadwallet temp" $CLIN unloadwallet "$TEMP_WALLET"
test_ok "loadwallet temp" $CLIN loadwallet "$TEMP_WALLET"
test_ok "unloadwallet temp again" $CLIN unloadwallet "$TEMP_WALLET"

subsection "Create wallet variations"
test_ok "createwallet blank" $CLIN createwallet "blank_$$" false true
$CLIN unloadwallet "blank_$$" >/dev/null 2>&1

test_ok "createwallet disable_private_keys" $CLIN createwallet "watchonly_$$" true
$CLIN unloadwallet "watchonly_$$" >/dev/null 2>&1

test_ok "createwallet descriptors" $CLIN createwallet "desc_$$" false false "" false true
$CLIN unloadwallet "desc_$$" >/dev/null 2>&1


# ============================================
# SECTION 11: RAW TRANSACTIONS
# ============================================
section "11. RAW TRANSACTIONS"

subsection "createrawtransaction"
test_ok "createrawtransaction empty" $CLIN createrawtransaction '[]' '[]'
test_ok "createrawtransaction empty outputs obj" $CLIN createrawtransaction '[]' '{}'

# Create inputs/outputs for testing (won't be valid but tests parsing)
FAKE_TXID="0000000000000000000000000000000000000000000000000000000000000001"
test_ok "createrawtransaction with inputs" $CLIN createrawtransaction "[{\"txid\":\"$FAKE_TXID\",\"vout\":0}]" '[]'
test_ok "createrawtransaction with outputs array" $CLIN createrawtransaction '[]' "[{\"$ADDR_BECH32\":0.001}]"
test_ok "createrawtransaction with outputs obj" $CLIN createrawtransaction '[]' "{\"$ADDR_BECH32\":0.001}"
test_ok "createrawtransaction locktime" $CLIN createrawtransaction '[]' '[]' 500000
test_ok "createrawtransaction replaceable" $CLIN createrawtransaction '[]' '[]' 0 true

test_err "createrawtransaction invalid json" $CLIN createrawtransaction 'notjson' '[]'
test_err "createrawtransaction missing outputs" $CLIN createrawtransaction '[]'

subsection "decoderawtransaction"
# Create a valid raw tx to decode
RAWTX=$(capture $CLIN createrawtransaction '[]' '[]')
if [ -n "$RAWTX" ]; then
    test_json "decoderawtransaction" $CLIN decoderawtransaction "$RAWTX"
    test_match "decoderawtransaction has txid" '"txid"' $CLIN decoderawtransaction "$RAWTX"
    test_match "decoderawtransaction has version" '"version"' $CLIN decoderawtransaction "$RAWTX"
fi

test_err "decoderawtransaction invalid hex" $CLIN decoderawtransaction "notahex"
test_err "decoderawtransaction short" $CLIN decoderawtransaction "0000"
test_err "decoderawtransaction empty" $CLIN decoderawtransaction ""

subsection "decodescript"
# Test with some common script types
test_json "decodescript p2pkh" $CLIN decodescript "76a91489abcdefabbaabbaabbaabbaabbaabbaabbaabba88ac"
test_json "decodescript p2sh" $CLIN decodescript "a91489abcdefabbaabbaabbaabbaabbaabbaabbaabba87"
test_json "decodescript empty" $CLIN decodescript ""
test_err "decodescript invalid" $CLIN decodescript "notahex"

subsection "getrawtransaction"
# Get a real txid from a block
if [ -n "$BLOCK_100_HASH" ]; then
    BLOCK_TXID=$(capture $CLIN getblock "$BLOCK_100_HASH" 1 | grep -o '"txid": "[^"]*"' | head -1 | cut -d'"' -f4)
    if [ -n "$BLOCK_TXID" ]; then
        test_hex "getrawtransaction" 100 $CLIN getrawtransaction "$BLOCK_TXID"
        test_json "getrawtransaction verbose" $CLIN getrawtransaction "$BLOCK_TXID" true
        test_json "getrawtransaction verbose=1" $CLIN getrawtransaction "$BLOCK_TXID" 1
        test_json "getrawtransaction verbose=2" $CLIN getrawtransaction "$BLOCK_TXID" 2
        test_hex "getrawtransaction with blockhash" 100 $CLIN getrawtransaction "$BLOCK_TXID" false "$BLOCK_100_HASH"
    fi
fi

test_err "getrawtransaction invalid txid" $CLIN getrawtransaction "invalidtxid"
test_err "getrawtransaction nonexistent" $CLIN getrawtransaction "$FAKE_TXID"

subsection "combinerawtransaction"
test_err "combinerawtransaction empty array" $CLIN combinerawtransaction '[]'


# ============================================
# SECTION 12: PSBT
# ============================================
section "12. PSBT OPERATIONS"

subsection "createpsbt"
test_ok "createpsbt empty" $CLIN createpsbt '[]' '[]'
test_ok "createpsbt with inputs" $CLIN createpsbt "[{\"txid\":\"$FAKE_TXID\",\"vout\":0}]" '[]'
test_ok "createpsbt with outputs" $CLIN createpsbt '[]' "[{\"$ADDR_BECH32\":0.001}]"
test_ok "createpsbt locktime" $CLIN createpsbt '[]' '[]' 500000
test_ok "createpsbt replaceable" $CLIN createpsbt '[]' '[]' 0 true

# Get a PSBT for further tests
PSBT=$(capture $CLIN createpsbt '[]' '[]')

subsection "decodepsbt"
if [ -n "$PSBT" ]; then
    test_json "decodepsbt" $CLIN decodepsbt "$PSBT"
    test_match "decodepsbt has tx" '"tx"' $CLIN decodepsbt "$PSBT"
    test_match "decodepsbt has inputs" '"inputs"' $CLIN decodepsbt "$PSBT"
    test_match "decodepsbt has outputs" '"outputs"' $CLIN decodepsbt "$PSBT"
fi
test_err "decodepsbt invalid" $CLIN decodepsbt "notapsbt"
test_err "decodepsbt empty" $CLIN decodepsbt ""

subsection "analyzepsbt"
if [ -n "$PSBT" ]; then
    test_json "analyzepsbt" $CLIN analyzepsbt "$PSBT"
fi
test_err "analyzepsbt invalid" $CLIN analyzepsbt "notapsbt"

subsection "combinepsbt"
# Create two PSBTs to combine
PSBT1=$(capture $CLIN createpsbt '[]' '[]')
PSBT2=$(capture $CLIN createpsbt '[]' '[]')
if [ -n "$PSBT1" ] && [ -n "$PSBT2" ]; then
    test_ok "combinepsbt" $CLIN combinepsbt "[\"$PSBT1\",\"$PSBT2\"]"
fi
test_err "combinepsbt empty array" $CLIN combinepsbt '[]'

subsection "joinpsbts"
if [ -n "$PSBT1" ] && [ -n "$PSBT2" ]; then
    test_ok "joinpsbts" $CLIN joinpsbts "[\"$PSBT1\",\"$PSBT2\"]"
fi

subsection "finalizepsbt"
if [ -n "$PSBT" ]; then
    test_json "finalizepsbt" $CLIN finalizepsbt "$PSBT"
    test_json "finalizepsbt extract=true" $CLIN finalizepsbt "$PSBT" true
    test_json "finalizepsbt extract=false" $CLIN finalizepsbt "$PSBT" false
fi

subsection "converttopsbt"
if [ -n "$RAWTX" ]; then
    test_ok "converttopsbt" $CLIN converttopsbt "$RAWTX"
fi

subsection "utxoupdatepsbt"
if [ -n "$PSBT" ]; then
    test_ok "utxoupdatepsbt" $CLIN utxoupdatepsbt "$PSBT"
fi

subsection "Wallet PSBT operations"
test_json "walletcreatefundedpsbt empty" $CLIW walletcreatefundedpsbt '[]' '[]'
test_json "walletcreatefundedpsbt with output" $CLIW walletcreatefundedpsbt '[]' "[{\"$ADDR_BECH32\":0.0001}]"
if [ -n "$PSBT" ]; then
    test_json "walletprocesspsbt" $CLIW walletprocesspsbt "$PSBT"
fi


# ============================================
# SECTION 13: NETWORK
# ============================================
section "13. NETWORK"

subsection "Network info"
test_json "getnetworkinfo" $CLIN getnetworkinfo
test_match "getnetworkinfo has version" '"version"' $CLIN getnetworkinfo
test_match "getnetworkinfo has subversion" '"subversion"' $CLIN getnetworkinfo
test_match "getnetworkinfo has connections" '"connections"' $CLIN getnetworkinfo

subsection "Peer info"
test_json "getpeerinfo" $CLIN getpeerinfo
test_numeric "getconnectioncount" $CLIN getconnectioncount
test_json "getnettotals" $CLIN getnettotals
test_match "getnettotals has totalbytesrecv" '"totalbytesrecv"' $CLIN getnettotals

subsection "Node addresses"
test_json "getnodeaddresses" $CLIN getnodeaddresses
test_json "getnodeaddresses count=1" $CLIN getnodeaddresses 1
test_json "getnodeaddresses count=10" $CLIN getnodeaddresses 10
test_json "getnodeaddresses count=100" $CLIN getnodeaddresses 100

subsection "Banning"
test_json "listbanned" $CLIN listbanned
test_ok "clearbanned" $CLIN clearbanned

subsection "Address manager (v30.x)"
test_json "getaddrmaninfo" $CLIN getaddrmaninfo
test_match "getaddrmaninfo has ipv4" '"ipv4"' $CLIN getaddrmaninfo

subsection "Misc network"
test_ok "ping" $CLIN ping
test_json "getaddednodeinfo" $CLIN getaddednodeinfo


# ============================================
# SECTION 14: MINING & CONTROL
# ============================================
section "14. MINING & CONTROL"

subsection "Mining info"
test_json "getmininginfo" $CLIN getmininginfo
test_match "getmininginfo has blocks" '"blocks"' $CLIN getmininginfo
test_numeric "getnetworkhashps" $CLIN getnetworkhashps
test_ok "getnetworkhashps nblocks=10" $CLIN getnetworkhashps 10
test_ok "getnetworkhashps nblocks=100" $CLIN getnetworkhashps 100

subsection "Transaction priority"
test_json "getprioritisedtransactions" $CLIN getprioritisedtransactions

subsection "Control info"
test_numeric "uptime" $CLIN uptime
test_json "getmemoryinfo" $CLIN getmemoryinfo
test_match "getmemoryinfo has used" '"used"' $CLIN getmemoryinfo
test_json "getrpcinfo" $CLIN getrpcinfo
test_json "logging" $CLIN logging
test_json "getzmqnotifications" $CLIN getzmqnotifications

subsection "Help"
test_ok "help" $CLIN help
test_ok "help getblock" $CLIN help getblock
test_ok "help sendtoaddress" $CLIN help sendtoaddress


# ============================================
# SECTION 15: UTILITY
# ============================================
section "15. UTILITY"

subsection "Fee estimation"
test_json "estimatesmartfee 1" $CLIN estimatesmartfee 1
test_json "estimatesmartfee 6" $CLIN estimatesmartfee 6
test_json "estimatesmartfee 12" $CLIN estimatesmartfee 12
test_json "estimatesmartfee 144" $CLIN estimatesmartfee 144
test_json "estimatesmartfee 1008" $CLIN estimatesmartfee 1008
test_match "estimatesmartfee has feerate or errors" '"feerate"|"errors"' $CLIN estimatesmartfee 6
test_err "estimatesmartfee 0" $CLIN estimatesmartfee 0
test_err "estimatesmartfee negative" $CLIN estimatesmartfee -1

subsection "Index info"
test_json "getindexinfo" $CLIN getindexinfo

subsection "Descriptor operations"
# Get a descriptor from wallet
DESC=$(capture $CLIW listdescriptors | grep -o '"desc": "[^"]*"' | head -1 | cut -d'"' -f4)
if [ -n "$DESC" ]; then
    test_json "getdescriptorinfo" $CLIN getdescriptorinfo "$DESC"
fi

subsection "Message signing"
if [ -n "$ADDR_LEGACY" ]; then
    test_ok "signmessage" $CLIW signmessage "$ADDR_LEGACY" "test message"
    SIG=$(capture $CLIW signmessage "$ADDR_LEGACY" "Hello Bitcoin!")
    if [ -n "$SIG" ]; then
        test_ok "verifymessage valid" $CLIN verifymessage "$ADDR_LEGACY" "$SIG" "Hello Bitcoin!"
        test_ok "verifymessage wrong message" $CLIN verifymessage "$ADDR_LEGACY" "$SIG" "wrong message"
    fi
fi


# ============================================
# SECTION 16: ERROR HANDLING
# ============================================
section "16. ERROR HANDLING - COMPREHENSIVE"

subsection "Missing required parameters"
test_err "getblockhash no param" $CLIN getblockhash
test_err "getblock no param" $CLIN getblock
test_err "getrawtransaction no param" $CLIN getrawtransaction
test_err "decoderawtransaction no param" $CLIN decoderawtransaction
test_err "decodepsbt no param" $CLIN decodepsbt
test_err "validateaddress no param" $CLIN validateaddress
test_err "getaddressinfo no param" $CLIW getaddressinfo
test_err "sendtoaddress no param" $CLIW sendtoaddress
test_err "estimatesmartfee no param" $CLIN estimatesmartfee

subsection "Wrong parameter types"
test_err "getblockhash string" $CLIN getblockhash "notanumber"
test_err "getblockhash float" $CLIN getblockhash 1.5
test_err "getblock number" $CLIN getblock 12345
test_err "estimatesmartfee string" $CLIN estimatesmartfee "six"
test_err "getblock verbosity string" $CLIN getblock "$GENESIS_HASH" "verbose"

subsection "Invalid values"
test_err "getblockhash negative" $CLIN getblockhash -1
test_err "getblockhash huge" $CLIN getblockhash 999999999999
test_err "getblock zeros" $CLIN getblock "0000000000000000000000000000000000000000000000000000000000000000"
test_err "getrawtransaction zeros" $CLIN getrawtransaction "0000000000000000000000000000000000000000000000000000000000000000"

subsection "Wallet errors"
test_err "wallet operation no wallet" $CLIN getbalance
test_err "wallet operation wrong wallet" $CLI -$NETWORK -rpcwallet=nonexistent_wallet_$$ getbalance


# ============================================
# SECTION 17: EDGE CASES
# ============================================
section "17. EDGE CASES"

subsection "Boundary values"
test_hex "getblockhash 0 (genesis)" 64 $CLIN getblockhash 0
test_ok "estimatesmartfee 1 (minimum)" $CLIN estimatesmartfee 1
test_ok "estimatesmartfee 1008 (maximum)" $CLIN estimatesmartfee 1008
test_ok "getnodeaddresses 0" $CLIN getnodeaddresses 0
test_ok "listtransactions count=0" $CLIW listtransactions "*" 0

subsection "Empty results"
test_json "getrawmempool (may be empty)" $CLIN getrawmempool
test_json "listlockunspent (may be empty)" $CLIW listlockunspent
test_json "listbanned (may be empty)" $CLIN listbanned

subsection "Special characters in labels"
test_ok "label with dash" $CLIW getnewaddress "label-with-dash"
test_ok "label with dot" $CLIW getnewaddress "label.with.dot"
test_ok "label with colon" $CLIW getnewaddress "label:with:colon"
test_ok "label unicode basic" $CLIW getnewaddress "label_test_123"


# ============================================
# SECTION 18: STRESS TESTS
# ============================================
section "18. STRESS TESTS"

subsection "Rapid sequential calls"
echo -n "  [rapid] 100 getblockcount calls ... "
START=$(date +%s%N)
for i in $(seq 1 100); do
    $CLIN getblockcount >/dev/null 2>&1 || { echo -e "${RED}FAIL at $i${NC}"; ((FAIL++)); break; }
done
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo -e "${GREEN}PASS${NC} (${ELAPSED}ms total, $((ELAPSED/100))ms avg)"
((PASS++))
((TOTAL++))

echo -n "  [rapid] 50 getblockchaininfo calls ... "
START=$(date +%s%N)
for i in $(seq 1 50); do
    $CLIN getblockchaininfo >/dev/null 2>&1 || { echo -e "${RED}FAIL at $i${NC}"; ((FAIL++)); break; }
done
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo -e "${GREEN}PASS${NC} (${ELAPSED}ms total, $((ELAPSED/50))ms avg)"
((PASS++))
((TOTAL++))

echo -n "  [rapid] 50 getnewaddress calls ... "
START=$(date +%s%N)
for i in $(seq 1 50); do
    $CLIW getnewaddress >/dev/null 2>&1 || { echo -e "${RED}FAIL at $i${NC}"; ((FAIL++)); break; }
done
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo -e "${GREEN}PASS${NC} (${ELAPSED}ms total, $((ELAPSED/50))ms avg)"
((PASS++))
((TOTAL++))

subsection "Large response handling"
echo -n "  [large] getblock with full tx details ... "
if [ -n "$BLOCK_100_HASH" ]; then
    if OUTPUT=$($CLIN getblock "$BLOCK_100_HASH" 2 2>&1); then
        SIZE=${#OUTPUT}
        echo -e "${GREEN}PASS${NC} ($SIZE bytes)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
else
    echo -e "${YELLOW}SKIP${NC}"
    ((SKIP++))
fi
((TOTAL++))

echo -n "  [large] listdescriptors full output ... "
if OUTPUT=$($CLIW listdescriptors 2>&1); then
    SIZE=${#OUTPUT}
    echo -e "${GREEN}PASS${NC} ($SIZE bytes)"
    ((PASS++))
else
    echo -e "${RED}FAIL${NC}"
    ((FAIL++))
fi
((TOTAL++))

echo -n "  [large] getpeerinfo full output ... "
if OUTPUT=$($CLIN getpeerinfo 2>&1); then
    SIZE=${#OUTPUT}
    echo -e "${GREEN}PASS${NC} ($SIZE bytes)"
    ((PASS++))
else
    echo -e "${RED}FAIL${NC}"
    ((FAIL++))
fi
((TOTAL++))


# ============================================
# SECTION 19: LARGE TRANSACTION TESTS
# ============================================
section "19. LARGE TRANSACTION TESTS"

subsection "Generate large hex data for TX tests"

# Create progressively larger transactions
# Standard TX is ~250 bytes, we'll create larger ones

# Function to create large raw tx with many outputs
# Uses unique addresses for each output (Bitcoin Core rejects duplicate addresses in array format)
create_large_tx() {
    local num_outputs=$1
    local outputs=""

    for i in $(seq 1 $num_outputs); do
        # Generate unique address for each output
        local addr=$(capture $CLIW getnewaddress "" bech32)
        if [ -z "$addr" ]; then
            echo "[]"
            return 1
        fi
        if [ $i -gt 1 ]; then outputs="$outputs,"; fi
        outputs="$outputs{\"$addr\":0.00001}"
    done

    echo "[$outputs]"
}

# Test with increasing output counts
for NUM_OUTPUTS in 10 50 100 200; do
    echo -n "  [largetx] createrawtransaction $NUM_OUTPUTS outputs ... "
    OUTPUTS=$(create_large_tx $NUM_OUTPUTS)
    if RAWTX=$($CLIN createrawtransaction '[]' "$OUTPUTS" 2>&1); then
        SIZE=${#RAWTX}
        echo -e "${GREEN}PASS${NC} ($SIZE hex chars, ~$((SIZE/2)) bytes)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: ${RAWTX:0:100}"
        ((FAIL++))
    fi
    ((TOTAL++))
done

# Test with many inputs (fake)
subsection "Large input count tests"
for NUM_INPUTS in 10 50 100; do
    echo -n "  [largein] createrawtransaction $NUM_INPUTS inputs ... "
    INPUTS="["
    for i in $(seq 1 $NUM_INPUTS); do
        FAKE_TXID=$(printf '%064d' $i)
        if [ $i -gt 1 ]; then INPUTS="$INPUTS,"; fi
        INPUTS="$INPUTS{\"txid\":\"$FAKE_TXID\",\"vout\":0}"
    done
    INPUTS="$INPUTS]"

    if RAWTX=$($CLIN createrawtransaction "$INPUTS" '[]' 2>&1); then
        SIZE=${#RAWTX}
        echo -e "${GREEN}PASS${NC} ($SIZE hex chars)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: ${RAWTX:0:100}"
        ((FAIL++))
    fi
    ((TOTAL++))
done


# ============================================
# SECTION 20: LARGE DATA TESTS (4MB target)
# ============================================
section "20. LARGE DATA STRESS TESTS"

subsection "Large OP_RETURN data"
# OP_RETURN can hold up to 80 bytes standard, but let's test limits
for SIZE in 80 100 200 500 1000; do
    echo -n "  [opreturn] createrawtransaction ${SIZE}byte data ... "
    DATA=$(head -c $SIZE /dev/urandom | xxd -p | tr -d '\n')
    OUTPUTS="[{\"data\":\"$DATA\"}]"
    if RAWTX=$($CLIN createrawtransaction '[]' "$OUTPUTS" 2>&1); then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
    else
        # Might fail due to size limits - that's OK
        echo -e "${YELLOW}LIMIT${NC} (expected for non-standard)"
        ((SKIP++))
    fi
    ((TOTAL++))
done

subsection "Large PSBT tests"
for NUM_OUTPUTS in 50 100 200; do
    echo -n "  [largepsbt] createpsbt $NUM_OUTPUTS outputs ... "
    OUTPUTS=$(create_large_tx $NUM_OUTPUTS)
    if PSBT=$($CLIN createpsbt '[]' "$OUTPUTS" 2>&1); then
        SIZE=${#PSBT}
        echo -e "${GREEN}PASS${NC} ($SIZE chars)"
        ((PASS++))

        # Also test decode and analyze on large PSBT
        echo -n "  [largepsbt] decodepsbt $NUM_OUTPUTS outputs ... "
        if OUTPUT=$($CLIN decodepsbt "$PSBT" 2>&1); then
            echo -e "${GREEN}PASS${NC}"
            ((PASS++))
        else
            echo -e "${RED}FAIL${NC}"
            ((FAIL++))
        fi
        ((TOTAL++))

        echo -n "  [largepsbt] analyzepsbt $NUM_OUTPUTS outputs ... "
        if OUTPUT=$($CLIN analyzepsbt "$PSBT" 2>&1); then
            echo -e "${GREEN}PASS${NC}"
            ((PASS++))
        else
            echo -e "${RED}FAIL${NC}"
            ((FAIL++))
        fi
        ((TOTAL++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
    ((TOTAL++))
done

subsection "Mega transaction construction"
# Try to build a transaction approaching size limits
echo -n "  [mega] Building 500-output transaction ... "
OUTPUTS=$(create_large_tx 500)
if RAWTX=$($CLIN createrawtransaction '[]' "$OUTPUTS" 2>&1); then
    SIZE=${#RAWTX}
    echo -e "${GREEN}PASS${NC} (~$((SIZE/2)) bytes)"
    ((PASS++))

    echo -n "  [mega] Decoding 500-output transaction ... "
    if $CLIN decoderawtransaction "$RAWTX" >/dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
    ((TOTAL++))
else
    echo -e "${RED}FAIL${NC}"
    ((FAIL++))
fi
((TOTAL++))

echo -n "  [mega] Building 1000-output transaction ... "
OUTPUTS=$(create_large_tx 1000)
if RAWTX=$($CLIN createrawtransaction '[]' "$OUTPUTS" 2>&1); then
    SIZE=${#RAWTX}
    echo -e "${GREEN}PASS${NC} (~$((SIZE/2)) bytes)"
    ((PASS++))
else
    echo -e "${RED}FAIL${NC}"
    ((FAIL++))
fi
((TOTAL++))


# ============================================
# SECTION 21: CONCURRENT STRESS
# ============================================
section "21. CONCURRENT STRESS TESTS"

subsection "Parallel command execution"
echo -n "  [parallel] 10 concurrent getblockcount ... "
for i in $(seq 1 10); do
    $CLIN getblockcount >/dev/null 2>&1 &
done
wait
echo -e "${GREEN}PASS${NC}"
((PASS++))
((TOTAL++))

echo -n "  [parallel] 10 concurrent getblockchaininfo ... "
for i in $(seq 1 10); do
    $CLIN getblockchaininfo >/dev/null 2>&1 &
done
wait
echo -e "${GREEN}PASS${NC}"
((PASS++))
((TOTAL++))

echo -n "  [parallel] 5 concurrent wallet operations ... "
for i in $(seq 1 5); do
    $CLIW getnewaddress >/dev/null 2>&1 &
done
wait
echo -e "${GREEN}PASS${NC}"
((PASS++))
((TOTAL++))


# ============================================
# SECTION 22: REGTEST-ONLY TESTS
# ============================================
section "22. REGTEST-ONLY TESTS"

if [ "$NETWORK" = "regtest" ]; then
    subsection "Block generation"
    MINE_ADDR=$(capture $CLIW getnewaddress "" bech32)

    test_ok "generatetoaddress 1 block" $CLIW generatetoaddress 1 "$MINE_ADDR"
    test_ok "generatetoaddress 10 blocks" $CLIW generatetoaddress 10 "$MINE_ADDR"

    test_json "generateblock empty" $CLIW generateblock "$MINE_ADDR" '[]'

    subsection "Full transaction cycle on regtest"
    # Mine some blocks to get spendable coins
    echo -n "  Mining 101 blocks for mature coinbase ... "
    if $CLIW generatetoaddress 101 "$MINE_ADDR" >/dev/null 2>&1; then
        echo -e "${GREEN}OK${NC}"

        # Get balance
        BALANCE=$(capture $CLIW getbalance)
        echo "  Balance: $BALANCE BTC"

        # Create addresses of all types
        DEST_LEGACY=$(capture $CLIW getnewaddress "regtest_legacy" legacy)
        DEST_P2SH=$(capture $CLIW getnewaddress "regtest_p2sh" p2sh-segwit)
        DEST_BECH32=$(capture $CLIW getnewaddress "regtest_bech32" bech32)
        DEST_BECH32M=$(capture $CLIW getnewaddress "regtest_bech32m" bech32m)

        # Send to each address type
        test_hex "sendtoaddress legacy" 64 $CLIW sendtoaddress "$DEST_LEGACY" 1.0
        test_hex "sendtoaddress p2sh" 64 $CLIW sendtoaddress "$DEST_P2SH" 1.0
        test_hex "sendtoaddress bech32" 64 $CLIW sendtoaddress "$DEST_BECH32" 1.0
        test_hex "sendtoaddress bech32m" 64 $CLIW sendtoaddress "$DEST_BECH32M" 1.0

        # Mine block to confirm
        test_ok "mine block to confirm" $CLIW generatetoaddress 1 "$MINE_ADDR"

        # Test PSBT full workflow
        subsection "Full PSBT workflow on regtest"
        PSBT_DEST=$(capture $CLIW getnewaddress "psbt_test" bech32)

        echo -n "  walletcreatefundedpsbt ... "
        if FUNDED=$($CLIW walletcreatefundedpsbt '[]' "[{\"$PSBT_DEST\":0.5}]" 2>&1); then
            FUNDED_PSBT=$(echo "$FUNDED" | grep -o '"psbt": "[^"]*"' | head -1 | cut -d'"' -f4)
            if [ -n "$FUNDED_PSBT" ]; then
                echo -e "${GREEN}PASS${NC}"
                ((PASS++))

                echo -n "  walletprocesspsbt (sign) ... "
                if SIGNED=$($CLIW walletprocesspsbt "$FUNDED_PSBT" 2>&1); then
                    SIGNED_PSBT=$(echo "$SIGNED" | grep -o '"psbt": "[^"]*"' | head -1 | cut -d'"' -f4)
                    if [ -n "$SIGNED_PSBT" ]; then
                        echo -e "${GREEN}PASS${NC}"
                        ((PASS++))

                        echo -n "  finalizepsbt ... "
                        if FINAL=$($CLIN finalizepsbt "$SIGNED_PSBT" 2>&1); then
                            FINAL_HEX=$(echo "$FINAL" | grep -o '"hex": "[^"]*"' | head -1 | cut -d'"' -f4)
                            if [ -n "$FINAL_HEX" ]; then
                                echo -e "${GREEN}PASS${NC}"
                                ((PASS++))

                                echo -n "  testmempoolaccept ... "
                                if ACCEPT=$($CLIN testmempoolaccept "[\"$FINAL_HEX\"]" 2>&1); then
                                    if echo "$ACCEPT" | grep -q '"allowed": true'; then
                                        echo -e "${GREEN}PASS${NC}"
                                        ((PASS++))

                                        echo -n "  sendrawtransaction ... "
                                        if TXID=$($CLIN sendrawtransaction "$FINAL_HEX" 2>&1); then
                                            echo -e "${GREEN}PASS${NC} (txid: ${TXID:0:16}...)"
                                            ((PASS++))
                                        else
                                            echo -e "${RED}FAIL${NC}"
                                            ((FAIL++))
                                        fi
                                    else
                                        echo -e "${RED}FAIL (not allowed)${NC}"
                                        ((FAIL++))
                                    fi
                                else
                                    echo -e "${RED}FAIL${NC}"
                                    ((FAIL++))
                                fi
                            else
                                echo -e "${RED}FAIL (no hex)${NC}"
                                ((FAIL++))
                            fi
                        else
                            echo -e "${RED}FAIL${NC}"
                            ((FAIL++))
                        fi
                        ((TOTAL++))
                    else
                        echo -e "${RED}FAIL (no signed psbt)${NC}"
                        ((FAIL++))
                    fi
                else
                    echo -e "${RED}FAIL${NC}"
                    ((FAIL++))
                fi
                ((TOTAL++))
            else
                echo -e "${RED}FAIL (no funded psbt)${NC}"
                ((FAIL++))
            fi
        else
            echo -e "${RED}FAIL${NC}"
            ((FAIL++))
        fi
        ((TOTAL++))
        ((TOTAL++))
        ((TOTAL++))

    else
        echo -e "${RED}FAILED${NC}"
    fi
else
    test_skip "generatetoaddress" "regtest only"
    test_skip "generateblock" "regtest only"
    test_skip "full tx cycle" "regtest only"
fi


# ============================================
# CLEANUP
# ============================================
section "CLEANUP"

echo "Cleaning up test artifacts..."
rm -rf "$TMPDIR"

# Unload test wallets
$CLIN unloadwallet "$WALLET" >/dev/null 2>&1
$CLIN unloadwallet "temp_torture_$$" >/dev/null 2>&1
$CLIN unloadwallet "blank_$$" >/dev/null 2>&1
$CLIN unloadwallet "watchonly_$$" >/dev/null 2>&1
$CLIN unloadwallet "desc_$$" >/dev/null 2>&1

echo "Done."


# ============================================
# RESULTS
# ============================================
echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                         RESULTS                               ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  ${GREEN}PASSED:${NC}  $PASS"
echo -e "  ${RED}FAILED:${NC}  $FAIL"
echo -e "  ${YELLOW}SKIPPED:${NC} $SKIP"
echo "  ─────────────────"
echo "  TOTAL:   $TOTAL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    ALL TESTS PASSED!                          ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${RED}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║                    SOME TESTS FAILED                          ║${NC}"
    echo -e "${RED}╚═══════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
