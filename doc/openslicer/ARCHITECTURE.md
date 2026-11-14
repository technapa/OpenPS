# OpenSlicer architecture

OpenSlicer adds features to PrusaSlicer **without rebuilding it**. The official Windows release of PrusaSlicer is
split in two: a tiny `prusa-slicer.exe` and `PrusaSlicer.dll`, which contains the whole application and exports
`slic3r_main()`. OpenSlicer replaces the small executable with its own, `OpenPS.exe`, and changes PrusaSlicer's
behavior from inside the process.

```
OpenPS.exe (src/openps_launcher)
 ├─ LoadLibrary("PrusaSlicer.dll")          official PrusaSlicer 2.9.7, unmodified file
 ├─ check the DLL build                     PrusaSlicerDll.hpp: PE time stamp + image size
 ├─ in-memory patches (supported build)     branding, splash colors, accent colors
 ├─ IAT hooks on PrusaSlicer.dll imports    CreateWindowExW, SetWindowTextW, TrackPopupMenu(Ex), ChooseColorW
 ├─ WindowMonitorThread                     finds the main frame, keeps title and icon
 └─ slic3r_main(argc, argv)                 PrusaSlicer runs; OpenSlicer reacts through hooks
      └─ main frame subclass (30 ms timer)
           ├─ ViewCube window + "Set origin"      direct calls into PrusaSlicer.dll
           ├─ push button restyling
           ├─ Sync tab (SyncIndxTab.hpp)          native Win32 window over the PrusaSlicer page
           │    └─ SyncEngine worker thread       Prusa Connect REST + GraphQL (SyncIndxEngine.hpp)
           └─ language follow-up                  SyncIndxI18n.hpp
```

Everything is confined to the OpenSlicer process: no file of the PrusaSlicer installation is modified, and the
user's settings, profiles and Prusa Account session are the ones of PrusaSlicer (`%APPDATA%\PrusaSlicer`).

## Source files

| File | Role |
|---|---|
| `OpenPS_launcher.cpp` | Entry point, DLL loading and patches, hooks, main frame subclass, branding, button restyling, ViewCube and *Set origin* |
| `PrusaSlicerDll.hpp` | The only place that knows PrusaSlicer internals: supported build identity, function addresses (RVA), object layouts. `dll::At(rva)` returns `nullptr` for an unsupported DLL |
| `SyncIndxTab.hpp` | *Sync* tab: top bar button, workspace window, Prusa Connect styled cards and buttons, *Assign a material* dialog, bridge to the PrusaSlicer sidebar |
| `SyncIndxEngine.hpp` | Data model, JSON parser, WinHTTP client, Prusa Account session reader, Prusa Connect calls, worker thread |
| `SyncIndxI18n.hpp` | OpenSlicer strings in 21 languages, language detection, reader for PrusaSlicer's `.mo` catalogs |
| `OpenPS.rc`, `OpenPS.manifest` | Version info, icon, per-monitor DPI awareness, common controls v6 |

All code is header-only except the launcher, so the program is one translation unit (`cl OpenPS_launcher.cpp`).

## Startup sequence (`wWinMain`)

1. GDI+ startup, command line forwarded unchanged.
2. `LoadLibraryExW(<exe dir>\PrusaSlicer.dll)`. A missing DLL shows a translated error.
3. `dll::Supported() = IsSupportedPrusaSlicerDll()`. When it is false a one-time warning is shown
   (`[Launcher] WarnedDll` in `openps_sync.ini`) and every feature using `dll::At()` stays off.
4. `PatchBrandingAndModernTheme()` (supported build only). Each code patch checks the original instruction bytes
   before writing, then flushes the instruction cache.
5. IAT hooks are installed in `PrusaSlicer.dll`'s import table.
6. `WindowMonitorThread` is started, then `slic3r_main()` runs until the application exits.

## Hooks and subclassing

| Hook | Purpose |
|---|---|
| `CreateWindowExW` | Rebrands window titles, restyles push buttons as they are created |
| `SetWindowTextW` | Detects the main frame (`AttachMainFrameHooks`), rebrands titles, restyles buttons |
| `TrackPopupMenu` / `TrackPopupMenuEx` | Appends the *Set origin* submenu to the 3D view and object context menus, dispatches its commands (wxWidgets would drop unknown command ids) |
| `ChooseColorW` | Lets the Sync tab set an extruder color without showing the color dialog (`g_autoChooseColorActive`) |

`MainWndSubclassProc` runs a 30 ms timer on the GUI thread. It keeps the ViewCube positioned, follows the selection
when *Center of active object* is on, restyles new buttons and maintains the Sync tab button (every 150 ms), and
checks for a language change (every ~2 s).

