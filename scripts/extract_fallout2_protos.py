#!/usr/bin/env python3
"""Extract Fallout 2 prototype files from DAT2 archives.

This intentionally extracts only the prototype files needed for PID/subtype
resolution. Do not commit the output directory; it contains game assets.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import struct
import sys
import zlib


DEFAULT_STEAM_ROOT = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\Fallout 2"
)

PROTO_DIRS = {
    "proto\\items\\",
    "proto\\critters\\",
    "proto\\scenery\\",
    "proto\\walls\\",
    "proto\\tiles\\",
    "proto\\misc\\",
}


class DatError(RuntimeError):
    pass


class DatEntry:
    def __init__(
        self,
        path: str,
        compressed: bool,
        uncompressed_size: int,
        data_size: int,
        data_offset: int,
    ) -> None:
        self.path = path
        self.compressed = compressed
        self.uncompressed_size = uncompressed_size
        self.data_size = data_size
        self.data_offset = data_offset


def read_u32_le(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise DatError(f"u32 read out of range at {offset}")
    return struct.unpack_from("<I", data, offset)[0]


def is_needed_proto_path(path: str) -> bool:
    normalized = path.replace("/", "\\").lower()
    if not any(normalized.startswith(prefix) for prefix in PROTO_DIRS):
        return False
    return normalized.endswith(".pro") or normalized.endswith(".lst")


def safe_output_path(output_root: Path, archive_path: str) -> Path:
    parts = [
        part
        for part in archive_path.replace("/", "\\").split("\\")
        if part and part not in {".", ".."}
    ]
    if not parts:
        raise DatError(f"empty archive path {archive_path!r}")
    target = output_root.joinpath(*parts)
    resolved_root = output_root.resolve()
    resolved_target_parent = target.parent.resolve()
    if resolved_root != resolved_target_parent and resolved_root not in resolved_target_parent.parents:
        raise DatError(f"refusing to write outside output root: {archive_path}")
    return target


class Dat2Archive:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.bytes = path.read_bytes()
        self.data_base = 0
        self.entries = self._read_entries()

    def _read_entries(self) -> list[DatEntry]:
        data = self.bytes
        if len(data) < 8:
            raise DatError(f"{self.path} is too small to be a DAT2 archive")

        tree_size = read_u32_le(data, len(data) - 8)
        data_size = read_u32_le(data, len(data) - 4)
        if data_size > len(data):
            raise DatError(f"{self.path} has invalid data size {data_size}")
        if tree_size < 4 or tree_size + 8 > data_size:
            raise DatError(f"{self.path} has invalid tree size {tree_size}")

        self.data_base = len(data) - data_size
        tree_offset = len(data) - tree_size - 8
        tree_end = tree_offset + tree_size
        if tree_offset < 0 or tree_end > len(data):
            raise DatError(f"{self.path} has out-of-range directory tree")

        file_count = read_u32_le(data, tree_offset)
        cursor = tree_offset + 4
        entries: list[DatEntry] = []
        for _ in range(file_count):
            path_length = read_u32_le(data, cursor)
            cursor += 4
            if cursor + path_length + 13 > tree_end:
                raise DatError(f"{self.path} has truncated directory entry")
            raw_path = data[cursor : cursor + path_length]
            cursor += path_length
            path = raw_path.decode("latin-1")
            compressed = data[cursor] != 0
            cursor += 1
            uncompressed_size = read_u32_le(data, cursor)
            cursor += 4
            data_size = read_u32_le(data, cursor)
            cursor += 4
            data_offset = read_u32_le(data, cursor)
            cursor += 4
            entries.append(
                DatEntry(path, compressed, uncompressed_size, data_size, data_offset)
            )

        return entries

    def read_entry(self, entry: DatEntry) -> bytes:
        start = self.data_base + entry.data_offset
        end = start + entry.data_size
        if start < 0 or end > len(self.bytes):
            raise DatError(f"{self.path}:{entry.path} data range is invalid")
        payload = self.bytes[start:end]
        if not entry.compressed:
            if len(payload) != entry.uncompressed_size:
                raise DatError(f"{self.path}:{entry.path} plain size mismatch")
            return payload
        inflated = zlib.decompress(payload)
        if len(inflated) != entry.uncompressed_size:
            raise DatError(f"{self.path}:{entry.path} inflated size mismatch")
        return inflated


def extract_dat(dat_path: Path, output_root: Path, overwrite: bool) -> tuple[int, int]:
    archive = Dat2Archive(dat_path)
    matched = 0
    written = 0
    for entry in archive.entries:
        if not is_needed_proto_path(entry.path):
            continue
        matched += 1
        target = safe_output_path(output_root, entry.path)
        if target.exists() and not overwrite:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(archive.read_entry(entry))
        written += 1
    return matched, written


def copy_loose_protos(data_root: Path, output_root: Path, overwrite: bool) -> int:
    proto_root = data_root / "proto"
    if not proto_root.exists():
        return 0

    copied = 0
    for source in proto_root.rglob("*"):
        if not source.is_file():
            continue
        relative = source.relative_to(data_root)
        normalized = str(relative).replace("/", "\\").lower()
        if not is_needed_proto_path(normalized):
            continue
        target = safe_output_path(output_root, normalized)
        if target.exists() and not overwrite:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.read_bytes())
        copied += 1
    return copied


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Extract Fallout 2 proto/*.pro and proto/*.lst files."
    )
    parser.add_argument(
        "--fallout2-root",
        type=Path,
        default=DEFAULT_STEAM_ROOT,
        help=f"Fallout 2 install directory. Default: {DEFAULT_STEAM_ROOT}",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=Path(".local_fallout2_data"),
        help="Output directory. Default: .local_fallout2_data",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing extracted files.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    game_root = args.fallout2_root
    output_root = args.out

    if not game_root.exists():
        print(f"Fallout 2 root does not exist: {game_root}", file=sys.stderr)
        return 2

    dats = [game_root / "master.dat", game_root / "critter.dat"]
    missing = [str(path) for path in dats if not path.exists()]
    if missing:
        print("Missing DAT archive(s):", file=sys.stderr)
        for path in missing:
            print(f"  {path}", file=sys.stderr)
        return 2

    output_root.mkdir(parents=True, exist_ok=True)
    total_matched = 0
    total_written = 0
    for dat_path in dats:
        matched, written = extract_dat(dat_path, output_root, args.overwrite)
        print(f"{dat_path.name}: matched {matched}, wrote {written}")
        total_matched += matched
        total_written += written

    loose_written = copy_loose_protos(game_root / "data", output_root, True)
    if loose_written:
        print(f"data\\proto loose overrides: wrote {loose_written}")
        total_written += loose_written

    print(f"Done. Matched {total_matched} archive proto files.")
    print(f"Output: {output_root.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
