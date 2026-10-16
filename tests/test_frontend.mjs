// Browser test for the static WebAssembly frontend.
// Serves web/ with a plain static file server (no C++ backend), drives the
// page in headless Google Chrome over the DevTools protocol, and checks the UI,
// network requests, and console output.
// Usage (from any directory): node tests/test_frontend.mjs

import { spawn } from "node:child_process";
import { cpSync, existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");
const WEB = join(ROOT, "web");
const CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const PORT = 18181;

let failures = 0;
function check(condition, name, detail = "") {
  if (condition) {
    console.log(`PASS: ${name}`);
  } else {
    failures++;
    console.log(`FAIL: ${name} ${detail}`);
  }
}

if (!existsSync(CHROME)) {
  console.log(`SKIPPED: browser tests did not run because Google Chrome was not found at ${CHROME}`);
  process.exit(0);
}
if (!existsSync(join(WEB, "generated", "cpu.wasm"))) {
  console.log("FAIL: web/generated/cpu.wasm not found; run ./scripts/build_wasm.sh");
  process.exit(1);
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const children = [];
const tempDirs = [];

function removeTempDirs() {
  for (const dir of tempDirs) {
    try {
      rmSync(dir, { recursive: true, force: true, maxRetries: 5 });
    } catch {
      // Leftover files in the system temp directory are harmless.
    }
  }
}

// Stops Chrome and the static servers, waiting until they have exited so the
// Chrome profile directory can be removed, then exits.
async function shutdown(code) {
  await Promise.all(children.map((child) => new Promise((resolve) => {
    if (child.exitCode !== null || child.signalCode !== null) return resolve();
    child.once("exit", resolve);
    child.kill();
  })));
  removeTempDirs();
  process.exit(code);
}
process.on("exit", () => {
  for (const child of children) child.kill();
});

async function startStaticServer(directory, port) {
  const server = spawn("python3", ["-m", "http.server", String(port),
    "--bind", "127.0.0.1", "--directory", directory], { stdio: "ignore" });
  children.push(server);
  for (let i = 0; i < 50; i++) {
    try {
      await fetch(`http://127.0.0.1:${port}/`);
      return server;
    } catch {
      await sleep(100);
    }
  }
  throw new Error(`static server on port ${port} did not start`);
}

async function startChrome() {
  const profile = mkdtempSync(join(tmpdir(), "cpu-frontend-test-"));
  tempDirs.push(profile);
  const chrome = spawn(CHROME, ["--headless=new", "--remote-debugging-port=0",
    `--user-data-dir=${profile}`, "--no-first-run", "--no-default-browser-check",
    "--window-size=1400,1000", "about:blank"], { stdio: "ignore" });
  children.push(chrome);

  const portFile = join(profile, "DevToolsActivePort");
  for (let i = 0; i < 100 && !existsSync(portFile); i++) await sleep(100);
  const port = readFileSync(portFile, "utf8").split("\n")[0];
  const targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
  return targets.find((t) => t.type === "page").webSocketDebuggerUrl;
}

class Page {
  constructor(wsUrl) {
    this.ws = new WebSocket(wsUrl);
    this.nextId = 0;
    this.pending = new Map();
    this.problems = [];
    this.requests = [];
    this.responses = [];
    this.dialogs = [];
    this.ws.addEventListener("message", (event) => this.onMessage(JSON.parse(event.data)));
  }

  async open() {
    await new Promise((resolve) => this.ws.addEventListener("open", resolve));
    for (const domain of ["Runtime", "Log", "Page", "Network"]) await this.send(`${domain}.enable`);
  }

  onMessage(msg) {
    if (msg.id && this.pending.has(msg.id)) {
      this.pending.get(msg.id)(msg);
      this.pending.delete(msg.id);
      return;
    }
    const p = msg.params;
    switch (msg.method) {
      case "Runtime.consoleAPICalled":
        if (p.type === "error" || p.type === "warning") {
          this.problems.push(`console.${p.type}: ${p.args.map((a) => a.value ?? a.description).join(" ")}`);
        }
        break;
      case "Runtime.exceptionThrown":
        this.problems.push(`exception: ${p.exceptionDetails.exception?.description ?? p.exceptionDetails.text}`);
        break;
      case "Log.entryAdded":
        if (p.entry.level === "error" || p.entry.level === "warning") {
          this.problems.push(`log: ${p.entry.text} ${p.entry.url ?? ""}`);
        }
        break;
      case "Network.requestWillBeSent":
        this.requests.push(p.request.url);
        break;
      case "Network.responseReceived":
        this.responses.push({ url: p.response.url, status: p.response.status, mime: p.response.mimeType });
        break;
      case "Network.loadingFailed":
        this.problems.push(`network: ${p.errorText}`);
        break;
      case "Page.javascriptDialogOpening":
        this.dialogs.push(p.message);
        this.send("Page.handleJavaScriptDialog", { accept: true });
        break;
    }
  }

  send(method, params = {}) {
    return new Promise((resolve) => {
      const id = ++this.nextId;
      this.pending.set(id, resolve);
      this.ws.send(JSON.stringify({ id, method, params }));
    });
  }

  async eval(expression) {
    const res = await this.send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true });
    if (res.result.exceptionDetails) throw new Error(res.result.exceptionDetails.text);
    return res.result.result.value;
  }

  async waitFor(expression, timeout = 5000) {
    const end = Date.now() + timeout;
    while (Date.now() < end) {
      if (await this.eval(expression)) return true;
      await sleep(50);
    }
    return false;
  }

  async goto(url) {
    this.problems = [];
    this.requests = [];
    this.responses = [];
    await this.send("Page.navigate", { url });
  }

  click(id) {
    return this.eval(`document.getElementById(${JSON.stringify(id)}).click()`);
  }

  // Sets a control and fires the event the app listens for.
  set(id, value, eventName) {
    return this.eval(`(() => { const e = document.getElementById(${JSON.stringify(id)});
      e.value = ${JSON.stringify(value)}; e.dispatchEvent(new Event(${JSON.stringify(eventName)})); })()`);
  }

  // Replaces the editor text as if the user typed it.
  type(source) {
    return this.set("asm-source", source, "input");
  }

  editor() {
    return this.eval(`(() => {
      const t = (id) => document.getElementById(id).textContent;
      const h = document.getElementById("editor-highlight");
      const area = document.getElementById("asm-source");
      const lineHeight = parseFloat(getComputedStyle(area).lineHeight);
      const padding = parseFloat(getComputedStyle(area).paddingTop);
      return {
        source: area.value,
        status: t("asm-status"),
        statusKind: document.getElementById("asm-status").className.replace("asm-status", "").trim(),
        errors: [...document.querySelectorAll("#asm-errors li")].map((li) => li.textContent),
        errorLines: [...document.querySelectorAll("#editor-lines .error")].map((d) => Number(d.textContent)),
        currentLine: Number(document.querySelector("#editor-lines .current")?.textContent ?? 0),
        highlight: h.hidden ? null : {
          kind: h.className.replace("editor-highlight", "").trim(),
          line: Math.round((parseFloat(h.style.top) - padding + area.scrollTop) / lineHeight) + 1,
        },
        program: document.getElementById("program").value,
        inputADisabled: document.getElementById("input-a").disabled,
        examples: [...document.querySelectorAll("#example-select option")].map((o) => o.value),
        selection: area.selectionStart,
      };
    })()`);
  }

  snapshot() {
    return this.eval(`(() => {
      const t = (id) => document.getElementById(id).textContent;
      const cells = [...document.querySelectorAll(".memory-cell")];
      return {
        engine: t("server-status-text"), status: t("cpu-status"), result: t("cpu-result"),
        next: t("cpu-next"), regA: t("reg-a"), regADec: t("reg-a-dec"), regB: t("reg-b"),
        pc: t("pc"), sp: t("sp"),
        flags: [t("flag-zero"), t("flag-less"), t("flag-greater")].join(""),
        traceCount: t("trace-count"),
        trace: [...document.querySelectorAll(".trace-entry .instruction")]
          .map((e) => e.firstChild.textContent),
        memory: cells.map((c) => c.textContent),
        regions: cells.map((c) => c.classList.contains("code") ? "c" :
          c.classList.contains("stack") ? "s" : "d").join(""),
        active: cells.findIndex((c) => c.classList.contains("active")),
        message: document.getElementById("message").hidden ? null : t("message"),
        runDisabled: document.getElementById("run-btn").disabled,
        stepDisabled: document.getElementById("step-btn").disabled,
        resetDisabled: document.getElementById("reset-btn").disabled,
        bDisabled: document.getElementById("input-b").disabled,
        cpuProgram: t("cpu-program"),
        steps: t("cpu-steps"),
        last: t("cpu-last"),
        statusClass: document.getElementById("cpu-status").className,
        description: t("program-description"),
        builtinVisible: !document.getElementById("builtin-panel").hidden,
        editorVisible: !document.getElementById("editor-panel").hidden,
        controlsIn: document.getElementById("run-controls").parentElement.id,
        tabs: ["mode-builtin", "mode-custom"].map((id) =>
          document.getElementById(id).getAttribute("aria-selected")).join(),
      };
    })()`);
  }
}

