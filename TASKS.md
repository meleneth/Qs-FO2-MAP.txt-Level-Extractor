# TASKS

## Direction

Move the project toward modern C++20 with explicit ownership, bounded views, and testable parser/model/export boundaries.

The loaded map files are tiny on modern systems. It is fine to read the whole file into an owning `std::vector<std::byte>` or `std::string`. After that, prefer non-owning views into the loaded data:

- `std::span<const std::byte>` for binary `.map` data.
- `std::string` for owned textual mapper `.txt` data.
- `std::string_view` for non-owning views into loaded text.
- Offsets/ranges for parsed sections instead of raw interior pointers.

Only copy bytes when transformation is required. Do not copy just to parse.

Binary parsing should expose method accessors everywhere practical. Callers should ask the model/reader for typed values instead of doing byte arithmetic or endian conversion directly.

Text export should always write CRLF line endings.

Tests should become dense, inline Catch2 examples, closer to an RSpec style: small scenario fixtures embedded in the test file, clear behavior names, shared helpers that keep examples succinct. Do not load external files in new tests. Existing large fixture tests can remain temporarily as regression coverage, but the target style is inline, minimal, maintainable examples.

## Phase 1: Establish Safe Data Primitives

- [x] Add a small binary view/reader type for `.map` parsing.
  - [x] Owns no memory.
  - [x] Wraps `std::span<const std::byte>`.
  - [x] Tracks cursor offset.
  - [x] Provides checked big-endian reads.
  - [x] Returns `Result<T>` instead of silently returning `0`.
  - [x] Provides typed accessor methods so callers do not hand-roll byte interpretation.
  - [x] Remove the old unchecked `B_Endian` helper.

- [x] Add a text range model for `.txt` parsing.
  - [x] Own loaded text as `std::string`.
  - [x] Use `std::string_view` for parser inputs and section views.
  - [x] Represent sections as `{offset, size}` ranges.
  - [x] Prefer `std::optional<Range>` for absent elevations.
  - [x] Keep CRLF and LF marker support.

- [x] Replace ad hoc pointer math helpers with named types.
  - [x] `Range`
  - [x] `ElevationIndex`
  - [x] `ScriptType`
  - [x] `MapFileKind`

- [x] Start using RAII containers for owned data.
  - [x] `std::vector<std::byte>` for binary file bytes.
  - [x] `std::string` for loaded text.
  - [x] `std::vector<Script>` instead of `script*`.
  - [x] `std::array` for fixed three-elevation data.
  - [x] `std::vector<uint8_t>` and `std::string` for GUI dropped-file ownership.

- [x] Add a small local `Result<T>` type.
  - [x] Use it for parser/export operations that can fail with useful diagnostics.
  - [x] Keep `std::optional<T>` for simple absent/present values where no error detail is needed.
  - [x] Do not add a dependency just to get `expected`.

## Phase 2: Make `.txt` Parsing Bounded

- [x] Replace `find_str(uint8_t*, char*, int)` with bounded search.
  - [x] No `strlen` on loaded file data.
  - [x] No scanning past `file_siz`.
  - [x] No mutable `char*` inputs for read-only text.

- [x] Replace legacy text section pointers with ranges.
  - [x] Parsed output should not depend on the original buffer being null-terminated.
  - [x] Section extraction should be by `std::string_view::substr`.

- [x] Make `map_level_sizes()` unnecessary.
  - [x] Parsing should produce complete section ranges in one pass.
  - [x] Header, elevation ranges, scripts, and objects should be internally consistent when returned.

- [x] Add inline Catch2 tests for:
  - [x] CRLF elevation markers.
  - [x] LF elevation markers.
  - [x] missing middle elevation.
  - [x] missing first elevation.
  - [x] scripts/objects section detection.
  - [x] malformed input with no scripts section.
  - [x] malformed input with no objects section.

## Phase 3: Separate Text Transform From Export

- [x] Extract a pure transform layer.
  - [x] Input: parsed left/right text maps and an output plan.
  - [x] Output: a model describing selected elevations, scripts, objects, and chosen header.
  - [x] No ImGui.
  - [x] No file paths.
  - [x] No global state.

- [x] Replace label-string matching with stable source references.
  - [x] Avoid using `char[16]` labels as identity.
  - [x] Use `{side, elevation}` or similar explicit selection data.

