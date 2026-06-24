An attempt at making a map_name.txt level merger for Fallout 2's mapper system.<br>
When finished this can be used with .txt map files exported from the mapper (either using ALT+P or the "Create ALL MAP TEXTS" menu option in mapper2.exe).<br><br>
This will allow the user to 
> copy levels from one map_name1.txt file to another map_name2.txt file,<br>
> extract map levels,<br>
> insert map levels,<br>
<br>
and maybe other stuff, dunno yet.<br>
<img width="437" height="306" alt="image" src="https://github.com/user-attachments/assets/46426cd7-fa3a-424e-90ba-97a141415498" />

This tool should do the job of 
- extracting a map level from a Fallout 2 "map.txt" file, 
- then generating a new "Q-map.txt" file from the extracted levels.

This can also be used to merge two maps together, making map merges muuuuch easier and less prone to error.
Also, this fixes issues with scripts having overlapping script ID's, causing the mapper to reject them until the ID's
are manually fixed so they no longer overlap.

Use is simple in the GUI, drag and drop only.
<br>Drag and drop a Fallout 2 "map_name.txt" file onto one side or the other.
<br>Select the level you want to extract from on either side, and the level to extract to in the middle.
<br>Click the arrow for the appropriate side to set the indicator to show which level is extracted where.
<br>Select which map header you want to use (there's a bunch of information in the header that might be necessary depending on the map variables used in the scripts attached to any objects/spatials being exported on a level.
<br>Type in a filename and click "Export".

The Export will automatically add a "Q.txt" to the filename so files aren't over-written.
<br>You should be able to rename this however you want, but be careful not to over-write an old map until you know the new one works.

## Command Line Workflow

The CLI is built as `qmap_cli.exe` under the selected CMake build directory.
For the preferred debug preset that is:

```powershell
out\build\ucrt64-debug\qmap_cli.exe --help
```

Current commands:

```powershell
out\build\ucrt64-debug\qmap_cli.exe parse-stats <map.txt|map.map> [--proto-root <proto-root>]
out\build\ucrt64-debug\qmap_cli.exe extract <input.txt> <output.txt> --elevation <0|1|2> [-f]
out\build\ucrt64-debug\qmap_cli.exe split <input.txt> <output-dir> [-f]
out\build\ucrt64-debug\qmap_cli.exe combine <left.txt> <right.txt> <output.txt> --header <0|1> --select <DEST=SIDE:SOURCE>... [-f]
out\build\ucrt64-debug\qmap_cli.exe replace-elevation <source.map> <destination.map> <output.map> --source-elevation <0|1|2> --dest-elevation <0|1|2> --proto-root <proto-root> [--dry-run] [-f]
```

`combine` currently operates on mapper-exported `.txt` files and moves whole
elevations. `SIDE` is `L` or `R`; `DEST` and `SOURCE` are elevation numbers
`0`, `1`, or `2`.

Example: build a new text map using the left header, left elevation 0 as output
elevation 0, right elevation 0 as output elevation 1, and left elevation 1 as
output elevation 2:

```powershell
out\build\ucrt64-debug\qmap_cli.exe combine .\maps\town_a.txt .\maps\town_b.txt .\out\interleaved.Q.txt --header 0 --select 0=L:0 --select 1=R:0 --select 2=L:1 -f
```

That is elevation-level interleaving. Area/region patching inside a binary
`.map` elevation is not implemented yet; that is the direction for the binary
patch work after whole-elevation replacement.

Binary whole-elevation replacement is supported for prototype-backed `.map`
files. It loads both maps with prototype metadata, deletes the destination
elevation contents, copies the selected source elevation, rewrites copied
object/script IDs, validates the patched bytes by parsing them back, and then
writes the output. Use `--dry-run` to inspect what would be deleted, copied,
reassigned, and preserved without writing.

```powershell
out\build\ucrt64-debug\qmap_cli.exe replace-elevation .\maps\source.map .\maps\destination.map .\out\patched.map --source-elevation 0 --dest-elevation 2 --proto-root .local_fallout2_data\proto --dry-run
```

## Binary `.map` Prototype Data

Binary `.map` parsing is under active development. Some object records cannot be
decoded correctly from the `.map` file alone because the object PID only gives
the broad object kind and prototype index. The exact item/scenery/misc subtype
comes from Fallout 2 prototype files under `proto/`, and that subtype determines
some variable object tail layouts.

If Fallout 2 is installed through Steam in the default location, extract the
needed prototype metadata with:

```powershell
python scripts\extract_fallout2_protos.py --overwrite
```

For a custom install path:

```powershell
python scripts\extract_fallout2_protos.py --fallout2-root "C:\Path\To\Fallout 2" --overwrite
```

This writes only `proto/**/*.pro` and `proto/**/*.lst` files to
`.local_fallout2_data/`. That directory contains local game assets and is
ignored by git; do not commit it.

After extraction, pass the prototype root to binary map stats parsing. Binary
`.map` object parsing requires this metadata; the CLI intentionally fails
without `--proto-root`.

```powershell
out\build\ucrt64-debug\qmap_cli.exe parse-stats test_maps\ARVILL2.map --proto-root .local_fallout2_data\proto
```

Current binary status: with extracted Fallout 2 prototype metadata, the real
fixture maps in `test_maps/` parse through their object records, including
nested inventory records, and whole-elevation binary replacement is covered by
parse-back fixture tests. Area/region patching and merge-in mode are still
future work.
