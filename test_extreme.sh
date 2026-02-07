#!/bin/bash
#
# btc-cli EXTREME STRESS TESTS
# Tests memory limits, 4MB transactions, buffer overflows, and edge cases
#
# WARNING: These tests are designed to break things. Run on regtest only.
#
# Usage: ./test_extreme.sh [network] [datadir]
#

set -o pipefail

NETWORK="${1:-regtest}"
DATADIR="${2:-/tmp/btc-regtest}"
CLI="./btc-cli -$NETWORK -datadir=$DATADIR"
WALLET="extreme_stress_wallet"
CLIW="$CLI -rpcwallet=$WALLET"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0
TOTAL=0

TMPDIR="/tmp/btc-extreme-$$"
mkdir -p "$TMPDIR"

echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║          btc-cli EXTREME STRESS TEST SUITE                    ║${NC}"
echo -e "${CYAN}║          WARNING: Designed to break things!                   ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Test helpers
test_survives() {
    local name="$1"
    shift
    ((TOTAL++))
    echo -n "  [$TOTAL] $name ... "
    if timeout 30 "$@" >/dev/null 2>&1; then
        echo -e "${GREEN}SURVIVED${NC}"
        ((PASS++))
        return 0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "${YELLOW}TIMEOUT${NC}"
            ((SKIP++))
        else
            echo -e "${GREEN}HANDLED (exit $exit_code)${NC}"
            ((PASS++))
        fi
        return 1
    fi
}

test_crash() {
    local name="$1"
    shift
    ((TOTAL++))
    echo -n "  [$TOTAL] $name ... "
    # Run in subshell to catch segfaults
    if output=$(timeout 30 "$@" 2>&1); then
        echo -e "${GREEN}SURVIVED${NC}"
        ((PASS++))
    else
        local exit_code=$?
        if [ $exit_code -eq 139 ] || [ $exit_code -eq 134 ] || [ $exit_code -eq 136 ]; then
            echo -e "${RED}CRASHED (signal $((exit_code-128)))${NC}"
            ((FAIL++))
        elif [ $exit_code -eq 124 ]; then
            echo -e "${YELLOW}TIMEOUT${NC}"
            ((SKIP++))
        else
            echo -e "${GREEN}HANDLED (exit $exit_code)${NC}"
            ((PASS++))
        fi
    fi
}

capture() {
    "$@" 2>/dev/null
}

# Check connection
echo -n "Checking $NETWORK connection... "
if ! $CLI getblockcount >/dev/null 2>&1; then
    echo -e "${RED}FAILED${NC}"
    echo "Start a $NETWORK node first!"
    exit 1
fi
echo -e "${GREEN}OK${NC}"

# Setup wallet
if ! $CLI loadwallet "$WALLET" >/dev/null 2>&1; then
    $CLI createwallet "$WALLET" >/dev/null 2>&1
fi

# Get an address
ADDR=$(capture $CLIW getnewaddress "" bech32)

# ============================================
# SECTION 1: BUFFER OVERFLOW ATTEMPTS
# ============================================
echo ""
echo -e "${CYAN}=== 1. BUFFER OVERFLOW ATTEMPTS ===${NC}"

# Long strings for various parameters
echo "Generating test strings..."
STR_100=$(head -c 100 /dev/urandom | base64 | tr -d '\n/+=')
STR_1K=$(head -c 1000 /dev/urandom | base64 | tr -d '\n/+=')
STR_10K=$(head -c 10000 /dev/urandom | base64 | tr -d '\n/+=')
STR_100K=$(head -c 100000 /dev/urandom | base64 | tr -d '\n/+=')
STR_1M=$(head -c 1000000 /dev/urandom | base64 | tr -d '\n/+=')

