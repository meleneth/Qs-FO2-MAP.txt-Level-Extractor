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

- Add a small binary view/reader type for `.map` parsing.
  - Owns no memory.
  - Wraps `std::span<const std::byte>`.
  - Tracks cursor offset.
  - Provides checked big-endian reads.
  - Returns `Result<T>` instead of silently returning `0`.
  - Provides typed accessor methods so callers do not hand-roll byte interpretation.

- Add a text range model for `.txt` parsing.
  - Own loaded text as `std::string`.
  - Use `std::string_view` for parser inputs and section views.
  - Represent sections as `{offset, size}` ranges.
  - Prefer `std::optional<Range>` for absent elevations.
  - Keep CRLF and LF marker support.

- Replace ad hoc pointer math helpers with named types.
  - `Range`
  - `ElevationIndex`
  - `ScriptType`
  - `MapFileKind`

- Start using RAII containers for owned data.
  - `std::vector<std::byte>` for binary file bytes.
  - `std::string` for loaded text.
  - `std::vector<Script>` instead of `script*`.
  - `std::array` for fixed three-elevation data.

- Add a small local `Result<T>` type.
  - Use it for parser/export operations that can fail with useful diagnostics.
  - Keep `std::optional<T>` for simple absent/present values where no error detail is needed.
  - Do not add a dependency just to get `expected`.

## Phase 2: Make `.txt` Parsing Bounded

- Replace `find_str(uint8_t*, char*, int)` with bounded search.
  - No `strlen` on loaded file data.
  - No scanning past `file_siz`.
  - No mutable `char*` inputs for read-only text.

- Replace `map_lvls::level`, `scripts`, and `objects` text pointers with ranges.
  - Parsed output should not depend on the original buffer being null-terminated.
  - Section extraction should be by `std::string_view::substr`.

- Make `map_level_sizes()` unnecessary.
  - Parsing should produce complete section ranges in one pass.
  - Header, elevation ranges, scripts, and objects should be internally consistent when returned.

- Add inline Catch2 tests for:
  - CRLF elevation markers.
  - LF elevation markers.
  - missing middle elevation.
  - missing first elevation.
  - scripts/objects section detection.
  - malformed input with no scripts section.
  - malformed input with no objects section.

## Phase 3: Separate Text Transform From Export

- Extract a pure transform layer.
  - Input: parsed left/right text maps and an output plan.
  - Output: a model describing selected elevations, scripts, objects, and chosen header.
  - No ImGui.
  - No file paths.
  - No global state.

- Replace label-string matching with stable source references.
  - Avoid using `char[16]` labels as identity.
  - Use `{side, elevation}` or similar explicit selection data.

- Rewrite text export around `std::string`.
  - Append to a string or checked writer.
  - No precomputed `left_size + right_size` buffer.
  - No output pointer arithmetic.
  - Always serialize line endings as CRLF.
  - Detect and return errors instead of truncating silently.

- Add inline Catch2 tests for:
  - export one elevation with chosen header.
  - export multiple elevations in new positions.
  - rewrite `obj_elev` to destination elevation.
  - preserve unselected elevations as absent.
  - reject missing header selection.
  - reject invalid source selection.

## Phase 4: Model Scripts Instead Of Editing Decimal Strings

- Parse text scripts into structured records.
  - Preserve enough original text to round-trip fields that are not yet understood.
  - Extract `scr_id`, script type, object id, spatial tile, spatial radius, and local var count.

- Parse text objects enough to associate scripts.
  - Extract object range.
  - Extract `obj_elev`.
  - Extract `obj_sid`.
  - Preserve object text for fields not yet understood.

- Replace decimal substring matching for `obj_sid`/`scr_id`.
  - Match numeric parsed IDs.
  - Rewrite fields by serialization, not in-place digit replacement.

- Complete script id collision handling.
  - Spatial scripts.
  - Object scripts.
  - Critter scripts.
  - Keep `obj_sid` and `scr_id` paired after rewrites.

- Add inline Catch2 tests for:
  - object script copied only when owning object is copied.
  - critter script copied only when owning critter is copied.
  - spatial script copied by spatial elevation.
  - duplicate spatial script ids are reassigned.
  - duplicate object/critter ids are reassigned with matching `obj_sid`.
  - longer replacement IDs serialize correctly.

## Phase 5: Fix Binary `.map` Parsing

- Replace binary parsing with a cursor over `std::span`.
  - Header parse should fail cleanly on short input.
  - Vars parse should bounds-check counts before allocating/copying.
  - Tile parse should bounds-check elevation data.
  - Script parse should bounds-check every record and padding block.

- Fix object parsing.
  - Advance cursor per object.
  - Store object records in a `std::vector`.
  - Preserve raw object ranges if variable-size records are not fully modeled yet.
  - Do not overwrite indexes per elevation.

