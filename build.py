#!/usr/bin/env python3
"""LinxCoreModel CMake build script."""

import argparse
import os
import platform
import shutil
import subprocess
import sys

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BUILD_DIR = os.path.join(PROJECT_ROOT, "build")

GCC12_SEARCH_PATHS = [
    "/software/public/gcc-12.2.0/bin",
    "/usr/bin",
    "/usr/local/bin",
    "/opt/homebrew/bin",
    "/home/linuxbrew/.linuxbrew/bin",
]


def run(cmd, cwd=None):
    print(f"==> {' '.join(cmd)}")
    try:
        subprocess.check_call(cmd, cwd=cwd or PROJECT_ROOT)
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except FileNotFoundError as e:
        print(f"[ERROR] Command not found: {e}", file=sys.stderr)
        sys.exit(1)


def ensure_safe_build_dir_removal(build_dir):
    abs_build_dir = os.path.abspath(build_dir)
    abs_project_root = os.path.abspath(PROJECT_ROOT)
    abs_default_build_dir = os.path.abspath(DEFAULT_BUILD_DIR)
    if (
        abs_build_dir == abs_project_root
        or abs_project_root.startswith(abs_build_dir + os.sep)
        or abs_build_dir == os.path.abspath(os.sep)
    ):
        print(
            f"[ERROR] Refusing to remove build directory '{build_dir}' as it conflicts with project root or system root.",
            file=sys.stderr,
        )
        sys.exit(1)

    is_default_build_dir = abs_build_dir == abs_default_build_dir
    has_cmake_artifacts = (
        os.path.exists(os.path.join(abs_build_dir, "CMakeCache.txt"))
        or os.path.exists(os.path.join(abs_build_dir, "CMakeFiles"))
    )
    if os.path.exists(abs_build_dir) and not (is_default_build_dir or has_cmake_artifacts):
        print(
            f"[ERROR] Refusing to remove '{build_dir}' as it does not appear to be a valid CMake build directory "
            "(missing CMakeCache.txt or CMakeFiles).",
            file=sys.stderr,
        )
        sys.exit(1)


def find_compiler(name, extra_paths=None):
    result = shutil.which(name)
    if result:
        return os.path.abspath(result)

    paths = list(GCC12_SEARCH_PATHS)
    if extra_paths:
        paths = extra_paths + paths
    for d in paths:
        p = os.path.join(d, name)
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return os.path.abspath(p)
    return None


def detect_system():
    s = platform.system()
    is_wsl = False
    if s == "Linux":
        try:
            with open("/proc/version", "r") as f:
                is_wsl = "microsoft" in f.read().lower()
        except OSError:
            pass
    if is_wsl:
        return "WSL"
    if s == "Darwin":
        return "macOS"
    return s


