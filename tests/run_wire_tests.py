#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""Run every B-AEC Wire Test, one after another, and report one result.

    python3 tests/run_wire_tests.py [--exe build/BACnetExampleBAEC] [--port 48040]

Exit code 0 = every check in every script passed.
"""
import argparse
import subprocess
import sys
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPOSITORY = TESTS.parent
sys.path.insert(0, str(REPOSITORY))

import wiretest  # noqa: E402  (the vendored harness at the repository root)

SCRIPTS = ["discovery_test.py", "elevator_test.py", "object_test.py", "schedule_test.py",
           "event_log_test.py", "backup_restore_test.py"]


def main():
    arguments = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBAEC")),
                           help="path to BACnetExampleBAEC (default: build/BACnetExampleBAEC)")
    arguments.add_argument("--port", type=int, default=48040, help="the advanced elevator controller's UDP port (default 48040)")
    args = arguments.parse_args()

    outcomes = []
    for script in SCRIPTS:
        print(f"\n===== {script} =====", flush=True)
        run = subprocess.run([sys.executable, "-B", str(TESTS / script), "--exe", args.exe, "--port", str(args.port)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        print(run.stdout, end="", flush=True)
        totals = [line for line in run.stdout.splitlines() if "checks passed" in line]
        outcomes.append((script, run.returncode, totals[-1] if totals else "no summary"))

    print("\n===== summary =====")
    for script, returncode, totals in outcomes:
        print(f"{'PASS' if returncode == 0 else 'FAIL'}  {script:24} {totals}")
    failed = [script for script, returncode, _ in outcomes if returncode != 0]
    print("ALL WIRE TESTS PASSED" if not failed else f"FAILED: {', '.join(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
