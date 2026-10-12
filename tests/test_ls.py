#!/usr/bin/env python3
"""Black-box tests derived from the supplied manual; no GNU ls dependency."""
import errno
import os
from pathlib import Path
import pty
import pwd
import socket
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(Path('./ls').resolve())


class LsTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.env = dict(os.environ, LC_ALL='C', TZ='UTC', BLOCKSIZE='512')
        (self.root / 'alpha').write_bytes(b'a')
        (self.root / 'beta').write_bytes(b'b' * 2048)
        (self.root / '.hidden').touch()
        (self.root / 'sub').mkdir()
        (self.root / 'sub' / 'child').touch()

    def tearDown(self):
        self.tmp.cleanup()

    def run_ls(self, *args, env=None, ok=True):
        result = subprocess.run([BINARY, *args], cwd=self.root,
            env=env or self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, timeout=10)
        if ok:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def test_default_and_hidden(self):
        lines = self.run_ls().stdout.splitlines()
        expected = ['alpha', 'beta', 'sub']
        if os.geteuid() == 0:
            expected.insert(0, '.hidden')
        self.assertEqual(lines, expected)
        self.assertEqual(self.run_ls('-A').stdout.splitlines(), ['.hidden', 'alpha', 'beta', 'sub'])
        self.assertEqual(self.run_ls('-a').stdout.splitlines(), ['.', '..', '.hidden', 'alpha', 'beta', 'sub'])

    def test_sort_and_reverse_operands(self):
        self.assertEqual(self.run_ls('beta', 'alpha').stdout, 'alpha\nbeta\n')
        self.assertEqual(self.run_ls('-r', 'alpha', 'beta').stdout, 'beta\nalpha\n')
        self.assertEqual(self.run_ls('-S', 'alpha', 'beta').stdout, 'beta\nalpha\n')
        self.assertEqual(self.run_ls('-Sr', 'alpha', 'beta').stdout, 'alpha\nbeta\n')
        self.assertEqual(self.run_ls('-f', 'beta', 'alpha').stdout, 'beta\nalpha\n')
        self.assertEqual(self.run_ls('-fr', 'beta', 'alpha').stdout, 'beta\nalpha\n')

    def test_time_selection_and_ties(self):
        os.utime(self.root / 'alpha', ns=(400000000000, 100000000001))
        os.utime(self.root / 'beta', ns=(200000000000, 100000000002))
        self.assertEqual(self.run_ls('-t', 'alpha', 'beta').stdout, 'beta\nalpha\n')
        self.assertEqual(self.run_ls('-tu', 'alpha', 'beta').stdout, 'alpha\nbeta\n')
        self.assertEqual(self.run_ls('-tcu', 'alpha', 'beta').stdout, 'alpha\nbeta\n')
        expected = sorted(['alpha', 'beta'], key=lambda n: (-os.stat(self.root / n).st_ctime_ns, n))
        self.assertEqual(self.run_ls('-tuc', 'alpha', 'beta').stdout.splitlines(), expected)
        os.utime(self.root / 'beta', ns=(400000000000, 100000000001))
        self.assertEqual(self.run_ls('-t', 'beta', 'alpha').stdout, 'alpha\nbeta\n')

    def test_directory_and_recursion(self):
        self.assertEqual(self.run_ls('-d', 'sub').stdout, 'sub\n')
        self.assertEqual(self.run_ls('-Rd', 'sub').stdout, 'sub\n')
        self.assertIn('sub:\nchild\n', self.run_ls('-dR', 'sub').stdout)
        os.symlink('..', self.root / 'sub' / 'parent')
        result = self.run_ls('-Ra').stdout
        self.assertIn('./sub:\n', result)
        self.assertNotIn('./sub/parent:\n', result)
        self.assertNotIn('./..:\n', result)

    def test_multiple_operands(self):
        result = self.run_ls('sub', 'beta', 'alpha').stdout
        self.assertTrue(result.startswith('alpha\nbeta\n\nsub:\n'), result)
        self.assertEqual(self.run_ls('missing', 'alpha', ok=False).stdout, 'alpha\n')
        self.assertIn('missing', self.run_ls('missing', ok=False).stderr)
        self.run_ls('-z', ok=False)
        (self.root / '-name').touch()
        self.assertEqual(self.run_ls('--', '-name').stdout, '-name\n')

    def test_classification(self):
        (self.root / 'alpha').chmod(0o755)
        os.symlink('alpha', self.root / 'link')
        os.mkfifo(self.root / 'pipe')
        with socket.socket(socket.AF_UNIX) as sock:
            sock.bind(str(self.root / 'sock'))
            lines = self.run_ls('-F').stdout.splitlines()
        for name in ['alpha*', 'link@', 'pipe|', 'sock=', 'sub/']:
            self.assertIn(name, lines)

    def test_symlinks(self):
        os.symlink('sub', self.root / 'dirlink')
        os.symlink('missing', self.root / 'broken')
        self.assertEqual(self.run_ls('dirlink').stdout, 'child\n')
        self.assertEqual(self.run_ls('-d', 'dirlink').stdout, 'dirlink\n')
        self.assertIn('dirlink -> sub', self.run_ls('-l', 'dirlink').stdout)
        self.assertIn('broken -> missing', self.run_ls('-l', 'broken').stdout)

    def test_long_format_and_permissions(self):
        (self.root / 'alpha').chmod(0o6754)
        row = self.run_ls('-n', 'alpha').stdout.split()
        self.assertEqual(row[0], '-rwsr-sr--')
        self.assertEqual(row[2], str(os.getuid()))
        self.assertEqual(row[3], str(os.getgid()))
        self.assertEqual(row[4], '1')
        (self.root / 'alpha').chmod(0o6600)
        self.assertTrue(self.run_ls('-l', 'alpha').stdout.startswith('-rwS--S---'))
        (self.root / 'sub').chmod(0o1777)
        self.assertTrue(self.run_ls('-ld', 'sub').stdout.startswith('drwxrwxrwt'))
        (self.root / 'sub').chmod(0o1766)
        self.assertTrue(self.run_ls('-ld', 'sub').stdout.startswith('drwxrw-rwT'))
        (self.root / 'sub').chmod(0o755)
        self.assertEqual(self.run_ls('-ln', 'beta').stdout.split()[2], str(os.getuid()))
        self.assertEqual(self.run_ls('-nl', 'beta').stdout.split()[2], pwd.getpwuid(os.getuid()).pw_name)

    def test_block_sizes_and_sparse_files(self):
        with (self.root / 'sparse').open('wb') as file:
            file.truncate(1024 * 1024)
        st = os.stat(self.root / 'sparse')
        self.assertEqual(int(self.run_ls('-s', 'sparse').stdout.split()[0]), st.st_blocks)
        st = os.stat(self.root / 'beta')
        self.assertEqual(int(self.run_ls('-sk', 'beta').stdout.split()[0]), (st.st_blocks + 1) // 2)
        env = dict(self.env, BLOCKSIZE='3K')
        self.assertEqual(int(self.run_ls('-s', 'beta', env=env).stdout.split()[0]), (st.st_blocks * 512 + 3071) // 3072)
        self.assertEqual(self.run_ls('-k', 'beta').stdout, 'beta\n')
        self.assertEqual(self.run_ls('-h', 'beta').stdout, 'beta\n')
        self.assertIn('2.0K', self.run_ls('-lh', 'beta').stdout)
        self.assertEqual(self.run_ls('-lhk', 'beta').stdout.split()[4], '2048')
        self.assertEqual(self.run_ls('-lkh', 'beta').stdout.split()[4], '2.0K')
        self.assertEqual(self.run_ls('-shk', 'beta').stdout.split()[0], str((st.st_blocks + 1) // 2))

    def test_total(self):
        total = sum(os.lstat(path).st_blocks for path in self.root.iterdir())
        self.assertEqual(self.run_ls('-Al').stdout.splitlines()[0], f'total {total}')
        self.assertEqual(self.run_ls('-Als').stdout.count('total '), 1)
        self.assertFalse(self.run_ls('-As').stdout.startswith('total '))
        (self.root / 'empty').mkdir()
        self.assertEqual(self.run_ls('-l', 'empty').stdout, 'total 0\n')

    def test_inode_device_timezone(self):
        self.assertEqual(int(self.run_ls('-i', 'alpha').stdout.split()[0]), os.stat(self.root / 'alpha').st_ino)
        st = os.stat('/dev/null')
        self.assertIn(f'{os.major(st.st_rdev)}, {os.minor(st.st_rdev)}', self.run_ls('-l', '/dev/null').stdout)
        os.utime(self.root / 'alpha', (0, 0))
        self.assertIn('Jan  1 00:00', self.run_ls('-l', 'alpha').stdout)
        self.assertIn('Jan  1 07:00', self.run_ls('-l', 'alpha', env=dict(self.env, TZ='UTC-7')).stdout)

    def test_nonprintable(self):
        name = 'line\nbreak'
        (self.root / name).touch()
        self.assertEqual(self.run_ls('-q', name).stdout, 'line?break\n')
        self.assertEqual(self.run_ls('-wq', name).stdout, 'line?break\n')
        self.assertEqual(self.run_ls('-qw', name).stdout, name + '\n')
        self.assertEqual(self.run_ls(name).stdout, name + '\n')
        os.symlink(name, self.root / 'link')
        self.assertIn('-> line?break', self.run_ls('-lq', 'link').stdout)

    def test_terminal_defaults(self):
        name = 'bad\tname'
        (self.root / name).touch()
        master, slave = pty.openpty()
        try:
            process = subprocess.Popen([BINARY, '-s'], cwd=self.root, env=self.env,
                stdout=slave, stderr=subprocess.PIPE)
            os.close(slave)
            slave = None
            output = b''
            while True:
                try:
                    data = os.read(master, 4096)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not data:
                    break
                output += data
            _, errors = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 0, errors)
            self.assertTrue(output.startswith(b'total '))
            self.assertIn(b'bad?name', output)
        finally:
            os.close(master)
            if slave is not None:
                os.close(slave)

    @unittest.skipIf(os.geteuid() == 0, 'root bypasses file permissions')
    def test_permission_denied(self):
        (self.root / 'sub').chmod(0)
        try:
            self.assertIn('sub', self.run_ls('sub', ok=False).stderr)
        finally:
            (self.root / 'sub').chmod(0o755)


if __name__ == '__main__':
    unittest.main(verbosity=2)
