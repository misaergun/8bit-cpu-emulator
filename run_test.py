
import subprocess
import sys

try:
    result = subprocess.run(
        ["./cpu_emulator"],
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
