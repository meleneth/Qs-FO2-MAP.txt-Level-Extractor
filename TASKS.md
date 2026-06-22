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

- [x] Add a text range model for `.txt` parsing.
  - [x] Own loaded text as `std::string`.
  - [x] Use `std::string_view` for parser inputs and section views.
  - [x] Represent sections as `{offset, size}` ranges.
  - [x] Prefer `std::optional<Range>` for absent elevations.
  - [x] Keep CRLF and LF marker support.

- [ ] Replace ad hoc pointer math helpers with named types. Partial: `Range` and `ScriptType` exist; legacy pointer APIs and missing `ElevationIndex`/`MapFileKind` remain.
  - [x] `Range`
  - [ ] `ElevationIndex`
  - [x] `ScriptType`
  - [ ] `MapFileKind`

- [ ] Start using RAII containers for owned data. Partial: new parser/CLI paths use RAII; GUI file loading still uses raw pointers, `malloc`, and borrowed interior `char*`.
  - [x] `std::vector<std::byte>` for binary file bytes.
  - [x] `std::string` for loaded text.
  - [x] `std::vector<Script>` instead of `script*`.
  - [x] `std::array` for fixed three-elevation data.

- [x] Add a small local `Result<T>` type.
  - [x] Use it for parser/export operations that can fail with useful diagnostics.
  - [x] Keep `std::optional<T>` for simple absent/present values where no error detail is needed.
  - [x] Do not add a dependency just to get `expected`.

## Phase 2: Make `.txt` Parsing Bounded

- [x] Replace `find_str(uint8_t*, char*, int)` with bounded search.
  - [x] No `strlen` on loaded file data.
  - [x] No scanning past `file_siz`.
  - [x] No mutable `char*` inputs for read-only text.

- [ ] Replace `map_lvls::level`, `scripts`, and `objects` text pointers with ranges. Partial: `ParsedTextMap` uses ranges; `map_lvls` compatibility pointers remain in GUI/export.
  - [x] Parsed output should not depend on the original buffer being null-terminated.
  - [x] Section extraction should be by `std::string_view::substr`.

- [x] Make `map_level_sizes()` unnecessary.
  - [x] Parsing should produce complete section ranges in one pass.
  - [x] Header, elevation ranges, scripts, and objects should be internally consistent when returned.

- [ ] Add inline Catch2 tests for:
  - [x] CRLF elevation markers.
  - [x] LF elevation markers.
  - [x] missing middle elevation.
  - [x] missing first elevation.
  - [x] scripts/objects section detection.
  - [x] malformed input with no scripts section.
  - [x] malformed input with no objects section.

## Phase 3: Separate Text Transform From Export

- [ ] Extract a pure transform layer. Partial: `export_text_map()` takes parsed sources and an output plan without ImGui/paths/globals, but it exports directly instead of returning a separate transform model.
  - [x] Input: parsed left/right text maps and an output plan.
  - [ ] Output: a model describing selected elevations, scripts, objects, and chosen header.
  - [x] No ImGui.
  - [x] No file paths.
  - [x] No global state.

- [ ] Replace label-string matching with stable source references. Partial: modern export uses `{side, elevation}`; legacy GUI still uses `char[16]` labels as identity.
  - [ ] Avoid using `char[16]` labels as identity.
  - [x] Use `{side, elevation}` or similar explicit selection data.

- [x] Rewrite text export around `std::string`.
  - [x] Append to a string or checked writer.
  - [x] No precomputed `left_size + right_size` buffer.
  - [x] No output pointer arithmetic.
  - [x] Always serialize line endings as CRLF.
  - [x] Detect and return errors instead of truncating silently.

- [ ] Add inline Catch2 tests for:
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

- [ ] Add inline Catch2 tests for:
  - [x] object script copied only when owning object is copied.
  - [x] critter script copied only when owning critter is copied.
  - [x] spatial script copied by spatial elevation.
  - [x] duplicate spatial script ids are reassigned.
  - [x] duplicate object/critter ids are reassigned with matching `obj_sid`.
  - [ ] longer replacement IDs serialize correctly. Note: supported copied script IDs keep type in the high byte; add this only with a valid fixture that can actually grow in decimal width.

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

- [ ] Parse the rest of the Fallout `.map` file format. Partial: header, variables, tiles, scripts, object prefixes, and several object tails are modeled; complete object variants and inventories remain.
  - [ ] Use published Fallout/Fallout 2 MAP format references to identify sections and record layouts that are not implemented yet.
  - [ ] Cite the references used for format decisions in comments or docs near the relevant parser code.
  - [x] Capture map variables and local variables as modeled data.
  - [x] Parse tile/elevation blocks into explicit structures.
  - [x] Parse scripts, script padding, and script footers completely.
  - [ ] Parse objects by PID/object kind, including type-specific tails.
  - [ ] Parse inventories and nested inventory objects.
  - [x] Preserve raw byte ranges for fields or object variants that are still unknown so round-trip/export work is not blocked.
  - [x] Treat current debug/suspect messages as parsing diagnostics, not necessarily fatal errors; the tail/padding behavior is not fully understood yet.
  - [ ] Document every still-unknown field with offset, observed values, and fixture coverage.

