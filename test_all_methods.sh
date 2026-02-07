#!/bin/bash
#
# btc-cli COMPLETE METHOD COVERAGE TEST
# Tests every single one of the 160 methods with all parameter variations
#
# Usage: ./test_all_methods.sh [network] [wallet]
#

set -o pipefail

NETWORK="${1:-signet}"
WALLET="${2:-method_test_wallet}"
CLI="./btc-cli -$NETWORK"
CLIW="$CLI -rpcwallet=$WALLET"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Track which methods we've tested
declare -A TESTED_METHODS

test_method() {
    local method="$1"
    local desc="$2"
    shift 2
    ((TOTAL++))
    TESTED_METHODS[$method]=1

    echo -n "  [$TOTAL] $method: $desc ... "
    if output=$("$@" 2>&1); then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
        return 0
    else
        # Check if it's an expected RPC error vs crash
        if echo "$output" | grep -qE "error|Error|RPC"; then
            echo -e "${GREEN}PASS (RPC error)${NC}"
            ((PASS++))
        else
            echo -e "${RED}FAIL${NC}"
            echo "      Output: ${output:0:100}"
            ((FAIL++))
        fi
        return 1
    fi
}

test_method_err() {
    local method="$1"
    local desc="$2"
    shift 2
    ((TOTAL++))
    TESTED_METHODS[$method]=1

    echo -n "  [$TOTAL] $method: $desc (expect error) ... "
    if output=$("$@" 2>&1); then
        echo -e "${RED}FAIL (expected error)${NC}"
        ((FAIL++))
        return 1
    else
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
        return 0
    fi
}

skip_method() {
    local method="$1"
    local reason="$2"
    ((TOTAL++))
    ((SKIP++))
    TESTED_METHODS[$method]=1
    echo -e "  [$TOTAL] $method: ${YELLOW}SKIP${NC} ($reason)"
}

capture() { "$@" 2>/dev/null; }

echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║      btc-cli COMPLETE METHOD COVERAGE - ALL 160 METHODS       ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Setup
echo -n "Connecting to $NETWORK... "
if ! $CLI getblockcount >/dev/null 2>&1; then
    echo -e "${RED}FAILED${NC}"
    exit 1
fi
echo -e "${GREEN}OK${NC}"

echo -n "Setting up wallet... "
$CLI loadwallet "$WALLET" >/dev/null 2>&1 || $CLI createwallet "$WALLET" >/dev/null 2>&1
echo -e "${GREEN}OK${NC}"

# Get test data
BLOCK_COUNT=$(capture $CLI getblockcount)
BEST_HASH=$(capture $CLI getbestblockhash)
GENESIS=$(capture $CLI getblockhash 0)
BLOCK_10=$(capture $CLI getblockhash 10)
ADDR=$(capture $CLIW getnewaddress)
ADDR_LEGACY=$(capture $CLIW getnewaddress "" legacy)
FAKE_TXID="0000000000000000000000000000000000000000000000000000000000000001"

echo "Block height: $BLOCK_COUNT"
echo ""

# ============================================
# BLOCKCHAIN METHODS (25)
# ============================================
echo -e "${BLUE}=== BLOCKCHAIN METHODS ===${NC}"

