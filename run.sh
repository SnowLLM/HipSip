#!/bin/sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=${OUT:-$here/build}
exe=${1:-}
[ -n "$exe" ] || { echo "usage: run.sh <program.exe> [args...]" >&2; exit 1; }
shift
[ -f "$exe" ] || { echo "executable not found: $exe" >&2; exit 1; }
[ -f "$out/libhipsip.so" ] || { echo "missing $out/libhipsip.so; run make first" >&2; exit 1; }
dir=$(cd "$(dirname "$exe")" && pwd)
for src in "$out"/amdhip64*.dll; do
    [ "$dir/$(basename "$src")" -ef "$src" ] || cp -f "$src" "$dir/"
done
LD_PRELOAD="$out/libhipsip.so${LD_PRELOAD:+:$LD_PRELOAD}" \
    WINEDEBUG=${WINEDEBUG:--all} exec wine "$exe" "$@"
