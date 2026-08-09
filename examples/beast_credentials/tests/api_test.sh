#!/usr/bin/env bash
# Black-box test automation for the beast_credentials REST API: starts real
# server processes, drives them with curl, and asserts on status codes and
# response bodies. No dependencies beyond bash, curl and the built server
# binary -- see README.md for the equivalent Postman collection.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_BIN="${SERVER_BIN:-$SCRIPT_DIR/../../../build/examples/beast_credentials/beast_credentials_server}"

if [[ ! -x "$SERVER_BIN" ]]; then
    echo "error: server binary not found at $SERVER_BIN" >&2
    echo "build it first, e.g.:" >&2
    echo "  cmake -S . -B build -DOPHEAP_BUILD_EXAMPLES=ON && cmake --build build" >&2
    echo "or point SERVER_BIN at an existing build." >&2
    exit 1
fi

ADMIN_TOKEN="test-admin-token-$$"
PASS=0
FAIL=0
PIDS=()
DIRS=()

cleanup() {
    local pid
    for pid in "${PIDS[@]:-}"; do [[ -n "$pid" ]] && kill "$pid" >/dev/null 2>&1 || true; done
    for pid in "${PIDS[@]:-}"; do [[ -n "$pid" ]] && wait "$pid" 2>/dev/null || true; done
    local dir
    for dir in "${DIRS[@]:-}"; do [[ -n "$dir" ]] && rm -rf "$dir"; done
}
trap cleanup EXIT

# start_server PORT STORAGE_DIR -- blocks until /healthz responds, sets
# SERVER_PID. Run directly (never inside `$(...)`) so PIDS stays mutable.
start_server() {
    local port="$1" storage="$2"
    "$SERVER_BIN" --port "$port" --storage "$storage" --admin-token "$ADMIN_TOKEN" \
        >"$storage/server.log" 2>&1 &
    SERVER_PID=$!
    PIDS+=("$SERVER_PID")
    for _ in $(seq 1 50); do
        if curl -s -o /dev/null "http://127.0.0.1:$port/healthz"; then return 0; fi
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo "error: server on port $port exited before becoming healthy; log:" >&2
            cat "$storage/server.log" >&2
            return 1
        fi
        sleep 0.1
    done
    echo "error: server on port $port did not become healthy in time" >&2
    return 1
}

stop_server() {
    local pid="$1"
    kill "$pid" >/dev/null 2>&1 || true
    wait "$pid" 2>/dev/null || true
}

check() {
    local description="$1" ok="$2"
    if [[ "$ok" == true ]]; then
        echo "[PASS] $description"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $description (got: code=$LAST_CODE body=$LAST_BODY)"
        FAIL=$((FAIL + 1))
    fi
}

# call METHOD URL [BODY] [EXTRA_HEADER] -- sets LAST_CODE and LAST_BODY.
call() {
    local method="$1" url="$2" body="${3:-}" header="${4:-}"
    local -a args=(-s -w '\n%{http_code}' -X "$method" "$url" -H 'Content-Type: application/json')
    [[ -n "$header" ]] && args+=(-H "$header")
    [[ -n "$body" ]] && args+=(-d "$body")
    local response
    response="$(curl "${args[@]}")"
    LAST_CODE="${response##*$'\n'}"
    LAST_BODY="${response%$'\n'*}"
}

ADMIN_HEADER="X-Admin-Token: $ADMIN_TOKEN"
SECRET="correct horse battery staple"

echo "== functional tests =="
FUNC_PORT=18098
FUNC_STORAGE="$(mktemp -d)"
DIRS+=("$FUNC_STORAGE")
start_server "$FUNC_PORT" "$FUNC_STORAGE" || exit 1
FUNC_PID="$SERVER_PID"
FUNC_BASE="http://127.0.0.1:$FUNC_PORT"
PRINCIPAL="test-user-$$"