- [x] Rewrite text export around `std::string`.
  - [x] Append to a string or checked writer.
  - [x] No precomputed `left_size + right_size` buffer.
  - [x] No output pointer arithmetic.
  - [x] Always serialize line endings as CRLF.
  - [x] Detect and return errors instead of truncating silently.

- [x] Add inline Catch2 tests for:
  - [x] export one elevation with chosen header.
  - [x] export multiple elevations in new positions.
  - [x] rewrite `obj_elev` to destination elevation.
  - [x] preserve unselected elevations as absent.
  - [x] reject missing header selection.
  - [x] reject invalid source selection.

## Phase 4: Model Scripts Instead Of Editing Decimal Strings

- [x] Parse text scripts into structured records.
  - [x] Preserve enough original text to round-trip fields that are not yet understood.
  - [x] Extract `scr_id`, script type, object id, spatial tile, spatial radius, and local var count.

- [x] Parse text objects enough to associate scripts.
  - [x] Extract object range.
  - [x] Extract `obj_elev`.
  - [x] Extract `obj_sid`.
  - [x] Preserve object text for fields not yet understood.

- [x] Replace decimal substring matching for `obj_sid`/`scr_id`.
  - [x] Match numeric parsed IDs.
  - [x] Rewrite fields by serialization, not in-place digit replacement.

- [x] Complete script id collision handling.
  - [x] Spatial scripts.
  - [x] Object scripts.
  - [x] Critter scripts.
  - [x] Keep `obj_sid` and `scr_id` paired after rewrites.

- [x] Add inline Catch2 tests for:
  - [x] object script copied only when owning object is copied.
  - [x] critter script copied only when owning critter is copied.
  - [x] spatial script copied by spatial elevation.
  - [x] duplicate spatial script ids are reassigned.
  - [x] duplicate object/critter ids are reassigned with matching `obj_sid`.
  - [x] longer replacement IDs serialize correctly. Not applicable to current copied script types: supported spatial, object, and critter script IDs all stay in eight decimal digits while reassigned within their high-byte type range.

## Phase 5: Fix Binary `.map` Parsing

- [x] Replace binary parsing with a cursor over `std::span`.
  - [x] Header parse should fail cleanly on short input.
  - [x] Vars parse should bounds-check counts before allocating/copying.
  - [x] Tile parse should bounds-check elevation data.
  - [x] Script parse should bounds-check every record and padding block.

- [x] Fix object parsing.
  - [x] Advance cursor per object.
  - [x] Store object records in a `std::vector`.
  - [x] Preserve raw object ranges if variable-size records are not fully modeled yet.
  - [x] Do not overwrite indexes per elevation.

