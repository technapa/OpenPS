# OpenSlicer

**English** · [Français](README.fr.md)

**OpenSlicer** is a Windows build of [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) 2.9.7 with a few extra tools,
made first for owners of the **Prusa CORE One INDX** (multi-tool):

- **Sync tab**: a real two-way synchronization of filaments between the printer (through Prusa Connect) and the slicer.
- **3D ViewCube** over the plater, to change the view in one click.
- **Set origin**: choose whether the camera orbits around the selected object or around the build plate.

Everything else is PrusaSlicer, unchanged: same slicing engine, same profiles, same project files.

> [!IMPORTANT]
> **This project was vibe coded.** It was written by AI coding agents, directed and tested by one maker on one
> printer. Read [How this was made](#how-this-was-made-vibe-coding) before using it or contributing.

> OpenSlicer is a community project. It is not affiliated with or endorsed by Prusa Research.
> Prusa, PrusaSlicer and Prusa Connect are trademarks of Prusa Research a.s.

![Sync tab](doc/openslicer/images/sync-tab.png)

---

## Project status

| | |
|---|---|
| Version | 0.2, experimental |
| Platform | Windows 10/11 x64 only |
| Base | Official PrusaSlicer 2.9.7 for Windows (not rebuilt) |
| Tested on | One Prusa CORE One INDX (8 tools) with firmware 8, Prusa Connect, Windows 11 |
| Releases | None yet: build it from source (a few seconds, see [Building](#building)) |

It has been used by its author since November 2026 and has been stable so far, but it has had no review by
anyone else yet. Bug reports are very welcome.

---

## Why the Sync tab?

Since firmware 8, the CORE One INDX lets you assign a material and a color to each of its tools, and Prusa Connect
shows them in its *Tools* tab. PrusaSlicer, however, still has no way to read that setup or to send its own filaments
back: you re-enter everything by hand on one side or the other.

The Sync tab shows both sides next to each other, styled like the Prusa Connect *Tools* tab:

| Panel | Content | Actions |
|---|---|---|
| **Filaments installed in &lt;printer&gt;** | One card per tool, read live from Prusa Connect: material, brand, color, nozzle (0.4 HF...), tool in the print head, remaining weight | **Assign / Change** (same material catalogue as Connect, or a custom type + color), **Unload** |
| **Active filaments in OpenSlicer** | One card per filament of the active printer profile: profile, color | **Change** (compatible profile for a material type), **Color** |

and two buttons:

- **Send to OpenSlicer**: tool *N* of the printer → filament *N* of the slicer (color + a compatible filament profile).
- **Send to &lt;printer&gt;**: filament *N* of the slicer → tool *N* of the printer in Prusa Connect. When a Connect
  catalogue material has the same brand, type and color it is used; otherwise a custom material named
  `"<name> #RRGGBB"` is created once in your Connect warehouse and reused.

Nothing is ever sent to the printer as G-code: OpenSlicer uses the same Prusa Connect calls as its web application
(see [Prusa Connect API](doc/openslicer/ARCHITECTURE.md#prusa-connect-api)). As Connect itself explains, an assignment
records which material is on a tool; it does not load filament or change what the printer's own screen displays.

The tab works in the 21 languages of PrusaSlicer and follows the language chosen in *Configuration > Preferences*.

![Assign a material](doc/openslicer/images/assign-material.png)

## ViewCube and Set origin

![ViewCube](doc/openslicer/images/plater-viewcube.png)

- Click a face or an axis of the cube to look from that side, **ISO** for the isometric view.
- **ORIGIN** (also *Set origin* at the bottom of the 3D view and object list context menus):
  - *Center of active object*: the camera pivots around the selected object and follows the selection.
  - *Center of build plate*: back to the PrusaSlicer behavior.

---

## Your account and your printer

OpenSlicer touches your Prusa account and your printer only through Prusa Connect, and only when you ask:

- **Login**: OpenSlicer reuses the session of PrusaSlicer (Windows Credential Manager). It only **reads** the access
  token, never writes or refreshes it, and never sees your password.
- **Reading**: the printer list and the tools are read when you open the Sync tab or click *Refresh*. There is no
  background polling and no access to the local network.
- **Writing**: only *Assign / Change*, *Send to &lt;printer&gt;* (material assignments, custom materials in your
  warehouse) and *Unload* (only when the printer is idle). All of them can also be done or undone from Prusa Connect.
- **Undocumented API**: Prusa Connect has no public API for this. OpenSlicer uses the internal calls of the Connect
  web application; if Prusa changes them, the Sync tab may stop working (your account and printer are not affected).

---

## Installing

OpenSlicer needs **PrusaSlicer 2.9.7 for Windows x64** installed (it reuses its `PrusaSlicer.dll` and resources).

1. Install [PrusaSlicer 2.9.7](https://github.com/prusa3d/PrusaSlicer/releases/tag/version_2.9.7).
2. Build OpenSlicer (below) with `-Publish`.
3. Start `OpenPS.exe`. Settings, profiles and the Prusa Account login are shared with PrusaSlicer
   (`%APPDATA%\PrusaSlicer`). PrusaSlicer itself keeps working and can stay open.
4. For the Sync tab, log in to your Prusa Account in the **Prusa Connect** tab once.

If `PrusaSlicer.dll` is not the 2.9.7 Windows x64 release, OpenSlicer still starts, tells you once, and disables the
features that depend on PrusaSlicer internals (ViewCube, Set origin, automatic filament profile selection).

## Building

Requirements: Windows 10/11 x64, Visual Studio 2019 or newer with *Desktop development with C++*,
PowerShell 5+, PrusaSlicer 2.9.7 installed.

```powershell
# build only -> build\openslicer\OpenPS.exe
powershell -ExecutionPolicy Bypass -File scripts\build_launcher.ps1

# build and install to C:\App\OpenPS (closes a running OpenSlicer from that folder, never forced)
powershell -ExecutionPolicy Bypass -File scripts\build_launcher.ps1 -Publish -InstallDir C:\App\OpenPS
```

The launcher is a single C++17 translation unit and builds in a few seconds. PrusaSlicer itself is **not** rebuilt,
so none of the PrusaSlicer dependencies (`deps/`) are needed.

Branded resources (recolored icons, splash screen, French catalog) are already in `resources/`. To regenerate them
from a PrusaSlicer install: `python scripts\build_assets_and_loc.py --help` (requires Pillow).

## How it works

`OpenPS.exe` loads the official `PrusaSlicer.dll` and calls its `slic3r_main()` entry point, exactly like
`prusa-slicer.exe` does. Before that, it installs a few import hooks and subclasses PrusaSlicer windows to add the
OpenSlicer features; for the ViewCube and Set origin it calls PrusaSlicer functions whose addresses are known for the
2.9.7 build. Details: **[doc/openslicer/ARCHITECTURE.md](doc/openslicer/ARCHITECTURE.md)**.

| Path | Content |
|---|---|
| `src/openps_launcher/` | All OpenSlicer code (launcher, Sync tab, Prusa Connect client, translations) |
| `scripts/` | Build, install and resource generation scripts |
| `resources/icons`, `resources/localization/fr` | Branded resources installed over PrusaSlicer's |
| `patches/` | Experimental changes for a full source build (not used by the launcher) |
| `doc/openslicer/` | Developer documentation |
| everything else | PrusaSlicer 2.9.7 sources, untouched ([original README](doc/PrusaSlicer-README.md)) |

The git history keeps the full PrusaSlicer history up to the `version_2.9.7` tag; OpenSlicer is the commits on top.

---

## How this was made (vibe coding)

OpenSlicer was **vibe coded**: its author ([Tech Napa](https://github.com/technapa), a maker and 3D printing
YouTuber, not a professional C++ developer) described the features, tested each build on a real printer and
reported what was wrong; AI coding agents wrote the code.

- **Prototype**: [Google Antigravity](https://antigravity.google/) (Gemini). It produced the first launcher, the
  first Sync tab and the ViewCube. The Sync worked from printer to slicer, but the other direction uploaded a G-code
  file that the printer ran as a print job and that rewrote its per-tool filament settings.
- **Rewrite and clean-up**: [Claude Code](https://claude.com/claude-code) (Claude Opus). It rewrote the Sync tab and
  its Prusa Connect client from scratch (Connect styling, assignments through the same GraphQL calls as the Connect
  web app, no G-code at all, read-only use of the session), translated every added text into the 21 languages,
  added the DLL build check, the build scripts and this documentation. Commits made this way carry a
  `Co-Authored-By: Claude` line.
- **PrusaSlicer internals**: the function addresses and object layouts in
  [`PrusaSlicerDll.hpp`](src/openps_launcher/PrusaSlicerDll.hpp) were found by the agents by comparing the PrusaSlicer
  2.9.7 sources with the disassembly of the official DLL.
- **Prusa Connect calls**: copied from the requests of the Connect web application, not from a documentation.

What this means for you:

- The code has been **read by its author, tested on real hardware, and built without warnings (`/W4`)**, but it has
  **not been reviewed by an experienced C++ or Windows developer**. Treat it as you would any young hobby project.
- **Verified**: Sync tab reading and *Send to printer* / *Send to OpenSlicer* on a CORE One INDX with 8 tools; UI in
  English, French, German and Japanese; the DLL check; ViewCube and Set origin at 150 % display scaling.
- **Not verified yet**: *Unload* from the Sync tab, changing the language while OpenSlicer runs, other printers
  (single-tool printers should work but were not tried), Windows 10.
- The translations other than English and French were written by the AI without a native speaker; corrections are
  welcome ([TRANSLATIONS.md](doc/openslicer/TRANSLATIONS.md)).

Human reviews, tests and pull requests, AI-assisted or not, are welcome. If you use an AI agent, please say so in
your pull request.

---

## Contributing

Issues and pull requests are welcome. Useful entry points:

- [Architecture](doc/openslicer/ARCHITECTURE.md): how the launcher, the hooks and the Sync tab fit together.
- [Translations](doc/openslicer/TRANSLATIONS.md): adding or fixing a string in the 21 languages.
- [Porting to another PrusaSlicer version](doc/openslicer/UPDATING_PRUSASLICER.md).

Please keep the code in English (comments and identifiers), build without warnings (`/W4`), and add new user-visible
text to `SyncIndxI18n.hpp` in every language. Never commit tokens, keys or anything from your Prusa account.

### Known limitations

- Windows x64 only, PrusaSlicer 2.9.7 only for the ViewCube / Set origin (see above).
- Outside French, some PrusaSlicer texts still name "PrusaSlicer" (only the French catalog is rebranded).
- The slicer colors are read from the filament combo boxes of the plater; open the Sync tab from the plater after
  changing a color so that it is read again.
- The Sync tab depends on undocumented Prusa Connect calls (see above).

## License

OpenSlicer is free software under the **GNU Affero General Public License v3** ([LICENSE](LICENSE)), like PrusaSlicer.
PrusaSlicer is © Prusa Research and contributors, based on Slic3r by Alessandro Ranellucci and the RepRap community.
