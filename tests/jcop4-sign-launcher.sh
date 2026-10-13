#!/usr/bin/env bash
# Synthetic helper/OpenSSL under a PTY; no card, actual signing or credential.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/vendor/opensc" "$task_tmp/build/opensc"
cp "$task_root/scripts/sign-jcop4.sh" "$task_tmp/scripts/"
cat > "$task_tmp/bin/git" <<'GIT'
#!/usr/bin/env bash
printf '%040d\n' 1
GIT
cat > "$task_tmp/scripts/build-card-sign-test.sh" <<'BUILD'
#!/usr/bin/env bash
printf 'build\n' >> "$FIXTURE_SIGN_ROOT/called"
exit "${FIXTURE_BUILD_FAILURE:-0}"
BUILD
cat > "$task_tmp/bin/openssl" <<'OPENSSL'
#!/usr/bin/env bash
set -euo pipefail
printf 'openssl %s\n' "$1 $2" >> "$FIXTURE_SIGN_ROOT/called"
case "$1 $2" in
    'dgst -sha256')
        if [[ $3 == -binary ]]; then printf '%032d' 0
        elif [[ $3 == -verify ]]; then printf 'Verified OK\n'
        else exit 92; fi ;;
    'x509 -inform') printf 'fixture public key\n' ;;
    *) exit 93 ;;
