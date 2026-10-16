// Tests the WebAssembly CPU interface (src/wasm_api.cpp) in Node.
// Requires web/generated/cpu.js and cpu.wasm (./scripts/build_wasm.sh) and
// build/cpu_emulator (./run_all_tests.sh builds it).
// Usage (from any directory): node tests/test_wasm.mjs

import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { createRequire } from "node:module";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");
const MODULE_JS = join(ROOT, "web", "generated", "cpu.js");
const CPU_EMULATOR = join(ROOT, "build", "cpu_emulator");

let failures = 0;
function check(condition, name, detail = "") {
  if (condition) {
    console.log(`PASS: ${name}`);
  } else {
    failures++;
    console.log(`FAIL: ${name} ${detail}`);
  }
}

for (const file of [MODULE_JS, CPU_EMULATOR]) {
  if (!existsSync(file)) {
    console.log(`FAIL: ${file} not found`);
    process.exit(1);
  }
}

const createCpuModule = createRequire(import.meta.url)(MODULE_JS);
const cpu = await createCpuModule();

function call(name, ...args) {
  const types = args.map((x) => (typeof x === "string" ? "string" : "number"));
  return JSON.parse(cpu.UTF8ToString(cpu.ccall(name, "number", types, args)));
}
const load = (program, a, b = NaN) => call("cpu_load", program, a, b);

function terminalResult(choice, a, b) {
  const lines = [choice, a];
  if (choice <= 6) lines.push(b);
  lines.push("r", "n");
  const out = spawnSync(CPU_EMULATOR, { input: lines.join("\n") + "\n", encoding: "utf8" }).stdout;
  const match = out.match(/Final Register A: (\d+)/);
  return match ? Number(match[1]) : null;
}

// ---------------------------------------------------------------------------
// Initial state and program list
// ---------------------------------------------------------------------------

let r = call("cpu_state");
check(r.ok && r.state.status === "IDLE" && r.state.program === null,
  "Module starts IDLE with no program");
r = call("cpu_step");
check(!r.ok && r.error.includes("No program loaded"), "Step without a program is rejected");
r = call("cpu_run");
check(!r.ok && r.error.includes("No program loaded"), "Run without a program is rejected");

const PROGRAMS = [
  ["comparison", 1, (a, b) => (a < b ? 1 : a > b ? 2 : 3)],
  ["addition", 2, (a, b) => (a + b) & 0xff],
  ["subtraction", 3, (a, b) => (a - b) & 0xff],
  ["and", 4, (a, b) => a & b],
  ["or", 5, (a, b) => a | b],
  ["xor", 6, (a, b) => a ^ b],
  ["not", 7, (a) => ~a & 0xff],
  ["shl", 8, (a) => (a << 1) & 0xff],
  ["shr", 9, (a) => a >> 1],
];
const INPUTS = [[3, 9], [9, 3], [7, 7], [10, 5], [200, 100], [0, 0], [255, 1]];

r = call("cpu_programs");
check(r.ok && r.programs.map((p) => p.id).join() === PROGRAMS.map((p) => p[0]).join(),
  "cpu_programs lists the 9 programs");
check(r.programs.filter((p) => !p.needs_b).map((p) => p.id).join() === "not,shl,shr",
  "NOT/SHL/SHR do not need input B");

// ---------------------------------------------------------------------------
// All nine programs: WASM == terminal emulator == expected
// ---------------------------------------------------------------------------

for (const [id, choice, expected] of PROGRAMS) {
  const mismatches = [];
  for (const [a, b] of INPUTS) {
    const loaded = choice <= 6 ? load(id, a, b) : load(id, a);
    const run = call("cpu_run");
    const wasmA = run.state && run.state.registers.a;
    const want = expected(a, b);
    const term = terminalResult(choice, a, b);
    if (!loaded.ok || !run.ok || run.state.status !== "HALTED" || wasmA !== want || wasmA !== term) {
      mismatches.push({ a, b, wasmA, want, term });
    }
  }
  check(mismatches.length === 0, `${id}: WASM result == terminal result == expected`,
    JSON.stringify(mismatches));
}

