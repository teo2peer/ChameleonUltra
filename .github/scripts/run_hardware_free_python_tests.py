#!/usr/bin/env python3
"""Run every Python test script except explicitly documented hardware tests."""

from pathlib import Path
import os
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = REPO_ROOT / "software" / "script" / "tests"

MANUAL_OR_HIL_TESTS = {
    "test_hard_acquire.py": "requires a serial device and a physical HF tag",
    "test_ultra.py": "requires a connected ChameleonUltra device",
}


def main() -> int:
    candidates = sorted(TEST_DIR.glob("test_*.py"))
    missing_exclusions = set(MANUAL_OR_HIL_TESTS) - {path.name for path in candidates}
    if missing_exclusions:
        raise RuntimeError(f"stale hardware-test exclusions: {sorted(missing_exclusions)}")

    tests = [path for path in candidates if path.name not in MANUAL_OR_HIL_TESTS]
    if not tests:
        raise RuntimeError("no hardware-free Python tests found")

    for path in tests:
        print(f"Running {path.relative_to(REPO_ROOT)}", flush=True)
        python_path = os.pathsep.join(
            filter(None, (str(TEST_DIR.parent), os.environ.get("PYTHONPATH")))
        )
        subprocess.run(
            [sys.executable, str(path)],
            cwd=TEST_DIR.parent,
            env={**os.environ, "PYTHONPATH": python_path},
            check=True,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
