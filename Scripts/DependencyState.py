import hashlib
import json
from pathlib import Path
from typing import Any


STATE_VERSION = 1


def file_sha256(path: Path) -> str | None:
    if not path.exists():
        return None

    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_vcpkg_install_state(
    manifest_root: Path,
    install_root: Path,
    triplet: str,
    vcpkg_ref: str,
    manifest_files: list[Path],
) -> dict[str, Any]:
    resolved_manifest_root = manifest_root.resolve()
    tracked_files: dict[str, str | None] = {}

    for manifest_file in manifest_files:
        try:
            key = str(manifest_file.resolve().relative_to(resolved_manifest_root)).replace("\\", "/")
        except ValueError:
            key = str(manifest_file.resolve())
        tracked_files[key] = file_sha256(manifest_file)

    return {
        "version": STATE_VERSION,
        "kind": "vcpkg-manifest-install",
        "vcpkg_ref": vcpkg_ref,
        "triplet": triplet,
        "manifest_root": str(resolved_manifest_root),
        "install_root": str(install_root.resolve()),
        "files": tracked_files,
    }


def dependency_state_is_current(
    stamp_file: Path,
    expected_state: dict[str, Any],
    required_paths: list[Path],
) -> bool:
    if not stamp_file.exists():
        return False

    if any(not path.exists() for path in required_paths):
        return False

    try:
        actual_state = json.loads(stamp_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False

    return actual_state == expected_state


def write_dependency_state(stamp_file: Path, state: dict[str, Any]) -> None:
    stamp_file.parent.mkdir(parents=True, exist_ok=True)
    stamp_file.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n", encoding="utf-8")