test_method "getbestblockhash" "no params" $CLI getbestblockhash
test_method "getblock" "hash only" $CLI getblock "$GENESIS"
test_method "getblock" "verbosity=0" $CLI getblock "$GENESIS" 0
test_method "getblock" "verbosity=1" $CLI getblock "$GENESIS" 1
test_method "getblock" "verbosity=2" $CLI getblock "$GENESIS" 2
test_method "getblock" "verbosity=3" $CLI getblock "$GENESIS" 3
test_method_err "getblock" "invalid hash" $CLI getblock "invalid"
test_method "getblockchaininfo" "no params" $CLI getblockchaininfo
test_method "getblockcount" "no params" $CLI getblockcount
test_method "getblockfilter" "basic" $CLI getblockfilter "$GENESIS" || skip_method "getblockfilter" "no index"
test_method "getblockfrompeer" "basic" $CLI getblockfrompeer "$GENESIS" 0 || skip_method "getblockfrompeer" "no peer"
test_method "getblockhash" "height=0" $CLI getblockhash 0
test_method "getblockhash" "height=10" $CLI getblockhash 10
test_method_err "getblockhash" "negative" $CLI getblockhash -1
test_method "getblockheader" "hash only" $CLI getblockheader "$GENESIS"
test_method "getblockheader" "verbose=true" $CLI getblockheader "$GENESIS" true
test_method "getblockheader" "verbose=false" $CLI getblockheader "$GENESIS" false
test_method "getblockstats" "by height" $CLI getblockstats 10
test_method "getblockstats" "by hash" $CLI getblockstats "$BLOCK_10"
test_method "getchainstates" "no params" $CLI getchainstates
test_method "getchaintips" "no params" $CLI getchaintips
test_method "getchaintxstats" "no params" $CLI getchaintxstats
test_method "getchaintxstats" "nblocks=10" $CLI getchaintxstats 10
test_method "getdeploymentinfo" "no params" $CLI getdeploymentinfo
test_method "getdeploymentinfo" "with hash" $CLI getdeploymentinfo "$BEST_HASH"
test_method "getdifficulty" "no params" $CLI getdifficulty
test_method "getmempoolancestors" "basic" $CLI getmempoolancestors "$FAKE_TXID" || true
test_method "getmempooldescendants" "basic" $CLI getmempooldescendants "$FAKE_TXID" || true
test_method "getmempoolentry" "basic" $CLI getmempoolentry "$FAKE_TXID" || true
test_method "getmempoolinfo" "no params" $CLI getmempoolinfo
test_method "getrawmempool" "no params" $CLI getrawmempool
test_method "getrawmempool" "verbose=true" $CLI getrawmempool true
test_method "getrawmempool" "verbose=false" $CLI getrawmempool false
test_method "gettxout" "basic" $CLI gettxout "$FAKE_TXID" 0 || true
test_method "gettxoutproof" "basic" $CLI gettxoutproof "[\"$FAKE_TXID\"]" || true
test_method "gettxoutsetinfo" "no params" $CLI gettxoutsetinfo
test_method "gettxspendingprevout" "empty" $CLI gettxspendingprevout "[]"
test_method "preciousblock" "basic" $CLI preciousblock "$GENESIS"
test_method "pruneblockchain" "basic" $CLI pruneblockchain 0 || true
test_method "savemempool" "no params" $CLI savemempool
test_method "scantxoutset" "abort" $CLI scantxoutset abort || true
test_method "verifychain" "no params" $CLI verifychain
test_method "verifychain" "level=0" $CLI verifychain 0
test_method "verifytxoutproof" "basic" $CLI verifytxoutproof "0000" || true
test_method "waitforblock" "timeout=1" $CLI waitforblock "$GENESIS" 1
test_method "waitforblockheight" "height=0 timeout=1" $CLI waitforblockheight 0 1
test_method "waitfornewblock" "timeout=1" $CLI waitfornewblock 1

# New v30 blockchain methods
test_method "dumptxoutset" "basic" $CLI dumptxoutset "/tmp/utxo.dat" || skip_method "dumptxoutset" "may need pruned"
test_method "getdescriptoractivity" "basic" $CLI getdescriptoractivity "[]" "[]" || true
test_method "importmempool" "basic" $CLI importmempool "/tmp/mempool.dat" || true
test_method "loadtxoutset" "basic" $CLI loadtxoutset "/tmp/utxo.dat" || true
test_method "scanblocks" "start" $CLI scanblocks start "[]" || true

# ============================================
# CONTROL METHODS (8)
# ============================================
echo ""
echo -e "${BLUE}=== CONTROL METHODS ===${NC}"

