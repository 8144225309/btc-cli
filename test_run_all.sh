#!/bin/bash
#
# btc-cli MASTER TEST RUNNER
# Runs all test suites and aggregates results
#
# Usage: ./test_run_all.sh [network]
#

NETWORK="${1:-signet}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║            btc-cli MASTER TEST RUNNER                         ║${NC}"
echo -e "${CYAN}║            Network: $NETWORK                                    ${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""

RESULTS_DIR="/tmp/btc-cli-tests-$$"
mkdir -p "$RESULTS_DIR"

run_test_suite() {
    local name="$1"
    local script="$2"
    local log="$RESULTS_DIR/$name.log"

    echo -e "${CYAN}Running: $name${NC}"
    echo "Log: $log"
    echo ""

    if [ -x "$script" ]; then
        if "$script" "$NETWORK" > "$log" 2>&1; then
            echo -e "${GREEN}$name: PASSED${NC}"
            return 0
        else
            echo -e "${RED}$name: FAILED${NC}"
            echo "See log for details: $log"
            return 1
        fi
    else
        chmod +x "$script"
        if "$script" "$NETWORK" > "$log" 2>&1; then
            echo -e "${GREEN}$name: PASSED${NC}"
            return 0
        else
            echo -e "${RED}$name: FAILED${NC}"
            return 1
        fi
    fi
}

TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0

# Run basic test first
echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
((TOTAL_SUITES++))
if run_test_suite "Basic Tests" "./test_btc_cli.sh"; then
    ((PASSED_SUITES++))
else
    ((FAILED_SUITES++))
fi
echo ""

# Run method coverage test
echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
((TOTAL_SUITES++))
if run_test_suite "Method Coverage" "./test_all_methods.sh"; then
    ((PASSED_SUITES++))
else
    ((FAILED_SUITES++))
fi
echo ""

# Run torture test
echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
((TOTAL_SUITES++))
if run_test_suite "Torture Tests" "./test_torture.sh"; then
    ((PASSED_SUITES++))
else
    ((FAILED_SUITES++))
fi
echo ""

# Run extreme tests only on regtest (they're destructive)
if [ "$NETWORK" = "regtest" ]; then
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    ((TOTAL_SUITES++))
    if run_test_suite "Extreme Stress" "./test_extreme.sh"; then
        ((PASSED_SUITES++))
    else
        ((FAILED_SUITES++))
    fi
    echo ""
fi

# Run regtest-specific tests
if [ "$NETWORK" = "regtest" ] && [ -f "./test_regtest.sh" ]; then
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    ((TOTAL_SUITES++))
    if run_test_suite "Regtest TX Tests" "./test_regtest.sh"; then
        ((PASSED_SUITES++))
    else
        ((FAILED_SUITES++))
    fi
    echo ""
fi

# Summary
echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    MASTER TEST RESULTS                        ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  ${GREEN}PASSED SUITES:${NC} $PASSED_SUITES"
echo -e "  ${RED}FAILED SUITES:${NC} $FAILED_SUITES"
echo "  ─────────────────"
echo "  TOTAL SUITES:  $TOTAL_SUITES"
echo ""
echo "Test logs saved to: $RESULTS_DIR"
echo ""

# Extract counts from each log
echo "Individual test counts:"
for log in "$RESULTS_DIR"/*.log; do
    name=$(basename "$log" .log)
    passed=$(grep -oE 'PASSED:[[:space:]]*[0-9]+' "$log" | grep -oE '[0-9]+' | tail -1)
    failed=$(grep -oE 'FAILED:[[:space:]]*[0-9]+' "$log" | grep -oE '[0-9]+' | tail -1)
    echo "  $name: ${passed:-0} passed, ${failed:-0} failed"
done

echo ""
if [ $FAILED_SUITES -eq 0 ]; then
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║              ALL TEST SUITES PASSED!                          ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${RED}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║              SOME TEST SUITES FAILED                          ║${NC}"
    echo -e "${RED}╚═══════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