- Parse the rest of the Fallout `.map` file format.
  - Use published Fallout/Fallout 2 MAP format references to identify sections and record layouts that are not implemented yet.
  - Cite the references used for format decisions in comments or docs near the relevant parser code.
  - Capture map variables and local variables as modeled data.
  - Parse tile/elevation blocks into explicit structures.
  - Parse scripts, script padding, and script footers completely.
  - Parse objects by PID/object kind, including type-specific tails.
  - Parse inventories and nested inventory objects.
  - Preserve raw byte ranges for fields or object variants that are still unknown so round-trip/export work is not blocked.
  - Treat current debug/suspect messages as parsing diagnostics, not necessarily fatal errors; the tail/padding behavior is not fully understood yet.
  - Document every still-unknown field with offset, observed values, and fixture coverage.

- Remove or isolate placeholder object structs that imply unsupported parsing.
  - Unknown fields are acceptable.
  - False confidence is not.

- Add inline Catch2 tests for small synthetic binary buffers.
  - valid header.
  - short header rejected.
  - script count and record parse.
  - padding/footer handling.
  - object cursor advances across multiple objects.
  - malformed object section rejected.
  - representative object kind records.
  - inventory/nested object records.
  - unknown fields are preserved in raw ranges.

## Phase 6: Stop Exposing Unfinished `.map` Export

- Disable binary `.map` export in UI until real export exists.
  - Show a clear "not implemented" state if selected inputs require `.map` export.
  - Do not call `export_map_map()` from the GUI unless it writes a tested file.
  - Add a "file parsed" indicator when loading/parsing succeeds.
  - Surface parser diagnostics separately from hard errors.

- Convert `export_map_map()` into one of:
  - a tested real exporter, or
  - a removed/deleted placeholder with a tracked task.

## Phase 7: Add Command Line Operations

- Add CLI11, likely via `FetchContent`, pinned to an explicit version.
  - Keep SDL/ImGui UI dependencies separate from CLI parsing.
  - CLI mode should not initialize ImGui or SDL.

- Add spdlog, likely via `FetchContent`, pinned to an explicit version.
  - Use it for CLI diagnostics, parser diagnostics, and future GUI-visible logs.
  - Keep parser code able to return diagnostics without requiring a global logger.
  - Prefer structured messages with enough context to debug unknown `.map` tail/padding behavior.

- Provide command line operations for core workflows.
  - Parse a file and print a detailed stat breakdown.
  - Extract one elevation from a map text export.
  - Split a map into per-elevation outputs.
  - Combine selected elevations from source maps into a new output file.
  - Write outputs to explicit paths and fail before overwriting unless requested.

- Wire reasonable CLI logging options.
  - `--verbose` / `-v` to increase detail.
  - `--quiet` / `-q` to suppress non-error output.
  - `--log-level <trace|debug|info|warn|error|critical|off>`.
  - `--log-file <path>` for file logging.
  - `--log-format <human|plain|json>` if JSON output becomes useful for tooling.
  - Default CLI behavior should be human-readable and concise.

- CLI output should be useful for development and debugging.
  - File kind.
  - Header presence/size.
  - Present elevations and byte/line ranges.
  - Script counts by type.
  - Object counts by elevation and object kind when known.
  - Diagnostics for unknown tail/padding data.
  - Parse success/failure status.

- Add inline Catch2 tests for CLI-adjacent command planning where practical.
  - Argument parsing can be covered lightly.
  - Core operations should be tested below the CLI layer through application services.

## Phase 8: Move GUI State Behind A Plain Model

- Introduce an application/session state struct.
  - Left loaded map.
  - Right loaded map.
  - Output selections.
  - Header selection.
  - Export path.
  - Current error.

- Keep ImGui code as rendering and event forwarding.
  - No parser internals in UI rendering code.
  - No export algorithm in UI code.
  - No global maps or labels.

- Replace `char[16]` label identity with display-only strings.
  - Labels may truncate for UI.
  - They must not drive export behavior.

- Add non-UI tests for session behavior.
  - load left/right map updates available elevations.
  - selecting header updates default export path.
  - invalid mixed file kinds produces a domain error.

## Phase 9: Error Handling And Result Types

- Stop using `printf` for parser/export failures.
  - Return structured errors.
  - Keep diagnostics available to GUI and tests.

- Pick a simple project-wide result pattern.
  - `std::optional<T>` for simple absent/present.
  - Local `Result<T>` for failures with diagnostics.
  - Avoid exceptions unless the project deliberately adopts them.

- Tests should assert specific failure reasons where practical.

## Phase 10: Retire Legacy Structures

- Replace `map_lvls` with separate concepts.
  - Loaded file ownership.
  - Parsed text map.
  - Parsed binary map.
  - GUI/session display model.

- Remove raw owning pointers.
  - No `malloc`/`free` in project code except when forced by third-party APIs.
  - Use `std::unique_ptr` only for polymorphism or incomplete types.
  - Prefer values, vectors, arrays, strings, and spans.

- Remove C macros where scoped constants or enums work.
  - `NAME_LENGTH`
  - `LEFT`, `MIDDLE`, `RIGHT`
  - `uint`

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