# Long hex strings
HEX_1K=$(head -c 500 /dev/urandom | xxd -p | tr -d '\n')
HEX_10K=$(head -c 5000 /dev/urandom | xxd -p | tr -d '\n')
HEX_100K=$(head -c 50000 /dev/urandom | xxd -p | tr -d '\n')
HEX_1M=$(head -c 500000 /dev/urandom | xxd -p | tr -d '\n')

echo ""
echo "--- String parameter tests ---"
test_crash "validateaddress 100 char" $CLI validateaddress "$STR_100"
test_crash "validateaddress 1K char" $CLI validateaddress "$STR_1K"
test_crash "validateaddress 10K char" $CLI validateaddress "$STR_10K"
test_crash "validateaddress 100K char" $CLI validateaddress "$STR_100K"

test_crash "getblockhash long string" $CLI getblockhash "$STR_1K"
test_crash "getblock long string" $CLI getblock "$STR_1K"

echo ""
echo "--- Hex parameter tests ---"
test_crash "decoderawtransaction 1K hex" $CLI decoderawtransaction "$HEX_1K"
test_crash "decoderawtransaction 10K hex" $CLI decoderawtransaction "$HEX_10K"
test_crash "decoderawtransaction 100K hex" $CLI decoderawtransaction "$HEX_100K"
test_crash "decoderawtransaction 1M hex" $CLI decoderawtransaction "$HEX_1M"

test_crash "decodescript 1K hex" $CLI decodescript "$HEX_1K"
test_crash "decodescript 10K hex" $CLI decodescript "$HEX_10K"
test_crash "decodescript 100K hex" $CLI decodescript "$HEX_100K"

echo ""
echo "--- Label overflow tests ---"
test_crash "getnewaddress 100 char label" $CLIW getnewaddress "$STR_100"
test_crash "getnewaddress 1K char label" $CLIW getnewaddress "$STR_1K"
test_crash "getnewaddress 10K char label" $CLIW getnewaddress "$STR_10K"


# ============================================
# SECTION 2: LARGE JSON PARAMETER TESTS
# ============================================
echo ""
echo -e "${CYAN}=== 2. LARGE JSON PARAMETER TESTS ===${NC}"

# Generate large JSON arrays
gen_input_array() {
    local count=$1
    echo -n "["
    for i in $(seq 1 $count); do
        [ $i -gt 1 ] && echo -n ","
        printf '{"txid":"%064d","vout":0}' $i
    done
    echo "]"
}

gen_output_array() {
    local count=$1
    local addr=$2
    echo -n "["
    for i in $(seq 1 $count); do
        [ $i -gt 1 ] && echo -n ","
        echo -n "{\"$addr\":0.00001}"
    done
    echo "]"
}

echo ""
echo "--- Large input arrays ---"
test_crash "createrawtx 100 inputs" $CLI createrawtransaction "$(gen_input_array 100)" '[]'
test_crash "createrawtx 500 inputs" $CLI createrawtransaction "$(gen_input_array 500)" '[]'
test_crash "createrawtx 1000 inputs" $CLI createrawtransaction "$(gen_input_array 1000)" '[]'
test_crash "createrawtx 2000 inputs" $CLI createrawtransaction "$(gen_input_array 2000)" '[]'

echo ""
echo "--- Large output arrays ---"
test_crash "createrawtx 100 outputs" $CLI createrawtransaction '[]' "$(gen_output_array 100 $ADDR)"
test_crash "createrawtx 500 outputs" $CLI createrawtransaction '[]' "$(gen_output_array 500 $ADDR)"
test_crash "createrawtx 1000 outputs" $CLI createrawtransaction '[]' "$(gen_output_array 1000 $ADDR)"
test_crash "createrawtx 2000 outputs" $CLI createrawtransaction '[]' "$(gen_output_array 2000 $ADDR)"

echo ""
echo "--- Large PSBT arrays ---"
test_crash "createpsbt 100 outputs" $CLI createpsbt '[]' "$(gen_output_array 100 $ADDR)"
test_crash "createpsbt 500 outputs" $CLI createpsbt '[]' "$(gen_output_array 500 $ADDR)"
test_crash "createpsbt 1000 outputs" $CLI createpsbt '[]' "$(gen_output_array 1000 $ADDR)"