const statusIs = (status) =>
  `document.getElementById("cpu-status").textContent === ${JSON.stringify(status)}`;

// ---------------------------------------------------------------------------

await startStaticServer(WEB, PORT);
const page = new Page(await startChrome());
await page.open();

const BASE = `http://127.0.0.1:${PORT}/`;
await page.goto(BASE);
check(await page.waitFor(statusIs("READY")), "Page loads the WASM CPU and reaches READY");

let s = await page.snapshot();
check(s.engine === "WASM CPU", "Header shows the WASM engine is running", s.engine);
check(s.memory.length === 256, "Memory grid shows 256 bytes");
check(s.regions === "c".repeat(23) + "d".repeat(169) + "s".repeat(64),
  "Memory regions: 23 code, 169 data, 64 stack bytes (comparison program)");
check(s.memory.slice(0, 5).join(" ") === "01 0A 08 05 0E", // LOAD 10, LOAD_B 5, CMP
  "Memory shows the loaded program bytes", s.memory.slice(0, 5).join(" "));
check(s.active === 0 && s.next === "@00 LOAD 10", "PC highlight and next instruction at 0x00");
const initial = s;

// ---------------------------------------------------------------------------
// Modes and layout
// ---------------------------------------------------------------------------

check(s.builtinVisible && !s.editorVisible && s.controlsIn === "builtin-actions" && s.tabs === "true,false",
  "Starts in Built-in mode with Run/Step/Reset in the program panel", JSON.stringify(s));
