#!/usr/bin/env bash
# Actual config parser and fake firmador only, never Java/PCSC/PIN/GUI.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
read -r -a task_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -Wno-deprecated-declarations \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
    "$task_root/tests/pkcs11-login-config.c" -L"$task_build/src/libopensc/.libs" \
    -Wl,-rpath,"$task_build/src/libopensc/.libs" -Wl,--gc-sections -lopensc "${task_libs[@]}" \
    -o "$task_build/pkcs11-login-config-regression"
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc/src/pkcs11/.libs" "$task_tmp/build/opensc/src/libopensc/.libs"
cp "$task_root/scripts/firmador-jcop4.sh" "$task_tmp/scripts/"
cp "$task_build/pkcs11-login-config-regression" "$task_tmp/check-config"
touch "$task_tmp/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so" "$task_tmp/build/opensc/src/libopensc/.libs/libopensc.so"
cat > "$task_tmp/bin/firmador" <<'FAKE'
#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd "$(dirname "$0")/.." && pwd)
[[ $# == 0 && $OPENSC_DEBUG == 0 && $DISPLAY == fixture-display && $XDG_RUNTIME_DIR == fixture-desktop ]]
[[ $LIBASEP11 == "$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so" ]]
[[ $LD_LIBRARY_PATH == "$task_root/build/opensc/src/pkcs11/.libs:$task_root/build/opensc/src/libopensc/.libs" ]]
[[ ! ${LD_PRELOAD+x} && ! ${LD_AUDIT+x} && ! ${OPENSC_DRIVER+x} && ! ${OPENSC_DEBUG_FILE+x} &&
    ! ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE+x} && ! ${OPENSC_JCOP3_SESSION_STATE+x} && ! ${PCSCLITE_CSOCK_NAME+x} &&
    ! ${LIBPCSCLITE_DELEGATE+x} && ! ${JAVA_TOOL_OPTIONS+x} && ! ${_JAVA_OPTIONS+x} && ! ${JDK_JAVA_OPTIONS+x} && ! ${OPENSSL_CONF+x} ]]
[[ $(stat -c %a "$OPENSC_CONF") == 600 && $(stat -c %a "$OPENSC_API_TRACE_FILE") == 600 &&
    $(stat -c %a "$(dirname "$OPENSC_CONF")") == 700 ]]
"$task_root/check-config" "$OPENSC_CONF" 0 chipdoc
printf 'C_SignFinal session=0x1 rv=0x0 a=0x1 b=0x0\n' >> "$OPENSC_API_TRACE_FILE"
printf '%s\n' "$OPENSC_CONF" > "$task_root/last-config"
: > "$task_root/called"
exit 17
FAKE
chmod 700 "$task_tmp/bin/firmador"
task_status=0
env PATH="$task_tmp/bin:$PATH" DISPLAY=fixture-display XDG_RUNTIME_DIR=fixture-desktop \
    OPENSC_DEBUG=9 OPENSC_DRIVER=default OPENSC_DEBUG_FILE=dummy OPENSC_API_TRACE_FILE=dummy \
    OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes OPENSC_JCOP3_SESSION_STATE=yes PCSCLITE_CSOCK_NAME=dummy \
    LIBPCSCLITE_DELEGATE=dummy JAVA_TOOL_OPTIONS=dummy _JAVA_OPTIONS=dummy JDK_JAVA_OPTIONS=dummy OPENSSL_CONF=dummy \
    bash "$task_tmp/scripts/firmador-jcop4.sh" > "$task_tmp/output" 2>&1 || task_status=$?
[[ $task_status == 17 && -f "$task_tmp/called" ]]
task_config=$(< "$task_tmp/last-config")
task_report=$(< "$(dirname "$task_config")/report.txt")
[[ $task_report == *'firmador_exit_status=17'* && $task_report == *'mode=chipdoc_cr_rsa2048_gui'* &&
    $task_report != *dummy* && $(stat -c %a "$(dirname "$task_config")/report.txt") == 600 ]]
printf 'PASS GUI launcher uses local module/libOpenSC, real parsed ChipDoc/cache/login/PCSC configuration, private metadata files and preserves desktop while clearing inherited diagnostics and Java options\n'
printf 'PASS fake GUI runs once without arguments or PIN handling, propagates application exit status, and never treats it as proof of signature/closure\n'
python3 - "$task_config" "$task_tmp" <<'PY'
import pathlib,subprocess,sys
config=pathlib.Path(sys.argv[1]).read_text(); root=pathlib.Path(sys.argv[2])
for before,after in [('cr-rsa2048','unknown'), ('card_drivers = iasecc','card_drivers = default'),
 ('use_pin_caching = no','use_pin_caching = yes'), ('use_file_caching = no','use_file_caching = yes'),
 ('try_emulation_first = no','try_emulation_first = yes'), ('lock_login = false','lock_login = true'),
 ('atomic = false','atomic = true'), ('slots_per_card = 1','slots_per_card = 2'),
 ('enable_pinpad = false','enable_pinpad = true'), ('enable_pace = false','enable_pace = true'),
 ('reconnect_action = leave','reconnect_action = reset')]:
 p=root/'mutated.conf'; p.write_text(config.replace(before,after))
 assert subprocess.run([str(root/'check-config'),str(p),'0','chipdoc']).returncode==1, before
PY
printf 'PASS real OpenSC config checker rejects changed profile/driver, caching/emulation, generic login modes, slot count, PIN-pad/PACE and reconnect policy without a context or hardware\n'
rm "$task_tmp/called"
task_status=0
PATH="$task_tmp/bin:$PATH" bash "$task_tmp/scripts/firmador-jcop4.sh" --cli > "$task_tmp/output" 2>&1 || task_status=$?
[[ $task_status == 2 && ! -f "$task_tmp/called" ]]
rm "$task_tmp/build/opensc/src/libopensc/.libs/libopensc.so"
task_status=0
PATH="$task_tmp/bin:$PATH" bash "$task_tmp/scripts/firmador-jcop4.sh" > "$task_tmp/output" 2>&1 || task_status=$?
[[ $task_status == 1 && ! -f "$task_tmp/called" ]]
printf 'PASS unexpected arguments and missing local core stop before launching GUI\n'
# Changed checker keeps its original interface and actual JCOP3 policy intact.
bash "$task_root/tests/firmador-launcher.sh" "$task_build/pkcs11-login-config-regression"
