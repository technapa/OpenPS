# Translations

OpenSlicer shows its texts in the language selected in PrusaSlicer (*Configuration > Preferences > Language*), in
the 21 languages PrusaSlicer ships: be, ca, cs, de, en, es, fi, fr, hu, it, ja, ko, nl, pl, pt_BR, ru, sl, tr, uk,
zh_CN, zh_TW. Unknown languages fall back to English.

There are two sources of text:

1. **OpenSlicer strings**, in `src/openps_launcher/SyncIndxI18n.hpp` (`kLangs`). Everything OpenSlicer adds:
   the Sync tab, the *Set origin* menu, the ViewCube *ORIGIN* button, the window title suffix, error messages.
   Wording shared with Prusa Connect (*Assign*, *Unload*, *No material assigned*...) uses the official Prusa Connect
   translations.
2. **PrusaSlicer's catalog** (`resources/localization/<lang>/PrusaSlicer.mo`), read by `AppCatalog` / `TrApp()`
   for words PrusaSlicer already translates, so that they match the rest of the application: the ViewCube faces
   (*Top*, *Front*, ... from the *View* menu) and the context menu entries used to recognize menus.

## Adding a string

1. Add an id to `enum StrId`, just before `S_COUNT` (a comment documents its `{0}`, `{1}` placeholders).
2. Add the text **at the same position in every language block** of `kLangs` (keep the placeholders).
3. Use it with `Tr(i18n::S_MY_TEXT)` or `Fmt(i18n::S_MY_TEXT, { arg0, arg1 })`.

Each block must contain exactly `S_COUNT` strings; a missing entry shifts every following string, so check the
count when you edit a block. Keep labels short: buttons shrink their font before truncating, but card texts and
the ViewCube button have little room.

## Fixing a translation

Edit the text in its language block. Native speakers are very welcome: several translations were written without
one (see the git history).

## Testing a language

Set the environment variable `OPENPS_SYNC_LANG` before starting OpenSlicer to force the OpenSlicer strings and the
catalog to a language, without changing PrusaSlicer's own setting:

```powershell
$env:OPENPS_SYNC_LANG = "ja"; & "C:\App\OpenPS\OpenPS.exe"
```

## The French PrusaSlicer catalog

`resources/localization/fr/PrusaSlicer.mo` is PrusaSlicer's French catalog where "PrusaSlicer" is replaced by
"OpenSlicer" when it names the running application (not in compatibility or documentation messages). It is generated
by `scripts/build_assets_and_loc.py --localization`. Other languages use PrusaSlicer's catalogs unchanged.
