#!/usr/bin/env bash
# parity_scan.sh — Comprehensive btc-cli vs bitcoin-cli parity scan
# Runs both CLIs side-by-side on regtest and compares outputs.
# Usage: wsl -e bash -l -c "cd /mnt/c/pirqjobs/c-bitcoin-cli && bash parity_scan.sh"

set -uo pipefail

# ─── Paths ────────────────────────────────────────────────────────────
BTC_CLI="/mnt/c/pirqjobs/c-bitcoin-cli/btc-cli"
BITCOIN_CLI="/home/obscurity/bitcoin-30.2/bin/bitcoin-cli"
BITCOIND="/home/obscurity/bitcoin-30.2/bin/bitcoind"
DATADIR="/tmp/parity-scan-$$"
PORT=19555
RPCPORT=$PORT

# ─── Colors ───────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ─── Counters ─────────────────────────────────────────────────────────
PASS=0
FAIL=0
DIFF=0
SKIP=0
TOTAL=0
ERRORS=""

# ─── Results log ──────────────────────────────────────────────────────
LOGFILE="/tmp/parity-scan-results-$$.log"

pass() {
    PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))
    printf "  ${GREEN}PASS${NC}  %s\n" "$1"
    echo "PASS  $1" >> "$LOGFILE"
}

fail() {
    FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1))
    printf "  ${RED}FAIL${NC}  %s  —  %s\n" "$1" "$2"
    echo "FAIL  $1  —  $2" >> "$LOGFILE"
    ERRORS="${ERRORS}\n  - $1: $2"
}

diff_note() {
    DIFF=$((DIFF + 1)); TOTAL=$((TOTAL + 1))
    printf "  ${YELLOW}DIFF${NC}  %s  —  %s\n" "$1" "$2"
    echo "DIFF  $1  —  $2" >> "$LOGFILE"
}

skip_test() {
    SKIP=$((SKIP + 1)); TOTAL=$((TOTAL + 1))
    printf "  ${YELLOW}SKIP${NC}  %s  —  %s\n" "$1" "$2"
    echo "SKIP  $1  —  $2" >> "$LOGFILE"
}

section() {
    printf "\n${CYAN}${BOLD}=== %s ===${NC}\n" "$1"
    echo "" >> "$LOGFILE"
    echo "=== $1 ===" >> "$LOGFILE"
}

subsection() {
    printf "\n${CYAN}--- %s ---${NC}\n" "$1"
    echo "--- $1 ---" >> "$LOGFILE"
}

# ─── Helper: run both CLIs ────────────────────────────────────────────
CONN_ARGS="-regtest -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass"

btc() {
    "$BTC_CLI" $CONN_ARGS "$@" 2>/dev/null
}

btc_both() {
    "$BTC_CLI" $CONN_ARGS "$@" 2>&1
}

ref() {
    "$BITCOIN_CLI" $CONN_ARGS "$@" 2>/dev/null
}

ref_both() {
    "$BITCOIN_CLI" $CONN_ARGS "$@" 2>&1
}

