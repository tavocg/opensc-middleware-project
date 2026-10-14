#!/usr/bin/env python3
"""Copied launcher, fake tools and a real PTY; no PCSC, card or real PIN."""
import errno
import os
from pathlib import Path
import pty
import select
import shutil
import stat
import subprocess
import tempfile
import termios
import time

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as temporary:
    sandbox = Path(temporary)
    (sandbox / 'scripts').mkdir()
    (sandbox / 'bin').mkdir()
    module = sandbox / 'build/opensc/src/pkcs11/.libs/opensc-pkcs11.so'
    module.parent.mkdir(parents=True)
    module.touch()
    launcher = sandbox / 'scripts/sign-jcop4-pkcs11.sh'
    shutil.copyfile(root / 'scripts/sign-jcop4-pkcs11.sh', launcher)
    builder = sandbox / 'scripts/build-jcop4-pkcs11-sign.sh'
    builder.write_text('''#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd "$(dirname "$0")/.." && pwd)
printf 'build\n' >> "$task_root/calls"
[[ $# == 0 || ( $# == 1 && $1 == --rsa-pkcs ) ]]
[[ ! -f $task_root/build-fail ]] || exit 7
''')
    helper = sandbox / 'build/opensc/jcop4-pkcs11-sign'
    helper.write_text('''#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd "$(dirname "$0")/../.." && pwd)
printf 'helper\n' >> "$task_root/calls"
[[ $# == 2 && $1 == 1 && $2 == /*/opensc-pkcs11.so ]]
[[ ! ${OPENSC_DRIVER+x} && ! ${OPENSC_API_TRACE_FILE+x} && ! ${LIBASEP11+x} && ! ${LIBPCSCLITE_DELEGATE+x} && ! ${PCSCLITE_CSOCK_NAME+x} && ! ${OPENSSL_CONF+x} ]]
[[ $OPENSC_DEBUG == 0 && $LC_ALL == C && $(cat /dev/stdin) == '' ]]
python3 - "$OPENSC_CONF" <<'PY'
from pathlib import Path
import stat,sys
p=Path(sys.argv[1]); s=p.read_text()
assert stat.S_IMODE(p.stat().st_mode)==0o600
assert stat.S_IMODE(p.parent.stat().st_mode)==0o700
for value in ['card_drivers = iasecc;', 'chipdoc_profile = cr-rsa2048;', 'use_file_caching = no;', 'use_pin_caching = no;', 'atomic = false;', 'lock_login = false;', 'enable_pace = false;', 'enable_pinpad = false;']:
 assert value in s
PY
printf 'signature_test_result=validated_rsa2048_sha256_pkcs11_closed\n'
''')
    helper.chmod(0o700)
    shutil.copyfile(helper, sandbox/'build/opensc/jcop4-pkcs11-rsa-pkcs')
    (sandbox/'build/opensc/jcop4-pkcs11-rsa-pkcs').chmod(0o700)
    timeout = sandbox / 'bin/timeout'
    timeout.write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $1 == --foreground && $2 == --signal=TERM && $3 == --kill-after=5s && $4 == 120s ]]
shift 4
task_root=$(cd "$(dirname "$0")/.." && pwd)
printf 'timeout\n' >> "$task_root/calls"
# Simulate an abruptly killed helper's terminal state, restored by launcher.
stty -echo </dev/tty
[[ ! -f $task_root/timeout-fail ]] || exit 124
[[ ! -f $task_root/helper-fail ]] || exit 3
"$@"
''')
    timeout.chmod(0o700)
    environment = dict(os.environ, PATH=str(sandbox/'bin')+os.pathsep+os.environ['PATH'],
                       OPENSC_DEBUG='9', OPENSC_DRIVER='default', OPENSC_API_TRACE_FILE='dummy',
                       LIBASEP11='dummy', LIBPCSCLITE_DELEGATE='dummy', PCSCLITE_CSOCK_NAME='dummy', OPENSSL_CONF='dummy')
    no_terminal = subprocess.run(['bash', str(launcher), '1'], env=environment, stdin=subprocess.DEVNULL,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert no_terminal.returncode == 2 and not (sandbox/'calls').exists()
    for variant_args in [[], ['--rsa-pkcs']]:
        for case, expected in [('success', 0), ('build-fail', 7), ('helper-fail', 3), ('timeout-fail', 124), ('bad-reader', 2)]:
            for name in ['build-fail', 'helper-fail', 'timeout-fail', 'calls']:
                (sandbox/name).unlink(missing_ok=True)
            if case.endswith('-fail'):
                (sandbox/case).touch()
            pid, master = pty.fork()
            if not pid:
                os.execve('/usr/bin/env', ['env', 'bash', str(launcher)]+variant_args+['1025' if case=='bad-reader' else '1'], environment)
            captured = bytearray()
            deadline = time.monotonic()+10
            while time.monotonic() < deadline:
                ready, _, _ = select.select([master], [], [], 0.2)
                if not ready:
                    continue
                try:
                    data = os.read(master, 8192)
                    if not data:
                        break
                    captured.extend(data)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
            else:
                os.kill(pid, 9)
                os.waitpid(pid, 0)
                raise AssertionError('software launcher exceeded test deadline')
            assert termios.tcgetattr(master)[3] & termios.ECHO, case
            os.close(master)
            _, status = os.waitpid(pid, 0)
            assert os.waitstatus_to_exitcode(status) == expected, (case, bytes(captured))
            calls = (sandbox/'calls').read_text().splitlines() if (sandbox/'calls').exists() else []
            assert calls == {'success':['build','timeout','helper'], 'build-fail':['build'], 'helper-fail':['build','timeout'], 'timeout-fail':['build','timeout'], 'bad-reader':[]}[case]
            if case == 'bad-reader':
                continue
            report = max((sandbox/'local').glob('*/report.txt'), key=lambda p:p.stat().st_mtime_ns)
            text = report.read_text()
            assert stat.S_IMODE(report.stat().st_mode) == 0o600
            assert 'reader_index=1' in text and 'one_login_one_pso' in text
            assert ('rsa_pkcs_digestinfo' in text) == bool(variant_args)
            if case == 'build-fail':
                assert 'helper_build_status=7' in text and 'helper_exit_status' not in text
            else:
                assert f'helper_exit_status={expected}' in text
            if case == 'success':
                assert 'validated_rsa2048_sha256_pkcs11_closed' in text
print('PASS signing launcher requires a terminal, clears inherited delegates/config, selects one local module/reader, keeps private permissions and invokes foreground timeout/helper at most once')
print('PASS build/helper/timeout/argument failures propagate without retry; launcher restores PTY echo even after simulated helper kill')
