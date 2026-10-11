#!/usr/bin/env bash
set -euo pipefail
umask 077

task_mode=${2:---probe}
if (( $# > 2 )) || [[ ! ${1:-0} =~ ^[0-9]+$ ]] || [[ $task_mode != --probe && $task_mode != --inspect && $task_mode != --read-dir && $task_mode != --pkcs11 && $task_mode != --pin-context ]]; then
    printf 'Uso: bash scripts/probe-jcop3.sh [índice de lector] [--probe|--inspect|--read-dir|--pkcs11|--pin-context]\n' >&2
    exit 2
fi

if ! command -v opensc-tool >/dev/null 2>&1; then
    printf 'Falta opensc-tool. Ejecutar nix develop y después make probe/jcop3.\n' >&2
    exit 1
fi

if [[ $task_mode == --inspect || $task_mode == --pkcs11 ]] && ! command -v pkcs15-tool >/dev/null 2>&1; then
    printf 'Falta pkcs15-tool. Ejecutar nix develop antes de make inspect/jcop3.\n' >&2
    exit 1
fi

task_reader=${1:-0}
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ $task_mode == --pkcs11 ]]; then
    task_module="$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
    task_slot_index=${SLOT_INDEX:-0}
    if [[ ! $task_slot_index =~ ^[0-9]+$ ]]; then
        printf 'SLOT_INDEX debe ser un índice numérico de slot PKCS#11.\n' >&2
        exit 2
    fi
    if ! command -v pkcs11-tool >/dev/null 2>&1 || [[ ! -f "$task_module" ]]; then
        printf 'Falta OpenSC compilado; ejecutar make build/opensc.\n' >&2
        exit 1
    fi
fi
mkdir -p -- "$task_root/local"
if [[ $task_mode == --inspect ]]; then
    task_report=$(mktemp "$task_root/local/jcop3-inspection-XXXXXXXX.txt")
elif [[ $task_mode == --read-dir ]]; then
    task_report=$(mktemp "$task_root/local/jcop3-dir-XXXXXXXX.txt")
elif [[ $task_mode == --pkcs11 ]]; then
    task_report=$(mktemp "$task_root/local/jcop3-pkcs11-XXXXXXXX.txt")
elif [[ $task_mode == --pin-context ]]; then
    task_report=$(mktemp "$task_root/local/jcop3-pin-context-XXXXXXXX.txt")
else
    task_report=$(mktemp "$task_root/local/jcop3-inventory-XXXXXXXX.txt")
fi

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
    printf 'mode=%s\n' "$task_mode"
    printf 'opensc_tool=%s\n' "$(command -v opensc-tool)"
} > "$task_report"

if command -v git >/dev/null 2>&1 && [[ -f "$task_root/vendor/opensc/.git" || -d "$task_root/vendor/opensc/.git" ]]; then
    task_record git -C "$task_root/vendor/opensc" rev-parse HEAD
    task_record git -C "$task_root/vendor/opensc" status --short
fi

task_failed=0
task_record opensc-tool --info || task_failed=1
task_record opensc-tool --list-readers || task_failed=1
# Solo ATR: opensc-tool termina antes de identificar la aplicación de tarjeta.
task_record opensc-tool --reader "$task_reader" --atr || task_failed=1

if [[ $task_mode == --inspect ]] && (( task_failed == 0 )); then
    # Descubrimiento de aplicación y objetos, sin verificar PIN ni firmar.
    # Los APDU y metadatos pueden identificar al titular: conservar en local/.
    task_record env OPENSC_DEBUG=9 opensc-tool --reader "$task_reader" --name || task_failed=1
    task_record env OPENSC_DEBUG=9 pkcs15-tool --reader "$task_reader" --no-cache --list-certificates --list-public-keys || task_failed=1
fi

if [[ $task_mode == --pkcs11 ]] && (( task_failed == 0 )); then
    # Solo metadatos de llaves/PIN y consultas PKCS#11; no autenticar ni firmar.
    task_config="$task_report.opensc.conf"
    cat > "$task_config" <<'CONFIG'
