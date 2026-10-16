#!/usr/bin/env python3
"""HTTP integration tests for web_server.

Builds build/web_server, starts it on a test port, and checks the API against
the terminal emulator (build/cpu_emulator must already be built).
Usage (from any directory): python3 tests/test_web_server.py
"""

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

PORT = 18080
BASE = f"http://127.0.0.1:{PORT}"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
WEB_SERVER = os.path.join(BUILD, "web_server")
CPU_EMULATOR = os.path.join(BUILD, "cpu_emulator")

failures = 0


def check(condition, name, detail=""):
    global failures
    if condition:
        print(f"PASS: {name}")
    else:
        failures += 1
        print(f"FAIL: {name} {detail}")


def request(method, path, body=None, headers=None, raw_body=None):
    data = None
    all_headers = {}
    if body is not None:
        data = json.dumps(body).encode()
        all_headers["Content-Type"] = "application/json"
    if raw_body is not None:
        data = raw_body
    all_headers.update(headers or {})
    req = urllib.request.Request(BASE + path, data=data, method=method,
                                 headers=all_headers)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status, resp.read().decode(), resp.headers
    except urllib.error.HTTPError as err:
        return err.code, err.read().decode(), err.headers


def api(method, path, body=None, **kwargs):
    status, text, _ = request(method, path, body, **kwargs)
    return status, json.loads(text)


def raw_exchange(payload, delay=None):
    """Send raw bytes over a socket and return the response text."""
    with socket.create_connection(("127.0.0.1", PORT), timeout=10) as sock:
        if delay is None:
            sock.sendall(payload)
        else:
            for part in payload:
                sock.sendall(part)
                time.sleep(delay)
        chunks = []
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
    return b"".join(chunks).decode(errors="replace")


def terminal_result(choice, a, b):
    """Run the terminal emulator and return its final Register A."""
    lines = [str(choice), str(a)]
    if choice <= 6:
        lines.append(str(b))
    lines += ["r", "n"]
    out = subprocess.run([CPU_EMULATOR], input="\n".join(lines) + "\n",
                         capture_output=True, text=True, timeout=5,
                         cwd=ROOT).stdout
    for line in out.splitlines():
        if line.startswith("Final Register A:"):
            return int(line.split(":")[1])
    return None


PROGRAMS = [
    # (id, terminal menu choice, expected function)
    ("comparison", 1, lambda a, b: 1 if a < b else 2 if a > b else 3),
    ("addition", 2, lambda a, b: (a + b) & 0xFF),
    ("subtraction", 3, lambda a, b: (a - b) & 0xFF),
    ("and", 4, lambda a, b: a & b),
    ("or", 5, lambda a, b: a | b),
    ("xor", 6, lambda a, b: a ^ b),
    ("not", 7, lambda a, b: (~a) & 0xFF),
    ("shl", 8, lambda a, b: (a << 1) & 0xFF),
    ("shr", 9, lambda a, b: a >> 1),
]
INPUTS = [(3, 9), (9, 3), (7, 7), (10, 5), (200, 100), (0, 0), (255, 1)]


def test_programs():
    status, data = api("GET", "/api/programs")
    ids = [p["id"] for p in data["programs"]]
    check(status == 200 and ids == [p[0] for p in PROGRAMS],
          "GET /api/programs lists the 9 programs")
    unary = [p["id"] for p in data["programs"] if not p["needs_b"]]
    check(unary == ["not", "shl", "shr"], "NOT/SHL/SHR do not need input B")

    for program_id, choice, expected_fn in PROGRAMS:
        mismatches = []
        for a, b in INPUTS:
            body = {"program": program_id, "a": a}
            if choice <= 6:
                body["b"] = b
            status, data = api("POST", "/api/program", body)
            if status != 200 or data["state"]["status"] != "READY":
                mismatches.append((a, b, "load", status))
                continue
            status, data = api("POST", "/api/run")
            state = data["state"]
            web_a = state["registers"]["a"]
            expected = expected_fn(a, b)
            term_a = terminal_result(choice, a, b)
            if (status != 200 or state["status"] != "HALTED"
                    or web_a != expected or web_a != term_a):
                mismatches.append((a, b, web_a, expected, term_a))
        check(not mismatches,
              f"{program_id}: web result == terminal result == expected",
              str(mismatches))


