# SPDX-License-Identifier: GPL-2.0-or-later
"""Every locale file has the same keys, and every key the code asks for exists.

obs_module_text() returns the KEY itself when a translation is missing, so a gap shows up
as "Dock.Title" printed on a button. Exit code 1 lists every problem.
"""
import re
import sys
from pathlib import Path

LOCALES = ("en-US.ini", "it-IT.ini")
KEY_LINE = re.compile(r'^\s*([A-Za-z0-9_.]+)\s*=', re.MULTILINE)
USED_KEY = re.compile(r'obs_module_text\(\s*"([^"]+)"')


def keys_of(path: Path) -> set[str]:
    return set(KEY_LINE.findall(path.read_text(encoding="utf-8")))


def used_keys(root: Path) -> set[str]:
    found: set[str] = set()
    for path in (root / "src").rglob("*"):
        if path.suffix in (".cpp", ".hpp", ".c", ".h"):
            found |= set(USED_KEY.findall(path.read_text(encoding="utf-8")))
    return found


def problems(root: Path) -> list[str]:
    locale_dir = root / "data" / "locale"
    sets = {name: keys_of(locale_dir / name) for name in LOCALES}
    reference = sets[LOCALES[0]]
    out: list[str] = []
    for name in LOCALES[1:]:
        out += [f"{name}: missing key {k}" for k in sorted(reference - sets[name])]
        out += [f"{name}: extra key {k}" for k in sorted(sets[name] - reference)]
    out += [f"{LOCALES[0]}: missing key {k} (used in code)" for k in sorted(used_keys(root) - reference)]
    return out


def main() -> int:
    found = problems(Path(__file__).resolve().parent.parent)
    for line in found:
        print(line)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
