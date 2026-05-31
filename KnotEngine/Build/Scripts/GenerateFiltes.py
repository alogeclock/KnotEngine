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


def is_msbuild_expression(value: str) -> bool:
    return "$(" in value or "%(" in value or "*" in value


def path_filter_for_include(include: str, project_file: Path, engine_dir: Path) -> str:
    if not include or is_msbuild_expression(include):
        return ""

    include_path = Path(include)
    if include_path.is_absolute():
        absolute_path = include_path
    else:
        absolute_path = project_file.parent / include_path

    try:
        relative_path = absolute_path.resolve().relative_to(engine_dir.resolve())
    except ValueError:
        return ""

    if not relative_path.parts or len(relative_path.parts) == 1:
        return ""

    if relative_path.parts[0].lower() == "intermediate":
        return "Generated\\Precompiled Headers" if absolute_path.name.startswith("cmake_pch") else ""

    return str(relative_path.parent).replace("/", "\\")


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


def update_project_filters(
    project_file: Path,
    engine_dir: Path,
    project_name: str,
    bom: bool = False,
) -> None:
    if not project_file.exists():
        return

    filters_file = project_file.with_suffix(project_file.suffix + ".filters")
    items = read_project_items(project_file)
    item_filters: dict[tuple[str, str], str] = {}
    filters: set[str] = set()

    for item_type, includes in items.items():
        for include in includes:
            filter_name = path_filter_for_include(include, project_file, engine_dir)
            item_filters[(item_type, include)] = filter_name
            if filter_name:
                add_filter_and_parents(filters, filter_name)

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
        for include in items[item_type]:
            item_element = ET.SubElement(item_group, item_type, Include=include)
            filter_name = item_filters[(item_type, include)]
            if filter_name:
                ET.SubElement(item_element, "Filter").text = filter_name

    write_xml(root, filters_file, bom=bom)
    print(f"Filter settings generated: {filters_file}")
