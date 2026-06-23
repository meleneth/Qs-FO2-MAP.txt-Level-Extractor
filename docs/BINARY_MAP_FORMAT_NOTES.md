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

Generic modeled tail sizes are the byte counts the parser can determine from
the PID kind alone. Some prototype IDs have documented subtype tails; those are
handled by an explicit fixture-backed resolver in `binary_map_objects.cpp`.

| Object kind | PID high byte | Modeled tail bytes | Fixture coverage |
| --- | ---: | ---: | --- |
| item | 0 | 0 | `parse_binary_map_object_records parses inventory object records` |
| critter | 1 | 40 | `parse_binary_critter_tail decodes preserved critter tail fields`; `test_maps/test16.map` has 128-byte critter records, matching a 0x58-byte prefix plus ten 4-byte fields |
| scenery | 2 | 0 | Generic scenery object coverage; subtype tails require explicit prototype ID rules |
| wall | 3 | 0 | Object prefix/type coverage only |
| tile | 4 | 0 | Object prefix/type coverage only |
| misc | 5 | 0 | Generic misc object coverage; exit-grid-like prototypes use explicit PID rules |
| interface | 6 | 0 | Object prefix/type coverage only |
| inventory | 7 | 0 | Nested inventory structure coverage |
| head | 8 | 0 | Object prefix/type coverage only |
| background | 9 | 0 | Object prefix/type coverage only |

Item and scenery subtype tails are not fully modeled because the MAP object
prefix only carries the PID. Correctly choosing ammo/key/misc item/weapon or
door/ladder/exit-grid layouts requires prototype-level subtype knowledge that
this parser does not load yet.

Prototype metadata work now uses Fallout 2 Community Edition's `proto_types.h`
and object read logic as the reference for item/scenery subtype enum values and
runtime object-data tail sizes. The extracted `.pro` files store their PID at
offset `0x00`; item and scenery subtype values are at offset `0x20`.

The object parser must not infer tails by scanning forward for plausible object
prefixes. Where a PID's subtype is not known, parsing should stop with a useful
diagnostic or add a fixture-backed resolver rule after proving the documented
layout.

Current fixture-backed misc prototype tail rules:

| PID | Tail bytes | Evidence |
| ---: | ---: | --- |
| `0x0500000C` | 44 | `ARVILL2.map` object at offset 44600; `BROKEN1.map` object at offset 88056 |
| `0x0500000E` | 16 | Synthetic regression fixture matching the published four-word misc/exit-grid shape |
| `0x05000010` | 16 | `Newr1.map` object at offset 134884; `Newr2.map` object at offset 134724 |
| `0x05000013` | 16 | `Newr2.map` object at offset 134828 |
| `0x05000017` | 16 | `BROKEN2.map` object at offset 134600 |

Current fixture-backed item prototype tail rules:

| PID | Tail bytes | Evidence |
| ---: | ---: | --- |
| `0x00000121` | 4 | `ARVILL2.map` fixture evidence for a four-byte item subtype |

The following item tail rules are intentionally offset-specific because the
same PID can appear elsewhere without the same payload in current fixture
evidence. They should be replaced with prototype-subtype based resolution once
the parser loads prototype metadata:

| PID | Tail bytes | Evidence |
| ---: | ---: | --- |
| `0x00000016` | 48 | `ARVILL2.map` object at offset 95772 |
| `0x0000004F` | 16 | `BROKEN2.map` object at offset 137036 |
| `0x00000001` | 4 | `Newr1.map` object at offset 154112 |
| `0x00000234` | 60 | `Newr2.map` object at offset 137616 |

The public MAP reference lists ten critter extra fields after the object
prefix: reaction, current movement points, combat results, damage last turn,
AI packet, group id, who-hit-me, current hit points, radiation, and poison.
Older local synthetic tests modeled eleven words; that layout is not used for
normal MAP records unless a future fixture proves a variant.

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
