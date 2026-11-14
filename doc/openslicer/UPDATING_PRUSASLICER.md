# Porting OpenSlicer to another PrusaSlicer release

OpenSlicer runs on top of the official `PrusaSlicer.dll`. Most features only use Windows APIs and window structure
and keep working on another release; a few call PrusaSlicer functions directly or patch its code, and those depend on
the exact binary. They are gathered in `src/openps_launcher/PrusaSlicerDll.hpp` and in `PatchBrandingAndModernTheme()`
(`OpenPS_launcher.cpp`).

| Feature | Depends on the DLL build |
|---|---|
| Sync tab (Prusa Connect, cards, colors) | No |
| Automatic filament profile when sending to the slicer | Yes: `kRvaWxGetApp`, `kRvaGuiAppSelectFilamentConnect` |
| ViewCube, *Set origin* | Yes: camera, plater, selection, bed functions and layouts |
| Branding of splash screen, About dialog, menus; accent colors | Yes: code and data patches |
| Window titles, button style | No |

With an unsupported DLL, OpenSlicer warns once and disables the build-dependent features (`dll::At()` returns
`nullptr`), so it never calls a wrong address.

## Steps

1. **Identify the build.** Read the PE header of the new `PrusaSlicer.dll` (`IMAGE_FILE_HEADER::TimeDateStamp`,
   `IMAGE_OPTIONAL_HEADER::SizeOfImage`), e.g. with `dumpbin /headers PrusaSlicer.dll`, and update
   `kSupportedVersion`, `kSupportedTimeStamp`, `kSupportedSizeOfImage` in `PrusaSlicerDll.hpp` and the same values in
   `scripts/publish.ps1`.

2. **Find the functions.** PrusaSlicer is open source but its Windows release has no public symbols, so each address
   is found by matching the source of the same tag with the disassembly (Ghidra, IDA, x64dbg...). Useful anchors:
   - string literals referenced by the function (e.g. `"iso"`, `"top"` near `Plater::select_view`,
     the version check URL near `GUI_App::app_version_check`),
   - calls between functions you already found (`wxGetApp()` is called almost everywhere),
   - exported `slic3r_main` as a starting point.
   Write the new RVAs (address minus image base) in `PrusaSlicerDll.hpp`.

3. **Check the object layouts.** Offsets such as `Plater::p`, `GLCanvas3D::m_selection` or `Camera::m_view_matrix`
   change when members are added. Read them from the instructions of small accessors (`Plater::get_camera`,
   `GLCanvas3D::zoom_to_bed`, `Selection::get_bounding_box`) and update the `kOff*` constants.

4. **Re-check the patches.** Every code patch in `PatchBrandingAndModernTheme()` verifies the original bytes before
   writing; on a new build they are simply skipped. Locate the same instructions again (splash screen colors,
   `SplashScreen::ConstantText::init`, About dialog, menu labels, `GUI_App::init_ui_colours`) and update the
   addresses. The ImGui color tables are found by value and need no address.

5. **Test** the ViewCube (every face, ISO, ORIGIN), *Set origin* in the 3D view and object list menus, *Send to
   OpenSlicer* (profile selection) and the splash screen / About dialog, in light and dark mode.

The window-structure code (top bar buttons, sidebar filament combo boxes, swatch layout) is documented where it is
used in `SyncIndxTab.hpp`; check it too if PrusaSlicer changes its sidebar or top bar.
