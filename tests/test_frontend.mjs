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
