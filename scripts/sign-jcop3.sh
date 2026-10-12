#!/usr/bin/env bash
set -euo pipefail
umask 077

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tool="$task_root/build/opensc/src/tools/pkcs11-tool"
task_module="$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
task_slot_index=${SLOT_INDEX:-0}
# ID observado de LlaveDeFirma; selecciona el certificado y su llave asociada.
task_key_id=4c6c61766544654669726d61
task_mechanism=RSA-PKCS
task_post_login_status=no

if (( $# == 1 )) && [[ $1 == --sha256 ]]; then
    task_mechanism=SHA256-RSA-PKCS
elif (( $# == 1 )) && [[ $1 == --pin-status ]]; then
    task_post_login_status=yes
elif (( $# != 0 )); then
    printf 'Uso: make sign/jcop3, make sign/jcop3-sha256 o make sign/jcop3-pin-status [SLOT_INDEX=0]\n' >&2
    exit 2
fi
if [[ ! $task_slot_index =~ ^[0-9]+$ ]]; then
    printf 'SLOT_INDEX debe ser un índice numérico de slot PKCS#11.\n' >&2
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
printf 'date_utc=%s\nmodule=%s\nslot_index=%s\nkey_id=%s\nmechanism=%s\ndigest_algorithm=SHA256\n' \
    "$task_date" "$task_module" "$task_slot_index" "$task_key_id" "$task_mechanism" > "$task_report"
printf 'post_login_status_experiment=%s\n' "$task_post_login_status" >> "$task_report"
git -C "$task_root/vendor/opensc" rev-parse HEAD >> "$task_report"
git -C "$task_root/vendor/opensc" status --short >> "$task_report"

cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    framework pkcs15 {
        use_file_caching = no;
        use_pin_caching = no;
        try_emulation_first = yes;
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

task_common=(env -u OPENSC_API_TRACE_FILE -u OPENSC_JCOP3_SESSION_STATE
    -u OPENSC_JCOP3_POST_LOGIN_STATUS OPENSC_DEBUG=0 OPENSC_CONF="$task_config" "$task_tool"
    --module "$task_module" --slot-index "$task_slot_index" --id "$task_key_id")

task_record "${task_common[@]}" --read-object --type cert --output-file "$task_output/certificate.der"
"${task_common[@]}" --read-object --type cert --output-file "$task_output/certificate.der" >> "$task_report" 2>&1
task_record openssl x509 -inform DER -in "$task_output/certificate.der" -pubkey -noout
openssl x509 -inform DER -in "$task_output/certificate.der" -pubkey -noout \
    > "$task_output/public-key.pem" 2>> "$task_report"

task_record bash "$task_root/scripts/sha256-digestinfo.sh" "$task_output/message.txt"
bash "$task_root/scripts/sha256-digestinfo.sh" "$task_output/message.txt" \
    > "$task_output/digest-info.der" 2>> "$task_report"

printf '\nSe firmara este texto con LlaveDeFirma:\n'
cat -- "$task_output/message.txt"
task_input="$task_output/digest-info.der"
if [[ $task_mechanism == SHA256-RSA-PKCS ]]; then
    task_input="$task_output/message.txt"
    printf '\nOpenSC recibe el texto y prepara SHA-256 y DigestInfo; la tarjeta realiza RSA PKCS#1 v1.5.\n'
else
    printf '\nSHA-256 se calcula en el equipo; la tarjeta realiza RSA PKCS#1 v1.5.\n'
fi
printf '\nIntroduce tu PIN cuando lo solicite pkcs11-tool.\n'
task_authenticated=("${task_common[@]}")
if [[ $task_post_login_status == yes ]]; then
    task_trace="$task_output/api.log"
    : > "$task_trace"
    # Insert experiment variables before the tool, never credentials.
    task_authenticated=(env -u OPENSC_JCOP3_SESSION_STATE OPENSC_DEBUG=0
        OPENSC_CONF="$task_config" OPENSC_API_TRACE_FILE="$task_trace"
        OPENSC_JCOP3_POST_LOGIN_STATUS=yes "$task_tool"
        --module "$task_module" --slot-index "$task_slot_index" --id "$task_key_id")
    printf 'Experimento: una consulta vacía de estado inmediatamente después del VERIFY exitoso.\n'
    printf 'No agrega SELECT ni lectura de política. Metadatos: %s\n' "$task_trace"
fi
task_record "${task_authenticated[@]}" --login --sign --mechanism "$task_mechanism" \
    --input-file "$task_input" --output-file "$task_output/signature.bin"
# stdout debe seguir conectado a la terminal: util_getpass lo utiliza para
# desactivar el eco. No registrar APDU ni pasar el PIN en argumentos.
"${task_authenticated[@]}" --login --sign --mechanism "$task_mechanism" \
    --input-file "$task_input" --output-file "$task_output/signature.bin" 2>> "$task_report"
if [[ $task_post_login_status == yes ]]; then
    if [[ $(grep -c '^POST_LOGIN_STATUS_BEGIN ' "$task_trace" || true) != 1 ]] ||
       ! grep -q '^POST_LOGIN_STATUS session=0x0 rv=0x0 ' "$task_trace"; then
        printf 'No se confirmó la consulta experimental; revisar módulo compilado y perfil.\n' >> "$task_report"
        exit 1
    fi
fi

task_record openssl dgst -sha256 -verify "$task_output/public-key.pem" \
    -signature "$task_output/signature.bin" "$task_output/message.txt"
openssl dgst -sha256 -verify "$task_output/public-key.pem" \
    -signature "$task_output/signature.bin" "$task_output/message.txt" 2>> "$task_report" | tee -a "$task_report"
printf 'Firma verificada. Informe: %s\n' "$task_report"
