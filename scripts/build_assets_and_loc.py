# SPDX-License-Identifier: AGPL-3.0-or-later
"""Builds the OpenSlicer branded resources from an installed PrusaSlicer 2.9.7.

    python scripts/build_assets_and_loc.py [--prusaslicer "C:/Program Files/Prusa3D/PrusaSlicer"]
                                           [--icons] [--splash] [--localization]

--icons         copies resources/icons from PrusaSlicer, recolors the orange accents
                (#ED6B21...) to the OpenSlicer red (#E02424) and installs the app icon.
--splash        draws resources/icons/splashscreen*.jpg (right half of the splash screen;
                the left half is painted by PrusaSlicer itself).
--localization  writes resources/localization/fr/PrusaSlicer.mo: the French catalog of
                PrusaSlicer with "PrusaSlicer" replaced by "OpenSlicer" where it names the
                running application (other languages use PrusaSlicer's catalogs unchanged).
Without option, all three steps run. Requires Pillow (pip install pillow).
"""
import argparse
import math
import os
import re
import shutil
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT_DIR = Path(__file__).resolve().parents[1]
ICONS_DST = ROOT_DIR / "resources" / "icons"
LOC_DST = ROOT_DIR / "resources" / "localization" / "fr"
PRUSA_INSTALL = Path(os.environ.get("PRUSASLICER_DIR", r"C:\Program Files\Prusa3D\PrusaSlicer"))

PRESERVE_CUSTOM = {
    "OpenPS.ico",
    "OpenPS.png",
    "OpenPS.svg",
    "PrusaSlicer.ico",
    "PrusaSlicer.png",
    "PrusaSlicer.svg",
    "PrusaSlicer_128px.png",
    "PrusaSlicer_192px.png",
    "PrusaSlicer_192px_transparent.png",
    "PrusaSlicer-gcodeviewer.ico",
    "PrusaSlicer-gcodeviewer_128px.png",
    "PrusaSlicer-gcodeviewer_192px.png",
    "splashscreen.jpg",
    "splashscreen-gcodepreview.jpg",
}

PRESERVE_RUNTIME_TEMPLATE_SVGS = {
    "mode.svg",
    "connect_status.svg",
}