app default {
    framework pkcs15 {
        use_file_caching = no;
    }
}
CONFIG
    printf 'pkcs11_module=%s\nslot_index=%s\nopensc_conf=%s\n' \
        "$task_module" "$task_slot_index" "$task_config" >> "$task_report"
    task_record env OPENSC_DEBUG=9 pkcs15-tool --reader "$task_reader" --no-cache --list-keys --list-pins || task_failed=1
    task_record env OPENSC_DEBUG=9 OPENSC_CONF="$task_config" pkcs11-tool \
        --module "$task_module" --show-info --list-slots || task_failed=1
    task_record env OPENSC_DEBUG=9 OPENSC_CONF="$task_config" pkcs11-tool \
        --module "$task_module" --slot-index "$task_slot_index" --list-mechanisms || task_failed=1
    task_record env OPENSC_DEBUG=9 OPENSC_CONF="$task_config" pkcs11-tool \
        --module "$task_module" --slot-index "$task_slot_index" --list-objects || task_failed=1
fi

if [[ $task_mode == --read-dir ]] && (( task_failed == 0 )); then
    # Selecciones observadas en la captura de esta JCOP3 y lectura de EF.DIR.
    # El driver default evita la interpretación de ACL del driver IAS/ECC;
    # la tarjeta sigue aplicando sus propias condiciones de acceso.
    # EF.DIR declara 256 bytes: leer dos bloques de 128 bytes, sin PIN.
    task_record env OPENSC_DEBUG=9 opensc-tool --reader "$task_reader" --card-driver default \
        --send-apdu 00A404040BF0496173456363526F6F7400 \
        --send-apdu 00A40904022F0000 \
        --send-apdu 00B0000080 \
        --send-apdu 00B0008080 || task_failed=1

fi

if [[ $task_mode == --pin-context ]] && (( task_failed == 0 )); then
    # Solo SELECT y GET DATA de metadatos DOCP del PIN 1. No hay VERIFY,
    # autenticación, firma ni consultas del valor del PIN.
    for task_aid in F0496173456363526F6F74 E828BD080FD25047656E65726963; do
        if [[ $task_aid == F0496173456363526F6F74 ]]; then
            task_select_aid=00A404040BF0496173456363526F6F7400
        else
            task_select_aid=00A404040EE828BD080FD25047656E6572696300
        fi
        for task_select_file in 00A4020402000100 00A4010402000100 00A4000402000100; do
            # Reiniciar el contexto antes de cada comparación: EF, DF, cualquiera.
            printf '\ncontext_aid=%s\nselect_0001=%s\n' "$task_aid" "$task_select_file" >> "$task_report"
            task_record env OPENSC_DEBUG=9 opensc-tool --reader "$task_reader" --card-driver default \
                --send-apdu "$task_select_aid" --send-apdu "$task_select_file" || task_failed=1
        done
        printf '\ncontext_aid=%s\nquery=CHV_1_DOCP_metadata\n' "$task_aid" >> "$task_report"
        # Mismo formato que iasecc_sdo_get_tagged_data(), clase CHV=1,
        # referencia=1 y tag DOCP=A0; describe condiciones y contadores.
        task_record env OPENSC_DEBUG=9 opensc-tool --reader "$task_reader" --card-driver default \
            --send-apdu "$task_select_aid" \
            --send-apdu 00CB3FFF0A4D087006BF810102A08000 || task_failed=1
    done
fi

if [[ $task_mode == --read-dir || $task_mode == --pin-context ]]; then
    # opensc-tool puede devolver 0 aunque una APDU devuelva un SW de error.
    # En el diagnóstico de contexto, los SW negativos también son evidencia.
    task_expected_responses=4
    if [[ $task_mode == --pin-context ]]; then
        task_expected_responses=16
    fi
    task_responses=0
    while IFS= read -r task_line; do
        if [[ $task_line == 'Received (SW1='* ]]; then
            task_responses=$((task_responses + 1))
            printf '%s\n' "$task_line"
            if [[ $task_mode == --read-dir && $task_line != 'Received (SW1=0x90, SW2=0x00)'* ]]; then
                task_failed=1
            fi
        fi
    done < "$task_report"
    if (( task_responses != task_expected_responses )); then
        task_failed=1
    fi
fi

if [[ $task_mode == --probe ]]; then
    cat -- "$task_report"
else
    printf 'Detalle de la consulta guardado en el informe local.\n'
fi
if (( task_failed )); then
    printf '\nLa observación está incompleta; revisar los errores del informe.\n' >&2
    exit 1
fi

printf '\nConsultas terminadas. Revisar el informe antes del siguiente paso.\n'