def check_macos_libelf(auto_yes=False):
    """Check if libelf is installed on macOS, prompt to install if missing."""
    if detect_system() != "macOS":
        return True

    # Check if brew is available\n    brew_path = shutil.which("brew")\n    if not brew_path:\n        for path in ["/opt/homebrew/bin/brew", "/usr/local/bin/brew"]:\n            if os.path.isfile(path) and os.access(path, os.X_OK):\n                brew_path = path\n                os.environ["PATH"] = os.path.dirname(path) + os.pathsep + os.environ.get("PATH", "")\n                break\n\n    if not brew_path:\n        print("[ERROR] Homebrew is not installed. Please install Homebrew first:")\n        print("  /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"")\n        return False

    # Check if libelf is installed
    result = subprocess.run(
        ["brew", "list", "libelf"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True
    )
    if result.returncode == 0:
        print("[deps] libelf is already installed")
        return True

    # libelf not found
    print("[deps] libelf is required on macOS but not installed")

    # Check if running in interactive terminal
    is_interactive = sys.stdin.isatty()

    if auto_yes:
        print("[deps] --yes flag provided, installing libelf...")
        should_install = True
    elif not is_interactive:
        # Non-interactive environment (agent, CI, etc.)
        print("[deps] Non-interactive environment detected.")
        print("[deps] Please install libelf manually: brew install libelf")
        print("[deps] Or run with --yes flag to auto-install")
        return False
    else:
        # Interactive terminal, prompt user
        try:
            response = input("Install libelf via Homebrew? [y/N]: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            response = "n"
        should_install = response in ("y", "yes")

    if not should_install:
        print("[deps] libelf installation skipped. Build may fail.")
        return False

    # Install libelf
    print("[deps] Installing libelf...")
    try:
        subprocess.check_call(["brew", "install", "libelf"])
        print("[deps] libelf installed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Failed to install libelf: {e}")
        return False


def get_compiler_version(compiler_path):
    """Get compiler version as tuple (major, minor, patch)."""
    try:
        result = subprocess.run(
            [compiler_path, "-dumpversion"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True
        )
        ver_str = result.stdout.strip()
        # Some compilers return only major version, some return full version
        parts = ver_str.split('.')
        # Pad to exactly 3 elements (major, minor, patch)
        ver_nums = []
        for part in parts[:3]:
            digits = []
            for char in part:
                if char.isdigit():
                    digits.append(char)
                else:
                    break
            ver_nums.append(int(''.join(digits)) if digits else 0)
        while len(ver_nums) < 3:
            ver_nums.append(0)
        return tuple(ver_nums[:3])
    except (subprocess.SubprocessError, OSError, ValueError):
        return (0, 0, 0)


def get_cmake_version():
    """Get CMake version as tuple (major, minor) and original version string."""
    if not shutil.which("cmake"):
        return (0, 0), "0.0"

    try:
        result = subprocess.run(
            ["cmake", "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            check=True
        )
        lines = result.stdout.splitlines()
        cmake_ver = lines[0].split()[-1] if lines else "0.0"
        cmake_ver_clean = cmake_ver.split('-')[0].split('+')[0]
        cmake_ver_clean = ''.join(c for c in cmake_ver_clean if c.isdigit() or c == '.')
        parts = [int(p) for p in cmake_ver_clean.split('.') if p]
        while len(parts) < 2:
            parts.append(0)
        return tuple(parts[:2]), cmake_ver
    except (subprocess.SubprocessError, OSError, ValueError, IndexError):
        return (0, 0), "0.0"


def require_cmake():
    cmake_ver_tuple, cmake_ver = get_cmake_version()
    if cmake_ver_tuple == (0, 0):
        print("[ERROR] CMake was not found in PATH. Please install CMake 3.10 or higher.", file=sys.stderr)
        sys.exit(1)
    if cmake_ver_tuple < (3, 10):
        print(f"[ERROR] CMake version {cmake_ver} is below the minimum required version 3.10", file=sys.stderr)
        sys.exit(1)
    return cmake_ver_tuple, cmake_ver


def setup_compiler_env(args):
    cc = os.environ.get("CC")
    cxx = os.environ.get("CXX")

    if cc and cxx:
        print(f"[compiler] Using env CC={cc}, CXX={cxx}")
        return cc, cxx

    sys_type = detect_system()
    print(f"[compiler] Detected environment: {sys_type}")

    # Respect partial env settings\n    if cc or cxx:\n        if cc and not cxx:\n            if "clang" in os.path.basename(cc).lower():\n                cxx = shutil.which("clang++") or "clang++"\n            else:\n                cxx = find_compiler("g++") or shutil.which("g++") or "g++"\n        elif cxx and not cc:\n            if "clang++" in os.path.basename(cxx).lower():\n                cc = shutil.which("clang") or "clang"\n            else:\n                cc = find_compiler("gcc") or shutil.which("gcc") or "gcc"\n        print(f"[compiler] Using env CC={cc}, CXX={cxx}")\n        return cc, cxx

    if sys_type == "macOS":
        cc = shutil.which("clang") or "clang"
        cxx = shutil.which("clang++") or "clang++"
        print(f"[compiler] Using macOS default: {cc}, {cxx}")
    else:
        cc = find_compiler("gcc") or shutil.which("gcc") or "gcc"
        cxx = find_compiler("g++") or shutil.which("g++") or "g++"
        print(f"[compiler] Using: {cc}, {cxx}")

    # Check GCC version on Linux (skip for Clang)
    if sys_type != "macOS" and "clang" not in os.path.basename(cc).lower():
        gcc_ver = get_compiler_version(cc)
        print(f"[compiler] GCC version: {'.'.join(map(str, gcc_ver))}")
        if gcc_ver < (8, 0, 0):
            print(f"[WARNING] GCC {'.'.join(map(str, gcc_ver))} is below minimum recommended version 8.0")
            print("[WARNING] Build may fail due to incomplete C++17 support.")
            print("[WARNING] Please install GCC 8+ or set CC/CXX environment variables.")

    return cc, cxx


def cmake_configure(args):
    build_dir = args.build_dir
    source_dir = PROJECT_ROOT

    # Check platform dependencies
    auto_yes = getattr(args, "yes", False)
    if not check_macos_libelf(auto_yes=auto_yes):
        print("[WARNING] Missing dependencies. Build may fail.")

    if args.clean and os.path.exists(build_dir):
        ensure_safe_build_dir_removal(build_dir)
        shutil.rmtree(build_dir)
        print(f"Cleaned build directory: {build_dir}")

    os.makedirs(build_dir, exist_ok=True)

    cc, cxx = setup_compiler_env(args)

    cmake_ver_tuple, _ = require_cmake()

    configure_cwd = None
    if cmake_ver_tuple >= (3, 13):
        cmake_args = [
            "cmake",
            "-S", source_dir,
            "-B", build_dir,
        ]
    else:
        cmake_args = ["cmake"]
        configure_cwd = build_dir

    generator = args.generator
    if generator:
        cmake_args += ["-G", generator]

    cmake_args += [
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        f"-DCMAKE_C_COMPILER={cc}",
        f"-DCMAKE_CXX_COMPILER={cxx}",
        f"-DOPT_LEVEL={args.opt_level}",
        f"-DBUILD_TESTS={'ON' if args.build_tests else 'OFF'}",
        f"-DGENERIC_SOC={'ON' if args.generic_soc else 'OFF'}",
        f"-DGENERIC_SOC_NEW={'ON' if args.generic_soc_new else 'OFF'}",
        f"-DDISABLE_DEBUG_SYMBOLS={'ON' if args.no_debug else 'OFF'}",
        f"-DCMAKE_VERBOSE_MAKEFILE={'ON' if args.verbose else 'OFF'}",
        f"-DENABLE_ASAN={'ON' if args.asan else 'OFF'}",
        f"-DENABLE_UBSAN={'ON' if args.ubsan else 'OFF'}",
        f"-DENABLE_COVERAGE={'ON' if args.coverage else 'OFF'}",
    ]

    if args.coverage_mode:
        cmake_args.append(f"-DCOVERAGE_MODE={args.coverage_mode}")

    if cmake_ver_tuple < (3, 13):
        cmake_args.append(source_dir)

    run(cmake_args, cwd=configure_cwd)


def cmake_build(args):
    build_dir = args.build_dir
    if not os.path.exists(build_dir):
        print(f"Build directory '{build_dir}' does not exist. Run configure first.")
        sys.exit(1)

    cmake_ver_tuple, _ = require_cmake()

    build_cmd = ["cmake", "--build", build_dir]
    if args.target:
        build_cmd += ["--target", args.target]

    if args.parallel:
        if cmake_ver_tuple >= (3, 12):
            build_cmd += ["-j", str(args.parallel)]
        else:
            build_cmd += ["--", "-j", str(args.parallel)]

    run(build_cmd)


def cmake_clean(args):
    build_dir = args.build_dir
    if os.path.exists(build_dir):
        ensure_safe_build_dir_removal(build_dir)
        shutil.rmtree(build_dir)
        print(f"Removed build directory: {build_dir}")
    else:
        print(f"Build directory '{build_dir}' does not exist, nothing to clean.")


def main():
    parser = argparse.ArgumentParser(
        description="LinxCoreModel CMake build script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py configure              # configure with defaults (Release, O2)
  python build.py configure -t Debug     # configure with Debug build type
  python build.py configure --tests      # configure with tests enabled
  python build.py configure --generic-soc # configure with generic SOC
  python build.py build -j8              # build with 8 parallel jobs
  python build.py build --target gfrun   # build only gfrun target
  python build.py clean                  # remove build directory
  python build.py all -j8                # configure + build in one step
  python build.py all --clean -j8        # clean + configure + build
        """,
    )

    subparsers = parser.add_subparsers(dest="command", help="Sub-command")

    # --- Common options shared across subcommands ---
    def add_common_options(p):
        p.add_argument(
            "--build-dir",
            default=DEFAULT_BUILD_DIR,
            help=f"Build directory (default: {DEFAULT_BUILD_DIR})",
        )
        p.add_argument(
            "-y", "--yes",
            action="store_true",
            help="Auto-confirm dependency installation (non-interactive)",
        )

    # --- configure ---
    cfg = subparsers.add_parser("configure", help="Run CMake configure")
    add_common_options(cfg)
    cfg.add_argument("-t", "--build-type", default="Release", choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"], help="CMake build type (default: Release)")
    cfg.add_argument("-O", "--opt-level", default="O2", choices=["O0", "O1", "O2", "O3", "Os"], help="Optimization level (default: O2)")
    cfg.add_argument("-G", "--generator", default=None, help="CMake generator (e.g. Ninja, 'Unix Makefiles')")
    cfg.add_argument("--tests", action="store_true", dest="build_tests", help="Build unit tests")
    cfg.add_argument("--generic-soc", action="store_true", help="Enable generic SOC V3.1.1")
    cfg.add_argument("--generic-soc-new", action="store_true", help="Enable new generic SOC V6.6.4")
    cfg.add_argument("--no-debug", action="store_true", help="Disable debug symbols")
    cfg.add_argument("--verbose", action="store_true", help="Enable verbose build output")
    cfg.add_argument("--asan", action="store_true", help="Enable AddressSanitizer")
    cfg.add_argument("--ubsan", action="store_true", help="Enable UndefinedBehaviorSanitizer")
    cfg.add_argument("--coverage", action="store_true", help="Enable code coverage")
    cfg.add_argument("--coverage-mode", choices=["FULL", "DIFF"], help="Coverage mode (default: FULL)")
    cfg.add_argument("--clean", action="store_true", help="Clean build dir before configuring")

    # --- build ---
    bld = subparsers.add_parser("build", help="Run CMake build")
    add_common_options(bld)
    bld.add_argument("-j", "--parallel", type=int, default=None, help="Number of parallel build jobs")
    bld.add_argument("--target", default=None, help="Build only the specified target (e.g. gfrun, gfsim)")

    # --- clean ---
    cln = subparsers.add_parser("clean", help="Remove build directory")
    add_common_options(cln)

    # --- all (configure + build) ---
    allp = subparsers.add_parser("all", help="Configure and build")
    add_common_options(allp)
    allp.add_argument("-t", "--build-type", default="Release", choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"], help="CMake build type (default: Release)")
    allp.add_argument("-O", "--opt-level", default="O2", choices=["O0", "O1", "O2", "O3", "Os"], help="Optimization level (default: O2)")
    allp.add_argument("-G", "--generator", default=None, help="CMake generator")
    allp.add_argument("--tests", action="store_true", dest="build_tests", help="Build unit tests")
    allp.add_argument("--generic-soc", action="store_true", help="Enable generic SOC V3.1.1")
    allp.add_argument("--generic-soc-new", action="store_true", help="Enable new generic SOC V6.6.4")
    allp.add_argument("--no-debug", action="store_true", help="Disable debug symbols")
    allp.add_argument("--verbose", action="store_true", help="Enable verbose build output")
    allp.add_argument("--asan", action="store_true", help="Enable AddressSanitizer")
    allp.add_argument("--ubsan", action="store_true", help="Enable UndefinedBehaviorSanitizer")
    allp.add_argument("--coverage", action="store_true", help="Enable code coverage")
    allp.add_argument("--coverage-mode", choices=["FULL", "DIFF"], help="Coverage mode")
    allp.add_argument("-j", "--parallel", type=int, default=None, help="Number of parallel build jobs")
    allp.add_argument("--target", default=None, help="Build only the specified target")
    allp.add_argument("--clean", action="store_true", help="Clean build dir before configuring")

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        sys.exit(0)

    if hasattr(args, "build_dir") and args.build_dir:
        args.build_dir = os.path.abspath(args.build_dir)

    if args.command == "configure":
        cmake_configure(args)
    elif args.command == "build":
        cmake_build(args)
    elif args.command == "clean":
        cmake_clean(args)
    elif args.command == "all":
        cmake_configure(args)
        cmake_build(args)


if __name__ == "__main__":
    main()