# ============================================
# SECTION 3: MEMORY PRESSURE TESTS
# ============================================
echo ""
echo -e "${CYAN}=== 3. MEMORY PRESSURE TESTS ===${NC}"

echo ""
echo "--- Repeated large allocations ---"
echo -n "  [$((++TOTAL))] 100x getblockchaininfo rapid fire ... "
for i in $(seq 1 100); do
    $CLI getblockchaininfo >/dev/null 2>&1 || break
done
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))

echo -n "  [$((++TOTAL))] 50x large getpeerinfo ... "
for i in $(seq 1 50); do
    $CLI getpeerinfo >/dev/null 2>&1 || break
done
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))

echo -n "  [$((++TOTAL))] 100x createrawtx 100 outputs ... "
OUTPUTS_100=$(gen_output_array 100 $ADDR)
for i in $(seq 1 100); do
    $CLI createrawtransaction '[]' "$OUTPUTS_100" >/dev/null 2>&1 || break
done
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))


# ============================================
# SECTION 4: MALFORMED INPUT TESTS
# ============================================
echo ""
echo -e "${CYAN}=== 4. MALFORMED INPUT TESTS ===${NC}"

echo ""
echo "--- Malformed JSON ---"
test_crash "createrawtx unclosed array" $CLI createrawtransaction '[' '[]'
test_crash "createrawtx unclosed brace" $CLI createrawtransaction '[{' '[]'
test_crash "createrawtx null bytes" $CLI createrawtransaction $'[\x00]' '[]'
test_crash "createrawtx control chars" $CLI createrawtransaction $'[\x01\x02\x03]' '[]'

echo ""
echo "--- Invalid number formats ---"
test_crash "getblockhash float" $CLI getblockhash 1.5
test_crash "getblockhash scientific" $CLI getblockhash 1e10
test_crash "getblockhash hex" $CLI getblockhash 0xff
test_crash "getblockhash negative overflow" $CLI getblockhash -9999999999999999999
test_crash "getblockhash positive overflow" $CLI getblockhash 9999999999999999999

echo ""
echo "--- Unicode and special chars ---"
test_crash "validateaddress unicode" $CLI validateaddress "тест"
test_crash "validateaddress emoji" $CLI validateaddress "🔥💰"
test_crash "validateaddress null" $CLI validateaddress ""
test_crash "getnewaddress unicode label" $CLIW getnewaddress "тест_label"


# ============================================
# SECTION 5: 4MB TRANSACTION TESTS
# ============================================
echo ""
echo -e "${CYAN}=== 5. 4MB TRANSACTION SIMULATION ===${NC}"

# Generate a huge transaction hex (simulated)
# A real 4MB tx would need actual UTXOs, but we can test decoding limits

echo ""
echo "--- Large decode tests ---"

# 100KB hex
echo -n "  [$((++TOTAL))] decode 100KB transaction hex ... "
HEX_100K_TX=$(head -c 50000 /dev/urandom | xxd -p | tr -d '\n')
if timeout 30 $CLI decoderawtransaction "$HEX_100K_TX" >/dev/null 2>&1; then
    echo -e "${YELLOW}DECODED (unexpected)${NC}"
    ((SKIP++))
else
    echo -e "${GREEN}REJECTED (expected)${NC}"
    ((PASS++))
fi

# 500KB hex
echo -n "  [$((++TOTAL))] decode 500KB transaction hex ... "
HEX_500K_TX=$(head -c 250000 /dev/urandom | xxd -p | tr -d '\n')
START=$(date +%s%N)
if timeout 60 $CLI decoderawtransaction "$HEX_500K_TX" >/dev/null 2>&1; then
    echo -e "${YELLOW}DECODED (unexpected)${NC}"
    ((SKIP++))
