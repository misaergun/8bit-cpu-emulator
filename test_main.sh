
#!/bin/bash

set -e

run_test() {
    local name="$1"
    local input="$2"
    local expected="$3"
    local expected_status="${4:-0}"

    echo ""
    echo "$name"

    local output
    local status=0

    output=$(printf '%b' "$input" | python3 run_test.py 2>&1) || status=$?

    if [[ "$output" == *"$expected"* &&
          "$output" != *"Program timed out"* &&
          "$status" -eq "$expected_status" ]]; then
        echo "PASS: $name"
    else
        echo "FAIL: $name"
        echo "Expected exit code: $expected_status"
        echo "Actual exit code: $status"
        echo "$output"
        exit 1
    fi
}

run_test "Test 1: Invalid program choice" \
    "abc\n" \
    "Invalid input. Enter a valid program choice." 1

run_test "Test 2: Invalid number input" \
    "2\nabc\n" \
    "Invalid number. Enter a value from 0 to 255." 1

run_test "Test 3: EOF during first number input" \
    "2\n" \
    "Enter the first number (0-255):" 1

run_test "Test 4: EOF during second number input" \
    "2\n10\n" \
    "Enter the second number (0-255):" 1

run_test "Test 5: Addition" \
    "2\n10\n5\nr\nn\n" \
    "Final Register A: 15"

run_test "Test 6: Subtraction" \
    "3\n10\n4\nr\nn\n" \
    "Final Register A: 6"

run_test "Test 7: Bitwise AND" \
    "4\n12\n10\nr\nn\n" \
    "Final Register A: 8"

run_test "Test 8: Bitwise OR" \
    "5\n12\n10\nr\nn\n" \
    "Final Register A: 14"

run_test "Test 9: Bitwise XOR" \
    "6\n12\n10\nr\nn\n" \
    "Final Register A: 6"

run_test "Test 10: Bitwise NOT" \
    "7\n12\nr\nn\n" \
    "Final Register A: 243"

run_test "Test 11: Shift Left" \
    "8\n12\nr\nn\n" \
    "Final Register A: 24"

run_test "Test 12: Shift Right" \
    "9\n12\nr\nn\n" \
    "Final Register A: 6"

echo ""
echo "All main tests passed."
