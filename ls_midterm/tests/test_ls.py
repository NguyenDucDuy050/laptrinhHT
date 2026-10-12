#!/usr/bin/env python3
"""Small black-box checks for the required ls(1) behaviors."""

import os
import pathlib
import subprocess
import sys
import tempfile


def run(binary, *args, cwd):
    return subprocess.run([binary, *args], cwd=cwd, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main():
    binary = os.path.abspath(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        (root / "visible").write_text("x\n")
        (root / ".hidden").write_text("y\n")
        (root / "nested").mkdir()
        (root / "nested" / "child").write_text("z\n")
        (root / "runme").write_text("run\n")
        (root / "runme").chmod(0o755)
        os.symlink("visible", root / "link")

        result = run(binary, cwd=str(root))
        assert result.returncode == 0
        assert "visible" in result.stdout and ".hidden" not in result.stdout

        result = run(binary, "-A", cwd=str(root))
        assert ".hidden" in result.stdout and ".\n" not in result.stdout

        result = run(binary, "-aF", cwd=str(root))
        assert "nested/" in result.stdout and "link@" in result.stdout

        result = run(binary, "-l", cwd=str(root))
        assert "total " in result.stdout and "visible" in result.stdout

        result = run(binary, "-R", cwd=str(root))
        assert "nested:" in result.stdout and "child" in result.stdout

        result = run(binary, "-d", "nested", cwd=str(root))
        assert result.stdout.strip() == "nested"

        result = run(binary, "missing", cwd=str(root))
        assert result.returncode != 0 and "missing" in result.stderr

    print("all ls_midterm tests passed")


if __name__ == "__main__":
    main()
