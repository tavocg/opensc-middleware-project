#!/usr/bin/env python3
"""Patch/diagram/bundle checks using files and a fake firmador, never hardware."""
import hashlib
import importlib.util
import io
import json
import os
import shutil
import stat
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'vendor/opensc'


def git(*args):
    return subprocess.check_output(['git', '-C', str(SOURCE), *args])


def tree(directory):
    result = {}
    for path in directory.rglob('*'):
        relative = str(path.relative_to(directory))
        if path.is_symlink():
            result[relative] = ('link', os.readlink(path))
        elif path.is_file():
            result[relative] = ('file', stat.S_IMODE(path.stat().st_mode), hashlib.sha256(path.read_bytes()).hexdigest())
    return result


class ExportTests(unittest.TestCase):
    def test_complete_git_apply_roundtrip(self):
        meta = json.loads((ROOT/'patches/opensc-base.json').read_text())
        patch = ROOT/'patches'/meta['patch_file']
        self.assertEqual(hashlib.sha256(patch.read_bytes()).hexdigest(),meta['patch_sha256'])
        with tempfile.TemporaryDirectory(prefix='opensc-export-') as temporary:
            base, target = Path(temporary)/'base', Path(temporary)/'target'
            base.mkdir(); target.mkdir()
            for commit, directory in [(meta['base_commit'],base),(meta['patched_tree'],target)]:
                with tarfile.open(fileobj=io.BytesIO(git('archive',commit))) as archive:
                    archive.extractall(directory, filter='data')
            subprocess.run(['git','-C',str(base),'apply','--check',str(patch)],check=True)
            subprocess.run(['git','-C',str(base),'apply',str(patch)],check=True)
            self.assertEqual(tree(base),tree(target),'Applied source, symlinks and modes must match the entire submodule')
            self.assertNotEqual(subprocess.run(['git','-C',str(base),'apply','--check',str(patch)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode,0)

    def test_diagram_inventory_and_call_targets(self):
        data = json.loads((ROOT/'docs/opensc-surface.json').read_text())
        meta = json.loads((ROOT/'patches/opensc-base.json').read_text())
        rows = [r.split('\t') for r in git('diff','--numstat',data['base'],meta['patched_tree']).decode().splitlines()]
        self.assertEqual({r[2]:(int(r[0]),int(r[1])) for r in rows}, {f['path']:(f['added'],f['deleted']) for f in data['files']})
        functions = {(f['path'],fn['name']):fn for f in data['files'] for fn in f['functions']}
        for fn in functions.values():
            for edge in fn['edges']:
                self.assertTrue(functions[edge['file'],edge['function']]['new'])
        diagram = (ROOT/'docs/opensc-surface.mmd').read_text()
        self.assertLessEqual(diagram.count('-->')+diagram.count('.->'),500)
        self.assertIn(diagram,(ROOT/'README.md').read_text())
        self.assertIn('subgraph scope3["JCOP3"]',diagram)
        self.assertIn('subgraph scope4["JCOP4"]',diagram)

    def test_parser_skips_literals_macros_and_prototypes(self):
        spec=importlib.util.spec_from_file_location('exporter',ROOT/'scripts/export-opensc-patch.py')
        module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        sample='''#define MACRO(x) { not_a_function(x); }
int prototype(int x);
static int actual(int x) { const char *s="} fake() {"; /* bogus(){} */ return another(x); }
struct table { int (*callback)(int); };
'''
        found=module.functions(sample)
        self.assertEqual(set(found),{'actual'})
        self.assertIn('another(x)',found['actual']['bodies'][0])
        self.assertNotIn('fake',found['actual']['bodies'][0])


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='opensc-release-launcher-')
        self.root=Path(self.temporary.name)
        for directory in ['bundle/bin','bundle/lib','bundle/config','bin']:
            (self.root/directory).mkdir(parents=True)
        shutil.copy(ROOT/'scripts/run-release-firmador.sh',self.root/'bundle/bin/firmador-jcop')
        for name in ['jcop3.conf','jcop4.conf']:
            shutil.copy(ROOT/'config'/name,self.root/'bundle/config'/name)
        (self.root/'bundle/lib/libjcop3p11.so').touch()
        fake=self.root/'bin/firmador'
        fake.write_text('''#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd "$(dirname "$0")/.." && pwd)
[[ $# == 0 && $OPENSC_DEBUG == 0 && $DISPLAY == fixture-desktop ]]
[[ $LIBASEP11 == "$task_root/bundle/lib/libjcop3p11.so" && $LD_LIBRARY_PATH == "$task_root/bundle/lib" ]]
[[ ! ${OPENSC_API_TRACE_FILE+x} && ! ${OPENSC_JCOP3_SESSION_STATE+x} && ! ${OPENSC_JCOP3_POST_LOGIN_STATUS+x} && ! ${JAVA_TOOL_OPTIONS+x} && ! ${OPENSC_DRIVER+x} && ! ${LIBPCSCLITE_DELEGATE+x} ]]
if [[ $OPENSC_CONF == */jcop3.conf ]]; then
    [[ $OPENSC_JCOP3_PIN_STATUS_IN_PLACE == yes ]]
else
    [[ $OPENSC_CONF == */jcop4.conf && ! ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE+x} ]]
fi
printf 'called\\n' >> "$task_root/calls"
exit 17
''')
        fake.chmod(0o700)
        self.env={**os.environ,'PATH':str(self.root/'bin')+':'+os.environ['PATH'],'DISPLAY':'fixture-desktop',
                  'OPENSC_API_TRACE_FILE':'dummy','OPENSC_JCOP3_SESSION_STATE':'yes', 'OPENSC_JCOP3_POST_LOGIN_STATUS':'yes',
                  'OPENSC_DRIVER':'default','OPENSC_DEBUG':'9','LIBPCSCLITE_DELEGATE':'dummy','JAVA_TOOL_OPTIONS':'dummy',
                  'OPENSC_JCOP3_PIN_STATUS_IN_PLACE':'yes'}

    def tearDown(self):
        self.temporary.cleanup()

    def run_launcher(self,*args):
        return subprocess.run(['bash',str(self.root/'bundle/bin/firmador-jcop'),*args],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

    def test_two_profiles_launch_once_and_propagate_exit(self):
        for profile in ['jcop3','jcop4']:
            self.assertEqual(self.run_launcher(profile).returncode,17)
        self.assertEqual((self.root/'calls').read_text(),'called\ncalled\n')

    def test_bad_profile_arguments_or_missing_module_stop_before_gui(self):
        for args in [(),('jcop5',),('jcop3','extra')]:
            self.assertEqual(self.run_launcher(*args).returncode,2)
        (self.root/'bundle/lib/libjcop3p11.so').unlink()
        self.assertEqual(self.run_launcher('jcop4').returncode,1)
        self.assertFalse((self.root/'calls').exists())


if __name__=='__main__':
    unittest.main(verbosity=2)