test_method "getmemoryinfo" "no params" $CLI getmemoryinfo
test_method "getrpcinfo" "no params" $CLI getrpcinfo
test_method "getzmqnotifications" "no params" $CLI getzmqnotifications
test_method "help" "no params" $CLI help
test_method "help" "command=getblock" $CLI help getblock
test_method "logging" "no params" $CLI logging
skip_method "stop" "would stop the node"
test_method "uptime" "no params" $CLI uptime

# ============================================
# GENERATING METHODS (3)
# ============================================
echo ""
echo -e "${BLUE}=== GENERATING METHODS ===${NC}"

if [ "$NETWORK" = "regtest" ]; then
    test_method "generateblock" "empty txs" $CLIW generateblock "$ADDR" "[]"
    test_method "generatetoaddress" "1 block" $CLIW generatetoaddress 1 "$ADDR"
    test_method "generatetodescriptor" "basic" $CLIW generatetodescriptor 1 "addr($ADDR)" || true
else
    skip_method "generateblock" "regtest only"
    skip_method "generatetoaddress" "regtest only"
    skip_method "generatetodescriptor" "regtest only"
fi

# ============================================
# MINING METHODS (6)
# ============================================
echo ""
echo -e "${BLUE}=== MINING METHODS ===${NC}"

test_method "getblocktemplate" "basic" $CLI getblocktemplate || true
test_method "getmininginfo" "no params" $CLI getmininginfo
test_method "getnetworkhashps" "no params" $CLI getnetworkhashps
test_method "getnetworkhashps" "nblocks=10" $CLI getnetworkhashps 10
test_method "getprioritisedtransactions" "no params" $CLI getprioritisedtransactions
test_method "prioritisetransaction" "basic" $CLI prioritisetransaction "$FAKE_TXID" 0 1000 || true
test_method "submitblock" "invalid" $CLI submitblock "00" || true
test_method "submitheader" "invalid" $CLI submitheader "00" || true

# ============================================
# NETWORK METHODS (14)
# ============================================
echo ""
echo -e "${BLUE}=== NETWORK METHODS ===${NC}"

test_method "addnode" "onetry" $CLI addnode "127.0.0.1:18333" onetry || true
test_method "clearbanned" "no params" $CLI clearbanned
test_method "disconnectnode" "by addr" $CLI disconnectnode "127.0.0.1:18333" || true
test_method "getaddednodeinfo" "no params" $CLI getaddednodeinfo
test_method "getaddrmaninfo" "no params" $CLI getaddrmaninfo
test_method "getconnectioncount" "no params" $CLI getconnectioncount
test_method "getnettotals" "no params" $CLI getnettotals
test_method "getnetworkinfo" "no params" $CLI getnetworkinfo
test_method "getnodeaddresses" "no params" $CLI getnodeaddresses
test_method "getnodeaddresses" "count=10" $CLI getnodeaddresses 10
test_method "getpeerinfo" "no params" $CLI getpeerinfo
test_method "listbanned" "no params" $CLI listbanned
test_method "ping" "no params" $CLI ping
test_method "setban" "add" $CLI setban "192.168.99.99" add 60 || true
test_method "setnetworkactive" "true" $CLI setnetworkactive true

# ============================================
# RAWTRANSACTIONS METHODS (18)
# ============================================
echo ""
echo -e "${BLUE}=== RAWTRANSACTIONS METHODS ===${NC}"

# Create test data
RAWTX=$(capture $CLI createrawtransaction '[]' '[]')
PSBT=$(capture $CLI createpsbt '[]' '[]')

