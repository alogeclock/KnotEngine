"""
GenerateProjectFiles.py — Auto-generate .vcxproj, .vcxproj.filters, .sln
for SimpleEngine from the on-disk folder structure.

Project layout:
    ROOT/                        ← repo root (SimpleEngine.sln lives here)
    ├── main.cpp                 ← entry point (outside the VS project folder)
    └── SimpleEngine/            ← PROJECT_DIR (.vcxproj lives here)
        ├── Source/
        ├── ThirdParty/ImGui/
        └── Shaders/

Usage:
    python Scripts/GenerateProjectFiles.py
"""

import hashlib
import os
import xml.etree.ElementTree as ET
from pathlib import Path

# ──────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────
ROOT = Path(__file__).resolve().parent.parent
PROJECT_DIR = ROOT / "SimpleEngine"   # directory that contains SimpleEngine.vcxproj

PROJECT_NAME = "SimpleEngine"
PROJECT_GUID = "{55068e81-c0a0-49f9-ab7b-54aea968722b}"
ROOT_NAMESPACE = "SimpleEngine"

SOLUTION_GUID = "{4EBC5DD2-CECA-4722-9D19-87C7CB5F481B}"
VS_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

CONFIGURATIONS = [
    ("Debug",   "Win32"),
    ("Release", "Win32"),
    ("Debug",   "x64"),
    ("Release", "x64"),
]

# Directories to recursively scan for source/header files (relative to PROJECT_DIR)
SCAN_DIRS = ["Source"]

# Directories to scan at the top level ONLY (no recursion) — e.g. ImGui
FLAT_SCAN_DIRS = ["ThirdParty\\ImGui"]

# Directories to scan for shader files (relative to PROJECT_DIR)
SHADER_DIRS = ["Shaders"]

# File extensions to include
SOURCE_EXTS = {".cpp", ".c", ".cc", ".cxx"}
HEADER_EXTS = {".h", ".hpp", ".hxx", ".inl"}
SHADER_EXTS = {".hlsl", ".hlsli"}
NATVIS_EXTS = {".natvis"}
NONE_EXTS = {".natstepfilter", ".config"}

# Files outside PROJECT_DIR to include — paths relative to PROJECT_DIR
# main.cpp sits one level above PROJECT_DIR (at ROOT), so it's "..\main.cpp"
EXTRA_SOURCE_FILES = [r"..\main.cpp"]

# Name of the PCH header (must match the filename in Source\Core\)
PCH_HEADER = "pch.h"
# Path to pch.cpp relative to PROJECT_DIR
PCH_CPP = r"Source\Core\pch.cpp"

# Include paths (relative to PROJECT_DIR)
INCLUDE_PATHS = [
    "Source",
    "Source\\Engine",
    "Source\\Editor",
    "Source\\Core",
    "ThirdParty\\ImGui",
    ".",
]

# Library paths (relative to PROJECT_DIR)
LIBRARY_PATHS = []

# NuGet packages (id, version)
NUGET_PACKAGES = []

# ImGui source prefix — these files must NOT use the PCH
IMGUI_PREFIX = "ThirdParty\\ImGui\\"

NS = "http://schemas.microsoft.com/developer/msbuild/2003"


# ──────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────
def scan_files(project_dir: Path) -> dict[str, list[str]]:
    """Scan directories and collect files grouped by type."""
    result = {"ClCompile": [], "ClInclude": [], "FxCompile": [], "Natvis": [], "None": []}

    def classify(rel_str: str, ext: str):
        if ext in SOURCE_EXTS:
            result["ClCompile"].append(rel_str)
        elif ext in HEADER_EXTS:
            result["ClInclude"].append(rel_str)
        elif ext in NATVIS_EXTS:
            result["Natvis"].append(rel_str)
        elif ext in NONE_EXTS:
            result["None"].append(rel_str)

    # Recursively scan source/header directories
    for scan_dir in SCAN_DIRS:
        full_dir = project_dir / scan_dir
        if not full_dir.exists():
            continue
        for dirpath, _, filenames in os.walk(full_dir):
            for fname in sorted(filenames):
                full = Path(dirpath) / fname
                rel_str = str(full.relative_to(project_dir)).replace("/", "\\")
                classify(rel_str, full.suffix.lower())

    # Top-level only scan (no recursion into sub-directories)
    for scan_dir in FLAT_SCAN_DIRS:
        full_dir = project_dir / scan_dir
        if not full_dir.exists():
            continue
        for fname in sorted(os.listdir(full_dir)):
            full = full_dir / fname
            if full.is_file():
                rel_str = str(full.relative_to(project_dir)).replace("/", "\\")
                classify(rel_str, full.suffix.lower())

    # Scan shader directories
    for shader_dir in SHADER_DIRS:
        full_dir = project_dir / shader_dir
        if not full_dir.exists():
            continue
        for dirpath, _, filenames in os.walk(full_dir):
            for fname in sorted(filenames):
                full = Path(dirpath) / fname
                rel_str = str(full.relative_to(project_dir)).replace("/", "\\")
                if full.suffix.lower() in SHADER_EXTS:
                    result["FxCompile"].append(rel_str)

    # Add extra files that live outside PROJECT_DIR (e.g. main.cpp at ROOT)
    for extra in EXTRA_SOURCE_FILES:
        result["ClCompile"].append(extra)

    return result


