
#!/bin/bash

set -e


echo "Test 1: Invalid program choice"

set +e
printf 'abc\n' | python3 run_test.py > /tmp/cpu_test_output.txt 2>&1
status=$?
set -e

output=$(cat /tmp/cpu_test_output.txt)

if [[ "$output" == *"Invalid input. Enter a valid program choice."* && "$status" -eq 1 ]]; then
    echo "PASS: Invalid input is rejected and program exits with code 1"
else
    echo "FAIL: Invalid program choice"
    echo "Exit code: $status"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 2: Invalid number input"

output=$(printf '2\nabc\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Invalid number. Enter a value from 0 to 255."* ]]; then
    echo "PASS: Invalid number is rejected"
else
    echo "FAIL: Invalid number"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 3: EOF during first number input"

output=$(printf '2\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Enter the first number (0-255):"* && "$output" != *"Program timed out"* ]]; then
    echo "PASS: EOF during first number input is handled"
else
    echo "FAIL: EOF during first number input"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 4: EOF during second number input"

output=$(printf '2\n10\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Enter the second number (0-255):"* && "$output" != *"Program timed out"* ]]; then
    echo "PASS: EOF during second number input is handled"
else
    echo "FAIL: EOF during second number input"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 5: Addition program"

output=$(printf '2\n10\n5\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 15"* ]]; then
    echo "PASS: 10 + 5 = 15"
else
    echo "FAIL: Addition result is incorrect"
    echo "$output"
    exit 1
fi


echo ""
echo "Test 6: Subtraction program"

output=$(printf '3\n10\n4\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 6"* ]]; then
    echo "PASS: 10 - 4 = 6"
else
    echo "FAIL: Subtraction result is incorrect"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 7: Bitwise AND"

output=$(printf '4\n12\n10\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 8"* ]]; then
    echo "PASS: 12 AND 10 = 8"
else
    echo "FAIL: Bitwise AND result is incorrect"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 8: Bitwise OR"

output=$(printf '5\n12\n10\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 14"* ]]; then
    echo "PASS: 12 OR 10 = 14"
else
    echo "FAIL: Bitwise OR result is incorrect"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 9: Bitwise XOR"

output=$(printf '6\n12\n10\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 6"* ]]; then
    echo "PASS: 12 XOR 10 = 6"
else
    echo "FAIL: Bitwise XOR result is incorrect"
    echo "$output"
    exit 1
fi


echo ""
echo "Test 10: Bitwise NOT"

output=$(printf '7\n12\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 243"* ]]; then
    echo "PASS: NOT 12 = 243"
else
    echo "FAIL: Bitwise NOT result is incorrect"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 11: Shift Left (SHL)"

output=$(printf '8\n12\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 24"* ]]; then
    echo "PASS: 12 << 1 = 24"
else
    echo "FAIL: Shift Left result is incorrect"
    echo "$output"
    exit 1
fi

echo ""
echo "Test 12: Shift Right (SHR)"

output=$(printf '9\n12\nr\nn\n' | python3 run_test.py 2>&1 || true)

if [[ "$output" == *"Final Register A: 6"* ]]; then
    echo "PASS: 12 >> 1 = 6"
else
    echo "FAIL: Shift Right result is incorrect"
    echo "$output"
    exit 1
fi



echo ""
echo "All main tests passed."