# ---------------------------------------------------------------------------
# 1. Icons
# ---------------------------------------------------------------------------
def sync_icons():
    src_icons = PRUSA_INSTALL / "resources" / "icons"
    copied_count = 0
    recolored_svg_count = 0

    if src_icons.exists():
        for item in src_icons.iterdir():
            dst_item = ICONS_DST / item.name
            if item.is_dir():
                if dst_item.exists():
                    shutil.rmtree(dst_item)
                shutil.copytree(item, dst_item)
                continue

            if item.name in PRESERVE_CUSTOM and dst_item.exists():
                continue

            if item.suffix.lower() == ".svg" and item.name not in PRESERVE_RUNTIME_TEMPLATE_SVGS:
                svg_text = item.read_text(encoding="utf-8", errors="ignore")
                orig_text = svg_text
                # Prusa orange shades -> OpenSlicer reds
                svg_text = re.sub(r"#ed6b21", "#E02424", svg_text, flags=re.IGNORECASE)
                svg_text = re.sub(r"#eb6b21", "#E02424", svg_text, flags=re.IGNORECASE)
                svg_text = re.sub(r"#ed8d21", "#EF4444", svg_text, flags=re.IGNORECASE)
                svg_text = re.sub(r"#fa6831", "#E02424", svg_text, flags=re.IGNORECASE)
                svg_text = re.sub(r"#bf6637", "#B91C1C", svg_text, flags=re.IGNORECASE)
                if svg_text != orig_text:
                    recolored_svg_count += 1
                dst_item.write_text(svg_text, encoding="utf-8")
            else:
                shutil.copy2(item, dst_item)
            copied_count += 1

    # Refine PrusaSlicer.svg & OpenPS.svg so it renders with razor-sharp contrast
    # on BOTH the dark #181A20 SplashScreen and the light #FFFFFF AboutDialog
    OPENPS_BADGE_SVG = """<svg xmlns="http://www.w3.org/2000/svg" width="800" height="800" viewBox="0 0 800 800">
      <circle cx="400" cy="400" r="390" fill="#ffffff" stroke="#d0d7e2" stroke-width="14"/>
      <!-- Upper-left half: anthracite (#24272E) -->
      <path d="M 588.09 192.11 A 280 280 0 0 0 192.11 588.09 L 287.57 492.63 A 145 145 0 0 1 492.63 287.57 Z" fill="#24272E"/>
      <!-- Lower-right half: OpenSlicer red (#E02424) -->
      <path d="M 211.91 607.89 A 280 280 0 0 0 607.89 211.91 L 512.43 307.37 A 145 145 0 0 1 307.37 512.43 Z" fill="#E02424"/>
    </svg>
    """
    (ICONS_DST / "PrusaSlicer.svg").write_text(OPENPS_BADGE_SVG, encoding="utf-8")
    (ICONS_DST / "OpenPS.svg").write_text(OPENPS_BADGE_SVG, encoding="utf-8")
    (ICONS_DST / "PrusaSlicer-gcodeviewer.svg").write_text(OPENPS_BADGE_SVG, encoding="utf-8")

    # Also copy OpenPS PNG icons over PrusaSlicer_*px.png so any PNG fallback matches Red & Black
    if (ICONS_DST / "OpenPS.png").exists():
        base_icon = Image.open(ICONS_DST / "OpenPS.png").convert("RGBA")
        for sz_name, sz in [
            ("PrusaSlicer.png", 256),
            ("PrusaSlicer_192px.png", 192),
            ("PrusaSlicer_192px_transparent.png", 192),
            ("PrusaSlicer_128px.png", 128),
            ("PrusaSlicer-gcodeviewer_192px.png", 192),
            ("PrusaSlicer-gcodeviewer_128px.png", 128),
        ]:
            base_icon.resize((sz, sz), Image.Resampling.LANCZOS).save(ICONS_DST / sz_name, "PNG")
    if (ICONS_DST / "OpenPS.ico").exists():
        shutil.copy2(ICONS_DST / "OpenPS.ico", ICONS_DST / "PrusaSlicer.ico")
        shutil.copy2(ICONS_DST / "OpenPS.ico", ICONS_DST / "PrusaSlicer-gcodeviewer.ico")

    print(f"Icons synced: {copied_count} files, {recolored_svg_count} SVGs recolored to OpenPS Red (#E02424).")


# ---------------------------------------------------------------------------
# 2. Splash screen artwork (600x600 right panel, supersampled 2x).
#    The left 400x600 panel (logo, name, version, status, credits) is drawn by
#    PrusaSlicer's SplashScreen::MakeBitmap with the colors patched by the launcher.
#    Texts are in English on purpose: the image cannot follow the UI language.
# ---------------------------------------------------------------------------
def load_font(name: str, size: int):
    win_fonts = Path(r"C:\Windows\Fonts")
    p = win_fonts / name
    if p.exists():
        return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