check(s.description.startsWith("Compares A with B") && s.cpuProgram === "Comparison (A = 10, B = 5)" &&
  s.steps === "0" && s.last === "—" && s.statusClass === "status-pill status-ready",
  "Program description and CPU details reflect the loaded program", JSON.stringify(s));

const rect = (selector) => page.eval(`(() => {
  const r = document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();
  return { left: r.left, right: r.right, top: r.top, bottom: r.bottom };
})()`);
let left = await rect(".main-left");
let right = await rect(".main-right");
let trace = await rect(".trace-panel");
// The trace continues the left column: same edges, one grid gap (16px) below
// the program panel, not below the taller CPU/Memory column.
const traceBelowLeft = (l, t) =>
  Math.abs(t.left - l.left) < 1 && Math.abs(t.right - l.right) < 1 &&
  t.top - l.bottom > 15 && t.top - l.bottom < 17;
check(right.left > left.right && Math.abs(right.top - left.top) < 2 &&
  traceBelowLeft(left, trace) && trace.top < right.bottom,
  "Desktop: program panel left, CPU state right, trace directly below the program panel",
  JSON.stringify({ left, right, trace }));

await page.click("mode-custom");
await page.waitFor(`document.getElementById("cpu-program").textContent.startsWith("Custom code")`);
s = await page.snapshot();
check(!s.builtinVisible && s.editorVisible && s.controlsIn === "editor-actions" && s.tabs === "false,true" &&
  s.status === "READY" && s.cpuProgram === "Custom code (11 bytes)",
  "Custom tab shows the editor, moves Run/Step/Reset below it and loads the editor code",
  JSON.stringify(s));
