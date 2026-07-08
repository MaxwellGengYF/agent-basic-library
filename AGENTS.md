# Agent Guide

This document contains essential rules for AI agents working in this repository.

## Project Overview

This is `agent-basic-library`, a C11 implementation of core agent hot-path utilities. The project is built with [XMake](https://xmake.io) and organized as follows:

- `src/` — Main source tree.
  - `src/message_sanitization/` — JSON message sanitization utilities (shared library + public headers).
  - `src/prompt_builder/` — Prompt construction helpers (shared library + public headers).
  - `src/conversation_loop/` — Conversation message sanitization/repair (shared library + public headers).
  - `src/context_compressor/` — Context compression / pruning helpers (shared library + public headers).
  - `src/include/` — Shared internal headers (e.g. `agent_compat.h`, `agent_config.h`).
  - `src/ext/` — Bundled third-party dependencies (`yyjson`, `xxhash`, `mimalloc`, `glib`).
  - `src/tests/` — Lightweight C test framework and per-feature test executables.
  - `src/xmake.lua` — Source-level target definitions.
- `scripts/` — Reserved for future build automation / helper scripts (currently empty).
- `xmake.lua` — Root build configuration.

All feature targets, the demo binary, and tests are configured to compile against the **C11 language standard** (`set_languages("c11")`).

## Agent Rules

1. **Read the XMake skill before configuring, building, or running.**
   - Always consult `.agents/skills/xmake/SKILL.md` before invoking any `xmake` commands.
   - Prefer the documented quick-start flow:
     ```bash
     xmake f -m debug -c -y
     xmake build
     xmake run <target>
     ```
   - Use `-y` for non-interactive / scripted runs.
   - Debug mode automatically enables AddressSanitizer (ASan) via `build.sanitizer.address`.

2. **Use C11.**
   - All C source files in this project must remain compatible with the **C11** standard.
   - Do not introduce C23-only, GCC-only, or platform-specific extensions unless explicitly requested and validated.
   - Keep source comments ASCII-only to avoid MSVC code-page warnings.

3. **Verify with tests.**
   - After any source change, build and run the registered tests:
     ```bash
     xmake test
     ```
   - Individual test binaries can also be executed via `xmake run <test_target>`.

4. **Respect MSVC compatibility.**
   - Avoid `__attribute__((constructor))` and other compiler-specific extensions not supported by MSVC.
   - The build already defines `_CRT_NONSTDC_NO_DEPRECATE` and `_CRT_SECURE_NO_WARNINGS` on Windows; do not redefine them per-target.

5. **Ensure cross-platform portability.**
   - All code must be cross-platform and build cleanly on **Windows**, **macOS**, and **Linux**.
   - Do not rely on platform-specific headers, APIs, file-path separators, or compiler extensions without appropriate `#ifdef` guards and fallbacks.
   - Use standard C library facilities where possible, and abstract any platform-dependent behavior behind compatible helpers.
   - Prefer forward slashes (`/`) for paths in source strings; when interacting with the native filesystem, convert paths appropriately for the target platform.
