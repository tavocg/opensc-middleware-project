#!/usr/bin/env bash
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build=$1
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
"${CC:-cc}" -std=c99 -fPIC -shared -Wall -Wextra -Werror -Wno-unused-parameter \
    -I"$task_root/vendor/opensc/src" "$task_root/tests/pkcs11-sign-fallback-module.c" \
    -o "$task_tmp/module.so"
printf 'synthetic signing fixture\n' > "$task_tmp/message.txt"
for task_case in auth-error device-error cancel-error success; do
    task_error=0x101
    case "$task_case" in
        device-error) task_error=0x30 ;;
        cancel-error) task_error=0x1 ;;
        success) task_error=0 ;;
    esac
    : > "$task_tmp/api.log"
    task_status=0
    env -u OPENSC_PKCS11_TOOL_NO_SIGN_FALLBACK FIXTURE_API_LOG="$task_tmp/api.log" \
        FIXTURE_SIGN_ERROR="$task_error" "$task_build/src/tools/pkcs11-tool" \
        --module "$task_tmp/module.so" --slot-index 0 --sign --mechanism RSA-PKCS \
        --input-file "$task_tmp/message.txt" --output-file "$task_tmp/signature.bin" \
        > "$task_tmp/report.txt" 2>&1 || task_status=$?
    if [[ $task_case == success ]]; then
        [[ $task_status == 0 && $(grep -c '^init$' "$task_tmp/api.log") == 1 ]]
        [[ $(wc -c < "$task_tmp/signature.bin") == 256 ]]
    else
        # The synchronized upstream tool has its ordinary multipart fallback;
        # the dedicated sign/auto helper separately enforces one C_Sign.
        [[ $task_status != 0 && $(grep -c '^init$' "$task_tmp/api.log") == 2 ]]
        [[ $(grep -c '^sign$' "$task_tmp/api.log") == 1 && $(grep -c '^final$' "$task_tmp/api.log") == 1 ]]
    fi
    printf 'PASS pkcs11-tool signing fallback: %s\n' "$task_case"
done
