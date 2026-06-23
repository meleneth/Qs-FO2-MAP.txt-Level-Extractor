# Agent Guide

## Working Agreement

This branch is for making the project better, not for preserving every existing design choice. The current code works in places and captures useful domain knowledge, but it also needs senior engineering pressure: safer parsing, clearer ownership, fewer globals, better tests, and a design that can grow from `.txt` merging into reliable `.map` support.

Do not treat rough code as sacred. Keep the useful behavior, fixtures, and hard-won Fallout map knowledge, but be willing to reshape APIs and move code when that makes the system easier to reason about.

## Project Purpose

This is a C++17 Fallout 2 map patch/merge tool. Whole-elevation merging is the first supported workflow, not the final product boundary. The practical workflow today is loading mapper-exported `map_name.txt` files, selecting source elevations, choosing an output header, and exporting a new `.Q.txt` map dump. The project direction is to grow from safe whole-elevation `.txt` merging into reliable binary `.map` parsing, planning, patching, and export for smaller map pieces.

Binary `.map` object parsing increasingly depends on Fallout prototype metadata. A `.map` object PID identifies the broad object kind and prototype index, but many variable object tails depend on the prototype subtype from `proto/<kind>/*.pro` and the corresponding `*.lst` files. Prefer prototype-backed subtype resolution over heuristic tail scanning.

The long-term direction should be a tool with a real parser/model/patch/export pipeline, not a GUI that manipulates raw text by accident. When choosing between approaches, preserve the current elevation workflow while designing APIs that can also support region-level binary map patches later.

## Current Shape

- `src/main.cpp`: SDL3/OpenGL/ImGui shell and drag/drop event forwarding.
- `src/map_txt_gui.cpp`: GUI rendering plus most session state. This file currently knows too much.
- `src/map_txt_parser.cpp`: `.txt` parsing and export. It contains valuable behavior but mixes scanning, model extraction, script filtering, id rewriting, output formatting, and allocation.
- `src/map_map_parser.cpp`: binary `.map` parser. Header/script parsing is tested; object parsing and export are incomplete.
- `src/map_structs.h`: shared raw structs and loose data contract. Many fields are borrowed pointers into loaded buffers.
- `tests/parser_tests.cpp`: current regression suite over real `.map` and `.txt` fixtures.
- `test_maps/`: important fixtures. Treat changes here as deliberate test-data changes.
- `scripts/extract_fallout2_protos.py`: local helper for extracting only `proto/**/*.pro` and `proto/**/*.lst` from a Fallout 2 install's DAT archives.
- `.local_fallout2_data/`: ignored local output for extracted Fallout prototype files. It contains game assets and must not be committed.

## Engineering Direction

Prefer moving toward these boundaries:

- File loading owns bytes and lifetime.
- Parsers convert bytes into explicit map/elevation/script/object data.
- Transform logic decides which elevations/scripts/objects move where.
- Exporters serialize from model data, not from GUI labels or scattered raw pointers.
- GUI only presents state and calls application services.

It is fine to make intermediate steps. A useful refactor that improves ownership, testability, or correctness is welcome even if it does not reach the final architecture in one pass.

## Build And Test

Preferred local Windows/MSYS2 commands:

```powershell
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --test-dir out/build/ucrt64-debug --output-on-failure
```

Release-style build:

```powershell
cmake --preset ucrt64-release
cmake --build --preset ucrt64-release
```

The presets assume MSYS2 UCRT64 tools at `C:/msys64/ucrt64/bin`. `CMakeLists.txt` uses `FetchContent` for SDL3 and Catch2, so a clean machine may need network access during configure.

## What To Fix Aggressively

- Replace string scanning that depends on accidental null terminators with bounded parsing over `std::string_view`, spans, offsets, or explicit buffer ranges.
- Stop storing important parser state as raw borrowed `char*` unless the lifetime is obvious and tested.
- Separate `.txt` parse, transform, and export logic. `export_map_txt()` is doing too much.
- Reduce global GUI state. Session state should become a struct or controller object that can be tested without ImGui.
- Make script id collision handling complete and covered by fixtures.
- Make binary `.map` object parsing cursor-based and bounds-checked before attempting serious binary export.
- Resolve binary `.map` object tail sizes from prototype subtype data where possible; do not reintroduce forward scanning for plausible next objects.
- Replace magic numbers with named constants where the domain meaning is known.
- Prefer tests that lock down behavior before and after risky refactors.

## Design Invariants

- Fallout binary `.map` fields are big-endian. Do not reinterpret packed structs directly.
- Fallout 2 DAT2 archive metadata is little-endian and compressed entries are zlib streams. Keep DAT parsing separate from `.map` parsing.
- Fallout `.pro` prototype fields are game-format binary data; parse them with explicit endian reads and fixtures before trusting subtype offsets.
- PID high bytes identify broad object kind, not necessarily enough information for subtype-specific `.map` object tails.
- `.txt` elevation markers may use CRLF or LF. Keep both working.
- Script type ids are encoded in the high byte of `scr_id`/PID-like values. Use the `script_type` enum constants.
- Object scripts are associated through `obj_sid` matching `scr_id`.
- Spatial scripts encode elevation in `built_tile`; output elevation rewrites must keep this consistent.
- `test_maps/` fixtures are part of parser correctness. Update expected values when fixture changes are intentional.
- Extracted Fallout assets under `.local_fallout2_data/` are local reference data only. Never commit them.
- `.map` export is not complete until it is implemented and tested end to end.

## Coding Standards

- Favor boring, explicit C++17 with clear ownership over clever pointer arithmetic.
- Keep parser/library code independent from ImGui and SDL.
- Use small types that make invalid states harder to represent: offsets instead of ambiguous pointers, counts with buffers, enums instead of raw ints.
- Add regression tests for parser and exporter behavior. Use the existing fixtures first; add minimal new fixtures only when they isolate a case better.
- Keep comments focused on domain facts, format quirks, and non-obvious decisions. Remove stale comments when the code makes them false.
- Do not churn third-party dependency code unless the task is explicitly about dependencies.

## Repo Hygiene

- Check `git status --short` before editing. There may be unrelated local changes; do not revert them unless explicitly asked.
- Ignore build output under `out/` and CMake/CPack products.
- Dependency submodules are configured with `ignore = dirty`; avoid committing dependency noise.
- This branch is allowed to break things temporarily while actively refactoring, but do not leave the final state unbuilt or untested when tests are available.

## Senior Review Bias

When in doubt, bias toward making the code more understandable and testable. Call out weak assumptions. If a function is unsafe because it trusts malformed input, say so and fix it. If a file mixes three responsibilities, split it. If an API makes ownership unclear, redesign it.

The goal is not minimal disturbance. The goal is a tool whose behavior is reliable enough that the author can keep building without stepping on hidden traps.
