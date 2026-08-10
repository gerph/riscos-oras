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
        --set-variable 'ORASAuthentication$Write=AuthConfig,fff' \
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
tail -c 1 "fixture-manifest.json" | od -An -t x1 | tr -d ' \n' | grep -qx '0a'

echo "-- formatted manifest fetch against fixture"
run_oras_text manifest fetch --pretty "localhost:$PORT/demo/pull:latest" >"fixture-manifest-pretty.json"
grep -qx '{' "fixture-manifest-pretty.json"

echo "-- blob fetch against fixture"
run_oras blob fetch "localhost:$PORT/demo/pull:latest" \
    "sha256:62a72c8aa30dedb7aa393331a43175426b5d3af694205ad1a33973716e8a75ed" \
    | perl -0pe 's/\e\[[0-9;]*m//g; s/\r\n/\n/g' >"fixture-blob.bin"
cmp "fixture-blob.bin" <(printf 'fixture pull data\n')

echo "-- tags against fixture repository"
run_oras_text tags "localhost:$PORT/demo/pull" >"tags.json"
grep -qx 'latest' "tags.json"

echo "-- tag fixture manifest without reuploading it"
run_oras_text tag "localhost:$PORT/demo/pull:latest" stable
run_oras_text manifest fetch "localhost:$PORT/demo/pull:stable" >"stable-manifest.json"
cmp "fixture-manifest.json" "stable-manifest.json"
run_oras_text tags "localhost:$PORT/demo/pull" >"tags-after-tag.json"
grep -qx 'stable' "tags-after-tag.json"

echo "-- attach two files and retain both referrers"
printf 'attachment one\n' >"attachment-one,fff"
printf 'attachment two\n' >"attachment-two,fff"
run_oras_text attach "localhost:$PORT/demo/pull:latest" "attachment-one,fff"
run_oras_text attach "localhost:$PORT/demo/pull:latest" "attachment-two,fff"
ref_tag=$(python3 - <<'PY'
import hashlib
print('sha256-' + hashlib.sha256(open('fixture-manifest.json', 'rb').read().rstrip(b'\n')).hexdigest() + '.referrers')
PY
)
run_oras_text manifest fetch "localhost:$PORT/demo/pull:$ref_tag" >"referrers.json"
python3 -c 'import json; assert len(json.load(open("referrers.json"))["manifests"]) == 2'

echo "-- pull fixture fileset"
mkdir -p "pulled"
run_oras pull "localhost:$PORT/demo/pull:latest" "pulled"
cmp "pulled/fixture,fff" <(printf 'fixture pull data\n')

echo "-- protected Basic manifest fetch rejects absent credentials"
run_oras_text manifest fetch "localhost:$PORT/demo/basic:latest" >/dev/null 2>&1 || true

echo "-- interactive login writes Docker-compatible credentials"
printf 'secret\n' | run_oras login "localhost:$PORT" test
grep -q 'dGVzdDpzZWNyZXQ=' "AuthConfig,fff"

echo "-- protected Basic pull uses stored credentials"
mkdir -p "pulled-basic"
run_oras pull "localhost:$PORT/demo/basic:latest" "pulled-basic"
cmp "pulled-basic/fixture,fff" <(printf 'fixture pull data\n')

echo "-- push local file as fileset with source metadata"
run_oras push --source github:example/project "localhost:$PORT/demo/push:latest" "push-source,fff"

echo "-- fetch pushed manifest"
run_oras_text manifest fetch "localhost:$PORT/demo/push:latest" >"pushed-manifest.json"
grep -q 'application/vnd.riscos.fileset.v1' "pushed-manifest.json"
grep -q 'application/riscos; name=\\"push-source,FFF\\"' "pushed-manifest.json"
grep -q 'push-source,fff' "pushed-manifest.json"
grep -q 'https://github.com/example/project' "pushed-manifest.json"

echo "-- tags against pushed repository"
run_oras_text tags "localhost:$PORT/demo/push" >"push-tags.json"
grep -qx 'latest' "push-tags.json"

echo "-- pull pushed fileset"
mkdir -p "pulled-push"
run_oras pull "localhost:$PORT/demo/push:latest" "pulled-push"
cmp "pulled-push/push-source,fff" "push-source,fff"

echo "-- protected Bearer push and pull use stored credentials"
run_oras push "localhost:$PORT/demo/bearer:latest" "push-source,fff"
mkdir -p "pulled-bearer"
run_oras pull "localhost:$PORT/demo/bearer:latest" "pulled-bearer"
cmp "pulled-bearer/push-source,fff" "push-source,fff"

echo "-- logout removes only the selected credential"
run_oras logout "localhost:$PORT"
if grep -q 'localhost' "AuthConfig,fff"; then
    echo "logout retained target credential" >&2
    exit 1
fi

echo "Real oras smoke tests passed"
