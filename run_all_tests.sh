#!/bin/bash
# Entry point kept at the project root; the real script is in scripts/.
exec "$(dirname "$0")/scripts/run_all_tests.sh" "$@"
