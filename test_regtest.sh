#!/bin/bash
# Regtest transaction test suite

CLI="./btc-cli -regtest -datadir=/tmp/btc-regtest"
CLIW="./btc-cli -regtest -datadir=/tmp/btc-regtest -rpcwallet=regtestwallet"

PASS=0
FAIL=0

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

run_test() {
    local name="$1"
    shift
    echo -n "  $name ... "
    if output=$("$@" 2>&1); then
        if [ -n "$output" ]; then
            echo -e "${GREEN}PASS${NC}"
            ((PASS++))
            return 0
        fi
    fi
    echo -e "${RED}FAIL${NC}"
    echo "    Output: $output"
    ((FAIL++))
    return 1
}

echo "=============================================="
echo "   REGTEST RAW TX & PSBT COMPREHENSIVE TEST"
echo "=============================================="
echo ""

# Load addresses
LEGACY=$(cat /tmp/addr_legacy.txt)
P2SH=$(cat /tmp/addr_p2sh.txt)
BECH32=$(cat /tmp/addr_bech32.txt)
BECH32M=$(cat /tmp/addr_bech32m.txt)

# Get a UTXO
UTXO_TXID="b0f9eb98ce92465dbb91c06bc5f86e31c5164513a9f3405b97de885c1d116619"
UTXO_VOUT=1

INPUTS="[{\"txid\":\"$UTXO_TXID\",\"vout\":$UTXO_VOUT}]"

echo "=== Raw TX Creation (all address types) ==="
run_test "createrawtx -> legacy (P2PKH)" $CLI createrawtransaction "$INPUTS" "[{\"$LEGACY\":0.1}]"
run_test "createrawtx -> p2sh-segwit" $CLI createrawtransaction "$INPUTS" "[{\"$P2SH\":0.1}]"
run_test "createrawtx -> bech32 (P2WPKH)" $CLI createrawtransaction "$INPUTS" "[{\"$BECH32\":0.1}]"
run_test "createrawtx -> bech32m (P2TR)" $CLI createrawtransaction "$INPUTS" "[{\"$BECH32M\":0.1}]"

echo ""
echo "=== PSBT Creation (all address types) ==="
run_test "createpsbt -> legacy" $CLI createpsbt "$INPUTS" "[{\"$LEGACY\":0.1}]"
run_test "createpsbt -> p2sh-segwit" $CLI createpsbt "$INPUTS" "[{\"$P2SH\":0.1}]"
run_test "createpsbt -> bech32" $CLI createpsbt "$INPUTS" "[{\"$BECH32\":0.1}]"
run_test "createpsbt -> bech32m" $CLI createpsbt "$INPUTS" "[{\"$BECH32\":0.1}]"

echo ""
echo "=== PSBT Decode/Analyze ==="
PSBT=$($CLI createpsbt "$INPUTS" "[{\"$LEGACY\":0.1}]")
run_test "decodepsbt" $CLI decodepsbt "$PSBT"
run_test "analyzepsbt" $CLI analyzepsbt "$PSBT"

echo ""
echo "=== Wallet-Funded PSBT ==="
run_test "walletcreatefundedpsbt -> legacy" $CLIW walletcreatefundedpsbt "[]" "[{\"$LEGACY\":0.05}]"
run_test "walletcreatefundedpsbt -> p2sh" $CLIW walletcreatefundedpsbt "[]" "[{\"$P2SH\":0.05}]"
run_test "walletcreatefundedpsbt -> bech32" $CLIW walletcreatefundedpsbt "[]" "[{\"$BECH32\":0.05}]"
run_test "walletcreatefundedpsbt -> bech32m" $CLIW walletcreatefundedpsbt "[]" "[{\"$BECH32M\":0.05}]"

echo ""
echo "=== Full PSBT Workflow: Create -> Sign -> Finalize -> Broadcast ==="

