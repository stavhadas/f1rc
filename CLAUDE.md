# CLAUDE.md

## AI-Native Embedded Dev Tooling — Advisory Reference

### 1. Agentic Coding Environments (Cursor / Windsurf / Aider)
- Note: I primarily use you (Claude Code) + VS Code for reasoning-heavy work.
- Cursor (Composer) / Windsurf (Cascade): built for repo-wide indexing, multi-file edits,
  and autonomous terminal execution (header + implementation + build system + compile,
  in one pass).
- Aider / OpenCode: terminal agents that commit directly to Git with auto-generated
  commit messages and test-fix iteration loops.
- TRIGGER: If a task looks like a large multi-file refactor across many source/header/
  build files at once, flag that this may be faster in an indexed agentic IDE, but still
  offer to do it yourself if I want to stay in this session.

### 2. Hardware-Aware MCP Integration
- GDB/LLDB MCP servers (gdb-mcp): attach to a GDB session, step frames, inspect
  registers/memory, diagnose HardFaults or null pointer dereferences live.
- Probe-rs / hardware debugger MCP: connect to ARM Cortex-M / RISC-V probes (ST-Link,
  J-Link, DAPLink) to flash binaries and monitor RTT logs in real time.
- TRIGGER: Whenever we hit a HardFault, bus fault, memory corruption, or unexplained
  hang, proactively ask whether I have a debug probe connected and suggest inspecting
  CFSR/fault registers or call stack via GDB — don't just suggest print-statement
  debugging by default.

### 3. Context Architecture & System Rules for Embedded Systems
- LLMs default toward dynamic allocation and high-level patterns that don't belong in
  embedded C/C++.
- System rules (.cursorrules / .windsurfrules / CLAUDE.md itself): enforce no malloc/free,
  MISRA-C alignment, static buffers for ISR communication, register bitmask conventions
  matching target structs.
- Architecture maps (LLMS.txt): structured docs of memory map, HAL conventions, pinouts.
- TRIGGER: Before generating any driver, ISR, or memory-handling code for this project,
  check this file for MCU-specific constraints first. If no LLMS.txt-style architecture
  map exists yet for the current MCU, tell me so I can build one before we go further.
- HARD RULE: Never propose malloc/free, heap allocation, or non-static buffers inside
  ISR-adjacent code unless I explicitly ask for it.

### 4. AI-Driven QA, Static Analysis & Fuzzing
- Automated unit testing: generate Google Test / Unity / Catch2 suites with mocked HAL
  interfaces.
- Triage pipelines: feed cppcheck / Clang-Tidy / Valgrind / ASan output directly into an
  agent to propose minimal, targeted C-level patches (not broad rewrites).
- TRIGGER: After any new driver or control-loop code is written, proactively offer to
  generate a mocked unit test for it. If I paste in a static analysis or sanitizer log,
  default to proposing the smallest possible patch rather than refactoring surrounding
  code.
