"""Generate reflection registration from Clang declarations and active marker macros."""

import argparse
import hashlib
import json
import os
import re
import sys
import shlex
from functools import lru_cache
from dataclasses import dataclass
from pathlib import Path

from Toolchain import LLVM_ROOT, LLVM_VERSION


def load_clang():
    sys.path.insert(0, str(LLVM_ROOT / "python"))
    from clang import cindex
    cindex.Config.set_library_file(str(LLVM_ROOT / "bin" / "libclang.dll"))
    return cindex


@lru_cache(maxsize=None)
def resolved_path(name: str) -> Path:
    return Path(name).resolve()


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def display_name(name: str, boolean: bool = False) -> str:
    if boolean and len(name) > 1 and name[0] == "b" and name[1].isupper():
        name = name[1:]
    name = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name).replace("_", " ")


def write_changed(path: Path, text: str) -> None:
    data = text.encode("utf-8")
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def fingerprint(paths: list[Path]) -> str:
    result = hashlib.sha256(LLVM_VERSION.encode())
    for path in sorted(set(paths)):
        result.update(str(path).encode())
        result.update(path.read_bytes() if path.is_file() else b"<missing>")
    return result.hexdigest()


def qualified(cursor) -> str:
    names = [cursor.spelling]
    parent = cursor.semantic_parent
    while parent and parent.kind.name != "TRANSLATION_UNIT":
        if parent.spelling:
            names.append(parent.spelling)
        parent = parent.semantic_parent
    return "::".join(reversed(names))


@dataclass
class Marker:
    name: str
    start: int
    end: int
    line: int
    options: dict[str, str | bool]
    tooltip: str
    used: bool = False


@dataclass
class ReflectedType:
    name: str
    kind: str
    cursor: object
    path: Path
    marker: Marker | None
    parent: str | None = None

    @property
    def symbol(self) -> str:
        return "KnotType_" + hashlib.sha256(self.name.encode()).hexdigest()[:16]


