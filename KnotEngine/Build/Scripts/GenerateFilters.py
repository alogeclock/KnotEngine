import hashlib
import xml.etree.ElementTree as ET
from pathlib import Path


MSBUILD_NS = "http://schemas.microsoft.com/developer/msbuild/2003"
FILTER_ITEM_TYPES = (
    "ClCompile",
    "ClInclude",
    "FxCompile",
    "None",
    "CustomBuild",
    "ResourceCompile",
    "Natvis",
)
SOURCE_COMPILE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx"}
SOURCE_HEADER_EXTENSIONS = {".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
SHADER_EXTENSIONS = {".hlsl", ".hlsli", ".fx", ".fxh"}


def indent_xml(element: ET.Element, level: int = 0) -> None:
    indent = "\n" + "  " * level
    if len(element):
        if not element.text or not element.text.strip():
            element.text = indent + "  "
        for child in element:
            indent_xml(child, level + 1)
        if not element[-1].tail or not element[-1].tail.strip():
            element[-1].tail = indent
    if level and (not element.tail or not element.tail.strip()):
        element.tail = indent
    if level == 0:
        element.tail = "\n"


def write_xml(root: ET.Element, destination: Path, bom: bool = False) -> None:
    indent_xml(root)
    tree = ET.ElementTree(root)
    destination.parent.mkdir(parents=True, exist_ok=True)
    encoding = "utf-8-sig" if bom else "utf-8"
    with destination.open("w", encoding=encoding, newline="\r\n") as file:
        file.write('<?xml version="1.0" encoding="utf-8"?>\n')
        tree.write(file, encoding="unicode", xml_declaration=False)


def stable_filter_guid(project_name: str, filter_name: str) -> str:
    digest = hashlib.md5(f"{project_name}:{filter_name}".encode("utf-8")).hexdigest()
    return f"{{{digest[:8]}-{digest[8:12]}-{digest[12:16]}-{digest[16:20]}-{digest[20:32]}}}"


def add_filter_and_parents(filters: set[str], filter_name: str) -> None:
    parts = filter_name.split("\\")
    for index in range(1, len(parts) + 1):
        filters.add("\\".join(parts[:index]))


def normalized_path_key(path: Path) -> str:
    return str(path.resolve()).replace("/", "\\").lower()


def is_msbuild_expression(value: str) -> bool:
    return "$(" in value or "%(" in value or "*" in value


def resolve_project_include_path(include: str, project_file: Path) -> Path | None:
    if not include or is_msbuild_expression(include):
        return None

    include_path = Path(include)
    if include_path.is_absolute():
        return include_path

    return project_file.parent / include_path


def read_project_items(project_file: Path) -> dict[str, list[str]]:
    root = ET.parse(project_file).getroot()
    items: dict[str, list[str]] = {item_type: [] for item_type in FILTER_ITEM_TYPES}

    for item_type in FILTER_ITEM_TYPES:
        seen: set[str] = set()
        for element in root.iter(f"{{{MSBUILD_NS}}}{item_type}"):
            include = element.get("Include")
            if include and include not in seen:
                items[item_type].append(include)
                seen.add(include)

    return items


def read_project_include_lookup(project_file: Path) -> dict[str, list[tuple[str, str]]]:
    if not project_file.exists():
        return {}

    items = read_project_items(project_file)
    include_lookup: dict[str, list[tuple[str, str]]] = {}

    for item_type, includes in items.items():
        for include in includes:
            include_path = resolve_project_include_path(include, project_file)
            if include_path is None:
                continue
            include_lookup.setdefault(normalized_path_key(include_path), []).append((item_type, include))

    return include_lookup


def default_item_type_for_path(path: Path) -> str | None:
    suffix = path.suffix.lower()
    if suffix in SOURCE_COMPILE_EXTENSIONS:
        return "ClCompile"
    if suffix in SOURCE_HEADER_EXTENSIONS:
        return "ClInclude"
    if suffix in SHADER_EXTENSIONS:
        return "FxCompile"
    return None


def include_for_path(path: Path, project_file: Path, engine_dir: Path, include_lookup: dict[str, list[tuple[str, str]]]) -> tuple[str, str] | None:
    default_item_type = default_item_type_for_path(path)
    if default_item_type is None:
        return None

    existing_items = include_lookup.get(normalized_path_key(path), [])
    for item_type, include in existing_items:
        if item_type == default_item_type:
            return item_type, include

    if existing_items:
        return existing_items[0]

    try:
        include = str(path.resolve().relative_to(project_file.parent.resolve())).replace("/", "\\")
    except ValueError:
        include = str(path.resolve())

    return default_item_type, include


def filter_for_path(path: Path, engine_dir: Path) -> str:
    try:
        relative_path = path.resolve().relative_to(engine_dir.resolve())
    except ValueError:
        return ""

    if not relative_path.parts or len(relative_path.parts) == 1:
        return ""

    return str(relative_path.parent).replace("/", "\\")


def scan_project_files(engine_dir: Path) -> list[Path]:
    files: list[Path] = []

    for root_file_name in ("main.cpp", "pch.cpp", "pch.h"):
        root_file = engine_dir / root_file_name
        if root_file.exists():
            files.append(root_file)

    for directory in (engine_dir / "Source", engine_dir / "Shaders"):
        if directory.exists():
            files.extend(path for path in directory.rglob("*") if path.is_file() and default_item_type_for_path(path) is not None)

    return sorted(files, key=lambda path: str(path.resolve()).lower())


def build_filter_items(
    project_file: Path,
    engine_dir: Path,
) -> tuple[dict[str, list[tuple[str, str]]], set[str]]:
    include_lookup = read_project_include_lookup(project_file)
    items: dict[str, list[tuple[str, str]]] = {item_type: [] for item_type in FILTER_ITEM_TYPES}
    filters: set[str] = set()
    seen: set[tuple[str, str]] = set()

    for path in scan_project_files(engine_dir):
        item = include_for_path(path, project_file, engine_dir, include_lookup)
        if item is None:
            continue

        item_type, include = item
        key = (item_type, include)
        if key in seen:
            continue
        seen.add(key)

        filter_name = filter_for_path(path, engine_dir)
        items[item_type].append((include, filter_name))
        if filter_name:
            add_filter_and_parents(filters, filter_name)

    return items, filters


def update_project_filters(
    project_file: Path,
    engine_dir: Path,
    project_name: str,
    bom: bool = False,
) -> None:
    if not project_file.exists():
        return

    filters_file = project_file.with_suffix(project_file.suffix + ".filters")
    items, filters = build_filter_items(project_file, engine_dir)

    root = ET.Element("Project", ToolsVersion="4.0", xmlns=MSBUILD_NS)

    if filters:
        filter_group = ET.SubElement(root, "ItemGroup")
        for filter_name in sorted(filters):
            filter_element = ET.SubElement(filter_group, "Filter", Include=filter_name)
            ET.SubElement(filter_element, "UniqueIdentifier").text = stable_filter_guid(project_name, filter_name)

    for item_type in FILTER_ITEM_TYPES:
        if not items[item_type]:
            continue

        item_group = ET.SubElement(root, "ItemGroup")
        for include, filter_name in items[item_type]:
            item_element = ET.SubElement(item_group, item_type, Include=include)
            if filter_name:
                ET.SubElement(item_element, "Filter").text = filter_name

    write_xml(root, filters_file, bom=bom)
    print(f"Filter settings generated: {filters_file}")
