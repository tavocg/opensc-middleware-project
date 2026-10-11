#!/usr/bin/env bash
set -euo pipefail
umask 077

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tool="$task_root/build/opensc/src/tools/pkcs11-tool"
task_module="$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
task_slot_index=${SLOT_INDEX:-0}
# ID observado de LlaveDeFirma; selecciona el certificado y su llave asociada.
task_key_id=4c6c61766544654669726d61

if (( $# != 0 )) || [[ ! $task_slot_index =~ ^[0-9]+$ ]]; then
    printf 'Uso: make sign/jcop3 [SLOT_INDEX=0]\n' >&2
    exit 2
fi
if [[ ! -t 0 || ! -t 1 ]]; then
    printf 'La prueba requiere una terminal para introducir el PIN sin eco.\n' >&2
    exit 2
fi
if [[ ! -x "$task_tool" || ! -f "$task_module" ]] || ! command -v openssl >/dev/null 2>&1; then
    printf 'Entrar con nix develop y ejecutar make build/opensc.\n' >&2
    exit 1
fi

mkdir -p -- "$task_root/local"
task_output=$(mktemp -d "$task_root/local/jcop3-signature-XXXXXXXX")
task_report="$task_output/report.txt"
task_config="$task_output/opensc.conf"
task_date=$(date -u +%Y-%m-%dT%H:%M:%SZ)
printf 'Resultado y archivos: %s\n' "$task_output"
printf 'date_utc=%s\nmodule=%s\nslot_index=%s\nkey_id=%s\nmechanism=SHA256-RSA-PKCS\n' \
    "$task_date" "$task_module" "$task_slot_index" "$task_key_id" > "$task_report"
git -C "$task_root/vendor/opensc" rev-parse HEAD >> "$task_report"
git -C "$task_root/vendor/opensc" status --short >> "$task_report"

cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    framework pkcs15 {
        use_file_caching = no;
    }
}
CONFIG
printf 'libjcop3p11: prueba tecnica de firma RSA SHA-256.\nFecha UTC: %s\n' \
    "$task_date" > "$task_output/message.txt"

task_record() {
    printf '\n$' >> "$task_report"
    printf ' %q' "$@" >> "$task_report"
    printf '\n' >> "$task_report"
}
task_on_exit() {
    local task_status=$?
    printf 'exit_status=%s\n' "$task_status" >> "$task_report"
    if (( task_status != 0 )); then
        cat -- "$task_report" >&2
        printf '\nLa prueba se detuvo; no se reintenta el PIN. Informe: %s\n' "$task_report" >&2
    fi
}
trap task_on_exit EXIT

task_common=(env OPENSC_DEBUG=0 OPENSC_CONF="$task_config" "$task_tool"
    --module "$task_module" --slot-index "$task_slot_index" --id "$task_key_id")

task_record "${task_common[@]}" --read-object --type cert --output-file "$task_output/certificate.der"
"${task_common[@]}" --read-object --type cert --output-file "$task_output/certificate.der" >> "$task_report" 2>&1
task_record openssl x509 -inform DER -in "$task_output/certificate.der" -pubkey -noout
openssl x509 -inform DER -in "$task_output/certificate.der" -pubkey -noout \
    > "$task_output/public-key.pem" 2>> "$task_report"

printf '\nSe firmara este texto con LlaveDeFirma:\n'
cat -- "$task_output/message.txt"
printf '\nIntroduce tu PIN cuando lo solicite pkcs11-tool.\n'
task_record "${task_common[@]}" --login --sign --mechanism SHA256-RSA-PKCS \
    --input-file "$task_output/message.txt" --output-file "$task_output/signature.bin"
# stdout debe seguir conectado a la terminal: util_getpass lo utiliza para
# desactivar el eco. No registrar APDU ni pasar el PIN en argumentos.
"${task_common[@]}" --login --sign --mechanism SHA256-RSA-PKCS \
    --input-file "$task_output/message.txt" --output-file "$task_output/signature.bin" 2>> "$task_report"

task_record openssl dgst -sha256 -verify "$task_output/public-key.pem" \
    -signature "$task_output/signature.bin" "$task_output/message.txt"
openssl dgst -sha256 -verify "$task_output/public-key.pem" \
    -signature "$task_output/signature.bin" "$task_output/message.txt" 2>> "$task_report" | tee -a "$task_report"
printf 'Firma verificada. Informe: %s\n' "$task_report"