- [x] Remove or isolate placeholder object structs that imply unsupported parsing.
  - [x] Unknown fields are acceptable.
  - [x] False confidence is not.

- [ ] Add inline Catch2 tests for small synthetic binary buffers.
  - [x] valid header.
  - [x] short header rejected.
  - [x] script count and record parse.
  - [x] padding/footer handling.
  - [x] object cursor advances across multiple objects.
  - [x] malformed object section rejected.
  - [x] representative object kind records.
  - [ ] inventory/nested object records.
  - [x] unknown fields are preserved in raw ranges.

## Phase 6: Stop Exposing Unfinished `.map` Export

- [x] Disable binary `.map` export in UI until real export exists.
  - [x] Show a clear "not implemented" state if selected inputs require `.map` export.
  - [x] Do not call `export_map_map()` from the GUI unless it writes a tested file.
  - [x] Add a "file parsed" indicator when loading/parsing succeeds.
  - [ ] Surface parser diagnostics separately from hard errors.

- [x] Convert `export_map_map()` into one of:
  - [ ] a tested real exporter, or
  - [x] a removed/deleted placeholder with a tracked task.

## Phase 7: Add Command Line Operations

- [x] Add CLI11, likely via `FetchContent`, pinned to an explicit version.
  - [x] Keep SDL/ImGui UI dependencies separate from CLI parsing.
  - [x] CLI mode should not initialize ImGui or SDL.

- [ ] Add spdlog, likely via `FetchContent`, pinned to an explicit version. Partial: dependency and CLI logging exist; parser diagnostics still return through `Result` and are not wired through spdlog.
  - [x] Use it for CLI diagnostics, parser diagnostics, and future GUI-visible logs.
  - [x] Keep parser code able to return diagnostics without requiring a global logger.
  - [ ] Prefer structured messages with enough context to debug unknown `.map` tail/padding behavior.

- [x] Provide command line operations for core workflows.
  - [x] Parse a file and print a detailed stat breakdown.
  - [x] Extract one elevation from a map text export.
  - [x] Split a map into per-elevation outputs.
  - [x] Combine selected elevations from source maps into a new output file.
  - [x] Write outputs to explicit paths and fail before overwriting unless requested.

- [ ] Wire reasonable CLI logging options. Partial: options exist, but `json` log format is accepted without JSON formatting.
  - [x] `--verbose` / `-v` to increase detail.
  - [x] `--quiet` / `-q` to suppress non-error output.
  - [x] `--log-level <trace|debug|info|warn|error|critical|off>`.
  - [x] `--log-file <path>` for file logging.
  - [x] `--log-format <human|plain|json>` if JSON output becomes useful for tooling.
  - [x] Default CLI behavior should be human-readable and concise.

- [ ] CLI output should be useful for development and debugging. Partial: binary stats are detailed; text stats do not yet include script/object counts.
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

- [ ] Introduce an application/session state struct.
  - [ ] Left loaded map.
  - [ ] Right loaded map.
  - [ ] Output selections.
  - [ ] Header selection.
  - [ ] Export path.
  - [ ] Current error.

- [ ] Keep ImGui code as rendering and event forwarding.
  - [ ] No parser internals in UI rendering code.
  - [ ] No export algorithm in UI code.
  - [ ] No global maps or labels.

- [ ] Replace `char[16]` label identity with display-only strings.
  - [ ] Labels may truncate for UI.
  - [ ] They must not drive export behavior.

- [ ] Add non-UI tests for session behavior.
  - [ ] load left/right map updates available elevations.
  - [ ] selecting header updates default export path.
  - [ ] invalid mixed file kinds produces a domain error.

## Phase 9: Error Handling And Result Types

- [ ] Stop using `printf` for parser/export failures. Partial: modern text parsers/exporters return `Result`; legacy binary parser compatibility still prints diagnostics.
  - [x] Return structured errors.
  - [ ] Keep diagnostics available to GUI and tests.

- [x] Pick a simple project-wide result pattern.
  - [x] `std::optional<T>` for simple absent/present.
  - [x] Local `Result<T>` for failures with diagnostics.
  - [x] Avoid exceptions unless the project deliberately adopts them.

- [x] Tests should assert specific failure reasons where practical.

## Phase 10: Retire Legacy Structures

- [ ] Replace `map_lvls` with separate concepts. Partial: parsed text/binary concepts exist; GUI still uses `map_lvls`.
  - [ ] Loaded file ownership.
  - [x] Parsed text map.
  - [x] Parsed binary map.
  - [ ] GUI/session display model.

- [ ] Remove raw owning pointers.
  - [ ] No `malloc`/`free` in project code except when forced by third-party APIs.
  - [x] Use `std::unique_ptr` only for polymorphism or incomplete types.
  - [ ] Prefer values, vectors, arrays, strings, and spans.

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
