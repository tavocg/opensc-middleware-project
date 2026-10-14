#!/usr/bin/env bash
# Temporary copied launcher and fake builder/helper/timeout; never PCSC.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/build/opensc/src/pkcs11/.libs" "$task_tmp/bin"
cp "$task_root/scripts/probe-jcop4-pkcs11.sh" "$task_tmp/scripts/"
touch "$task_tmp/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
cat > "$task_tmp/scripts/build-jcop4-pkcs11-public.sh" <<'BUILD'
#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd "$(dirname "$0")/.." && pwd)
[[ ! -f $task_root/build-fail ]] || exit 7
cat > "$task_root/build/opensc/jcop4-pkcs11-public" <<'HELPER'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 2 && $1 == 1 && $2 == /*/opensc-pkcs11.so ]]
[[ ! ${LD_PRELOAD+x} && ! ${LD_AUDIT+x} && ! ${OPENSC_DRIVER+x} && ! ${OPENSC_API_TRACE_FILE+x} && ! ${LIBASEP11+x} && ! ${LIBPCSCLITE_DELEGATE+x} && ! ${PCSCLITE_CSOCK_NAME+x} ]]
[[ $OPENSC_DEBUG == 0 && $LC_ALL == C && ! -t 0 ]]
[[ $(cat /dev/stdin) == '' ]]
python3 - "$OPENSC_CONF" <<'PY'
import pathlib,stat,sys
p=pathlib.Path(sys.argv[1]); s=p.read_text()
assert stat.S_IMODE(p.stat().st_mode)==0o600
assert stat.S_IMODE(p.parent.stat().st_mode)==0o700
for expected in ['card_drivers = iasecc;', 'chipdoc_profile = cr-rsa2048;', 'use_file_caching = no;', 'use_pin_caching = no;', 'atomic = false;', 'lock_login = false;', 'enable_pace = false;', 'enable_pinpad = false;']:
 assert expected in s,expected
PY
printf 'public_inspection_result=validated_public_only_no_login_or_signature\n'
HELPER
chmod 700 "$task_root/build/opensc/jcop4-pkcs11-public"
BUILD
cat > "$task_tmp/bin/timeout" <<'TIMEOUT'
#!/usr/bin/env bash
set -euo pipefail
[[ $1 == --signal=TERM && $2 == --kill-after=5s && $3 == 45s ]]
shift 3
task_root=$(cd "$(dirname "$0")/.." && pwd)
if [[ -f $task_root/timeout-fail ]]; then exit 124; fi
if [[ -f $task_root/helper-fail ]]; then exit 3; fi
"$@"
TIMEOUT
chmod 700 "$task_tmp/bin/timeout"
# Seed inherited state safely: launcher env-i must remove these dummy values.
for task_case in success build-fail helper-fail timeout-fail; do
    rm -f "$task_tmp/build-fail" "$task_tmp/helper-fail" "$task_tmp/timeout-fail"
    [[ $task_case == success ]] || touch "$task_tmp/$task_case"
    task_status=0
    env PATH="$task_tmp/bin:$PATH" OPENSC_DRIVER=default OPENSC_DEBUG=9 OPENSC_API_TRACE_FILE=private_dummy \
        LIBASEP11=private_dummy LIBPCSCLITE_DELEGATE=private_dummy PCSCLITE_CSOCK_NAME=private_dummy \
        bash "$task_tmp/scripts/probe-jcop4-pkcs11.sh" 1 > "$task_tmp/out" 2>&1 || task_status=$?
    case $task_case in success) [[ $task_status == 0 ]];; build-fail) [[ $task_status == 7 ]];; helper-fail) [[ $task_status == 3 ]];; timeout-fail) [[ $task_status == 124 ]];; esac
    python3 - "$task_tmp/local" "$task_case" <<'PY'
import pathlib,stat,sys
reports=list(pathlib.Path(sys.argv[1]).glob('*/report.txt'))
r=max(reports,key=lambda p:p.stat().st_mtime_ns)
s=r.read_text();assert stat.S_IMODE(r.stat().st_mode)==0o600
assert 'reader_index=1' in s and 'no_pin_once' in s
if sys.argv[2]=='success':assert 'validated_public_only' in s and 'helper_exit_status=0' in s
if sys.argv[2]=='build-fail':assert 'helper_build_status=7' in s and 'helper_exit_status' not in s
if sys.argv[2]=='helper-fail':assert 'helper_exit_status=3' in s
if sys.argv[2]=='timeout-fail':assert 'helper_exit_status=124' in s
PY
done
printf 'PASS launcher uses one empty-environment bounded invocation, chosen reader/module/config and private permissions; preserves build/helper/timeout failures without retry\n'
for task_bad in -1 abc '1 2' 1025; do
    task_status=0
    PATH="$task_tmp/bin:$PATH" bash "$task_tmp/scripts/probe-jcop4-pkcs11.sh" "$task_bad" >/dev/null 2>&1 || task_status=$?
    [[ $task_status == 2 ]]
done
printf 'PASS invalid reader arguments stop before building or invoking diagnostic\n'