esac
OPENSSL
cat > "$task_tmp/build/opensc/card-sign-test" <<'HELPER'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${FIXTURE_VERIFY_FIRST:-0} == 1 ]]; then
    [[ $# == 3 && $3 == --verify-first ]] || exit 95
else
    [[ $# == 2 ]] || exit 95
fi
[[ $1 == 2 && -t 0 && -t 1 &&
    ${OPENSC_API_TRACE_FILE-unset} == unset && ${OPENSC_DEBUG_FILE-unset} == unset &&
    ${OPENSC_JCOP3_SESSION_STATE-unset} == unset && ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE-unset} == unset &&
    ${OPENSC_JCOP3_POST_LOGIN_STATUS-unset} == unset && ${OPENSC_JCOP3_POST_LOGIN_SELECT-unset} == unset &&
    ${LIBPCSCLITE_DELEGATE-unset} == unset && ${LIBPCSCLITE_SPY_DELEGATE-unset} == unset &&
    ${LD_PRELOAD-unset} == unset && ${OPENSC_DEEPBIND-unset} == unset &&
    ${JCOP4_PUBLIC_DISCOVERY_TRACE-unset} == unset && ${JCOP4_PUBLIC_SELECT_SPY_TRACE-unset} == unset &&
    ${JCOP4_DIAGNOSTIC_READ4141-unset} == unset && ${JCOP4_DIAGNOSTIC_READ4109-unset} == unset && ${JCOP4_DIAGNOSTIC_AODF-unset} == unset && ${JCOP4_DIAGNOSTIC_PRKDF-unset} == unset &&
    ${OPENSC_DEBUG-} == 0 && $(stat -c %a "$2") == 700 && $(stat -c %a "$OPENSC_CONF") == 600 ]] || exit 94
printf 'helper\n' >> "$FIXTURE_SIGN_ROOT/called"
if [[ ${FIXTURE_HELPER_FAILURE:-0} != 0 ]]; then exit "$FIXTURE_HELPER_FAILURE"; fi
printf 'fixture certificate\n' > "$2/certificate.der"
printf '%0256d' 0 > "$2/signature.bin"
HELPER
chmod +x "$task_tmp/bin/openssl" "$task_tmp/bin/git" "$task_tmp/build/opensc/card-sign-test"
export FIXTURE_SIGN_ROOT="$task_tmp" PATH="$task_tmp/bin:$PATH" READER=2
export OPENSC_API_TRACE_FILE=/should/not/be/used OPENSC_DEBUG_FILE=/should/not/be/used OPENSC_DEBUG=99
export OPENSC_JCOP3_SESSION_STATE=yes OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes
export OPENSC_JCOP3_POST_LOGIN_STATUS=yes OPENSC_JCOP3_POST_LOGIN_SELECT=yes
export LIBPCSCLITE_DELEGATE=/should/not/be/used LIBPCSCLITE_SPY_DELEGATE=/should/not/be/used
export LD_PRELOAD= OPENSC_DEEPBIND=1 JCOP4_PUBLIC_DISCOVERY_TRACE=/should/not/be/used JCOP4_PUBLIC_SELECT_SPY_TRACE=/should/not/be/used
export JCOP4_DIAGNOSTIC_READ4141=1
export JCOP4_DIAGNOSTIC_READ4109=1
export JCOP4_DIAGNOSTIC_AODF=1
export JCOP4_DIAGNOSTIC_PRKDF=1
printf -v task_command 'bash %q' "$task_tmp/scripts/sign-jcop4.sh"
task_run() {
    local task_expected=$1 task_rc=0
    rm -f "$task_tmp/called"
    script -q -e -c "$task_command" /dev/null </dev/null > "$task_tmp/output.txt" 2>&1 || task_rc=$?
    [[ $task_rc == "$task_expected" ]]
}
task_rc=0
bash "$task_tmp/scripts/sign-jcop4.sh" </dev/null > "$task_tmp/output.txt" 2>&1 || task_rc=$?
[[ $task_rc == 2 && ! -e $task_tmp/called ]]
printf 'PASS JCOP4 signing launcher rejects redirected input/output before compiling or accessing the card\n'
task_run 0
[[ $(< "$task_tmp/called") == $'build\nopenssl dgst -sha256\nhelper\nopenssl x509 -inform\nopenssl dgst -sha256' &&
    $(< "$task_tmp/output.txt") == *'Firma verificada.'* ]]
for task_report in "$task_tmp"/local/jcop4-signature-*/report.txt; do
    [[ $(stat -c %a "$task_report") == 600 && $(< "$task_report") == *'Verified OK'* ]]
done
printf 'PASS JCOP4 signing launcher keeps the helper on a terminal, isolates debug and verifies once afterward\n'
export FIXTURE_VERIFY_FIRST=1
printf -v task_command 'bash %q --verify-first' "$task_tmp/scripts/sign-jcop4.sh"
task_run 0
[[ $(< "$task_tmp/called") == $'build\nopenssl dgst -sha256\nhelper\nopenssl x509 -inform\nopenssl dgst -sha256' &&
    $(< "$task_tmp/output.txt") == *'Firma verificada.'* ]]
task_found=0
for task_report in "$task_tmp"/local/jcop4-signature-*/report.txt; do
    if [[ $(< "$task_report") == *'mode=experimental_apdu_sha256_rsa_verify_first'* ]]; then
        [[ $(< "$task_report") == *'Verified OK'* && $(stat -c %a "$task_report") == 600 ]]
        task_found=1
    fi
done
[[ $task_found == 1 ]]
printf 'PASS opt-in VERIFY-first is explicit, isolates capture/delegation and independently verifies its output\n'
export FIXTURE_HELPER_FAILURE=17
task_run 17
[[ $(< "$task_tmp/called") == $'build\nopenssl dgst -sha256\nhelper' &&
    $(< "$task_tmp/output.txt") != *'Firma verificada.'* ]]
printf 'PASS JCOP4 signing launcher stops after helper failure without retry or verification\n'
export FIXTURE_BUILD_FAILURE=19
task_run 19
[[ $(< "$task_tmp/called") == build ]]
printf 'PASS JCOP4 signing launcher stops before helper execution if compilation fails\n'
printf -v task_command 'bash %q --unexpected-mode' "$task_tmp/scripts/sign-jcop4.sh"
task_run 2
[[ ! -e $task_tmp/called ]]
printf 'PASS unknown signing variants are rejected before build or helper execution\n'
