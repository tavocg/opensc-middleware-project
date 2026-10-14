#!/usr/bin/env bash
# One explicit PKCS11 login/signature, interactive terminal only; no GUI/Idopte.
set -euo pipefail
umask 077
ulimit -c 0
task_reader=${1:-${READER:-0}}
if (( $# > 1 )) || [[ ! $task_reader =~ ^[0-9]{1,4}$ ]] || (( 10#$task_reader > 1024 )); then
    printf 'Uso: bash scripts/sign-jcop4-pkcs11.sh [lector 0..1024]\n' >&2; exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
command -v timeout >/dev/null || { printf 'Falta timeout; ejecutar desde nix develop.\n' >&2; exit 1; }
[[ -f "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" ]] || { printf 'Falta el módulo compilado; ejecutar make build/opensc.\n' >&2; exit 1; }
if [[ ! -t 0 || ! -t 1 ]]; then
    printf 'Se requiere una terminal para introducir el PIN sin eco.\n' >&2; exit 2
fi
task_tty_state=$(stty -g </dev/tty) || exit 2
# Restore the caller's terminal even if timeout has to kill the helper.
trap 'stty "$task_tty_state" </dev/tty 2>/dev/null || true' EXIT
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/jcop4-pkcs11-sign-XXXXXXXX")
task_report="$task_dir/report.txt"
task_config="$task_dir/opensc.conf"
cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    card_drivers = iasecc;
    card_driver iasecc { chipdoc_profile = cr-rsa2048; }
    reader_driver pcsc {
        enable_pinpad = false;
        enable_pace = false;
        disconnect_action = leave;
        transaction_end_action = leave;
        reconnect_action = leave;
    }
    framework pkcs15 { use_file_caching = no; use_pin_caching = no; }
    pkcs11 { atomic = false; lock_login = false; slots_per_card = 1; }
}
CONFIG
{
    printf 'date_utc=%s\nreader_index=%s\nmode=single_pkcs11_sha256_rsa2048_one_login_one_pso\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_reader"
    printf 'project_commit=%s\nopensc_commit=%s\n' \
        "$(git -C "$task_root" rev-parse HEAD 2>/dev/null || true)" \
        "$(git -C "$task_root/vendor/opensc" rev-parse HEAD 2>/dev/null || true)"
} > "$task_report"
printf 'Informe: %s\n' "$task_report"
if bash "$task_root/scripts/build-jcop4-pkcs11-sign.sh" >> "$task_report" 2>&1; then
    printf 'helper_build_status=0\n' >> "$task_report"
else
    task_status=$?; printf 'helper_build_status=%s\n' "$task_status" >> "$task_report"
    printf 'No se pudo compilar la prueba de firma. Revisar el informe.\n' >&2; exit "$task_status"
fi
task_status=0
# Empty environment prevents debug/preload/spies/providers or credential state
# inherited from another diagnostic. No activation/scanning subprocesses.
env -i PATH="$PATH" LC_ALL=C OPENSC_CONF="$task_config" OPENSC_DEBUG=0 \
    LD_LIBRARY_PATH="$task_build/src/pkcs11/.libs:$task_build/src/libopensc/.libs" \
    timeout --foreground --signal=TERM --kill-after=5s 120s \
    "$task_build/jcop4-pkcs11-sign" "$task_reader" "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" \
    </dev/null >> "$task_report" 2>/dev/null || task_status=$?
printf 'helper_exit_status=%s\n' "$task_status" >> "$task_report"
if (( task_status )); then
    printf 'Firma no confirmada (estado %s). Revisar el primer fallo y cierre; no repetir el PIN por inercia.\n' "$task_status" >&2
else
    printf 'Firma PKCS11 verificada y mensaje alterado rechazado; cierre comprobado. Revisar el informe.\n'
fi
exit "$task_status"
