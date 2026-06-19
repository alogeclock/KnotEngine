import argparse
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from GenerateFilters import update_project_filters

# 하드코딩 디렉토리 상수
BUILD_CONFIG_DIR = SCRIPT_DIR.parent
ENGINE_DIR = BUILD_CONFIG_DIR.parent
CMAKE_SOURCE_DIR = BUILD_CONFIG_DIR / "CMake"
INTERMEDIATE_DIR = ENGINE_DIR / "Intermediate"
CMAKE_EXE = INTERMEDIATE_DIR / "Tools" / "cmake" / "bin" / "cmake.exe"
VCPKG_ROOT = INTERMEDIATE_DIR / "Tools" / "vcpkg"
BINARY_CACHE_DIR = INTERMEDIATE_DIR / "Cache" / "vcpkg-binary"
BUILD_DIR = INTERMEDIATE_DIR / "Build" / "VS2022-x64"
PROJECT_NAME = "KnotEngine"
GENERATED_PROJECT_FILE = BUILD_DIR / f"{PROJECT_NAME}.vcxproj"
LEGACY_PROJECT_FILE = ENGINE_DIR / f"{PROJECT_NAME}.vcxproj"
MSBUILD_NS = "http://schemas.microsoft.com/developer/msbuild/2003"
HIDDEN_PROJECT_ITEM_TYPES = (
    "ClCompile",
    "ClInclude",
    "CustomBuild",
    "Natvis",
)

def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("> " + " ".join(command), flush=True)
    subprocess.run(command, cwd=str(cwd), env=env, check=True)


def ensure_dependencies() -> None:
    run([
        str(SCRIPT_DIR / "python" / "python.exe"),
        str(SCRIPT_DIR / "GenerateDependencies.py"),
        "--no-pause",
    ], cwd=ENGINE_DIR)


def make_env() -> dict[str, str]:
    env = os.environ.copy()
    cmake_bin = str(CMAKE_EXE.parent)
    env["PATH"] = cmake_bin + os.pathsep + env.get("PATH", "")
    env["VCPKG_ROOT"] = str(VCPKG_ROOT)
    env["VCPKG_DEFAULT_TRIPLET"] = "x64-windows"
    env["VCPKG_BINARY_SOURCES"] = f"clear;files,{BINARY_CACHE_DIR},readwrite"
    return env


def configure_project() -> None:
    remove_stale_cmake_cache()
    run([str(CMAKE_EXE), "--preset", "vs2022-x64"], cwd=CMAKE_SOURCE_DIR, env=make_env())
    update_project_filters(GENERATED_PROJECT_FILE, ENGINE_DIR, PROJECT_NAME)
    hide_generated_project_items(GENERATED_PROJECT_FILE)
    copy_generated_project_to_root()


def copy_generated_project_to_root() -> None:
    generated_filters_file = GENERATED_PROJECT_FILE.with_suffix(GENERATED_PROJECT_FILE.suffix + ".filters")
    legacy_filters_file = LEGACY_PROJECT_FILE.with_suffix(LEGACY_PROJECT_FILE.suffix + ".filters")

    shutil.copy2(GENERATED_PROJECT_FILE, LEGACY_PROJECT_FILE)
    remove_legacy_visible_generated_items()
    if generated_filters_file.exists():
        shutil.copy2(generated_filters_file, legacy_filters_file)
    print(f"Copied generated project file: {LEGACY_PROJECT_FILE}")


def should_hide_project_item(include: str) -> bool:
    normalized = include.replace("/", "\\").lower()
    return (
        "\\build\\vs2022-x64\\cmakefiles\\" in normalized
        or "\\build\\cmake\\cmakelists.txt" in normalized
        or "\\intermediate\\vcpkg_installed\\" in normalized
        or normalized.endswith("\\cmake_pch.cxx")
        or normalized.endswith("\\cmake_pch.hxx")
    )


def hide_generated_project_items(project_file: Path) -> None:
    if not project_file.exists():
        return

    ET.register_namespace("", MSBUILD_NS)
    tree = ET.parse(project_file)
    root = tree.getroot()

    for item_type in HIDDEN_PROJECT_ITEM_TYPES:
        for element in root.iter(f"{{{MSBUILD_NS}}}{item_type}"):
            include = element.get("Include")
            if not include or not should_hide_project_item(include):
                continue

            visible = element.find(f"{{{MSBUILD_NS}}}Visible")
            if visible is None:
                visible = ET.SubElement(element, f"{{{MSBUILD_NS}}}Visible")
            visible.text = "false"

    tree.write(project_file, encoding="utf-8", xml_declaration=True)


def remove_legacy_visible_generated_items() -> None:
    if not LEGACY_PROJECT_FILE.exists():
        return

    ET.register_namespace("", MSBUILD_NS)
    tree = ET.parse(LEGACY_PROJECT_FILE)
    root = tree.getroot()

    for parent in root.findall(f"{{{MSBUILD_NS}}}ItemGroup"):
        for child in list(parent):
            include = child.get("Include")
            if include and should_hide_project_item(include):
                parent.remove(child)

    tree.write(LEGACY_PROJECT_FILE, encoding="utf-8", xml_declaration=True)


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
    parser.add_argument("--skip-dependencies", action="store_true", help="Do not run GenerateDependencies.py first.")
    parser.add_argument("--build", action="store_true", help="Build Debug after configuring.")
    args = parser.parse_args()

    if not args.skip_dependencies:
        ensure_dependencies()

    if not CMAKE_EXE.exists():
        print(f"CMake was not found at {CMAKE_EXE}. Run GenerateDependencies.bat first.", file=sys.stderr)
        return 1

    configure_project()

    if args.build:
        run([str(CMAKE_EXE), "--build", str(BUILD_DIR), "--config", "Debug"], cwd=CMAKE_SOURCE_DIR, env=make_env())

    print(f"Project files generated: {BUILD_DIR}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