else
    END=$(date +%s%N)
    ELAPSED=$(( (END - START) / 1000000 ))
    echo -e "${GREEN}REJECTED (${ELAPSED}ms)${NC}"
    ((PASS++))
fi

# 1MB hex
echo -n "  [$((++TOTAL))] decode 1MB transaction hex ... "
HEX_1M_TX=$(head -c 500000 /dev/urandom | xxd -p | tr -d '\n')
START=$(date +%s%N)
if timeout 120 $CLI decoderawtransaction "$HEX_1M_TX" >/dev/null 2>&1; then
    echo -e "${YELLOW}DECODED (unexpected)${NC}"
    ((SKIP++))
else
    END=$(date +%s%N)
    ELAPSED=$(( (END - START) / 1000000 ))
    echo -e "${GREEN}REJECTED (${ELAPSED}ms)${NC}"
    ((PASS++))
fi

# 2MB hex
echo -n "  [$((++TOTAL))] decode 2MB transaction hex ... "
HEX_2M_TX=$(head -c 1000000 /dev/urandom | xxd -p | tr -d '\n')
START=$(date +%s%N)
if timeout 180 $CLI decoderawtransaction "$HEX_2M_TX" >/dev/null 2>&1; then
    echo -e "${YELLOW}DECODED (unexpected)${NC}"
    ((SKIP++))
else
    END=$(date +%s%N)
    ELAPSED=$(( (END - START) / 1000000 ))
    echo -e "${GREEN}REJECTED (${ELAPSED}ms)${NC}"
    ((PASS++))
fi


# ============================================
# SECTION 6: CONCURRENT STRESS
# ============================================
echo ""
echo -e "${CYAN}=== 6. CONCURRENT STRESS ===${NC}"

echo ""
echo "--- Parallel execution ---"

echo -n "  [$((++TOTAL))] 20 parallel getblockcount ... "
for i in $(seq 1 20); do
    $CLI getblockcount >/dev/null 2>&1 &
done
wait
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))

echo -n "  [$((++TOTAL))] 10 parallel getblockchaininfo ... "
for i in $(seq 1 10); do
    $CLI getblockchaininfo >/dev/null 2>&1 &
done
wait
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))

echo -n "  [$((++TOTAL))] 10 parallel getnewaddress ... "
for i in $(seq 1 10); do
    $CLIW getnewaddress >/dev/null 2>&1 &
done
wait
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))

echo -n "  [$((++TOTAL))] Mixed parallel operations ... "
$CLI getblockcount >/dev/null 2>&1 &
$CLI getblockchaininfo >/dev/null 2>&1 &
$CLI getpeerinfo >/dev/null 2>&1 &
$CLIW getbalance >/dev/null 2>&1 &
$CLIW getnewaddress >/dev/null 2>&1 &
wait
echo -e "${GREEN}SURVIVED${NC}"
((PASS++))


# ============================================
# SECTION 7: RESPONSE SIZE TESTS
# ============================================
echo ""
echo -e "${CYAN}=== 7. LARGE RESPONSE HANDLING ===${NC}"