class HeaderTool:
    CORE_TYPES = {"UObject", "UField", "UStruct", "UClass", "UScriptStruct", "UFunction", "UEnum"}
    MARKERS = {"UPROPERTY", "UFUNCTION", "UCLASS", "USTRUCT", "UENUM"}
    BODY_MARKERS = {"GENERATED_CLASS", "GENERATED_STRUCT"}

    def __init__(self, clang, headers: list[Path], source: Path):
        self.clang = clang
        self.headers = set(headers)
        self.source = source
        self.sources = {p: p.read_bytes() for p in headers}
        self.markers: dict[Path, list[Marker]] = {}
        self.types: dict[str, ReflectedType] = {}
        self.records = []
        self.body_markers = {}
        self.constructible = {}
        self.includes: set[str] = set()

    def error(self, cursor, message: str):
        location = cursor.location
        raise RuntimeError(f"{location.file}({location.line},{location.column}): error KHT: {message}")

    def tooltip(self, path: Path, line: int, start: int) -> str:
        prefix = self.sources[path][:start].decode("utf-8-sig")
        block = re.search(r"/\*\*(?!\*)(?:(?!\*/)[\s\S])*\*/[ \t]*(?:\r?\n[ \t]*)?\Z", prefix)
        if block:
            comments = block.group().strip()[3:-2].splitlines()
            comments = [re.sub(r"^\s*\* ?", "", text).strip() for text in comments]
            while comments and not comments[0]:
                comments.pop(0)
        else:
            if prefix.split("\n")[-1].strip():
                return ""
            lines = self.sources[path].decode("utf-8-sig").splitlines()
            comments = []
            for previous in reversed(lines[:line - 1]):
                stripped = previous.lstrip()
                if not stripped.startswith("///") or stripped.startswith("////"):
                    break
                comments.append(stripped[3:].strip())
            comments.reverse()
        paragraph = []
        for text in comments:
            if not text:
                break
            paragraph.append(text)
        return "\n".join(paragraph)

    def parse_options(self, cursor) -> dict[str, str | bool]:
        tokens = [token.spelling for token in cursor.get_tokens()]
        if len(tokens) < 3 or tokens[1] != "(" or tokens[-1] != ")":
            self.error(cursor, "Use a direct reflection marker invocation.")
        parts = []
        group = []
        for token in tokens[2:-1]:
            if token == ",":
                if not group:
                    self.error(cursor, "Empty marker option.")
                parts.append(group)
                group = []
            else:
                group.append(token)
        if group:
            parts.append(group)
        elif parts:
            self.error(cursor, "Trailing marker comma.")
        options = {}
        for part in parts:
            key = part[0]
            if key in options:
                self.error(cursor, f"Duplicate option: {key}")
            if len(part) == 1 and key in {"NoEdit", "Transient"} and cursor.spelling == "UPROPERTY":
                options[key] = True
            elif len(part) == 3 and part[1] == "=" and key in {"Category", "DisplayName"}:
                try:
                    value = json.loads(part[2])
                except (ValueError, TypeError):
                    self.error(cursor, f"{key} requires a plain quoted string.")
                if not isinstance(value, str):
                    self.error(cursor, f"{key} requires a string.")
                options[key] = value
            else:
                self.error(cursor, f"Unsupported option {key}. Tooltips use adjacent /// or /** */ comments.")
        return options

    def get_marker(self, cursor, expected: str) -> Marker | None:
        path = resolved_path(cursor.location.file.name)
        start = cursor.extent.start.offset
        for marker in reversed(self.markers.get(path, [])):
            if marker.end > start:
                continue
            gap = self.sources[path][marker.end:start].decode("utf-8")
            gap = re.sub(r"/\*.*?\*/|//[^\n]*", "", gap, flags=re.S)
            if gap.strip():
                return None
            if marker.name != expected:
                self.error(cursor, f"{marker.name} cannot annotate this declaration; expected {expected}.")
            if marker.used:
                self.error(cursor, "Use one marker and one member declaration; comma-separated members are unsupported.")
            marker.used = True
            return marker
        return None

    def derives_object(self, cursor, visited=None) -> bool:
        if qualified(cursor) == "UObject":
            return True
        visited = set() if visited is None else visited
        identity = cursor.get_usr()
        if identity in visited:
            return False
        visited.add(identity)
        return any(self.derives_object(c.type.get_declaration(), visited) for c in cursor.get_children() if c.kind.name == "CXX_BASE_SPECIFIER")

    def collect(self, translation_unit) -> None:
        for cursor in translation_unit.cursor.get_children():
            if cursor.kind.name != "MACRO_INSTANTIATION" or cursor.spelling not in self.MARKERS | self.BODY_MARKERS or not cursor.location.file:
                continue
            path = resolved_path(cursor.location.file.name)
            if path not in self.headers:
                continue
            if cursor.spelling in self.BODY_MARKERS:
                self.body_markers.setdefault(path, []).append(cursor)
                continue
            marker = Marker(cursor.spelling, cursor.extent.start.offset, cursor.extent.end.offset, cursor.location.line,
                            self.parse_options(cursor), self.tooltip(path, cursor.location.line, cursor.extent.start.offset))
            self.markers.setdefault(path, []).append(marker)
        for markers in self.markers.values():
            markers.sort(key=lambda m: m.start)

        def visit(cursor):
            if cursor.location.file and resolved_path(cursor.location.file.name) not in self.headers:
                return
            kind = cursor.kind.name
            if kind in {"STRUCT_DECL", "CLASS_DECL", "ENUM_DECL"} and cursor.is_definition():
                self.records.append(cursor)
            if kind in {"TRANSLATION_UNIT", "NAMESPACE", "STRUCT_DECL", "CLASS_DECL"}:
                for child in cursor.get_children():
                    visit(child)
        visit(translation_unit.cursor)
        for cursor in self.records:
            name = qualified(cursor)
            path = resolved_path(cursor.location.file.name)
            enum = cursor.kind.name == "ENUM_DECL"
            object_type = not enum and self.derives_object(cursor)
            marker = self.get_marker(cursor, "UENUM" if enum else "UCLASS" if object_type else "USTRUCT")
            if name in self.CORE_TYPES:
                if marker:
                    self.error(cursor, "This type already has builtin registration.")
                continue
            if not object_type and not marker:
                continue
            if not cursor.spelling or cursor.semantic_parent.kind.name not in {"NAMESPACE", "TRANSLATION_UNIT"}:
                self.error(cursor, "Reflected types must be named namespace-scope definitions.")
            parent_scope = cursor.semantic_parent
            while parent_scope.kind.name != "TRANSLATION_UNIT":
                if not parent_scope.spelling:
                    self.error(cursor, "Anonymous namespace types are unsupported.")
                parent_scope = parent_scope.semantic_parent
            bases = [c for c in cursor.get_children() if c.kind.name == "CXX_BASE_SPECIFIER"]
            if len(bases) > 1 or any(c.access_specifier.name != "PUBLIC" or "virtual" in [t.spelling for t in c.get_tokens()] for c in bases):
                self.error(cursor, "Only public single non-virtual inheritance is supported.")
            parent = qualified(bases[0].type.get_declaration()) if bases else None
            if not enum:
                expected = "GENERATED_CLASS" if object_type else "GENERATED_STRUCT"
                bodies = [m for m in self.body_markers.get(path, []) if cursor.extent.start.offset < m.extent.start.offset < cursor.extent.end.offset]
                if len(bodies) != 1 or bodies[0].spelling != expected:
                    self.error(cursor, f"{name} requires one {expected} declaration.")
                tokens = [t.spelling for t in bodies[0].get_tokens()]
                if len(tokens) < 4 or tokens[2] != cursor.spelling:
                    self.error(bodies[0], "The generated body must name its enclosing type.")
            self.types[name] = ReflectedType(name, "class" if object_type else "enum" if enum else "struct", cursor, path, marker, parent)
        folded = {name.casefold() for name in self.CORE_TYPES}
        for item in self.types.values():
            if item.name.casefold() in folded:
                self.error(item.cursor, "Reflection names must be unique under FName case-insensitive comparison.")
            folded.add(item.name.casefold())
            if item.parent and item.parent not in self.types and item.parent not in self.CORE_TYPES:
                self.error(item.cursor, f"Base type is not registered: {item.parent}")

    def property_expression(self, value_type, name: str, owner: str, offset: str, flags: str, cursor, allow_const: bool = False) -> str:
        value_type = value_type.get_canonical()
        if value_type.is_const_qualified() and not allow_const or value_type.is_volatile_qualified():
            self.error(cursor, "const/volatile properties are unsupported.")
        kind = value_type.kind.name
        common = f"FName({cpp_string(name)}), {owner}, {offset}"
        numeric = {"INT": "FIntProperty", "BOOL": "FBoolProperty", "FLOAT": "FFloatProperty", "DOUBLE": "FDoubleProperty"}
        if kind in numeric:
            self.includes.add("NumericProperty")
            return f"std::make_unique<{numeric[kind]}>({common}, 1, {flags})"
        declaration = value_type.get_declaration()
        declared_name = qualified(declaration)
        if declared_name == "FName" or declared_name == "std::basic_string" and re.sub(r"^const\s+", "", value_type.spelling) == "std::basic_string<char>":
            prop = "Name" if declared_name == "FName" else "String"
            self.includes.add(prop + "Property")
            return f"std::make_unique<F{prop}Property>({common}, 1, {flags})"
        if declared_name in {"TObjectPtr", "TSoftObjectPtr"}:
            target = value_type.get_template_argument_type(0)
            target_name = qualified(target.get_declaration())
            if target_name not in self.CORE_TYPES and (target_name not in self.types or self.types[target_name].kind != "class"):
                self.error(cursor, f"Object reference target is not registered: {target.spelling}")
            if target.is_const_qualified() or target.is_volatile_qualified():
                self.error(cursor, "Qualified object reference targets are unsupported.")
            soft = declared_name == "TSoftObjectPtr"
            prop = "SoftObjectProperty" if soft else "ObjectProperty"
            self.includes.add(prop)
            ops = "GetTSoftObjectPtrOps" if soft else "GetTObjectPtrOps"
            return f"std::make_unique<F{prop}>({common}, {target_name}::StaticClass(), {ops}<{target_name}>(), 1, {flags})"
        if declared_name == "std::vector":
            element = value_type.get_template_argument_type(0)
            allocator = value_type.get_template_argument_type(1)
            if qualified(allocator.get_declaration()) != "std::allocator" or allocator.get_template_argument_type(0) != element:
                self.error(cursor, "Only the default TArray allocator is supported.")
            if element.kind.name == "BOOL":
                self.error(cursor, "TArray<bool> proxy elements are unsupported.")
            self.includes.add("ArrayProperty")
            inner = self.property_expression(element, name + "_Element", owner, "0", "EPropertyFlags::None", cursor)
            return f"std::make_unique<FArrayProperty>({common}, static_cast<uint32>(sizeof({value_type.spelling})), GetArrayOps<{element.spelling}>(), {inner}, {flags})"
        reflected = self.types.get(declared_name)
        if reflected and reflected.kind in {"struct", "enum"}:
            enum = reflected is not None and reflected.kind == "enum"
            prop = "EnumProperty" if enum else "StructProperty"
            lookup = f"StaticEnum<{declared_name}>()" if enum else f"{declared_name}::StaticStruct()"
            self.includes.add(prop)
            return f"std::make_unique<F{prop}>({common}, {lookup}, 1, {flags})"
        self.error(cursor, f"Unsupported reflected type: {value_type.spelling}. Raw UObject pointers require TObjectPtr<T>.")

    def metadata(self, variable: str, name: str, category: str, marker: Marker | None, boolean: bool = False) -> str:
        options = marker.options if marker else {}
        display = options.get("DisplayName", display_name(name, boolean))
        category = options.get("Category", category)
        tooltip = marker.tooltip if marker else ""
        return f"{variable}->GetMetadata() = FReflectionMetadata({cpp_string(display)}, {cpp_string(category)}, {cpp_string(tooltip)});"

    def members(self, record, unions=()):
        children = list(record.get_children())
        for child in children:
            branch = unions + ((record.hash, child.hash),) if record.kind.name == "UNION_DECL" else unions
            if child.kind.name in {"STRUCT_DECL", "UNION_DECL"} and child.is_anonymous():
                # A named field of an unnamed type is not an injected anonymous member.
                if not any(c.kind.name == "FIELD_DECL" and c.type.get_declaration() == child for c in children):
                    yield from self.members(child, branch)
            else:
                yield child, branch

    def generate_type(self, item: ReflectedType) -> str:
        name = item.name
        category = item.cursor.spelling
        lines = [f"template <>\nstruct TReflectionAccess<{name}>\n{{"]
        if item.kind == "enum":
            lines += ["\tinline static UEnum* StaticEnumPrivate = nullptr;"]
        populate = []
        functions = []
        selected_unions = {}
        property_names = set()
        for member, unions in self.members(item.cursor):
            kind = member.kind.name
            if kind not in {"FIELD_DECL", "CXX_METHOD", "VAR_DECL", "FUNCTION_TEMPLATE", "CONSTRUCTOR", "DESTRUCTOR"}:
                continue
            marker = self.get_marker(member, "UPROPERTY" if kind in {"FIELD_DECL", "VAR_DECL"} else "UFUNCTION")
            if not marker:
                continue
            if kind not in {"FIELD_DECL", "CXX_METHOD"}:
                self.error(member, "Only non-static fields and ordinary member functions can be reflected.")
            if kind == "CXX_METHOD":
                if item.kind != "class":
                    self.error(member, "Only UObject member functions can be reflected.")
                functions.append((member, marker))
                continue
            if member.is_bitfield():
                self.error(member, "Bit-field properties are unsupported.")
            for union, branch in unions:
                if union in selected_unions and selected_unions[union] != branch:
                    self.error(member, "UPROPERTY may select only one storage branch of an anonymous union.")
                selected_unions[union] = branch
            prop_name = member.spelling
            if prop_name.casefold() in property_names:
                self.error(member, "Property names must be unique under FName case-insensitive comparison.")
            property_names.add(prop_name.casefold())
            base = item.parent
            while base in self.types:
                ancestor = self.types[base]
                if any(c.kind.name == "FIELD_DECL" and c.spelling.casefold() == prop_name.casefold() for c, _ in self.members(ancestor.cursor)):
                    self.error(member, "Inherited property name hiding is unsupported.")
                base = ancestor.parent
            flags = ["EPropertyFlags::NoEdit"] if marker.options.get("NoEdit") else []
            if marker.options.get("Transient"):
                flags.append("EPropertyFlags::Transient")
            variable = f"Property{len(populate)}"
            expression = self.property_expression(member.type, prop_name, "Schema", f"static_cast<uint32>(offsetof({name}, {prop_name}))",
                                                  " | ".join(flags) or "EPropertyFlags::None", member)
            populate += [f"auto {variable} = {expression};", self.metadata(variable, prop_name, category, marker, member.type.get_canonical().kind.name == "BOOL"),
                         f"Schema->AddProperty(std::move({variable}));"]
        for method, marker in functions:
            generated, registration = self.generate_function(item, method, marker)
            lines.extend(generated)
            populate.extend(registration)
        lines += ["\tstatic void Register(FReflectionRegistry& Registry)", "\t{"]
        if item.kind == "class":
            flags = "EClassFlags::Abstract" if item.cursor.is_abstract_record() else "EClassFlags::None"
            parent = f"{item.parent}::StaticClass()" if item.parent else "nullptr"
            factory = "&CreateObject" if self.constructible.get(name) else "nullptr"
            construction = f"std::make_unique<UClass>(FName({cpp_string(name)}), {parent}, sizeof({name}), alignof({name}), {flags}, {factory})"
            lines += [f"\t\tstatic_assert(std::is_same_v<{name}::Super, {item.parent}>);"]
            lines += [f"\t\tauto Schema = {construction};"]
        elif item.kind == "struct":
            parent = f"{item.parent}::StaticStruct()" if item.parent else "nullptr"
            lines += [f"\t\tstatic_assert(std::is_default_constructible_v<{name}> && std::is_copy_assignable_v<{name}>);",
                      f"\t\tauto Schema = std::make_unique<UScriptStruct>(FName({cpp_string(name)}), sizeof({name}), alignof({name}), GetStructOps<{name}>(), nullptr, {parent});"]
        else:
            values = []
            for value in item.cursor.get_children():
                if value.kind.name == "ENUM_CONSTANT_DECL":
                    if not -(2**63) <= value.enum_value < 2**63:
                        self.error(value, "Enum value must fit the runtime int64 schema.")
                    values.append(f"{{FName({cpp_string(value.spelling)}), {cpp_string(display_name(value.spelling))}, static_cast<int64>({name}::{value.spelling})}}")
            lines += ["\t\tTArray<FEnumValue> Values = {" + ", ".join(values) + "};",
                      f"\t\tauto Schema = std::make_unique<UEnum>(FName({cpp_string(name)}), static_cast<uint8>(sizeof({name})), std::move(Values));"]
        lines += ["\t\t" + self.metadata("Schema", category, category, item.marker)]
        registration = {"class": "RegisterClass", "struct": "RegisterScriptStruct", "enum": "RegisterEnum"}[item.kind]
        assignment = f"{name}::StaticClassPrivate = " if item.kind == "class" else f"{name}::StaticStructPrivate = " if item.kind == "struct" else "StaticEnumPrivate = "
        lines += [f"\t\t{assignment}Registry.{registration}(std::move(Schema));", "\t}", "", "\tstatic void Populate()", "\t{"]
        if populate:
            lookup = "StaticClass" if item.kind == "class" else "StaticStruct"
            lines += [f"\t\tauto* Schema = {name}::{lookup}();"] + ["\t\t" + line for line in populate]
        lines += ["\t}"]
        if item.kind == "class" and self.constructible.get(name):
            lines += ["\tstatic UObject* CreateObject(UClass* Class)", "\t{", f"\t\tpanic(Class == {name}::StaticClass());",
                      f"\t\treturn GUObjectManager.Create<{name}>();", "\t}"]
        lines += ["};", ""]
        if item.kind != "enum":
            schema_type = "UClass" if item.kind == "class" else "UScriptStruct"
            entry = "StaticClass" if item.kind == "class" else "StaticStruct"
            lines += [f"{schema_type}* {name}::{entry}Private = nullptr;", "", f"{schema_type}* {name}::{entry}()", "{",
                      f"\tpanic({entry}Private);", f"\treturn {entry}Private;", "}", ""]
        if item.kind == "enum":
            lines += [f"template <> UEnum* StaticEnum<{name}>()", "{",
                      f"\tpanic(TReflectionAccess<{name}>::StaticEnumPrivate);",
                      f"\treturn TReflectionAccess<{name}>::StaticEnumPrivate;", "}", ""]
        return "\n".join(lines)

    def generate_function(self, item: ReflectedType, method, marker: Marker):
        if method.is_static_method() or method.type.is_function_variadic() or method.spelling.startswith("operator"):
            self.error(method, "Static, variadic and operator functions are unsupported.")
        if method.type.get_ref_qualifier().name != "NONE" or "volatile" in method.type.spelling:
            self.error(method, "Ref-qualified or volatile member functions are unsupported.")
        overloads = [m for m in item.cursor.get_children() if m.spelling == method.spelling and m.kind.name == "CXX_METHOD"]
        if len(overloads) != 1:
            self.error(method, "Reflected function overloads are unsupported.")
        base = item.parent
        while base in self.types:
            if any(m.spelling == method.spelling and m.kind.name == "CXX_METHOD" for m in self.types[base].cursor.get_children()):
                self.error(method, "Register an inherited function only on its declaring class.")
            base = self.types[base].parent
        self.includes.add("@Function")
        params = []
        call_args = []
        for index, argument in enumerate(method.get_arguments()):
            value_type = argument.type.get_canonical()
            flags = "EPropertyFlags::Parameter"
            if value_type.kind.name == "LVALUEREFERENCE":
                value_type = value_type.get_pointee()
                if not value_type.is_const_qualified():
                    flags += " | EPropertyFlags::OutParameter"
            elif value_type.kind.name == "RVALUEREFERENCE":
                self.error(argument, "Rvalue reference parameters are unsupported.")
            # Const input storage is mutable in the parameter frame; classify the unqualified type through Clang.
            params.append((f"P{index}", argument.spelling or f"Arg{index}", value_type, flags, argument))
            call_args.append(f"Values->P{index}")
        result = method.result_type.get_canonical()
        has_return = result.kind.name != "VOID"
        if has_return:
            params.append(("Result", "ReturnValue", result, "EPropertyFlags::Parameter | EPropertyFlags::ReturnParameter", method))
        if len({p[1].casefold() for p in params}) != len(params):
            self.error(method, "Parameter names must be unique and must not collide with ReturnValue.")
        frame = "FParams_" + method.spelling
        lines = []
        if params:
            lines += [f"\tstruct {frame}", "\t{"]
            for storage, _, value_type, _, cursor in params:
                spelling = value_type.spelling
                if value_type.is_volatile_qualified():
                    self.error(cursor, "Volatile function parameters are unsupported.")
                if value_type.is_const_qualified():
                    spelling = re.sub(r"^const\s+", "", spelling)
                lines += [f"\t\t{spelling} {storage}{{}};"]
            lines += ["\t};"]
        lines += [f"\tstatic void Invoke_{method.spelling}(UObject* Context, void* Params)", "\t{"]
        if params:
            lines += [f"\t\tauto* Values = static_cast<{frame}*>(Params);"]
        else:
            lines += ["\t\t(void)Params;"]
        call = f"static_cast<{item.name}*>(Context)->{method.spelling}({', '.join(call_args)})"
        lines += ["\t\t" + ("Values->Result = " if has_return else "") + call + ";", "\t}"]
        flags = "EFunctionFlags::Native | EFunctionFlags::Callable" + (" | EFunctionFlags::Const" if method.is_const_method() else "")
        size, alignment, ops = (f"sizeof({frame})", f"alignof({frame})", f"GetStructOps<{frame}>()") if params else ("0", "1", "nullptr")
        var = "Function_" + method.spelling
        registration = [f"auto {var} = std::make_unique<UFunction>(FName({cpp_string(method.spelling)}), Schema, {size}, {alignment}, {flags}, {ops}, &Invoke_{method.spelling});",
                        self.metadata(var, method.spelling, item.cursor.spelling, marker)]
        for storage, external, value_type, param_flags, cursor in params:
            expression = self.property_expression(value_type, external, var + ".get()", f"static_cast<uint32>(offsetof({frame}, {storage}))", param_flags, cursor, allow_const=True)
            registration += [f"{var}->AddProperty({expression});"]
        registration += [f"Schema->AddFunction(std::move({var}));"]
        return lines, registration

    def generate(self, output: Path) -> list[Path]:
        ordered = []
        pending = dict(self.types)
        while pending:
            ready = sorted(name for name, item in pending.items() if item.parent not in pending)
            if not ready:
                raise RuntimeError("Cyclic reflection inheritance.")
            for name in ready:
                ordered.append(pending.pop(name))
        self.includes = set()
        content = "\n".join(self.generate_type(item) for item in ordered)
        for path, markers in self.markers.items():
            for marker in markers:
                if not marker.used:
                    raise RuntimeError(f"{path}({marker.line}): error KHT: Unattached {marker.name}; check the declaration kind and reflected owner.")
        prefix = "// Generated by KnotHeaderTool. Do not edit.\n"
        prefix += "".join(f'#include "{path.as_posix()}"\n' for path in sorted({item.path for item in ordered}))
        prefix += '#include "Object/Class.h"\n#include "Object/Reflection/ReflectionRegistry.h"\n'
        if "@Function" in self.includes:
            prefix += '#include "Object/Function.h"\n'
        prefix += "".join(f'#include "Object/Property/{p}.h"\n' for p in sorted(self.includes) if not p.startswith("@"))
        prefix += "#include <cstddef>\n\n"
        prefix += "".join(f"template <> UEnum* StaticEnum<{item.name}>();\n" for item in ordered if item.kind == "enum")
        content += "\nvoid FReflectionRegistry::RegisterStaticClass()\n{\n"
        content += "".join(f"\tTReflectionAccess<{item.name}>::Register(*this);\n" for item in ordered)
        content += "\n"
        content += "".join(f"\tTReflectionAccess<{item.name}>::Populate();\n" for item in ordered)
        content += "}\n\nvoid FReflectionRegistry::ResetStaticClass()\n{\n"
        for item in ordered:
            if item.kind != "enum":
                entry = "StaticClass" if item.kind == "class" else "StaticStruct"
                content += f"\t{item.name}::{entry}Private = nullptr;\n"
            else:
                content += f"\tTReflectionAccess<{item.name}>::StaticEnumPrivate = nullptr;\n"
        content += "}\n"
        generated_path = output / "Reflection.gen.cpp"
        write_changed(generated_path, prefix + content)
        return [generated_path]



