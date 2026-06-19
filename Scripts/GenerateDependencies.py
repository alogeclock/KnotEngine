import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from DependencyState import (
    build_vcpkg_install_state,
    dependency_state_is_current,
    write_dependency_state,
)

REPOSITORY_DIR = SCRIPT_DIR.parent
ENGINE_DIR = REPOSITORY_DIR / "KnotEngine"
BUILD_DIR = ENGINE_DIR / "Build"
CMAKE_SOURCE_DIR = BUILD_DIR / "CMake"
INTERMEDIATE_DIR = ENGINE_DIR / "Intermediate"
TOOLS_DIR = INTERMEDIATE_DIR / "Tools"
CACHE_DIR = INTERMEDIATE_DIR / "Cache"
DOWNLOAD_DIR = CACHE_DIR / "downloads"
BINARY_CACHE_DIR = CACHE_DIR / "vcpkg-binary"
DEPENDENCY_STATE_DIR = CACHE_DIR / "dependency-state"
VCPKG_INSTALLED_DIR = INTERMEDIATE_DIR / "vcpkg_installed"

CMAKE_VERSION = "3.31.8"
CMAKE_ARCHIVE_NAME = f"cmake-{CMAKE_VERSION}-windows-x86_64"
CMAKE_URL = (
    f"https://github.com/Kitware/CMake/releases/download/v{CMAKE_VERSION}/"
    f"{CMAKE_ARCHIVE_NAME}.zip"
)

NINJA_VERSION = "1.12.1"
NINJA_URL = f"https://github.com/ninja-build/ninja/releases/download/v{NINJA_VERSION}/ninja-win.zip"

VCPKG_REF = "2026.04.27"
VCPKG_URL = f"https://github.com/microsoft/vcpkg/archive/refs/tags/{VCPKG_REF}.zip"

CMAKE_ROOT = TOOLS_DIR / "cmake"
NINJA_ROOT = TOOLS_DIR / "ninja"
VCPKG_ROOT = TOOLS_DIR / "vcpkg"

TRIPLET = "x64-windows"
VCPKG_STATE_FILE = DEPENDENCY_STATE_DIR / f"vcpkg-{TRIPLET}.json"
VCPKG_MANIFEST_FILES = [
    CMAKE_SOURCE_DIR / "vcpkg.json",
    CMAKE_SOURCE_DIR / "vcpkg-configuration.json",
]
VCPKG_REQUIRED_PATHS = [
    VCPKG_INSTALLED_DIR / "vcpkg" / "status",
    VCPKG_INSTALLED_DIR / TRIPLET / "share" / "imgui" / "imgui-config.cmake",
    VCPKG_INSTALLED_DIR / TRIPLET / "share" / "nlohmann_json" / "nlohmann_jsonConfig.cmake",
]


def version_file(tool_dir: Path) -> Path:
    return tool_dir / ".knot-tool-version"


def has_tool(tool_dir: Path, version: str, required_file: Path) -> bool:
    return (
        tool_dir.exists()
        and required_file.exists()
        and version_file(tool_dir).exists()
        and version_file(tool_dir).read_text(encoding="utf-8").strip() == version
    )


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        return

    print(f"Downloading {url}")
    with urllib.request.urlopen(url) as response:
        with destination.open("wb") as output:
            shutil.copyfileobj(response, output)


def replace_dir_from_zip(zip_path: Path, target_dir: Path, version: str, strip_single_root: bool) -> None:
    with tempfile.TemporaryDirectory(prefix="knot_extract_") as temp_name:
        temp_dir = Path(temp_name)
        with zipfile.ZipFile(zip_path) as archive:
            archive.extractall(temp_dir)

        if target_dir.exists():
            shutil.rmtree(target_dir)
        target_dir.parent.mkdir(parents=True, exist_ok=True)

        if strip_single_root:
            children = [child for child in temp_dir.iterdir()]
            if len(children) != 1 or not children[0].is_dir():
                raise RuntimeError(f"Expected a single root directory in {zip_path}")
            shutil.move(str(children[0]), str(target_dir))
        else:
            target_dir.mkdir(parents=True, exist_ok=True)
            for child in temp_dir.iterdir():
                shutil.move(str(child), str(target_dir / child.name))

        version_file(target_dir).write_text(version + "\n", encoding="utf-8")


