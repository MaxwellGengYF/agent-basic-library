---
name: xmake
description: XMake build configuration, options, commands, and patterns for LuisaCompute.
---

# XMake Build System

Primary build system. Requires XMake 3.0.6+. Optional: CUDA Toolkit, Vulkan SDK, LLVM 20, Rust.

## Quick Start

```bash
xmake f -m debug -c -y
xmake build
# Update compile_commands.json:
xmake project -k compile_commands --lsp=clangd .vscode
```

## Configuration

| Platform | Command |
|---|---|
| Linux GCC | `xmake f -p linux -a x86_64 --toolchain=gcc -m release -c` |
| Linux Clang | `xmake f -p linux -a x86_64 --toolchain=clang -m release -c` |
| Windows MSVC | `xmake f -p windows -a x64 --toolchain=msvc -m release -c` |
| Windows Clang-CL | `xmake f -p windows -a x64 --toolchain=clang-cl -m release -c` |
| Windows LLVM | `xmake f -p windows -a x64 --toolchain=llvm -m release -c` |
| macOS Clang | `xmake f -p macosx -a arm64 --toolchain=clang -m release -c` |

### Flags
`-c` clean cache, `-m <mode>` (release/debug/releasedbg), `-p <plat>` (linux/windows/macosx), `-a <arch>` (x86_64/x64/arm64), `--check` check before building, `-y` auto-accept all prompts and skip interaction (useful in scripts/CI).

## Commands

| Command | Description |
|---|---|
| `xmake clean` | Clean |
| `xmake -r` | Rebuild |
| `xmake build <target>` | Build target |
| `xmake run <target>` | Run target |
| `xmake run <target> <args>` | Run target with arguments |
| `xmake -l` | List targets |
| `xmake install -o <dir>` | Install binaries to `<dir>` |
| `xmake -y` | Auto-accept all prompts (downloads, overwrites, etc.), skip interaction |
| `xmake project -k compile_commands --lsp=clangd .vscode` | Generate `compile_commands.json` |

## Common Issues

- `-v`, `-D`, `--diagnosis` invalid; use `--verbose`
- Boolean options: `--lc_option=true`/`=false`
- Use `-c` to clean cache when reconfiguring with different options
- Use `-y` to auto-accept all prompts and skip interaction — essential in automated scripts and CI pipelines
- `lc_fallback_backend` requires both `lc_llvm_path` and `lc_embree_path`
- `lc_dx_backend` is silently disabled on non-Windows platforms
- `lc_metal_backend` is silently disabled on non-macOS platforms
- `lc_cuda_backend` is silently disabled outside Windows/Linux
- Vulkan compute shader codegen options are mutually exclusive: keep `lc_vk_backend_use_xir_spirv=true` for the default native SPIR-V path, or set it to `false` before enabling `lc_vk_backend_use_ast_llvm_spirv=true`.

## Tutorial

For a comprehensive guide on writing xmake targets, Lua scripting in xmake, and advanced patterns, see the **[xmake_tutorial.md](./xmake_tutorial.md)** file in this directory.