#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )) || [[ ! -f $1 || ! -r $1 ]]; then
    printf 'Uso: bash scripts/sha256-digestinfo.sh archivo\n' >&2
    exit 2
fi
# DER DigestInfo: SEQUENCE { AlgorithmIdentifier { sha256, NULL },
# OCTET STRING (32 bytes) }. RSA-PKCS adds the PKCS#1 v1.5 padding on card.
printf '\x30\x31\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01\x05\x00\x04\x20'
openssl dgst -sha256 -binary "$1"