- [ ] Parse the rest of the Fallout `.map` file format. Partial: header, variables, tiles, scripts, object prefixes, inventories, and several object tails are modeled; complete object variants remain.
  - [x] Use published Fallout/Fallout 2 MAP format references to identify sections and record layouts that are not implemented yet.
  - [x] Cite the references used for format decisions in comments or docs near the relevant parser code.
  - [ ] Add prototype metadata support for PID subtype resolution.
    - [x] Add a local helper script to extract only `proto/**/*.pro` and `proto/**/*.lst` from a Fallout 2 install into `.local_fallout2_data/`.
    - [x] Keep extracted Fallout game assets ignored and out of git.
    - [x] Parse `proto/items/items.lst`, `proto/scenery/scenery.lst`, `proto/misc/misc.lst`, and other kind lists into PID-index-to-file mappings.
    - [x] Parse just enough `.pro` data to determine item/scenery/misc subtype values needed for `.map` object tail layouts.
    - [x] Add a `PrototypeDatabase` or similar plain model that can be loaded from an extracted proto root without GUI dependencies.
    - [x] Thread optional prototype metadata through binary object parsing entry points for callers that provide a `PrototypeDatabase`.
    - [x] Add a prototype-backed top-level `parse_binary_map` overload so callers do not duplicate the staged parse pipeline.
    - [x] Add `parse-stats --proto-root` so CLI parsing can load and carry extracted prototype metadata.
    - [x] Require `--proto-root` for CLI binary `.map` object parsing instead of pretending prototype metadata is optional.
    - [ ] Require prototype metadata in binary GUI/workflow entry points before exposing binary object parsing beyond diagnostics.
    - [ ] Enable prototype metadata to drive object tail resolution only after the subtype rule is fixture-validated. Partial: item subtype tails, scenery subtype tails, and misc exit-grid PID tails are enabled.
    - [x] Replace offset-specific object tail exceptions with prototype-subtype rules where the extracted data proves the layout.
    - [x] Add tests with tiny inline `.lst` and `.pro` fixtures; do not add proprietary Fallout assets to the repository.
    - [x] Surface a clear diagnostic when prototype data is unavailable and the parser reaches a PID whose tail cannot be resolved safely.
    - [ ] Stretch: add C++ DAT2 reading/extraction to the app or CLI so users do not need a separate extraction step.
    - [ ] Stretch: support loose `data/proto` overrides on top of DAT-extracted prototype data.
    - [ ] Reconcile object tail sizes against the published MAP format references and fixture evidence before adding more subtype rules.
    - [x] Correct normal MAP critter tails to the documented/observed 40-byte layout; remove or quarantine the older 44-byte/11-word assumption unless a specific fixture proves it.
    - [ ] Re-check item, scenery, exit-grid, misc, and other PID-kind tails against `falloutmods.fandom.com/wiki/MAP_File_Format` and `fodev.net/files/fo2/map.html`.
      - [x] Replace fixture-specific misc exit-grid rules with Fallout CE's documented `FIRST_EXIT_GRID_PID..LAST_EXIT_GRID_PID` range (`0x05000010..0x05000017`), and keep `0x0500000C` as a generic 0-byte misc object.
      - [x] Validate scenery door subtype tail offsets against real fixtures before allowing prototype scenery metadata to drive object cursor advancement; after item/misc corrections, enabling scenery tails lets `ARVILL2.map` parse all 1141 object records and advances `BROKEN1.map`, `BROKEN2.map`, `Newr1.map`, and `Newr2.map` past their prior door-tail cursor errors.
      - [ ] Validate stairs/elevator/ladder scenery subtype tail offsets against real fixtures as they are encountered.
    - [ ] Keep any compatibility parsing for variant tails explicitly documented with fixture names, byte offsets, and why the public reference or current prototype support is insufficient.
      - [x] Document the misc exit-grid PID range without reintroducing prefix scanning, and document why `0x0500000C` remains a generic 0-byte misc tail.
      - [x] Remove offset-specific fixture overrides once prototype subtype resolution covers those records.
  - [ ] Reconcile inventory parsing with the documented count-prefixed inventory object format.
    - [ ] Prefer bounded parent inventory parsing over global backtracking across parent record boundaries.
    - [x] Treat "direct child object" layouts as suspect until a fixture proves there is no preceding quantity/count word.
    - [ ] Add fixture-backed tests for `BROKEN1.map` style nested inventory alignment once the tail sizes are corrected.
  - [x] Consume serialized zero object-count slots for absent elevations after present object blocks.
  - [x] Remove forward-scanning tail candidate selection; object tails now resolve through fixed kind sizes, prototype subtype metadata, and the documented misc exit-grid PID range.
  - [ ] Add more documented PID subtype rules so `BROKEN1.map`, `BROKEN2.map`, `Newr1.map`, and `Newr2.map` can progress without heuristic scanning. Partial: item subtype tails, scenery subtype tails, and misc exit-grid PID range `0x05000010..0x05000017` now parse the current smoke fixtures with extracted prototypes. Prefer prototype-backed rules for new work.
  - [x] Capture map variables and local variables as modeled data.
  - [x] Parse tile/elevation blocks into explicit structures.
  - [x] Parse scripts, script padding, and script footers completely.
  - [ ] Parse objects by PID/object kind, including type-specific tails. Partial: object record boundaries are fixture-validated with extracted prototypes; many tails are still preserved as raw ranges.
  - [x] Parse inventories and nested inventory objects.
  - [x] Add real fixture regression coverage for prototype-backed binary object parsing over `ARVILL2.map`, `BROKEN1.map`, `BROKEN2.map`, `Newr1.map`, and `Newr2.map`.
  - [ ] Expand fixture assertions beyond counts once the object model has stable typed fields needed by export.
  - [x] Preserve raw byte ranges for fields or object variants that are still unknown so round-trip/export work is not blocked.
  - [x] Treat current debug/suspect messages as parsing diagnostics, not necessarily fatal errors; the tail/padding behavior is not fully understood yet.
  - [x] Document every still-unknown field with offset, observed values, and fixture coverage.

