# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from nvtx.__main__ import main


class TestMain(unittest.TestCase):
    def test_script_arguments_are_forwarded(self):
        script_arguments = [
            ["--batch-size", "32"],
            ["--no-linenos", "--annotate-cfuncs"],
            ["--help"],
            ["input.txt", "--", "--batch-size", "32"],
        ]
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "script.py"
            output = Path(directory) / "argv.json"
            script.write_text(
                "import json, sys\n"
                f"with open({str(output)!r}, 'w') as output:\n"
                "    json.dump(sys.argv, output)\n",
                encoding="utf-8",
            )
            for arguments in script_arguments:
                with self.subTest(arguments=arguments):
                    argv = ["nvtx", str(script), *arguments]
                    with patch.object(sys, "argv", argv), patch("nvtx.Profile") as profile:
                        main()
                    self.assertEqual(
                        json.loads(output.read_text(encoding="utf-8")),
                        [str(script), *arguments],
                    )
                    profile.assert_called_once_with(linenos=True, annotate_cfuncs=False)
                    profile.return_value.enable.assert_called_once_with()
                    profile.return_value.disable.assert_called_once_with()

    def test_profiler_options_precede_script(self):
        with patch.object(
            sys, "argv", ["nvtx", "--no-linenos", "--annotate-cfuncs", "script.py"]
        ), patch("nvtx.Profile") as profile, patch("nvtx.__main__.run_path") as run_path:
            main()
        profile.assert_called_once_with(linenos=False, annotate_cfuncs=True)
        run_path.assert_called_once_with("script.py", run_name="__main__")