load("comparison", 3, 9);
check(call("cpu_run").state.result.text === "A < B", "Comparison text A < B");
load("comparison", 9, 3);
check(call("cpu_run").state.result.text === "A > B", "Comparison text A > B");
load("comparison", 7, 7);
check(call("cpu_run").state.result.text === "A == B", "Comparison text A == B");

// ---------------------------------------------------------------------------
// Step, run, trace
// ---------------------------------------------------------------------------

const initial = load("comparison", 9, 3).state;
check(initial.status === "READY" && initial.steps === 0 && initial.trace.length === 0,
  "Load gives READY with an empty trace");

let s = call("cpu_step").state;
check(s.steps === 1 && s.trace.length === 1 && s.status === "RUNNING",
  "Step executes exactly one instruction");
check(s.trace[0].name === "LOAD" && s.trace[0].operand === 9 && s.trace[0].address === 0 &&
  s.trace[0].before.a === 0 && s.trace[0].after.a === 9 && s.registers.pc === 2,
  "Step trace entry: LOAD 9 at 0, A 0 -> 9, PC -> 2");

s = call("cpu_step").state;
check(s.steps === 2 && s.registers.b === 3 && s.registers.pc === 4,
  "Second step executes only LOAD_B");

s = call("cpu_run").state;
const names = s.trace.map((t) => t.name).join(" ");
check(s.status === "HALTED" && names === "LOAD LOAD_B CMP JL JG LOAD HALT",
  "Run continues from the current PC and stops at HALT", names);
check(s.trace.every((t, i) => i === 0 || s.trace[i - 1].after.pc === t.before.pc &&
  JSON.stringify(s.trace[i - 1].after) === JSON.stringify(t.before)),
  "Trace entries are linked (after == next before)");
check(s.next_instruction === null, "No next instruction after HALT");

r = call("cpu_step");
check(!r.ok && r.error.includes("halted") && r.state.status === "HALTED",
  "Step after HALT is rejected and returns the state");
r = call("cpu_run");
check(!r.ok && r.error.includes("halted"), "Run after HALT is rejected");

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

s = call("cpu_reset").state;
check(s.status === "READY" && s.steps === 0 && s.trace.length === 0, "Reset returns to READY");
check(JSON.stringify(s.registers) === JSON.stringify(initial.registers) &&
  JSON.stringify(s.flags) === JSON.stringify(initial.flags),
  "Reset restores the initial registers and flags");
check(JSON.stringify(s.memory) === JSON.stringify(initial.memory),
  "Reset restores the initial memory");
check(call("cpu_run").state.result.text === "A > B", "Program runs again after reset");

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

s = load("comparison", 200, 100).state;
check(Array.isArray(s.memory) && s.memory.length === 256, "State contains 256 memory bytes");
const call_memory = (address) =>
  cpu.ccall("cpu_memory_byte", "number", ["number"], [address]);
const direct = [];
for (let i = 0; i < 256; i++) direct.push(call_memory(i));
check(JSON.stringify(direct) === JSON.stringify(s.memory),
  "cpu_memory_byte matches state memory for all 256 addresses");
check(s.memory.slice(0, 4).join() === "1,200,8,100" &&
  s.memory.slice(23).every((v) => v === 0),
  "Program bytes at the start, rest of memory zero");
check(s.layout.code_size === 23 && s.layout.stack_limit === 192 && s.layout.memory_size === 256,
  "Memory layout reported");
check(call_memory(-1) === -1 && call_memory(256) === -1 && call_memory(100000) === -1,
  "Out-of-range memory addresses return -1");

// ---------------------------------------------------------------------------
// Invalid inputs
// ---------------------------------------------------------------------------