def generate_right_splash(output_path: Path, badge_text: str, is_gcode_viewer: bool = False):
    SS = 2  # 2x supersampling
    W, H = 600 * SS, 600 * SS
    img = Image.new("RGB", (W, H), (24, 26, 32))
    px = img.load()

    # Smooth horizontal gradient starting at exact RGB(24, 26, 32) at x=0 (100% seamless with left 400px panel!)
    for y in range(H):
        ty = y / float(H - 1)
        for x in range(W):
            tx = x / float(W - 1)
            # Subtle architectural darkening towards right edge
            ease = tx * tx * (3.0 - 2.0 * tx)
            r = int(round(24 - 6 * ease + 2 * (1.0 - abs(ty - 0.45))))
            g = int(round(26 - 6 * ease + 3 * (1.0 - abs(ty - 0.45))))
            b = int(round(32 - 5 * ease + 5 * (1.0 - abs(ty - 0.45))))
            px[x, y] = (max(0, r), max(0, g), max(0, b))

    draw = ImageDraw.Draw(img, "RGBA")

    # Subtle vertical architectural divider separating Left Info Column (400px) and Right 3D Stage (600px)
    div_x = 8 * SS
    draw.line([(div_x, 28 * SS), (div_x, 572 * SS)], fill=(46, 52, 64, 220), width=1 * SS)
    # Crimson Red accent bar aligned with the OpenSlicer title height on the left panel
    draw.line([(div_x, 128 * SS), (div_x, 198 * SS)], fill=(224, 36, 36, 255), width=2 * SS)

    # Isometric projection helper centered with generous margins inside the 600x600 right stage
    cx, cy = 304 * SS, 344 * SS
    cos30 = 0.8660254
    sin30 = 0.5000000

    def iso(x_mm: float, y_mm: float, z_mm: float = 0.0):
        sx = cx + (x_mm - y_mm) * cos30 * SS
        sy = cy + (x_mm + y_mm) * sin30 * 0.56 * SS - z_mm * SS
        return (sx, sy)

    # 1. Isometric CAD Build Plate (Anthracite-Black bed with precision grid, 100% inside margins)
    bed_half_x = 154.0
    bed_half_y = 138.0

    # Subtle shadow beneath build plate
    shadow_pts = [
        iso(-bed_half_x, -bed_half_y, -10),
        iso(+bed_half_x, -bed_half_y, -10),
        iso(+bed_half_x, +bed_half_y, -10),
        iso(-bed_half_x, +bed_half_y, -10),
    ]
    draw.polygon(shadow_pts, fill=(10, 11, 15, 190))

    # Bed thickness rim
    bed_top_pts = [
        iso(-bed_half_x, -bed_half_y, 0),
        iso(+bed_half_x, -bed_half_y, 0),
        iso(+bed_half_x, +bed_half_y, 0),
        iso(-bed_half_x, +bed_half_y, 0),
    ]
    bed_bot_pts = [
        iso(-bed_half_x, -bed_half_y, -6),
        iso(+bed_half_x, -bed_half_y, -6),
        iso(+bed_half_x, +bed_half_y, -6),
        iso(-bed_half_x, +bed_half_y, -6),
    ]
    # Front-left and front-right bevel sides
    draw.polygon([bed_top_pts[1], bed_top_pts[2], bed_bot_pts[2], bed_bot_pts[1]], fill=(36, 40, 49, 255))
    draw.polygon([bed_top_pts[2], bed_top_pts[3], bed_bot_pts[3], bed_bot_pts[2]], fill=(28, 31, 39, 255))

    # Main build plate surface
    draw.polygon(bed_top_pts, fill=(30, 33, 41, 255), outline=(82, 92, 110, 255), width=2 * SS)

    # Fine CAD grid on the build plate
    steps = 10
    for i in range(1, steps):
        u = -bed_half_x + (2.0 * bed_half_x * i / steps)
        v = -bed_half_y + (2.0 * bed_half_y * i / steps)
        is_major = (i == steps // 2)
        col = (68, 76, 92, 210) if is_major else (44, 49, 60, 165)
        w_line = (2 if is_major else 1) * SS
        draw.line([iso(u, -bed_half_y, 0), iso(u, +bed_half_y, 0)], fill=col, width=w_line)
        draw.line([iso(-bed_half_x, v, 0), iso(+bed_half_x, v, 0)], fill=col, width=w_line)

    # Center origin marker on build plate
    c0 = iso(0, 0, 0)
    r_dot = 3.5 * SS
    draw.ellipse([c0[0] - r_dot, c0[1] - r_dot, c0[0] + r_dot, c0[1] + r_dot], fill=(224, 36, 36, 220))

    # Front corner XYZ coordinate triad
    ox, oy = -bed_half_x + 18, +bed_half_y - 18
    o_pt = iso(ox, oy, 1)
    draw.line([o_pt, iso(ox + 48, oy, 1)], fill=(224, 36, 36, 255), width=3 * SS)      # X Red
    draw.line([o_pt, iso(ox, oy - 48, 1)], fill=(46, 160, 67, 255), width=3 * SS)      # Y Green
    draw.line([o_pt, iso(ox, oy, 44)], fill=(56, 139, 253, 255), width=3 * SS)         # Z Blue

    # 2. Sliced 3D Engineering Part (Clean stepped octagon/chamfered CAD block with active Crimson Red toolpaths)
    def chamfer_rect_pts(hx: float, hy: float, ch: float, z: float):
        coords = [
            (-hx + ch, -hy), (+hx - ch, -hy),
            (+hx, -hy + ch), (+hx, +hy - ch),
            (+hx - ch, +hy), (-hx + ch, +hy),
            (-hx, +hy - ch), (-hx, -hy + ch),
        ]
        return [iso(px_, py_, z) for (px_, py_) in coords]

    num_layers = 22
    for lyr in range(num_layers):
        z = 2.0 + lyr * 5.2
        # Two-tier architectural CAD profile
        if lyr < 10:
            t = lyr / 10.0
            hx = 96.0 - t * 6.0
            hy = 84.0 - t * 5.0
            ch = 22.0
        else:
            t = (lyr - 10) / 12.0
            hx = 78.0 - t * 12.0
            hy = 68.0 - t * 10.0
            ch = 18.0

        poly = chamfer_rect_pts(hx, hy, ch, z)

        if lyr == num_layers - 1:
            # Active top slicing plane: Crimson Red (#E02424) outer & inner perimeters + rectilinear infill!
            draw.polygon(poly, fill=(44, 24, 28, 240), outline=(224, 36, 36, 255), width=3 * SS)
            for inset in (8.0, 16.0):
                inner_poly = chamfer_rect_pts(hx - inset, hy - inset, max(6.0, ch - inset * 0.6), z)
                draw.polygon(inner_poly, outline=(244, 67, 54, 235), width=2 * SS)

            # Diagonal infill toolpath lines inside inner core
            infill_lim_x = hx - 22.0
            infill_lim_y = hy - 22.0
            for k in range(-5, 6):
                offset = k * 14.0
                x1 = max(-infill_lim_x, -infill_lim_y + offset)
                y1 = x1 - offset
                x2 = min(+infill_lim_x, +infill_lim_y + offset)
                y2 = x2 - offset
                if x1 < x2 and abs(y1) <= infill_lim_y + 1 and abs(y2) <= infill_lim_y + 1:
                    col_inf = (224, 36, 36, 210) if not is_gcode_viewer else (248, 113, 113, 230)
                    draw.line([iso(x1, y1, z), iso(x2, y2, z)], fill=col_inf, width=2 * SS)

            # Center bore hole on the CAD part
            bore_pts = [
                iso(20.0 * math.cos(a * math.pi / 12.0), 20.0 * math.sin(a * math.pi / 12.0), z)
                for a in range(24)
            ]
            draw.polygon(bore_pts, fill=(24, 27, 34, 255), outline=(224, 36, 36, 255), width=2 * SS)
        else:
            # Lower sliced layers: crisp Anthracite-Slate CAD shading
            shade = 42 + int(lyr * 2.1)
            fill_c = (shade - 4, shade, shade + 8, 245)
            edge_c = (76 + int(lyr * 2.6), 84 + int(lyr * 2.8), 98 + int(lyr * 3.0), 235)
            if is_gcode_viewer and lyr % 4 == 0:
                edge_c = (185, 28, 28, 210)
            draw.polygon(poly, fill=fill_c, outline=edge_c, width=2 * SS)

    # Subtle horizontal slicing plane indicator line with CAD Z-height callout
    top_z = 2.0 + (num_layers - 1) * 5.2
    top_center_pt = iso(0.0, 0.0, top_z)
    plane_y = top_center_pt[1]
    draw.line([(56 * SS, plane_y), (176 * SS, plane_y)], fill=(224, 36, 36, 150), width=1 * SS)
    draw.line([(432 * SS, plane_y), (556 * SS, plane_y)], fill=(224, 36, 36, 150), width=1 * SS)
    font_callout = load_font("consolab.ttf", 11 * SS)
    draw.text((468 * SS, plane_y - 16 * SS), "Z = 24.80 mm", fill=(224, 36, 36, 230), font=font_callout)

    # 3. Clean Top-Right Pill Badge (Zero title duplication!)
    font_badge = load_font("segoeuib.ttf", 12 * SS)
    font_meta = load_font("segoeui.ttf", 12 * SS)
    font_meta_b = load_font("segoeuib.ttf", 12 * SS)

    bbox = draw.textbbox((0, 0), badge_text, font=font_badge)
    tw = bbox[2] - bbox[0]
    pad_x = 16 * SS
    pill_w = tw + pad_x * 2 + 12 * SS
    pill_h = 30 * SS
    pill_x2 = W - 28 * SS
    pill_x1 = pill_x2 - pill_w
    pill_y1 = 26 * SS
    pill_y2 = pill_y1 + pill_h

    draw.rounded_rectangle(
        [pill_x1, pill_y1, pill_x2, pill_y2],
        radius=6 * SS,
        fill=(32, 36, 44, 245),
        outline=(64, 72, 88, 255),
        width=1 * SS,
    )
    # Red status dot inside badge
    dot_cx = pill_x1 + 14 * SS
    dot_cy = (pill_y1 + pill_y2) // 2
    draw.ellipse([dot_cx - 4 * SS, dot_cy - 4 * SS, dot_cx + 4 * SS, dot_cy + 4 * SS], fill=(224, 36, 36, 255))
    draw.text((pill_x1 + 25 * SS, pill_y1 + 6 * SS), badge_text, fill=(240, 244, 248, 255), font=font_badge)

    # 4. Clean Bottom Engineering Metadata Bar (aligned with bottom credits of left column)
    bar_y = 548 * SS
    draw.line([(28 * SS, bar_y), (W - 28 * SS, bar_y)], fill=(48, 54, 66, 230), width=1 * SS)

    left_caption = "Architecture PrusaSlicer 2.9.7  \u2022  Pipeline FFF / SLA"
    right_caption = "OpenSlicer 0.2"
    draw.text((28 * SS, bar_y + 12 * SS), left_caption, fill=(142, 152, 168, 255), font=font_meta)

    rb = draw.textbbox((0, 0), right_caption, font=font_meta_b)
    rw = rb[2] - rb[0]
    rx = W - 28 * SS - rw
    # Small red square accent before OpenSlicer 0.2
    draw.rectangle([rx - 14 * SS, bar_y + 17 * SS, rx - 7 * SS, bar_y + 24 * SS], fill=(224, 36, 36, 255))
    draw.text((rx, bar_y + 12 * SS), right_caption, fill=(235, 239, 245, 255), font=font_meta_b)

    final_img = img.resize((600, 600), Image.Resampling.LANCZOS)
    final_img.save(output_path, "JPEG", quality=96, subsampling=0)
    return final_img


def make_splash():
    generate_right_splash(ICONS_DST / "splashscreen.jpg", "OPEN SOURCE  •  AGPL-3.0", is_gcode_viewer=False)
    generate_right_splash(ICONS_DST / "splashscreen-gcodepreview.jpg", "G-CODE VIEWER", is_gcode_viewer=True)
    print("Generated splashscreen.jpg and splashscreen-gcodepreview.jpg")


# ---------------------------------------------------------------------------
# 3. Build Clean French Localization (PrusaSlicer.mo)
#    Remove operational "PrusaSlicer" UI self-references while preserving:
#    - Fork lineage credits (SplashScreen & AboutDialog)
#    - Documentary links ("Site documentaire PrusaSlicer (Fork)")
#    - 3MF/config compatibility notes referencing upstream PrusaSlicer versions
#    - Filesystem directory paths ("PrusaSlicer/physical_printer")
# ---------------------------------------------------------------------------


def make_french_catalog():
    src_mo = PRUSA_INSTALL / "resources" / "localization" / "fr" / "PrusaSlicer.mo"
    dst_mo = LOC_DST / "PrusaSlicer.mo"

    with open(src_mo, "rb") as f:
        mo = f.read()

    magic, version, nstrings, orig_tab_off, trans_tab_off, hash_sz, hash_off = struct.unpack("<Iiiiiii", mo[:28])

    # Keys where "PrusaSlicer" is documentary, upstream compatibility, or fork lineage (handled explicitly or preserved)
    EXACT_OVERRIDES = {
        # SplashScreen fork lineage credits (cleanly organized on the 400px left panel)
        "is based on Slic3r by Alessandro Ranellucci and the RepRap community.": (
            "est un fork bas\u00e9 sur PrusaSlicer 2.9.7 (Prusa Research) et Slic3r (Alessandro Ranellucci & la communaut\u00e9 RepRap)."
        ),
        "Developed by Prusa Research.": (
            "\u00c9dition OpenSlicer \u2022 Conception CAO modernis\u00e9e."
        ),
        # AboutDialog fork lineage
        "PrusaSlicer is based on Slic3r by Alessandro Ranellucci and the RepRap community.": (
            "OpenSlicer est un fork bas\u00e9 sur PrusaSlicer 2.9.7 (Prusa Research) et Slic3r (Alessandro Ranellucci et la communaut\u00e9 RepRap)."
        ),
        # Help menu About & Documentary links
        "&About %s": "&\u00c0 propos d'%s",
        "About %s": "\u00c0 propos d'%s",
        "%s &Website": "Documentation &Web %s (Fork)",
        "Open the %s website in your browser": "Ouvrir la documentation de r\u00e9f\u00e9rence %s (Fork) dans votre navigateur",
        # File associations & G-code viewer
        "Associate .3mf files to PrusaSlicer": "Associer les fichiers .3mf \u00e0 OpenSlicer",
        "Associate .stl files to PrusaSlicer": "Associer les fichiers .stl \u00e0 OpenSlicer",
        "Associate .bgcode files to PrusaSlicer G-code Viewer": "Associer les fichiers .bgcode \u00e0 l'Aper\u00e7u G-code OpenSlicer",
        "Associate .gcode files to PrusaSlicer G-code Viewer": "Associer les fichiers .gcode \u00e0 l'Aper\u00e7u G-code OpenSlicer",
        "If enabled, sets PrusaSlicer G-code Viewer as default application to open .bgcode files.": (
            "Si activ\u00e9, d\u00e9finit l'Aper\u00e7u G-code OpenSlicer comme application par d\u00e9faut pour ouvrir les fichiers .bgcode."
        ),
        "If enabled, sets PrusaSlicer G-code Viewer as default application to open .gcode files.": (
            "Si activ\u00e9, d\u00e9finit l'Aper\u00e7u G-code OpenSlicer comme application par d\u00e9faut pour ouvrir les fichiers .gcode."
        ),
        # Compatibility & documentary messages where upstream PrusaSlicer parentage makes technical sense
        "Most likely the configuration was produced by a newer version of PrusaSlicer or by some PrusaSlicer fork.": (
            "La configuration a probablement \u00e9t\u00e9 g\u00e9n\u00e9r\u00e9e par une version plus r\u00e9cente de PrusaSlicer ou par un autre fork compatible."
        ),
        "We do not send any personal information nor anything that would allow us to identify you later. To detect duplicate entries, a unique number derived from your system is sent, but the source information cannot be reconstructed. Apart from that, only general data about your OS, hardware and OpenGL installation are sent. PrusaSlicer is open source, if you want to inspect the code actually performing the communication, see %1%.": (
            "Aucune information personnelle permettant de vous identifier n'est transmise. Seules les donn\u00e9es techniques g\u00e9n\u00e9rales (OS, mat\u00e9riel, OpenGL) sont concern\u00e9es. OpenSlicer (bas\u00e9 sur PrusaSlicer open source) permet d'inspecter ce code : voir %1%."
        ),
    }

    # Keys where PrusaSlicer refers to upstream 3MF file format compatibility, physical_printer folder, or min/max preset spec
    DOCUMENTARY_COMPAT_KEYS_SUBSTR = (
        "PrusaSlicer/physical_printer",
        "newer version of PrusaSlicer",
        "min PrusaSlicer version",
        "max PrusaSlicer version",
        "newest PrusaSlicer versions",
    )


    def replace_prusaslicer_french(text: str) -> str:
        s = text
        s = s.replace("prusa-slicer.exe", "OpenPS.exe")
        s = s.replace("PrusaSlicer G-code Viewer", "OpenSlicer G-code Viewer")
        # Clean French elisions before vowel 'O'
        s = s.replace("du m\u00eame PrusaSlicer", "d'OpenSlicer")
        s = s.replace("de la GUI de PrusaSlicer", "de l'interface d'OpenSlicer")
        s = s.replace("la GUI de PrusaSlicer", "l'interface d'OpenSlicer")
        s = s.replace("de PrusaSlicer", "d'OpenSlicer")
        s = s.replace("De PrusaSlicer", "D'OpenSlicer")
        s = s.replace("&PrusaSlicer", "&OpenSlicer")
        s = s.replace("PrusaSlicer", "OpenSlicer")
        return s


    entries = []
    replaced_loc_count = 0

    for i in range(nstrings):
        ol, oo = struct.unpack("<ii", mo[orig_tab_off + i * 8 : orig_tab_off + (i + 1) * 8])
        tl, to = struct.unpack("<ii", mo[trans_tab_off + i * 8 : trans_tab_off + (i + 1) * 8])
        k = mo[oo : oo + ol].decode("utf-8")
        v = mo[to : to + tl].decode("utf-8")

        if k in EXACT_OVERRIDES:
            v = EXACT_OVERRIDES[k]
            replaced_loc_count += 1
        elif any(sub in k for sub in DOCUMENTARY_COMPAT_KEYS_SUBSTR):
            # Keep documentary / format compatibility reference intact
            pass
        elif "PrusaSlicer" in v or "prusa-slicer" in v:
            new_v = replace_prusaslicer_french(v)
            if new_v != v:
                v = new_v
                replaced_loc_count += 1

        entries.append((k, v))

    # Add explicit translations for our new C++ source strings as well
    extra_translations = {
        "Set origin": "D\u00e9finir l'origine",
        "Center of active object": "Au centre de l'objet actif",
        "Center of build plate": "Au centre du plateau",
    }
    existing_keys = {k for k, _ in entries}
    for ek, ev in extra_translations.items():
        if ek not in existing_keys:
            entries.append((ek, ev))

    entries.sort(key=lambda item: item[0].encode("utf-8"))

    offsets = []
    ids = bytearray()
    strs = bytearray()

    for k, v in entries:
        k_b = k.encode("utf-8")
        v_b = v.encode("utf-8")
        offsets.append((len(ids), len(k_b), len(strs), len(v_b)))
        ids.extend(k_b + b"\x00")
        strs.extend(v_b + b"\x00")

    key_start = 7 * 4 + len(entries) * 8 * 2
    val_start = key_start + len(ids)

    k_table = []
    v_table = []
    for o1, l1, o2, l2 in offsets:
        k_table.extend([l1, key_start + o1])
        v_table.extend([l2, val_start + o2])

    header = struct.pack(
        "<Iiiiiii",
        0x950412DE,
        0,
        len(entries),
        7 * 4,
        7 * 4 + len(entries) * 8,
        0,
        0,
    )

    with open(dst_mo, "wb") as f:
        f.write(header)
        f.write(struct.pack(f"<{len(k_table)}i", *k_table))
        f.write(struct.pack(f"<{len(v_table)}i", *v_table))
        f.write(ids)
        f.write(strs)

    print(f"Localization built: {len(entries)} total entries, {replaced_loc_count} customized entries -> {dst_mo}")


def main():
    global PRUSA_INSTALL
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--prusaslicer", type=Path, default=PRUSA_INSTALL, help="PrusaSlicer 2.9.7 install folder")
    parser.add_argument("--icons", action="store_true")
    parser.add_argument("--splash", action="store_true")
    parser.add_argument("--localization", action="store_true")
    args = parser.parse_args()
    PRUSA_INSTALL = args.prusaslicer
    run_all = not (args.icons or args.splash or args.localization)

    ICONS_DST.mkdir(parents=True, exist_ok=True)
    LOC_DST.mkdir(parents=True, exist_ok=True)
    if run_all or args.icons:
        sync_icons()
    if run_all or args.splash:
        make_splash()
    if run_all or args.localization:
        make_french_catalog()


if __name__ == "__main__":
    main()
