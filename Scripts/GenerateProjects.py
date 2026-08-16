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
from Toolchain import Toolchain, prepare_toolchain

# 하드코딩 디렉토리 상수
REPOSITORY_DIR = SCRIPT_DIR.parent
ENGINE_DIR = REPOSITORY_DIR / "KnotEngine"
BUILD_CONFIG_DIR = ENGINE_DIR / "Build"
CMAKE_SOURCE_DIR = BUILD_CONFIG_DIR / "CMake"
BUILD_DIR = BUILD_CONFIG_DIR / "VS2022-x64"
PROJECT_NAME = "KnotEngine"
BUILD_CONFIGURATIONS = ("Debug", "Development", "Shipping")
GENERATED_PROJECT_FILE = BUILD_DIR / f"{PROJECT_NAME}.vcxproj"
LEGACY_PROJECT_FILE = ENGINE_DIR / f"{PROJECT_NAME}.vcxproj"
ROOT_SOLUTION_FILE = REPOSITORY_DIR / f"{PROJECT_NAME}.sln"
MSBUILD_NS = "http://schemas.microsoft.com/developer/msbuild/2003"
CPP_PROJECT_TYPE_GUID = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
ROOT_SOLUTION_GUID = "{4EBC5DD2-CECA-4722-9D19-87C7CB5F481B}"
HIDDEN_PROJECT_ITEM_TYPES = (
    "ClCompile",
    "ClInclude",
    "CustomBuild",
    "Natvis",
)

def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("> " + " ".join(command), flush=True)
    subprocess.run(command, cwd=str(cwd), env=env, check=True)


def configure_project(toolchain: Toolchain) -> None:
    remove_stale_cmake_cache()
    run([str(toolchain.cmake_exe), "--preset", "vs2022-x64"], cwd=CMAKE_SOURCE_DIR, env=toolchain.environment)
    update_project_filters(GENERATED_PROJECT_FILE, ENGINE_DIR, PROJECT_NAME)
    hide_generated_project_items(GENERATED_PROJECT_FILE)
    copy_generated_project_to_root()
    generate_root_solution()


def copy_generated_project_to_root() -> None:
    generated_filters_file = GENERATED_PROJECT_FILE.with_suffix(GENERATED_PROJECT_FILE.suffix + ".filters")
    legacy_filters_file = LEGACY_PROJECT_FILE.with_suffix(LEGACY_PROJECT_FILE.suffix + ".filters")

    shutil.copy2(GENERATED_PROJECT_FILE, LEGACY_PROJECT_FILE)
    redirect_legacy_project_intermediate_dir()
    remove_legacy_visible_generated_items()
    if generated_filters_file.exists():
        shutil.copy2(generated_filters_file, legacy_filters_file)
    print(f"Copied generated project file: {LEGACY_PROJECT_FILE}")


def generate_root_solution() -> None:
    ET.register_namespace("", MSBUILD_NS)
    tree = ET.parse(LEGACY_PROJECT_FILE)
    project_guid_element = tree.getroot().find(f".//{{{MSBUILD_NS}}}ProjectGuid")
    if project_guid_element is None or not project_guid_element.text:
        raise RuntimeError(f"ProjectGuid was not found in {LEGACY_PROJECT_FILE}")

    project_guid = project_guid_element.text.strip().upper()
    lines = [
        "Microsoft Visual Studio Solution File, Format Version 12.00",
        "# Visual Studio Version 17",
        "VisualStudioVersion = 17.0.31903.59",
        "MinimumVisualStudioVersion = 10.0.40219.1",
        f'Project("{CPP_PROJECT_TYPE_GUID}") = "{PROJECT_NAME}", "KnotEngine\\{PROJECT_NAME}.vcxproj", "{project_guid}"',
        "EndProject",
        "Global",
        "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution",
    ]
    for configuration in BUILD_CONFIGURATIONS:
        lines.append(f"\t\t{configuration}|x64 = {configuration}|x64")
    lines.extend([
        "\tEndGlobalSection",
        "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution",
    ])
    for configuration in BUILD_CONFIGURATIONS:
        lines.append(f"\t\t{project_guid}.{configuration}|x64.ActiveCfg = {configuration}|x64")
        lines.append(f"\t\t{project_guid}.{configuration}|x64.Build.0 = {configuration}|x64")
    lines.extend([
        "\tEndGlobalSection",
        "\tGlobalSection(SolutionProperties) = preSolution",
        "\t\tHideSolutionNode = FALSE",
        "\tEndGlobalSection",
        "\tGlobalSection(ExtensibilityGlobals) = postSolution",
        f"\t\tSolutionGuid = {ROOT_SOLUTION_GUID}",
        "\tEndGlobalSection",
        "EndGlobal",
        "",
    ])

    ROOT_SOLUTION_FILE.write_text("\n".join(lines), encoding="utf-8", newline="\r\n")
    print(f"Generated root solution: {ROOT_SOLUTION_FILE}")


def redirect_legacy_project_intermediate_dir() -> None:
    if not LEGACY_PROJECT_FILE.exists():
        return

    ET.register_namespace("", MSBUILD_NS)
    tree = ET.parse(LEGACY_PROJECT_FILE)
    root = tree.getroot()
    intermediate_dir = f"Build\\VS2022-x64\\{PROJECT_NAME}.dir\\$(Configuration)\\"

    for element in root.iter(f"{{{MSBUILD_NS}}}IntDir"):
        element.text = intermediate_dir

    tree.write(LEGACY_PROJECT_FILE, encoding="utf-8", xml_declaration=True)


def should_hide_project_item(include: str) -> bool:
    normalized = include.replace("/", "\\").lower()
    return (
        "\\build\\vs2022-x64\\cmakefiles\\" in normalized
        or "\\build\\cmake\\cmakelists.txt" in normalized
        or "\\intermediate\\vcpkg_installed\\" in normalized
        or normalized.endswith("\\cmake_pch.cxx")
        or normalized.endswith("\\cmake_pch.hxx")
    )


def is_required_generated_project_item(include: str) -> bool:
    normalized = include.replace("/", "\\").lower()
    return normalized.endswith("\\cmake_pch.cxx") or normalized.endswith("\\cmake_pch.hxx")


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
            if include and should_hide_project_item(include) and not is_required_generated_project_item(include):
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
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