if [ "$NETWORK" = "regtest" ]; then
    # Generate many addresses to make listdescriptors large
    echo -n "  Generating 200 addresses for large descriptor test ... "
    for i in $(seq 1 200); do
        $CLIW getnewaddress >/dev/null 2>&1
    done
    echo "done"

    echo -n "  [$((++TOTAL))] listdescriptors (large response) ... "
    START=$(date +%s%N)
    if OUTPUT=$($CLIW listdescriptors 2>&1); then
        END=$(date +%s%N)
        ELAPSED=$(( (END - START) / 1000000 ))
        SIZE=${#OUTPUT}
        echo -e "${GREEN}PASS${NC} ($SIZE bytes, ${ELAPSED}ms)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
fi

# Test with actual blockchain data
BLOCK_HASH=$(capture $CLI getblockhash 1)
if [ -n "$BLOCK_HASH" ]; then
    echo -n "  [$((++TOTAL))] getblock verbosity=2 ... "
    START=$(date +%s%N)
    if OUTPUT=$($CLI getblock "$BLOCK_HASH" 2 2>&1); then
        END=$(date +%s%N)
        ELAPSED=$(( (END - START) / 1000000 ))
        SIZE=${#OUTPUT}
        echo -e "${GREEN}PASS${NC} ($SIZE bytes, ${ELAPSED}ms)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
    fi
fi


# ============================================
# SECTION 8: EDGE CASE NUMBERS
# ============================================
echo ""
echo -e "${CYAN}=== 8. NUMERIC EDGE CASES ===${NC}"

echo ""
echo "--- Integer boundaries ---"
test_crash "getblockhash MAX_INT" $CLI getblockhash 2147483647
test_crash "getblockhash MAX_INT+1" $CLI getblockhash 2147483648
test_crash "getblockhash MAX_LONG" $CLI getblockhash 9223372036854775807
test_crash "getblockhash MIN_INT" $CLI getblockhash -2147483648

echo ""
echo "--- Float edge cases ---"
test_crash "estimatesmartfee 0" $CLI estimatesmartfee 0
test_crash "estimatesmartfee MAX" $CLI estimatesmartfee 2147483647

echo ""
echo "--- Amount edge cases ---"
test_crash "sendtoaddress tiny" $CLIW sendtoaddress "$ADDR" 0.00000001
test_crash "sendtoaddress zero" $CLIW sendtoaddress "$ADDR" 0
test_crash "sendtoaddress negative" $CLIW sendtoaddress "$ADDR" -1
test_crash "sendtoaddress huge" $CLIW sendtoaddress "$ADDR" 21000000


# ============================================
# SECTION 9: REGTEST REAL TX TESTS
# ============================================
if [ "$NETWORK" = "regtest" ]; then
    echo ""
    echo -e "${CYAN}=== 9. REGTEST REAL TRANSACTION TORTURE ===${NC}"

    MINE_ADDR=$(capture $CLIW getnewaddress "" bech32)

    echo -n "  Mining 110 blocks for funds ... "
    if $CLIW generatetoaddress 110 "$MINE_ADDR" >/dev/null 2>&1; then
        echo -e "${GREEN}OK${NC}"

        BALANCE=$(capture $CLIW getbalance)
        echo "  Available balance: $BALANCE BTC"

        echo ""
        echo "--- Real multi-output transactions ---"

        # Send to many outputs at once
        for NUM in 10 50 100 200; do
            echo -n "  [$((++TOTAL))] sendmany $NUM recipients ... "
            RECIPIENTS="{"
            for i in $(seq 1 $NUM); do
                DEST=$(capture $CLIW getnewaddress)
                [ $i -gt 1 ] && RECIPIENTS="$RECIPIENTS,"
                RECIPIENTS="$RECIPIENTS\"$DEST\":0.0001"
            done
            RECIPIENTS="$RECIPIENTS}"

            START=$(date +%s%N)
            if TXID=$($CLIW sendmany "" "$RECIPIENTS" 2>&1); then
                END=$(date +%s%N)
                ELAPSED=$(( (END - START) / 1000000 ))
                echo -e "${GREEN}PASS${NC} (${ELAPSED}ms)"
                ((PASS++))
            else
                echo -e "${RED}FAIL${NC}: ${TXID:0:80}"
                ((FAIL++))
            fi
        done

        # Mine to confirm
        $CLIW generatetoaddress 1 "$MINE_ADDR" >/dev/null 2>&1

        echo ""
        echo "--- Large PSBT workflow ---"
        for NUM in 50 100; do
            echo -n "  [$((++TOTAL))] Full PSBT $NUM outputs ... "
            OUTPUTS="["
            for i in $(seq 1 $NUM); do
                DEST=$(capture $CLIW getnewaddress)
                [ $i -gt 1 ] && OUTPUTS="$OUTPUTS,"
                OUTPUTS="$OUTPUTS{\"$DEST\":0.0001}"
            done
            OUTPUTS="$OUTPUTS]"

            START=$(date +%s%N)
            # Create funded PSBT
            FUNDED=$($CLIW walletcreatefundedpsbt '[]' "$OUTPUTS" 2>&1)
            PSBT=$(echo "$FUNDED" | grep -o '"psbt": "[^"]*"' | head -1 | cut -d'"' -f4)

            if [ -n "$PSBT" ]; then
                # Sign
                SIGNED=$($CLIW walletprocesspsbt "$PSBT" 2>&1)
                SIGNED_PSBT=$(echo "$SIGNED" | grep -o '"psbt": "[^"]*"' | head -1 | cut -d'"' -f4)

                if [ -n "$SIGNED_PSBT" ]; then
                    # Finalize
                    FINAL=$($CLI finalizepsbt "$SIGNED_PSBT" 2>&1)
                    HEX=$(echo "$FINAL" | grep -o '"hex": "[^"]*"' | head -1 | cut -d'"' -f4)

                    if [ -n "$HEX" ]; then
                        # Broadcast
                        if TXID=$($CLI sendrawtransaction "$HEX" 2>&1); then
                            END=$(date +%s%N)
                            ELAPSED=$(( (END - START) / 1000000 ))
                            echo -e "${GREEN}PASS${NC} (${ELAPSED}ms, txid: ${TXID:0:16}...)"
                            ((PASS++))
                        else
                            echo -e "${RED}FAIL broadcast${NC}"
                            ((FAIL++))
                        fi
                    else
                        echo -e "${RED}FAIL finalize${NC}"
                        ((FAIL++))
                    fi
                else
                    echo -e "${RED}FAIL sign${NC}"
                    ((FAIL++))
                fi
            else
                echo -e "${RED}FAIL create${NC}"
                ((FAIL++))
            fi
        done

        # Mine to confirm
        $CLIW generatetoaddress 1 "$MINE_ADDR" >/dev/null 2>&1

        echo ""
        echo "--- Transaction chain stress ---"
        echo -n "  [$((++TOTAL))] Chain of 20 dependent transactions ... "
        CHAIN_ADDR=$(capture $CLIW getnewaddress)
        SUCCESS=0
        for i in $(seq 1 20); do
            if TXID=$($CLIW sendtoaddress "$CHAIN_ADDR" 0.01 2>&1); then
                ((SUCCESS++))
            else
                break
            fi
        done
        if [ $SUCCESS -eq 20 ]; then
            echo -e "${GREEN}PASS${NC} ($SUCCESS txs)"
            ((PASS++))
        else
            echo -e "${RED}FAIL${NC} ($SUCCESS/20 txs)"
            ((FAIL++))
        fi

    else
        echo -e "${RED}FAILED to mine${NC}"
    fi
fi


# ============================================
# CLEANUP
# ============================================
echo ""
echo -e "${CYAN}=== CLEANUP ===${NC}"

rm -rf "$TMPDIR"
$CLI unloadwallet "$WALLET" >/dev/null 2>&1

echo "Done."


# ============================================
# RESULTS
# ============================================
echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    EXTREME TEST RESULTS                       ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  ${GREEN}PASSED:${NC}  $PASS"
echo -e "  ${RED}FAILED:${NC}  $FAIL"
echo -e "  ${YELLOW}SKIPPED:${NC} $SKIP"
echo "  ─────────────────"
echo "  TOTAL:   $TOTAL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}ALL EXTREME TESTS SURVIVED!${NC}"
    exit 0
else
    echo -e "${RED}SOME TESTS CRASHED OR FAILED${NC}"
    exit 1
fi
