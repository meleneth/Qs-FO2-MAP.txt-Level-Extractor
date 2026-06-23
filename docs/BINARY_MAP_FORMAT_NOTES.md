# Binary MAP Format Notes

These notes track parser decisions that are grounded in public Fallout/Fallout 2 MAP
format references, plus fields that are still preserved as unknown data.

References:

- https://falloutmods.fandom.com/wiki/MAP_File_Format
- https://fodev.net/files/fo2/map.html

## Object Layout Decisions

The object parser treats object records as:

- A fixed 0x58-byte prefix.
- A PID/object-kind dependent tail.
- Zero or more inventory entries when the prefix inventory count is non-zero.

Inventory entries are parsed as a 4-byte quantity followed by another complete
object record. Nested object records are included in the owning object's raw
range and stored in `BinaryObjectRecord::inventory`.

Current modeled tail sizes are the byte counts the parser consumes today, not a
claim that every subtype layout is fully decoded:

| Object kind | PID high byte | Modeled tail bytes | Fixture coverage |
| --- | ---: | ---: | --- |
| item | 0 | 0 | `parse_binary_map_object_records parses inventory object records` |
| critter | 1 | 44 | `parse_binary_critter_tail decodes preserved critter tail fields` |
| scenery | 2 | 12 | `parse_binary_scenery_tail decodes preserved scenery tail fields` |
| wall | 3 | 0 | Object prefix/type coverage only |
| tile | 4 | 0 | Object prefix/type coverage only |
| misc | 5 | 0 | `parse_binary_misc_tail decodes preserved misc tail fields` |
| interface | 6 | 0 | Object prefix/type coverage only |
| inventory | 7 | 0 | Nested inventory structure coverage |
| head | 8 | 0 | Object prefix/type coverage only |
| background | 9 | 0 | Object prefix/type coverage only |

Item and scenery subtype tails are not fully modeled because the MAP object
prefix only carries the PID. Correctly choosing ammo/key/misc item/weapon or
door/ladder/exit-grid layouts requires prototype-level subtype knowledge that
this parser does not load yet.

## Unknown Fields

| Model field | Offset | Size | Current observed fixture values | Coverage |
| --- | ---: | ---: | --- | --- |
| `BinaryMapHeader::unknown[44]` | 0x003C | 176 | Synthetic header uses 0..43 | `parse_binary_map_header reads typed header fields` |
| `BinaryScriptRecord::unknown_1` | base script offset 0x0038 | 4 | Synthetic scripts use 0 | `parse_binary_map_scripts reads script records and block footers` |
| `BinaryScriptRecord::how_much` | base script offset 0x003C | 4 | Synthetic scripts use 0 | `parse_binary_map_scripts reads script records and block footers` |
| `BinaryScriptRecord::unknown_2` | base script offset 0x0040 | 4 | Synthetic scripts use 0 | `parse_binary_map_scripts reads script records and block footers` |
| `BinaryObjectPrefix::obj_id` | object offset 0x0000 | 4 | Synthetic objects use 100, 101, 200, 300 | Object prefix and object record tests |
| `BinaryObjectPrefix::x` | object offset 0x0008 | 4 | Synthetic objects use 1 | Object prefix and object record tests |
| `BinaryObjectPrefix::y` | object offset 0x000C | 4 | Synthetic objects use 2 | Object prefix and object record tests |
| `BinaryObjectPrefix::screen_x` | object offset 0x0010 | 4 | Synthetic objects use 3 | Object prefix and object record tests |
| `BinaryObjectPrefix::screen_y` | object offset 0x0014 | 4 | Synthetic objects use 4 | Object prefix and object record tests |
| `BinaryObjectPrefix::unknown_10` | object offset 0x0050 | 4 | Synthetic objects use 901 | CLI binary stats and prefix tests |
| `BinaryObjectPrefix::unknown_11` | object offset 0x0054 | 4 | Synthetic objects use 902 | CLI binary stats and prefix tests |

The parser preserves raw ranges for complete object records and type-specific
tails, so unsupported fields can still be round-tripped once binary export is
implemented.
