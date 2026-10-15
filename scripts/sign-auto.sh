#!/usr/bin/env bash
# Normal PKCS11 autodetection; only the user runs this against a real card.
set -euo pipefail
umask 077
ulimit -c 0
task_slot=${1:-${SLOT_INDEX:-0}}
task_label=${2:-${KEY_LABEL:-LlaveDeFirma}}
if (( $# > 2 )) || [[ ! $task_slot =~ ^[0-9]{1,2}$ ]] || (( 10#$task_slot > 31 )) || [[ -z $task_label || ${#task_label} -gt 128 ]]; then
    printf 'Uso: bash scripts/sign-auto.sh [slot 0..31] [etiqueta de certificado]\n' >&2; exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
command -v timeout >/dev/null || exit 1
[[ -t 0 && -t 1 ]] || { printf 'Se requiere una terminal para el PIN sin eco.\n' >&2; exit 2; }
task_tty_state=$(stty -g </dev/tty)
trap 'stty "$task_tty_state" </dev/tty 2>/dev/null || true' EXIT
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/auto-sign-XXXXXXXX")
task_report="$task_dir/report.txt"
printf 'Informe: %s\n' "$task_report"
printf 'date_utc=%s\nmode=autodetection_one_login_one_signature\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$task_report"
if bash "$task_root/scripts/build-pkcs11-auto-sign.sh" >> "$task_report" 2>&1; then
    printf 'helper_build_status=0\n' >> "$task_report"
else
    task_status=$?; printf 'helper_build_status=%s\n' "$task_status" >> "$task_report"; exit "$task_status"
fi
task_status=0
env -i PATH="$PATH" LC_ALL=C OPENSC_CONF=/dev/null OPENSC_DEBUG=0 \
    LD_LIBRARY_PATH="$task_build/src/pkcs11/.libs:$task_build/src/libopensc/.libs" \
    timeout --foreground --signal=TERM --kill-after=5s 120s \
    "$task_build/pkcs11-auto-sign" "$task_slot" "$task_label" \
    </dev/null >> "$task_report" 2>/dev/null || task_status=$?
printf 'helper_exit_status=%s\n' "$task_status" >> "$task_report"
if (( task_status )); then
    printf 'Firma no confirmada (estado %s). Revisar informe antes de otro intento.\n' "$task_status" >&2
else
    printf 'Firma verificada y mensaje alterado rechazado. Revisar informe.\n'
fi
exit "$task_status"
