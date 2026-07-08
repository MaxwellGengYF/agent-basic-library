#!/usr/bin/env python3
"""Build a standalone distribution of the agent-basic-library Python bindings.

Steps:
1. Configure and build the unified ``agent_core`` shared library in release mode.
2. Copy the shared library and the per-feature shared libraries into ``dist/``.
3. Copy the Python wrapper and tests into ``dist/``.
4. Run the Python test suite from inside ``dist/`` to verify the bundle.

Usage:
    python dist.py

Environment variables:
    XMAKE           Path to the xmake executable (default: "xmake").
    AGENT_DIST_DIR  Output directory (default: "dist" relative to this script).
"""

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def _lib_names() -> list[str]:
    """Return the platform-specific shared library basenames to distribute."""
    system = platform.system()
    if system == "Windows":
        suffix = ".dll"
    elif system == "Darwin":
        suffix = ".dylib"
    else:
        suffix = ".so"

    names = ["agent_core", "message_sanitization", "prompt_builder",
             "conversation_loop", "context_compressor"]
    if suffix == ".so" or suffix == ".dylib":
        return [f"lib{name}{suffix}" for name in names]
    return [f"{name}{suffix}" for name in names]


def _find_build_dir(project_root: Path) -> Path:
    """Locate the xmake output directory for the current release build."""
    system = platform.system().lower()
    arch = platform.machine().lower()
    if arch in ("amd64", "x86_64"):
        arch = "x64"
    elif arch in ("arm64", "aarch64"):
        arch = "arm64"

    candidate = project_root / "build" / system / arch / "release"
    if candidate.is_dir():
        return candidate

    # Fallbacks for simpler layouts.
    for layout in (
        project_root / "build" / "release",
        project_root / "build" / system / "release",
        project_root / "build",
    ):
        if layout.is_dir():
            return layout

    raise RuntimeError(f"Could not find xmake build directory under {project_root / 'build'}")


def _run(cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    """Run a command, streaming output, and raise on failure."""
    print(f"$ {' '.join(cmd)}", flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def main() -> int:
    project_root = Path(__file__).resolve().parent
    dist_dir = Path(os.environ.get("AGENT_DIST_DIR", project_root / "dist")).resolve()
    xmake = os.environ.get("XMAKE", "xmake")

    print(f"Project root: {project_root}", flush=True)
    print(f"Distribution dir: {dist_dir}", flush=True)

    # 1. Build release agent_core (and its dependencies).
    print("\n[1/4] Configuring release build ...", flush=True)
    _run([xmake, "f", "-m", "release", "-c", "-y"], cwd=project_root)

    print("\n[2/4] Building agent_core ...", flush=True)
    _run([xmake, "build", "agent_core"], cwd=project_root)

    build_dir = _find_build_dir(project_root)
    print(f"Build directory: {build_dir}", flush=True)

    # 3. Assemble dist/ layout.
    print("\n[3/4] Assembling distribution ...", flush=True)
    if dist_dir.exists():
        shutil.rmtree(dist_dir)
    dist_dir.mkdir(parents=True)

    # Copy shared libraries.
    copied_libs: list[Path] = []
    for name in _lib_names():
        src = build_dir / name
        if src.is_file():
            dst = dist_dir / name
            shutil.copy2(src, dst)
            copied_libs.append(dst)
            print(f"  copied {dst.name}", flush=True)

    if not copied_libs:
        raise RuntimeError(f"No shared libraries found in {build_dir}")

    # Copy Python wrapper.
    scripts_dir = project_root / "scripts"
    shutil.copy2(scripts_dir / "agent_native.py", dist_dir / "agent_native.py")
    shutil.copy2(scripts_dir / "__init__.py", dist_dir / "__init__.py")
    print("  copied agent_native.py", flush=True)
    print("  copied __init__.py", flush=True)

    # Copy tests.
    tests_src = scripts_dir / "tests"
    tests_dst = dist_dir / "tests"
    shutil.copytree(tests_src, tests_dst)
    print("  copied tests/", flush=True)

    # 4. Verify by running the test suite from dist/.
    print("\n[4/4] Verifying distribution ...", flush=True)
    env = os.environ.copy()
    # Ensure the copied library is found even if discovery heuristics fail.
    env["AGENT_NATIVE_LIB_PATH"] = str(copied_libs[0])

    result = subprocess.run(
        [sys.executable, "-m", "pytest", "tests/test_agent_native.py", "-v"],
        cwd=dist_dir,
        env=env,
    )

    # Remove generated caches from the distribution to keep it clean.
    for cache in (dist_dir / ".pytest_cache", dist_dir / "__pycache__"):
        if cache.exists():
            shutil.rmtree(cache)
    for pycache in dist_dir.rglob("__pycache__"):
        if pycache.is_dir():
            shutil.rmtree(pycache)

    if result.returncode != 0:
        print("\nVerification failed.", flush=True)
        return result.returncode

    print(f"\nDistribution ready at: {dist_dir}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