def ensure_cmake() -> Path:
    cmake_exe = CMAKE_ROOT / "bin" / "cmake.exe"
    if not has_tool(CMAKE_ROOT, CMAKE_VERSION, cmake_exe):
        archive = DOWNLOAD_DIR / f"{CMAKE_ARCHIVE_NAME}.zip"
        download(CMAKE_URL, archive)
        replace_dir_from_zip(archive, CMAKE_ROOT, CMAKE_VERSION, strip_single_root=True)
    return cmake_exe


def ensure_ninja() -> Path:
    ninja_exe = NINJA_ROOT / "ninja.exe"
    if not has_tool(NINJA_ROOT, NINJA_VERSION, ninja_exe):
        archive = DOWNLOAD_DIR / f"ninja-{NINJA_VERSION}-win.zip"
        download(NINJA_URL, archive)
        replace_dir_from_zip(archive, NINJA_ROOT, NINJA_VERSION, strip_single_root=False)
    return ninja_exe


def ensure_vcpkg() -> Path:
    vcpkg_exe = VCPKG_ROOT / "vcpkg.exe"
    version = VCPKG_REF
    if not has_tool(VCPKG_ROOT, version, VCPKG_ROOT / "bootstrap-vcpkg.bat"):
        archive = DOWNLOAD_DIR / f"vcpkg-{VCPKG_REF}.zip"
        download(VCPKG_URL, archive)
        replace_dir_from_zip(archive, VCPKG_ROOT, version, strip_single_root=True)

    if not vcpkg_exe.exists():
        print("Bootstrapping vcpkg")
        run([str(VCPKG_ROOT / "bootstrap-vcpkg.bat"), "-disableMetrics"], cwd=VCPKG_ROOT)

    return vcpkg_exe


def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("> " + " ".join(command), flush=True)
    subprocess.run(command, cwd=str(cwd), env=env, check=True)


def make_env(cmake_exe: Path, ninja_exe: Path) -> dict[str, str]:
    env = os.environ.copy()
    paths = [
        str(cmake_exe.parent),
        str(ninja_exe.parent),
        env.get("PATH", ""),
    ]
    env["PATH"] = os.pathsep.join(paths)
    env["VCPKG_ROOT"] = str(VCPKG_ROOT)
    env["VCPKG_DEFAULT_TRIPLET"] = TRIPLET
    env["VCPKG_BINARY_SOURCES"] = f"clear;files,{BINARY_CACHE_DIR},readwrite"
    return env


def expected_vcpkg_state() -> dict:
    return build_vcpkg_install_state(
        manifest_root=CMAKE_SOURCE_DIR,
        install_root=VCPKG_INSTALLED_DIR,
        triplet=TRIPLET,
        vcpkg_ref=VCPKG_REF,
        manifest_files=VCPKG_MANIFEST_FILES,
    )


def install_dependencies(vcpkg_exe: Path, env: dict[str, str], force: bool = False) -> None:
    expected_state = expected_vcpkg_state()
    if not force and dependency_state_is_current(VCPKG_STATE_FILE, expected_state, VCPKG_REQUIRED_PATHS):
        print("vcpkg dependencies are up to date.")
        return

    BINARY_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    run(
        [
            str(vcpkg_exe),
            "install",
            "--triplet",
            TRIPLET,
            f"--x-manifest-root={CMAKE_SOURCE_DIR}",
            f"--x-install-root={VCPKG_INSTALLED_DIR}",
            "--clean-after-build",
        ],
        cwd=CMAKE_SOURCE_DIR,
        env=env,
    )
    write_dependency_state(VCPKG_STATE_FILE, expected_state)


def main() -> int:
    parser = argparse.ArgumentParser(description="Prepare KnotEngine build dependencies.")
    parser.add_argument("--skip-install", action="store_true", help="Only download tools; do not run vcpkg install.")
    parser.add_argument("--force-install", action="store_true", help="Run vcpkg install even if the dependency stamp is current.")
    parser.add_argument("--no-pause", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    TOOLS_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    VCPKG_INSTALLED_DIR.mkdir(parents=True, exist_ok=True)

    cmake_exe = ensure_cmake()
    ninja_exe = ensure_ninja()
    vcpkg_exe = ensure_vcpkg()
    env = make_env(cmake_exe, ninja_exe)

    if not args.skip_install:
        install_dependencies(vcpkg_exe, env, force=args.force_install)

    print("Dependencies are ready.")
    print(f"  CMake : {cmake_exe}")
    print(f"  Ninja : {ninja_exe}")
    print(f"  vcpkg : {vcpkg_exe}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