const viewport = await page.eval("window.innerHeight");
const runControls = await rect("#run-controls");
const cpuPanel = await rect(".cpu-panel");
check(runControls.bottom <= viewport && cpuPanel.top < viewport && cpuPanel.left > runControls.left,
  "Desktop: editor controls and CPU state are visible together without scrolling",
  JSON.stringify({ runControls, cpuPanel, viewport }));
left = await rect(".main-left");
trace = await rect(".trace-panel");
check(traceBelowLeft(left, trace), "Desktop: in Custom mode the trace starts right below the editor",
  JSON.stringify({ left, trace }));

await page.click("mode-builtin");
await page.waitFor(`document.getElementById("cpu-program").textContent.startsWith("Comparison")`);
s = await page.snapshot();
check(s.builtinVisible && !s.editorVisible && s.controlsIn === "builtin-actions" &&
  s.cpuProgram === "Comparison (A = 10, B = 5)" && s.status === "READY",
  "Built-in tab returns to the last built-in program with the same inputs", JSON.stringify(s));

await page.send("Emulation.setDeviceMetricsOverride",
  { width: 390, height: 844, deviceScaleFactor: 2, mobile: true });
await sleep(200);
left = await rect(".main-left");
right = await rect(".main-right");
trace = await rect(".trace-panel");
const overflow = await page.eval("document.documentElement.scrollWidth - window.innerWidth");
check(right.top >= left.bottom && trace.top >= right.bottom && overflow <= 0,
  "Narrow screen: program, then CPU state and memory, then trace, without horizontal scrolling",
  JSON.stringify({ left, right, trace, overflow }));
await page.send("Emulation.clearDeviceMetricsOverride");
await sleep(200);

// Step
await page.click("step-btn");
await page.waitFor(statusIs("RUNNING"));
s = await page.snapshot();
check(s.trace.length === 1 && s.trace[0] === "LOAD 10" && s.regA === "0A" && s.pc === "0002" &&
  s.active === 2, "Step executes one instruction and moves the PC highlight", JSON.stringify(s.trace));

await page.click("step-btn");
await page.waitFor(`document.querySelectorAll(".trace-entry").length === 2`);
s = await page.snapshot();
check(s.trace.length === 2 && s.regB === "05" && s.pc === "0004", "Second step executes LOAD_B only");

// Run
await page.click("run-btn");
await page.waitFor(statusIs("HALTED"));
s = await page.snapshot();
check(s.result === "A > B" && s.regA === "02" && s.flags === "001" &&
  s.trace.join(",") === "LOAD 10,LOAD_B 5,CMP,JL 0x0C,JG 0x10,LOAD 2,HALT",
  "Run continues to HALT with the full trace", JSON.stringify(s.trace));
check(s.runDisabled && s.stepDisabled && !s.resetDisabled, "Run/Step disabled after HALT, Reset enabled");

// Reset
await page.click("reset-btn");
await page.waitFor(statusIs("READY"));
s = await page.snapshot();
check(s.trace.length === 0 && s.regA === initial.regA && s.pc === initial.pc &&
  s.flags === initial.flags && s.memory.join() === initial.memory.join(),
  "Reset restores the initial registers, flags, trace, and memory");

// Double click Run: the second click must not produce an error.
await page.eval(`(() => { const b = document.getElementById("run-btn"); b.click(); b.click(); })()`);
await page.waitFor(statusIs("HALTED"));
await sleep(200);
s = await page.snapshot();
check(s.message === null && s.trace.length === 7, "Double click on Run executes once without errors");

