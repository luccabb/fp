#!/bin/sh
# fp test suite
set -e

PASS=0
FAIL=0
FP=${FP:-./fp}

ok() {
	PASS=$((PASS + 1))
	printf "  \033[32mPASS\033[0m %s\n" "$1"
}

fail() {
	FAIL=$((FAIL + 1))
	printf "  \033[31mFAIL\033[0m %s\n" "$1"
}

assert() {
	desc="$1"; shift
	if "$@" >/dev/null 2>&1; then ok "$desc"; else fail "$desc"; fi
}

assert_fail() {
	desc="$1"; shift
	if "$@" >/dev/null 2>&1; then fail "$desc"; else ok "$desc"; fi
}

is_number()  { echo "$1" | grep -qE '^[0-9]+$'; }
is_semver()  { echo "$1" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; }
in_range()   { [ "$1" -ge "$2" ] && [ "$1" -le "$3" ]; }
line_count() { echo "$1" | wc -l | tr -d ' '; }
uniq_count() { echo "$1" | sort -u | wc -l | tr -d ' '; }

# ── basic ────────────────────────────────────────────────────────
echo "== basic =="
PORT=$($FP)
assert "outputs a number"       is_number "$PORT"
assert "port in valid range"    in_range "$PORT" 1 65535

# ── version ──────────────────────────────────────────────────────
echo "== version =="
VER=$($FP -v)
assert "version is semver"      is_semver "$VER"

# ── multiple ports ───────────────────────────────────────────────
echo "== multiple ports (-n) =="
PORTS=$($FP -n 10)
assert "returns 10 ports"       [ "$(line_count "$PORTS")" -eq 10 ]
assert "all 10 are unique"      [ "$(uniq_count "$PORTS")" -eq 10 ]

# ── range mode ───────────────────────────────────────────────────
echo "== range mode (-r) =="
PORT=$($FP -r 9000:9100)
assert "range port is number"   is_number "$PORT"
assert "port 9000-9100"         in_range "$PORT" 9000 9100

PORTS=$($FP -n 5 -r 10000:10050)
for p in $PORTS; do
	assert "range port $p ok"   in_range "$p" 10000 10050
done

# ── UDP mode ─────────────────────────────────────────────────────
echo "== UDP mode (-u) =="
PORT=$($FP -u)
assert "UDP port is number"     is_number "$PORT"
assert "UDP port in range"      in_range "$PORT" 1 65535

# ── IPv6 ─────────────────────────────────────────────────────────
echo "== IPv6 (-6) =="
if $FP -6 >/dev/null 2>&1; then
	PORT=$($FP -6)
	assert "IPv6 port is number"    is_number "$PORT"
	assert "IPv6 port in range"     in_range "$PORT" 1 65535
else
	echo "  SKIP: IPv6 loopback not available"
fi

# ── combined flags ───────────────────────────────────────────────
echo "== combined flags =="
PORT=$($FP -u -r 12000:12100)
assert "UDP+range works"        in_range "$PORT" 12000 12100

PORTS=$($FP -n 3 -r 20000:20100 -u)
assert "UDP+range+count"        [ "$(line_count "$PORTS")" -eq 3 ]

# ── error handling ───────────────────────────────────────────────
echo "== error handling =="
assert_fail "bad range rejected"     $FP -r 100:50
assert_fail "bad count rejected"     $FP -n 0
assert_fail "too many for range"     $FP -n 200 -r 8000:8010

# ── stress test (uniqueness under load) ──────────────────────────
echo "== stress test =="
PORTS=$($FP -n 100)
assert "100 ports returned"     [ "$(line_count "$PORTS")" -eq 100 ]
assert "all 100 unique"         [ "$(uniq_count "$PORTS")" -eq 100 ]

# ── summary ──────────────────────────────────────────────────────
echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] || exit 1
