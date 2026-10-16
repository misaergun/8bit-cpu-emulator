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
    exampleSelect: document.getElementById("example-select"),
    loadAsmBtn: document.getElementById("load-asm-btn"),
    asmSource: document.getElementById("asm-source"),
    editorLines: document.getElementById("editor-lines"),
    editorHighlight: document.getElementById("editor-highlight"),
    asmStatus: document.getElementById("asm-status"),
    asmErrors: document.getElementById("asm-errors"),
    modeBuiltin: document.getElementById("mode-builtin"),
    modeCustom: document.getElementById("mode-custom"),
    builtinPanel: document.getElementById("builtin-panel"),
    editorPanel: document.getElementById("editor-panel"),
    builtinActions: document.getElementById("builtin-actions"),
    editorActions: document.getElementById("editor-actions"),
    runControls: document.getElementById("run-controls"),
    programDescription: document.getElementById("program-description"),
    cpuProgram: document.getElementById("cpu-program"),
    cpuSteps: document.getElementById("cpu-steps"),
    cpuLast: document.getElementById("cpu-last"),
  };

  let programs = new Map(); // id -> { id, name, needs_b }
  let cpu = null; // WebAssembly module from generated/cpu.js
  let cpuLog = ""; // last diagnostic printed by the WebAssembly runtime
  let state = null; // last state reported by the CPU
  let busy = false; // a Run/Step/Reset action is in progress
  let pendingLoad = null; // promise of an in-flight program load
  let inputTimer = null;
  const memoryCells = [];

  // Assembly editor
  const CUSTOM = "custom"; // program id of code loaded from the editor
  let assembly = null; // { source, size, warnings, lines: Map address -> line }
  let asmErrors = []; // errors from the last failed Load Program
  let editorEdited = false; // the user typed since the last example was inserted
  let lastBuiltin = "comparison"; // built-in program to return to from Custom mode

  // ------------------------------------------------------------------
  // Helpers
  // ------------------------------------------------------------------

  const hex = (value, digits) =>
    Number(value).toString(16).toUpperCase().padStart(digits, "0");

  class ApiError extends Error {
    constructor(message, apiState, data) {
      super(message);
      this.state = apiState || null;
      this.data = data || null; // full response, e.g. assembly errors
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
      throw new ApiError((data && data.error) || `${name} failed.`, data && data.state, data);
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
    // Assembles source code and loads it. Fails without changing the CPU.
    loadAssembly: (source) => callCpu("cpu_load_assembly", ["string"], [source]),
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
    if (el.program.value === CUSTOM) {
      // Custom code sets its registers itself; Input A and B are not used.
      el.inputA.classList.remove("invalid");
      el.inputB.classList.remove("invalid");
      return { request: { program: CUSTOM } };
    }

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
    if (request.program === CUSTOM) return loaded.id !== CUSTOM;
    return (
      loaded.id !== request.program ||
      loaded.a !== request.a ||
      (loaded.needs_b && loaded.b !== request.b)
    );
  }

  function updateInputB() {
    const custom = el.program.value === CUSTOM;
    const program = selectedProgram();
    const needsB = !custom && (program ? program.needs_b : true);
    el.inputA.disabled = custom;
    el.inputB.disabled = !needsB;
    el.inputBGroup.title = custom
      ? "Custom programs set their registers in code."
      : needsB ? "" : "This program only uses Input A.";
    renderMode();
  }

  // The program menu decides the mode: "custom" shows the Assembly Editor,
  // any other value shows the built-in program panel.
  function renderMode() {
    const custom = el.program.value === CUSTOM;
    if (!custom) lastBuiltin = el.program.value;

    el.builtinPanel.hidden = custom;
    el.editorPanel.hidden = !custom;
    el.modeBuiltin.setAttribute("aria-selected", String(!custom));
    el.modeCustom.setAttribute("aria-selected", String(custom));

    // There is one set of Run/Step/Reset buttons; show it in the active panel.
    const actions = custom ? el.editorActions : el.builtinActions;
    if (el.runControls.parentElement !== actions) actions.appendChild(el.runControls);

    const option = el.program.selectedOptions[0];
    el.programDescription.textContent = (option && option.dataset.description) || "";
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
    const custom = el.program.value === CUSTOM;
    el.program.disabled = busy;
    el.inputA.disabled = busy || custom;
    el.inputB.disabled = busy || custom || !(selectedProgram() || { needs_b: true }).needs_b;
    el.loadAsmBtn.disabled = busy || !cpu;
    el.exampleSelect.disabled = busy;
    el.modeBuiltin.disabled = busy;
    el.modeCustom.disabled = busy;
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

  // ------------------------------------------------------------------
  // Assembly editor rendering
  // ------------------------------------------------------------------

  // True when the editor still shows the code that is loaded in the CPU.
  function editorMatchesCpu() {
    return Boolean(assembly && state && state.program &&
      state.program.id === CUSTOM && el.asmSource.value === assembly.source);
  }

  // The source line to highlight, from the CPU state and the source map:
  // the next instruction while running, the last one after HALT, or the
  // failing one after a CPU error. Returns null when it cannot be known.
  function highlightedLine() {
    if (!editorMatchesCpu()) return null;

    if (state.next_instruction) {
      const line = assembly.lines.get(state.next_instruction.address);
      return line ? { line, kind: "current" } : null;
    }

    let address = null;
    const fault = state.error && state.error.match(/CPU fault at address 0x([0-9A-F]{2})/);
    if (fault) {
      address = parseInt(fault[1], 16);
    } else if (state.trace.length) {
      address = state.trace[state.trace.length - 1].address;
    }
    const line = address === null ? undefined : assembly.lines.get(address);
    if (!line) return null;
    return { line, kind: state.status === "ERROR" ? "error" : "halted" };
  }

  function editorMetrics() {
    const style = getComputedStyle(el.asmSource);
    return {
      lineHeight: parseFloat(style.lineHeight) || 20,
      paddingTop: parseFloat(style.paddingTop) || 0,
    };
  }

  // Keeps the line numbers and the highlight aligned with the textarea.
  function syncEditorScroll() {
    el.editorLines.style.transform = `translateY(${-el.asmSource.scrollTop}px)`;
    const highlight = highlightedLine();
    el.editorHighlight.hidden = !highlight;
    if (highlight) {
      const { lineHeight, paddingTop } = editorMetrics();
      el.editorHighlight.className = `editor-highlight ${highlight.kind}`;
      el.editorHighlight.style.top =
        `${paddingTop + (highlight.line - 1) * lineHeight - el.asmSource.scrollTop}px`;
    }
  }

  function renderLineNumbers() {
    const count = el.asmSource.value.split("\n").length;
    const errorLines = new Set(asmErrors.map((e) => e.line));
    const highlight = highlightedLine();
    const fragment = document.createDocumentFragment();
    for (let line = 1; line <= count; line++) {
      const number = document.createElement("div");
      number.textContent = String(line);
      if (errorLines.has(line)) number.className = "error";
      else if (highlight && highlight.line === line) number.className = "current";
      fragment.appendChild(number);
    }
    el.editorLines.replaceChildren(fragment);
  }

  function renderAsmStatus() {
    let text;
    let kind = "";
    if (asmErrors.length) {
      const count = asmErrors.length;
      text = `Assembly failed with ${count} ${count === 1 ? "error" : "errors"}. ` +
        "The previously loaded program is unchanged.";
      kind = "error";
    } else if (!assembly) {
      text = "Not loaded yet. Click Load Program to assemble this code and load it into the CPU.";
    } else if (!state || !state.program || state.program.id !== CUSTOM) {
      const name = state && state.program ? state.program.name : "another program";
      text = `The CPU is running the built-in program "${name}". ` +
        "Click Load Program to run the code in the editor.";
    } else if (el.asmSource.value !== assembly.source) {
      text = "Edited since loading. Click Load Program to assemble the changes.";
      kind = "warning";
    } else {
      const last = hex(assembly.size - 1, 2);
      text = `Loaded ${assembly.size} ${assembly.size === 1 ? "byte" : "bytes"} ` +
        `at 0x00–0x${last}.`;
      kind = "ok";
      if (assembly.warnings.length) {
        text += ` Warning: ${assembly.warnings.map((w) => w.message).join(" ")}`;
        kind = "warning";
      }
    }
    el.asmStatus.textContent = text;
    el.asmStatus.className = `asm-status ${kind}`;

    el.asmErrors.hidden = asmErrors.length === 0;
    const fragment = document.createDocumentFragment();
    for (const error of asmErrors) {
      const item = document.createElement("li");
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = error.line > 0
        ? `Line ${error.line}: ${error.message}`
        : error.message;
      if (error.line > 0) {
        button.title = "Go to this line";
        button.addEventListener("click", () => focusLine(error.line));
      } else {
        button.disabled = true;
      }
      item.appendChild(button);
      fragment.appendChild(item);
    }
    el.asmErrors.replaceChildren(fragment);
  }

  function renderEditor() {
    renderLineNumbers();
    renderAsmStatus();
    syncEditorScroll();
  }

  // Selects a source line and scrolls it into view.
  function focusLine(line) {
    const lines = el.asmSource.value.split("\n");
    let start = 0;
    for (let i = 0; i < line - 1 && i < lines.length; i++) start += lines[i].length + 1;
    const end = start + (lines[line - 1] || "").length;
    el.asmSource.focus();
    el.asmSource.setSelectionRange(start, end);
    const { lineHeight } = editorMetrics();
    el.asmSource.scrollTop = Math.max(0, (line - 1) * lineHeight - el.asmSource.clientHeight / 3);
    syncEditorScroll();
  }

  function describeProgram(program) {
    if (!program) return "None";
    if (program.id === CUSTOM) return `Custom code (${program.size} bytes)`;
    return program.needs_b
      ? `${program.name} (A = ${program.a}, B = ${program.b})`
      : `${program.name} (A = ${program.a})`;
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
    el.cpuStatus.className = `status-pill status-${s.status.toLowerCase()}`;

    el.cpuProgram.textContent = describeProgram(s.program);
    el.cpuSteps.textContent = String(s.steps);
    const last = s.trace.length ? s.trace[s.trace.length - 1] : null;
    el.cpuLast.textContent = last
      ? `@${hex(last.address, 2)} ${formatInstruction(last.name, last.operand)}`
      : "—";

    const next = s.next_instruction;
    el.cpuNext.textContent = next
      ? `@${hex(next.address, 2)} ${formatInstruction(next.name, next.operand)}`
      : "—";
    el.cpuResult.textContent = s.result ? s.result.text : "—";

    renderMemory(s);
    renderTrace(s.trace, s.steps);
    renderEditor();
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

    if (controls.request.program === CUSTOM) {
      return Promise.resolve(loadAssembly());
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

  // Assembles the editor's code and loads it into the CPU. If assembly fails,
  // the errors are shown and the loaded program stays as it was.
  function loadAssembly() {
    const source = el.asmSource.value;
    try {
      const data = cpuApi.loadAssembly(source);
      const lines = new Map();
      for (const entry of data.assembly.source_map) {
        if (entry.instruction) lines.set(entry.address, entry.line);
      }
      assembly = { source, size: data.assembly.size, warnings: data.assembly.warnings, lines };
      asmErrors = [];
      el.program.value = CUSTOM;
      updateInputB();
      clearMessage();
      render(data.state);
      return true;
    } catch (error) {
      const details = error.data && error.data.assembly;
      asmErrors = details ? details.errors : [{ line: 0, message: error.message }];
      // Stay in Custom mode so the errors can be fixed. The CPU keeps the
      // program it had, which the CPU State panel shows.
      updateInputB();
      renderEditor();
      updateButtons();
      return false;
    }
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
    if (el.program.value === CUSTOM) {
      onLoadAssembly();
      return;
    }
    renderTrace([], 0); // old trace no longer applies
    loadProgram();
  }

  function onModeTab(custom) {
    const target = custom ? CUSTOM : lastBuiltin;
    if (busy || el.program.value === target) return;
    el.program.value = target;
    onProgramChange();
  }

  function onLoadAssembly() {
    runAction(async () => {
      if (pendingLoad) await pendingLoad;
      loadAssembly();
    });
  }

  function onEditorInput() {
    editorEdited = true;
    renderEditor();
  }

  function onExampleChange() {
    const example = (typeof ASM_EXAMPLES === "undefined" ? [] : ASM_EXAMPLES)
      .find((e) => e.id === el.exampleSelect.value);
    el.exampleSelect.value = "";
    if (!example) return;
    if (editorEdited && el.asmSource.value.trim() !== "" &&
        !window.confirm(`Replace the code in the editor with "${example.name}"?`)) {
      return;
    }
    el.asmSource.value = example.source;
    el.asmSource.scrollTop = 0;
    editorEdited = false;
    asmErrors = [];
    renderEditor();
  }

  function setUpEditor() {
    const examples = typeof ASM_EXAMPLES === "undefined" ? [] : ASM_EXAMPLES;
    for (const example of examples) {
      const option = document.createElement("option");
      option.value = example.id;
      option.textContent = example.name;
      el.exampleSelect.appendChild(option);
    }
    if (examples.length && !el.asmSource.value) el.asmSource.value = examples[0].source;
    renderEditor();
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
    setUpEditor();
    renderMode();
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
  el.loadAsmBtn.addEventListener("click", onLoadAssembly);
  el.modeBuiltin.addEventListener("click", () => onModeTab(false));
  el.modeCustom.addEventListener("click", () => onModeTab(true));
  el.exampleSelect.addEventListener("change", onExampleChange);
  el.asmSource.addEventListener("input", onEditorInput);
  el.asmSource.addEventListener("scroll", syncEditorScroll);

  init();
})();
