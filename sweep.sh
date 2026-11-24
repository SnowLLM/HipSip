#!/bin/sh
# Each kernel test runs in a separate Wine process because a failing test can abort.
set -u
bridge=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
dir=${1:?usage: sweep.sh <dir-of-exes> [fixtures-dir]}
fixtures=${2:-}
set --
if [ -n "$fixtures" ]; then
    [ -d "$fixtures" ] || { echo "missing fixture directory: $fixtures" >&2; exit 1; }
    fixture_dir=$(cd "$fixtures" && pwd) || exit 1
    win_fixtures=$(winepath -w "$fixture_dir") || exit 1
    set -- "$win_fixtures"
fi
run_test() {
    if [ -n "${HIPSIP_TEST_WRAPPER:-}" ]; then
        "$HIPSIP_TEST_WRAPPER" "$bridge/run.sh" "$@"
    else
        "$bridge/run.sh" "$@"
    fi
}
pass=0 fail=0 skip=0
for exe in "$dir"/test_*.exe; do
    [ -f "$exe" ] || { echo "no test_*.exe in $dir" >&2; exit 1; }
    name=$(basename "$exe")
    start=$(date +%s)
    out=$(run_test "$exe" "$@" 2>&1)
    rc=$?
    secs=$(($(date +%s) - start))
    case "$rc:$fixtures:$out" in
        77:*) printf '%-42s SKIP (exit 77)\n' "$name"
              skip=$((skip + 1)); continue;;
        2::*"usage:"*"<fixture_dir>"*) printf '%-42s SKIP (needs an argument this does not supply)\n' "$name"
                    skip=$((skip + 1)); continue;;
    esac
    if [ $rc -eq 0 ]; then
        printf '%-42s PASS  %ss\n' "$name" "$secs"; pass=$((pass + 1))
    else
        printf '%-42s FAIL  %ss  rc=%s\n' "$name" "$secs" "$rc"; fail=$((fail + 1))
        printf '%s\n' "$out" | tail -4 | sed 's/^/    /'
    fi
done
printf '\n%d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ]
