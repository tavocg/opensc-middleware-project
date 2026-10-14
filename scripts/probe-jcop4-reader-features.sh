#!/usr/bin/env bash
# Reader controls only; no Idopte, PIN, card APDU or reset.
set -euo pipefail
umask 077
ulimit -c 0
task_reader=${1:-${READER:-0}}
if (( $# > 1 )) || [[ ! $task_reader =~ ^[0-9]+$ ]]; then exit 2; fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
command -v timeout >/dev/null
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/jcop4-reader-features-XXXXXXXX")
task_report="$task_dir/report.txt"
printf 'date_utc=%s\nreader_index=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_reader" > "$task_report"
printf 'Informe: %s\n' "$task_report"
if bash "$task_root/scripts/build-jcop4-reader-features.sh" >> "$task_report" 2>&1; then
    printf 'helper_build_status=0\n' >> "$task_report"
else
    task_status=$?
    printf 'helper_build_status=%d\n' "$task_status" >> "$task_report"
    exit "$task_status"
fi
task_status=0
env -u LD_PRELOAD -u LD_AUDIT -u LD_LIBRARY_PATH \
    -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE \
    -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE \
    -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    timeout --signal=TERM --kill-after=5s 20s \
    "$task_root/build/diagnostics/jcop4-reader-features" "$task_reader" \
    </dev/null >> "$task_report" 2>&1 || task_status=$?
printf 'helper_exit_status=%d\n' "$task_status" >> "$task_report"
printf 'Consulta del lector terminada (estado %d), sin PIN ni APDU de tarjeta. Revisar el informe.\n' "$task_status"
exit "$task_status"