def test_step_and_trace():
    status, data = api("POST", "/api/program",
                       {"program": "comparison", "a": 9, "b": 3})
    state = data["state"]
    check(state["status"] == "READY" and state["trace"] == []
          and state["steps"] == 0, "Load: READY with empty trace")
    check(len(state["memory"]) == 256, "State contains all 256 memory bytes")
    check(state["memory"][:5] == [1, 9, 8, 3, 14],
          "Program bytes are in memory")
    check(state["layout"] == {"memory_size": 256, "code_size": 23,
                              "stack_limit": 192}, "Memory layout reported")
    check(state["next_instruction"] == {"address": 0, "opcode": 1,
                                        "name": "LOAD", "operand": 9},
          "Next instruction is LOAD 9 at 0")

    status, data = api("POST", "/api/step")
    state = data["state"]
    entry = state["trace"][0]
    check(status == 200 and state["status"] == "RUNNING"
          and len(state["trace"]) == 1, "Step executes exactly one instruction")
    check(entry["name"] == "LOAD" and entry["address"] == 0
          and entry["operand"] == 9 and entry["before"]["a"] == 0
          and entry["after"]["a"] == 9 and entry["after"]["pc"] == 2,
          "Trace entry has address, name, operand, before/after")
    check(state["registers"]["a"] == 9 and state["registers"]["pc"] == 2,
          "Registers reflect the step")

    steps = 1
    while state["status"] != "HALTED" and steps < 50:
        status, data = api("POST", "/api/step")
        state = data["state"]
        steps += 1
    names = [t["name"] for t in state["trace"]]
    check(names == ["LOAD", "LOAD_B", "CMP", "JL", "JG", "LOAD", "HALT"],
          "Comparison 9 vs 3 follows the JG branch", str(names))
    check(state["flags"] == {"zero": False, "less": False, "greater": True},
          "Flags after CMP 9 vs 3")
    check(state["result"]["text"] == "A > B", "Comparison result text")
    linked = all(state["trace"][i]["after"] == state["trace"][i + 1]["before"]
                 for i in range(len(state["trace"]) - 1))
    check(linked, "Each trace 'after' equals the next 'before'")
    check(state["next_instruction"] is None, "No next instruction when halted")

    status, data = api("POST", "/api/step")
    check(status == 409 and data["ok"] is False, "Step after HALT -> 409")
    status, data = api("POST", "/api/run")
    check(status == 409, "Run after HALT -> 409")

    status, data = api("POST", "/api/reset")
    state = data["state"]
    check(status == 200 and state["status"] == "READY"
          and state["trace"] == [] and state["registers"]["a"] == 0
          and state["registers"]["pc"] == 0 and state["memory"][1] == 9,
          "Reset restarts the loaded program")

    api("POST", "/api/step")
    status, data = api("POST", "/api/program",
                       {"program": "not", "a": 12})
    state = data["state"]
    check(status == 200 and state["trace"] == []
          and state["program"]["id"] == "not"
          and state["program"]["b"] is None,
          "Changing program clears trace and old state")

    status, data = api("GET", "/api/state")
    check(status == 200 and data["state"]["program"]["id"] == "not",
          "GET /api/state returns current state")


def test_validation():
    bad = [
        ({"program": "addition", "a": 256, "b": 1}, "a = 256"),
        ({"program": "addition", "a": -1, "b": 1}, "a = -1"),
        ({"program": "addition", "a": 1.5, "b": 1}, "a = 1.5"),
        ({"program": "addition", "a": "10", "b": 1}, "a as string"),
        ({"program": "addition", "a": 1}, "missing b"),
        ({"program": "addition", "b": 1}, "missing a"),
        ({"program": "addition", "a": 1, "b": 99999999999}, "huge b"),
        ({"program": "nope", "a": 1}, "unknown program"),
        ({"a": 1}, "missing program"),
    ]
    for body, label in bad:
        status, data = api("POST", "/api/program", body)
        check(status == 400 and data["ok"] is False and data["error"],
              f"Invalid input rejected: {label}", f"[{status}] {data}")

    for raw, label in [(b'{"program":"addition",', "truncated JSON"),
                       (b'{"program":{"x":1}}', "nested object"),
                       (b"", "empty body"),
                       (b"[1,2]", "array body")]:
        status, text, _ = request("POST", "/api/program", raw_body=raw,
                                  headers={"Content-Type": "application/json"})
        check(status == 400 and json.loads(text)["ok"] is False,
              f"Malformed JSON rejected: {label}")

    status, data = api("POST", "/api/program", {"program": "shl", "a": 129})
    check(status == 200, "Unary program accepted without b")

    status, text, _ = request("POST", "/api/program",
                              raw_body=b'{"program":"not","a":1}')
    check(status == 415, "Non-JSON Content-Type -> 415")
    status, text, _ = request("POST", "/api/program",
                              raw_body=b"x" * 5000,
                              headers={"Content-Type": "application/json"})
    check(status == 413, "Oversized body -> 413")
    status, text, headers = request("GET", "/api/run")
    check(status == 405 and headers.get("Allow") == "POST",
          "Wrong method -> 405 with Allow header")
    status, data = api("GET", "/api/unknown")
    check(status == 404 and data["ok"] is False, "Unknown API path -> 404 JSON")


