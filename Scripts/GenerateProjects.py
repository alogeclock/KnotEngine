import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from Toolchain import Toolchain, prepare_toolchain

REPOSITORY_DIR = SCRIPT_DIR.parent
ENGINE_DIR = REPOSITORY_DIR / "KnotEngine"
BUILD_CONFIG_DIR = ENGINE_DIR / "Build"
CMAKE_SOURCE_DIR = BUILD_CONFIG_DIR / "CMake"
BUILD_DIR = BUILD_CONFIG_DIR / "VS2022-x64"
BUILD_CONFIGURATIONS = ("Debug", "Development", "Shipping")
SOLUTION_FILE = BUILD_DIR / "KnotEngine.sln"

def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("> " + " ".join(command), flush=True)
    subprocess.run(command, cwd=str(cwd), env=env, check=True)


def configure_project(toolchain: Toolchain) -> None:
    remove_stale_cmake_cache()
    run([str(toolchain.cmake_exe), "--preset", "vs2022-x64"], cwd=CMAKE_SOURCE_DIR, env=toolchain.environment)


def open_solution() -> None:
    if not SOLUTION_FILE.exists():
        raise RuntimeError(f"Generated solution was not found: {SOLUTION_FILE}")

    print(f"Opening solution: {SOLUTION_FILE}")
    os.startfile(SOLUTION_FILE)


def is_same_path(left: Path, right: Path) -> bool:
    return os.path.normcase(os.path.abspath(str(left))) == os.path.normcase(os.path.abspath(str(right)))


def remove_stale_cmake_cache() -> None:
    cache_file = BUILD_DIR / "CMakeCache.txt"
    if not cache_file.exists():
        return

    cache_source_dir = None
    for line in cache_file.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
            cache_source_dir = Path(line.split("=", 1)[1])
            break

    if cache_source_dir is not None and not is_same_path(cache_source_dir, CMAKE_SOURCE_DIR):
        print(f"Removing stale CMake build directory: {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate KnotEngine CMake projects.")
    parser.add_argument("--no-pause", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--no-open", action="store_true", help="Generate project files without opening the Visual Studio solution.")
    build_group = parser.add_mutually_exclusive_group()
    build_group.add_argument("--build", action="store_true", help="Build the selected configuration after configuring.")
    build_group.add_argument("--build-all", action="store_true", help="Build every KnotEngine configuration after configuring.")
    parser.add_argument("--config", choices=BUILD_CONFIGURATIONS, default="Development", help="Configuration used by --build (default: Development).")
    args = parser.parse_args()

    toolchain = prepare_toolchain()
    configure_project(toolchain)

    configurations = BUILD_CONFIGURATIONS if args.build_all else (args.config,) if args.build else ()
    for configuration in configurations:
        run(
            [str(toolchain.cmake_exe), "--build", str(BUILD_DIR), "--config", configuration],
            cwd=CMAKE_SOURCE_DIR,
            env=toolchain.environment,
        )

    print(f"Project files generated: {BUILD_DIR}")
    if not args.no_open:
        open_solution()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