// All nine programs through the UI
const PROGRAMS = [
  ["comparison", 3, 9, "A < B"], ["comparison", 7, 7, "A == B"],
  ["addition", 200, 100, "Register A = 44 (0x2C)"],
  ["subtraction", 10, 4, "Register A = 6 (0x06)"],
  ["and", 12, 10, "Register A = 8 (0x08)"],
  ["or", 12, 10, "Register A = 14 (0x0E)"],
  ["xor", 12, 10, "Register A = 6 (0x06)"],
  ["not", 12, null, "Register A = 243 (0xF3)"],
  ["shl", 129, null, "Register A = 2 (0x02)"],
  ["shr", 12, null, "Register A = 6 (0x06)"],
];
for (const [program, a, b, expected] of PROGRAMS) {
  await page.set("program", program, "change");
  await page.waitFor(statusIs("READY"));
  const afterChange = await page.snapshot();
  await page.set("input-a", String(a), "input");
  if (b !== null) await page.set("input-b", String(b), "input");
  await page.click("run-btn");
  await page.waitFor(statusIs("HALTED"));
  s = await page.snapshot();
  check(afterChange.trace.length === 0 && s.result === expected && s.bDisabled === (b === null),
    `UI runs ${program}(${a}${b === null ? "" : `, ${b}`}) -> ${expected}`, s.result);
}

// All nine programs with Step only, then Reset
const STEP_PROGRAMS = [
  // [program, a, b, expected A, instructions executed including HALT]
  ["comparison", 200, 13, 2, 7], ["addition", 77, 23, 100, 4],
  ["subtraction", 3, 9, 250, 4], ["and", 0xF0, 0x3C, 0x30, 4],
  ["or", 0xF0, 0x0F, 0xFF, 4], ["xor", 0xFF, 0x0F, 0xF0, 4],
  ["not", 0, null, 255, 3], ["shl", 0x40, null, 0x80, 3], ["shr", 1, null, 0, 3],
];
for (const [program, a, b, expectedA, steps] of STEP_PROGRAMS) {
  await page.set("program", program, "change");
  await page.waitFor(statusIs("READY"));
  await page.set("input-a", String(a), "input");
  if (b !== null) await page.set("input-b", String(b), "input");
  await page.click("step-btn");
  await page.waitFor(`document.getElementById("cpu-steps").textContent === "1"`);
  const first = await page.snapshot();
  for (let i = 1; i < 50 && (await page.snapshot()).status !== "HALTED"; i++) {
    await page.click("step-btn");
    await page.waitFor(`document.getElementById("cpu-steps").textContent === "${i + 1}"`);
  }
  s = await page.snapshot();
  const inputs = b === null ? `A = ${a}` : `A = ${a}, B = ${b}`;
  check(first.status === "RUNNING" && first.trace.length === 1 &&
    first.regA === a.toString(16).toUpperCase().padStart(2, "0") &&
    s.status === "HALTED" && parseInt(s.regA, 16) === expectedA && s.steps === String(steps) &&
    s.last.endsWith("HALT") && s.cpuProgram.endsWith(`(${inputs})`),
    `Step through ${program}(${a}${b === null ? "" : `, ${b}`}) -> A = ${expectedA} in ${steps} steps`,
    JSON.stringify({ regA: s.regA, steps: s.steps, last: s.last, program: s.cpuProgram }));
  await page.click("reset-btn");
  await page.waitFor(statusIs("READY"));
  s = await page.snapshot();
  check(s.steps === "0" && s.trace.length === 0 && s.regA === "00" && s.pc === "0000" && s.last === "—",
    `Reset after ${program} returns to READY with 0 steps`);
}

// Invalid input
await page.set("program", "addition", "change");
await page.waitFor(statusIs("READY"));
await page.set("input-a", "300", "input");
s = await page.snapshot();
check(s.message === "Input A must be a whole number from 0 to 255." && s.runDisabled && s.stepDisabled,
  "Invalid input shows an error and disables Run/Step", JSON.stringify(s.message));
await page.set("input-a", "2.5", "input");
s = await page.snapshot();
check(s.message !== null && s.runDisabled, "Fractional input is rejected");
await page.set("input-a", "250", "input");
await page.set("input-b", "10", "input");
await page.click("run-btn");
await page.waitFor(statusIs("HALTED"));
s = await page.snapshot();
check(s.message === null && s.result === "Register A = 4 (0x04)", "Fixed input runs (250 + 10 wraps to 4)");