- [ ] Define the binary `.map` patch/merge model.
  - [x] First binary operation is whole-elevation replacement, not rectangular region patching.
  - [x] Replacement deletes destination elevation contents first. Future merge-in mode should preserve destination contents with explicit collision rules.
  - [x] Keep the destination header and variables; only elevation-present flags may be changed by the eventual writer.
  - [x] Preserve tile positions exactly; v1 has no x/y/tile offset.
  - [x] Assign new object IDs to copied top-level objects and nested inventory objects.
  - [x] Assign new script IDs to copied scripts even when no collision exists.
  - [x] Copy object/critter scripts attached to copied objects; rewrite object/script references to the new IDs.
  - [x] Copy spatial scripts whose `built_tile` elevation is the source elevation; rewrite `built_tile` to destination elevation and preserve tile/radius.
  - [x] Keep destination map/local variables unchanged.
  - [x] Keep destination map script unchanged by default.
  - [x] Treat known unsafe conflicts as hard errors by default; add an explicit risky override later.
  - [x] Allow replacing into an absent destination elevation; source elevation absence is a hard error.
  - [x] Preserve exit grids by default and report them as external links; hard-error only if the exit-grid tail cannot be decoded.
  - [x] Cross-elevation links in copied content are hard errors for v1 unless they can be safely rewritten from source elevation to destination elevation.
  - [ ] Add planning tests before writing bytes: selected tiles, selected objects, selected scripts, ID reassignment, exit-grid reporting, and rejected boundary/cross-elevation cases.
    - [x] Cover selected object/script counts, ID reassignment, absent destination, absent source, missing attached scripts, outside-object script references, invalid spatial built_tile elevation bits, missing copied scenery prototype metadata, copied elevation-linking scenery, and undecodable exit-grid tails.
    - [x] Cover source variable requirements exceeding destination variables.
    - [x] Cover exhausted object/script ID space.

- [ ] Implement `replace-elevation --dry-run` planning before binary writing.
  - [x] Add CLI command shape: `replace-elevation <source.map> <destination.map> <output.map> --source-elevation N --dest-elevation N --proto-root PATH --dry-run`.
  - [x] Require output path even in dry-run so the command is the same as future write mode.
  - [x] Make non-dry-run hard-error with "binary map export not implemented" until serialization exists.
  - [x] Load both maps with prototype metadata and fail without `--proto-root`.
  - [x] Produce human-readable dry-run output first; JSON can come later.
  - [x] Echo source/destination/output/proto/elevation arguments in dry-run output.
  - [x] Report destination deletion counts: tiles/elevation block, objects, spatial scripts, attached object/critter scripts.
  - [x] Report source copy counts: tiles/elevation block, objects, inventories, spatial scripts, attached object/critter scripts.
  - [x] Report object ID and script ID reassignment counts.
  - [x] Report bounded previews of object/script ID reassignment mappings.
  - [x] Report preserved exit grids as external links with destination map/tile/elevation/rotation.
  - [x] Preserve source and destination tile byte ranges so the writer can replace the exact destination elevation tile block.
  - [x] Preserve source raw ranges for planned copied objects and scripts so the writer can copy exact records before applying rewrites.
  - [x] Preserve destination raw ranges for planned deleted objects and scripts so the writer can remove exact records.
  - [x] Hard-error if any planned tile/object/script copy or deletion range is not backed by the expected source/destination map bytes.
  - [x] Plan destination object and script section counts after delete-then-copy so the eventual writer has explicit totals to serialize.
  - [x] Add a binary patch writer boundary and keep non-dry-run replace-elevation failing there until serialization is implemented.
  - [x] Add tested byte-level tile block replacement inside the writer boundary while keeping full binary output disabled.
  - [x] Add tested header map-flag patching so replacing into an absent destination elevation marks that elevation present.
  - [x] Add a tested checked byte-range replacement primitive for later script/object section serialization.
  - [x] Add tested multi-range deletion for planned destination script/object raw records, with overlap and bounds checks.
  - [x] Add tested multi-range insertion for planned copied source script/object raw records, with contained inventory range normalization.
  - [x] Add a tested big-endian int32 patch primitive for future count and ID serialization.
  - [x] Add a tested multi-int32 patch primitive with overlap/conflict validation for future copied-record field rewrites.
  - [x] Add tested destination object count patching for total and per-elevation object counts.
  - [x] Preserve parsed binary script count offsets and add tested destination script count patching.
  - [x] Preserve parsed object/script field offsets needed for writer rewrites of object IDs, object elevation/script links, script IDs, spatial tile elevation, script object links, and local-var ranges.
  - [x] Add tested source-record rewrite patch planning for copied object IDs, object elevation/script links, script IDs, spatial tile elevation, and script object links.
  - [x] Wire copied-record rewrite patching into the binary writer preflight while keeping final binary export disabled.
  - [x] Hard-error when any copied-record rewrite patch is not fully contained by a planned copied source range.
  - [x] Add a tested copied-byte-stream helper that applies source-offset rewrites only to planned copied ranges before insertion.
  - [x] Add tested offset adjustment after deletions and insert copied scripts before the shifted destination object section instead of appending every copied record at EOF.
  - [ ] Hard-error for missing prototype metadata, absent source elevation, missing object scripts, copied scripts referencing outside objects, undecodable elevation-bearing links, detectable source variable requirements exceeding destination variables, and exhausted object/script ID space.
    - [x] Implement current hard errors for absent source elevation, missing copied object scripts, copied scripts referencing outside copied objects, invalid spatial elevation bits, missing copied scenery prototype metadata, copied stairs/elevator/ladder scenery links, copied script local-variable ranges unavailable in the destination, exhausted object/script ID namespaces, and undecodable exit-grid tails.
  - [x] Add tests around the pure planner before wiring the CLI command.
  - [x] Add formatter coverage for the dry-run CLI report.

