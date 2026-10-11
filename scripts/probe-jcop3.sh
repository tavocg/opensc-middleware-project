#!/usr/bin/env bash
set -euo pipefail
umask 077

if (( $# > 1 )) || [[ ! ${1:-0} =~ ^[0-9]+$ ]]; then
    printf 'Uso: bash scripts/probe-jcop3.sh [índice de lector]\n' >&2
    exit 2
fi

if ! command -v opensc-tool >/dev/null 2>&1; then
    printf 'Falta opensc-tool. Ejecutar nix develop y después make probe/jcop3.\n' >&2
    exit 1
fi

task_reader=${1:-0}
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p -- "$task_root/local"
task_report=$(mktemp "$task_root/local/jcop3-inventory-XXXXXXXX.txt")

task_record() {
    local task_status=0
    printf '\n$' >> "$task_report"
    printf ' %q' "$@" >> "$task_report"
    printf '\n' >> "$task_report"
    "$@" >> "$task_report" 2>&1 || task_status=$?
    printf 'exit_status=%s\n' "$task_status" >> "$task_report"
    return "$task_status"
}

printf 'Informe: %s\n' "$task_report"
{
    printf 'date_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'system=%s\n' "$(uname -srm)"
    printf 'reader_index=%s\n' "$task_reader"
    printf 'opensc_tool=%s\n' "$(command -v opensc-tool)"
} > "$task_report"

if command -v git >/dev/null 2>&1 && [[ -f "$task_root/vendor/opensc/.git" || -d "$task_root/vendor/opensc/.git" ]]; then
    task_record git -C "$task_root/vendor/opensc" rev-parse HEAD
    task_record git -C "$task_root/vendor/opensc" status --short
fi

task_failed=0
task_record opensc-tool --version || task_failed=1
task_record opensc-tool --list-readers || task_failed=1
# Solo ATR: opensc-tool termina antes de identificar la aplicación de tarjeta.
task_record opensc-tool --reader "$task_reader" --atr || task_failed=1

cat -- "$task_report"
if (( task_failed )); then
    printf '\nLa observación está incompleta; revisar los errores del informe.\n' >&2
    exit 1
fi

printf '\nConsultas terminadas. Revisar que haya lector, tarjeta y ATR en el informe.\n'