Context menus are recognized by their owner (`wxGLCanvas`) or by their entries (*Add Shape*, *Add part*, *Delete*,
*Split*), compared with PrusaSlicer's own translations so that detection works in every language.

## ViewCube and Set origin

The ViewCube is a layered top-level window (`OpenSlicer_ViewCube3D`) owned by the main frame and placed over the
top-right corner of the 3D canvas. It reads the camera rotation (`Camera::m_view_matrix`) to draw the cube and calls
`Plater::select_view("top"|"front"|...)` when a face is clicked. Its layout is defined at 96 DPI and scaled with the
DPI of the main window. Face names come from PrusaSlicer's *View* menu translations.

*Set origin* calls `Camera::set_target()` with either the center of the selection bounding box
(`Selection::get_bounding_box()`) or the center of the active bed (`Bed3D` box + `MultipleBeds` offset). While
*Center of active object* is active, the timer moves the target when the selection changes.

## Sync tab

### User interface

`SyncTabManager` inserts an owner-drawn **Sync** button after the last button of PrusaSlicer's top bar
(`TopBarItemsCtrl`). Selecting it hides the visible PrusaSlicer page and shows `OpenPS_SyncWorkspace`, a native
window covering the same area; clicking any native tab, or a page shown by a shortcut, gives control back.

The workspace paints its texts, cards and badges with GDI+ (double buffered) and hosts real `BUTTON` controls
(`BS_OWNERDRAW`) whose label is the window text. Button widths are measured from the translated label; cards are
laid out 8, 4 or 2 per row depending on the width, and the page scrolls with the mouse wheel. Colors and metrics
follow the Prusa Connect stylesheet (light and dark themes, detected from PrusaSlicer's top bar).

### Slicer side

The filament list is read from the PrusaSlicer sidebar: the filament combo boxes are found by window structure,
their text is the selected profile, and the extruder color is read from the rendered swatch (`PrintWindow`).
Colors are only sampled while the plater is visible; afterwards the values read when the tab was opened are kept.

Writing to the slicer uses PrusaSlicer's own paths:

- **color**: a click on the color swatch of the combo box, with the `ChooseColorW` hook answering the color;
- **profile**: `GUI_App::select_filament_from_connect()` with `{"tools":{"<n>":{"material":"PETG",...}}}`, the
  function PrusaSlicer uses when a printer is selected from Prusa Connect (supported DLL only).

### Prusa Connect API

`SyncEngine` runs every network call on one worker thread and posts `WM_OPENPS_SYNC_EVENT` to the workspace.

| Call | Used for |
|---|---|
| `GET connect.prusa3d.com/slicer/v2/printers` (fallback `/app/printers`) | Printers of the account |
| `GET /app/printers/{uuid}` | State, firmware, tool count, team |
| `GET /app/users/teams/{team_id}` | Organization of the team (GraphQL needs it) |
| GraphQL `GetPrinterFffTelemetry` | Tools as in the Connect *Tools* tab: ids, nozzles, assigned material, color, brand, weights, NFC |
| GraphQL `WarehouseMaterials` | Material catalogue (*Assign a material* dialog, color matching) |
| GraphQL `MaterialBrands` | Brand identifiers for custom materials |
| GraphQL `CreateCustomMaterial` | Custom material (`"<name> #RRGGBB"`), same UUIDv5 identifiers as the Connect warehouse form |
| GraphQL `UpdatePrinterMaterial` | Assigns a material to a tool |
| `POST /app/printers/{uuid}/commands/sync` `UNLOAD_FILAMENT` | *Unload* (printer idle only) |

GraphQL documents are copies of those used by the Prusa Connect web application (`connect-api.prusa3d.com/graphql`).
Nothing is uploaded to the printer and no G-code is sent.

### Authentication

PrusaSlicer stores its Prusa Account session in the Windows Credential Manager (generic credential
`PrusaSlicer/PrusaAccount/tokens`, value `access_token|refresh_token|expiry|pid`). OpenSlicer **reads** the access
token and never writes or refreshes it: PrusaSlicer owns the session and refreshes it while it runs. If the token is
missing or expired, the tab asks the user to log in from the *Prusa Connect* tab.

### Settings

`%APPDATA%\PrusaSlicer\openps_sync.ini`:

| Key | Meaning |
|---|---|
| `[Sync] SelectedUuid` | Printer shown in the Sync tab |
| `[Launcher] WarnedDll` | Unsupported DLL build already reported to the user |

## Translations

See [TRANSLATIONS.md](TRANSLATIONS.md). The language is PrusaSlicer's (`translation_language` in `PrusaSlicer.ini`),
re-read when PrusaSlicer changes it at run time.

## Porting to another PrusaSlicer version

See [UPDATING_PRUSASLICER.md](UPDATING_PRUSASLICER.md).
