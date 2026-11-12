#!/usr/bin/env python3
"""Checks (or applies, with --fix) the code style of src/ and test/ with clang-format.

Uses the .clang-format at the project root. Skips the data and generated
files (GunShotAudio.h, page_gz.h, version_gen.h). clang-format comes from
the PATH or from `pip install clang-format` (same Python as this script).
Exit code 1 if a file would change.
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = {"GunShotAudio.h", "page_gz.h", "version_gen.h"}


def binary():
    found = shutil.which("clang-format")
    if found:
        return found
    scripts = os.path.join(os.path.dirname(sys.executable), "clang-format")
    for candidate in (scripts, scripts + ".exe"):
        if os.path.exists(candidate):
            return candidate
    sys.exit("clang-format not found (pip install clang-format)")


def sources():
    for folder in ("src", "test"):
        for directory, _, names in os.walk(os.path.join(ROOT, folder)):
            for name in sorted(names):
                if name.endswith((".h", ".cpp")) and name not in SKIP:
                    yield os.path.join(directory, name)


def main():
    fix = "--fix" in sys.argv
    tool = binary()
    bad = []
    for path in sources():
        if fix:
            subprocess.run([tool, "-i", "--style=file", path], check=True)
            continue
        result = subprocess.run([tool, "--dry-run", "--Werror", "--style=file", path], capture_output=True, text=True)
        if result.returncode != 0:
            bad.append(os.path.relpath(path, ROOT))
    for path in bad:
        print("not formatted:", path)
    if bad:
        print("run: python scripts/check_format.py --fix")
    print("check_format: %d file(s) checked, %d to format" % (len(list(sources())), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
