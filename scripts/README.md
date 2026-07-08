# Python FFI Bindings for agent-basic-library

This directory contains cross-platform Python bindings for the
`agent-basic-library` native shared library.

## Files

- `agent_native.py` — ctypes-based wrapper exposing all 16 public C functions.
- `tests/test_agent_native.py` — pytest-compatible test suite.

## Building the native library

The xmake target `agent_core` builds a single shared library (`agent_core.dll` /
`libagent_core.so` / `libagent_core.dylib`) that bundles all four feature
libraries:

```bash
xmake f -m release -c -y
xmake build agent_core
```

## Running the Python tests

```bash
cd scripts
python -m pytest tests/test_agent_native.py -v
```

## Library discovery

`agent_native.py` locates the shared library automatically from common xmake
output directories, preferring release builds. You can override discovery with
environment variables:

- `AGENT_NATIVE_LIB_PATH` — full path to the shared library file.
- `AGENT_NATIVE_BUILD_DIR` — directory containing the shared library file.