const before = call("cpu_state").state;
const invalid = [
  [["addition", 256, 1], "a = 256"],
  [["addition", -1, 1], "a = -1"],
  [["addition", 1.5, 1], "a = 1.5"],
  [["addition", NaN, 1], "a = NaN"],
  [["addition", Infinity, 1], "a = Infinity"],
  [["addition", 1, NaN], "missing b"],
  [["addition", 1, 300], "b = 300"],
  [["addition", 1, 2.25], "b = 2.25"],
  [["nope", 1, 2], "unknown program"],
  [["", 1, 2], "empty program id"],
];
for (const [args, label] of invalid) {
  r = load(...args);
  check(!r.ok && typeof r.error === "string" && r.error.length > 0,
    `Invalid input rejected: ${label}`, JSON.stringify(r).slice(0, 120));
}
check(JSON.stringify(call("cpu_state").state) === JSON.stringify(before),
  "Rejected inputs leave the CPU state unchanged");

r = load("not", 12);
check(r.ok && r.state.program.b === null, "Unary program accepted without b");
r = load("shl", 12, 999);
check(r.ok, "Unary program ignores b");

// ---------------------------------------------------------------------------
// Assembly loading (cpu_load_assembly)
// ---------------------------------------------------------------------------

const asm = (source) => call("cpu_load_assembly", source);

load("addition", 10, 5);
const builtinState = call("cpu_state").state;
r = asm("LOAD 1\nLAOD 2\nJMP missing\nHALT");
check(!r.ok && r.error === "Assembly failed with 2 errors." && !("state" in r) &&
  JSON.stringify(r.assembly.errors) === JSON.stringify([
    { line: 2, message: "Unknown instruction 'LAOD'" },
    { line: 3, message: "Undefined label 'missing'" }]),
  "cpu_load_assembly reports errors with line numbers");
check(JSON.stringify(call("cpu_state").state) === JSON.stringify(builtinState),
  "Failed assembly does not change the loaded program or CPU state");

r = asm("start:  LOAD 40   ; comment\n        ADD 2\n        HALT\n");
check(r.ok && r.state.program.id === "custom" && r.state.status === "READY" &&
  r.state.layout.code_size === 5 && r.state.memory.slice(0, 6).join() === "1,40,2,2,255,0",
  "Valid assembly is loaded into memory and the CPU is reset");
check(JSON.stringify(r.assembly.source_map) === JSON.stringify([
  { line: 1, address: 0, size: 2, instruction: true },
  { line: 2, address: 2, size: 2, instruction: true },
  { line: 3, address: 4, size: 1, instruction: true }]) && r.assembly.warnings.length === 0,
  "Source map links each instruction to its source line");
s = call("cpu_step").state;
check(s.steps === 1 && s.registers.a === 40 && s.registers.pc === 2, "Step runs one custom instruction");
s = call("cpu_run").state;
check(s.status === "HALTED" && s.result.text === "Register A = 42 (0x2A)" && s.trace.length === 3,
  "Run finishes the custom program");
s = call("cpu_reset").state;
check(s.status === "READY" && s.program.id === "custom" && s.registers.a === 0 &&
  s.memory.slice(0, 5).join() === "1,40,2,2,255", "Reset restarts the custom program");

r = asm("loop: JMP loop");
check(r.ok && r.assembly.warnings.length === 1, "Program without HALT loads with a warning");
r = call("cpu_run");
check(!r.ok && r.error.includes("Instruction limit reached") && r.state.steps === 1000,
  "Custom infinite loop stops at the instruction limit");

for (const [source, label] of [["", "empty source"], ["; nothing", "comments only"],
  ["LOAD 1\n".repeat(100), "too large"], ["STORE 200\nHALT", "stack write"],
  ["\u0000\u0001", "control characters"]]) {
  r = asm(source);
  check(!r.ok && r.assembly.errors.length > 0, `cpu_load_assembly rejects ${label}`);
}
check(call("cpu_state").state.program.id === "custom", "Rejected programs leave the custom program loaded");

load("comparison", 3, 9);
check(call("cpu_run").state.result.text === "A < B", "Built-in programs still work after custom code");

if (failures) {
  console.log(`\n${failures} WASM test(s) failed.`);
  process.exit(1);
}
console.log("\nAll WASM interface tests passed.");