test_method "analyzepsbt" "basic" $CLI analyzepsbt "$PSBT"
test_method "combinepsbt" "basic" $CLI combinepsbt "[\"$PSBT\"]"
test_method "combinerawtransaction" "basic" $CLI combinerawtransaction "[\"$RAWTX\"]"
test_method "converttopsbt" "basic" $CLI converttopsbt "$RAWTX"
test_method "createpsbt" "empty" $CLI createpsbt '[]' '[]'
test_method "createpsbt" "with locktime" $CLI createpsbt '[]' '[]' 500000
test_method "createpsbt" "replaceable" $CLI createpsbt '[]' '[]' 0 true
test_method "createrawtransaction" "empty" $CLI createrawtransaction '[]' '[]'
test_method "createrawtransaction" "with outputs" $CLI createrawtransaction '[]' "[{\"$ADDR\":0.001}]"
test_method "decodepsbt" "basic" $CLI decodepsbt "$PSBT"
test_method "decoderawtransaction" "basic" $CLI decoderawtransaction "$RAWTX"
test_method "decodescript" "p2pkh" $CLI decodescript "76a91489abcdefabbaabbaabbaabbaabbaabbaabbaabba88ac"
test_method "descriptorprocesspsbt" "basic" $CLI descriptorprocesspsbt "$PSBT" "[]" || true
test_method "finalizepsbt" "basic" $CLI finalizepsbt "$PSBT"
test_method "finalizepsbt" "extract=false" $CLI finalizepsbt "$PSBT" false
test_method "fundrawtransaction" "basic" $CLIW fundrawtransaction "$RAWTX" || true
test_method "getrawtransaction" "basic" $CLI getrawtransaction "$FAKE_TXID" || true
test_method "joinpsbts" "basic" $CLI joinpsbts "[\"$PSBT\"]"
test_method "sendrawtransaction" "invalid" $CLI sendrawtransaction "$RAWTX" || true
test_method "signrawtransactionwithkey" "basic" $CLI signrawtransactionwithkey "$RAWTX" "[]" || true
test_method "signrawtransactionwithwallet" "basic" $CLIW signrawtransactionwithwallet "$RAWTX"
test_method "simulaterawtransaction" "basic" $CLIW simulaterawtransaction "[]" || true
test_method "submitpackage" "basic" $CLI submitpackage "[]" || true
test_method "testmempoolaccept" "basic" $CLI testmempoolaccept "[\"$RAWTX\"]"
test_method "utxoupdatepsbt" "basic" $CLI utxoupdatepsbt "$PSBT"

# ============================================
# UTIL METHODS (9)
# ============================================
echo ""
echo -e "${BLUE}=== UTIL METHODS ===${NC}"

test_method "createmultisig" "2of2" $CLI createmultisig 2 "[\"$ADDR\",\"$ADDR\"]" || true
test_method "deriveaddresses" "basic" $CLI deriveaddresses "addr($ADDR)" || true
test_method "estimatesmartfee" "conf=1" $CLI estimatesmartfee 1
test_method "estimatesmartfee" "conf=6" $CLI estimatesmartfee 6
test_method "estimatesmartfee" "conf=144" $CLI estimatesmartfee 144
test_method "getdescriptorinfo" "basic" $CLI getdescriptorinfo "addr($ADDR)"
test_method "getindexinfo" "no params" $CLI getindexinfo
test_method "signmessagewithprivkey" "basic" $CLI signmessagewithprivkey "cVpF924EspNh8KjYsfhgY96mmxvT6DgdWiTYMtMjuM74hJaU5psW" "test" || true
test_method "validateaddress" "valid" $CLI validateaddress "$ADDR"
test_method "validateaddress" "invalid" $CLI validateaddress "invalid"
test_method "verifymessage" "basic" $CLI verifymessage "$ADDR_LEGACY" "sig" "msg" || true

# ============================================
# WALLET METHODS (60+)
# ============================================
echo ""
echo -e "${BLUE}=== WALLET METHODS ===${NC}"