// ---------------------------------------------------------------------------
// Assembly editor
// ---------------------------------------------------------------------------

const EXAMPLES = await page.eval("ASM_EXAMPLES.map((e) => ({ id: e.id, expectedA: e.expectedA }))");
let e = await page.editor();
check(e.examples.join() === ["", ...EXAMPLES.map((x) => x.id)].join() && EXAMPLES.length === 4,
  "Example menu lists the 4 examples");
check(e.source.startsWith("; Count down") && e.status.startsWith("The CPU is running the built-in program"),
  "Editor keeps its code and says a built-in program is loaded", e.status);

const loadAsm = async () => {
  const before = await page.eval(`document.getElementById("cpu-status").textContent`);
  await page.click("load-asm-btn");
  await sleep(150);
  return before;
};

for (const example of EXAMPLES) {
  await page.set("example-select", example.id, "change");
  await loadAsm();
  await page.waitFor(statusIs("READY"));
  e = await page.editor();
  await page.click("run-btn");
  await page.waitFor(statusIs("HALTED"));
  s = await page.snapshot();
  check(e.program === "custom" && e.inputADisabled && e.statusKind === "ok" &&
    parseInt(s.regA, 16) === example.expectedA && s.trace.length > 0,
    `Example "${example.id}" loads and runs on the WASM CPU (A = ${example.expectedA})`,
    JSON.stringify({ program: e.program, status: e.status, regA: s.regA }));
}

// Step through the countdown example and follow the highlight.
await page.set("example-select", "countdown", "change");
await loadAsm();
await page.waitFor(statusIs("READY"));
e = await page.editor();
s = await page.snapshot();
check(e.highlight?.kind === "current" && e.highlight.line === 4 && e.currentLine === 4 &&
  s.memory.slice(0, 4).join(" ") === "01 05 07 B0" && s.regions.startsWith("c".repeat(11) + "d"),
  "Load Program puts the machine code in memory and highlights line 4 (LOAD 5)",
  JSON.stringify({ highlight: e.highlight, memory: s.memory.slice(0, 4), regions: s.regions.slice(0, 14) }));
const loadedMemory = s.memory;

await page.click("step-btn");
await page.waitFor(`document.querySelectorAll(".trace-entry").length === 1`);
e = await page.editor();
s = await page.snapshot();
check(s.regA === "05" && s.pc === "0002" && s.trace[0] === "LOAD 5" && e.highlight?.line === 5,
  "Step runs one instruction: A = 5, PC = 2, highlight moves to line 5", JSON.stringify(e.highlight));

await page.click("step-btn");
await page.waitFor(`document.querySelectorAll(".trace-entry").length === 2`);
s = await page.snapshot();
e = await page.editor();
check(s.memory[0xB0] === "05" && s.trace[1] === "STORE 0xB0" && e.highlight?.line === 6,
  "STORE writes memory[0xB0] = 05 and the trace shows it", JSON.stringify({ b0: s.memory[0xB0] }));

await page.click("run-btn");
await page.waitFor(statusIs("HALTED"));
s = await page.snapshot();
e = await page.editor();
check(s.regA === "00" && s.memory[0xB0] === "01" && s.traceCount === "21 instructions" &&
  s.flags === "100" && e.highlight?.kind === "halted" && e.highlight.line === 9,
  "Run finishes the loop: 21 instructions, A = 0, Zero flag set, HALT line highlighted",
  JSON.stringify({ regA: s.regA, b0: s.memory[0xB0], count: s.traceCount, h: e.highlight }));
left = await rect(".main-left");
trace = await rect(".trace-panel");
const traceList = await rect("#trace-list");
check(traceBelowLeft(left, trace) && traceList.bottom - traceList.top <= 341,
  "Desktop: with 21 trace entries the trace stays below the editor and scrolls inside its panel",
  JSON.stringify({ left, trace, traceList }));

await page.click("reset-btn");
await page.waitFor(statusIs("READY"));
s = await page.snapshot();
e = await page.editor();
check(s.trace.length === 0 && s.regA === "00" && s.pc === "0000" &&
  s.memory.join() === loadedMemory.join() && e.highlight?.line === 4 && e.program === "custom",
  "Reset restarts the custom program with its original memory");