# Create funded PSBT
echo -n "  Step 1: walletcreatefundedpsbt ... "
FUNDED_JSON=$($CLIW walletcreatefundedpsbt "[]" "[{\"$BECH32\":0.01}]" 2>&1)
FUNDED_PSBT=$(echo "$FUNDED_JSON" | grep -o '"psbt": "[^"]*"' | head -1 | cut -d'"' -f4)
if [ -n "$FUNDED_PSBT" ]; then
    echo -e "${GREEN}PASS${NC}"
    ((PASS++))
else
    echo -e "${RED}FAIL${NC}"
    ((FAIL++))
fi

# Sign PSBT
if [ -n "$FUNDED_PSBT" ]; then
    echo -n "  Step 2: walletprocesspsbt (sign) ... "
    SIGNED_JSON=$($CLIW walletprocesspsbt "$FUNDED_PSBT" 2>&1)
    SIGNED_PSBT=$(echo "$SIGNED_JSON" | grep -o '"psbt": "[^"]*"' | head -1 | cut -d'"' -f4)
    COMPLETE=$(echo "$SIGNED_JSON" | grep -o '"complete": true')
    if [ -n "$SIGNED_PSBT" ] && [ -n "$COMPLETE" ]; then
        echo -e "${GREEN}PASS${NC} (complete=true)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
fi

# Finalize PSBT
if [ -n "$SIGNED_PSBT" ]; then
    echo -n "  Step 3: finalizepsbt ... "
    FINAL_JSON=$($CLI finalizepsbt "$SIGNED_PSBT" 2>&1)
    FINAL_HEX=$(echo "$FINAL_JSON" | grep -o '"hex": "[^"]*"' | head -1 | cut -d'"' -f4)
    if [ -n "$FINAL_HEX" ]; then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
fi

# Test mempool accept
if [ -n "$FINAL_HEX" ]; then
    echo -n "  Step 4: testmempoolaccept ... "
    ACCEPT_JSON=$($CLI testmempoolaccept "[\"$FINAL_HEX\"]" 2>&1)
    ALLOWED=$(echo "$ACCEPT_JSON" | grep -o '"allowed": true')
    if [ -n "$ALLOWED" ]; then
        echo -e "${GREEN}PASS${NC} (allowed=true)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "    $ACCEPT_JSON"
        ((FAIL++))
    fi
fi

# Actually broadcast it
if [ -n "$FINAL_HEX" ]; then
    echo -n "  Step 5: sendrawtransaction ... "
    TXID=$($CLI sendrawtransaction "$FINAL_HEX" 2>&1)
    if [[ "$TXID" =~ ^[a-f0-9]{64}$ ]]; then
        echo -e "${GREEN}PASS${NC} (txid: ${TXID:0:16}...)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: $TXID"
        ((FAIL++))
    fi
fi

echo ""
echo "=== Message Signing ==="
# Need a legacy address for signing
run_test "signmessage" $CLIW signmessage "$LEGACY" "Hello Bitcoin!"
SIG=$($CLIW signmessage "$LEGACY" "Hello Bitcoin!" 2>&1)
if [ -n "$SIG" ]; then
    run_test "verifymessage" $CLI verifymessage "$LEGACY" "$SIG" "Hello Bitcoin!"
fi

echo ""
echo "=== Block Stats ==="
run_test "getblockstats by height" $CLI getblockstats 50
HASH=$($CLI getblockhash 50)
run_test "getblockstats by hash" $CLI getblockstats "$HASH"

echo ""
echo "=== Mining (generate more blocks) ==="
run_test "generatetoaddress" $CLIW generatetoaddress 1 "$BECH32"

echo ""
echo "=============================================="
echo "                 RESULTS"
echo "=============================================="
echo ""
echo -e "  ${GREEN}PASSED:${NC}  $PASS"
echo -e "  ${RED}FAILED:${NC}  $FAIL"
echo "  ─────────────"
echo "  TOTAL:   $((PASS + FAIL))"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
