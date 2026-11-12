#!/usr/bin/env python3
"""Checks that the README lists every console command of the firmware.

The commands are the rows of Actions::COMMANDS (src/control/Actions.cpp), one
per line: CMD("name", "usage", ...). Section 11 of README.md must have a row
`name` for each of them, and must not list a command that no longer exists.
Run from Turret_firmware/ (the CI does); exit code 1 on a mismatch.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def firmware_commands():
    text = open(os.path.join(ROOT, "src", "control", "Actions.cpp"), encoding="utf-8").read()
    return set(re.findall(r'^\s*CMD\("([a-z-]+)",', text, re.MULTILINE))


def readme_commands():
    text = open(os.path.join(ROOT, "README.md"), encoding="utf-8").read()
    start = text.index("## 11. Serial console")
    end = text.index("## 12.", start)
    names = set()
    for line in text[start:end].splitlines():
        if not line.startswith("| `"):
            continue
        first_cell = re.split(r"(?<!\\)\|", line)[1]  # cells; "\|" inside a cell is not a separator
        # Every `...` of the first cell: its first word is a command name.
        for code in re.findall(r"`([^`]+)`", first_cell):
            names.add(code.split()[0].split("\\")[0])
    return names


def main():
    firmware = firmware_commands()
    readme = readme_commands()
    # "set LabMarkers true" and "?" are documented as examples, not commands.
    readme = {n for n in readme if n.islower() and n.replace("-", "").isalpha()}
    missing = sorted(firmware - readme)
    stale = sorted(readme - firmware)
    for name in missing:
        print("README section 11 does not list the command:", name)
    for name in stale:
        print("README section 11 lists a command the firmware does not have:", name)
    print("check_commands: %d commands in the firmware, %d in the README" % (len(firmware), len(readme)))
    return 1 if missing or stale else 0


if __name__ == "__main__":
    sys.exit(main())