// Editing after loading
await page.type(e.source.replace("LOAD 5", "LOAD 3"));
e = await page.editor();
check(e.statusKind === "warning" && e.status.startsWith("Edited since loading") && e.highlight === null,
  "Editing marks the loaded code as out of date and hides the highlight");
await loadAsm();
await page.click("run-btn");
await page.waitFor(statusIs("HALTED"));
s = await page.snapshot();
check(s.traceCount === "13 instructions", "Load Program applies the edit (LOAD 3 loops 3 times)", s.traceCount);

// Assembly errors keep the loaded program.
const haltedState = await page.snapshot();
await page.type("LOAD 5\nLAOD 1\nJMP nowhere\nHALT");
await loadAsm();
e = await page.editor();
s = await page.snapshot();
check(e.errors.join("|") === "Line 2: Unknown instruction 'LAOD'|Line 3: Undefined label 'nowhere'" &&
  e.errorLines.join() === "2,3" && e.statusKind === "error",
  "Assembly errors are listed with line numbers and marked in the gutter", JSON.stringify(e.errors));
check(s.status === "HALTED" && s.traceCount === haltedState.traceCount &&
  s.memory.join() === haltedState.memory.join() && e.program === "custom",
  "Failed assembly leaves the loaded program and CPU state unchanged");

await page.eval(`document.querySelector("#asm-errors button").click()`);
e = await page.editor();
check(e.selection === "LOAD 5\n".length, "Clicking an error selects that line in the editor", String(e.selection));

await page.set("program", "addition", "change");
await page.waitFor(statusIs("READY"));
await page.click("mode-custom"); // tries to load the invalid editor code
await page.waitFor(`document.getElementById("asm-errors").children.length > 0`);
e = await page.editor();
s = await page.snapshot();
check(e.program === "custom" && s.editorVisible && s.status === "READY" && s.next === "@00 LOAD 250" &&
  s.cpuProgram.startsWith("Addition (A = 250"),
  "Failed assembly from the Custom tab stays in the editor and keeps the built-in program loaded",
  JSON.stringify({ program: e.program, next: s.next, cpu: s.cpuProgram }));
await page.click("run-btn");
await sleep(150);
s = await page.snapshot();
check(s.status === "READY" && s.steps === "0" && (await page.editor()).errors.length === 2,
  "Run with invalid code shows the errors instead of running the built-in program");
check(e.status.startsWith("Assembly failed with 2 errors"), "Status explains the failure", e.status);

for (const [source, expected, label] of [
  ["", "Program is empty", "empty program"],
  ["LOAD 1\n".repeat(97), "reserved for the stack", "program larger than 192 bytes"],
  ["STORE 0xC0\nHALT", "Line 1: STORE address 0xC0 is in the stack region", "write into the stack region"],
  ["LOAD 256\nHALT", "Line 1: Value 256 is out of range", "out-of-range value"],
  ["a: HALT\na: HALT", "Line 2: Duplicate label 'a'", "duplicate label"],
  ["HALT 1", "Line 1: HALT does not take an operand", "extra operand"],
]) {
  await page.type(source);
  await loadAsm();
  e = await page.editor();
  check(e.errors.some((x) => x.includes(expected)) &&
    (await page.snapshot()).cpuProgram.startsWith("Addition"),
    `Rejected in the editor: ${label}`, JSON.stringify(e.errors));
}

// Choosing "Custom" in the program menu assembles the editor's code.
await page.type("LOAD 7\nLOAD_B 6\nADD_B\nHALT");
await page.set("program", "custom", "change");
await page.waitFor(`document.getElementById("cpu-next").textContent === "@00 LOAD 7"`);
await page.click("run-btn");
await page.waitFor(statusIs("HALTED"));
s = await page.snapshot();
e = await page.editor();
check(e.program === "custom" && s.regA === "0D" && s.regB === "06" && e.errors.length === 0,
  "Selecting Custom in the program menu loads the editor code (7 + 6 = 13)");

