Developed with supplemental AI assistance
# Double Items for Pulsar

Adds a second item slot, physical double item boxes, faster box respawns and a two-second regular-box pickup cooldown. 
No extra assets required in any .szs files


## Installation


### 1. Copy the source folders

Copy the contents of `PulsarEngine/` and `GameSource/` into the matching folders in your project. Keep the folder paths; do not delete the other files already in those folders.

On a clean Pulsar project, replace matching files. If you have already edited them, compare the files and keep your own changes when adding ours.

### 2. Add the symbols

Add the entries from this folder's `symbols.txt` to your project's `GameSource/symbols.txt`. Keep one entry per name, using the supplied value. Do not replace your whole symbol file with this small list.

### 3. Update the version entries

Using base Pulladium? Follow `gameversionspulladium.txt` instead of `gameversions.txt`. That list is based on the Pulladium version file supplied with this package work. Both lists use the same remove/add instructions; apply only the one matching your starting project.

Open this folder's `gameversions.txt` beside your project's `GameSource/versions.txt`. 


Some existing ranges must be replaced, not just appended. Removing the marked lines first prevents overlapping mappings. If you previously changed one of those ranges yourself, compare it before replacing it.

The two modules include the same list. Apply it once even when installing both.

### 4. Configure the module

Open `PulsarEngine/DoubleItems/Config.hpp` to enable/disable Double Items and adjust item boxes, cooldown and the second item window.

## Compatibility and testing

Offline GP/VS/battle with native items. Online not supported yet. 