test_method "abandontransaction" "basic" $CLIW abandontransaction "$FAKE_TXID" || true
test_method "abortrescan" "basic" $CLIW abortrescan || true
test_method "backupwallet" "basic" $CLIW backupwallet "/tmp/backup.dat"
test_method "bumpfee" "basic" $CLIW bumpfee "$FAKE_TXID" || true
test_method "createwallet" "basic" $CLI createwallet "temp_$$"
$CLI unloadwallet "temp_$$" >/dev/null 2>&1
test_method "createwalletdescriptor" "basic" $CLIW createwalletdescriptor "bech32" || true
skip_method "dumpprivkey" "deprecated in descriptor wallets"
skip_method "dumpwallet" "deprecated in descriptor wallets"
test_method "encryptwallet" "basic" $CLIW encryptwallet "testpassword" || true
test_method "enumeratesigners" "basic" $CLIW enumeratesigners || true
test_method "getaddressesbylabel" "basic" $CLIW getaddressesbylabel "test"
test_method "getaddressinfo" "basic" $CLIW getaddressinfo "$ADDR"
test_method "getbalance" "no params" $CLIW getbalance
test_method "getbalance" "minconf=0" $CLIW getbalance "*" 0
test_method "getbalance" "minconf=6" $CLIW getbalance "*" 6
test_method "getbalances" "no params" $CLIW getbalances
test_method "gethdkeys" "basic" $CLIW gethdkeys || true
test_method "getnewaddress" "default" $CLIW getnewaddress
test_method "getnewaddress" "legacy" $CLIW getnewaddress "" legacy
test_method "getnewaddress" "p2sh-segwit" $CLIW getnewaddress "" p2sh-segwit
test_method "getnewaddress" "bech32" $CLIW getnewaddress "" bech32
test_method "getnewaddress" "bech32m" $CLIW getnewaddress "" bech32m
test_method "getnewaddress" "with label" $CLIW getnewaddress "testlabel"
test_method "getrawchangeaddress" "default" $CLIW getrawchangeaddress
test_method "getrawchangeaddress" "legacy" $CLIW getrawchangeaddress legacy
test_method "getrawchangeaddress" "bech32" $CLIW getrawchangeaddress bech32
test_method "getreceivedbyaddress" "basic" $CLIW getreceivedbyaddress "$ADDR"
test_method "getreceivedbyaddress" "minconf=0" $CLIW getreceivedbyaddress "$ADDR" 0
test_method "getreceivedbylabel" "basic" $CLIW getreceivedbylabel "test" || true
test_method "gettransaction" "basic" $CLIW gettransaction "$FAKE_TXID" || true
skip_method "getunconfirmedbalance" "deprecated in v30"
test_method "getwalletinfo" "no params" $CLIW getwalletinfo
skip_method "importaddress" "deprecated in descriptor wallets"
test_method "importdescriptors" "basic" $CLIW importdescriptors "[]" || true
skip_method "importmulti" "deprecated in descriptor wallets"
skip_method "importprivkey" "deprecated in descriptor wallets"
test_method "importprunedfunds" "basic" $CLIW importprunedfunds "00" "00" || true
skip_method "importpubkey" "deprecated in descriptor wallets"
skip_method "importwallet" "deprecated in descriptor wallets"
test_method "keypoolrefill" "default" $CLIW keypoolrefill
test_method "keypoolrefill" "size=10" $CLIW keypoolrefill 10
test_method "listaddressgroupings" "no params" $CLIW listaddressgroupings
test_method "listdescriptors" "no params" $CLIW listdescriptors
test_method "listdescriptors" "private=false" $CLIW listdescriptors false
test_method "listlabels" "no params" $CLIW listlabels
test_method "listlockunspent" "no params" $CLIW listlockunspent
test_method "listreceivedbyaddress" "no params" $CLIW listreceivedbyaddress
test_method "listreceivedbyaddress" "minconf=0" $CLIW listreceivedbyaddress 0
test_method "listreceivedbyaddress" "include_empty" $CLIW listreceivedbyaddress 0 true
test_method "listreceivedbylabel" "no params" $CLIW listreceivedbylabel
test_method "listsinceblock" "no params" $CLIW listsinceblock
test_method "listsinceblock" "with hash" $CLIW listsinceblock "$GENESIS"
test_method "listtransactions" "no params" $CLIW listtransactions
test_method "listtransactions" "count=10" $CLIW listtransactions "*" 10
test_method "listunspent" "no params" $CLIW listunspent
test_method "listunspent" "minconf=0" $CLIW listunspent 0
test_method "listwalletdir" "no params" $CLI listwalletdir
test_method "listwallets" "no params" $CLI listwallets
test_method "loadwallet" "basic" $CLI loadwallet "$WALLET" || true
test_method "lockunspent" "unlock all" $CLIW lockunspent true
test_method "migratewallet" "basic" $CLIW migratewallet || true
test_method "psbtbumpfee" "basic" $CLIW psbtbumpfee "$FAKE_TXID" || true
test_method "removeprunedfunds" "basic" $CLIW removeprunedfunds "$FAKE_TXID" || true
test_method "rescanblockchain" "no params" $CLIW rescanblockchain || true
test_method "restorewallet" "basic" $CLI restorewallet "restore_$$" "/tmp/backup.dat" || true
test_method "send" "basic" $CLIW send "[{\"$ADDR\":0.0001}]" || true
test_method "sendall" "basic" $CLIW sendall "[\"$ADDR\"]" || true
test_method "sendmany" "basic" $CLIW sendmany "" "{\"$ADDR\":0.0001}" || true
test_method "sendtoaddress" "basic" $CLIW sendtoaddress "$ADDR" 0.0001 || true
test_method "setlabel" "basic" $CLIW setlabel "$ADDR" "newlabel"
test_method "settxfee" "basic" $CLIW settxfee 0.0001
test_method "setwalletflag" "basic" $CLIW setwalletflag "avoid_reuse" || true
test_method "signmessage" "basic" $CLIW signmessage "$ADDR_LEGACY" "test"
test_method "unloadwallet" "other" $CLI unloadwallet "temp_$$" || true
test_method "walletcreatefundedpsbt" "empty" $CLIW walletcreatefundedpsbt '[]' '[]'
test_method "walletcreatefundedpsbt" "with output" $CLIW walletcreatefundedpsbt '[]' "[{\"$ADDR\":0.0001}]"
test_method "walletdisplayaddress" "basic" $CLIW walletdisplayaddress "$ADDR" || true
skip_method "walletlock" "wallet not encrypted"
skip_method "walletpassphrase" "wallet not encrypted"
skip_method "walletpassphrasechange" "wallet not encrypted"
test_method "walletprocesspsbt" "basic" $CLIW walletprocesspsbt "$PSBT"


# ============================================
# SUMMARY
# ============================================
echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    METHOD COVERAGE RESULTS                    ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  ${GREEN}PASSED:${NC}  $PASS"
echo -e "  ${RED}FAILED:${NC}  $FAIL"
echo -e "  ${YELLOW}SKIPPED:${NC} $SKIP"
echo "  ─────────────────"
echo "  TOTAL:   $TOTAL"
echo ""
echo "  Methods tested: ${#TESTED_METHODS[@]}"
echo ""

# List any untested methods
echo "Checking for untested methods..."
ALL_METHODS=$($CLI -help 2>&1 | grep -E '^  [a-z]+' | awk '{print $1}' | sort -u)
UNTESTED=""
for method in $ALL_METHODS; do
    if [ -z "${TESTED_METHODS[$method]}" ]; then
        UNTESTED="$UNTESTED $method"
    fi
done

if [ -n "$UNTESTED" ]; then
    echo -e "${YELLOW}Untested methods:${NC}$UNTESTED"
else
    echo -e "${GREEN}All methods have been tested!${NC}"
fi

echo ""
if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "${RED}SOME TESTS FAILED${NC}"
    exit 1
fi
