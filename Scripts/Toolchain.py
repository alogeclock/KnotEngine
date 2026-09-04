import os
import hashlib
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_DIR = SCRIPT_DIR.parent
ENGINE_DIR = REPOSITORY_DIR / "KnotEngine"
INTERMEDIATE_DIR = ENGINE_DIR / "Intermediate"
TOOLS_DIR = INTERMEDIATE_DIR / "Tools"
CACHE_DIR = INTERMEDIATE_DIR / "Cache"
DOWNLOAD_DIR = CACHE_DIR / "downloads"
BINARY_CACHE_DIR = CACHE_DIR / "vcpkg-binary"

CMAKE_VERSION = "3.31.8"
CMAKE_ARCHIVE_NAME = f"cmake-{CMAKE_VERSION}-windows-x86_64"
CMAKE_URL = (
    f"https://github.com/Kitware/CMake/releases/download/v{CMAKE_VERSION}/"
    f"{CMAKE_ARCHIVE_NAME}.zip"
)

VCPKG_REF = "2026.04.27"
VCPKG_URL = f"https://github.com/microsoft/vcpkg/archive/refs/tags/{VCPKG_REF}.zip"

CMAKE_ROOT = TOOLS_DIR / "cmake"
VCPKG_ROOT = TOOLS_DIR / "vcpkg"
TRIPLET = "x64-windows"

LLVM_VERSION = "20.1.8"
LLVM_ROOT = TOOLS_DIR / f"llvm-{LLVM_VERSION}"
LLVM_SHA256 = "3197846a2b19063687dd56e93e34cd941e3548d907f23a6131571321bdf9fe7b"


def ensure_reflection_tools() -> Path:
    """Extract the pinned Clang parser locally without running a system installer."""
    library = LLVM_ROOT / "bin" / "libclang.dll"
    bindings = LLVM_ROOT / "python" / "clang"
    if has_tool(LLVM_ROOT, LLVM_VERSION, library) and (bindings / "cindex.py").exists():
        return LLVM_ROOT

    DOWNLOAD_DIR.mkdir(parents=True, exist_ok=True)
    archive = DOWNLOAD_DIR / f"LLVM-{LLVM_VERSION}-win64.exe"
    download(f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{LLVM_VERSION}/{archive.name}", archive)
    with archive.open("rb") as source:
        archive_hash = hashlib.sha256()
        for block in iter(lambda: source.read(1024 * 1024), b""):
            archive_hash.update(block)
        digest = archive_hash.hexdigest()
    if digest != LLVM_SHA256:
        raise RuntimeError(f"LLVM archive checksum mismatch: {archive}")

    vcpkg = ensure_vcpkg()
    fetched = subprocess.check_output([str(vcpkg), "fetch", "7zip"], text=True)
    seven_zip = Path(fetched.strip().splitlines()[-1].strip('"'))
    if not seven_zip.is_file():
        raise RuntimeError(f"vcpkg did not return a valid 7zip executable: {fetched}")
    LLVM_ROOT.mkdir(parents=True, exist_ok=True)
    run([str(seven_zip), "x", "-y", f"-o{LLVM_ROOT}", str(archive), "bin/libclang.dll", "lib/clang/*"], cwd=LLVM_ROOT)
    bindings.mkdir(parents=True, exist_ok=True)
    for name in ("__init__.py", "cindex.py"):
        download(f"https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-{LLVM_VERSION}/clang/bindings/python/clang/{name}", bindings / name)
    if not library.is_file():
        raise RuntimeError(f"LLVM extraction did not produce {library}")
    version_file(LLVM_ROOT).write_text(LLVM_VERSION + "\n", encoding="utf-8")
    return LLVM_ROOT


@dataclass(frozen=True)
class Toolchain:
    cmake_exe: Path
    environment: dict[str, str]


def run(command: list[str], cwd: Path) -> None:
    print("> " + " ".join(command), flush=True)
    subprocess.run(command, cwd=str(cwd), check=True)


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


def replace_dir_from_zip(zip_path: Path, target_dir: Path, version: str) -> None:
    with tempfile.TemporaryDirectory(prefix="knot_extract_") as temp_name:
        temp_dir = Path(temp_name)
        with zipfile.ZipFile(zip_path) as archive:
            archive.extractall(temp_dir)

        children = list(temp_dir.iterdir())
        if len(children) != 1 or not children[0].is_dir():
            raise RuntimeError(f"Expected a single root directory in {zip_path}")

        if target_dir.exists():
            shutil.rmtree(target_dir)
        target_dir.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(children[0]), str(target_dir))
        version_file(target_dir).write_text(version + "\n", encoding="utf-8")


def ensure_cmake() -> Path:
    cmake_exe = CMAKE_ROOT / "bin" / "cmake.exe"
    if not has_tool(CMAKE_ROOT, CMAKE_VERSION, cmake_exe):
        archive = DOWNLOAD_DIR / f"{CMAKE_ARCHIVE_NAME}.zip"
        download(CMAKE_URL, archive)
        replace_dir_from_zip(archive, CMAKE_ROOT, CMAKE_VERSION)
    return cmake_exe


def ensure_vcpkg() -> Path:
    vcpkg_exe = VCPKG_ROOT / "vcpkg.exe"
    if not has_tool(VCPKG_ROOT, VCPKG_REF, VCPKG_ROOT / "bootstrap-vcpkg.bat"):
        archive = DOWNLOAD_DIR / f"vcpkg-{VCPKG_REF}.zip"
        download(VCPKG_URL, archive)
        replace_dir_from_zip(archive, VCPKG_ROOT, VCPKG_REF)

    if not vcpkg_exe.exists():
        print("Bootstrapping vcpkg")
        run([str(VCPKG_ROOT / "bootstrap-vcpkg.bat"), "-disableMetrics"], cwd=VCPKG_ROOT)
    return vcpkg_exe


def prepare_toolchain() -> Toolchain:
    TOOLS_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    BINARY_CACHE_DIR.mkdir(parents=True, exist_ok=True)

    cmake_exe = ensure_cmake()
    vcpkg_exe = ensure_vcpkg()

    environment = os.environ.copy()
    environment["PATH"] = str(cmake_exe.parent) + os.pathsep + environment.get("PATH", "")
    environment["VCPKG_ROOT"] = str(VCPKG_ROOT)
    environment["VCPKG_DEFAULT_TRIPLET"] = TRIPLET
    environment["VCPKG_BINARY_SOURCES"] = f"clear;files,{BINARY_CACHE_DIR},readwrite"

    print("Toolchain is ready.")
    print(f"  CMake : {cmake_exe}")
    print(f"  vcpkg : {vcpkg_exe}")
    return Toolchain(cmake_exe=cmake_exe, environment=environment)
