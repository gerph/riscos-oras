#!/usr/bin/env bash
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
URL_PYMOD="/riscos-resources/Install/Tools/Linux/pyromaniac-resources/pyromaniac-resources/riscos/pymods/driver/url.py"
PORT="${PORT:-5443}"
TMP="tests/tmp-real"
SERVER_LOG="$TMP/server.log"
PIDFILE="$TMP/server.pid"

cd "$ROOT"

cleanup() {
    if [ -f "$PIDFILE" ]; then
        kill "$(cat "$PIDFILE")" 2>/dev/null || true
        rm -f "$PIDFILE"
    fi
    rm -f tests.tmp-real.* 2>/dev/null || true
}

trap cleanup EXIT

rm -rf "$TMP"
mkdir -p "$TMP"

python3 "$ROOT/tests/fake_registry.py" \
    --port "$PORT" \
    >"$SERVER_LOG" 2>&1 &
echo $! >"$PIDFILE"
sleep 1

run_oras() {
    riscos-run \
        --load-pymodule "$URL_PYMOD" \
        --run-native "$ROOT/aif32/oras,ff8" \
        "$@"
}

run_oras_text() {
    run_oras "$@" | perl -pe 's/\e\[[0-9;]*m//g; s/\r$//'
}

run_oras_blob_to_file() {
    local outfile="$1"
    shift
    run_oras "$@" | perl -0pe 's/\e\[[0-9;]*m//g; s/\r\n/\n/g' >"$outfile"
}

echo "== local HTTP registry smoke =="

cd "$TMP"

echo "fixture push source" >"push-source,fff"

echo "-- manifest fetch against fixture"
run_oras_text manifest fetch "localhost:$PORT/demo/pull:latest" >"fixture-manifest.json"
grep -q 'application/vnd.riscos.fileset.v1' "fixture-manifest.json"

echo "-- blob fetch against fixture"
run_oras blob fetch "localhost:$PORT/demo/pull:latest" \
    "sha256:62a72c8aa30dedb7aa393331a43175426b5d3af694205ad1a33973716e8a75ed" \
    | perl -0pe 's/\e\[[0-9;]*m//g; s/\r\n/\n/g' >"fixture-blob.bin"
cmp "fixture-blob.bin" <(printf 'fixture pull data\n')

echo "-- tags against fixture repository"
run_oras_text tags "localhost:$PORT/demo/pull" >"tags.json"
grep -qx 'latest' "tags.json"

echo "-- pull fixture fileset"
mkdir -p "pulled"
run_oras pull "localhost:$PORT/demo/pull:latest" "pulled"
cmp "pulled.fixture,fff" <(printf 'fixture pull data\n')

echo "-- push local file as fileset"
run_oras push "localhost:$PORT/demo/push:latest" "push-source,fff"

echo "-- fetch pushed manifest"
run_oras_text manifest fetch "localhost:$PORT/demo/push:latest" >"pushed-manifest.json"
grep -q 'application/vnd.riscos.fileset.v1' "pushed-manifest.json"
grep -q 'push-source,fff' "pushed-manifest.json"

echo "-- tags against pushed repository"
run_oras_text tags "localhost:$PORT/demo/push" >"push-tags.json"
grep -qx 'latest' "push-tags.json"

echo "-- pull pushed fileset"
mkdir -p "pulled-push"
run_oras pull "localhost:$PORT/demo/push:latest" "pulled-push"
cmp "pulled-push.push-source,fff" "push-source,fff"

echo "Real oras smoke tests passed"
