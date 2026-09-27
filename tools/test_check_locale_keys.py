# SPDX-License-Identifier: GPL-2.0-or-later
import tempfile
import unittest
from pathlib import Path

import check_locale_keys as clk


class LocaleKeysTest(unittest.TestCase):
    def make_repo(self, en: str, it: str, code: str) -> Path:
        root = Path(tempfile.mkdtemp())
        (root / "data" / "locale").mkdir(parents=True)
        (root / "src" / "plugin").mkdir(parents=True)
        (root / "data" / "locale" / "en-US.ini").write_text(en, encoding="utf-8")
        (root / "data" / "locale" / "it-IT.ini").write_text(it, encoding="utf-8")
        (root / "src" / "plugin" / "a.cpp").write_text(code, encoding="utf-8")
        return root

    def test_all_good(self):
        root = self.make_repo('A="a"\nB="b"\n', 'A="a"\nB="b"\n', 'obs_module_text("A"); obs_module_text(\n "B")')
        self.assertEqual(clk.problems(root), [])

    def test_missing_in_italian(self):
        root = self.make_repo('A="a"\nB="b"\n', 'A="a"\n', 'obs_module_text("A")')
        self.assertIn("it-IT.ini: missing key B", clk.problems(root))

    def test_used_but_undefined(self):
        root = self.make_repo('A="a"\n', 'A="a"\n', 'obs_module_text("Z")')
        self.assertIn("en-US.ini: missing key Z (used in code)", clk.problems(root))


if __name__ == "__main__":
    unittest.main()