def get_filter(rel_path: str) -> str:
    r"""Return the filter (directory portion) from a relative path.
    Files outside the project dir (e.g. ..\main.cpp) have no filter.
    """
    if rel_path.startswith(".."):
        return ""
    parts = rel_path.replace("/", "\\").rsplit("\\", 1)
    return parts[0] if len(parts) > 1 else ""


def collect_all_filters(files: dict[str, list[str]]) -> set[str]:
    """Collect all unique filter paths including parent paths."""
    filters = set()
    for file_list in files.values():
        for f in file_list:
            filt = get_filter(f)
            if filt:
                parts = filt.split("\\")
                for i in range(1, len(parts) + 1):
                    filters.add("\\".join(parts[:i]))
    return filters


# ──────────────────────────────────────────────
# XML Generation
# ──────────────────────────────────────────────
def indent_xml(elem, level=0):
    """Add indentation to XML tree."""
    i = "\n" + "  " * level
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for child in elem:
            indent_xml(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i
    if level == 0:
        elem.tail = "\n"


def write_xml(root_elem, filepath: Path, bom=False):
    """Write XML tree to file with proper declaration."""
    indent_xml(root_elem)
    tree = ET.ElementTree(root_elem)
    with open(filepath, "w", encoding="utf-8", newline="\r\n") as f:
        if bom:
            f.write("\ufeff")
        f.write('<?xml version="1.0" encoding="utf-8"?>\n')
        tree.write(f, encoding="unicode", xml_declaration=False)


# ──────────────────────────────────────────────
# .vcxproj
# ──────────────────────────────────────────────
def generate_vcxproj(files: dict[str, list[str]]):
    proj = ET.Element("Project", DefaultTargets="Build", xmlns=NS)

    # ProjectConfigurations
    ig = ET.SubElement(proj, "ItemGroup", Label="ProjectConfigurations")
    for cfg, plat in CONFIGURATIONS:
        pc = ET.SubElement(ig, "ProjectConfiguration", Include=f"{cfg}|{plat}")
        ET.SubElement(pc, "Configuration").text = cfg
        ET.SubElement(pc, "Platform").text = plat

    # Globals
    pg = ET.SubElement(proj, "PropertyGroup", Label="Globals")
    ET.SubElement(pg, "VCProjectVersion").text = "17.0"
    ET.SubElement(pg, "Keyword").text = "Win32Proj"
    ET.SubElement(pg, "ProjectGuid").text = PROJECT_GUID
    ET.SubElement(pg, "RootNamespace").text = ROOT_NAMESPACE
    ET.SubElement(pg, "WindowsTargetPlatformVersion").text = "10.0"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    # Configuration properties
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond, Label="Configuration")
        is_release = cfg == "Release"
        ET.SubElement(pg, "ConfigurationType").text = "Application"
        ET.SubElement(pg, "UseDebugLibraries").text = "false" if is_release else "true"
        ET.SubElement(pg, "PlatformToolset").text = "v143"
        if is_release:
            ET.SubElement(pg, "WholeProgramOptimization").text = "true"
        ET.SubElement(pg, "CharacterSet").text = "Unicode"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionSettings")
    ET.SubElement(proj, "ImportGroup", Label="Shared")

    # PropertySheets
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        ig = ET.SubElement(proj, "ImportGroup", Label="PropertySheets", Condition=cond)
        ET.SubElement(ig, "Import",
                      Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
                      Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
                      Label="LocalAppDataPlatform")

    ET.SubElement(proj, "PropertyGroup", Label="UserMacros")

    # OutDir, IntDir, IncludePath, LibraryPath, WorkingDirectory
    include_path_value = ";".join(INCLUDE_PATHS) + ";$(IncludePath)"
    library_path_value = ";".join(LIBRARY_PATHS) + ";$(LibraryPath)" if LIBRARY_PATHS else "$(LibraryPath)"
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond)
        ET.SubElement(pg, "OutDir").text = "$(ProjectDir)Bin\\$(Configuration)\\"
        ET.SubElement(pg, "IntDir").text = "$(ProjectDir)Build\\$(Configuration)\\"
        ET.SubElement(pg, "IncludePath").text = include_path_value
        ET.SubElement(pg, "LibraryPath").text = library_path_value
        ET.SubElement(pg, "LocalDebuggerWorkingDirectory").text = "$(ProjectDir)"

    # ItemDefinitionGroups
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(proj, "ItemDefinitionGroup", Condition=cond)
        cl = ET.SubElement(idg, "ClCompile")
        ET.SubElement(cl, "WarningLevel").text = "Level3"

        is_release = cfg == "Release"
        is_win32 = plat == "Win32"
        is_x64 = plat == "x64"

        if is_release:
            ET.SubElement(cl, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl, "IntrinsicFunctions").text = "true"

        ET.SubElement(cl, "SDLCheck").text = "true"

        if is_win32:
            defs = f"WIN32;{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;WITH_EDITOR=1;NOMINMAX;%(PreprocessorDefinitions);"
        else:
            defs = f"{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;WITH_EDITOR=1;NOMINMAX;%(PreprocessorDefinitions);"

        ET.SubElement(cl, "PreprocessorDefinitions").text = defs
        ET.SubElement(cl, "MultiProcessorCompilation").text = "true"
        ET.SubElement(cl, "ConformanceMode").text = "true"
        ET.SubElement(cl, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"
        ET.SubElement(cl, "ExceptionHandling").text = "Async"
        ET.SubElement(cl, "LanguageStandard").text = "stdcpp20"
        # PCH: all files use pch.h by default; pch.cpp overrides to Create below
        ET.SubElement(cl, "PrecompiledHeader").text = "Use"
        ET.SubElement(cl, "PrecompiledHeaderFile").text = PCH_HEADER

        ET.SubElement(cl, "ForcedIncludeFiles").text = f"{PCH_HEADER};%(ForcedIncludeFiles)"

        link = ET.SubElement(idg, "Link")
        ET.SubElement(link, "SubSystem").text = "Windows" if is_x64 else "Console"
        ET.SubElement(link, "GenerateDebugInformation").text = "true"

    # ClCompile items
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClCompile"]:
        elem = ET.SubElement(ig, "ClCompile", Include=f)
        if f == PCH_CPP:
            # pch.cpp creates the PCH binary
            ET.SubElement(elem, "PrecompiledHeader").text = "Create"
        elif f.startswith(IMGUI_PREFIX):
            # Third-party files must not use our PCH
            ET.SubElement(elem, "PrecompiledHeader").text = "NotUsing"
            # [여기에 추가] 서드파티 라이브러리에는 강제 포함을 비활성화 (빈 값으로 덮어쓰기)
            ET.SubElement(elem, "ForcedIncludeFiles").text = ""

    # ClInclude items
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClInclude"]:
        ET.SubElement(ig, "ClInclude", Include=f)

    # FxCompile items (shaders — excluded from build, compiled at runtime)
    if files["FxCompile"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["FxCompile"]:
            elem = ET.SubElement(ig, "FxCompile", Include=f)
            for cfg, plat in CONFIGURATIONS:
                if plat == "x64":
                    cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
                    ET.SubElement(elem, "ExcludedFromBuild", Condition=cond).text = "true"

    # Natvis items
    if files["Natvis"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["Natvis"]:
            ET.SubElement(ig, "Natvis", Include=f)

    # None items
    if files["None"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["None"]:
            ET.SubElement(ig, "None", Include=f)

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")

    if NUGET_PACKAGES:
        ext_targets = ET.SubElement(proj, "ImportGroup", Label="ExtensionTargets")
        for pkg_id, pkg_ver in NUGET_PACKAGES:
            targets_path = f"packages\\{pkg_id}.{pkg_ver}\\build\\native\\{pkg_id}.targets"
            ET.SubElement(ext_targets, "Import",
                          Project=targets_path,
                          Condition=f"Exists('{targets_path}')")
    else:
        ET.SubElement(proj, "ImportGroup", Label="ExtensionTargets")

    write_xml(proj, PROJECT_DIR / f"{PROJECT_NAME}.vcxproj")


# ──────────────────────────────────────────────
# .vcxproj.filters
# ──────────────────────────────────────────────
def generate_filters(files: dict[str, list[str]]):
    proj = ET.Element("Project", ToolsVersion="4.0", xmlns=NS)

    all_filters = collect_all_filters(files)

    if all_filters:
        ig = ET.SubElement(proj, "ItemGroup")
        for filt in sorted(all_filters):
            f_elem = ET.SubElement(ig, "Filter", Include=filt)
            h = hashlib.md5(f"{PROJECT_NAME}:{filt}".encode()).hexdigest()
            uid = f"{{{h[:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}}}"
            ET.SubElement(f_elem, "UniqueIdentifier").text = uid

    for tag, key in [("FxCompile", "FxCompile"), ("ClCompile", "ClCompile"),
                     ("ClInclude", "ClInclude"), ("None", "None"), ("Natvis", "Natvis")]:
        if files[key]:
            ig = ET.SubElement(proj, "ItemGroup")
            for f in files[key]:
                filt = get_filter(f)
                elem = ET.SubElement(ig, tag, Include=f)
                if filt:
                    ET.SubElement(elem, "Filter").text = filt

    write_xml(proj, PROJECT_DIR / f"{PROJECT_NAME}.vcxproj.filters", bom=True)


# ──────────────────────────────────────────────
# .sln
# ──────────────────────────────────────────────
def generate_sln():
    lines = []
    lines.append("")
    lines.append("Microsoft Visual Studio Solution File, Format Version 12.00")
    lines.append("# Visual Studio Version 17")
    lines.append("VisualStudioVersion = 17.14.37012.4 d17.14")
    lines.append("MinimumVisualStudioVersion = 10.0.40219.1")

    guid_upper = PROJECT_GUID.upper()
    # .vcxproj is inside the SimpleEngine\ subdirectory, one level below the .sln
    vcxproj_rel = f"{PROJECT_NAME}\\{PROJECT_NAME}.vcxproj"
    lines.append(
        f'Project("{VS_PROJECT_TYPE}") = "{PROJECT_NAME}", '
        f'"{vcxproj_rel}", "{guid_upper}"'
    )
    lines.append("EndProject")

    lines.append("Global")

    lines.append("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution")
    for cfg, plat in CONFIGURATIONS:
        sln_plat = "x86" if plat == "Win32" else plat
        lines.append(f"\t\t{cfg}|{sln_plat} = {cfg}|{sln_plat}")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
    for cfg, plat in CONFIGURATIONS:
        sln_plat = "x86" if plat == "Win32" else plat
        lines.append(f"\t\t{guid_upper}.{cfg}|{sln_plat}.ActiveCfg = {cfg}|{plat}")
        lines.append(f"\t\t{guid_upper}.{cfg}|{sln_plat}.Build.0 = {cfg}|{plat}")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(SolutionProperties) = preSolution")
    lines.append("\t\tHideSolutionNode = FALSE")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(ExtensibilityGlobals) = postSolution")
    lines.append(f"\t\tSolutionGuid = {SOLUTION_GUID}")
    lines.append("\tEndGlobalSection")

    lines.append("EndGlobal")
    lines.append("")

    sln_path = ROOT / f"{PROJECT_NAME}.sln"
    with open(sln_path, "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("\n".join(lines))


# ──────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────
def main():
    print(f"Project dir : {PROJECT_DIR}")
    print(f"Repo root   : {ROOT}")
    print(f"Scanning project files...")

    files = scan_files(PROJECT_DIR)

    print(f"  ClCompile:  {len(files['ClCompile'])} files")
    print(f"  ClInclude:  {len(files['ClInclude'])} files")
    print(f"  FxCompile:  {len(files['FxCompile'])} files")
    print(f"  Natvis:     {len(files['Natvis'])} files")
    print(f"  None:       {len(files['None'])} files")

    print("Generating project files...")

    generate_vcxproj(files)
    print(f"  {PROJECT_DIR / (PROJECT_NAME + '.vcxproj')}")

    generate_filters(files)
    print(f"  {PROJECT_DIR / (PROJECT_NAME + '.vcxproj.filters')}")

    generate_sln()
    print(f"  {ROOT / (PROJECT_NAME + '.sln')}")

    print("Done!")


if __name__ == "__main__":
    main()