// CPU errors and the instruction limit still come from the C++ CPU.
await page.type("LOAD 1\nPOP\nHALT");
await loadAsm();
await page.click("step-btn");
await page.click("step-btn");
await page.waitFor(statusIs("ERROR"));
s = await page.snapshot();
e = await page.editor();
check(s.message?.includes("Stack underflow") && e.highlight?.kind === "error" && e.highlight.line === 2,
  "A CPU error (POP on an empty stack) is shown and its line is highlighted",
  JSON.stringify({ message: s.message, h: e.highlight }));

await page.type("loop: JMP loop");
await loadAsm();
await page.click("run-btn");
await page.waitFor(statusIs("ERROR"));
s = await page.snapshot();
check(s.message?.includes("Instruction limit reached (1000") && s.traceCount === "1000 instructions",
  "An infinite loop stops at the 1000-instruction limit", s.message);

await page.type("LOAD 1");
await loadAsm();
e = await page.editor();
check(e.statusKind === "warning" && e.status.includes("no HALT"), "Missing HALT shows a warning", e.status);

// Replacing edited code with an example asks first.
await page.type("LOAD 99\nHALT");
await page.set("example-select", "compare", "change");
e = await page.editor();
check(page.dialogs.length === 1 && e.source.startsWith("; Compare"),
  "Choosing an example after editing asks for confirmation", JSON.stringify(page.dialogs));

// Built-in programs still work after custom code.
await page.set("program", "subtraction", "change");
await page.waitFor(statusIs("READY"));
await page.set("input-a", "10", "input");
await page.set("input-b", "4", "input");
await page.click("run-btn");
await page.waitFor(statusIs("HALTED"));
s = await page.snapshot();
e = await page.editor();
check(s.result === "Register A = 6 (0x06)" && !s.bDisabled && e.highlight === null &&
  e.status.startsWith("The CPU is running the built-in program"),
  "Built-in programs still work after custom code", s.result);

// Network and console
const apiRequests = page.requests.filter((url) => url.includes("/api/"));
check(apiRequests.length === 0, "No requests to the old HTTP API", JSON.stringify(apiRequests));
const badResponses = page.responses.filter((r) => r.status >= 400);
check(badResponses.length === 0, "No failed network requests", JSON.stringify(badResponses));
const wasm = page.responses.find((r) => r.url.endsWith("/generated/cpu.wasm"));
check(wasm && wasm.mime === "application/wasm", "cpu.wasm served as application/wasm", JSON.stringify(wasm));
check(page.problems.length === 0, "No console errors, warnings, or exceptions", JSON.stringify(page.problems));

// ---------------------------------------------------------------------------
// Loading failures
// ---------------------------------------------------------------------------

async function brokenSite(port, missingFile) {
  const dir = mkdtempSync(join(tmpdir(), "cpu-frontend-broken-"));
  tempDirs.push(dir);
  cpSync(WEB, dir, { recursive: true });
  rmSync(join(dir, "generated", missingFile));
  await startStaticServer(dir, port);
  await page.goto(`http://127.0.0.1:${port}/`);
  await page.waitFor(`document.getElementById("server-status-text").textContent === "WASM UNAVAILABLE"`);
  return page.snapshot();
}

s = await brokenSite(PORT + 1, "cpu.wasm");
check(s.engine === "WASM UNAVAILABLE" && s.message?.startsWith("Could not start the WebAssembly CPU") &&
  s.runDisabled && s.stepDisabled && s.resetDisabled,
  "Missing cpu.wasm shows a clear error and disables the controls", JSON.stringify(s.message));

s = await brokenSite(PORT + 2, "cpu.js");
check(s.engine === "WASM UNAVAILABLE" && s.message?.startsWith("Could not load generated/cpu.js") &&
  s.runDisabled, "Missing cpu.js shows a clear error", JSON.stringify(s.message));

page.ws.close();
if (failures) {
  console.log(`\n${failures} frontend test(s) failed.`);
  await shutdown(1);
}
console.log("\nAll frontend tests passed.");
await shutdown(0);