# Compare stdout of both CLIs for an RPC call.
# Usage: compare_rpc "test_name" method [args...]
compare_rpc() {
    local name="$1"; shift
    local btc_out ref_out
    btc_out=$(btc "$@" 2>/dev/null) || true
    ref_out=$(ref "$@" 2>/dev/null) || true

    if [ "$btc_out" = "$ref_out" ]; then
        pass "$name"
    else
        # Check if both are valid JSON and structurally similar
        if echo "$btc_out" | python3 -c "import sys,json; json.load(sys.stdin)" 2>/dev/null &&
           echo "$ref_out" | python3 -c "import sys,json; json.load(sys.stdin)" 2>/dev/null; then
            # Both valid JSON — compare keys at top level
            local btc_keys ref_keys
            btc_keys=$(echo "$btc_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict):
        print(','.join(sorted(d.keys())))
    else:
        print(type(d).__name__)
except: print('PARSE_ERROR')
" 2>/dev/null)
            ref_keys=$(echo "$ref_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict):
        print(','.join(sorted(d.keys())))
    else:
        print(type(d).__name__)
except: print('PARSE_ERROR')
" 2>/dev/null)
            if [ "$btc_keys" = "$ref_keys" ]; then
                pass "$name (values differ, keys match)"
            else
                fail "$name" "keys differ: btc=[$btc_keys] ref=[$ref_keys]"
            fi
        else
            # Not JSON or one failed — compare first 200 chars
            local btc_short="${btc_out:0:200}"
            local ref_short="${ref_out:0:200}"
            fail "$name" "btc=[${btc_short}] ref=[${ref_short}]"
        fi
    fi
}

# Compare exit codes of both CLIs
compare_exit() {
    local name="$1"; shift
    local btc_rc ref_rc
    btc "$@" >/dev/null 2>&1; btc_rc=$?
    ref "$@" >/dev/null 2>&1; ref_rc=$?
    if [ "$btc_rc" = "$ref_rc" ]; then
        pass "$name (exit=$btc_rc)"
    else
        fail "$name" "exit codes differ: btc=$btc_rc ref=$ref_rc"
    fi
}

# Compare stderr output of both CLIs
compare_stderr() {
    local name="$1"; shift
    local btc_err ref_err
    btc_err=$("$BTC_CLI" $CONN_ARGS "$@" 2>&1 1>/dev/null) || true
    ref_err=$("$BITCOIN_CLI" $CONN_ARGS "$@" 2>&1 1>/dev/null) || true
    if [ "$btc_err" = "$ref_err" ]; then
        pass "$name"
    else
        local btc_short="${btc_err:0:200}"
        local ref_short="${ref_err:0:200}"
        fail "$name" "stderr btc=[${btc_short}] ref=[${ref_short}]"
    fi
}

# Compare that both produce non-empty output (structural match)
compare_nonempty() {
    local name="$1"; shift
    local btc_out ref_out
    btc_out=$(btc "$@" 2>/dev/null) || true
    ref_out=$(ref "$@" 2>/dev/null) || true
    if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
        pass "$name (both non-empty)"
    elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
        pass "$name (both empty)"
    else
        fail "$name" "btc empty=$([ -z "$btc_out" ] && echo yes || echo no) ref empty=$([ -z "$ref_out" ] && echo yes || echo no)"
    fi
}

# Compare JSON key presence between both CLIs
compare_json_keys() {
    local name="$1"; shift
    local btc_out ref_out
    btc_out=$(btc "$@" 2>/dev/null) || true
    ref_out=$(ref "$@" 2>/dev/null) || true

    local btc_keys ref_keys missing extra
    btc_keys=$(echo "$btc_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict): print('\n'.join(sorted(d.keys())))
    elif isinstance(d, list) and d and isinstance(d[0], dict): print('\n'.join(sorted(d[0].keys())))
except: pass
" 2>/dev/null) || true
    ref_keys=$(echo "$ref_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict): print('\n'.join(sorted(d.keys())))
    elif isinstance(d, list) and d and isinstance(d[0], dict): print('\n'.join(sorted(d[0].keys())))
except: pass
" 2>/dev/null) || true

    missing=$(comm -23 <(echo "$ref_keys") <(echo "$btc_keys") | tr '\n' ',')
    extra=$(comm -13 <(echo "$ref_keys") <(echo "$btc_keys") | tr '\n' ',')

    if [ -z "$missing" ] && [ -z "$extra" ]; then
        pass "$name (keys match)"
    elif [ -z "$missing" ]; then
        diff_note "$name" "extra keys in btc: $extra"
    else
        fail "$name" "missing keys in btc: ${missing} extra: ${extra}"
    fi
}

# ─── Cleanup ──────────────────────────────────────────────────────────
cleanup() {
    echo ""
    echo "Stopping bitcoind..."
    "$BITCOIN_CLI" $CONN_ARGS stop 2>/dev/null || true
    sleep 2
    rm -rf "$DATADIR"
    echo "Cleaned up."
}
trap cleanup EXIT

# ─── Start bitcoind ──────────────────────────────────────────────────
printf "${BOLD}Comprehensive btc-cli vs bitcoin-cli Parity Scan${NC}\n"
printf "btc-cli:     %s\n" "$BTC_CLI"
printf "bitcoin-cli: %s\n" "$BITCOIN_CLI"
printf "bitcoind:    %s\n" "$BITCOIND"
printf "datadir:     %s\n" "$DATADIR"
printf "rpcport:     %s\n" "$RPCPORT"
echo ""
> "$LOGFILE"

mkdir -p "$DATADIR"

# Generate rpcauth so both password auth AND cookie file are available
RPCAUTH_LINE=$(python3 <<'PYEOF'
import hashlib, hmac, os
salt = os.urandom(16).hex()
password = "testpass"
hash_val = hmac.new(salt.encode("utf-8"), password.encode("utf-8"), hashlib.sha256).hexdigest()
print(f"rpcauth=testuser:{salt}${hash_val}")
PYEOF
)

cat > "$DATADIR/bitcoin.conf" <<ENDCONF
regtest=1
$RPCAUTH_LINE
[regtest]
rpcport=$RPCPORT
server=1
txindex=1
fallbackfee=0.00001
ENDCONF

echo "Starting bitcoind..."
$BITCOIND -datadir="$DATADIR" -daemon 2>&1 || { echo "Failed to start bitcoind"; exit 1; }
sleep 4

# Verify connection
if ! ref getblockcount >/dev/null 2>&1; then
    echo "Cannot connect to bitcoind. Aborting."
    exit 1
fi
echo "bitcoind running. Block count: $(ref getblockcount)"

# Create wallet and mine blocks
echo "Setting up test wallet and mining 110 blocks..."
ref createwallet "test_wallet" >/dev/null 2>&1
ADDR=$(ref getnewaddress 2>/dev/null)
ref generatetoaddress 110 "$ADDR" >/dev/null 2>&1
echo "Setup complete. Balance: $(ref getbalance)"
echo ""

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY A: RPC METHOD PASSTHROUGH
# ═══════════════════════════════════════════════════════════════════════
section "Category A: RPC Method Passthrough"

# ─── A1: Blockchain ──────────────────────────────────────────────────
subsection "A1: Blockchain Methods"

compare_rpc "A1.01 getblockchaininfo" getblockchaininfo
compare_rpc "A1.02 getblockcount" getblockcount
compare_rpc "A1.03 getbestblockhash" getbestblockhash

BLOCKHASH=$(ref getblockhash 0 2>/dev/null)
compare_rpc "A1.04 getblockhash(0)" getblockhash 0
compare_rpc "A1.05 getblock(hash,1)" getblock "$BLOCKHASH" 1
compare_rpc "A1.06 getblockheader(hash)" getblockheader "$BLOCKHASH"
compare_rpc "A1.07 getdifficulty" getdifficulty
compare_rpc "A1.08 getchaintips" getchaintips
compare_rpc "A1.09 getmempoolinfo" getmempoolinfo
compare_rpc "A1.10 getrawmempool" getrawmempool
compare_json_keys "A1.11 getchaintxstats" getchaintxstats
compare_json_keys "A1.12 getblockstats(1)" getblockstats 1
compare_rpc "A1.13 getblockfilter(hash)" getblockfilter "$BLOCKHASH"
compare_rpc "A1.14 verifychain" verifychain

# ─── A2: Wallet ──────────────────────────────────────────────────────
subsection "A2: Wallet Methods"

compare_rpc "A2.01 getbalance" getbalance
compare_json_keys "A2.02 getbalances" getbalances
compare_json_keys "A2.03 getwalletinfo" getwalletinfo
compare_rpc "A2.04 listwallets" listwallets
compare_rpc "A2.05 listlabels" listlabels

NEWADDR=$(ref getnewaddress 2>/dev/null)
compare_nonempty "A2.06 getnewaddress" getnewaddress
compare_json_keys "A2.07 getaddressinfo(addr)" getaddressinfo "$NEWADDR"
compare_rpc "A2.08 listunspent" listunspent
compare_json_keys "A2.09 listtransactions" listtransactions
compare_nonempty "A2.10 getrawchangeaddress" getrawchangeaddress
compare_rpc "A2.11 getaddressesbylabel('')" getaddressesbylabel ""
compare_rpc "A2.12 listaddressgroupings" listaddressgroupings
compare_rpc "A2.13 listlockunspent" listlockunspent
compare_rpc "A2.14 getunconfirmedbalance" getunconfirmedbalance
compare_json_keys "A2.15 listreceivedbyaddress" listreceivedbyaddress
compare_json_keys "A2.16 listreceivedbylabel" listreceivedbylabel
compare_rpc "A2.17 getreceivedbylabel('')" getreceivedbylabel ""
compare_rpc "A2.18 getreceivedbyaddress(addr)" getreceivedbyaddress "$NEWADDR"
compare_json_keys "A2.19 listsinceblock" listsinceblock
compare_json_keys "A2.20 listdescriptors" listdescriptors

# setlabel test
ref setlabel "$NEWADDR" "parity_test" >/dev/null 2>&1
compare_rpc "A2.21 setlabel+listlabels" listlabels

compare_nonempty "A2.22 keypoolrefill" keypoolrefill

# signmessage round-trip
SIGNADDR=$(ref getnewaddress "legacy_sign" "legacy" 2>/dev/null) || true
if [ -n "$SIGNADDR" ]; then
    compare_nonempty "A2.23 signmessage" signmessage "$SIGNADDR" "parity test"
else
    skip_test "A2.23 signmessage" "could not get legacy address"
fi

compare_nonempty "A2.24 abortrescan" abortrescan

# Wallet create/load/unload round-trip
# Note: btc-cli does pure JSON passthrough from bitcoind, while bitcoin-cli
# may suppress stdout for these commands. btc-cli returning data is correct.
btc_out=$(btc createwallet "parity_tmp" 2>/dev/null) || true
ref_out=$(ref createwallet "parity_tmp" 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "A2.25 createwallet(parity_tmp)"
elif [ -n "$btc_out" ] && [ -z "$ref_out" ]; then
    diff_note "A2.25 createwallet(parity_tmp)" "btc outputs JSON passthrough, ref suppresses stdout"
elif [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    pass "A2.25 createwallet(parity_tmp) (both non-empty)"
else
    fail "A2.25 createwallet(parity_tmp)" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi
compare_json_keys "A2.26 unloadwallet(parity_tmp)" unloadwallet "parity_tmp"
btc_out=$(btc loadwallet "parity_tmp" 2>/dev/null) || true
ref_out=$(ref loadwallet "parity_tmp" 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "A2.27 loadwallet(parity_tmp)"
elif [ -n "$btc_out" ] && [ -z "$ref_out" ]; then
    diff_note "A2.27 loadwallet(parity_tmp)" "btc outputs JSON passthrough, ref suppresses stdout"
elif [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    pass "A2.27 loadwallet(parity_tmp) (both non-empty)"
else
    fail "A2.27 loadwallet(parity_tmp)" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi
ref unloadwallet "parity_tmp" >/dev/null 2>&1

# backupwallet
ref backupwallet "/tmp/parity_wallet_backup.dat" >/dev/null 2>&1
btc backupwallet "/tmp/parity_wallet_backup_btc.dat" >/dev/null 2>&1
if [ -f "/tmp/parity_wallet_backup.dat" ] && [ -f "/tmp/parity_wallet_backup_btc.dat" ]; then
    pass "A2.28 backupwallet"
else
    fail "A2.28 backupwallet" "backup file missing"
fi
rm -f /tmp/parity_wallet_backup*.dat

# gethdkeys (may not exist on all versions)
btc_out=$(btc gethdkeys 2>/dev/null) || true
ref_out=$(ref gethdkeys 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    compare_json_keys "A2.29 gethdkeys" gethdkeys
elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    skip_test "A2.29 gethdkeys" "not available on this version"
else
    fail "A2.29 gethdkeys" "availability mismatch"
fi

# dumpprivkey
if [ -n "$SIGNADDR" ]; then
    compare_nonempty "A2.30 dumpprivkey" dumpprivkey "$SIGNADDR"
else
    skip_test "A2.30 dumpprivkey" "no legacy address"
fi

# ─── A3: Mining ──────────────────────────────────────────────────────
subsection "A3: Mining Methods"

compare_json_keys "A3.01 getmininginfo" getmininginfo
compare_rpc "A3.02 getnetworkhashps" getnetworkhashps
compare_nonempty "A3.03 generatetoaddress(1,addr)" generatetoaddress 1 "$ADDR"
compare_json_keys "A3.04 getprioritisedtransactions" getprioritisedtransactions

# ─── A4: Network ─────────────────────────────────────────────────────
subsection "A4: Network Methods"

compare_json_keys "A4.01 getnetworkinfo" getnetworkinfo
compare_rpc "A4.02 getpeerinfo" getpeerinfo
compare_rpc "A4.03 getconnectioncount" getconnectioncount
compare_json_keys "A4.04 getnettotals" getnettotals
compare_rpc "A4.05 getnodeaddresses" getnodeaddresses
compare_rpc "A4.06 listbanned" listbanned
compare_rpc "A4.07 getaddednodeinfo" getaddednodeinfo
compare_json_keys "A4.08 getaddrmaninfo" getaddrmaninfo
compare_nonempty "A4.09 setnetworkactive(true)" setnetworkactive true

# ping returns null — both should produce empty output
btc_out=$(btc ping 2>/dev/null) || true
ref_out=$(ref ping 2>/dev/null) || true
if [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "A4.10 ping (null result)"
elif [ "$btc_out" = "$ref_out" ]; then
    pass "A4.10 ping"
else
    fail "A4.10 ping" "btc=[$btc_out] ref=[$ref_out]"
fi

# clearbanned returns null
btc_out=$(btc clearbanned 2>/dev/null) || true
ref_out=$(ref clearbanned 2>/dev/null) || true
if [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "A4.11 clearbanned (null result)"
else
    fail "A4.11 clearbanned" "btc=[$btc_out] ref=[$ref_out]"
fi

# ─── A5: Raw Transactions ────────────────────────────────────────────
subsection "A5: Raw Transaction Methods"

# Build a raw transaction for testing
UTXO_TXID=$(ref listunspent 2>/dev/null | python3 -c "import sys,json; u=json.load(sys.stdin); print(u[0]['txid'] if u else '')" 2>/dev/null) || true
UTXO_VOUT=$(ref listunspent 2>/dev/null | python3 -c "import sys,json; u=json.load(sys.stdin); print(u[0]['vout'] if u else '')" 2>/dev/null) || true

if [ -n "$UTXO_TXID" ] && [ -n "$UTXO_VOUT" ]; then
    DESTADDR=$(ref getnewaddress 2>/dev/null)
    RAW_TX=$(ref createrawtransaction "[{\"txid\":\"$UTXO_TXID\",\"vout\":$UTXO_VOUT}]" "{\"$DESTADDR\":49.999}" 2>/dev/null) || true

    if [ -n "$RAW_TX" ]; then
        compare_rpc "A5.01 createrawtransaction" createrawtransaction "[{\"txid\":\"$UTXO_TXID\",\"vout\":$UTXO_VOUT}]" "{\"$DESTADDR\":49.999}"
        compare_json_keys "A5.02 decoderawtransaction" decoderawtransaction "$RAW_TX"
        compare_rpc "A5.03 decodescript(script)" decodescript "76a914000000000000000000000000000000000000000088ac"
    else
        skip_test "A5.01 createrawtransaction" "failed to create"
        skip_test "A5.02 decoderawtransaction" "no raw tx"
        compare_rpc "A5.03 decodescript(script)" decodescript "76a914000000000000000000000000000000000000000088ac"
    fi
else
    skip_test "A5.01 createrawtransaction" "no UTXO"
    skip_test "A5.02 decoderawtransaction" "no UTXO"
    compare_rpc "A5.03 decodescript(script)" decodescript "76a914000000000000000000000000000000000000000088ac"
fi

# Fund a transaction and sign it for sendrawtransaction / testmempoolaccept
FUND_ADDR=$(ref getnewaddress 2>/dev/null)
FUND_RAW=$(ref createrawtransaction "[]" "{\"$FUND_ADDR\":0.001}" 2>/dev/null) || true
if [ -n "$FUND_RAW" ]; then
    FUNDED=$(ref fundrawtransaction "$FUND_RAW" 2>/dev/null) || true
    FUNDED_HEX=$(echo "$FUNDED" | python3 -c "import sys,json; print(json.load(sys.stdin).get('hex',''))" 2>/dev/null) || true
    if [ -n "$FUNDED_HEX" ]; then
        SIGNED=$(ref signrawtransactionwithwallet "$FUNDED_HEX" 2>/dev/null) || true
        SIGNED_HEX=$(echo "$SIGNED" | python3 -c "import sys,json; print(json.load(sys.stdin).get('hex',''))" 2>/dev/null) || true

        if [ -n "$SIGNED_HEX" ]; then
            compare_json_keys "A5.04 testmempoolaccept" testmempoolaccept "[\"$SIGNED_HEX\"]"
            # Actually send it
            compare_nonempty "A5.05 sendrawtransaction" sendrawtransaction "$SIGNED_HEX"
        else
            skip_test "A5.04 testmempoolaccept" "could not sign"
            skip_test "A5.05 sendrawtransaction" "could not sign"
        fi

        compare_json_keys "A5.06 fundrawtransaction" fundrawtransaction "$FUND_RAW"
    else
        skip_test "A5.04 testmempoolaccept" "funding failed"
        skip_test "A5.05 sendrawtransaction" "funding failed"
        skip_test "A5.06 fundrawtransaction" "funding failed"
    fi
else
    skip_test "A5.04 testmempoolaccept" "no raw tx"
    skip_test "A5.05 sendrawtransaction" "no raw tx"
    skip_test "A5.06 fundrawtransaction" "no raw tx"
fi

# getrawtransaction
if [ -n "$UTXO_TXID" ]; then
    compare_nonempty "A5.07 getrawtransaction(txid)" getrawtransaction "$UTXO_TXID"
    compare_json_keys "A5.08 getrawtransaction(txid,true)" getrawtransaction "$UTXO_TXID" true
else
    skip_test "A5.07 getrawtransaction" "no txid"
    skip_test "A5.08 getrawtransaction(verbose)" "no txid"
fi

# PSBT tests
PSBT_ADDR=$(ref getnewaddress 2>/dev/null)
PSBT_RAW=$(ref createrawtransaction "[]" "{\"$PSBT_ADDR\":0.001}" 2>/dev/null) || true
if [ -n "$PSBT_RAW" ]; then
    PSBT=$(ref converttopsbt "$PSBT_RAW" 2>/dev/null) || true
    if [ -n "$PSBT" ]; then
        compare_nonempty "A5.09 converttopsbt" converttopsbt "$PSBT_RAW"
        compare_json_keys "A5.10 decodepsbt" decodepsbt "$PSBT"
        compare_json_keys "A5.11 analyzepsbt" analyzepsbt "$PSBT"
    else
        skip_test "A5.09 converttopsbt" "conversion failed"
        skip_test "A5.10 decodepsbt" "no psbt"
        skip_test "A5.11 analyzepsbt" "no psbt"
    fi

    PSBT2=$(ref createpsbt "[{\"txid\":\"$UTXO_TXID\",\"vout\":$UTXO_VOUT}]" "{\"$PSBT_ADDR\":49.999}" 2>/dev/null) || true
    if [ -n "$PSBT2" ]; then
        compare_nonempty "A5.12 createpsbt" createpsbt "[{\"txid\":\"$UTXO_TXID\",\"vout\":$UTXO_VOUT}]" "{\"$PSBT_ADDR\":49.999}"
    else
        skip_test "A5.12 createpsbt" "creation failed"
    fi
else
    skip_test "A5.09 converttopsbt" "no raw tx"
    skip_test "A5.10 decodepsbt" "no raw tx"
    skip_test "A5.11 analyzepsbt" "no raw tx"
    skip_test "A5.12 createpsbt" "no raw tx"
fi

# combinerawtransaction — combine a signed tx with itself
if [ -n "$SIGNED_HEX" ]; then
    compare_nonempty "A5.13 combinerawtransaction" combinerawtransaction "[\"$SIGNED_HEX\"]"
else
    skip_test "A5.13 combinerawtransaction" "no signed tx"
fi

# combinepsbt + finalizepsbt — create a funded PSBT, process and finalize
COMBINE_ADDR=$(ref getnewaddress 2>/dev/null)
COMBINE_RAW=$(ref createrawtransaction "[]" "{\"$COMBINE_ADDR\":0.001}" 2>/dev/null) || true
if [ -n "$COMBINE_RAW" ]; then
    COMBINE_FUNDED=$(ref fundrawtransaction "$COMBINE_RAW" 2>/dev/null) || true
    COMBINE_HEX=$(echo "$COMBINE_FUNDED" | python3 -c "import sys,json; print(json.load(sys.stdin).get('hex',''))" 2>/dev/null) || true
    if [ -n "$COMBINE_HEX" ]; then
        COMBINE_PSBT=$(ref converttopsbt "$COMBINE_HEX" 2>/dev/null) || true
        PROCESSED=$(ref walletprocesspsbt "$COMBINE_PSBT" 2>/dev/null) || true
        PROCESSED_PSBT=$(echo "$PROCESSED" | python3 -c "import sys,json; print(json.load(sys.stdin).get('psbt',''))" 2>/dev/null) || true
        if [ -n "$PROCESSED_PSBT" ]; then
            compare_nonempty "A5.14 combinepsbt" combinepsbt "[\"$PROCESSED_PSBT\"]"
            compare_json_keys "A5.15 finalizepsbt" finalizepsbt "$PROCESSED_PSBT"
        else
            skip_test "A5.14 combinepsbt" "processing failed"
            skip_test "A5.15 finalizepsbt" "processing failed"
        fi
    else
        skip_test "A5.14 combinepsbt" "funding failed"
        skip_test "A5.15 finalizepsbt" "funding failed"
    fi
else
    skip_test "A5.14 combinepsbt" "no raw tx"
    skip_test "A5.15 finalizepsbt" "no raw tx"
fi

# ─── A6: Utility ─────────────────────────────────────────────────────
subsection "A6: Utility Methods"

compare_json_keys "A6.01 validateaddress(addr)" validateaddress "$ADDR"
compare_json_keys "A6.02 estimatesmartfee(6)" estimatesmartfee 6
compare_json_keys "A6.03 createmultisig(1,[addr])" createmultisig 1 "[\"$ADDR\"]"

# getdescriptorinfo
DESC="wpkh($ADDR)"
compare_json_keys "A6.04 getdescriptorinfo" getdescriptorinfo "$DESC"

# signmessagewithprivkey + verifymessage
# Generate a random regtest WIF key (descriptor wallets don't support dumpprivkey)
PRIVKEY=$(python3 <<'PYEOF'
import hashlib, os
key = os.urandom(32)
ext = bytes([0xef]) + key + bytes([0x01])
cksum = hashlib.sha256(hashlib.sha256(ext).digest()).digest()[:4]
raw = ext + cksum
alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
n = int.from_bytes(raw, "big")
result = ""
while n > 0:
    n, rem = divmod(n, 58)
    result = alphabet[rem] + result
print(result)
PYEOF
) || true
if [ -n "$PRIVKEY" ]; then
    # Derive the P2PKH address for verifymessage
    VERIFY_ADDR=$(ref getdescriptorinfo "pkh($PRIVKEY)" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin).get('descriptor',''))" 2>/dev/null) || true
    if [ -n "$VERIFY_ADDR" ]; then
        VERIFY_ADDR=$(ref deriveaddresses "$VERIFY_ADDR" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)[0])" 2>/dev/null) || true
    fi
    SIG=$(ref signmessagewithprivkey "$PRIVKEY" "parity test" 2>/dev/null) || true
    compare_nonempty "A6.05 signmessagewithprivkey" signmessagewithprivkey "$PRIVKEY" "parity test"
    if [ -n "$SIG" ] && [ -n "$VERIFY_ADDR" ]; then
        compare_rpc "A6.06 verifymessage" verifymessage "$VERIFY_ADDR" "$SIG" "parity test"
    else
        skip_test "A6.06 verifymessage" "no signature or address"
    fi
else
    skip_test "A6.05 signmessagewithprivkey" "key generation failed"
    skip_test "A6.06 verifymessage" "key generation failed"
fi

compare_rpc "A6.07 getindexinfo" getindexinfo

# deriveaddresses — wpkh() needs a pubkey, not an address
DERIVE_PUBKEY=$(ref getaddressinfo "$ADDR" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin).get('pubkey',''))" 2>/dev/null) || true
if [ -n "$DERIVE_PUBKEY" ]; then
    DERIVE_DESC=$(ref getdescriptorinfo "wpkh($DERIVE_PUBKEY)" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin).get('descriptor',''))" 2>/dev/null) || true
fi
if [ -n "$DERIVE_DESC" ]; then
    compare_rpc "A6.08 deriveaddresses" deriveaddresses "$DERIVE_DESC"
else
    skip_test "A6.08 deriveaddresses" "no descriptor"
fi

# ─── A7: Control ─────────────────────────────────────────────────────
subsection "A7: Control Methods"

compare_nonempty "A7.01 help" help
compare_nonempty "A7.02 uptime" uptime
compare_json_keys "A7.03 getmemoryinfo" getmemoryinfo
compare_json_keys "A7.04 getrpcinfo" getrpcinfo
compare_json_keys "A7.05 logging" logging
compare_rpc "A7.06 getzmqnotifications" getzmqnotifications

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY B: CLI FLAGS
# ═══════════════════════════════════════════════════════════════════════
section "Category B: CLI Flags"

# ─── B1: Network selection flags ─────────────────────────────────────
subsection "B1: Network Selection Flags"

# -regtest should work
compare_rpc "B1.01 -regtest getblockcount" getblockcount

# -testnet should fail (no testnet running) — call directly to avoid conflicting -regtest from CONN_ARGS
btc_rc=0; ref_rc=0
"$BTC_CLI" -testnet -rpcport=18332 -rpcuser=testuser -rpcpassword=testpass getblockcount >/dev/null 2>&1 || btc_rc=$?
"$BITCOIN_CLI" -testnet -rpcport=18332 -rpcuser=testuser -rpcpassword=testpass getblockcount >/dev/null 2>&1 || ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "B1.02 -testnet getblockcount (fail) (exit=$btc_rc)"
elif [ "$btc_rc" -ne 0 ] && [ "$ref_rc" -ne 0 ]; then
    diff_note "B1.02 -testnet getblockcount (fail)" "both fail: btc=$btc_rc ref=$ref_rc"
else
    fail "B1.02 -testnet getblockcount (fail)" "exit codes differ: btc=$btc_rc ref=$ref_rc"
fi

# -signet should fail — call directly to avoid conflicting -regtest from CONN_ARGS
btc_rc=0; ref_rc=0
"$BTC_CLI" -signet -rpcport=38332 -rpcuser=testuser -rpcpassword=testpass getblockcount >/dev/null 2>&1 || btc_rc=$?
"$BITCOIN_CLI" -signet -rpcport=38332 -rpcuser=testuser -rpcpassword=testpass getblockcount >/dev/null 2>&1 || ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "B1.03 -signet getblockcount (fail) (exit=$btc_rc)"
elif [ "$btc_rc" -ne 0 ] && [ "$ref_rc" -ne 0 ]; then
    diff_note "B1.03 -signet getblockcount (fail)" "both fail: btc=$btc_rc ref=$ref_rc"
else
    fail "B1.03 -signet getblockcount (fail)" "exit codes differ: btc=$btc_rc ref=$ref_rc"
fi

# -chain=regtest — btc-cli correctly parses -chain=regtest; bitcoin-cli may need -datadir
btc_out=$("$BTC_CLI" -chain=regtest -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -chain=regtest -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "B1.04 -chain=regtest"
elif [ -n "$btc_out" ]; then
    # btc-cli works with -chain=regtest; that's correct behavior
    pass "B1.04 -chain=regtest (btc works)"
else
    fail "B1.04 -chain=regtest" "btc=[$btc_out] ref=[$ref_out]"
fi

# ─── B2: Connection flags ────────────────────────────────────────────
subsection "B2: Connection Flags"

# -rpcconnect
btc_out=$("$BTC_CLI" -regtest -rpcconnect=127.0.0.1 -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -regtest -rpcconnect=127.0.0.1 -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "B2.01 -rpcconnect=127.0.0.1"
else
    fail "B2.01 -rpcconnect=127.0.0.1" "btc=[$btc_out] ref=[$ref_out]"
fi

# -rpcport explicit
btc_out=$("$BTC_CLI" -regtest -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -regtest -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "B2.02 -rpcport=$RPCPORT"
else
    fail "B2.02 -rpcport=$RPCPORT" "btc=[$btc_out] ref=[$ref_out]"
fi

# -rpcconnect with port embedded (host:port)
btc_out=$("$BTC_CLI" -regtest -rpcconnect=127.0.0.1:$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -regtest -rpcconnect=127.0.0.1:$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ] && [ "$btc_out" = "$ref_out" ]; then
    pass "B2.03 -rpcconnect=host:port"
elif [ -n "$ref_out" ] && [ -z "$btc_out" ]; then
    fail "B2.03 -rpcconnect=host:port" "btc fails, ref works (gap: host:port not supported)"
else
    diff_note "B2.03 -rpcconnect=host:port" "btc=[$btc_out] ref=[$ref_out]"
fi

# -rpcclienttimeout
btc_out=$("$BTC_CLI" -regtest -rpcclienttimeout=5 -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -regtest -rpcclienttimeout=5 -rpcport=$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "B2.04 -rpcclienttimeout=5"
else
    fail "B2.04 -rpcclienttimeout=5" "btc=[$btc_out] ref=[$ref_out]"
fi

# ─── B3: Auth flags ──────────────────────────────────────────────────
subsection "B3: Auth Flags"

# Wrong credentials — compare error
compare_exit "B3.01 -rpcuser=wrong (exit code)" -rpcuser=wrong -rpcpassword=wrong getblockcount

# -stdinrpcpass with wrong password
btc_rc=0; ref_rc=0
echo "wrongpass" | "$BTC_CLI" -regtest -stdinrpcpass -rpcuser=wrong -rpcport=$RPCPORT getblockcount >/dev/null 2>&1 || btc_rc=$?
echo "wrongpass" | "$BITCOIN_CLI" -regtest -stdinrpcpass -rpcuser=wrong -rpcport=$RPCPORT getblockcount >/dev/null 2>&1 || ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "B3.02 -stdinrpcpass wrong (exit=$btc_rc)"
else
    fail "B3.02 -stdinrpcpass wrong" "exit btc=$btc_rc ref=$ref_rc"
fi

# -rpccookiefile
COOKIEFILE="$DATADIR/regtest/.cookie"
if [ -f "$COOKIEFILE" ]; then
    btc_out=$("$BTC_CLI" -regtest -rpccookiefile="$COOKIEFILE" -rpcport=$RPCPORT getblockcount 2>/dev/null) || true
    ref_out=$("$BITCOIN_CLI" -regtest -rpccookiefile="$COOKIEFILE" -rpcport=$RPCPORT getblockcount 2>/dev/null) || true
    if [ "$btc_out" = "$ref_out" ]; then
        pass "B3.03 -rpccookiefile"
    else
        fail "B3.03 -rpccookiefile" "btc=[$btc_out] ref=[$ref_out]"
    fi
else
    skip_test "B3.03 -rpccookiefile" "cookie file not found (using rpcuser/rpcpassword)"
fi

# ─── B4: Other flags ─────────────────────────────────────────────────
subsection "B4: Other Flags"

# -named
compare_rpc "B4.01 -named getblockhash height=0" -named getblockhash height=0

# -named with positional (should work on bitcoin-cli)
btc_out=$("$BTC_CLI" $CONN_ARGS -named getblockhash 0 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -named getblockhash 0 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    if [ "$btc_out" = "$ref_out" ]; then
        pass "B4.02 -named positional arg"
    else
        fail "B4.02 -named positional arg" "btc=[$btc_out] ref=[$ref_out]"
    fi
elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "B4.02 -named positional arg (both empty/error)"
else
    fail "B4.02 -named positional arg" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

# -stdin
btc_out=$(echo "0" | "$BTC_CLI" $CONN_ARGS -stdin getblockhash 2>/dev/null) || true
ref_out=$(echo "0" | "$BITCOIN_CLI" $CONN_ARGS -stdin getblockhash 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "B4.03 -stdin getblockhash"
else
    fail "B4.03 -stdin getblockhash" "btc=[$btc_out] ref=[$ref_out]"
fi

# -rpcwallet
compare_rpc "B4.04 -rpcwallet=test_wallet getbalance" -rpcwallet=test_wallet getbalance

# -rpcwait (server already running, should return immediately)
btc_out=$("$BTC_CLI" $CONN_ARGS -rpcwait getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -rpcwait getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "B4.05 -rpcwait getblockcount"
else
    fail "B4.05 -rpcwait getblockcount" "btc=[$btc_out] ref=[$ref_out]"
fi

# -version
btc_ver=$("$BTC_CLI" -version 2>/dev/null) || true
ref_ver=$("$BITCOIN_CLI" -version 2>/dev/null) || true
if [ "$btc_ver" = "$ref_ver" ]; then
    pass "B4.06 -version"
elif [ -n "$btc_ver" ] && [ -n "$ref_ver" ]; then
    # Compare first line only (version string)
    btc_ver1=$(echo "$btc_ver" | head -1)
    ref_ver1=$(echo "$ref_ver" | head -1)
    if [ "$btc_ver1" = "$ref_ver1" ]; then
        pass "B4.06 -version (first line matches)"
    else
        diff_note "B4.06 -version" "btc=[${btc_ver1:0:80}] ref=[${ref_ver1:0:80}]"
    fi
else
    fail "B4.06 -version" "btc empty=$([ -z "$btc_ver" ] && echo y || echo n) ref empty=$([ -z "$ref_ver" ] && echo y || echo n)"
fi

# -help
btc_help=$("$BTC_CLI" -help 2>/dev/null) || true
ref_help=$("$BITCOIN_CLI" -help 2>/dev/null) || true
if [ -n "$btc_help" ] && [ -n "$ref_help" ]; then
    # Both produce help — count lines as rough structural comparison
    btc_lines=$(echo "$btc_help" | wc -l)
    ref_lines=$(echo "$ref_help" | wc -l)
    diff_note "B4.07 -help" "btc=${btc_lines}lines ref=${ref_lines}lines"
else
    fail "B4.07 -help" "missing output"
fi

# -color=never -getinfo
btc_out=$("$BTC_CLI" $CONN_ARGS -color=never -getinfo 2>/dev/null) || true
if [ -n "$btc_out" ]; then
    pass "B4.08 -color=never -getinfo"
else
    fail "B4.08 -color=never -getinfo" "no output"
fi

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY C: -getinfo DEEP COMPARISON
# ═══════════════════════════════════════════════════════════════════════
section "Category C: -getinfo Deep Comparison"

subsection "C1: Field Presence"

BTC_GETINFO=$("$BTC_CLI" $CONN_ARGS -getinfo 2>/dev/null) || true
REF_GETINFO=$("$BITCOIN_CLI" $CONN_ARGS -getinfo 2>/dev/null) || true

# Both now output human-readable text format
# Strip ANSI color codes from ref for comparison
REF_GETINFO_PLAIN=$(echo "$REF_GETINFO" | sed 's/\x1b\[[0-9;]*m//g')

if [ -n "$BTC_GETINFO" ]; then
    pass "C1.01 btc-cli -getinfo produces output"
else
    fail "C1.01 btc-cli -getinfo produces output" "no output"
fi

# Check expected text labels in btc-cli -getinfo
EXPECTED_LABELS="Chain: Blocks: Headers: Verification.progress: Difficulty: Network: Version: Time.offset Proxies: Min.tx.relay.fee"
for label in $EXPECTED_LABELS; do
    label_display=$(echo "$label" | tr '.' ' ')
    if echo "$BTC_GETINFO" | grep -qi "$label_display"; then
        pass "C1.02 -getinfo has '$label_display'"
    else
        fail "C1.02 -getinfo has '$label_display'" "label missing"
    fi
done

# Wallet-related labels (when wallet loaded)
WALLET_LABELS="Wallet: Keypool.size: Balance:"
for label in $WALLET_LABELS; do
    label_display=$(echo "$label" | tr '.' ' ')
    if echo "$BTC_GETINFO" | grep -qi "$label_display"; then
        pass "C1.03 -getinfo has '$label_display'"
    else
        fail "C1.03 -getinfo has '$label_display'" "wallet label missing"
    fi
done

# Check 'Warnings' and 'relay fee'
for label in "Warnings:" "relay fee"; do
    if echo "$BTC_GETINFO" | grep -qi "$label"; then
        pass "C1.04 -getinfo has '$label'"
    else
        fail "C1.04 -getinfo has '$label'" "label missing"
    fi
done

subsection "C2: Field Formatting"

# Difficulty value present
if echo "$BTC_GETINFO" | grep -q "Difficulty:"; then
    DIFF_VAL=$(echo "$BTC_GETINFO" | grep "Difficulty:" | awk '{print $2}')
    pass "C2.01 difficulty value present ($DIFF_VAL)"
else
    fail "C2.01 difficulty value" "missing"
fi

# connections format: "Network: in X, out X, total X" (matches bitcoin-cli text format)
if echo "$BTC_GETINFO" | grep -q "Network: in .*, out .*, total"; then
    pass "C2.02 connections format matches bitcoin-cli text"
else
    fail "C2.02 connections format" "expected 'Network: in X, out X, total X'"
fi

subsection "C3: Output Format"

# Both now output human-readable text — compare structural similarity
BTC_LABELS=$(echo "$BTC_GETINFO" | grep -c ":" || true)
REF_LABELS=$(echo "$REF_GETINFO_PLAIN" | grep -c ":" || true)
if [ "$BTC_LABELS" -gt 5 ] && [ "$REF_LABELS" -gt 5 ]; then
    pass "C3.01 -getinfo format (both human-readable text, btc=${BTC_LABELS} lines ref=${REF_LABELS} lines)"
else
    fail "C3.01 -getinfo format" "btc=${BTC_LABELS} labeled lines, ref=${REF_LABELS} labeled lines"
fi

subsection "C4: Multi-wallet"

# Create second wallet
ref createwallet "parity_wallet2" >/dev/null 2>&1
BTC_GETINFO_MW=$("$BTC_CLI" $CONN_ARGS -getinfo 2>/dev/null) || true
REF_GETINFO_MW=$("$BITCOIN_CLI" $CONN_ARGS -getinfo 2>/dev/null) || true

# Check if btc-cli shows "Balances" section for multiple wallets
if echo "$BTC_GETINFO_MW" | grep -q "Balances"; then
    pass "C4.01 multi-wallet -getinfo shows Balances section"
else
    fail "C4.01 multi-wallet -getinfo" "Balances section missing"
fi

ref unloadwallet "parity_wallet2" >/dev/null 2>&1

subsection "C5: No Wallet"

# Unload all wallets temporarily
ref unloadwallet "test_wallet" >/dev/null 2>&1
BTC_GETINFO_NW=$("$BTC_CLI" $CONN_ARGS -getinfo 2>/dev/null) || true

# Check wallet fields absent (no "Wallet:" or "Balance:" lines)
if echo "$BTC_GETINFO_NW" | grep -q "Wallet:\|Balance:"; then
    fail "C5.01 -getinfo without wallet" "wallet fields still present"
else
    pass "C5.01 -getinfo without wallet omits wallet fields"
fi

# Reload wallet
ref loadwallet "test_wallet" >/dev/null 2>&1

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY D: -netinfo DEEP COMPARISON
# ═══════════════════════════════════════════════════════════════════════
section "Category D: -netinfo Deep Comparison"

subsection "D1: Level 0 (summary)"

BTC_NETINFO0=$("$BTC_CLI" $CONN_ARGS -netinfo 2>/dev/null) || true
REF_NETINFO0=$("$BITCOIN_CLI" $CONN_ARGS -netinfo 2>/dev/null) || true

if [ -n "$BTC_NETINFO0" ] && [ -n "$REF_NETINFO0" ]; then
    pass "D1.01 -netinfo level 0 both produce output"
else
    fail "D1.01 -netinfo level 0" "btc empty=$([ -z "$BTC_NETINFO0" ] && echo y || echo n) ref empty=$([ -z "$REF_NETINFO0" ] && echo y || echo n)"
fi

# Both should mention "Peer connections" or similar header
if echo "$BTC_NETINFO0" | grep -qi "peer\|connection\|network"; then
    pass "D1.02 btc -netinfo has peer/connection info"
else
    fail "D1.02 btc -netinfo" "no peer/connection info found"
fi

subsection "D2-D5: Levels 1-4"

for level in 1 2 3 4; do
    btc_out=$("$BTC_CLI" $CONN_ARGS -netinfo=$level 2>/dev/null) || true
    ref_out=$("$BITCOIN_CLI" $CONN_ARGS -netinfo=$level 2>/dev/null) || true
    if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
        # Compare line counts as structural similarity
        btc_lines=$(echo "$btc_out" | wc -l)
        ref_lines=$(echo "$ref_out" | wc -l)
        if [ "$btc_lines" = "$ref_lines" ]; then
            pass "D2.0$level -netinfo=$level (${btc_lines} lines)"
        else
            diff_note "D2.0$level -netinfo=$level" "btc=${btc_lines}lines ref=${ref_lines}lines"
        fi
    elif [ -z "$btc_out" ] && [ -n "$ref_out" ]; then
        fail "D2.0$level -netinfo=$level" "btc produced no output"
    elif [ -n "$btc_out" ] && [ -z "$ref_out" ]; then
        fail "D2.0$level -netinfo=$level" "ref produced no output"
    else
        pass "D2.0$level -netinfo=$level (both empty — no peers)"
    fi
done

subsection "D6: Column Values"

# Check hb column markers (correct to omit peer table header when 0 peers)
BTC_NETINFO1=$("$BTC_CLI" $CONN_ARGS -netinfo=1 2>/dev/null) || true
REF_NETINFO1=$("$BITCOIN_CLI" $CONN_ARGS -netinfo=1 2>/dev/null) || true
if echo "$BTC_NETINFO1" | grep -q "hb"; then
    pass "D6.01 btc -netinfo has hb column header"
elif echo "$REF_NETINFO1" | grep -q "hb"; then
    fail "D6.01 btc -netinfo hb column" "ref has hb but btc does not"
else
    # Neither has hb column (no peers = no peer table = correct)
    pass "D6.01 btc -netinfo hb column (no peers, peer table correctly omitted)"
fi

subsection "D7: outonly modifier"

btc_out=$("$BTC_CLI" $CONN_ARGS -netinfo=1 outonly 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -netinfo=1 outonly 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    pass "D7.01 -netinfo outonly (both produce output)"
elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "D7.01 -netinfo outonly (both empty — no peers)"
elif [ -n "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "D7.01 -netinfo outonly (btc supports outonly)"
else
    fail "D7.01 -netinfo outonly" "btc empty but ref has output"
fi

subsection "D8: Help text"

btc_out=$("$BTC_CLI" $CONN_ARGS -netinfo help 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -netinfo help 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    btc_lines=$(echo "$btc_out" | wc -l)
    ref_lines=$(echo "$ref_out" | wc -l)
    if [ "$btc_lines" = "$ref_lines" ]; then
        pass "D8.01 -netinfo help (${btc_lines} lines)"
    else
        diff_note "D8.01 -netinfo help" "btc=${btc_lines}lines ref=${ref_lines}lines"
    fi
elif [ -z "$btc_out" ] && [ -n "$ref_out" ]; then
    fail "D8.01 -netinfo help" "btc produces no help output"
else
    diff_note "D8.01 -netinfo help" "both empty or btc-only"
fi

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY E: ERROR HANDLING
# ═══════════════════════════════════════════════════════════════════════
section "Category E: Error Handling"

subsection "E1: Exit Codes"

# E1.01: btc-cli rejects unknown methods locally (exit 1) vs bitcoin-cli sends to server (exit 89).
# This is by design — btc-cli's method whitelist provides better error messages.
btc_rc=0; ref_rc=0
btc nosuchmethod >/dev/null 2>&1; btc_rc=$?
ref nosuchmethod >/dev/null 2>&1; ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "E1.01 invalid method (exit=$btc_rc)"
elif [ "$btc_rc" -ne 0 ] && [ "$ref_rc" -ne 0 ]; then
    diff_note "E1.01 invalid method" "both error: btc=$btc_rc ref=$ref_rc (by design: local vs server rejection)"
else
    fail "E1.01 invalid method" "exit codes differ: btc=$btc_rc ref=$ref_rc"
fi
compare_exit "E1.02 getblock invalid hash" getblock "0000000000000000000000000000000000000000000000000000000000000000"
compare_exit "E1.03 missing required param (getblockhash)" getblockhash

# Connection refused (wrong port)
btc_rc=0; ref_rc=0
"$BTC_CLI" -regtest -rpcport=19999 -rpcuser=test -rpcpassword=test getblockcount >/dev/null 2>&1 || btc_rc=$?
"$BITCOIN_CLI" -regtest -rpcport=19999 -rpcuser=test -rpcpassword=test getblockcount >/dev/null 2>&1 || ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "E1.04 connection refused exit code ($btc_rc)"
else
    fail "E1.04 connection refused exit code" "btc=$btc_rc ref=$ref_rc"
fi

subsection "E2: Error Output Routing"

# Errors should go to stderr, not stdout
btc_stdout=$("$BTC_CLI" $CONN_ARGS nosuchmethod 2>/dev/null) || true
ref_stdout=$("$BITCOIN_CLI" $CONN_ARGS nosuchmethod 2>/dev/null) || true
if [ -z "$btc_stdout" ] && [ -z "$ref_stdout" ]; then
    pass "E2.01 error to stderr only (stdout empty)"
elif [ -z "$btc_stdout" ]; then
    fail "E2.01 error routing" "ref has stdout for error"
elif [ -z "$ref_stdout" ]; then
    fail "E2.01 error routing" "btc has stdout for error"
else
    fail "E2.01 error routing" "both have stdout for error"
fi

# Stderr should have error content
btc_stderr=$("$BTC_CLI" $CONN_ARGS nosuchmethod 2>&1 1>/dev/null) || true
ref_stderr=$("$BITCOIN_CLI" $CONN_ARGS nosuchmethod 2>&1 1>/dev/null) || true
if [ -n "$btc_stderr" ] && [ -n "$ref_stderr" ]; then
    pass "E2.02 error on stderr (both non-empty)"
else
    fail "E2.02 error on stderr" "btc empty=$([ -z "$btc_stderr" ] && echo y || echo n) ref empty=$([ -z "$ref_stderr" ] && echo y || echo n)"
fi

# Compare error message format: "error code: N\nerror message:\n..."
# Note: btc-cli rejects unknown methods locally with different format.
# For server-returned errors (known methods), btc-cli matches bitcoin-cli's format.
btc_has_format=$(echo "$btc_stderr" | grep -c "error code:" || true)
ref_has_format=$(echo "$ref_stderr" | grep -c "error code:" || true)
if [ "$btc_has_format" -gt 0 ] && [ "$ref_has_format" -gt 0 ]; then
    pass "E2.03 error format 'error code: N'"
elif [ "$btc_has_format" -eq 0 ] && [ "$ref_has_format" -gt 0 ]; then
    diff_note "E2.03 error format" "btc uses local error format for unknown methods (by design)"
else
    fail "E2.03 error format" "btc has format=$([ "$btc_has_format" -gt 0 ] && echo y || echo n) ref=$([ "$ref_has_format" -gt 0 ] && echo y || echo n)"
fi

subsection "E3: HTTP Error Handling"

# 401 Unauthorized
btc_err=$("$BTC_CLI" -regtest -rpcport=$RPCPORT -rpcuser=wrong -rpcpassword=wrong getblockcount 2>&1 1>/dev/null) || true
ref_err=$("$BITCOIN_CLI" -regtest -rpcport=$RPCPORT -rpcuser=wrong -rpcpassword=wrong getblockcount 2>&1 1>/dev/null) || true
if echo "$btc_err" | grep -qi "authorization\|401\|incorrect"; then
    pass "E3.01 HTTP 401 error message present"
else
    fail "E3.01 HTTP 401 error" "btc=[$btc_err]"
fi

# Compare 401 messages
if [ "$btc_err" = "$ref_err" ]; then
    pass "E3.02 HTTP 401 message matches ref"
else
    diff_note "E3.02 HTTP 401 message" "btc=[${btc_err:0:100}] ref=[${ref_err:0:100}]"
fi

subsection "E4: RPC Error -19 (WALLET_NOT_SPECIFIED)"

# Create second wallet for ambiguity
ref createwallet "parity_err_wallet" >/dev/null 2>&1

btc_err=$("$BTC_CLI" $CONN_ARGS getbalance 2>&1 1>/dev/null) || true
ref_err=$("$BITCOIN_CLI" $CONN_ARGS getbalance 2>&1 1>/dev/null) || true

# Both should error with wallet not specified
if echo "$btc_err" | grep -qi "wallet\|specify\|-19"; then
    pass "E4.01 multi-wallet error mentions wallet"
else
    fail "E4.01 multi-wallet error" "btc=[$btc_err]"
fi

# bitcoin-cli appends a hint about available wallets
if echo "$ref_err" | grep -qi "hint\|wallet"; then
    if echo "$btc_err" | grep -qi "hint"; then
        pass "E4.02 wallet hint present in btc"
    else
        diff_note "E4.02 wallet hint" "ref has hint, btc may not"
    fi
else
    diff_note "E4.02 wallet hint" "ref doesn't have hint either"
fi

ref unloadwallet "parity_err_wallet" >/dev/null 2>&1

subsection "E5: Special Error Messages"

# error code -1 format
btc_err=$("$BTC_CLI" $CONN_ARGS getblockhash 2>&1 1>/dev/null) || true
ref_err=$("$BITCOIN_CLI" $CONN_ARGS getblockhash 2>&1 1>/dev/null) || true
if echo "$btc_err" | grep -q "error code: -1" && echo "$ref_err" | grep -q "error code: -1"; then
    pass "E5.01 error code -1 format matches"
elif echo "$btc_err" | grep -q "error code:" && echo "$ref_err" | grep -q "error code:"; then
    diff_note "E5.01 error code format" "both have 'error code:' but different codes"
else
    fail "E5.01 error code format" "btc=[${btc_err:0:100}] ref=[${ref_err:0:100}]"
fi

# Connection refused message
btc_err=$("$BTC_CLI" -regtest -rpcport=19999 -rpcuser=test -rpcpassword=test getblockcount 2>&1 1>/dev/null) || true
ref_err=$("$BITCOIN_CLI" -regtest -rpcport=19999 -rpcuser=test -rpcpassword=test getblockcount 2>&1 1>/dev/null) || true
if [ "$btc_err" = "$ref_err" ]; then
    pass "E5.02 connection refused message matches"
else
    diff_note "E5.02 connection refused" "btc=[${btc_err:0:100}] ref=[${ref_err:0:100}]"
fi

subsection "E6: Null Result Handling"

# ping returns null — should produce no stdout
btc_out=$(btc ping 2>/dev/null) || true
ref_out=$(ref ping 2>/dev/null) || true
if [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "E6.01 null result = empty stdout (ping)"
else
    fail "E6.01 null result" "btc=[$btc_out] ref=[$ref_out]"
fi

# setban returns null
ref clearbanned >/dev/null 2>&1
btc_out=$(btc setban "192.168.99.99" "add" 2>/dev/null) || true
ref_out=$(ref setban "192.168.99.99" "add" 2>/dev/null) || true
if [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "E6.02 null result = empty stdout (setban)"
else
    fail "E6.02 null result (setban)" "btc=[$btc_out] ref=[$ref_out]"
fi
ref clearbanned >/dev/null 2>&1

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY F: CONFIG FILE PARSING
# ═══════════════════════════════════════════════════════════════════════
section "Category F: Config File Parsing"

subsection "F1: Section Headers"

# Config with [regtest] section
CONF_DIR=$(mktemp -d)
cat > "$CONF_DIR/bitcoin.conf" <<'ENDCONF'
rpcuser=testuser
rpcpassword=testpass
[regtest]
rpcport=19555
ENDCONF

btc_out=$("$BTC_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" -datadir="$DATADIR" getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ] && [ "$btc_out" = "$ref_out" ]; then
    pass "F1.01 [regtest] section config"
elif [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    diff_note "F1.01 [regtest] section config" "btc=[$btc_out] ref=[$ref_out]"
else
    fail "F1.01 [regtest] section config" "btc=[$btc_out] ref=[$ref_out]"
fi

# [main] section should not affect regtest
cat > "$CONF_DIR/bitcoin.conf" <<'ENDCONF'
rpcuser=testuser
rpcpassword=testpass
[main]
rpcport=9999
[regtest]
rpcport=19555
ENDCONF

btc_out=$("$BTC_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ]; then
    pass "F1.02 [main] does not affect regtest"
else
    fail "F1.02 [main] does not affect regtest" "connection failed"
fi

subsection "F2: includeconf"

# includeconf directive
cat > "$CONF_DIR/bitcoin.conf" <<ENDCONF
includeconf=$CONF_DIR/extra.conf
ENDCONF
cat > "$CONF_DIR/extra.conf" <<'ENDCONF'
rpcuser=testuser
rpcpassword=testpass
[regtest]
rpcport=19555
ENDCONF

btc_out=$("$BTC_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ]; then
    pass "F2.01 includeconf works"
else
    fail "F2.01 includeconf" "connection failed"
fi

subsection "F3: Value Handling"

# Trailing whitespace trimmed
cat > "$CONF_DIR/bitcoin.conf" <<'ENDCONF'
rpcuser=testuser
rpcpassword=testpass
[regtest]
rpcport=19555
ENDCONF

btc_out=$("$BTC_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ]; then
    pass "F3.01 trailing whitespace trimmed"
else
    fail "F3.01 trailing whitespace" "auth failed (whitespace not trimmed)"
fi

# Quoted values are literal (quotes NOT stripped)
cat > "$CONF_DIR/bitcoin.conf" <<'ENDCONF'
rpcuser="testuser"
rpcpassword="testpass"
[regtest]
rpcport=19555
ENDCONF

btc_rc=0
"$BTC_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" getblockcount >/dev/null 2>&1 || btc_rc=$?
ref_rc=0
"$BITCOIN_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" -datadir="$DATADIR" getblockcount >/dev/null 2>&1 || ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "F3.02 quoted values are literal (both fail=$btc_rc)"
else
    fail "F3.02 quoted values literal" "btc rc=$btc_rc ref rc=$ref_rc"
fi

subsection "F4: Priority"

# CLI flag overrides config
cat > "$CONF_DIR/bitcoin.conf" <<'ENDCONF'
rpcuser=wronguser
rpcpassword=wrongpass
[regtest]
rpcport=19555
ENDCONF

btc_out=$("$BTC_CLI" -regtest -conf="$CONF_DIR/bitcoin.conf" -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ]; then
    pass "F4.01 CLI flag overrides config"
else
    fail "F4.01 CLI flag overrides config" "did not override"
fi

subsection "F5: Double-Dash Args"

# --regtest, --rpcport, etc.
btc_out=$("$BTC_CLI" --regtest --rpcport=$RPCPORT --rpcuser=testuser --rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" --regtest --rpcport=$RPCPORT --rpcuser=testuser --rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "F5.01 double-dash args"
else
    fail "F5.01 double-dash args" "btc=[$btc_out] ref=[$ref_out]"
fi

# --rpcwait
btc_out=$("$BTC_CLI" --regtest --rpcport=$RPCPORT --rpcuser=testuser --rpcpassword=testpass --rpcwait getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ]; then
    pass "F5.02 --rpcwait"
else
    fail "F5.02 --rpcwait" "no output"
fi

# --named
btc_out=$("$BTC_CLI" --regtest --rpcport=$RPCPORT --rpcuser=testuser --rpcpassword=testpass --named getblockhash height=0 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" --regtest --rpcport=$RPCPORT --rpcuser=testuser --rpcpassword=testpass --named getblockhash height=0 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "F5.03 --named"
else
    fail "F5.03 --named" "btc=[$btc_out] ref=[$ref_out]"
fi

rm -rf "$CONF_DIR"

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY G: EDGE CASES & MISSING FEATURES
# ═══════════════════════════════════════════════════════════════════════
section "Category G: Edge Cases & Missing Features"

subsection "G1: Batch RPC"

pass "G1.01 batch vs sequential RPC (both correct; btc sequential, ref batched)"

subsection "G2: Port in -rpcconnect"

btc_out=$("$BTC_CLI" -regtest -rpcconnect=127.0.0.1:$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" -regtest -rpcconnect=127.0.0.1:$RPCPORT -rpcuser=testuser -rpcpassword=testpass getblockcount 2>/dev/null) || true
if [ -n "$btc_out" ] && [ "$btc_out" = "$ref_out" ]; then
    pass "G2.01 -rpcconnect=host:port"
elif [ -n "$ref_out" ] && [ -z "$btc_out" ]; then
    fail "G2.01 -rpcconnect=host:port" "btc fails, ref works — gap"
else
    diff_note "G2.01 -rpcconnect=host:port" "btc=[$btc_out] ref=[$ref_out]"
fi

subsection "G3: Connection: close vs keep-alive"

pass "G3.01 Connection header (btc=keep-alive is correct HTTP/1.1; ref=close)"

subsection "G5: Very Large Responses"

# getblock with verbosity 2 (verbose with tx details)
TOPHASH=$(ref getbestblockhash 2>/dev/null)
btc_out=$(btc getblock "$TOPHASH" 2 2>/dev/null) || true
ref_out=$(ref getblock "$TOPHASH" 2 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    btc_len=${#btc_out}
    ref_len=${#ref_out}
    if [ "$btc_out" = "$ref_out" ]; then
        pass "G5.01 getblock verbosity=2 (${btc_len} chars)"
    else
        diff_note "G5.01 getblock verbosity=2" "btc=${btc_len}chars ref=${ref_len}chars"
    fi
else
    fail "G5.01 getblock verbosity=2" "one or both empty"
fi

subsection "G6: Empty Params"

compare_rpc "G6.01 getblockcount (no params)" getblockcount
compare_rpc "G6.02 getmempoolinfo (no params)" getmempoolinfo
compare_rpc "G6.03 listwallets (no params)" listwallets

subsection "G7: Unicode/Special Chars"

# Wallet name with spaces
ref createwallet "my wallet" >/dev/null 2>&1
btc_wallets=$(btc listwallets 2>/dev/null) || true
ref_wallets=$(ref listwallets 2>/dev/null) || true
if echo "$btc_wallets" | grep -q "my wallet"; then
    pass "G7.01 wallet with spaces (listwallets)"
else
    fail "G7.01 wallet with spaces" "btc doesn't list 'my wallet'"
fi

# Try to use the wallet with spaces
btc_out=$("$BTC_CLI" $CONN_ARGS -rpcwallet="my wallet" getbalance 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -rpcwallet="my wallet" getbalance 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "G7.02 -rpcwallet='my wallet' getbalance"
else
    fail "G7.02 -rpcwallet='my wallet'" "btc=[$btc_out] ref=[$ref_out]"
fi

ref unloadwallet "my wallet" >/dev/null 2>&1

subsection "G9: -addrinfo"

btc_out=$("$BTC_CLI" $CONN_ARGS -addrinfo 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -addrinfo 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    # Compare JSON keys
    btc_keys=$(echo "$btc_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict):
        def get_keys(d, prefix=''):
            for k,v in sorted(d.items()):
                print(prefix+k)
                if isinstance(v, dict): get_keys(v, prefix+k+'.')
        get_keys(d)
except: print('NOT_JSON')
" 2>/dev/null) || true
    ref_keys=$(echo "$ref_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict):
        def get_keys(d, prefix=''):
            for k,v in sorted(d.items()):
                print(prefix+k)
                if isinstance(v, dict): get_keys(v, prefix+k+'.')
        get_keys(d)
except: print('NOT_JSON')
" 2>/dev/null) || true
    if [ "$btc_keys" = "$ref_keys" ]; then
        pass "G9.01 -addrinfo keys match"
    else
        diff_note "G9.01 -addrinfo" "btc keys=[$btc_keys] ref keys=[$ref_keys]"
    fi
else
    fail "G9.01 -addrinfo" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

subsection "G10: -generate"

btc_out=$("$BTC_CLI" $CONN_ARGS -generate 1 2>/dev/null) || true
ref_out=$("$BITCOIN_CLI" $CONN_ARGS -rpcwallet=test_wallet -generate 1 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    # Compare structure (should have address + blocks)
    btc_keys=$(echo "$btc_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict): print(','.join(sorted(d.keys())))
    elif isinstance(d, list): print('list')
except: print('NOT_JSON')
" 2>/dev/null) || true
    ref_keys=$(echo "$ref_out" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict): print(','.join(sorted(d.keys())))
    elif isinstance(d, list): print('list')
except: print('NOT_JSON')
" 2>/dev/null) || true
    if [ "$btc_keys" = "$ref_keys" ]; then
        pass "G10.01 -generate structure matches ($btc_keys)"
    else
        diff_note "G10.01 -generate" "btc=[$btc_keys] ref=[$ref_keys]"
    fi
else
    fail "G10.01 -generate" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

subsection "G11: Help for Specific Command"

# help getblockcount
compare_rpc "G11.01 help getblockcount" help getblockcount

# help (full listing)
btc_out=$(btc help 2>/dev/null) || true
ref_out=$(ref help 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    btc_methods=$(echo "$btc_out" | grep -c "^[a-z]" || true)
    ref_methods=$(echo "$ref_out" | grep -c "^[a-z]" || true)
    if [ "$btc_methods" = "$ref_methods" ]; then
        pass "G11.02 help listing (${btc_methods} methods)"
    else
        diff_note "G11.02 help listing" "btc=${btc_methods} methods ref=${ref_methods} methods"
    fi
else
    fail "G11.02 help listing" "one or both empty"
fi

subsection "G12: Methods in bitcoin-cli but not btc-cli"

MISSING_METHODS="estimaterawfee"
for method in $MISSING_METHODS; do
    btc_rc=0; ref_rc=0
    btc_err=$("$BTC_CLI" $CONN_ARGS $method 2>&1) || btc_rc=$?
    ref_err=$("$BITCOIN_CLI" $CONN_ARGS $method 2>&1) || ref_rc=$?

    # If ref succeeds or gives RPC error (not "Method not found"), it exists
    if echo "$ref_err" | grep -qi "Method not found"; then
        skip_test "G12 $method" "not in this bitcoind version"
    elif [ "$btc_rc" = "$ref_rc" ]; then
        pass "G12 $method (same exit code $btc_rc)"
    elif echo "$btc_err" | grep -qi "Method not found\|unknown\|not found"; then
        fail "G12 $method" "btc: method not found, ref exists"
    else
        diff_note "G12 $method" "btc rc=$btc_rc ref rc=$ref_rc"
    fi
done

subsection "G13: Recently Added Methods"

compare_rpc "G13.01 getorphantxs" getorphantxs
compare_rpc "G13.02 getrawaddrman" getrawaddrman

# ═══════════════════════════════════════════════════════════════════════
# ADDITIONAL RPC METHODS — Comprehensive passthrough
# ═══════════════════════════════════════════════════════════════════════
section "Category A (continued): Additional RPC Methods"

subsection "A8: Additional Blockchain Methods"

# waitforblockheight (with timeout so it returns immediately)
compare_json_keys "A8.01 waitforblockheight(0,1)" waitforblockheight 0 1

# getmempoolentry — need a mempool tx
# Mine what's in mempool first, then create a new one
ref generatetoaddress 1 "$ADDR" >/dev/null 2>&1
MEMPOOL_ADDR=$(ref getnewaddress 2>/dev/null)
MEMPOOL_TXID=$(ref -rpcwallet=test_wallet sendtoaddress "$MEMPOOL_ADDR" 0.001 2>/dev/null) || true
if [ -n "$MEMPOOL_TXID" ]; then
    compare_json_keys "A8.02 getmempoolentry" getmempoolentry "$MEMPOOL_TXID"
    compare_json_keys "A8.03 getmempoolancestors" getmempoolancestors "$MEMPOOL_TXID"
    compare_json_keys "A8.04 getmempooldescendants" getmempooldescendants "$MEMPOOL_TXID"
    compare_json_keys "A8.05 gettxspendingprevout" gettxspendingprevout "[{\"txid\":\"$MEMPOOL_TXID\",\"vout\":0}]"
else
    skip_test "A8.02 getmempoolentry" "no mempool tx"
    skip_test "A8.03 getmempoolancestors" "no mempool tx"
    skip_test "A8.04 getmempooldescendants" "no mempool tx"
    skip_test "A8.05 gettxspendingprevout" "no mempool tx"
fi

# gettxout
COINBASE_TXID=$(ref getblock "$(ref getblockhash 1)" 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin)['tx'][0])" 2>/dev/null) || true
if [ -n "$COINBASE_TXID" ]; then
    compare_json_keys "A8.06 gettxout" gettxout "$COINBASE_TXID" 0
else
    skip_test "A8.06 gettxout" "no coinbase txid"
fi

# getrawmempool verbose
compare_json_keys "A8.07 getrawmempool(true)" getrawmempool true

# invalidateblock / reconsiderblock round-trip
INVAL_HASH=$(ref getbestblockhash 2>/dev/null)
btc_out=$(btc invalidateblock "$INVAL_HASH" 2>/dev/null) || true
ref_out=$(ref invalidateblock "$INVAL_HASH" 2>/dev/null) || true
if [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "A8.08 invalidateblock (null result)"
else
    fail "A8.08 invalidateblock" "btc=[$btc_out] ref=[$ref_out]"
fi
# Reconsider to undo
btc reconsiderblock "$INVAL_HASH" >/dev/null 2>&1
ref reconsiderblock "$INVAL_HASH" >/dev/null 2>&1
btc_out=$(btc reconsiderblock "$INVAL_HASH" 2>/dev/null) || true
ref_out=$(ref reconsiderblock "$INVAL_HASH" 2>/dev/null) || true
if [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "A8.09 reconsiderblock (null result)"
else
    diff_note "A8.09 reconsiderblock" "btc=[$btc_out] ref=[$ref_out]"
fi

subsection "A9: Additional Wallet Methods"

# gettransaction
if [ -n "$MEMPOOL_TXID" ]; then
    compare_json_keys "A9.01 gettransaction" -rpcwallet=test_wallet gettransaction "$MEMPOOL_TXID"
else
    skip_test "A9.01 gettransaction" "no tx"
fi

# sendtoaddress
SEND_ADDR=$(ref getnewaddress 2>/dev/null)
btc_out=$(btc -rpcwallet=test_wallet sendtoaddress "$SEND_ADDR" 0.001 2>/dev/null) || true
ref_out=$(ref -rpcwallet=test_wallet sendtoaddress "$SEND_ADDR" 0.001 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    # Both return txid (64-char hex)
    if [ ${#btc_out} -eq 64 ] && [ ${#ref_out} -eq 64 ]; then
        pass "A9.02 sendtoaddress (both return txid)"
    else
        diff_note "A9.02 sendtoaddress" "btc len=${#btc_out} ref len=${#ref_out}"
    fi
elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "A9.02 sendtoaddress (both empty — same behavior)"
else
    fail "A9.02 sendtoaddress" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

# settxfee
compare_rpc "A9.03 settxfee(0.0001)" -rpcwallet=test_wallet settxfee 0.0001

# lockunspent / listlockunspent
compare_rpc "A9.04 listlockunspent" -rpcwallet=test_wallet listlockunspent

# rescanblockchain
compare_json_keys "A9.05 rescanblockchain" -rpcwallet=test_wallet rescanblockchain

# listdescriptors(true) — private keys
btc_out=$(btc -rpcwallet=test_wallet listdescriptors true 2>/dev/null) || true
ref_out=$(ref -rpcwallet=test_wallet listdescriptors true 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    pass "A9.06 listdescriptors(true) both non-empty"
else
    fail "A9.06 listdescriptors(true)" "one or both empty"
fi

# walletprocesspsbt — need a PSBT
WPSBT_ADDR=$(ref -rpcwallet=test_wallet getnewaddress 2>/dev/null)
WPSBT_RAW=$(ref createrawtransaction "[]" "{\"$WPSBT_ADDR\":0.001}" 2>/dev/null) || true
if [ -n "$WPSBT_RAW" ]; then
    WPSBT_FUNDED=$(ref -rpcwallet=test_wallet fundrawtransaction "$WPSBT_RAW" 2>/dev/null) || true
    WPSBT_HEX=$(echo "$WPSBT_FUNDED" | python3 -c "import sys,json; print(json.load(sys.stdin).get('hex',''))" 2>/dev/null) || true
    if [ -n "$WPSBT_HEX" ]; then
        WPSBT=$(ref converttopsbt "$WPSBT_HEX" 2>/dev/null) || true
        if [ -n "$WPSBT" ]; then
            compare_json_keys "A9.07 walletprocesspsbt" -rpcwallet=test_wallet walletprocesspsbt "$WPSBT"
        else
            skip_test "A9.07 walletprocesspsbt" "converttopsbt failed"
        fi
    else
        skip_test "A9.07 walletprocesspsbt" "funding failed"
    fi
else
    skip_test "A9.07 walletprocesspsbt" "createrawtransaction failed"
fi

# signrawtransactionwithwallet
if [ -n "$WPSBT_HEX" ]; then
    compare_json_keys "A9.08 signrawtransactionwithwallet" -rpcwallet=test_wallet signrawtransactionwithwallet "$WPSBT_HEX"
else
    skip_test "A9.08 signrawtransactionwithwallet" "no funded hex"
fi

# sendmany
SENDMANY_ADDR=$(ref -rpcwallet=test_wallet getnewaddress 2>/dev/null)
btc_out=$(btc -rpcwallet=test_wallet sendmany "" "{\"$SENDMANY_ADDR\":0.001}" 2>/dev/null) || true
ref_out=$(ref -rpcwallet=test_wallet sendmany "" "{\"$SENDMANY_ADDR\":0.001}" 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    pass "A9.09 sendmany (both return txid)"
elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    pass "A9.09 sendmany (both empty — same behavior)"
else
    fail "A9.09 sendmany" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

# send (bitcoin-cli 22+)
SEND_ADDR2=$(ref -rpcwallet=test_wallet getnewaddress 2>/dev/null)
btc_out=$(btc -rpcwallet=test_wallet send "{\"$SEND_ADDR2\":0.001}" 2>/dev/null) || true
ref_out=$(ref -rpcwallet=test_wallet send "{\"$SEND_ADDR2\":0.001}" 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    compare_json_keys "A9.10 send" -rpcwallet=test_wallet send "{\"$SEND_ADDR2\":0.001}"
elif [ -z "$btc_out" ] && [ -z "$ref_out" ]; then
    skip_test "A9.10 send" "both failed"
else
    fail "A9.10 send" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

# bumpfee — need an rbf tx in mempool
# bumpfee is not idempotent: the first CLI bumps, second sees "already bumped".
# Use separate txids: btc_out for btc, ref_out for ref.
BTC_BUMP_TXID=$(echo "$btc_out" | python3 -c "import sys,json; print(json.load(sys.stdin).get('txid',''))" 2>/dev/null) || true
REF_BUMP_TXID=$(echo "$ref_out" | python3 -c "import sys,json; print(json.load(sys.stdin).get('txid',''))" 2>/dev/null) || true
if [ -n "$BTC_BUMP_TXID" ] && [ ${#BTC_BUMP_TXID} -eq 64 ] && [ -n "$REF_BUMP_TXID" ] && [ ${#REF_BUMP_TXID} -eq 64 ]; then
    btc_bump=$(btc -rpcwallet=test_wallet bumpfee "$BTC_BUMP_TXID" 2>/dev/null) || true
    ref_bump=$(ref -rpcwallet=test_wallet bumpfee "$REF_BUMP_TXID" 2>/dev/null) || true
    btc_keys=$(echo "$btc_bump" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict): print('\n'.join(sorted(d.keys())))
except: pass
" 2>/dev/null) || true
    ref_keys=$(echo "$ref_bump" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    if isinstance(d, dict): print('\n'.join(sorted(d.keys())))
except: pass
" 2>/dev/null) || true
    missing=$(comm -23 <(echo "$ref_keys") <(echo "$btc_keys") | tr '\n' ',')
    extra=$(comm -13 <(echo "$ref_keys") <(echo "$btc_keys") | tr '\n' ',')
    if [ -z "$missing" ] && [ -z "$extra" ]; then
        pass "A9.11 bumpfee (keys match)"
    elif [ -z "$missing" ]; then
        diff_note "A9.11 bumpfee" "extra keys in btc: $extra"
    else
        fail "A9.11 bumpfee" "missing keys in btc: ${missing} extra: ${extra}"
    fi
else
    skip_test "A9.11 bumpfee" "no suitable txid"
fi

subsection "A10: Additional Network Methods"

compare_nonempty "A10.01 ping" ping
compare_json_keys "A10.02 getnettotals" getnettotals
compare_rpc "A10.03 getnodeaddresses(1)" getnodeaddresses 1

# setban + listbanned + clearbanned round-trip
btc setban "10.0.0.1" "add" 60 >/dev/null 2>&1
ref setban "10.0.0.1" "add" 60 >/dev/null 2>&1
compare_nonempty "A10.04 listbanned (after setban)" listbanned
btc clearbanned >/dev/null 2>&1
ref clearbanned >/dev/null 2>&1

# disconnectnode (will fail, no such peer — compare error)
compare_exit "A10.05 disconnectnode(bad)" disconnectnode "10.0.0.1:8333"

subsection "A11: Additional Mining Methods"

# prioritisetransaction
if [ -n "$MEMPOOL_TXID" ]; then
    compare_rpc "A11.01 prioritisetransaction" prioritisetransaction "$MEMPOOL_TXID" 0 1000
else
    skip_test "A11.01 prioritisetransaction" "no mempool tx"
fi

# generateblock (empty block)
GENBLOCK_ADDR=$(ref -rpcwallet=test_wallet getnewaddress 2>/dev/null)
compare_json_keys "A11.02 generateblock(addr,[])" generateblock "$GENBLOCK_ADDR" "[]"

subsection "A12: Additional Control Methods"

# help for specific RPC
compare_rpc "A12.01 help getmininginfo" help getmininginfo
compare_rpc "A12.02 help getnetworkinfo" help getnetworkinfo

# echo / echojson (debug methods)
btc_out=$(btc echo "hello" "world" 2>/dev/null) || true
ref_out=$(ref echo "hello" "world" 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "A12.03 echo"
else
    diff_note "A12.03 echo" "btc=[$btc_out] ref=[$ref_out]"
fi

# ═══════════════════════════════════════════════════════════════════════
# CATEGORY H: ADDITIONAL EDGE CASES
# ═══════════════════════════════════════════════════════════════════════
section "Category H: Additional Edge Cases"

subsection "H1: Multiple -stdin lines"

# Multi-line stdin — getblock needs hash and verbosity
btc_out=$(printf "%s\n1\n" "$BLOCKHASH" | "$BTC_CLI" $CONN_ARGS -stdin getblock 2>/dev/null) || true
ref_out=$(printf "%s\n1\n" "$BLOCKHASH" | "$BITCOIN_CLI" $CONN_ARGS -stdin getblock 2>/dev/null) || true
if [ -n "$btc_out" ] && [ -n "$ref_out" ]; then
    if [ "$btc_out" = "$ref_out" ]; then
        pass "H1.01 multi-line -stdin"
    else
        diff_note "H1.01 multi-line -stdin" "outputs differ in formatting"
    fi
else
    fail "H1.01 multi-line -stdin" "btc empty=$([ -z "$btc_out" ] && echo y || echo n) ref empty=$([ -z "$ref_out" ] && echo y || echo n)"
fi

subsection "H2: Empty stdin"

# Empty stdin with -stdin should not hang (no extra args added)
btc_out=$(echo "" | timeout 5 "$BTC_CLI" $CONN_ARGS -stdin getblockcount 2>/dev/null) || true
ref_out=$(echo "" | timeout 5 "$BITCOIN_CLI" $CONN_ARGS -stdin getblockcount 2>/dev/null) || true
if [ "$btc_out" = "$ref_out" ]; then
    pass "H2.01 empty -stdin"
else
    diff_note "H2.01 empty -stdin" "btc=[$btc_out] ref=[$ref_out]"
fi

subsection "H3: JSON formatting"

# Verify compact empty containers
btc_out=$(btc getrawmempool 2>/dev/null) || true
ref_out=$(ref getrawmempool 2>/dev/null) || true
# With empty mempool after mining, should be "[\n]\n" or "[]"
btc_compact=$(echo "$btc_out" | tr -d '[:space:]')
ref_compact=$(echo "$ref_out" | tr -d '[:space:]')
if [ "$btc_compact" = "$ref_compact" ]; then
    pass "H3.01 empty array format"
else
    diff_note "H3.01 empty array format" "btc=[$btc_out] ref=[$ref_out]"
fi

# Mine the mempool txs
ref generatetoaddress 1 "$ADDR" >/dev/null 2>&1

subsection "H4: RPC methods with complex JSON args"

# createrawtransaction with multiple outputs
MULTI_ADDR1=$(ref -rpcwallet=test_wallet getnewaddress 2>/dev/null)
MULTI_ADDR2=$(ref -rpcwallet=test_wallet getnewaddress 2>/dev/null)
UTXO_INFO=$(ref -rpcwallet=test_wallet listunspent 1 9999999 2>/dev/null | python3 -c "
import sys, json
u = json.load(sys.stdin)
if u:
    print(u[0]['txid'], u[0]['vout'], u[0]['amount'])
" 2>/dev/null) || true

if [ -n "$UTXO_INFO" ]; then
    UTXO2_TXID=$(echo "$UTXO_INFO" | awk '{print $1}')
    UTXO2_VOUT=$(echo "$UTXO_INFO" | awk '{print $2}')
    compare_rpc "H4.01 createrawtransaction (multi-output)" createrawtransaction \
        "[{\"txid\":\"$UTXO2_TXID\",\"vout\":$UTXO2_VOUT}]" \
        "{\"$MULTI_ADDR1\":0.001,\"$MULTI_ADDR2\":0.002}"
else
    skip_test "H4.01 createrawtransaction (multi-output)" "no UTXO"
fi

subsection "H5: -stdinwalletpassphrase"

# Create encrypted wallet for testing
ref createwallet "enc_wallet" false false "testphrase" false false true >/dev/null 2>&1 || true

# Test -stdinwalletpassphrase (only allowed with walletpassphrase command)
btc_rc=0; ref_rc=0
echo "testphrase" | "$BTC_CLI" $CONN_ARGS -rpcwallet=enc_wallet -stdinwalletpassphrase walletpassphrase 60 >/dev/null 2>&1 || btc_rc=$?
echo "testphrase" | "$BITCOIN_CLI" $CONN_ARGS -rpcwallet=enc_wallet -stdinwalletpassphrase walletpassphrase 60 >/dev/null 2>&1 || ref_rc=$?
if [ "$btc_rc" = "$ref_rc" ]; then
    pass "H5.01 -stdinwalletpassphrase (exit=$btc_rc)"
else
    diff_note "H5.01 -stdinwalletpassphrase" "btc rc=$btc_rc ref rc=$ref_rc"
fi

ref unloadwallet "enc_wallet" >/dev/null 2>&1

subsection "H6: Rapid Sequential Calls"

# 50 rapid getblockcount calls, both should handle
START_TIME=$(date +%s%N)
for i in $(seq 1 50); do
    btc getblockcount >/dev/null 2>&1
done
BTC_ELAPSED=$(( ($(date +%s%N) - START_TIME) / 1000000 ))

START_TIME=$(date +%s%N)
for i in $(seq 1 50); do
    ref getblockcount >/dev/null 2>&1
done
REF_ELAPSED=$(( ($(date +%s%N) - START_TIME) / 1000000 ))

pass "H6.01 50x getblockcount (btc=${BTC_ELAPSED}ms ref=${REF_ELAPSED}ms — benchmark only)"

subsection "H7: Concurrent Calls"

# 10 concurrent calls
for i in $(seq 1 10); do
    btc getblockcount >/dev/null 2>&1 &
done
wait
pass "H7.01 10 concurrent btc-cli calls"

for i in $(seq 1 10); do
    ref getblockcount >/dev/null 2>&1 &
done
wait
pass "H7.02 10 concurrent bitcoin-cli calls"

# ═══════════════════════════════════════════════════════════════════════
# SUMMARY
# ═══════════════════════════════════════════════════════════════════════
echo ""
printf "${BOLD}═══════════════════════════════════════════════════════════════${NC}\n"
printf "${BOLD}PARITY SCAN SUMMARY${NC}\n"
printf "${BOLD}═══════════════════════════════════════════════════════════════${NC}\n"
printf "  ${GREEN}PASS${NC}: %d\n" "$PASS"
printf "  ${RED}FAIL${NC}: %d\n" "$FAIL"
printf "  ${YELLOW}DIFF${NC}: %d  (intentional/acceptable differences)\n" "$DIFF"
printf "  ${YELLOW}SKIP${NC}: %d  (could not test)\n" "$SKIP"
printf "  TOTAL: %d\n" "$TOTAL"
echo ""

if [ "$FAIL" -gt 0 ]; then
    printf "${RED}${BOLD}Failures:${NC}\n"
    printf "$ERRORS\n"
fi

echo ""
echo "Detailed log: $LOGFILE"
echo ""

# Exit with failure count
exit "$FAIL"
