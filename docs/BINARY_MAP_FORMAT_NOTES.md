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
range and stored in `BinaryObjectRecord::inventory`. The object prefix field
currently named `inventory_size` is the documented maximum inventory slot
capacity at object offset `0x4C`; it is preserved and validated against
negative values, but it is not used to locate inventory bytes. The parser does
not support a direct child-object fallback without the preceding quantity word.

Generic modeled tail sizes are the byte counts the parser can determine from
the PID kind alone. Full binary object parsing requires extracted prototype
metadata so item and scenery subtype tails can be resolved from `.pro` records
instead of guessed from the MAP prefix.

| Object kind | PID high byte | Modeled tail bytes | Fixture coverage |
| --- | ---: | ---: | --- |
| item | 0 | 0 | `parse_binary_map_object_records parses inventory object records` |
| critter | 1 | 40 | `parse_binary_critter_tail decodes preserved critter tail fields`; `test_maps/test16.map` has 128-byte critter records, matching a 0x58-byte prefix plus ten 4-byte fields |
| scenery | 2 | 0 | Generic scenery object coverage; prototype-backed subtype tails are enabled when metadata is supplied |
| wall | 3 | 0 | Object prefix/type coverage only |
| tile | 4 | 0 | Object prefix/type coverage only |
| misc | 5 | 0 | Generic misc object coverage; exit grids use the engine PID range `0x05000010..0x05000017` |
| interface | 6 | 0 | Object prefix/type coverage only |
| inventory | 7 | 0 | Nested inventory structure coverage |
| head | 8 | 0 | Object prefix/type coverage only |
| background | 9 | 0 | Object prefix/type coverage only |

Item and scenery subtype tails are not fully modeled because the MAP object
prefix only carries the PID. Correctly choosing ammo/key/misc item/weapon or
door/ladder layouts requires prototype-level subtype knowledge.

Prototype metadata work now uses Fallout 2 Community Edition's `proto_types.h`
and object read logic as the reference for item/scenery subtype enum values,
misc exit-grid PID bounds, and runtime object-data tail sizes. The extracted
`.pro` files store their PID at offset `0x00`; item and scenery subtype values
are at offset `0x20`. Misc prototypes are the short common misc struct and do
not carry a subtype word; exit-grid object data is selected by the engine PID
range `FIRST_EXIT_GRID_PID` through `LAST_EXIT_GRID_PID`, currently
`0x05000010..0x05000017`.

The object parser must not infer tails by scanning forward for plausible object
prefixes. User-facing binary parsing requires `--proto-root`; without prototype
metadata, low-level parser helpers may still parse synthetic buffers but should
not be treated as reliable for real `.map` object records.

Item prototype tails are applied when prototype metadata is supplied.
Typed item tail accessors decode the documented MAP extra fields for weapons
(ammo count then ammo PID), ammo quantity, misc charges, and key code. Raw tail
ranges remain authoritative for serialization.

Scenery prototype tails are applied when prototype metadata is supplied. Current
fixture smoke coverage includes door tails that advance `BROKEN1.map`,
`BROKEN2.map`, `Newr1.map`, and `Newr2.map` past their previous cursor errors.
Typed scenery tail accessors decode the documented door walkthrough word,
stairs/ladders packed destination hex/elevation plus Fallout 2 destination map,
and elevator type/level. Raw tail ranges remain authoritative for serialization.

Current documented misc exit-grid tail rule:

| PID | Tail bytes | Evidence |
| ---: | ---: | --- |
| `0x05000010..0x05000017` | 16 | Fallout CE `FIRST_EXIT_GRID_PID`/`LAST_EXIT_GRID_PID`; the public MAP reference describes exit grids as four extra 32-bit values. Fixture examples include `0x05000010` in `Newr1.map` at offset 134884 and `Newr2.map` at offset 134724, `0x05000013` in `Newr2.map` at offset 134828, and `0x05000017` in `BROKEN2.map` at offset 134600. |

`0x0500000C` has no special tail rule. `ARVILL2.map` object at offset 44600
and `BROKEN1.map` object at offset 88056 both advance directly from the fixed
0x58-byte prefix to the next mapper-text object.

Former offset-specific item overrides have been removed. The current smoke
fixtures parse completely using prototype subtype rules plus the documented
misc exit-grid PID range.

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
