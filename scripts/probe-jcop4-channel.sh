#!/usr/bin/env bash
# Bounded channel diagnostic; explicit separate login mode can collect one PIN.
# Explicit sign mode signs only its generated technical test text. No GUI/Idopte.
set -euo pipefail
umask 077
ulimit -c 0
task_reader=${1:-${READER:-0}}
task_mode=anonymous_channel_no_pin_once
task_prefix=jcop4-channel
task_helper_args=("$task_reader")
task_helper=jcop4-channel-probe
task_builder_args=()
task_timeout_args=()
if (( $# == 2 )) && [[ $2 == --pin-context ]]; then
    task_mode=anonymous_channel_pin_context_no_pin_once
    task_prefix=jcop4-channel-pin-context
    task_helper_args+=(--pin-context)
elif (( $# == 2 )) && [[ $2 == --login-once || $2 == --sign-once ]]; then
    # A controlling terminal is required, never accept credentials via pipe.
    if [[ ! -t 0 || ! -t 1 ]]; then
        printf 'El login requiere una terminal para introducir el PIN sin eco.\n' >&2
        exit 2
    fi
    task_mode=anonymous_channel_login_once_no_signature
    task_prefix=jcop4-channel-login
    task_helper=jcop4-channel-login
    task_helper_args+=(--login-once)
    task_builder_args=("" --login-once)
    # A timeout subprocess group is not the terminal foreground group when
    # launched from this script: keep terminal access and Ctrl+C on the helper.
    task_timeout_args=(--foreground)
    if [[ $2 == --sign-once ]]; then
        task_mode=anonymous_channel_signature_once_rsa2048_sha256
        task_prefix=jcop4-channel-signature
        task_helper=jcop4-channel-sign
        task_helper_args=("$task_reader" --sign-once)
        task_builder_args=("" --sign-once)
    fi
elif (( $# > 1 )); then
    printf 'Uso: bash scripts/probe-jcop4-channel.sh [lector] [--pin-context|--login-once|--sign-once]\n' >&2
    exit 2
fi
if [[ ! $task_reader =~ ^[0-9]+$ ]]; then
    printf 'El índice de lector debe ser numérico.\n' >&2
    exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if ! command -v timeout >/dev/null 2>&1; then
    printf 'Falta timeout; ejecutar desde nix develop.\n' >&2
    exit 1
fi
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/$task_prefix-XXXXXXXX")
task_report="$task_dir/report.txt"
task_config="$task_dir/opensc.conf"
if [[ $task_mode == anonymous_channel_signature_once_rsa2048_sha256 ]]; then
    # Validated earlier as LlaveDeFirma; cached public certificate only.
    task_helper_args+=("$task_root/local/jcop4-signature-5Dpadr1N/certificate.der" "$task_dir")
fi
cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    reader_driver pcsc {
        disconnect_action = leave;
        transaction_end_action = leave;
        reconnect_action = leave;
    }
    framework pkcs15 {
        use_file_caching = no;
        use_pin_caching = no;
    }
}
CONFIG
{
    printf 'date_utc=%s\nreader_index=%s\nmode=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_reader" "$task_mode"
    printf 'policy=opensc_t1_short_max256\n'
    git -C "$task_root" rev-parse HEAD || true
    git -C "$task_root" status --short || true
    git -C "$task_root/vendor/opensc" rev-parse HEAD || true
    git -C "$task_root/vendor/opensc" status --short || true
} > "$task_report" 2>&1
printf 'Informe: %s\n' "$task_report"
if bash "$task_root/scripts/build-jcop4-channel-probe.sh" "${task_builder_args[@]}" >> "$task_report" 2>&1; then
    printf 'helper_build_status=0\n' >> "$task_report"
else
    task_status=$?
    printf 'helper_build_status=%d\n' "$task_status" >> "$task_report"
    printf 'No se pudo compilar el diagnóstico. Revisar el informe.\n' >&2
    exit "$task_status"
fi
task_status=0
env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE -u OPENSC_DEEPBIND \
    -u LD_PRELOAD -u LD_AUDIT -u LD_LIBRARY_PATH \
    -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE \
    -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    OPENSC_CONF="$task_config" OPENSC_DEBUG=0 \
    timeout "${task_timeout_args[@]}" --signal=TERM --kill-after=5s 45s \
    "$task_root/build/opensc/$task_helper" "${task_helper_args[@]}" \
    </dev/null >> "$task_report" 2>&1 || task_status=$?
printf 'helper_exit_status=%d\n' "$task_status" >> "$task_report"
if [[ $task_mode == anonymous_channel_signature_once_rsa2048_sha256 ]]; then
    printf 'Prueba de firma terminada (estado %d). Revisar verificación RSA y cierre en el informe; no repetir por inercia.\n' "$task_status"
elif [[ $task_mode == anonymous_channel_login_once_no_signature ]]; then
    printf 'Login terminado (estado %d), sin firma. Revisar el informe; no repetir por inercia.\n' "$task_status"
elif (( task_status )); then
    if [[ $task_mode == anonymous_channel_pin_context_no_pin_once ]]; then
        printf 'Consulta de contexto incompleta. Revisar el informe; no repetir ni introducir PIN.\n' >&2
    else
        printf 'Canal no confirmado o diagnóstico incompleto. Revisar el informe; no repetir ni introducir PIN.\n' >&2
    fi
else
    printf 'Consulta terminada. Revisar confirmación, contexto solicitado y cierre en el informe. No se solicitó PIN.\n'
fi
exit "$task_status"
