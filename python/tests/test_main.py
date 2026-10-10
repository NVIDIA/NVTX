# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import subprocess
import sys

import pytest


@pytest.mark.parametrize("absolute_path", [False, True])
def test_cli_imports_sibling_module(tmp_path, absolute_path):
    (tmp_path / "sibling.py").write_text("VALUE = 'wrong module'\n")
    script_dir = tmp_path / "app"
    script_dir.mkdir()
    (script_dir / "sibling.py").write_text("VALUE = 'sibling imported'\n")
    script = script_dir / "main.py"
    script.write_text("import sibling\nprint(sibling.VALUE)\n")
    script_arg = str(script if absolute_path else script.relative_to(tmp_path))

    result = subprocess.run(
        [sys.executable, "-m", "nvtx", script_arg],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "sibling imported"
