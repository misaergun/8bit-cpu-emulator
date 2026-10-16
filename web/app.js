"use strict";

(() => {
  const MEMORY_SIZE = 256;
  const INPUT_DEBOUNCE_MS = 350;

  // Instructions whose operand is a memory address (shown in hex).
  const ADDRESS_OPERANDS = new Set([
    "JMP", "JZ", "JE", "JL", "JG", "CALL", "LOAD_MEM", "STORE", "STORE_B",
  ]);

  const el = {
    program: document.getElementById("program"),
    inputA: document.getElementById("input-a"),
    inputB: document.getElementById("input-b"),
    inputBGroup: document.getElementById("input-b-group"),
    runBtn: document.getElementById("run-btn"),
    stepBtn: document.getElementById("step-btn"),
    resetBtn: document.getElementById("reset-btn"),
    message: document.getElementById("message"),
    serverStatus: document.getElementById("server-status"),
    serverStatusText: document.getElementById("server-status-text"),
    regA: document.getElementById("reg-a"),
    regADec: document.getElementById("reg-a-dec"),
    regB: document.getElementById("reg-b"),
    regBDec: document.getElementById("reg-b-dec"),
    pc: document.getElementById("pc"),
    sp: document.getElementById("sp"),
    flagZero: document.getElementById("flag-zero"),
    flagLess: document.getElementById("flag-less"),
    flagGreater: document.getElementById("flag-greater"),
    cpuStatus: document.getElementById("cpu-status"),
    cpuNext: document.getElementById("cpu-next"),
    cpuResult: document.getElementById("cpu-result"),
    memoryGrid: document.getElementById("memory-grid"),
    traceList: document.getElementById("trace-list"),
    traceCount: document.getElementById("trace-count"),
  };

  let programs = new Map(); // id -> { id, name, needs_b }
  let cpu = null; // WebAssembly module from generated/cpu.js
  let cpuLog = ""; // last diagnostic printed by the WebAssembly runtime
  let state = null; // last state reported by the CPU
  let busy = false; // a Run/Step/Reset action is in progress
  let pendingLoad = null; // promise of an in-flight program load
  let inputTimer = null;
  const memoryCells = [];

  // ------------------------------------------------------------------
  // Helpers
  // ------------------------------------------------------------------

  const hex = (value, digits) =>
    Number(value).toString(16).toUpperCase().padStart(digits, "0");

  class ApiError extends Error {
    constructor(message, apiState) {
      super(message);
      this.state = apiState || null;
    }
  }

  function showMessage(text) {
    el.message.textContent = text;
    el.message.hidden = false;
  }

  function clearMessage() {
    el.message.textContent = "";
    el.message.hidden = true;
  }

  function setEngineStatus(text, ok) {
    el.serverStatus.classList.toggle("offline", !ok);
    el.serverStatusText.textContent = text;
  }

  // ------------------------------------------------------------------
  // WebAssembly CPU
  // ------------------------------------------------------------------

  // Calls a cpu_* function from src/wasm_api.cpp. Each one returns a JSON
  // string: {"ok":true,"state":{...}} or {"ok":false,"error":"...","state":{...}}.
  function callCpu(name, argTypes = [], args = []) {
    if (!cpu) throw new ApiError("The WebAssembly CPU is not loaded.");

    let data;
    try {
      const pointer = cpu.ccall(name, "number", argTypes, args);
      data = JSON.parse(cpu.UTF8ToString(pointer));
    } catch (error) {
      throw new ApiError(`WebAssembly CPU failed in ${name}: ${error.message}`);
    }

    if (!data || data.ok !== true) {
      throw new ApiError((data && data.error) || `${name} failed.`, data && data.state);
    }
    return data;
  }

  const cpuApi = {
    programs: () => callCpu("cpu_programs"),
    state: () => callCpu("cpu_state"),
    // b is NaN for programs that only use input A.
    load: ({ program, a, b }) =>
      callCpu("cpu_load", ["string", "number", "number"],
        [program, a, b === undefined ? NaN : b]),
    step: () => callCpu("cpu_step"),
    run: () => callCpu("cpu_run"),
    reset: () => callCpu("cpu_reset"),
  };

  async function loadCpuModule() {
    if (typeof WebAssembly !== "object") {
      throw new ApiError("This browser does not support WebAssembly.");
    }
    if (typeof createCpuModule !== "function") {
      throw new ApiError(
        "Could not load generated/cpu.js. Build it with ./scripts/build_wasm.sh."
      );
    }
    try {
      return await createCpuModule({
        print: (text) => { cpuLog = text; },
        printErr: (text) => { cpuLog = text; },
      });
    } catch (error) {
      const detail = cpuLog || (error && error.message) || String(error);
      throw new ApiError(`Could not start the WebAssembly CPU (generated/cpu.wasm): ${detail}`);
    }
  }

  // ------------------------------------------------------------------
  // Controls
  // ------------------------------------------------------------------

  function selectedProgram() {
    return programs.get(el.program.value) || null;
  }

  function parseByte(input) {
    const text = input.value.trim();
    if (!/^\d{1,3}$/.test(text)) return null;
    const value = Number(text);
    return value <= 255 ? value : null;
  }

  // Returns { request } with a valid program request, or { error }.
  function readControls() {
    const program = selectedProgram();
    if (!program) return { error: "Select a program." };

    const a = parseByte(el.inputA);
    el.inputA.classList.toggle("invalid", a === null);

    let b = null;
    if (program.needs_b) {
      b = parseByte(el.inputB);
      el.inputB.classList.toggle("invalid", b === null);
    } else {
      el.inputB.classList.remove("invalid");
    }

    if (a === null) return { error: "Input A must be a whole number from 0 to 255." };
    if (program.needs_b && b === null) {
      return { error: "Input B must be a whole number from 0 to 255." };
    }

    const request = { program: program.id, a };
    if (program.needs_b) request.b = b;
    return { request };
  }

  // True when the controls describe a different program than the one loaded.
  function controlsDiffer(request) {
    if (!state || !state.program) return true;
    const loaded = state.program;
    return (
      loaded.id !== request.program ||
      loaded.a !== request.a ||
      (loaded.needs_b && loaded.b !== request.b)
    );
  }

  function updateInputB() {
    const program = selectedProgram();
    const needsB = program ? program.needs_b : true;
    el.inputB.disabled = !needsB;
    el.inputBGroup.title = needsB ? "" : "This program only uses Input A.";
  }

  function updateButtons() {
    const controls = readControls();
    const valid = !controls.error;
    const dirty = valid && controlsDiffer(controls.request);
    const status = state ? state.status : "IDLE";
    const canExecute = valid && (dirty || status === "READY" || status === "RUNNING");

    el.runBtn.disabled = busy || !canExecute;
    el.stepBtn.disabled = busy || !canExecute;
    el.resetBtn.disabled = busy || !state || !state.program;
    el.program.disabled = busy;
    el.inputA.disabled = busy;
    el.inputB.disabled = busy || !(selectedProgram() || { needs_b: true }).needs_b;
  }

  // ------------------------------------------------------------------
  // Rendering
  // ------------------------------------------------------------------

  function buildMemoryGrid() {
    const fragment = document.createDocumentFragment();
    for (let address = 0; address < MEMORY_SIZE; address++) {
      const cell = document.createElement("div");
      cell.className = "memory-cell";
      cell.textContent = "00";
      memoryCells.push(cell);
      fragment.appendChild(cell);
    }
    el.memoryGrid.replaceChildren(fragment);
  }

  function setFlag(element, value) {
    element.textContent = value ? "1" : "0";
    element.classList.toggle("off", !value);
  }

  function formatInstruction(name, operand) {
    if (operand === null || operand === undefined) return name;
    if (ADDRESS_OPERANDS.has(name)) return `${name} 0x${hex(operand, 2)}`;
    return `${name} ${operand}`;
  }

  function describeChanges(before, after) {
    const changes = [];
    const reg = (label, key, digits) => {
      if (before[key] !== after[key]) {
        changes.push([label, `${hex(before[key], digits)}→${hex(after[key], digits)}`]);
      }
    };
    reg("A", "a", 2);
    reg("B", "b", 2);
    reg("PC", "pc", 4);
    reg("SP", "sp", 4);
    for (const [label, key] of [["Z", "zero"], ["L", "less"], ["G", "greater"]]) {
      if (before[key] !== after[key]) {
        changes.push([label, `${before[key] ? 1 : 0}→${after[key] ? 1 : 0}`]);
      }
    }
    return changes;
  }

  function renderTrace(trace, totalSteps) {
    if (!trace.length) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = "Instruction history will appear here.";
      el.traceList.replaceChildren(empty);
      el.traceCount.textContent = "0 instructions";
      return;
    }

    const fragment = document.createDocumentFragment();
    for (const entry of trace) {
      const row = document.createElement("div");
      row.className = "trace-entry";

      const index = document.createElement("span");
      index.className = "index";
      index.textContent = String(entry.index);

      const instruction = document.createElement("span");
      instruction.className = "instruction";
      instruction.textContent = formatInstruction(entry.name, entry.operand);

      const changes = document.createElement("span");
      changes.className = "changes";
      const diff = describeChanges(entry.before, entry.after);
      if (!diff.length) {
        changes.textContent = "no register changes";
      }
      diff.forEach(([label, text], i) => {
        if (i > 0) changes.append(" · ");
        const name = document.createElement("b");
        name.textContent = label;
        changes.append(name, ` ${text}`);
      });
      instruction.appendChild(changes);

      const address = document.createElement("span");
      address.className = "address";
      address.textContent = `@${hex(entry.address, 2)}`;

      row.append(index, instruction, address);
      fragment.appendChild(row);
    }
    el.traceList.replaceChildren(fragment);
    el.traceList.scrollTop = el.traceList.scrollHeight;

    const noun = totalSteps === 1 ? "instruction" : "instructions";
    el.traceCount.textContent =
      trace.length < totalSteps
        ? `${totalSteps} ${noun} (last ${trace.length} shown)`
        : `${totalSteps} ${noun}`;
  }

  function renderMemory(s) {
    const { code_size: codeSize, stack_limit: stackLimit } = s.layout;
    const sp = s.registers.sp;
    const pcActive = s.next_instruction !== null;

    for (let address = 0; address < MEMORY_SIZE; address++) {
      const cell = memoryCells[address];
      const value = s.memory[address];
      let region = "data";
      if (address >= stackLimit) region = "stack";
      else if (address < codeSize) region = "code";

      cell.textContent = hex(value, 2);
      cell.className = `memory-cell ${region}`;
      if (region === "stack" && address >= sp) cell.classList.add("used");
      if (pcActive && address === s.registers.pc) cell.classList.add("active");
      cell.title = `Address 0x${hex(address, 2)} (${address}) · ${region}\n` +
        `Value 0x${hex(value, 2)} (${value})`;
    }
  }

  function render(s) {
    state = s;

    el.regA.textContent = hex(s.registers.a, 2);
    el.regADec.textContent = `Decimal: ${s.registers.a}`;
    el.regB.textContent = hex(s.registers.b, 2);
    el.regBDec.textContent = `Decimal: ${s.registers.b}`;
    el.pc.textContent = hex(s.registers.pc, 4);
    el.sp.textContent = hex(s.registers.sp, 4);

    setFlag(el.flagZero, s.flags.zero);
    setFlag(el.flagLess, s.flags.less);
    setFlag(el.flagGreater, s.flags.greater);

    el.cpuStatus.textContent = s.status;
    el.cpuStatus.className = `status-${s.status.toLowerCase()}`;

    const next = s.next_instruction;
    el.cpuNext.textContent = next
      ? `@${hex(next.address, 2)} ${formatInstruction(next.name, next.operand)}`
      : "—";
    el.cpuResult.textContent = s.result ? s.result.text : "—";

    renderMemory(s);
    renderTrace(s.trace, s.steps);
    updateButtons();
  }

  // ------------------------------------------------------------------
  // Actions
  // ------------------------------------------------------------------

  // Loads the program described by the controls. Returns true on success.
  function loadProgram() {
    const controls = readControls();
    if (controls.error) {
      showMessage(controls.error);
      updateButtons();
      return Promise.resolve(false);
    }

    const promise = Promise.resolve()
      .then(() => cpuApi.load(controls.request))
      .then((data) => {
        clearMessage();
        render(data.state);
        return true;
      })
      .catch((error) => {
        showMessage(error.message);
        if (error.state) render(error.state);
        return false;
      })
      .finally(() => {
        if (pendingLoad === promise) pendingLoad = null;
        updateButtons();
      });

    pendingLoad = promise;
    return promise;
  }

  // Makes sure the CPU runs the program shown in the controls.
  async function ensureLoaded() {
    if (pendingLoad) await pendingLoad;
    const controls = readControls();
    if (controls.error) {
      showMessage(controls.error);
      return false;
    }
    if (controlsDiffer(controls.request)) return loadProgram();
    return true;
  }

  async function runAction(work) {
    if (busy) return;
    clearTimeout(inputTimer);
    busy = true;
    updateButtons();
    try {
      await work();
    } catch (error) {
      showMessage(error.message);
      if (error.state) render(error.state);
    } finally {
      busy = false;
      updateButtons();
    }
  }

  function onStep() {
    runAction(async () => {
      if (!(await ensureLoaded())) return;
      const data = cpuApi.step();
      clearMessage();
      render(data.state);
    });
  }

  function onRun() {
    runAction(async () => {
      if (!(await ensureLoaded())) return;
      const data = cpuApi.run();
      clearMessage();
      render(data.state);
    });
  }

  function onReset() {
    runAction(async () => {
      if (pendingLoad) await pendingLoad;
      const data = cpuApi.reset();
      clearMessage();
      render(data.state);
    });
  }

  function onProgramChange() {
    clearTimeout(inputTimer);
    updateInputB();
    renderTrace([], 0); // old trace no longer applies
    loadProgram();
  }

  function onInputChange() {
    clearTimeout(inputTimer);
    const controls = readControls();
    updateButtons();
    if (controls.error) {
      showMessage(controls.error);
      return;
    }
    clearMessage();
    if (controlsDiffer(controls.request)) {
      inputTimer = setTimeout(loadProgram, INPUT_DEBOUNCE_MS);
    }
  }

  // ------------------------------------------------------------------
  // Startup
  // ------------------------------------------------------------------

  async function init() {
    buildMemoryGrid();
    busy = true;
    updateButtons();

    try {
      cpu = await loadCpuModule();
      setEngineStatus("WASM CPU", true);

      programs = new Map(cpuApi.programs().programs.map((p) => [p.id, p]));
      updateInputB();
      render(cpuApi.state().state);
      busy = false;
      await loadProgram();
    } catch (error) {
      setEngineStatus("WASM UNAVAILABLE", false);
      showMessage(error.message);
    } finally {
      busy = false;
      updateButtons();
    }
  }

  el.program.addEventListener("change", onProgramChange);
  el.inputA.addEventListener("input", onInputChange);
  el.inputB.addEventListener("input", onInputChange);
  el.runBtn.addEventListener("click", onRun);
  el.stepBtn.addEventListener("click", onStep);
  el.resetBtn.addEventListener("click", onReset);

  init();
})();