- [ ] Decode typed object tails needed for binary export/merge decisions.
  - [ ] Item tails: weapon ammo PID/count, ammo quantity, misc charges, key code.
  - [ ] Scenery tails: door flags/key, stairs/elevator/ladder destinations.
  - [ ] Misc exit-grid tails: destination map, tile, elevation, rotation.
  - [ ] Critter tails: current HP/status fields already parsed enough for preservation; decide which fields need semantic names for merge UI.
  - [ ] Keep raw tail bytes as the source of truth until every decoded field can round-trip.

- [x] Remove or isolate placeholder object structs that imply unsupported parsing.
  - [x] Unknown fields are acceptable.
  - [x] False confidence is not.

- [x] Add inline Catch2 tests for small synthetic binary buffers.
  - [x] valid header.
  - [x] short header rejected.
  - [x] script count and record parse.
  - [x] padding/footer handling.
  - [x] object cursor advances across multiple objects.
  - [x] malformed object section rejected.
  - [x] representative object kind records.
  - [x] inventory/nested object records.
  - [x] unknown fields are preserved in raw ranges.

## Phase 6: Stop Exposing Unfinished `.map` Export

- [x] Disable binary `.map` export in UI until real export exists.
  - [x] Show a clear "not implemented" state if selected inputs require `.map` export.
  - [x] Do not call `export_map_map()` from the GUI unless it writes a tested file.
  - [x] Add a "file parsed" indicator when loading/parsing succeeds.
  - [x] Surface parser diagnostics separately from hard errors.

- [x] Remove the deleted `export_map_map()` placeholder and track real binary export separately.

## Phase 7: Add Command Line Operations

- [x] Add CLI11, likely via `FetchContent`, pinned to an explicit version.
  - [x] Keep SDL/ImGui UI dependencies separate from CLI parsing.
  - [x] CLI mode should not initialize ImGui or SDL.

- [x] Add spdlog via `FetchContent`, pinned to an explicit version.
  - [x] Use it for CLI diagnostics, parser diagnostics, and future GUI-visible logs.
  - [x] Keep parser code able to return diagnostics without requiring a global logger.
  - [x] Prefer structured messages with enough context to debug unknown `.map` tail/padding behavior.

- [x] Provide command line operations for core workflows.
  - [x] Parse a file and print a detailed stat breakdown.
  - [x] Extract one elevation from a map text export.
  - [x] Split a map into per-elevation outputs.
  - [x] Combine selected elevations from source maps into a new output file.
  - [x] Write outputs to explicit paths and fail before overwriting unless requested.
  - [x] Add a separate `qmap_weird_scan` executable for scanning binary `.map` files for suspicious object/script/inventory/exit-grid data.
    - [x] Require `--proto-root` and parse through the prototype-backed binary map parser.
    - [x] Report missing object script references.
    - [x] Report object/critter scripts pointing at missing objects.
    - [x] Report parsed inventory count mismatches.
    - [x] Report invalid exit-grid destination tile/elevation/rotation values.
    - [x] Summarize critters, inventory-owning objects, inventory items, and exit grids.
    - [ ] Add more domain checks as suspicious real-map cases are identified.