call GET "$FUNC_BASE/healthz"
check "healthz returns 200" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"
check "healthz body reports ok" "$([[ "$LAST_BODY" == *'"status":"ok"'* ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials" "{\"principal\":\"$PRINCIPAL\",\"secret\":\"$SECRET\"}"
check "provision without admin token is rejected" "$([[ "$LAST_CODE" == 401 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials" "{\"principal\":\"$PRINCIPAL\",\"secret\":\"$SECRET\"}" "$ADMIN_HEADER"
check "provision with admin token succeeds" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials" "{\"principal\":\"short-secret-user\",\"secret\":\"short\"}" "$ADMIN_HEADER"
check "provision rejects a secret shorter than policy" "$([[ "$LAST_CODE" == 400 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials" "{\"principal\":\"\",\"secret\":\"$SECRET\"}" "$ADMIN_HEADER"
check "provision rejects an empty principal" "$([[ "$LAST_CODE" == 400 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials/verify" "{\"principal\":\"$PRINCIPAL\",\"secret\":\"$SECRET\"}"
check "verify accepts the correct secret" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials/verify" "{\"principal\":\"$PRINCIPAL\",\"secret\":\"wrong secret\"}"
WRONG_SECRET_CODE="$LAST_CODE"
WRONG_SECRET_BODY="$LAST_BODY"
check "verify rejects a wrong secret" "$([[ "$LAST_CODE" == 401 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials/verify" "{\"principal\":\"no-such-user\",\"secret\":\"wrong secret\"}"
check "verify rejects an unknown principal" "$([[ "$LAST_CODE" == 401 ]] && echo true || echo false)"
check "unknown principal and wrong secret give the same status" \
    "$([[ "$LAST_CODE" == "$WRONG_SECRET_CODE" ]] && echo true || echo false)"
check "unknown principal and wrong secret give the same body (anti-enumeration)" \
    "$([[ "$LAST_BODY" == "$WRONG_SECRET_BODY" ]] && echo true || echo false)"

call DELETE "$FUNC_BASE/v1/credentials/$PRINCIPAL"
check "delete without admin token is rejected" "$([[ "$LAST_CODE" == 401 ]] && echo true || echo false)"

call DELETE "$FUNC_BASE/v1/credentials/$PRINCIPAL" "" "$ADMIN_HEADER"
check "delete with admin token succeeds" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"

call POST "$FUNC_BASE/v1/credentials/verify" "{\"principal\":\"$PRINCIPAL\",\"secret\":\"$SECRET\"}"
check "verify rejects a disabled credential's correct secret" "$([[ "$LAST_CODE" == 401 ]] && echo true || echo false)"

PERSIST_PRINCIPAL="persist-user-$$"
call POST "$FUNC_BASE/v1/credentials" "{\"principal\":\"$PERSIST_PRINCIPAL\",\"secret\":\"$SECRET\"}" "$ADMIN_HEADER"
check "provision persistence-test credential" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"

stop_server "$FUNC_PID"

echo
echo "== persistence-across-restart test =="
RESTART_PORT=18099
start_server "$RESTART_PORT" "$FUNC_STORAGE" || exit 1
RESTART_PID="$SERVER_PID"
RESTART_BASE="http://127.0.0.1:$RESTART_PORT"

call POST "$RESTART_BASE/v1/credentials/verify" "{\"principal\":\"$PERSIST_PRINCIPAL\",\"secret\":\"$SECRET\"}"
check "credential survives a server restart" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"

stop_server "$RESTART_PID"

echo
echo "== rate limiting test (isolated server + fresh principal) =="
RATE_PORT=18100
RATE_STORAGE="$(mktemp -d)"
DIRS+=("$RATE_STORAGE")
start_server "$RATE_PORT" "$RATE_STORAGE" || exit 1
RATE_PID="$SERVER_PID"
RATE_BASE="http://127.0.0.1:$RATE_PORT"

call POST "$RATE_BASE/v1/credentials" "{\"principal\":\"rate-user\",\"secret\":\"$SECRET\"}" "$ADMIN_HEADER"
check "provision rate-limit-test credential" "$([[ "$LAST_CODE" == 200 ]] && echo true || echo false)"

RATE_LIMITED_SEEN=false
ALLOWED_COUNT=0
for _ in $(seq 1 15); do
    call POST "$RATE_BASE/v1/credentials/verify" "{\"principal\":\"rate-user\",\"secret\":\"wrong\"}"
    if [[ "$LAST_CODE" == 429 ]]; then
        RATE_LIMITED_SEEN=true
        break
    fi
    ALLOWED_COUNT=$((ALLOWED_COUNT + 1))
done
check "repeated failed verifications eventually trigger rate limiting" "$([[ "$RATE_LIMITED_SEEN" == true ]] && echo true || echo false)"
# Matches service_config's default rate_limit.max_attempts (see credential_service.hpp).
check "rate limiting allows exactly the configured attempt budget (10) first" \
    "$([[ "$ALLOWED_COUNT" == 10 ]] && echo true || echo false)"

stop_server "$RATE_PID"

echo
echo "$PASS passed, $FAIL failed"
if [[ "$FAIL" -eq 0 ]]; then exit 0; else exit 1; fi
