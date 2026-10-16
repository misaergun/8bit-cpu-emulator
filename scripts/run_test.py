
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMULATOR = os.path.join(ROOT, "build", "cpu_emulator")

try:
    result = subprocess.run(
        [EMULATOR],
        input=sys.stdin.read(),
        capture_output=True,
        text=True,
        timeout=3,
    )

    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    sys.exit(result.returncode)

except subprocess.TimeoutExpired:
    print("FAIL: Program timed out (possible infinite loop).")
    sys.exit(124)
