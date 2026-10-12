#!/usr/bin/env bash
set -euo pipefail
umask 077

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_temp=$(mktemp -d)
trap 'rm -rf -- "$task_temp"' EXIT
printf abc > "$task_temp/message.txt"
bash "$task_root/scripts/sha256-digestinfo.sh" "$task_temp/message.txt" > "$task_temp/digest-info.der"
task_hex=$(od -An -v -tx1 "$task_temp/digest-info.der" | tr -d ' \n')
task_expected=3031300d060960864801650304020105000420ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
if [[ $task_hex != "$task_expected" ]]; then
    printf 'FAIL SHA-256 DigestInfo differs from the abc known vector\n' >&2
    exit 1
fi
printf 'PASS SHA-256 DigestInfo matches the complete abc known vector\n'
if bash "$task_root/scripts/sha256-digestinfo.sh" "$task_temp/missing" > "$task_temp/missing.der" 2>/dev/null; then
    printf 'FAIL DigestInfo accepts a missing message\n' >&2
    exit 1
fi
if [[ -s "$task_temp/missing.der" ]]; then
    printf 'FAIL DigestInfo writes data for a missing message\n' >&2
    exit 1
fi
printf 'PASS SHA-256 DigestInfo rejects missing input without output\n'
