# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import os
from pathlib import Path
import subprocess
import sys


def _run_python_cmd(*args: str, check: bool = True) -> int:
    return subprocess.run([sys.executable, *args], check=check)


def _run_tests(
    project_dir: Path,
    tests_dir: Path,
    dependency_mode: str,
) -> bool:
    build = os.environ.get("CIBUILDWHEEL_BUILD_IDENTIFIER", "sdist")
    failed = False
    for injection in (False, True):
        mode = dependency_mode + ("-injection" if injection else "")
        args = [
            "-m",
            "pytest",
            f"--junitxml={project_dir / f'junit-{mode}.xml'}",
            f"--junit-prefix={build}-{mode}",
        ]
        if injection:
            args.append("--enable-injection")
        args.append(str(tests_dir))
        returncode = _run_python_cmd(*args, check=False).returncode
        if returncode != 0:
            print(
                f"FAILED: pytest mode {mode} exited with {returncode}",
                file=sys.stderr,
            )
            failed = True
    return failed


def main() -> None:
    tests_dir = Path(__file__).resolve().parent
    project_dir = tests_dir.parents[1]

    _run_python_cmd("-m", "pip", "install", "numpy")
    failed = _run_tests(project_dir, tests_dir, "numpy")
    _run_python_cmd("-m", "pip", "uninstall", "-y", "numpy")
    failed = _run_tests(project_dir, tests_dir, "no-numpy") or failed
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