- [x] Wire reasonable CLI logging options.
  - [x] `--verbose` / `-v` to increase detail.
  - [x] `--quiet` / `-q` to suppress non-error output.
  - [x] `--log-level <trace|debug|info|warn|error|critical|off>`.
  - [x] `--log-file <path>` for file logging.
  - [x] `--log-format <human|plain>`.
  - [x] Default CLI behavior should be human-readable and concise.

- [x] CLI output should be useful for development and debugging.
  - [x] File kind.
  - [x] Header presence/size.
  - [x] Present elevations and byte/line ranges.
  - [x] Script counts by type.
  - [x] Object counts by elevation and object kind when known.
  - [x] Diagnostics for unknown tail/padding data.
  - [x] Parse success/failure status.

- [x] Add inline Catch2 tests for CLI-adjacent command planning where practical.
  - [x] Argument parsing can be covered lightly.
  - [x] Core operations should be tested below the CLI layer through application services.

## Phase 8: Move GUI State Behind A Plain Model

- [x] Introduce an application/session state struct.
  - [x] Left loaded map.
  - [x] Right loaded map.
  - [x] Output selections.
  - [x] Header selection.
  - [x] Export path.
  - [x] Current error.

- [x] Keep ImGui code as rendering and event forwarding.
  - [x] No parser internals in UI rendering code.
  - [x] No export algorithm in UI code.
  - [x] No global maps or labels.

- [x] Replace `char[16]` label identity with display-only strings.
  - [x] Labels may truncate for UI.
  - [x] They must not drive export behavior.

- [x] Add non-UI tests for session behavior.
  - [x] load left/right map updates available elevations.
  - [x] selecting header updates default export path.
  - [x] invalid mixed file kinds produces a domain error.

## Phase 9: Error Handling And Result Types

- [x] Stop using `printf` for parser/export failures.
  - [x] Return structured errors.
  - [x] Keep diagnostics available to GUI and tests.

- [x] Pick a simple project-wide result pattern.
  - [x] `std::optional<T>` for simple absent/present.
  - [x] Local `Result<T>` for failures with diagnostics.
  - [x] Avoid exceptions unless the project deliberately adopts them.

- [x] Tests should assert specific failure reasons where practical.

## Phase 10: Retire Legacy Structures

- [x] Replace `map_lvls` with separate concepts.
  - [x] Loaded file ownership.
  - [x] Parsed text map.
  - [x] Parsed binary map.
  - [x] GUI/session display model.
  - [x] Remove legacy binary `map_header` compatibility storage.

- [x] Remove raw owning pointers.
  - [x] No `malloc`/`free` in project code except when forced by third-party APIs.
  - [x] Use `std::unique_ptr` only for polymorphism or incomplete types.
  - [x] Prefer values, vectors, arrays, strings, and spans.

- [x] Remove C macros where scoped constants or enums work.
  - [x] `NAME_LENGTH`
  - [x] `LEFT`, `MIDDLE`, `RIGHT`
  - [x] `uint`

## Verification Standard

Each substantial refactor should include:

- Focused inline Catch2 tests for new behavior.
- Existing fixture regression tests kept passing until replaced by better coverage.
- `cmake --preset ucrt64-debug`
- `cmake --build --preset ucrt64-debug`
- `ctest --test-dir out/build/ucrt64-debug --output-on-failure`

## First Recommended Task

Start with the bounded text parser.

It is the highest leverage change because `.txt` export is the current user-facing workflow, and the unsafe null-terminated parsing assumption infects every later step. Build `Range`, parse from `std::string_view`, return a `ParsedTextMap`, and cover it with inline Catch2 tests. Once that is stable, export can stop depending on raw pointers and labels.

Before that parser refactor, add the foundation commit: `Result<T>`, `Range`, binary/text view conventions, and any temporary adapters needed to keep old callers compiling. Mark every compatibility adapter with a clear removal note.
