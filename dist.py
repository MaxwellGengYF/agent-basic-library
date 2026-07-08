#!/usr/bin/env python3
"""Build a standalone distribution of the agent-basic-library Python bindings.

Steps:
1. Configure and build the unified ``agent_core`` shared library.
2. Copy the shared library and Python wrapper into ``dist/`` (or to a
   custom deploy target).
3. Run tests to verify the bundle.

If ``xmake`` is not found on ``PATH`` (or via the ``XMAKE`` env var), the
script automatically downloads the portable xmake bundle for the current
platform from GitHub and caches it in ``.xmake-cache/``.

Usage:
    python dist.py [--mode release|debug] [--deploy-to PATH] [--skip-verify]

Environment variables:
    XMAKE           Path to the xmake executable (default: auto-detected).
    XMAKE_MODE      Build mode (``release`` or ``debug``).  CLI ``--mode`` wins.
    AGENT_DIST_DIR  Output directory for standalone assembly (default: ``dist/``).
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import stat
import subprocess
import sys
import urllib.request
from pathlib import Path


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

XMAKE_VERSION = "v3.0.9"
XMAKE_BASE_URL = f"https://github.com/xmake-io/xmake/releases/download/{XMAKE_VERSION}"

THIS_DIR = Path(__file__).resolve().parent


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _lib_name() -> str:
    """Return the platform-specific shared-library basename for agent_core."""
    system = platform.system()
    if system == "Windows":
        return "agent_core.dll"
    if system == "Darwin":
        return "libagent_core.dylib"
    return "libagent_core.so"


def _all_lib_names() -> list[str]:
    """Return platform-specific shared library basenames to distribute."""
    system = platform.system()
    if system == "Windows":
        suffix = ".dll"
    elif system == "Darwin":
        suffix = ".dylib"
    else:
        suffix = ".so"

    names = [
        "agent_core", "message_sanitization", "prompt_builder",
        "conversation_loop", "context_compressor",
    ]
    if suffix in (".so", ".dylib"):
        return [f"lib{name}{suffix}" for name in names]
    return [f"{name}{suffix}" for name in names]


def _find_build_dir(build_root: Path, mode: str) -> Path:
    """Locate the xmake output directory for the given build mode."""
    system = platform.system().lower()
    arch = platform.machine().lower()
    if arch in ("amd64", "x86_64"):
        arch = "x64"
    elif arch in ("arm64", "aarch64"):
        arch = "arm64"

    candidate = build_root / system / arch / mode
    if candidate.is_dir():
        return candidate

    for layout in (
        build_root / mode,
        build_root / system / mode,
        build_root,
    ):
        if layout.is_dir():
            return layout

    raise RuntimeError(
        f"Could not find xmake build directory under {build_root}\n"
        f"Checked: {build_root / system / arch / mode}, "
        f"{build_root / mode}, {build_root / system / mode}, {build_root}"
    )


def _xmake_bundle_name() -> str:
    """Return the xmake bundle filename for the current platform."""
    machine = platform.machine().lower()
    if machine in ("amd64", "x86_64"):
        arch = "x86_64"
    elif machine in ("arm64", "aarch64"):
        arch = "arm64"
    else:
        arch = machine

    system = platform.system()
    if system == "Windows":
        win_arch = "win64" if "64" in arch else "win32"
        return f"xmake-bundle-{XMAKE_VERSION}.{win_arch}.exe"
    elif system == "Darwin":
        return f"xmake-bundle-{XMAKE_VERSION}.macos.{arch}"
    else:
        return f"xmake-bundle-{XMAKE_VERSION}.linux.{arch}"


def _download(url: str, dest: Path) -> None:
    """Download *url* to *dest*, showing a simple progress line."""
    print(f"  downloading {url} ...", flush=True)
    dest.parent.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve(url, dest)  # noqa: S310 — GitHub HTTPS is safe
    size = dest.stat().st_size
    print(f"  saved {size / 1024:.1f} KB to {dest}", flush=True)


def _resolve_xmake() -> str:
    """Resolve the xmake executable path.

    Priority:
    1. ``XMAKE`` environment variable.
    2. ``xmake`` on ``PATH`` (via ``shutil.which``).
    3. Download the portable bundle for the current platform.
    """
    # 1. Explicit env var.
    env_path = os.environ.get("XMAKE")
    if env_path:
        exe = shutil.which(env_path)
        if exe:
            print(f"Using xmake from XMAKE env: {exe}", flush=True)
            return exe
        if Path(env_path).is_file():
            print(f"Using xmake from XMAKE env: {env_path}", flush=True)
            return env_path

    # 2. PATH lookup.
    found = shutil.which("xmake")
    if found:
        print(f"Found xmake on PATH: {found}", flush=True)
        return found

    # 3. Download portable bundle.
    bundle_name = _xmake_bundle_name()
    bundle_url = f"{XMAKE_BASE_URL}/{bundle_name}"
    bundle_path = THIS_DIR / ".xmake-cache" / bundle_name

    if not bundle_path.is_file():
        print(f"xmake not found on PATH.  Downloading {bundle_name} ...", flush=True)
        (THIS_DIR / ".xmake-cache").mkdir(parents=True, exist_ok=True)
        _download(bundle_url, bundle_path)
        if platform.system() != "Windows":
            bundle_path.chmod(bundle_path.stat().st_mode | stat.S_IEXEC)

    print(f"Using xmake bundle: {bundle_path}", flush=True)
    return str(bundle_path)


def _run(cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    """Run a command, streaming output, and raise on failure."""
    print(f"$ {' '.join(cmd)}", flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode",
        default=os.environ.get("XMAKE_MODE", "release"),
        choices=("release", "debug"),
        help="Build mode (default: release)",
    )
    parser.add_argument(
        "--deploy-to",
        default=None,
        type=Path,
        help="Copy only the agent_core shared library to this directory "
             "(for Hermes integration into agent/_agent_core/).",
    )
    parser.add_argument(
        "--skip-verify",
        action="store_true",
        help="Skip the post-build test suite verification.",
    )
    args = parser.parse_args()
    mode = args.mode
    deploy_target: Path | None = args.deploy_to
    xmake = _resolve_xmake()

    project_root = THIS_DIR
    print(f"Project root: {project_root}", flush=True)
    print(f"Build mode: {mode}", flush=True)
    if deploy_target:
        print(f"Deploy target: {deploy_target}", flush=True)
    else:
        dist_dir = Path(os.environ.get("AGENT_DIST_DIR", project_root / "dist")).resolve()
        print(f"Distribution dir: {dist_dir}", flush=True)

    # 1. Configure and build agent_core.
    print("\n[1/3] Configuring build ...", flush=True)
    _run([xmake, "f", "-m", mode, "-c", "-y"], cwd=project_root)

    print("\n[2/3] Building agent_core ...", flush=True)
    _run([xmake, "build", "agent_core"], cwd=project_root)

    build_dir = _find_build_dir(project_root / "build", mode)
    print(f"Build directory: {build_dir}", flush=True)

    # 2. Assemble the distribution.
    print("\n[3/3] Assembling distribution ...", flush=True)

    if deploy_target:
        # --deploy-to mode: copy only agent_core library to the given directory.
        deploy_target = deploy_target.resolve()
        lib_name = _lib_name()
        src = build_dir / lib_name
        if not src.is_file():
            raise RuntimeError(
                f"Built library not found at expected path: {src}\n"
                f"Contents of {build_dir}: {list(build_dir.iterdir())}"
            )
        deploy_target.mkdir(parents=True, exist_ok=True)
        dst = deploy_target / lib_name
        shutil.copy2(src, dst)
        print(f"  deployed {lib_name} to {dst}", flush=True)

        # Verify library loads.
        print("\nVerifying library loads correctly ...", flush=True)
        env = os.environ.copy()
        env["AGENT_NATIVE_LIB_PATH"] = str(dst)

        result = subprocess.run(
            [
                sys.executable,
                "-c",
                (
                    "import sys; sys.path.insert(0, '.'); "
                    "from scripts.agent_native import load_library; "
                    "lib = load_library(); "
                    "print(f'Library loaded: {lib}')"
                ),
            ],
            cwd=project_root,
            env=env,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print("STDERR:", result.stderr, flush=True)
            print("STDOUT:", result.stdout, flush=True)
            print("\nLibrary verification FAILED.", flush=True)
            return result.returncode
        print(result.stdout.strip(), flush=True)

    else:
        # Standalone mode: assemble dist/ with all libraries + Python wrappers + tests.
        if dist_dir.exists():
            shutil.rmtree(dist_dir)
        dist_dir.mkdir(parents=True)

        # Copy all shared libraries.
        copied_libs: list[Path] = []
        for name in _all_lib_names():
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

        if not args.skip_verify:
            print("\n[4/4] Verifying distribution ...", flush=True)
            env = os.environ.copy()
            env["AGENT_NATIVE_LIB_PATH"] = str(copied_libs[0])

            result = subprocess.run(
                [sys.executable, "-m", "pytest", "tests/test_agent_native.py", "-v"],
                cwd=dist_dir,
                env=env,
            )

            # Clean up caches from dist/.
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