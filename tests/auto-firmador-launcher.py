#!/usr/bin/env python3
"""Run the real shell launcher with a fake firmador; no Java, GUI or PCSC."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="auto-firmador-test-") as directory:
        project = Path(directory)
        (project / "scripts").mkdir()
        (project / "bin").mkdir()
        launcher = project / "scripts/firmador-auto.sh"
        launcher.write_bytes((ROOT / "scripts/firmador-auto.sh").read_bytes())
        module = project / "build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
        core = project / "build/opensc/src/libopensc/.libs/libopensc.so"
        for file in (module, core):
            file.parent.mkdir(parents=True)
            file.touch()
        fake = project / "bin/firmador"
        fake.write_text('''#!/usr/bin/env python3
import json, os, pathlib, resource, sys
root = pathlib.Path(__file__).resolve().parents[1]
assert len(sys.argv) == 1
assert os.environ["OPENSC_CONF"] == "/dev/null"
assert os.environ["OPENSC_DEBUG"] == "0"
assert "OPENSC_JCOP3_PIN_STATUS_IN_PLACE" not in os.environ
assert "OPENSC_API_TRACE_FILE" not in os.environ
assert os.environ["LIBASEP11"] == str(root/"build/opensc/src/pkcs11/.libs/opensc-pkcs11.so")
assert os.environ["LD_LIBRARY_PATH"] == str(root/"build/opensc/src/pkcs11/.libs")+":"+str(root/"build/opensc/src/libopensc/.libs")
allowed = {"PATH", "HOME", "USER", "LOGNAME", "LANG", "LC_ALL", "LC_CTYPE", "DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_DATA_DIRS", "XDG_CONFIG_DIRS", "SSL_CERT_FILE", "SSL_CERT_DIR", "NIX_SSL_CERT_FILE", "TZ", "LD_LIBRARY_PATH", "LIBASEP11", "OPENSC_CONF", "OPENSC_DEBUG"}
assert set(os.environ) <= allowed, set(os.environ)-allowed
assert os.environ["DISPLAY"] == "fixture-display"
assert os.environ["WAYLAND_DISPLAY"] == "fixture-wayland"
assert os.environ["SSL_CERT_FILE"] == "fixture-trust-store"
assert resource.getrlimit(resource.RLIMIT_CORE)[0] == 0
with (root/"calls").open("a") as output:
    output.write("invoked\\n")
(root/"environment.json").write_text(json.dumps(dict(os.environ)))
print("fake GUI output remains visible")
sys.exit(int((root/"exit-status").read_text()))
''')
        fake.chmod(0o700)
        env = {**os.environ, "PATH": str(project/"bin")+":"+os.environ["PATH"],
               "DISPLAY": "fixture-display", "WAYLAND_DISPLAY": "fixture-wayland",
               "SSL_CERT_FILE": "fixture-trust-store", "OPENSC_CONF": "/tmp/old-card.conf",
               "OPENSC_DRIVER": "default", "OPENSC_API_TRACE_FILE": "/tmp/old-trace",
               "OPENSC_DEBUG": "9", "OPENSC_DEBUG_FILE": "private-old-debug",
               "OPENSC_JCOP3_SESSION_STATE": "yes", "OPENSC_JCOP3_PIN_STATUS_IN_PLACE": "yes",
               "PCSCLITE_CSOCK_NAME": "private-old-socket", "LIBPCSCLITE_DELEGATE": "private-old-delegate",
               "JAVA_TOOL_OPTIONS": "private-old-options", "_JAVA_OPTIONS": "private-old-options",
               "JDK_JAVA_OPTIONS": "private-old-options", "OPENSSL_CONF": "private-old-crypto"}
        for expected in (0, 17):
            # Old flags are discarded; automatic PIN handling lives in the driver.
            env.pop("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", None)
            if expected == 17:
                env["OPENSC_JCOP3_PIN_STATUS_IN_PLACE"] = "no"
            (project/"exit-status").write_text(str(expected))
            completed = subprocess.run(["bash", str(launcher)], env=env, capture_output=True, text=True, check=False)
            assert completed.returncode == expected, completed.stderr
            assert "fake GUI output remains visible" in completed.stdout
        assert (project/"calls").read_text().splitlines() == ["invoked", "invoked"]
        reports = list((project/"local").glob("*/report.txt"))
        assert len(reports) == 2
        statuses = set()
        for report in reports:
            assert report.stat().st_mode & 0o777 == 0o600
            values = dict(line.split("=", 1) for line in report.read_text().splitlines())
            assert values["mode"] == "autodetection_gui"
            assert values["configuration"] == "none_dev_null"
            assert values["detection"] == "normal_driver_dispatch"
            assert values["jcop3_pin_status"] == "in_place_physical_query"
            assert values["jcop3_verify"] == "fresh_verify"
            assert "signature_verification" not in values
            statuses.add(values["firmador_exit_status"])
            assert "private-old" not in report.read_text()
            assert not (report.parent/"api.log").exists()
            assert not list(report.parent.glob("*.conf"))
            assert report.parent.stat().st_mode & 0o777 == 0o700
        assert statuses == {"0", "17"}
        print("PASS local module/core and normal autodetection without a .conf")
        print("PASS launcher never needs or forwards a JCOP3 activation flag")
        print("PASS desktop/trust store preserved and inherited diagnostics/Java options removed")
        print("PASS one GUI call without arguments, visible output and exit0/17 propagated")
        print("PASS private reports without obsolete API traces, no signature claim, core dumps disabled")
        for args, expected in [(["--cli"], 2), (["--pin", "dummy"], 2)]:
            completed = subprocess.run(["bash", str(launcher), *args], env=env, capture_output=True)
            assert completed.returncode == expected
        for file in (core, module):
            file.unlink()
            completed = subprocess.run(["bash", str(launcher)], env=env, capture_output=True)
            assert completed.returncode == 1
            file.touch()
        assert (project/"calls").read_text().splitlines() == ["invoked", "invoked"]
        assert len(list((project/"local").glob("*/report.txt"))) == 2
        print("PASS arguments and missing module/core stop before GUI or report creation")
        # A PATH without firmador must fail before generating reports.
        tools = project/"tools"
        tools.mkdir()
        for name in ("dirname",):
            (tools/name).symlink_to(shutil.which(name))
        completed = subprocess.run([shutil.which("bash"), str(launcher)], env={**env, "PATH": str(tools)}, capture_output=True)
        assert completed.returncode == 1
        assert len(list((project/"local").glob("*/report.txt"))) == 2
        print("PASS missing firmador stops before GUI or report creation")


if __name__ == "__main__":
    main()