def compiler_arguments(environment: dict[str, list[str]]) -> list[str]:
    import winreg
    compiler = Path(environment["compiler"][0])
    msvc_include = compiler.parents[3] / "include"
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots", 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
        sdk_root = Path(winreg.QueryValueEx(key, "KitsRoot10")[0])
    sdk_include = sdk_root / "Include" / environment["sdk"][0]
    system_includes = [msvc_include] + [sdk_include / part for part in ("ucrt", "shared", "um", "winrt", "cppwinrt")]
    if not all(path.is_dir() for path in system_includes):
        raise RuntimeError("The configured MSVC/Windows SDK include directories are unavailable.")
    args = ["-x", "c++", "-std=c++20", "--target=x86_64-pc-windows-msvc", "-fms-extensions", "-fms-compatibility", "-fms-compatibility-version=" + environment["compiler_version"][0],
            "-resource-dir", str(LLVM_ROOT / "lib" / "clang" / "20"), "-Wno-pragma-once-outside-header"]
    args += ["-D_MT", "-D_DLL"]
    for flag in shlex.split(environment.get("flags", [""])[0], posix=False):
        if flag.startswith(("/D", "-D", "/U", "-U")):
            args.append("-" + flag[1:])
        elif flag.upper() in {"/MDD", "/MTD"}:
            args.append("-D_DEBUG")
    if environment.get("configuration") == ["Debug"]:
        args.append("-D_DEBUG")
    for path in system_includes:
        args += ["-isystem", str(path)]
    for include in environment.get("include", []):
        args += ["-I", include]
    for define in environment.get("define", []):
        args += ["-D", define]
    args += ["-include", environment["pch"][0]]
    return args


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", type=Path, required=True)
    parser.add_argument("--headers", type=Path, required=True)
    parser.add_argument("--force", action="store_true")
    options = parser.parse_args()
    environment = {}
    for line in options.environment.read_text(encoding="utf-8-sig").splitlines():
        key, value = line.split("=", 1)
        if value:
            environment.setdefault(key, []).append(value)
    headers = [Path(line).resolve() for line in options.headers.read_text(encoding="utf-8-sig").splitlines() if line]
    source = Path(environment["source"][0]).resolve()
    output = options.environment.parent
    state_path = output / "ReflectionState.json"
    inputs = [options.environment, options.headers, Path(__file__), Path(__file__).with_name("Toolchain.py")]
    if state_path.is_file() and not options.force:
        state = json.loads(state_path.read_text(encoding="utf-8"))
        dependencies = [Path(p) for p in state["dependencies"]]
        outputs = [Path(p) for p in state["outputs"]]
        if fingerprint(inputs + headers + dependencies) == state["input_hash"] and fingerprint(outputs) == state["output_hash"]:
            return 0
    clang = load_clang()
    index = clang.Index.create()
    translation_source = output / "ReflectionInput.cpp"
    source_text = "".join(f'#include "{path.as_posix()}"\n' for path in headers if path.suffix in {".h", ".hpp", ".hxx", ".hh"})
    translation_unit = index.parse(str(translation_source), args=compiler_arguments(environment), unsaved_files=[(str(translation_source), source_text)],
                                   options=clang.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | clang.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
    errors = [str(d) for d in translation_unit.diagnostics if d.severity >= clang.Diagnostic.Error]
    if errors:
        raise RuntimeError("Clang declaration analysis failed:\n" + "\n".join(errors))
    tool = HeaderTool(clang, headers, source)
    tool.collect(translation_unit)
    # Let Clang evaluate constructibility rather than guessing from constructor spelling.
    queries = {item.symbol: item.name for item in tool.types.values() if item.kind == "class"}
    probe = source_text + "\n".join(f"enum {{ {symbol} = __is_constructible({name}) && !__is_abstract({name}) }};" for symbol, name in queries.items())
    translation_unit.reparse(unsaved_files=[(str(translation_source), probe)])
    errors = [str(d) for d in translation_unit.diagnostics if d.severity >= clang.Diagnostic.Error]
    if errors:
        raise RuntimeError("Clang constructor analysis failed:\n" + "\n".join(errors))
    tool = HeaderTool(clang, headers, source)
    tool.collect(translation_unit)
    for cursor in translation_unit.cursor.get_children():
        if cursor.kind.name == "ENUM_DECL" and cursor.location.file and resolved_path(cursor.location.file.name) == translation_source.resolve():
            for value in cursor.get_children():
                if value.spelling in queries:
                    tool.constructible[queries[value.spelling]] = bool(value.enum_value)
    outputs = tool.generate(output)
    dependencies = sorted({Path(entry.include.name).resolve() for entry in translation_unit.get_includes()})
    state = {"dependencies": [str(p) for p in dependencies], "outputs": [str(p) for p in outputs],
             "input_hash": fingerprint(inputs + headers + dependencies), "output_hash": fingerprint(outputs)}
    write_changed(state_path, json.dumps(state, indent=2))
    print(f"KnotHeaderTool: registered {len(tool.types)} types from {len(headers)} headers.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, ValueError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
