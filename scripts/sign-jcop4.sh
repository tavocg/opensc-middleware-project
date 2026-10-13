#!/usr/bin/env bash
# One experimental APDU signature, with local verification; no PKCS#11 yet.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_reader=${READER:-0}
if (( $# )) || [[ ! $task_reader =~ ^[0-9]+$ ]]; then
    printf 'Uso: make sign/jcop4-apdu [READER=0]\n' >&2
    exit 2
fi
if [[ ! -t 0 || ! -t 1 ]]; then
    printf 'Se requiere una terminal para introducir el PIN sin eco.\n' >&2
    exit 2
fi
if ! command -v openssl >/dev/null 2>&1; then
    printf 'Entrar con nix develop antes de ejecutar esta prueba.\n' >&2
    exit 1
fi
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/jcop4-signature-XXXXXXXX")
task_report="$task_dir/report.txt"
task_conf="$task_dir/opensc.conf"
printf 'Resultado y archivos: %s\n' "$task_dir"
cat > "$task_conf" <<'CONFIG'
app default {
    debug = 0;
    framework pkcs15 {
        use_file_caching = no;
        use_pin_caching = no;
    }
}
CONFIG
{
    printf 'date_utc=%s\nmode=experimental_apdu_sha256_rsa\nreader_index=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_reader"
    git -C "$task_root" rev-parse HEAD
    git -C "$task_root/vendor/opensc" rev-parse HEAD
} > "$task_report"
task_finish() {
    local task_status=$?
    printf 'exit_status=%s\n' "$task_status" >> "$task_report"
    if (( task_status )); then
        printf 'Prueba detenida, sin reintentos de PIN. Informe: %s\n' "$task_report" >&2
    fi
}
trap task_finish EXIT
bash "$task_root/scripts/build-card-sign-test.sh" >> "$task_report" 2>&1
printf 'libjcop3p11: prueba tecnica JCOP4 RSA SHA-256.\nFecha UTC: %s\n' \
    "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$task_dir/message.txt"
openssl dgst -sha256 -binary "$task_dir/message.txt" > "$task_dir/digest.bin" 2>> "$task_report"
printf '\nSe firmará este texto con LlaveDeFirma de la JCOP4:\n'
cat -- "$task_dir/message.txt"
# Keep both input and output on the terminal: util_getpass disables echo via stdout.
# Only fixed result metadata is written by the helper into report.txt.
env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE \
    -u OPENSC_JCOP3_SESSION_STATE -u OPENSC_JCOP3_PIN_STATUS_IN_PLACE \
    -u OPENSC_JCOP3_POST_LOGIN_STATUS -u OPENSC_JCOP3_POST_LOGIN_SELECT \
    OPENSC_DEBUG=0 OPENSC_CONF="$task_conf" \
    "$task_root/build/opensc/card-sign-test" "$task_reader" "$task_dir"
openssl x509 -inform DER -in "$task_dir/certificate.der" -pubkey -noout \
    > "$task_dir/public-key.pem" 2>> "$task_report"
# This pipeline contains only offline verification output, after PIN collection ended.
openssl dgst -sha256 -verify "$task_dir/public-key.pem" -signature "$task_dir/signature.bin" \
    "$task_dir/message.txt" 2>> "$task_report" | tee -a "$task_report"
printf 'Firma verificada. Informe: %s\n' "$task_report"
