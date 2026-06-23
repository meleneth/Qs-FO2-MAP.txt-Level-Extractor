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

Use is simple, drag and drop only.
<br>Drag and drop a Fallout 2 "map_name.txt" file onto one side or the other.
<br>Select the level you want to extract from on either side, and the level to extract to in the middle.
<br>Click the arrow for the appropriate side to set the indicator to show which level is extracted where.
<br>Select which map header you want to use (there's a bunch of information in the header that might be necessary depending on the map variables used in the scripts attached to any objects/spatials being exported on a level.
<br>Type in a filename and click "Export".

The Export will automatically add a "Q.txt" to the filename so files aren't over-written.
<br>You should be able to rename this however you want, but be careful not to over-write an old map until you know the new one works.

## Binary `.map` prototype data

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
