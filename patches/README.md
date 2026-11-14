# Experimental source-level changes

`source-build-experimental.patch` is **not used** by OpenSlicer as it is built today (the launcher in
`src/openps_launcher` works with the official `PrusaSlicer.dll`).

It is an early attempt at building OpenSlicer from the full PrusaSlicer sources:

- *Set origin* submenu (center of active object / center of build plate) in `GUI_Factories.cpp`, `Plater.cpp`,
  `GLCanvas3D.cpp`, `CameraUtils.cpp`,
- application name, version and Windows resources (`version.inc`, `PrusaSlicer.rc.in`),
- accepting Visual Studio 2026 in `build_win.bat`.

It has never been compiled or tested. Apply it on the PrusaSlicer 2.9.7 sources with:

```bash
git apply patches/source-build-experimental.patch
```
