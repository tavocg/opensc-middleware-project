#!/usr/bin/env python3
"""Real helper/OpenSSL/PTY and a standalone module; never load OpenSC or PCSC."""
import fcntl
import os
from pathlib import Path
import pty
import select
import shlex
import signal
import subprocess
import tempfile
import termios
import time

ROOT = Path(__file__).resolve().parents[1]


def controlling_tty():
    os.setsid()
    fcntl.ioctl(0, termios.TIOCSCTTY, 0)


def execute(command, env, response=None, terminal_stdout=False):
    master, slave = pty.openpty()
    before = termios.tcgetattr(slave)
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(command, env=env, stdin=slave,
                                   stdout=slave if terminal_stdout else output,
                                   stderr=output, preexec_fn=controlling_tty)
        console = b""
        sent = False
        deadline = time.monotonic()+20
        try:
            while process.poll() is None:
                assert time.monotonic() < deadline, "helper timed out"
                ready, _, _ = select.select([master], [], [], .05)
                if ready:
                    console += os.read(master, 4096)
                if response is not None and not sent and b"un solo intento): " in console:
                    hidden = termios.tcgetattr(slave)
                    assert not hidden[3] & (termios.ECHO | termios.ECHONL)
                    if isinstance(response, int):
                        os.kill(process.pid, response)
                    else:
                        os.write(master, response)
                    sent = True
            assert termios.tcgetattr(slave) == before, "terminal state changed"
            assert not select.select([slave], [], [], 0)[0], "pending PIN input"
            output.seek(0)
            report = output.read()
            assert b"777777" not in report+console
            assert b"Synthetic private identity" not in report+console
            return process.returncode, report, sent, console
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            os.close(master)
            os.close(slave)


def main():
    with tempfile.TemporaryDirectory(prefix="auto-sign-test-") as directory:
        tmp = Path(directory)
        module = tmp / "fixture.so"
        binary = tmp / "auto-sign"
        cflags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "libcrypto"], text=True))
        libs = shlex.split(subprocess.check_output(["pkg-config", "--libs", "libcrypto"], text=True))
        cc = [os.environ.get("CC", "cc"), "-std=c99", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
              "-I"+str(ROOT / "vendor/opensc/src"), *cflags]
        subprocess.run([*cc, "-shared", "-fPIC", str(ROOT / "tests/auto-sign-module.c"), *libs, "-o", str(module)], check=True)
        subprocess.run([*cc, '-DAUTO_MODULE_PATH="'+str(module)+'"', str(ROOT / "tools/pkcs11-auto-sign.c"),
                        "-ldl", *libs, "-o", str(binary)], check=True)
        cases = [("normal", b"777777\n", True), ("auto_logout", b"77777777777777\n", True),
                 ("login_fail", b"777777\n", False), ("sign_fail", b"777777\n", False),
                 ("bad_signature", b"777777\n", False), ("duplicate_cert", None, False),
                 ("duplicate_key", b"777777\n", False), ("no_mechanism", None, False),
                 ("already_logged", None, False), ("close_fail", b"777777\n", False),
                 ("finalize_fail", b"777777\n", False), ("normal", b"12345\n", False),
                 ("normal", b"7"*32+b"\n", False), ("normal", b"\x8023456\n", False),
                 ("normal", signal.SIGINT, False), ("normal", signal.SIGTERM, False)]
        for number, (mode, response, success) in enumerate(cases):
            events = tmp / f"events-{number}"
            env = {"PATH": os.environ["PATH"], "OPENSC_CONF": "/dev/null", "FIXTURE_MODE": mode,
                   "FIXTURE_EVENTS": str(events)}
            code, report, prompted, _ = execute([str(binary), "0", "LlaveDeFirma"], env, response)
            assert (code == 0) == success, (mode, code, report)
            history = events.read_text().splitlines()
            assert history.count("login") <= 1 and history.count("sign") <= 1
            assert history.count("initialize") == history.count("finalize") == 1
            assert history.count("open") == history.count("close")
            if response is None:
                assert not prompted and "login" not in history
            if mode == "login_fail":
                assert "sign_init" not in history and "logout" not in history
            if mode == "normal" and not success:
                assert "login" not in history and b"login_result" not in report
            if success:
                assert b"signature_verification=valid" in report
                assert b"changed_message_verification=rejected" in report
                assert b"signature_verified_cleanup_api_ok" in report
            print(f"PASS helper scenario {number}: {mode}")
        env = {"PATH": os.environ["PATH"], "OPENSC_CONF": "/dev/null"}
        for args in [("-1", "LlaveDeFirma"), ("32", "LlaveDeFirma"), ("0", "")]:
            assert subprocess.run([str(binary), *args], env=env, capture_output=True).returncode == 2
        assert subprocess.run([str(binary), "0", "LlaveDeFirma"], env={**env, "OPENSC_CONF": "/tmp/profile.conf"}, capture_output=True).returncode == 2
        print("PASS invalid arguments/configuration stop before module initialization")
        # Exercise the real launcher in an isolated project with software commands.
        project = tmp / "project"
        (project / "scripts").mkdir(parents=True)
        (project / "build/opensc").mkdir(parents=True)
        (project / "scripts/sign-auto.sh").write_bytes((ROOT / "scripts/sign-auto.sh").read_bytes())
        (project / "scripts/build-pkcs11-auto-sign.sh").write_text('exit 0\n')
        helper = project / "build/opensc/pkcs11-auto-sign"
        helper.write_text('''#!/usr/bin/env python3
import os, sys
assert sys.argv[1:] == ["0", "LlaveDeFirma"]
assert set(os.environ) <= {"PATH", "LC_ALL", "OPENSC_CONF", "OPENSC_DEBUG", "LD_LIBRARY_PATH", "LC_CTYPE"}
assert os.environ["OPENSC_CONF"] == "/dev/null"
assert os.environ["OPENSC_DEBUG"] == "0"
assert os.environ["LD_LIBRARY_PATH"].endswith("build/opensc/src/libopensc/.libs")
print("fixture_invocation=1")
''')
        helper.chmod(0o700)
        for exit_status in (0, 17):
            original = helper.read_text()
            helper.write_text(original+f"\nsys.exit({exit_status})\n")
            code, _, _, _ = execute(["bash", str(project / "scripts/sign-auto.sh"), "0", "LlaveDeFirma"],
                                   {"PATH": os.environ["PATH"], "OPENSC_CONF": "/tmp/old.conf", "OPENSC_JCOP3_SESSION_STATE": "yes"}, terminal_stdout=True)
            assert code == exit_status
            helper.write_text(original)
        reports = list((project / "local").glob("*/report.txt"))
        assert len(reports) == 2
        for report in reports:
            assert report.stat().st_mode & 0o777 == 0o600
            assert report.parent.stat().st_mode & 0o777 == 0o700
            assert report.read_text().count("fixture_invocation=1") == 1
            assert not list(report.parent.glob("*.conf"))
        print("PASS launcher: empty environment, no config, one call, exit status, private report")


if __name__ == "__main__":
    main()