def test_security_and_robustness():
    status, _, _ = request("POST", "/api/reset",
                           headers={"Origin": "http://evil.example"})
    check(status == 403, "Cross-origin POST -> 403")
    status, _, _ = request("POST", "/api/reset",
                           headers={"Origin": f"http://localhost:{PORT}"})
    check(status == 200, "Same-origin POST via localhost allowed")
    resp = raw_exchange(b"GET /api/state HTTP/1.1\r\nHost: evil.example\r\n\r\n")
    check(resp.startswith("HTTP/1.1 403"), "Foreign Host header -> 403")
    resp = raw_exchange(b"GARBAGE\r\n\r\n")
    check(resp.startswith("HTTP/1.1 400"), "Malformed request line -> 400")
    resp = raw_exchange(b"GET /api/state HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"
                        b"X: " % PORT + b"a" * 10000 + b"\r\n\r\n")
    check(resp.startswith("HTTP/1.1 431"), "Huge headers -> 431")

    body = b'{"program":"addition","a":10,"b":5}'
    head = (b"POST /api/program HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"
            b"Content-Type: application/json\r\nContent-Length: %d\r\n\r\n"
            % (PORT, len(body)))
    resp = raw_exchange([head, body[:10], body[10:]], delay=0.05)
    check(resp.startswith("HTTP/1.1 200") and '"id":"addition"' in resp,
          "Body split across packets is read completely")

    # Client that disconnects without sending anything.
    socket.create_connection(("127.0.0.1", PORT), timeout=2).close()
    # Client that promises a body and stops sending (server must time out).
    resp = raw_exchange(head)
    check(resp.startswith("HTTP/1.1 408") or resp.startswith("HTTP/1.1 400"),
          "Incomplete body is rejected", resp[:40])

    for path, ctype in [("/", "text/html"), ("/style.css", "text/css"),
                        ("/app.js", "application/javascript")]:
        status, text, headers = request("GET", path)
        check(status == 200 and headers["Content-Type"].startswith(ctype)
              and len(text) > 0, f"Static file {path} served")
    status, _, _ = request("GET", "/../cpu.h")
    check(status == 404, "Paths outside the whitelist -> 404")

    status, data = api("GET", "/api/state")
    check(status == 200, "Server still alive after bad requests")


def test_missing_web_files():
    # Mirrors the project layout: <tmp>/build/web_server, web files in <tmp>/web.
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "build"))
        exe = os.path.join(tmp, "build", "web_server")
        shutil.copy(WEB_SERVER, exe)
        result = subprocess.run([exe, "--port", str(PORT + 1)], cwd=tmp,
                                capture_output=True, text=True, timeout=5)
        check(result.returncode == 1 and "web/index.html" in result.stderr,
              "Missing web/ folder gives a clear error")

        os.makedirs(os.path.join(tmp, "web"))
        for name in ["index.html", "style.css"]:
            shutil.copy(os.path.join(ROOT, "web", name),
                        os.path.join(tmp, "web", name))
        result = subprocess.run([exe, "--port", str(PORT + 1)], cwd=tmp,
                                capture_output=True, text=True, timeout=5)
        check(result.returncode == 1 and "app.js" in result.stderr,
              "Missing web/app.js gives a clear error")


def main():
    os.chdir(ROOT)
    os.makedirs(BUILD, exist_ok=True)
    build = subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra",
                            "src/web_server.cpp", "-o", WEB_SERVER],
                           capture_output=True, text=True)
    check(build.returncode == 0 and "warning" not in build.stderr,
          "web_server builds without warnings", build.stderr)
    if build.returncode != 0:
        return 1
    if not os.path.exists(CPU_EMULATOR):
        print("FAIL: build/cpu_emulator not found; run ./run_all_tests.sh first")
        return 1

    server = subprocess.Popen([WEB_SERVER, "--port", str(PORT)],
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.PIPE)
    try:
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", PORT), 0.2).close()
                break
            except OSError:
                time.sleep(0.1)
        else:
            print("FAIL: server did not start:",
                  server.stderr.read().decode() if server.poll() else "")
            return 1

        status, data = api("POST", "/api/reset")
        test_programs()
        test_step_and_trace()
        test_validation()
        test_security_and_robustness()
        check(server.poll() is None, "Server process did not crash")
    finally:
        server.terminate()
        server.wait(timeout=5)

    test_missing_web_files()

    if failures:
        print(f"\n{failures} web test(s) failed.")
        return 1
    print("\nAll web server tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
