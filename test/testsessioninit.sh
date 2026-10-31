#!/bin/sh
# SPDX-License-Identifier: MIT
# Assert that warm-up uses only the existing session and reports failures.
set -eu
script=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/gdbus" <<'STUB'
#!/bin/sh
printf '%s\n' "$*" >> "$BAMI_TEST_TRACE"
case "$1" in
    wait) exit "${BAMI_TEST_WAIT_STATUS:-0}";;
    call) case "$*" in
        *GetNameOwner*) printf "(':1.42',)\n"; exit "${BAMI_TEST_OWNER_STATUS:-0}";;
        *) exit "${BAMI_TEST_CALL_STATUS:-0}";;
    esac;;
    *) exit 99;;
esac
STUB
chmod +x "$work/gdbus"
export PATH="$work:$PATH" BAMI_TEST_TRACE="$work/trace"
sh "$script" > "$work/output"
[ "$(wc -l < "$work/trace")" -eq 3 ]
grep -q 'wait --session --timeout=10 org.fcitx.Fcitx5' "$work/trace"
grep -q 'org.fcitx.Fcitx.Controller1.GetConfig fcitx://config/addon/bami' "$work/trace"
grep -q -- '--dest :1.42' "$work/trace"
! grep -q 'SetConfig\|SetCurrentIM\|Activate\|--activate' "$work/trace"
: > "$work/trace"
if BAMI_TEST_WAIT_STATUS=1 sh "$script" > /dev/null 2>&1; then exit 1; fi
[ "$(wc -l < "$work/trace")" -eq 1 ]
if BAMI_TEST_OWNER_STATUS=1 sh "$script" > /dev/null 2>&1; then exit 1; fi
if BAMI_TEST_CALL_STATUS=1 sh "$script" > /dev/null 2>&1; then exit 1; fi
for bad in 0 31 invalid 999999999999999999999 ''; do
    if BAMI_INIT_TIMEOUT="$bad" sh "$script" > /dev/null 2>&1; then
        # Empty selects the documented default via shell parameter expansion.
        [ -z "$bad" ] || exit 1
    fi
done
