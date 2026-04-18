#!/bin/sh
# smoke tests for cpubar. keep it simple.
set -eu

BIN=./cpubar
PASS=0
FAIL=0

check() {
    name=$1
    shift
    if "$@"; then
        echo "ok  - $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL - $name"
        FAIL=$((FAIL + 1))
    fi
}

# 1: --version prints version
check "version flag works" sh -c '$0 --version | grep -q "cpubar "' "$BIN"

# 2: --help prints usage
check "help flag works" sh -c '$0 --help | grep -q "usage:"' "$BIN"

# 3: runs twice and exits, output has core 0
OUT=$("$BIN" -n 2 -i 0.1)
check "runs n=2 and prints core 0" sh -c 'echo "$1" | grep -q "core 0"' _ "$OUT"

# 4: bar width flag
OUT2=$("$BIN" -n 1 -i 0.1 -w 5)
check "custom width runs" sh -c 'echo "$1" | grep -q "core 0"' _ "$OUT2"

# 5: bad flag exits nonzero
if "$BIN" --nope >/dev/null 2>&1; then
    echo "FAIL - bad flag should exit nonzero"
    FAIL=$((FAIL + 1))
else
    echo "ok  - bad flag exits nonzero"
    PASS=$((PASS + 1))
fi

echo ""
echo "passed: $PASS  failed: $FAIL"
[ "$FAIL" -eq 0 ]
