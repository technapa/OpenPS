// SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// SyncIndxTab.hpp - The "Sync" tab of OpenSlicer
//
// Adds a "Sync" button to PrusaSlicer's top bar. When selected, the PrusaSlicer page
// is hidden and a native Win32 window shows two panels styled like the "Tools" tab
// of Prusa Connect:
//   1. Filaments installed in the printer (read from Prusa Connect, Assign / Unload)
//   2. Filaments active in the slicer (Change profile / Color)
// and two buttons that synchronize them in either direction.
//
// The slicer side is read and written through the PrusaSlicer sidebar controls
// (filament combo boxes); network work is done by SyncEngine (SyncIndxEngine.hpp).
// All buttons are real text BUTTON controls sized from their translated label.
// ============================================================================
#pragma once
#ifndef OPENPS_SYNC_INDX_TAB_HPP
#define OPENPS_SYNC_INDX_TAB_HPP

#include "PrusaSlicerDll.hpp"
#include "SyncIndxEngine.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <cwctype>
#include <memory>

#pragma comment(lib, "comdlg32.lib")

namespace openps::sync {

inline constexpr wchar_t kAppName[] = L"OpenSlicer";

// Subclass IDs
inline constexpr UINT_PTR SUBCLASS_ID_SYNC_NATIVE_TABBTN = 410;
inline constexpr UINT_PTR SUBCLASS_ID_SYNC_UI_BTN        = 412;

// Popup menu command ranges
inline constexpr UINT IDM_SLICER_MATERIAL_BASE = 61400; // +0..10 (11 Buddy materials)
inline constexpr UINT IDM_PRINTER_SELECT_BASE  = 61800; // +0..31 Cloud printer list

// Workspace child control IDs
inline constexpr int IDC_SYNC_REFRESH     = 3100;
inline constexpr int IDC_SYNC_PRINTER     = 3101;
inline constexpr int IDC_SYNC_TO_SLICER   = 3102;
inline constexpr int IDC_SYNC_TO_MACHINE  = 3103;
inline constexpr int IDC_SYNC_MACHINE_A   = 3200; // +0..7  Change / Assign
inline constexpr int IDC_SYNC_MACHINE_B   = 3220; // +0..7  Unload
inline constexpr int IDC_SYNC_SLICER_A    = 3240; // +0..7  Change
inline constexpr int IDC_SYNC_SLICER_B    = 3260; // +0..7  Color

// ChooseColorW is hooked in PrusaSlicer.dll (see OpenPS_launcher.cpp) so that the extruder color of a
// filament combo box can be set without showing the color dialog (g_autoChooseColorActive).
typedef BOOL (APIENTRY *ChooseColorW_t)(LPCHOOSECOLORW);
inline ChooseColorW_t OriginalChooseColorW    = nullptr;
inline bool           g_autoChooseColorActive = false;
inline COLORREF       g_autoChooseColorValue  = RGB(234, 103, 12);
inline COLORREF       g_customColors[16]      = {
    RGB(0x1E,0x1E,0x1E), RGB(0xFF,0xFF,0xFF), RGB(0x84,0x84,0x84), RGB(0xB8,0xBC,0xCA),
    RGB(0xE0,0x24,0x24), RGB(0xEA,0x67,0x0C), RGB(0xF5,0xD0,0x00), RGB(0x19,0x80,0x38),
    RGB(0x0F,0x62,0xFE), RGB(0x7C,0x3A,0xED), RGB(0xEC,0x48,0x99), RGB(0xA5,0x29,0x29),
    RGB(0xC8,0x64,0x46), RGB(0xD4,0xAF,0x37), RGB(0xE2,0xE8,0xF0), RGB(0x36,0x42,0x52)
};

inline BOOL APIENTRY Hook_ChooseColorW(LPCHOOSECOLORW lpcc) {
    if (g_autoChooseColorActive && lpcc) {
        lpcc->rgbResult = g_autoChooseColorValue;
        return TRUE;
    }
    if (!OriginalChooseColorW) {
        HMODULE hComDlg = LoadLibraryW(L"COMDLG32.dll");
        if (hComDlg) {
            OriginalChooseColorW = (ChooseColorW_t)GetProcAddress(hComDlg, "ChooseColorW");
        }
    }
    return OriginalChooseColorW ? OriginalChooseColorW(lpcc) : FALSE;
}

// ----------------------------------------------------------------------------
// Prusa Connect design tokens (connect.prusa3d.com stylesheet, light & dark themes)
//   --color-primary #fa6831 · --color-text #212529 · --color-text-secondary #797979
//   --border-primary #000 · --border-secondary #e5e5e5 · --background-hover #f1f1f1
//   Buttons: 14px bold, 1px solid border-primary, radius 4px, hover #343a40 / #fff
// ----------------------------------------------------------------------------
struct UiPalette {
    bool           dark = false;
    Gdiplus::Color bg;
    Gdiplus::Color text;
    Gdiplus::Color textSecondary;
    Gdiplus::Color borderPrimary;
    Gdiplus::Color borderSecondary;
    Gdiplus::Color cardBorder;
    Gdiplus::Color hover;
    Gdiplus::Color primary;        // Prusa orange
    Gdiplus::Color primaryFaded;
    Gdiplus::Color btnHoverBg;
    Gdiplus::Color btnHoverText;
    Gdiplus::Color brand;          // OpenSlicer red
    Gdiplus::Color brandHover;
    Gdiplus::Color solid;
    Gdiplus::Color solidText;
    Gdiplus::Color danger;
    Gdiplus::Color stateOk;
    Gdiplus::Color stateBusy;
    Gdiplus::Color stateOff;

    static UiPalette ForTheme(bool isDark) {
        UiPalette p{};
        p.dark = isDark;
        p.primary      = Gdiplus::Color(255, 0xFA, 0x68, 0x31);
        p.btnHoverBg   = Gdiplus::Color(255, 0x34, 0x3A, 0x40);
        p.btnHoverText = Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
        p.brand        = Gdiplus::Color(255, 0xE0, 0x24, 0x24);
        p.brandHover   = Gdiplus::Color(255, 0xC5, 0x1B, 0x1B);
        p.stateOk      = Gdiplus::Color(255, 0x5C, 0xB8, 0x4C);
        p.stateBusy    = Gdiplus::Color(255, 0xF5, 0x9C, 0x66);
        p.stateOff     = Gdiplus::Color(255, 0xAD, 0xAD, 0xAD);
        if (!isDark) {
            p.bg              = Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
            p.text            = Gdiplus::Color(255, 0x21, 0x25, 0x29);
            p.textSecondary   = Gdiplus::Color(255, 0x79, 0x79, 0x79);
            p.borderPrimary   = Gdiplus::Color(255, 0x00, 0x00, 0x00);
            p.borderSecondary = Gdiplus::Color(255, 0xE5, 0xE5, 0xE5);
            p.cardBorder      = Gdiplus::Color(255, 0xD9, 0xD9, 0xD9);
            p.hover           = Gdiplus::Color(255, 0xF1, 0xF1, 0xF1);
            p.primaryFaded    = Gdiplus::Color(255, 0xFC, 0xD6, 0xC0);
            p.solid           = Gdiplus::Color(255, 0x00, 0x00, 0x00);
            p.solidText       = Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
            p.danger          = Gdiplus::Color(255, 0xDC, 0x35, 0x45);
        } else {
            p.bg              = Gdiplus::Color(255, 0x1C, 0x1E, 0x21);
            p.text            = Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
            p.textSecondary   = Gdiplus::Color(255, 0xA2, 0xA2, 0xA2);
            p.borderPrimary   = Gdiplus::Color(255, 0x8F, 0x8F, 0x8F);
            p.borderSecondary = Gdiplus::Color(255, 0x4E, 0x4E, 0x4E);
            p.cardBorder      = Gdiplus::Color(255, 0x4E, 0x4E, 0x4E);
            p.hover           = Gdiplus::Color(255, 0x2A, 0x2A, 0x2A);
            p.primaryFaded    = Gdiplus::Color(255, 0x9C, 0x40, 0x1E);
            p.solid           = Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
            p.solidText       = Gdiplus::Color(255, 0x21, 0x25, 0x29);
            p.danger          = Gdiplus::Color(255, 0xF2, 0x6B, 0x78);
        }
        return p;
    }
};

inline COLORREF ToColorRef(const Gdiplus::Color& c) {
    return RGB(c.GetR(), c.GetG(), c.GetB());
}

inline Gdiplus::Color BlendColor(const Gdiplus::Color& from, const Gdiplus::Color& to, float t) {
    auto mix = [&](BYTE a, BYTE b) { return (BYTE)std::clamp((int)std::lround(a + (b - a) * t), 0, 255); };
    return Gdiplus::Color(255, mix(from.GetR(), to.GetR()), mix(from.GetG(), to.GetG()), mix(from.GetB(), to.GetB()));
}

// Same contrast rule as Prusa Connect (relative luminance > 0.5 -> black text)
inline bool UseDarkTextOn(const Rgb& c) {
    auto lin = [](double v) {
        v /= 255.0;
        return (v <= 0.03928) ? (v / 12.92) : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return (0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b)) > 0.5;
}

inline void BuildRoundedRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rc, float radius) {
    path.Reset();
    if (radius <= 0.5f || rc.Width <= 2.0f || rc.Height <= 2.0f) {
        path.AddRectangle(rc);
        return;
    }
    float d = std::min({ radius * 2.0f, rc.Width, rc.Height });
    path.AddArc(rc.X, rc.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rc.GetRight() - d, rc.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rc.GetRight() - d, rc.GetBottom() - d, d, d, 0.0f, 90.0f);
    path.AddArc(rc.X, rc.GetBottom() - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

inline void FillStrokeRoundRect(Gdiplus::Graphics& g, const Gdiplus::RectF& rc, float radius,
                                const Gdiplus::Color& fill, const Gdiplus::Color& stroke, float strokeW = 1.0f)
{
    Gdiplus::GraphicsPath path;
    BuildRoundedRectPath(path, rc, radius);
    if (fill.GetAlpha() > 0) {
        Gdiplus::SolidBrush br(fill);
        g.FillPath(&br, &path);
    }
    if (stroke.GetAlpha() > 0 && strokeW > 0.0f) {
        Gdiplus::Pen pen(stroke, strokeW);
        g.DrawPath(&pen, &path);
    }
}

inline int GetWindowDpiSafe(HWND hWnd) {
    typedef UINT (WINAPI *GetDpiForWindow_t)(HWND);
    static GetDpiForWindow_t fn = (GetDpiForWindow_t)GetProcAddress(GetModuleHandleW(L"USER32.dll"), "GetDpiForWindow");
    if (fn && hWnd) {
        UINT d = fn(hWnd);
        if (d >= 72 && d <= 480) return (int)d;
    }
    HDC hdc = GetDC(hWnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hWnd, hdc);
    return (dpi > 0) ? dpi : 96;
}

inline bool DetectHostDarkTheme(HWND hRefWnd) {
    if (!hRefWnd || !IsWindow(hRefWnd)) return false;
    HDC hdc = GetDC(hRefWnd);
    if (!hdc) return false;
    COLORREF px = GetPixel(hdc, 2, 2);
    ReleaseDC(hRefWnd, hdc);
    if (px == CLR_INVALID) return false;
    int lum = (299 * GetRValue(px) + 587 * GetGValue(px) + 114 * GetBValue(px)) / 1000;
    return (lum < 110);
}

// ----------------------------------------------------------------------------
// Typography: Prusa Connect uses "Atlas Grotesk Web" with a Segoe UI fallback on Windows.
// Atlas Grotesk is used when installed on the machine, Segoe UI otherwise.
// ----------------------------------------------------------------------------
inline const std::wstring& UiFontFamily() {
    static const std::wstring family = []() -> std::wstring {
        for (const wchar_t* name : { L"Atlas Grotesk Web", L"Atlas Grotesk" }) {
            Gdiplus::FontFamily ff(name);
            if (ff.IsAvailable()) return name;
        }
        return L"Segoe UI";
    }();
    return family;
}

// weight: 400 regular, 500 medium, 700 bold
inline std::unique_ptr<Gdiplus::Font> MakeUiFont(float px, int weight) {
    const std::wstring& fam = UiFontFamily();
    if (fam == L"Segoe UI" && weight == 500) {
        return std::make_unique<Gdiplus::Font>(L"Segoe UI Semibold", px, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    }
    return std::make_unique<Gdiplus::Font>(fam.c_str(), px,
                                           weight >= 700 ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,
                                           Gdiplus::UnitPixel);
}

// Window DC + GDI+ graphics for text measurement (released in the right order)
struct MeasureContext {
    HWND                               hWnd;
    HDC                                hdc;
    std::unique_ptr<Gdiplus::Graphics> g;

    explicit MeasureContext(HWND h) : hWnd(h), hdc(GetDC(h)) {
        g = std::make_unique<Gdiplus::Graphics>(hdc);
        g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    }
    ~MeasureContext() {
        g.reset();
        if (hdc) ReleaseDC(hWnd, hdc);
    }
    MeasureContext(const MeasureContext&) = delete;
    MeasureContext& operator=(const MeasureContext&) = delete;
};

inline float MeasureTextWidth(Gdiplus::Graphics& g, const Gdiplus::Font& font, const std::wstring& text) {
    if (text.empty()) return 0.0f;
    // Default (non typographic) format: the result includes the side bearings DrawString adds
    Gdiplus::StringFormat sf;
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    Gdiplus::RectF box;
    g.MeasureString(text.c_str(), (INT)text.size(), &font, Gdiplus::PointF(0.0f, 0.0f), &sf, &box);
    return std::ceil(box.Width) + 1.0f;
}

// ----------------------------------------------------------------------------
// Text buttons: real BUTTON controls (owner-draw) rendered like Prusa Connect buttons.
// The label is the window text of the control, never a bitmap.
// ----------------------------------------------------------------------------
enum class BtnKind : int { Outline = 1, Brand = 2, Solid = 3, Swatch = 4 };
enum class BtnIcon : int { None = 0, ArrowDown = 1, ArrowUp = 2, Chevron = 3, Refresh = 4 };

inline constexpr wchar_t kPropBtnKind[]  = L"OpenPS.Btn.Kind";
inline constexpr wchar_t kPropBtnIcon[]  = L"OpenPS.Btn.Icon";
inline constexpr wchar_t kPropBtnHover[] = L"OpenPS.Btn.Hover";
inline constexpr wchar_t kPropBtnColor[] = L"OpenPS.Btn.Color";

inline LRESULT CALLBACK ConnectBtnSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg) {
    case WM_MOUSEMOVE:
        if (!GetPropW(hWnd, kPropBtnHover)) {
            SetPropW(hWnd, kPropBtnHover, (HANDLE)(INT_PTR)1);
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropW(hWnd, kPropBtnHover);
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDBLCLK:
        // Owner-draw buttons turn fast clicks into BN_DBLCLK: treat them as regular clicks
        return DefSubclassProc(hWnd, WM_LBUTTONDOWN, wParam, lParam);
    case WM_SETCURSOR:
        if (IsWindowEnabled(hWnd)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_ENABLE:
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    case WM_NCDESTROY:
        RemovePropW(hWnd, kPropBtnKind);
        RemovePropW(hWnd, kPropBtnIcon);
        RemovePropW(hWnd, kPropBtnHover);
        RemovePropW(hWnd, kPropBtnColor);
        RemoveWindowSubclass(hWnd, ConnectBtnSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

inline HWND CreateConnectButton(HWND hParent, int id, const std::wstring& text, BtnKind kind,
                                BtnIcon icon = BtnIcon::None, bool visible = false)
{
    HWND h = CreateWindowExW(0, L"BUTTON", text.c_str(),
                             WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW | (visible ? WS_VISIBLE : 0),
                             0, 0, 10, 10, hParent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (h) {
        SetPropW(h, kPropBtnKind, (HANDLE)(INT_PTR)(int)kind);
        SetPropW(h, kPropBtnIcon, (HANDLE)(INT_PTR)(int)icon);
        SetWindowSubclass(h, ConnectBtnSubclassProc, SUBCLASS_ID_SYNC_UI_BTN, 0);
    }
    return h;
}

inline void SetButtonSwatchColor(HWND hBtn, COLORREF c) {
    // Stored +1 so that black (0) is distinguishable from "no color"
    SetPropW(hBtn, kPropBtnColor, (HANDLE)(INT_PTR)((INT_PTR)c + 1));
    InvalidateRect(hBtn, nullptr, FALSE);
}

inline bool IsLargeButton(BtnKind kind) {
    return kind == BtnKind::Brand || kind == BtnKind::Solid;
}

// Width needed by a button for its (translated) label
inline int MeasureConnectButtonWidth(HWND hRef, const std::wstring& text, BtnKind kind, BtnIcon icon) {
    int dpi = GetWindowDpiSafe(hRef);
    auto S = [&](float v) { return v * (float)dpi / 96.0f; };
    bool large = IsLargeButton(kind);
    float tw = 0.0f;
    {
        MeasureContext mc(hRef);
        auto font = MakeUiFont(S(large ? 15.0f : 14.0f), 700);
        tw = MeasureTextWidth(*mc.g, *font, text);
    }
    float iconW = (icon != BtnIcon::None) ? S(large ? 18.0f : 12.0f) : 0.0f;
    float gap   = (iconW > 0.0f && !text.empty()) ? S(8.0f) : 0.0f;
    float padX  = S(large ? 18.0f : 12.0f);
    return (int)std::ceil(tw + iconW + gap + 2.0f * padX) + 2;
}

inline void DrawButtonIcon(Gdiplus::Graphics& g, BtnIcon icon, const Gdiplus::RectF& box, const Gdiplus::Color& color, float stroke) {
    Gdiplus::Pen pen(color, stroke);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    float cx = box.X + box.Width * 0.5f;
    float cy = box.Y + box.Height * 0.5f;
    float hw = box.Width * 0.5f;
    float hh = box.Height * 0.5f;
    switch (icon) {
    case BtnIcon::ArrowDown:
    case BtnIcon::ArrowUp: {
        float dir  = (icon == BtnIcon::ArrowDown) ? 1.0f : -1.0f;
        float tipY = cy + dir * hh * 0.82f;
        float endY = cy - dir * hh * 0.82f;
        float head = hw * 0.62f;
        g.DrawLine(&pen, cx, endY, cx, tipY);
        g.DrawLine(&pen, cx - head, tipY - dir * head, cx, tipY);
        g.DrawLine(&pen, cx + head, tipY - dir * head, cx, tipY);
        break;
    }
    case BtnIcon::Chevron: {
        float a = hw * 0.62f;
        g.DrawLine(&pen, cx - a, cy - a * 0.45f, cx, cy + a * 0.55f);
        g.DrawLine(&pen, cx + a, cy - a * 0.45f, cx, cy + a * 0.55f);
        break;
    }
    case BtnIcon::Refresh: {
        float r = std::min(hw, hh) * 0.82f;
        g.DrawArc(&pen, cx - r, cy - r, 2.0f * r, 2.0f * r, -50.0f, 290.0f);
        // Arrow head at the start of the arc (angle -50 degrees)
        float ax = cx + r * 0.6428f;
        float ay = cy - r * 0.7660f;
        float h  = r * 0.62f;
        g.DrawLine(&pen, ax, ay, ax + h * 0.15f, ay - h);
        g.DrawLine(&pen, ax, ay, ax - h, ay - h * 0.10f);
        break;
    }
    default:
        break;
    }
}

inline void DrawConnectButton(const DRAWITEMSTRUCT* dis, const UiPalette& pal) {
    if (!dis || dis->CtlType != ODT_BUTTON) return;
    HWND hBtn = dis->hwndItem;
    int w = dis->rcItem.right - dis->rcItem.left;
    int h = dis->rcItem.bottom - dis->rcItem.top;
    if (w <= 2 || h <= 2) return;

    int dpi = GetWindowDpiSafe(hBtn);
    auto S = [&](float v) { return v * (float)dpi / 96.0f; };

    BtnKind kind = (BtnKind)(int)(INT_PTR)GetPropW(hBtn, kPropBtnKind);
    BtnIcon icon = (BtnIcon)(int)(INT_PTR)GetPropW(hBtn, kPropBtnIcon);
    bool hover    = GetPropW(hBtn, kPropBtnHover) != nullptr;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed  = (dis->itemState & ODS_SELECTED) != 0;
    bool kbFocus  = (dis->itemState & ODS_FOCUS) != 0 && (dis->itemState & ODS_NOFOCUSRECT) == 0;
    bool active   = (hover || pressed) && !disabled;
    bool large    = IsLargeButton(kind);

    wchar_t textBuf[256]{};
    int textLen = GetWindowTextW(hBtn, textBuf, 255);
    std::wstring text(textBuf, (size_t)std::max(0, textLen));

    Gdiplus::Color fill = pal.bg, border = pal.borderPrimary, fg = pal.text;
    switch (kind) {
    case BtnKind::Brand:
        fill = border = active ? pal.brandHover : pal.brand;
        fg   = Gdiplus::Color(255, 255, 255, 255);
        break;
    case BtnKind::Solid:
        fill = border = active ? pal.btnHoverBg : pal.solid;
        fg   = active ? pal.btnHoverText : pal.solidText;
        break;
    case BtnKind::Swatch: {
        INT_PTR stored = (INT_PTR)GetPropW(hBtn, kPropBtnColor);
        COLORREF c = stored > 0 ? (COLORREF)(stored - 1) : RGB(216, 222, 230);
        fill   = Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
        border = active ? pal.primary : pal.borderPrimary;
        break;
    }
    case BtnKind::Outline:
    default:
        if (active) {
            fill = border = pal.btnHoverBg;
            fg   = pal.btnHoverText;
        }
        break;
    }
    if (disabled) {
        // Connect: opacity 0.65 on disabled buttons
        fill   = BlendColor(pal.bg, fill, 0.45f);
        border = BlendColor(pal.bg, border, 0.30f);
        fg     = BlendColor(pal.bg, fg, 0.40f);
    }

    HDC hdcMem = CreateCompatibleDC(dis->hDC);
    HBITMAP hBmp = CreateCompatibleBitmap(dis->hDC, w, h);
    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    {
        Gdiplus::Graphics g(hdcMem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        g.Clear(pal.bg);

        Gdiplus::RectF rc(0.5f, 0.5f, (float)w - 1.0f, (float)h - 1.0f);
        FillStrokeRoundRect(g, rc, S(4.0f), fill, border, 1.0f);
        if (kbFocus) {
            Gdiplus::RectF inner(2.5f, 2.5f, (float)w - 5.0f, (float)h - 5.0f);
            FillStrokeRoundRect(g, inner, S(2.5f), Gdiplus::Color(0, 0, 0, 0),
                                Gdiplus::Color(128, fg.GetR(), fg.GetG(), fg.GetB()), 1.0f);
        }

        if (kind != BtnKind::Swatch) {
            float iconW = (icon != BtnIcon::None) ? S(large ? 18.0f : 12.0f) : 0.0f;
            float gap   = (iconW > 0.0f && !text.empty()) ? S(8.0f) : 0.0f;
            float padX  = std::min(S(large ? 18.0f : 12.0f), std::max(S(4.0f), ((float)w - iconW) * 0.12f));
            float avail = std::max(4.0f, (float)w - 2.0f * padX - iconW - gap);

            // Labels shrink before they are ellipsized so every language stays readable
            float fontPx = S(large ? 15.0f : 14.0f);
            float minPx  = S(10.5f);
            auto font = MakeUiFont(fontPx, 700);
            float tw = MeasureTextWidth(g, *font, text);
            while (tw > avail && fontPx > minPx) {
                fontPx -= S(0.5f);
                font = MakeUiFont(fontPx, 700);
                tw = MeasureTextWidth(g, *font, text);
            }
            float drawW = std::min(tw, avail);
            float x = ((float)w - (iconW + gap + drawW)) * 0.5f;

            if (iconW > 0.0f) {
                Gdiplus::RectF iconBox(x, ((float)h - iconW) * 0.5f, iconW, iconW);
                DrawButtonIcon(g, icon, iconBox, fg, S(large ? 2.0f : 1.6f));
            }
            if (!text.empty()) {
                Gdiplus::StringFormat sf;
                sf.SetAlignment(Gdiplus::StringAlignmentNear);
                sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
                Gdiplus::SolidBrush br(fg);
                Gdiplus::RectF rcText(x + iconW + gap, 0.0f, drawW + 1.0f, (float)h);
                g.DrawString(text.c_str(), (INT)text.size(), font.get(), rcText, &sf, &br);
            }
        }
    }
    BitBlt(dis->hDC, dis->rcItem.left, dis->rcItem.top, w, h, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
}

inline void SetButtonState(HWND hBtn, const std::wstring& text, bool enabled) {
    if (!hBtn) return;
    wchar_t cur[256]{};
    GetWindowTextW(hBtn, cur, 255);
    if (text != cur) {
        SetWindowTextW(hBtn, text.c_str());
        InvalidateRect(hBtn, nullptr, FALSE);
    }
    if ((IsWindowEnabled(hBtn) != FALSE) != enabled) {
        EnableWindow(hBtn, enabled ? TRUE : FALSE);
    }
}

// ----------------------------------------------------------------------------
// Host Bridge: Discovers Sidebar Filament & Printer ComboBoxes in PrusaSlicer
// ----------------------------------------------------------------------------
struct HostSidebarControls {
    HWND              hPresetsPanel = nullptr;
    HWND              hPrintCombo   = nullptr;
    std::vector<HWND> hFilaments;   // 1..8 filament PlaterPresetComboBox handles
    HWND              hPrinterCombo = nullptr;
};

inline HostSidebarControls FindHostSidebarCombos(HWND hMainWnd) {
    HostSidebarControls out;
    if (!hMainWnd || !IsWindow(hMainWnd)) return out;

    struct ParentGroup {
        HWND parent = nullptr;
        std::vector<std::pair<int, HWND>> combosByTop;
    };
    std::vector<ParentGroup> groups;

    struct EnumCtx { std::vector<ParentGroup>* groups; } ctx{ &groups };
    // In PrusaSlicer 2.9.7, Slic3r::GUI::ComboBox inherits from TextInput (wxWindowNR)
    // with WS_VISIBLE set on the ComboBox wxWindowNR and a hidden internal Edit child (wxCB_READONLY).
    EnumChildWindows(hMainWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
        wchar_t cls[64]{};
        GetClassNameW(hChild, cls, 63);
        if (_wcsicmp(cls, L"Edit") != 0) return TRUE;

        // Read-only Slic3r::GUI::ComboBox hides its internal wxTextCtrl Edit child
        if (GetWindowLongW(hChild, GWL_STYLE) & WS_VISIBLE) return TRUE;

        HWND hCombo = GetParent(hChild);
        if (!hCombo) return TRUE;
        wchar_t cbCls[64]{};
        GetClassNameW(hCombo, cbCls, 63);
        if (_wcsicmp(cbCls, L"wxWindowNR") != 0) return TRUE;
        if (!(GetWindowLongW(hCombo, GWL_STYLE) & WS_VISIBLE)) return TRUE;

        HWND hPresetsPanel = GetParent(hCombo);
        if (!hPresetsPanel) return TRUE;
        wchar_t pCls[64]{};
        GetClassNameW(hPresetsPanel, pCls, 63);
        if (_wcsicmp(pCls, L"wxWindowNR") != 0) return TRUE;

        RECT rc{};
        GetWindowRect(hCombo, &rc);
        if ((rc.right - rc.left) < 120 || (rc.bottom - rc.top) > 48) return TRUE;

        auto* gVec = reinterpret_cast<EnumCtx*>(lParam)->groups;
        for (auto& g : *gVec) {
            if (g.parent == hPresetsPanel) {
                for (const auto& ex : g.combosByTop) {
                    if (ex.second == hCombo) return TRUE;
                }
                g.combosByTop.push_back({ (int)rc.top, hCombo });
                return TRUE;
            }
        }
        ParentGroup ng;
        ng.parent = hPresetsPanel;
        ng.combosByTop.push_back({ (int)rc.top, hCombo });
        gVec->push_back(std::move(ng));
        return TRUE;
    }, (LPARAM)&ctx);

    // Sidebar::m_presets_panel is the unique wxWindowNR owning >= 3 visible Slic3r::GUI::ComboBox controls
    // (Print settings at top, 1..8 Filament combos in the middle, Printer combo at bottom)
    ParentGroup* best = nullptr;
    for (auto& g : groups) {
        if (g.combosByTop.size() >= 3) {
            if (!best || g.combosByTop.size() > best->combosByTop.size()) {
                best = &g;
            }
        }
    }
    if (!best) return out;

    std::sort(best->combosByTop.begin(), best->combosByTop.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    out.hPresetsPanel = best->parent;
    out.hPrintCombo   = best->combosByTop.front().second;
    out.hPrinterCombo = best->combosByTop.back().second;
    for (size_t i = 1; i + 1 < best->combosByTop.size() && out.hFilaments.size() < (size_t)kToolCount; ++i) {
        out.hFilaments.push_back(best->combosByTop[i].second);
    }
    return out;
}

inline std::wstring GetComboSelectedText(HWND hCombo) {
    if (!hCombo || !IsWindow(hCombo)) return L"";
    wchar_t buf[512]{};
    int len = GetWindowTextW(hCombo, buf, 511);
    if (len > 0) {
        return std::wstring(buf, (size_t)len);
    }
    return L"";
}

// Samples the rendered extruder_rgb bar of PlaterPresetComboBox via PrintWindow(PW_RENDERFULLCONTENT)
inline bool SampleComboExtruderColor(HWND hCombo, Rgb& outRgb) {
    if (!hCombo || !IsWindow(hCombo)) return false;
    RECT rc{};
    GetClientRect(hCombo, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 60 || h < 16) return false;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp  = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);
    if (!hBmp || !bits) {
        if (hBmp) DeleteObject(hBmp);
        if (hdcMem) DeleteDC(hdcMem);
        return false;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    // Slic3r::GUI::TextInput draws exclusively inside wxEVT_PAINT; PW_RENDERFULLCONTENT (2) triggers WM_PAINT cleanly
    BOOL pwOk = PrintWindow(hCombo, hdcMem, 2);
    if (!pwOk && IsWindowVisible(hCombo)) {
        HDC hdcWin = GetDC(hCombo);
        if (hdcWin) {
            BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);
            ReleaseDC(hCombo, hdcWin);
        }
    }

    const uint32_t* px = static_cast<const uint32_t*>(bits);
    auto getRgb = [&](int x, int y) -> Rgb {
        uint32_t v = px[y * w + x];
        return Rgb((uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 8) & 0xFF), (uint8_t)(v & 0xFF), true);
    };

    int dpi = GetWindowDpiSafe(hCombo);
    int midY = h / 2;
    int bgSampleX = std::clamp(w - MulDiv(32, dpi, 96), 40, w - 4);
    Rgb bgClr = getRgb(bgSampleX, 3);

    int xMin  = std::max(2, MulDiv(4, dpi, 96));
    int vSpan = std::max(3, MulDiv(4, dpi, 96));

    auto isVerticallySolidAt = [&](int x) -> bool {
        if (midY - vSpan < 1 || midY + vSpan >= h - 1) return false;
        Rgb base = getRgb(x, midY);
        for (int dy = -vSpan; dy <= vSpan; ++dy) {
            Rgb c = getRgb(x, midY + dy);
            if (std::abs((int)c.r - (int)base.r) > 2 ||
                std::abs((int)c.g - (int)base.g) > 2 ||
                std::abs((int)c.b - (int)base.b) > 2) {
                return false;
            }
        }
        return true;
    };

    // BitmapCache::mksolid draws a 1px dark border around every swatch bar (making white #FFFFFF spools detectable!)
    auto hasSwatchTopBottomBorder = [&](int x) -> bool {
        bool topB = false, botB = false;
        for (int y = 2; y <= std::max(2, midY - vSpan); ++y) {
            Rgb c = getRgb(x, y);
            if (std::abs((int)c.r - (int)bgClr.r) + std::abs((int)c.g - (int)bgClr.g) + std::abs((int)c.b - (int)bgClr.b) > 24) {
                topB = true;
                break;
            }
        }
        for (int y = std::min(h - 3, midY + vSpan); y <= h - 3; ++y) {
            Rgb c = getRgb(x, y);
            if (std::abs((int)c.r - (int)bgClr.r) + std::abs((int)c.g - (int)bgClr.g) + std::abs((int)c.b - (int)bgClr.b) > 24) {
                botB = true;
                break;
            }
        }
        return topB && botB;
    };

    auto sameRgb = [](const Rgb& a, const Rgb& b) -> bool {
        return std::abs((int)a.r - (int)b.r) <= 2 &&
               std::abs((int)a.g - (int)b.g) <= 2 &&
               std::abs((int)a.b - (int)b.b) <= 2;
    };

    // In PlaterPresetComboBox (PresetComboBoxes.cpp:553-577):
    //   [padding] + [1px #6E6E6E] + [filament_rgb interior: 14px @96 / 22px @144]
    //             + [2px #6E6E6E divider] + [extruder_rgb interior: 6px @96 / 10px @144] + [1px #6E6E6E]
    //             + [2px space] + [16px #6E6E6E lock_closed]
    // Find xFil = exact first interior column of filament_rgb (both vertically AND horizontally uniform
    // with top/bottom mksolid border), then sample extruder_rgb interior at xFil + 19px @96 DPI!
    int xSearchMax = std::min(w - 16, MulDiv(28, dpi, 96));
    int minFilSpan = std::max(5, MulDiv(8, dpi, 96));
    int xFil       = -1;

    for (int x = xMin; x <= xSearchMax; ++x) {
        if (!isVerticallySolidAt(x) || !hasSwatchTopBottomBorder(x)) continue;
        Rgb baseFil = getRgb(x, midY);
        if (baseFil.r == 0 && baseFil.g == 0 && baseFil.b == 0 && bgClr.r == 0 && bgClr.g == 0 && bgClr.b == 0) {
            break;
        }
        bool spanOk = true;
        for (int dx = 1; dx < minFilSpan; ++dx) {
            if (x + dx >= w - 4 || !isVerticallySolidAt(x + dx) || !sameRgb(getRgb(x + dx, midY), baseFil)) {
                spanOk = false;
                break;
            }
        }
        if (spanOk) {
            xFil = x;
            break;
        }
    }

    bool found = false;
    if (xFil >= 0) {
        // extruder_rgb interior center is at xFil + 19px @96 DPI (e.g. 11 + 28 = 39 @144 DPI, inside [35..44])
        // Require >= 4 horizontally identical pixels so the 1-2px #6E6E6E mksolid border can NEVER match!
        int xExtCenter = std::min(w - 6, xFil + MulDiv(19, dpi, 96));
        static const int kOffsets[] = { 0, 1, -1, 2, -2, 3, -3, 4 };
        for (int off : kOffsets) {
            int sx = xExtCenter + off;
            if (sx <= xFil || sx + 2 >= w - 2) continue;
            if (!isVerticallySolidAt(sx) || !isVerticallySolidAt(sx - 1) ||
                !isVerticallySolidAt(sx + 1) || !isVerticallySolidAt(sx + 2)) {
                continue;
            }
            Rgb c = getRgb(sx, midY);
            if (sameRgb(c, getRgb(sx - 1, midY)) &&
                sameRgb(c, getRgb(sx + 1, midY)) &&
                sameRgb(c, getRgb(sx + 2, midY))) {
                outRgb = c;
                found  = true;
                break;
            }
        }
        if (!found) {
            outRgb = getRgb(xFil + minFilSpan / 2, midY);
            found  = true;
        }
    }

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    return found;
}

// Triggers PlaterPresetComboBox::change_extruder_color() seamlessly via Hook_ChooseColorW
inline bool ApplyColorToComboExtruder(HWND hCombo, const Rgb& color) {
    if (!hCombo || !IsWindow(hCombo)) return false;
    RECT rc{};
    GetClientRect(hCombo, &rc);
    int dpi = GetWindowDpiSafe(hCombo);
    // x = 24 @96 DPI lies inside both normal [4..32] and incompatible wide_icons [20..48] swatch hit zones!
    int clickX = MulDiv(24, dpi, 96);
    int clickY = std::max(6, (int)(rc.bottom - rc.top) / 2);

    g_autoChooseColorValue  = color.ToColorRef();
    g_autoChooseColorActive = true;
    SendMessageW(hCombo, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(clickX, clickY));
    SendMessageW(hCombo, WM_LBUTTONUP,   0,          MAKELPARAM(clickX, clickY));
    g_autoChooseColorActive = false;

    InvalidateRect(hCombo, nullptr, TRUE);
    return true;
}

inline void CallSelectFilamentFromConnect(const std::string* pPayload) {
    typedef void* (__fastcall *FnWxGetApp)();
    typedef void  (__fastcall *FnSelectFromConnect)(void* app, const std::string& jsonStr);
    BYTE* pGetApp = dll::At(dll::kRvaWxGetApp);
    BYTE* pSelect = dll::At(dll::kRvaGuiAppSelectFilamentConnect);
    if (!pGetApp || !pSelect || !pPayload) return;
    __try {
        void* app = reinterpret_cast<FnWxGetApp>(pGetApp)();
        if (app) reinterpret_cast<FnSelectFromConnect>(pSelect)(app, *pPayload);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Selects a filament profile compatible with a material type for one extruder, exactly like
// PrusaSlicer does when a printer is chosen from Prusa Connect
// (GUI_App::select_filament_from_connect). No-op when PrusaSlicer.dll is not the supported build.
inline void SelectCompatibleFilamentProfile(int extruderOneBased, const std::string& matCode, bool hardened) {
    if (extruderOneBased < 1 || extruderOneBased > kToolCount) return;
    std::string payload = "{\"tools\":{\"" + std::to_string(extruderOneBased) + "\":{\"material\":" +
                          JsonQuote(matCode) + ",\"hardened\":" + (hardened ? "true" : "false") + "}}}";
    CallSelectFilamentFromConnect(&payload);
}

// ----------------------------------------------------------------------------
// "Assign a material to <tool>" dialog — same catalogue and same call as the
// Prusa Connect "Tools" tab, plus a custom material (type + color) fallback.
// ----------------------------------------------------------------------------
struct MaterialDialogResult {
    enum Kind { None, Catalogue, Custom } kind = None;
    MaterialItem item;
    std::string  customMaterial;
    Rgb          customColor;
};

class MaterialDialog {
public:
    static MaterialDialog*& Current() {
        static MaterialDialog* current = nullptr;
        return current;
    }

    MaterialDialog(HWND hOwner, SyncEngine& engine, const UiPalette& pal,
                   const std::wstring& title, const MachineSlot& slot)
        : m_hOwner(hOwner), m_engine(engine), m_pal(pal), m_title(title), m_slot(slot)
    {
        m_customColor = (slot.loaded && slot.color.assigned) ? slot.color
                                                             : GetBuddyMaterialDef(slot.material).fallbackColor;
        m_customColor.assigned = true;
    }

    MaterialDialogResult RunModal() {
        static bool s_registered = false;
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        if (!s_registered) {
            WNDCLASSEXW wc{ sizeof(wc) };
            wc.lpfnWndProc   = WndProc;
            wc.hInstance     = hInst;
            wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = L"OpenPS_SyncMaterialDlg";
            RegisterClassExW(&wc);
            s_registered = true;
        }

        m_dpi = GetWindowDpiSafe(m_hOwner);
        RECT rcWin{ 0, 0, Px(560), Px(606) };
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        DWORD exStyle = WS_EX_DLGMODALFRAME;
        AdjustWindowRectEx(&rcWin, style, FALSE, exStyle);
        int w = rcWin.right - rcWin.left;
        int h = rcWin.bottom - rcWin.top;

        RECT rcOwner{};
        GetWindowRect(m_hOwner, &rcOwner);
        int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - w) / 2;
        int y = rcOwner.top  + ((rcOwner.bottom - rcOwner.top) - h) / 2;

        Current() = this;
        EnableWindow(m_hOwner, FALSE);
        m_hDlg = CreateWindowExW(exStyle, L"OpenPS_SyncMaterialDlg", m_title.c_str(), style,
                                 x, std::max(0, y), w, h, m_hOwner, nullptr, hInst, this);
        if (m_hDlg) {
            ShowWindow(m_hDlg, SW_SHOW);
            UpdateWindow(m_hDlg);
            MSG msg{};
            while (m_hDlg && IsWindow(m_hDlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
                if (!IsDialogMessageW(m_hDlg, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }
        if (!IsWindowEnabled(m_hOwner)) EnableWindow(m_hOwner, TRUE);
        Current() = nullptr;
        return m_result;
    }

    // Called by the workspace when the engine posted SYNC_EVT_MATERIALS_DONE
    void OnMaterialsEvent(int seq) {
        if (seq != m_seq || !m_hDlg) return;
        bool ok = false;
        std::vector<MaterialItem> items;
        if (!m_engine.GetMaterialsCopy(seq, items, ok)) return;
        m_items = std::move(items);

        SendMessageW(m_hList, WM_SETREDRAW, FALSE, 0);
        SendMessageW(m_hList, LB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < m_items.size(); ++i) {
            SendMessageW(m_hList, LB_ADDSTRING, 0, (LPARAM)i);
        }
        SendMessageW(m_hList, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(m_hList, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);

        m_statusError = !ok;
        SetWindowTextW(m_hStatus, !ok ? Tr(i18n::S_DLG_FAILED)
                                      : (m_items.empty() ? Tr(i18n::S_DLG_NONE) : L""));
        EnableWindow(m_hAssign, FALSE);
    }

private:
    enum : int { IDC_SEARCH = 2001, IDC_LIST, IDC_STATUS, IDC_CUSTOM_LABEL, IDC_COMBO, IDC_COLOR, IDC_APPLY_CUSTOM };
    static constexpr UINT_PTR kTimerSearch = 1;

    HWND                  m_hOwner;
    SyncEngine&           m_engine;
    UiPalette             m_pal;
    std::wstring          m_title;
    MachineSlot           m_slot;
    MaterialDialogResult  m_result;
    Rgb                   m_customColor;

    HWND   m_hDlg = nullptr, m_hSearch = nullptr, m_hList = nullptr, m_hStatus = nullptr;
    HWND   m_hCustomLabel = nullptr, m_hCombo = nullptr, m_hColor = nullptr, m_hApplyCustom = nullptr;
    HWND   m_hAssign = nullptr, m_hCancel = nullptr;
    HFONT  m_fontBody = nullptr, m_fontBold = nullptr;
    HBRUSH m_brBg = nullptr;
    int    m_dpi = 96;
    int    m_seq = 0;
    bool   m_statusError = false;
    std::vector<MaterialItem> m_items;

    int Px(int v) const { return MulDiv(v, m_dpi, 96); }

    static int& SeqCounter() {
        static int counter = 0;
        return counter;
    }

    void StartSearch() {
        wchar_t buf[256]{};
        GetWindowTextW(m_hSearch, buf, 255);
        m_seq = ++SeqCounter();
        m_statusError = false;
        SetWindowTextW(m_hStatus, Tr(i18n::S_DLG_LOADING));
        m_engine.RequestMaterials(WideToUtf8(buf), m_seq);
    }

    void Close() {
        if (!m_hDlg) return;
        HWND h = m_hDlg;
        m_hDlg = nullptr;
        KillTimer(h, kTimerSearch);
        // Re-enable the owner first so activation returns to it
        EnableWindow(m_hOwner, TRUE);
        SetForegroundWindow(m_hOwner);
        DestroyWindow(h);
    }

    void AcceptCatalogue() {
        LRESULT sel = SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
        if (sel == LB_ERR || (size_t)sel >= m_items.size()) return;
        m_result.kind = MaterialDialogResult::Catalogue;
        m_result.item = m_items[(size_t)sel];
        Close();
    }

    void AcceptCustom() {
        LRESULT sel = SendMessageW(m_hCombo, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel >= kBuddyMaterialCount) sel = 0;
        m_result.kind           = MaterialDialogResult::Custom;
        m_result.customMaterial = kBuddyMaterials[sel].code;
        m_result.customColor    = m_customColor;
        Close();
    }

    void PickCustomColor() {
        CHOOSECOLORW cc{};
        cc.lStructSize  = sizeof(cc);
        cc.hwndOwner    = m_hDlg;
        cc.lpCustColors = g_customColors;
        cc.rgbResult    = m_customColor.ToColorRef();
        cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
        g_autoChooseColorActive = false;
        if (Hook_ChooseColorW(&cc)) {
            m_customColor = Rgb::FromColorRef(cc.rgbResult, true);
            SetButtonSwatchColor(m_hColor, cc.rgbResult);
        }
    }

    void CreateControls(HINSTANCE hInst) {
        const std::wstring& fam = UiFontFamily();
        m_fontBody = CreateFontW(-Px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, fam.c_str());
        m_fontBold = CreateFontW(-Px(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, fam.c_str());
        m_brBg = CreateSolidBrush(ToColorRef(m_pal.bg));

        RECT rc{};
        GetClientRect(m_hDlg, &rc);
        int cw = rc.right, ch = rc.bottom;
        int m = Px(16), btnH = Px(31);

        m_hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    m, m, cw - 2 * m, Px(30), m_hDlg, (HMENU)(INT_PTR)IDC_SEARCH, hInst, nullptr);
        SendMessageW(m_hSearch, WM_SETFONT, (WPARAM)m_fontBody, TRUE);
        SendMessageW(m_hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)Tr(i18n::S_DLG_SEARCH));

        // Bottom row: [Cancel] [Assign]
        std::wstring assignText = Tr(i18n::S_ASSIGN);
        std::wstring cancelText = Tr(i18n::S_CANCEL);
        int wAssign = std::max(Px(110), MeasureConnectButtonWidth(m_hDlg, assignText, BtnKind::Solid, BtnIcon::None));
        int wCancel = std::max(Px(96),  MeasureConnectButtonWidth(m_hDlg, cancelText, BtnKind::Outline, BtnIcon::None));
        int bottomY = ch - m - btnH;
        m_hAssign = CreateConnectButton(m_hDlg, IDOK, assignText, BtnKind::Solid, BtnIcon::None, true);
        m_hCancel = CreateConnectButton(m_hDlg, IDCANCEL, cancelText, BtnKind::Outline, BtnIcon::None, true);
        SetWindowPos(m_hAssign, nullptr, cw - m - wAssign, bottomY, wAssign, btnH, SWP_NOZORDER);
        SetWindowPos(m_hCancel, nullptr, cw - m - wAssign - Px(8) - wCancel, bottomY, wCancel, btnH, SWP_NOZORDER);
        EnableWindow(m_hAssign, FALSE);

        // Custom material row: label / [type] [color] [Apply]
        int customRowY = bottomY - Px(20) - btnH;
        int customLblY = customRowY - Px(26);
        m_hCustomLabel = CreateWindowExW(0, L"STATIC", Tr(i18n::S_DLG_CUSTOM), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                                         m, customLblY, cw - 2 * m, Px(20), m_hDlg,
                                         (HMENU)(INT_PTR)IDC_CUSTOM_LABEL, hInst, nullptr);
        SendMessageW(m_hCustomLabel, WM_SETFONT, (WPARAM)m_fontBold, TRUE);

        m_hCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                   m, customRowY + Px(2), Px(130), Px(320), m_hDlg,
                                   (HMENU)(INT_PTR)IDC_COMBO, hInst, nullptr);
        SendMessageW(m_hCombo, WM_SETFONT, (WPARAM)m_fontBody, TRUE);
        int curSel = 0;
        for (int k = 0; k < kBuddyMaterialCount; ++k) {
            SendMessageW(m_hCombo, CB_ADDSTRING, 0, (LPARAM)Utf8ToWide(kBuddyMaterials[k].code).c_str());
            if (m_slot.loaded && m_slot.material == kBuddyMaterials[k].code) curSel = k;
        }
        SendMessageW(m_hCombo, CB_SETCURSEL, (WPARAM)curSel, 0);

        m_hColor = CreateConnectButton(m_hDlg, IDC_COLOR, L"", BtnKind::Swatch, BtnIcon::None, true);
        SetWindowPos(m_hColor, nullptr, m + Px(138), customRowY, Px(48), btnH, SWP_NOZORDER);
        SetButtonSwatchColor(m_hColor, m_customColor.ToColorRef());

        std::wstring applyText = Tr(i18n::S_APPLY);
        int wApply = std::max(Px(96), MeasureConnectButtonWidth(m_hDlg, applyText, BtnKind::Outline, BtnIcon::None));
        m_hApplyCustom = CreateConnectButton(m_hDlg, IDC_APPLY_CUSTOM, applyText, BtnKind::Outline, BtnIcon::None, true);
        SetWindowPos(m_hApplyCustom, nullptr, m + Px(194), customRowY, wApply, btnH, SWP_NOZORDER);

        // Catalogue list + status line fill the remaining height
        int statusY = customLblY - Px(14) - Px(20);
        m_hStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS,
                                    m, statusY, cw - 2 * m, Px(20), m_hDlg, (HMENU)(INT_PTR)IDC_STATUS, hInst, nullptr);
        SendMessageW(m_hStatus, WM_SETFONT, (WPARAM)m_fontBody, TRUE);

        int listY = m + Px(30) + Px(8);
        m_hList = CreateWindowExW(0, L"LISTBOX", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_BORDER |
                                  LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                  m, listY, cw - 2 * m, statusY - Px(6) - listY, m_hDlg,
                                  (HMENU)(INT_PTR)IDC_LIST, hInst, nullptr);

        // Same starting point as Connect: materials compatible with the type reported by the tool
        if (m_slot.loaded) {
            std::string type = !m_slot.abbrev.empty() ? m_slot.abbrev : m_slot.material;
            SetWindowTextW(m_hSearch, Utf8ToWide(type).c_str());
        }
        SendMessageW(m_hSearch, EM_SETSEL, 0, -1);
        SetFocus(m_hSearch);
        KillTimer(m_hDlg, kTimerSearch);
        StartSearch();
    }

    void DrawListItem(const DRAWITEMSTRUCT* dis) {
        int w = dis->rcItem.right - dis->rcItem.left;
        int h = dis->rcItem.bottom - dis->rcItem.top;
        if (w <= 0 || h <= 0) return;
        auto S = [&](float v) { return v * (float)m_dpi / 96.0f; };

        HDC hdcMem = CreateCompatibleDC(dis->hDC);
        HBITMAP hBmp = CreateCompatibleBitmap(dis->hDC, w, h);
        HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
        {
            Gdiplus::Graphics g(hdcMem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
            bool selected = (dis->itemState & ODS_SELECTED) != 0;
            g.Clear(selected ? m_pal.primaryFaded : m_pal.bg);

            if (dis->itemID != (UINT)-1 && dis->itemID < m_items.size()) {
                const MaterialItem& it = m_items[dis->itemID];

                float d = S(24.0f);
                Gdiplus::RectF rcDot(S(12.0f), ((float)h - d) * 0.5f, d, d);
                if (it.color.assigned) {
                    Gdiplus::SolidBrush br(Gdiplus::Color(255, it.color.r, it.color.g, it.color.b));
                    g.FillEllipse(&br, rcDot);
                } else {
                    Gdiplus::SolidBrush br(m_pal.hover);
                    g.FillEllipse(&br, rcDot);
                }
                Gdiplus::Pen ring(m_pal.borderSecondary, 1.0f);
                g.DrawEllipse(&ring, rcDot);

                auto fName = MakeUiFont(S(14.0f), 500);
                auto fSub  = MakeUiFont(S(12.0f), 400);
                Gdiplus::SolidBrush brText(m_pal.text);
                Gdiplus::SolidBrush brSub(selected ? m_pal.text : m_pal.textSecondary);
                Gdiplus::StringFormat sf;
                sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

                std::wstring name = Utf8ToWide(it.name.empty() ? it.abbreviation : it.name);
                if (name.empty()) name = Tr(i18n::S_UNKNOWN_MATERIAL);
                std::wstring sub = Utf8ToWide(it.brand);
                std::wstring type = Utf8ToWide(!it.abbreviation.empty() ? it.abbreviation : it.type);
                if (!type.empty()) sub += (sub.empty() ? L"" : L" · ") + type;

                float tx = rcDot.GetRight() + S(12.0f);
                float tw = (float)w - tx - S(10.0f);
                g.DrawString(name.c_str(), (INT)name.size(), fName.get(),
                             Gdiplus::RectF(tx, S(4.0f), tw, (float)h * 0.5f - S(2.0f)), &sf, &brText);
                g.DrawString(sub.c_str(), (INT)sub.size(), fSub.get(),
                             Gdiplus::RectF(tx, (float)h * 0.5f - S(1.0f), tw, (float)h * 0.5f - S(4.0f)), &sf, &brSub);

                Gdiplus::Pen sep(m_pal.borderSecondary, 1.0f);
                g.DrawLine(&sep, 0.0f, (float)h - 0.5f, (float)w, (float)h - 0.5f);
            }
        }
        BitBlt(dis->hDC, dis->rcItem.left, dis->rcItem.top, w, h, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOld);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        MaterialDialog* self = reinterpret_cast<MaterialDialog*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<MaterialDialog*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
            if (self) self->m_hDlg = hWnd;
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        if (!self) return DefWindowProcW(hWnd, msg, wParam, lParam);

        switch (msg) {
        case WM_CREATE:
            self->CreateControls(reinterpret_cast<CREATESTRUCTW*>(lParam)->hInstance);
            return 0;

        case WM_ERASEBKGND: {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            FillRect((HDC)wParam, &rc, self->m_brBg);
            return 1;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            bool isStatus = ((HWND)lParam == self->m_hStatus);
            const Gdiplus::Color& fg = isStatus ? (self->m_statusError ? self->m_pal.danger : self->m_pal.textSecondary)
                                                : self->m_pal.text;
            SetTextColor(hdc, ToColorRef(fg));
            SetBkColor(hdc, ToColorRef(self->m_pal.bg));
            return (LRESULT)self->m_brBg;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (mis && mis->CtlType == ODT_LISTBOX) {
                mis->itemHeight = (UINT)self->Px(48);
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (dis && dis->CtlType == ODT_LISTBOX) {
                self->DrawListItem(dis);
                return TRUE;
            }
            if (dis && dis->CtlType == ODT_BUTTON) {
                DrawConnectButton(dis, self->m_pal);
                return TRUE;
            }
            break;
        }

        case WM_TIMER:
            if (wParam == kTimerSearch) {
                KillTimer(hWnd, kTimerSearch);
                self->StartSearch();
                return 0;
            }
            break;

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == IDC_SEARCH && code == EN_CHANGE) {
                if (self->m_hList) SetTimer(hWnd, kTimerSearch, 350, nullptr);
                return 0;
            }
            if (id == IDC_LIST && code == LBN_SELCHANGE) {
                EnableWindow(self->m_hAssign, SendMessageW(self->m_hList, LB_GETCURSEL, 0, 0) != LB_ERR);
                return 0;
            }
            if (id == IDC_LIST && code == LBN_DBLCLK) { self->AcceptCatalogue(); return 0; }
            if (id == IDOK) {
                if (GetFocus() == self->m_hSearch &&
                    SendMessageW(self->m_hList, LB_GETCURSEL, 0, 0) == LB_ERR) {
                    KillTimer(hWnd, kTimerSearch);
                    self->StartSearch();
                } else {
                    self->AcceptCatalogue();
                }
                return 0;
            }
            if (id == IDCANCEL)         { self->Close(); return 0; }
            if (id == IDC_COLOR)        { self->PickCustomColor(); return 0; }
            if (id == IDC_APPLY_CUSTOM) { self->AcceptCustom(); return 0; }
            break;
        }

        case WM_CLOSE:
            self->Close();
            return 0;

        case WM_NCDESTROY:
            if (self->m_fontBody) { DeleteObject(self->m_fontBody); self->m_fontBody = nullptr; }
            if (self->m_fontBold) { DeleteObject(self->m_fontBold); self->m_fontBold = nullptr; }
            if (self->m_brBg)     { DeleteObject(self->m_brBg);     self->m_brBg = nullptr; }
            self->m_hDlg = nullptr;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
            break;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
};

// ----------------------------------------------------------------------------
// Main Sync Tab Controller & Native Windows State
// ----------------------------------------------------------------------------
class SyncTabManager {
public:
    static SyncTabManager& Instance() {
        static SyncTabManager inst;
        return inst;
    }

    bool IsActive() const { return m_syncActive; }

    void AttachAndSync(HWND hMainWnd) {
        if (!hMainWnd || !IsWindow(hMainWnd)) return;
        m_hMainWnd  = hMainWnd;

        if (!m_configLoaded) {
            m_userCfg.Load();
            m_configLoaded = true;
        }

        HWND hTopBarItems = FindVisibleTopBarItemsCtrl(hMainWnd);
        if (!hTopBarItems || !IsWindow(hTopBarItems)) return;

        HWND hHostBook = GetParent(hTopBarItems);
        if (!hHostBook || !IsWindow(hHostBook)) return;

        EnsureWindowClasses();
        EnsureTabButtonAndLayout(hTopBarItems, hHostBook);

        if (m_syncActive) {
            EnsureWorkspaceLayout(hTopBarItems, hHostBook);
        }
    }

    void Shutdown() {
        m_engine.Stop();
        if (m_hSyncBtn && IsWindow(m_hSyncBtn)) {
            DestroyWindow(m_hSyncBtn);
            m_hSyncBtn = nullptr;
        }
        if (m_hWorkspace && IsWindow(m_hWorkspace)) {
            DestroyWindow(m_hWorkspace);
            m_hWorkspace = nullptr;
        }
        m_syncActive = false;
    }

    void ActivateSyncTab() {
        if (!m_hTopBarItems || !IsWindow(m_hTopBarItems)) return;
        HWND hHostBook = GetParent(m_hTopBarItems);
        if (!hHostBook || !IsWindow(hHostBook)) return;

        // Read Slicer filament combos BEFORE hiding Plater
        RefreshSlicerSlotsFromHost();

        if (!m_syncActive) {
            m_hiddenPages.clear();
            HWND hContainer = ResolvePageContainer(hHostBook);
            for (HWND hChild = GetWindow(hContainer, GW_CHILD); hChild != nullptr; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                if (hChild == m_hTopBarItems || hChild == m_hWorkspace || hChild == hHostBook) continue;
                LONG st = GetWindowLongW(hChild, GWL_STYLE);
                if (!(st & WS_VISIBLE)) continue;
                RECT rc{};
                GetWindowRect(hChild, &rc);
                if ((rc.bottom - rc.top) > 100 && (rc.right - rc.left) > 200) {
                    m_hiddenPages.push_back(hChild);
                    ShowWindow(hChild, SW_HIDE);
                }
            }
            m_syncActive = true;
        }

        EnsureWorkspaceWindow(ResolvePageContainer(hHostBook));
        EnsureWorkspaceLayout(m_hTopBarItems, hHostBook);

        ShowWindow(m_hWorkspace, SW_SHOW);
        SetWindowPos(m_hWorkspace, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        m_engine.Start(m_hWorkspace);
        m_engine.RequestRefresh(m_userCfg);

        RelayoutAndRefresh();
        InvalidateAllTopBarButtons();
    }

    // Labels are read from the translation tables at each layout and paint
    void OnLanguageChanged() {
        if (m_hWorkspace && IsWindow(m_hWorkspace)) RelayoutAndRefresh();
    }

    void DeactivateSyncTab(bool restoreHiddenPages = true) {
        if (!m_syncActive) return;
        m_syncActive = false;

        if (m_hWorkspace && IsWindow(m_hWorkspace)) {
            ShowWindow(m_hWorkspace, SW_HIDE);
        }
        if (restoreHiddenPages) {
            for (HWND hPage : m_hiddenPages) {
                if (hPage && IsWindow(hPage)) {
                    ShowWindow(hPage, SW_SHOW);
                }
            }
        }
        m_hiddenPages.clear();
        InvalidateAllTopBarButtons();
    }

private:
    struct CardGeom {
        Gdiplus::RectF rc{};
        RECT           btnA{};
        RECT           btnB{};
    };

    // Geometry of the page in content coordinates (before vertical scrolling)
    struct PageLayout {
        float contentH = 0.0f;
        float x0 = 0.0f;
        float blockW = 0.0f;
        bool  compact = false;
        int   nMachine = 0;
        int   nSlicer = 0;
        bool  showPrinterBtn = false;
        bool  showState = false;
        Gdiplus::RectF title1{}, title2{}, state{}, status{}, empty1{}, empty2{};
        RECT  btnRefresh{}, btnPrinter{}, btnToSlicer{}, btnToMachine{};
        CardGeom machine[kToolCount]{};
        CardGeom slicer[kToolCount]{};
    };

    struct CardContent {
        std::wstring header;
        std::wstring nozzle;
        bool         hasBar = false;
        float        ratio = 0.0f;
        bool         hasColor = false;
        Rgb          color;
        std::wstring abbr;
        std::wstring name;
        bool         nameMuted = false;
        std::wstring lineA;
        std::wstring lineB;
        bool         ring = false;
    };

    HWND                       m_hMainWnd     = nullptr;
    HWND                       m_hTopBarItems = nullptr;
    HWND                       m_hSyncBtn     = nullptr;
    HWND                       m_hWorkspace   = nullptr;
    bool                       m_classesReg   = false;
    bool                       m_configLoaded = false;
    bool                       m_syncActive   = false;
    bool                       m_syncBtnHover = false;
    std::vector<HWND>          m_hiddenPages;
    std::vector<HWND>          m_nativeTabBtns;

    SyncUserConfig                        m_userCfg;
    SyncEngine                            m_engine;
    std::vector<MachineSnapshot>          m_printers;
    int                                   m_selectedPrinterIdx = -1;
    MachineSnapshot                       m_machine{};
    std::array<SlicerSlot, kToolCount>    m_slicer{};
    std::array<Rgb, kToolCount>           m_cachedSlicerColors{};
    std::array<bool, kToolCount>          m_hasCachedSlicerColor{};
    int                                   m_activeSlicerExtruders = 0;
    std::wstring                          m_slicerNozzle;
    std::wstring                          m_statusLine;
    bool                                  m_statusOk = true;

    UiPalette                             m_pal = UiPalette::ForTheme(false);
    PageLayout                            m_layout{};
    int                                   m_scrollY = 0;
    HBRUSH                                m_brBg = nullptr;
    COLORREF                              m_brBgColor = CLR_INVALID;

    HWND m_btnRefresh = nullptr, m_btnPrinter = nullptr, m_btnToSlicer = nullptr, m_btnToMachine = nullptr;
    std::array<HWND, kToolCount> m_btnMachineA{}, m_btnMachineB{}, m_btnSlicerA{}, m_btnSlicerB{};

    // Locate visible TopBarItemsCtrl (identified as the top wxWindowNR owning the Search Edit child)
    static HWND FindVisibleTopBarItemsCtrl(HWND hMainWnd) {
        HWND found = nullptr;
        RECT rcMain{};
        GetWindowRect(hMainWnd, &rcMain);

        EnumChildWindows(hMainWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            wchar_t cls[64]{};
            GetClassNameW(hChild, cls, 63);
            if (_wcsicmp(cls, L"Edit") != 0) return TRUE;

            HWND hSearchBox = GetParent(hChild);      // TextInput
            if (!hSearchBox) return TRUE;
            HWND hTopItems  = GetParent(hSearchBox);  // TopBarItemsCtrl
            if (!hTopItems) return TRUE;
            HWND hTopBar    = GetParent(hTopItems);   // TopBar
            if (!hTopBar) return TRUE;

            if (!IsWindowVisible(hTopBar) || !IsWindowVisible(hTopItems)) return TRUE;

            RECT rcItems{};
            GetWindowRect(hTopItems, &rcItems);
            if ((rcItems.right - rcItems.left) < 400 || (rcItems.bottom - rcItems.top) > 90) return TRUE;

            *reinterpret_cast<HWND*>(lParam) = hTopItems;
            return FALSE;
        }, (LPARAM)&found);

        return found;
    }

    HWND ResolvePageContainer(HWND hHostBook) const {
        RECT rcBook{};
        GetClientRect(hHostBook, &rcBook);
        if ((rcBook.bottom - rcBook.top) < 120 && m_hMainWnd) {
            return m_hMainWnd;
        }
        return hHostBook;
    }

    void EnsureWindowClasses() {
        if (m_classesReg) return;
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        WNDCLASSEXW wcBtn{ sizeof(wcBtn) };
        wcBtn.style         = CS_HREDRAW | CS_VREDRAW;
        wcBtn.lpfnWndProc   = SyncBtnWndProc;
        wcBtn.hInstance     = hInst;
        wcBtn.hCursor       = LoadCursorW(nullptr, IDC_HAND);
        wcBtn.lpszClassName = L"OpenPS_SyncTabBtn";
        RegisterClassExW(&wcBtn);

        WNDCLASSEXW wcWs{ sizeof(wcWs) };
        wcWs.style         = CS_HREDRAW | CS_VREDRAW;
        wcWs.lpfnWndProc   = WorkspaceWndProc;
        wcWs.hInstance     = hInst;
        wcWs.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wcWs.lpszClassName = L"OpenPS_SyncWorkspace";
        RegisterClassExW(&wcWs);

        m_classesReg = true;
    }

    void EnsureWorkspaceWindow(HWND hContainer) {
        if (m_hWorkspace && IsWindow(m_hWorkspace)) {
            if (GetParent(m_hWorkspace) != hContainer) {
                SetParent(m_hWorkspace, hContainer);
            }
            return;
        }
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        m_hWorkspace = CreateWindowExW(
            0, L"OpenPS_SyncWorkspace", L"",
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 44, 800, 600,
            hContainer, nullptr, hInst, this);
    }

    void InvalidateAllTopBarButtons() {
        if (m_hSyncBtn && IsWindow(m_hSyncBtn)) {
            InvalidateRect(m_hSyncBtn, nullptr, FALSE);
        }
        for (HWND hBtn : m_nativeTabBtns) {
            if (hBtn && IsWindow(hBtn)) {
                InvalidateRect(hBtn, nullptr, FALSE);
            }
        }
        if (m_hTopBarItems && IsWindow(m_hTopBarItems)) {
            InvalidateRect(m_hTopBarItems, nullptr, FALSE);
        }
    }

    // Subclasses native TopBar tab buttons so clicking any native tab cleanly exits Sync
    // and visually deselects the black active pill while Sync is active!
    static LRESULT CALLBACK NativeTabBtnSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                     UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        SyncTabManager* self = reinterpret_cast<SyncTabManager*>(dwRefData);
        switch (uMsg) {
        case WM_LBUTTONDOWN:
            if (self && self->m_syncActive) {
                self->DeactivateSyncTab(true);
            }
            break;

        case WM_PAINT:
            if (self && self->m_syncActive) {
                RECT rc{};
                GetClientRect(hWnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                if (w > 8 && h > 8) {
                    // Let wxWidgets TopBarItemsCtrl::Button::render() paint onto hWnd / wParam DC first!
                    // Unselected buttons remain 100% untouched; ONLY the 1 previously-selected button (dark pill)
                    // has pillDiff > 160 and gets smoothly inverted to unselected colors.
                    LRESULT paintRes = DefSubclassProc(hWnd, WM_PAINT, wParam, lParam);

                    HDC hdcTarget = (wParam != 0) ? reinterpret_cast<HDC>(wParam) : GetDC(hWnd);
                    if (hdcTarget) {
                        BITMAPINFO bmi{};
                        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth       = w;
                        bmi.bmiHeader.biHeight      = -h;
                        bmi.bmiHeader.biPlanes      = 1;
                        bmi.bmiHeader.biBitCount    = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        void* bits = nullptr;
                        HDC hdcMem = CreateCompatibleDC(hdcTarget);
                        HBITMAP hBmp = CreateDIBSection(hdcTarget, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
                        if (hBmp && bits) {
                            HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
                            BitBlt(hdcMem, 0, 0, w, h, hdcTarget, 0, 0, SRCCOPY);

                            uint32_t* px = static_cast<uint32_t*>(bits);
                            uint32_t cornerPx = px[0];
                            int bgR = (cornerPx >> 16) & 0xFF;
                            int bgG = (cornerPx >> 8)  & 0xFF;
                            int bgB =  cornerPx        & 0xFF;

                            uint32_t pillPx = px[(h / 2) * w + std::min(5, w - 1)];
                            int pR = (pillPx >> 16) & 0xFF;
                            int pG = (pillPx >> 8)  & 0xFF;
                            int pB =  pillPx        & 0xFF;

                            int pillDiff = std::abs(pR - bgR) + std::abs(pG - bgG) + std::abs(pB - bgB);
                            if (pillDiff > 160) {
                                POINT ptCur{};
                                GetCursorPos(&ptCur);
                                ScreenToClient(hWnd, &ptCur);
                                bool hovered = (ptCur.x >= 0 && ptCur.x < w && ptCur.y >= 0 && ptCur.y < h);
                                bool isDark  = ((299 * bgR + 587 * bgG + 114 * bgB) / 1000) < 110;

                                int dstBgR = hovered ? (isDark ? 0x38 : 0xF6) : bgR;
                                int dstBgG = hovered ? (isDark ? 0x24 : 0xDC) : bgG;
                                int dstBgB = hovered ? (isDark ? 0x26 : 0xDC) : bgB;

                                int dstFgR = hovered ? (isDark ? 0xFF : 0x76) : pR;
                                int dstFgG = hovered ? (isDark ? 0xE4 : 0x1E) : pG;
                                int dstFgB = hovered ? (isDark ? 0xE6 : 0x22) : pB;

                                float rad = std::max(3.0f, h * 0.16f);
                                for (int y = 0; y < h; ++y) {
                                    for (int x = 0; x < w; ++x) {
                                        float dx = 0.0f, dy = 0.0f;
                                        if (x < rad)           dx = rad - (x + 0.5f);
                                        else if (x >= w - rad) dx = (x + 0.5f) - (w - rad);
                                        if (y < rad)           dy = rad - (y + 0.5f);
                                        else if (y >= h - rad) dy = (y + 0.5f) - (h - rad);
                                        if (dx * dx + dy * dy > rad * rad + 1.0f) continue;

                                        uint32_t cur = px[y * w + x];
                                        int r = (cur >> 16) & 0xFF;
                                        int g = (cur >> 8)  & 0xFF;
                                        int b =  cur        & 0xFF;

                                        float denom = (float)(std::abs(bgR - pR) + std::abs(bgG - pG) + std::abs(bgB - pB));
                                        float num   = (float)(std::abs(r - pR)   + std::abs(g - pG)   + std::abs(b - pB));
                                        float t     = (denom > 1.0f) ? std::clamp(num / denom, 0.0f, 1.0f) : 0.0f;

                                        uint8_t outR = (uint8_t)std::clamp((int)std::lround(dstBgR + t * (dstFgR - dstBgR)), 0, 255);
                                        uint8_t outG = (uint8_t)std::clamp((int)std::lround(dstBgG + t * (dstFgG - dstBgG)), 0, 255);
                                        uint8_t outB = (uint8_t)std::clamp((int)std::lround(dstBgB + t * (dstFgB - dstBgB)), 0, 255);
                                        px[y * w + x] = (0xFFu << 24) | ((uint32_t)outR << 16) | ((uint32_t)outG << 8) | (uint32_t)outB;
                                    }
                                }
                                BitBlt(hdcTarget, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
                            }
                            SelectObject(hdcMem, hOld);
                        }
                        if (hBmp) DeleteObject(hBmp);
                        if (hdcMem) DeleteDC(hdcMem);
                        if (wParam == 0) ReleaseDC(hWnd, hdcTarget);
                    }
                    return paintRes;
                }
            }
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, NativeTabBtnSubclassProc, uIdSubclass);
            break;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    void EnsureTabButtonAndLayout(HWND hTopBarItems, HWND hHostBook) {
        m_hTopBarItems = hTopBarItems;

        // Identify the Search panel and all left-side tab buttons inside hTopBarItems
        HWND hSearchPanel = nullptr;
        std::vector<std::pair<RECT, HWND>> candidates;

        for (HWND hChild = GetWindow(hTopBarItems, GW_CHILD); hChild != nullptr; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
            if (hChild == m_hSyncBtn) continue;
            if (!(GetWindowLongW(hChild, GWL_STYLE) & WS_VISIBLE)) continue;

            // Check if this child owns an Edit box -> Search panel
            HWND hGrand = GetWindow(hChild, GW_CHILD);
            bool hasEdit = false;
            while (hGrand) {
                wchar_t gCls[32]{};
                GetClassNameW(hGrand, gCls, 31);
                if (_wcsicmp(gCls, L"Edit") == 0) { hasEdit = true; break; }
                hGrand = GetWindow(hGrand, GW_HWNDNEXT);
            }
            if (hasEdit) {
                hSearchPanel = hChild;
                continue;
            }

            RECT rc{};
            GetWindowRect(hChild, &rc);
            MapWindowPoints(HWND_DESKTOP, hTopBarItems, (LPPOINT)&rc, 2);
            candidates.push_back({ rc, hChild });
        }

        RECT rcSearch{ 99999, 0, 99999, 0 };
        if (hSearchPanel) {
            GetWindowRect(hSearchPanel, &rcSearch);
            MapWindowPoints(HWND_DESKTOP, hTopBarItems, (LPPOINT)&rcSearch, 2);
        }

        m_nativeTabBtns.clear();
        RECT lastBtnRc{ 8, 6, 80, 34 };
        bool hasLast = false;

        for (const auto& kv : candidates) {
            if (kv.first.left < rcSearch.left) {
                m_nativeTabBtns.push_back(kv.second);
                DWORD_PTR ref = 0;
                if (!GetWindowSubclass(kv.second, NativeTabBtnSubclassProc, SUBCLASS_ID_SYNC_NATIVE_TABBTN, &ref)) {
                    SetWindowSubclass(kv.second, NativeTabBtnSubclassProc, SUBCLASS_ID_SYNC_NATIVE_TABBTN, (DWORD_PTR)this);
                }
                if (!hasLast || kv.first.right > lastBtnRc.right) {
                    lastBtnRc = kv.first;
                    hasLast   = true;
                }
            }
        }

        if (!m_hSyncBtn || !IsWindow(m_hSyncBtn)) {
            m_hSyncBtn = CreateWindowExW(
                0, L"OpenPS_SyncTabBtn", L"Sync",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, 72, 28,
                hTopBarItems, nullptr, GetModuleHandleW(nullptr), this);
        } else if (GetParent(m_hSyncBtn) != hTopBarItems) {
            SetParent(m_hSyncBtn, hTopBarItems);
        }

        int dpi    = GetWindowDpiSafe(hTopBarItems);
        int gap    = MulDiv(6, dpi, 96);
        int btnW   = MulDiv(74, dpi, 96);
        int btnH   = hasLast ? (lastBtnRc.bottom - lastBtnRc.top) : MulDiv(28, dpi, 96);
        int btnX   = hasLast ? (lastBtnRc.right + gap) : MulDiv(12, dpi, 96);
        int btnY   = hasLast ? lastBtnRc.top : MulDiv(6, dpi, 96);

        RECT curRc{};
        GetWindowRect(m_hSyncBtn, &curRc);
        MapWindowPoints(HWND_DESKTOP, hTopBarItems, (LPPOINT)&curRc, 2);
        if (curRc.left != btnX || curRc.top != btnY ||
            (curRc.right - curRc.left) != btnW || (curRc.bottom - curRc.top) != btnH) {
            SetWindowPos(m_hSyncBtn, HWND_TOP, btnX, btnY, btnW, btnH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }

        // Prevent search bar overlap on narrow windows
        if (hSearchPanel && rcSearch.left < btnX + btnW + gap) {
            int newLeft = btnX + btnW + gap;
            int newW    = std::max(MulDiv(80, dpi, 96), (int)(rcSearch.right - newLeft));
            SetWindowPos(hSearchPanel, nullptr, newLeft, rcSearch.top, newW, rcSearch.bottom - rcSearch.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        // Detect if a host page was shown externally (e.g. keyboard shortcut Ctrl+1..Ctrl+4) while Sync was active
        if (m_syncActive) {
            HWND hContainer = ResolvePageContainer(hHostBook);
            for (HWND hChild = GetWindow(hContainer, GW_CHILD); hChild != nullptr; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                if (hChild == m_hTopBarItems || hChild == m_hWorkspace || hChild == hHostBook) continue;
                if (GetWindowLongW(hChild, GWL_STYLE) & WS_VISIBLE) {
                    RECT rc{};
                    GetWindowRect(hChild, &rc);
                    if ((rc.bottom - rc.top) > 120 && (rc.right - rc.left) > 200) {
                        // Host switched tabs via menu/shortcut -> yield cleanly!
                        DeactivateSyncTab(false);
                        break;
                    }
                }
            }
        }
    }

    void EnsureWorkspaceLayout(HWND hTopBarItems, HWND hHostBook) {
        if (!m_hWorkspace || !IsWindow(m_hWorkspace)) return;
        HWND hContainer = ResolvePageContainer(hHostBook);
        RECT rcCont{};
        GetClientRect(hContainer, &rcCont);

        RECT rcTop{};
        GetWindowRect(hTopBarItems, &rcTop);
        MapWindowPoints(HWND_DESKTOP, hContainer, (LPPOINT)&rcTop, 2);

        int x = 0;
        int y = std::max(0, (int)rcTop.bottom);
        int w = std::max(100, (int)(rcCont.right - rcCont.left));
        int h = std::max(100, (int)(rcCont.bottom - y));

        // Leave room for native StatusBar if container is MainFrame
        for (HWND hChild = GetWindow(hContainer, GW_CHILD); hChild != nullptr; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
            if (hChild == m_hWorkspace || hChild == hTopBarItems) continue;
            wchar_t cls[64]{};
            GetClassNameW(hChild, cls, 63);
            if (_wcsicmp(cls, L"msctls_statusbar32") == 0 && (GetWindowLongW(hChild, GWL_STYLE) & WS_VISIBLE)) {
                RECT rcSb{};
                GetWindowRect(hChild, &rcSb);
                h -= (rcSb.bottom - rcSb.top);
                break;
            }
        }

        RECT cur{};
        GetWindowRect(m_hWorkspace, &cur);
        MapWindowPoints(HWND_DESKTOP, hContainer, (LPPOINT)&cur, 2);
        if (cur.left != x || cur.top != y || (cur.right - cur.left) != w || (cur.bottom - cur.top) != h) {
            SetWindowPos(m_hWorkspace, HWND_TOP, x, y, w, h, SWP_NOACTIVATE);
            InvalidateRect(m_hWorkspace, nullptr, FALSE);
        }
    }

    // ------------------------------------------------------------------------
    // Live Slicer <-> Sync Data Binding
    // ------------------------------------------------------------------------
    static std::wstring ExtractNozzleLabel(const std::wstring& printerPreset) {
        // "Prusa CORE One INDX 8T HF0.4 nozzle" -> "0.4"
        std::wstring best;
        for (size_t i = 0; i + 2 < printerPreset.size(); ++i) {
            if (iswdigit(printerPreset[i]) && printerPreset[i + 1] == L'.' && iswdigit(printerPreset[i + 2]) &&
                (i == 0 || !iswdigit(printerPreset[i - 1]))) {
                size_t end = i + 3;
                while (end < printerPreset.size() && iswdigit(printerPreset[end])) ++end;
                if (end - i <= 4 && (printerPreset[i] == L'0' || printerPreset[i] == L'1')) {
                    best = printerPreset.substr(i, end - i);
                }
            }
        }
        return best;
    }

    void RefreshSlicerSlotsFromHost() {
        HostSidebarControls hc = FindHostSidebarCombos(m_hMainWnd);
        m_activeSlicerExtruders = (int)hc.hFilaments.size();
        m_slicerNozzle = ExtractNozzleLabel(GetComboSelectedText(hc.hPrinterCombo));

        for (int i = 0; i < kToolCount; ++i) {
            SlicerSlot& s = m_slicer[i];
            if (i < (int)hc.hFilaments.size()) {
                HWND hCb = hc.hFilaments[i];
                std::wstring presetW = GetComboSelectedText(hCb);
                s.valid       = !presetW.empty();
                s.presetName  = WideToUtf8(presetW);
                s.material    = NormalizeBuddyMaterial(s.presetName);
                if (s.material.empty()) s.material = "PLA";

                const BuddyMaterialDef& def = GetBuddyMaterialDef(s.material);

                // The color swatch can only be read while the plater is displayed: once the Sync tab
                // hides it, the colors read at activation (and the ones set by this tab) are kept.
                Rgb sampled;
                bool canSample = IsWindowVisible(hCb) || !m_hasCachedSlicerColor[i];
                if (canSample && SampleComboExtruderColor(hCb, sampled)) {
                    s.color                   = sampled;
                    m_cachedSlicerColors[i]   = sampled;
                    m_hasCachedSlicerColor[i] = true;
                } else if (m_hasCachedSlicerColor[i]) {
                    s.color = m_cachedSlicerColors[i];
                } else {
                    s.color = def.fallbackColor;
                }
            } else {
                s = SlicerSlot{};
                s.valid = false;
                s.color = Rgb(216, 222, 230, false);
            }
        }
    }

    bool SetSlicerSlotColorAndMaterial(int idx, const Rgb& targetColor, const std::string& targetMat, bool updateMat) {
        if (idx < 0 || idx >= kToolCount) return false;
        HostSidebarControls hc = FindHostSidebarCombos(m_hMainWnd);
        if (idx >= (int)hc.hFilaments.size()) return false;

        HWND hCb = hc.hFilaments[idx];
        if (updateMat && !targetMat.empty()) {
            std::string normTarget = NormalizeBuddyMaterial(targetMat);
            std::wstring curTextW  = GetComboSelectedText(hCb);
            std::string curMat     = NormalizeBuddyMaterial(WideToUtf8(curTextW));

            if (curMat != normTarget) {
                // Select compatible filament preset directly via PrusaSlicer's native select_filament_from_connect
                bool hardened = (idx < kToolCount) ? m_machine.tools[idx].hardened : false;
                SelectCompatibleFilamentProfile(idx + 1, normTarget, hardened);
                // Re-resolve ComboBox handle in case Sidebar recreated preset widgets
                hc = FindHostSidebarCombos(m_hMainWnd);
                if (idx < (int)hc.hFilaments.size()) {
                    hCb = hc.hFilaments[idx];
                }
            }
        }

        if (targetColor.assigned) {
            ApplyColorToComboExtruder(hCb, targetColor);
            m_cachedSlicerColors[idx]   = targetColor;
            m_hasCachedSlicerColor[idx] = true;
            m_slicer[idx].color         = targetColor;
        }

        std::wstring updatedText = GetComboSelectedText(hCb);
        if (!updatedText.empty()) {
            m_slicer[idx].valid       = true;
            m_slicer[idx].presetName  = WideToUtf8(updatedText);
            m_slicer[idx].material    = NormalizeBuddyMaterial(m_slicer[idx].presetName);
            if (m_slicer[idx].material.empty()) m_slicer[idx].material = "PLA";
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // Sync Tab Button Painting & Events ("Sync" pill in TopBarItemsCtrl)
    // ------------------------------------------------------------------------
    static LRESULT CALLBACK SyncBtnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        SyncTabManager* self = reinterpret_cast<SyncTabManager*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<SyncTabManager*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
            return TRUE;
        }
        if (!self) return DefWindowProcW(hWnd, msg, wParam, lParam);

        switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
            if (!self->m_syncBtnHover) {
                self->m_syncBtnHover = true;
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_MOUSELEAVE:
            self->m_syncBtnHover = false;
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            self->ActivateSyncTab();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc{};
            GetClientRect(hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ hOld = SelectObject(hdcMem, hBmp);

            bool isDark = DetectHostDarkTheme(GetParent(hWnd));
            Gdiplus::Color barBg = isDark ? Gdiplus::Color(255, 0x26, 0x26, 0x26) : Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
            Gdiplus::Color pillBg = barBg;
            Gdiplus::Color fgTxt  = isDark ? Gdiplus::Color(255, 0xEF, 0xEF, 0xEF) : Gdiplus::Color(255, 0x18, 0x1A, 0x20);

            if (self->m_syncActive) {
                pillBg = isDark ? Gdiplus::Color(255, 0xEF, 0xEF, 0xEF) : Gdiplus::Color(255, 0x18, 0x1A, 0x20);
                fgTxt  = isDark ? Gdiplus::Color(255, 0x18, 0x1A, 0x20) : Gdiplus::Color(255, 0xFF, 0xFF, 0xFF);
            } else if (self->m_syncBtnHover) {
                pillBg = isDark ? Gdiplus::Color(255, 0x3A, 0x24, 0x26) : Gdiplus::Color(255, 0xF6, 0xDC, 0xDC);
                fgTxt  = isDark ? Gdiplus::Color(255, 0xFF, 0xE4, 0xE6) : Gdiplus::Color(255, 0x76, 0x1E, 0x22);
            }

            {
                Gdiplus::Graphics g(hdcMem);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
                g.Clear(barBg);

                float rad = std::max(4.0f, h * 0.16f);
                FillStrokeRoundRect(g, Gdiplus::RectF(0.5f, 0.5f, (float)w - 1.0f, (float)h - 1.0f),
                                    rad, pillBg, Gdiplus::Color(0, 0, 0, 0), 0.0f);

                int dpi = GetWindowDpiSafe(hWnd);
                float fontPx = (float)MulDiv(13, dpi, 96);
                Gdiplus::Font font(L"Segoe UI", fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                Gdiplus::SolidBrush txtBr(fgTxt);

                // Crisp vector bidirectional arrows icon + "Sync"
                float iconCx = (float)MulDiv(16, dpi, 96);
                float iconCy = h * 0.5f;
                float arm    = (float)MulDiv(5, dpi, 96);
                Gdiplus::Pen iconPen(fgTxt, 1.5f);
                iconPen.SetStartCap(Gdiplus::LineCapRound);
                iconPen.SetEndCap(Gdiplus::LineCapRound);

                g.DrawLine(&iconPen, iconCx - arm, iconCy - 2.5f, iconCx + arm, iconCy - 2.5f);
                g.DrawLine(&iconPen, iconCx + arm - 2.5f, iconCy - 5.0f, iconCx + arm, iconCy - 2.5f);
                g.DrawLine(&iconPen, iconCx - arm, iconCy + 2.5f, iconCx + arm, iconCy + 2.5f);
                g.DrawLine(&iconPen, iconCx - arm + 2.5f, iconCy + 5.0f, iconCx - arm, iconCy + 2.5f);

                Gdiplus::StringFormat sf;
                sf.SetAlignment(Gdiplus::StringAlignmentNear);
                sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                Gdiplus::RectF rcText(iconCx + arm + (float)MulDiv(5, dpi, 96), 0.0f,
                                      (float)w - iconCx, (float)h);
                g.DrawString(L"Sync", -1, &font, rcText, &sf, &txtBr);
            }

            BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hOld);
            DeleteObject(hBmp);
            DeleteDC(hdcMem);
            EndPaint(hWnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // ------------------------------------------------------------------------
    // Workspace: theme, texts, layout
    // ------------------------------------------------------------------------
    static std::wstring ToolLabel(int idx) {
        return std::wstring(Tr(i18n::S_TOOL)) + L" " + std::to_wstring(idx + 1);
    }

    static std::wstring FilamentLabel(int idx) {
        return std::wstring(Tr(i18n::S_FILAMENT)) + L" " + std::to_wstring(idx + 1);
    }

    static std::wstring FormatNozzle(double diameter, bool highFlow) {
        int hundredths = (int)std::lround(diameter * 100.0);
        if (hundredths <= 0) return L"";
        wchar_t buf[24]{};
        if (hundredths % 10 == 0) swprintf_s(buf, L"%d.%d", hundredths / 100, (hundredths / 10) % 10);
        else                      swprintf_s(buf, L"%d.%02d", hundredths / 100, hundredths % 100);
        std::wstring out = buf;
        if (highFlow) out += L" HF";
        return out;
    }

    bool HasPrinter() const { return !m_machine.uuid.empty(); }

    std::string MachineState() const {
        return ToUpperAscii(TrimAscii(m_machine.connectState));
    }

    bool MachineOnline() const {
        std::string st = MachineState();
        return HasPrinter() && m_machine.cloudOnline && st != "OFFLINE";
    }

    std::wstring MachineStateLabel(Gdiplus::Color& dot) const {
        if (!HasPrinter()) { dot = m_pal.stateOff; return L""; }
        std::string st = MachineState();
        if (!MachineOnline()) { dot = m_pal.stateOff; return Tr(i18n::S_STATE_OFFLINE); }
        if (st == "IDLE" || st == "FINISHED" || st == "STOPPED") { dot = m_pal.stateOk; return Tr(i18n::S_STATE_IDLE); }
        if (st == "READY")    { dot = m_pal.stateOk;   return Tr(i18n::S_STATE_READY); }
        if (st == "PRINTING") { dot = m_pal.primary;   return Tr(i18n::S_STATE_PRINTING); }
        if (st == "BUSY")     { dot = m_pal.stateBusy; return Tr(i18n::S_STATE_BUSY); }
        dot = (st == "ERROR" || st == "ATTENTION") ? m_pal.danger : m_pal.stateBusy;
        // Other Connect states ("ATTENTION", "PAUSED"...) shown like Connect: "Attention", "Paused"
        std::string label;
        for (size_t k = 0; k < st.size(); ++k) {
            char c = (st[k] == '_') ? ' ' : st[k];
            label.push_back((k == 0) ? c : (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c));
        }
        return Utf8ToWide(label);
    }

    std::wstring PrinterDisplayName() const {
        if (!HasPrinter()) return L"";
        return Utf8ToWide(m_machine.name.empty() ? m_machine.model : m_machine.name);
    }

    std::wstring Title1() const {
        return HasPrinter() ? Fmt(i18n::S_INSTALLED_IN, { PrinterDisplayName() }) : std::wstring(Tr(i18n::S_NO_PRINTER));
    }

    std::wstring Title2() const { return Fmt(i18n::S_ACTIVE_IN, { kAppName }); }

    std::wstring ToSlicerText() const { return Fmt(i18n::S_SEND_TO, { kAppName }); }

    std::wstring ToMachineText() const {
        return Fmt(i18n::S_SEND_TO, { HasPrinter() ? PrinterDisplayName() : std::wstring(L"…") });
    }

    void UpdatePalette() {
        bool isDark = DetectHostDarkTheme(m_hTopBarItems ? m_hTopBarItems : m_hMainWnd);
        m_pal = UiPalette::ForTheme(isDark);
        COLORREF bg = ToColorRef(m_pal.bg);
        if (!m_brBg || bg != m_brBgColor) {
            if (m_brBg) DeleteObject(m_brBg);
            m_brBg      = CreateSolidBrush(bg);
            m_brBgColor = bg;
        }
    }

    void CreateWorkspaceControls(HWND hWnd) {
        m_btnRefresh   = CreateConnectButton(hWnd, IDC_SYNC_REFRESH, Tr(i18n::S_REFRESH), BtnKind::Outline, BtnIcon::Refresh);
        m_btnPrinter   = CreateConnectButton(hWnd, IDC_SYNC_PRINTER, L"", BtnKind::Outline, BtnIcon::Chevron);
        m_btnToSlicer  = CreateConnectButton(hWnd, IDC_SYNC_TO_SLICER, ToSlicerText(), BtnKind::Brand, BtnIcon::ArrowDown);
        m_btnToMachine = CreateConnectButton(hWnd, IDC_SYNC_TO_MACHINE, ToMachineText(), BtnKind::Solid, BtnIcon::ArrowUp);
        for (int i = 0; i < kToolCount; ++i) {
            m_btnMachineA[i] = CreateConnectButton(hWnd, IDC_SYNC_MACHINE_A + i, Tr(i18n::S_CHANGE), BtnKind::Outline);
            m_btnMachineB[i] = CreateConnectButton(hWnd, IDC_SYNC_MACHINE_B + i, Tr(i18n::S_UNLOAD), BtnKind::Outline);
            m_btnSlicerA[i]  = CreateConnectButton(hWnd, IDC_SYNC_SLICER_A + i,  Tr(i18n::S_CHANGE), BtnKind::Outline);
            m_btnSlicerB[i]  = CreateConnectButton(hWnd, IDC_SYNC_SLICER_B + i,  Tr(i18n::S_COLOR),  BtnKind::Outline);
        }
    }

    static RECT MakeRect(float x, float y, float w, float h) {
        RECT r{};
        r.left   = (LONG)std::lround(x);
        r.top    = (LONG)std::lround(y);
        r.right  = (LONG)std::lround(x + w);
        r.bottom = (LONG)std::lround(y + h);
        return r;
    }

    // Card metrics follow the Prusa Connect tool card (padding 8, gap 6, 44px spool badge, 12/11px texts)
    struct Metrics {
        float titleH, cardPad, hdrH, barGap, badgeZone, badge, nameH, lineH, btnH, btnGap, cardH, syncH, sectionGap;
    };

    Metrics ComputeMetrics(bool compact) const {
        int dpi = GetWindowDpiSafe(m_hWorkspace);
        auto S = [&](float v) { return v * (float)dpi / 96.0f; };
        Metrics m{};
        m.titleH     = S(compact ? 30.0f : 36.0f);
        m.cardPad    = S(8.0f);
        m.hdrH       = S(16.0f);
        m.barGap     = S(compact ? 6.0f : 10.0f);
        m.badge      = S(compact ? 38.0f : 44.0f);
        m.badgeZone  = m.badge + S(10.0f);
        m.nameH      = S(34.0f);
        m.lineH      = S(16.0f);
        m.btnH       = S(compact ? 28.0f : 31.0f);
        m.btnGap     = S(6.0f);
        m.syncH      = S(compact ? 36.0f : 42.0f);
        m.sectionGap = S(compact ? 10.0f : 18.0f);
        m.cardH = m.cardPad + m.hdrH + S(6.0f) + S(2.0f) + m.barGap + m.badgeZone + S(compact ? 2.0f : 6.0f) +
                  m.nameH + 2.0f * m.lineH + S(compact ? 4.0f : 8.0f) + 2.0f * m.btnH + m.btnGap + m.cardPad;
        return m;
    }

    PageLayout BuildLayout(int width, bool compact) {
        PageLayout L{};
        L.compact = compact;
        int dpi = GetWindowDpiSafe(m_hWorkspace);
        auto S = [&](float v) { return v * (float)dpi / 96.0f; };
        Metrics m = ComputeMetrics(compact);

        L.nMachine = HasPrinter() ? std::clamp(m_machine.toolCount, 1, kToolCount) : 0;
        L.nSlicer  = std::clamp(m_activeSlicerExtruders, 0, kToolCount);
        int n = std::max({ L.nMachine, L.nSlicer, 1 });

        float padX  = S(24.0f);
        float gap   = S(8.0f);
        float avail = std::max(S(220.0f), (float)width - 2.0f * padX);

        // 8 cards in a row when they fit, otherwise 4 or 2 per row
        int rows = 1, cols = n;
        while (cols > 1 && rows < 4 && (avail - (float)(cols - 1) * gap) / (float)cols < S(118.0f)) {
            rows *= 2;
            cols = (n + rows - 1) / rows;
        }
        float cardW  = std::floor(std::clamp((avail - (float)(cols - 1) * gap) / (float)cols, S(90.0f), S(196.0f)));
        float gridW  = (float)cols * cardW + (float)(cols - 1) * gap;
        float blockW = std::max(gridW, std::min(avail, S(620.0f)));
        float x0     = std::floor(padX + std::max(0.0f, (avail - blockW) * 0.5f));
        L.x0     = x0;
        L.blockW = blockW;

        MeasureContext mc(m_hWorkspace);
        Gdiplus::Graphics& g = *mc.g;
        auto fTitle = MakeUiFont(S(compact ? 18.0f : 20.0f), 700);
        auto fSmall = MakeUiFont(S(12.0f), 400);

        float y = S(compact ? 12.0f : 20.0f);

        // --- Section 1 header: title [printer v] ............ (o) state [Refresh]
        int wRefresh = std::max((int)S(96.0f), MeasureConnectButtonWidth(m_hWorkspace, Tr(i18n::S_REFRESH), BtnKind::Outline, BtnIcon::Refresh));
        float btnY = y + (m.titleH - m.btnH) * 0.5f;
        L.btnRefresh = MakeRect(x0 + blockW - (float)wRefresh, btnY, (float)wRefresh, m.btnH);
        float rightEdge = x0 + blockW - (float)wRefresh - S(14.0f);

        Gdiplus::Color dot;
        std::wstring stateText = MachineStateLabel(dot);
        float stateW = stateText.empty() ? 0.0f : (S(8.0f) + S(6.0f) + MeasureTextWidth(g, *fSmall, stateText) + S(4.0f));
        L.showPrinterBtn = m_printers.size() > 1;
        float chevronW = L.showPrinterBtn ? (m.btnH + S(8.0f)) : 0.0f;
        float titleW = MeasureTextWidth(g, *fTitle, Title1()) + S(4.0f);
        L.showState = stateW > 0.0f && (rightEdge - stateW - x0 - chevronW) >= std::min(titleW, S(180.0f));
        if (L.showState) {
            L.state = Gdiplus::RectF(rightEdge - stateW, y, stateW, m.titleH);
            rightEdge -= stateW + S(12.0f);
        }
        float titleMax = std::max(S(60.0f), rightEdge - x0 - chevronW);
        float titleUse = std::min(titleW, titleMax);
        L.title1 = Gdiplus::RectF(x0, y, titleUse, m.titleH);
        if (L.showPrinterBtn) {
            L.btnPrinter = MakeRect(x0 + titleUse + S(8.0f), btnY, m.btnH, m.btnH);
        }
        y += m.titleH + S(compact ? 8.0f : 12.0f);

        auto placeCards = [&](CardGeom* cards, int count, Gdiplus::RectF& emptyBox) {
            if (count <= 0) {
                emptyBox = Gdiplus::RectF(x0, y, blockW, S(88.0f));
                y += S(88.0f);
                return;
            }
            int usedRows = (count + cols - 1) / cols;
            for (int i = 0; i < count; ++i) {
                int r = i / cols, c = i % cols;
                CardGeom& cg = cards[i];
                cg.rc = Gdiplus::RectF(x0 + (float)c * (cardW + gap), y + (float)r * (m.cardH + gap), cardW, m.cardH);
                float bx = cg.rc.X + m.cardPad;
                float bw = cardW - 2.0f * m.cardPad;
                float bBottom = cg.rc.GetBottom() - m.cardPad;
                cg.btnB = MakeRect(bx, bBottom - m.btnH, bw, m.btnH);
                cg.btnA = MakeRect(bx, bBottom - 2.0f * m.btnH - m.btnGap, bw, m.btnH);
            }
            y += (float)usedRows * (m.cardH + gap) - gap;
        };
        placeCards(L.machine, L.nMachine, L.empty1);

        // --- Sync buttons (auto-sized from their translated labels, stacked when too wide)
        y += m.sectionGap;
        int w1 = std::max((int)S(220.0f), MeasureConnectButtonWidth(m_hWorkspace, ToSlicerText(), BtnKind::Brand, BtnIcon::ArrowDown));
        int w2 = std::max((int)S(220.0f), MeasureConnectButtonWidth(m_hWorkspace, ToMachineText(), BtnKind::Solid, BtnIcon::ArrowUp));
        float syncGap = S(12.0f);
        if ((float)(w1 + w2) + syncGap <= blockW) {
            float sx = x0 + (blockW - ((float)(w1 + w2) + syncGap)) * 0.5f;
            L.btnToSlicer  = MakeRect(sx, y, (float)w1, m.syncH);
            L.btnToMachine = MakeRect(sx + (float)w1 + syncGap, y, (float)w2, m.syncH);
            y += m.syncH;
        } else {
            float sw = std::min(blockW, (float)std::max(w1, w2));
            float sx = x0 + (blockW - sw) * 0.5f;
            L.btnToSlicer  = MakeRect(sx, y, sw, m.syncH);
            L.btnToMachine = MakeRect(sx, y + m.syncH + S(8.0f), sw, m.syncH);
            y += 2.0f * m.syncH + S(8.0f);
        }
        y += S(6.0f);
        L.status = Gdiplus::RectF(x0, y, blockW, S(20.0f));
        y += S(20.0f) + S(compact ? 4.0f : 10.0f);

        // --- Section 2
        L.title2 = Gdiplus::RectF(x0, y, blockW, m.titleH);
        y += m.titleH + S(compact ? 8.0f : 12.0f);
        placeCards(L.slicer, L.nSlicer, L.empty2);
        y += S(compact ? 12.0f : 20.0f);

        L.contentH = y;
        return L;
    }

    int MaxScroll() const {
        if (!m_hWorkspace) return 0;
        RECT rc{};
        GetClientRect(m_hWorkspace, &rc);
        return std::max(0, (int)std::ceil(m_layout.contentH) - (int)rc.bottom);
    }

    void PlaceButton(HWND hBtn, const RECT& rcContent, bool visible) {
        if (!hBtn) return;
        if (!visible) {
            if (IsWindowVisible(hBtn)) ShowWindow(hBtn, SW_HIDE);
            return;
        }
        int x = rcContent.left;
        int y = rcContent.top - m_scrollY;
        int w = rcContent.right - rcContent.left;
        int h = rcContent.bottom - rcContent.top;
        RECT cur{};
        GetWindowRect(hBtn, &cur);
        MapWindowPoints(HWND_DESKTOP, m_hWorkspace, (LPPOINT)&cur, 2);
        bool same = (cur.left == x && cur.top == y && (cur.right - cur.left) == w && (cur.bottom - cur.top) == h);
        if (!same || !IsWindowVisible(hBtn)) {
            SetWindowPos(hBtn, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }

    void ApplyLayout() {
        m_scrollY = std::clamp(m_scrollY, 0, MaxScroll());
        const PageLayout& L = m_layout;
        PlaceButton(m_btnRefresh, L.btnRefresh, true);
        PlaceButton(m_btnPrinter, L.btnPrinter, L.showPrinterBtn);
        PlaceButton(m_btnToSlicer, L.btnToSlicer, true);
        PlaceButton(m_btnToMachine, L.btnToMachine, true);
        for (int i = 0; i < kToolCount; ++i) {
            PlaceButton(m_btnMachineA[i], L.machine[i].btnA, i < L.nMachine);
            PlaceButton(m_btnMachineB[i], L.machine[i].btnB, i < L.nMachine);
            PlaceButton(m_btnSlicerA[i],  L.slicer[i].btnA,  i < L.nSlicer);
            PlaceButton(m_btnSlicerB[i],  L.slicer[i].btnB,  i < L.nSlicer);
        }
    }

    void UpdateControls() {
        std::string st = MachineState();
        bool online   = MachineOnline();
        bool busy     = IsPrinterBusyState(st);
        bool idle     = online && IsPrinterIdleState(st);

        bool anyLoaded = false;
        for (int i = 0; i < m_layout.nMachine; ++i) {
            const MachineSlot& t = m_machine.tools[i];
            if (t.loaded) anyLoaded = true;
            bool assigned = !t.materialUuid.empty() || !t.materialName.empty();
            // Same guards as Connect: printer online, not printing nor changing filament,
            // and never for a material read from the spool NFC tag
            SetButtonState(m_btnMachineA[i], Tr(assigned ? i18n::S_CHANGE : i18n::S_ASSIGN),
                           online && !busy && !t.isFromNfc);
            bool canUnload = t.loaded && idle && !t.toolId.empty();
            SetButtonState(m_btnMachineB[i], Tr(i18n::S_UNLOAD), canUnload);
        }
        bool anySlicer = false;
        for (int i = 0; i < m_layout.nSlicer; ++i) {
            if (m_slicer[i].valid) anySlicer = true;
            SetButtonState(m_btnSlicerA[i], Tr(i18n::S_CHANGE), m_slicer[i].valid);
            SetButtonState(m_btnSlicerB[i], Tr(i18n::S_COLOR),  m_slicer[i].valid);
        }
        SetButtonState(m_btnToSlicer,  ToSlicerText(),  anyLoaded && anySlicer);
        SetButtonState(m_btnToMachine, ToMachineText(), online && anySlicer && !busy);
        SetButtonState(m_btnRefresh,   Tr(i18n::S_REFRESH), true);
    }

    void RelayoutAndRefresh() {
        if (!m_hWorkspace || !IsWindow(m_hWorkspace)) return;
        RECT rc{};
        GetClientRect(m_hWorkspace, &rc);
        if (rc.right < 40 || rc.bottom < 40) return;
        UpdatePalette();
        m_layout = BuildLayout(rc.right, false);
        if (m_layout.contentH > (float)rc.bottom) {
            m_layout = BuildLayout(rc.right, true);
        }
        ApplyLayout();
        UpdateControls();
        InvalidateRect(m_hWorkspace, nullptr, FALSE);
    }

    void SetLocalStatus(const std::wstring& text, bool ok) {
        m_statusLine = text;
        m_statusOk   = ok;
        if (m_hWorkspace) InvalidateRect(m_hWorkspace, nullptr, FALSE);
    }

    // ------------------------------------------------------------------------
    // Workspace painting
    // ------------------------------------------------------------------------
    CardContent MachineCardContent(int i) const {
        const MachineSlot& t = m_machine.tools[i];
        CardContent c;
        c.header = ToolLabel(i);
        c.nozzle = FormatNozzle(t.nozzleDiameter, t.highFlow);
        c.ring   = t.active;
        if (t.loaded) {
            c.hasColor = t.color.assigned;
            c.color    = t.color;
            c.abbr     = Utf8ToWide(!t.abbrev.empty() ? t.abbrev : t.material);
        }
        bool assigned = !t.materialUuid.empty() || !t.materialName.empty();
        if (assigned) {
            c.name = Utf8ToWide(!t.materialName.empty() ? t.materialName : t.abbrev);
            // Custom materials created by "Send to <printer>" are named "<name> #RRGGBB" in Connect
            // (the color makes the name unique); the swatch already shows the color
            std::wstring hexSuffix = L" " + t.color.ToHexW();
            if (t.color.assigned && c.name.size() > hexSuffix.size() &&
                _wcsicmp(c.name.c_str() + c.name.size() - hexSuffix.size(), hexSuffix.c_str()) == 0) {
                c.name.resize(c.name.size() - hexSuffix.size());
            }
            if (c.name.empty()) c.name = Tr(i18n::S_UNKNOWN_MATERIAL);
            c.lineA = Utf8ToWide(t.brandName);
        } else {
            c.name      = Tr(i18n::S_NO_MATERIAL);
            c.nameMuted = true;
        }
        if (t.loaded && t.hasWeight && t.nominalWeightG > 1.0) {
            c.hasBar = true;
            c.ratio  = (float)std::clamp(t.remainingWeightG / t.nominalWeightG, 0.0, 1.0);
            c.lineB  = std::to_wstring((int)std::lround(t.remainingWeightG)) + L" g";
        }
        return c;
    }

    CardContent SlicerCardContent(int i) const {
        const SlicerSlot& s = m_slicer[i];
        CardContent c;
        c.header = FilamentLabel(i);
        c.nozzle = m_slicerNozzle;
        if (!s.valid) {
            c.name      = Tr(i18n::S_INACTIVE);
            c.nameMuted = true;
            return c;
        }
        c.hasColor = true;
        c.color    = s.color;
        c.abbr     = Utf8ToWide(s.material);
        std::wstring full = Utf8ToWide(s.presetName);
        size_t at = full.find(L" @");
        if (at != std::wstring::npos) {
            c.name  = full.substr(0, at);
            c.lineA = full.substr(at + 2);
        } else {
            c.name = full;
        }
        c.lineB = s.color.ToHexW();
        return c;
    }

    void DrawNozzleIcon(Gdiplus::Graphics& g, float x, float y, float size) const {
        // Small nozzle pictogram in Prusa orange (heater block + tip)
        Gdiplus::SolidBrush br(m_pal.primary);
        float bw = size * 0.86f, bh = size * 0.46f;
        g.FillRectangle(&br, x + (size - bw) * 0.5f, y, bw, bh);
        Gdiplus::PointF tip[4] = {
            Gdiplus::PointF(x + size * 0.22f, y + bh + size * 0.08f),
            Gdiplus::PointF(x + size * 0.78f, y + bh + size * 0.08f),
            Gdiplus::PointF(x + size * 0.60f, y + size),
            Gdiplus::PointF(x + size * 0.40f, y + size)
        };
        g.FillPolygon(&br, tip, 4);
    }

    void DrawCard(Gdiplus::Graphics& g, const Gdiplus::RectF& rc, const CardContent& c, const Metrics& m, int dpi) const {
        auto S = [&](float v) { return v * (float)dpi / 96.0f; };
        FillStrokeRoundRect(g, Gdiplus::RectF(rc.X + 0.5f, rc.Y + 0.5f, rc.Width - 1.0f, rc.Height - 1.0f),
                            S(8.0f), m_pal.bg, m_pal.cardBorder, 1.0f);

        auto fSmall = MakeUiFont(S(12.0f), 400);
        auto fTiny  = MakeUiFont(S(11.0f), 400);
        auto fName  = MakeUiFont(S(12.0f), 500);
        Gdiplus::SolidBrush brText(m_pal.text);
        Gdiplus::SolidBrush brSub(m_pal.textSecondary);

        Gdiplus::StringFormat sfLeft, sfRight, sfCenter, sfWrap;
        sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfLeft.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfLeft.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        sfRight.SetAlignment(Gdiplus::StringAlignmentFar);
        sfRight.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfRight.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfCenter.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        sfWrap.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfWrap.SetLineAlignment(Gdiplus::StringAlignmentNear);
        sfWrap.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfWrap.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);

        float x = rc.X + m.cardPad;
        float w = rc.Width - 2.0f * m.cardPad;
        float y = rc.Y + m.cardPad;

        // Header: "Tool 1" ........ 0.4 [nozzle]
        float nozzleW = 0.0f;
        if (!c.nozzle.empty()) {
            float icon = S(11.0f);
            float tw = MeasureTextWidth(g, *fSmall, c.nozzle) + S(3.0f);
            nozzleW = tw + S(4.0f) + icon;
            DrawNozzleIcon(g, x + w - icon, y + (m.hdrH - icon) * 0.5f, icon);
            g.DrawString(c.nozzle.c_str(), (INT)c.nozzle.size(), fSmall.get(),
                         Gdiplus::RectF(x + w - nozzleW - S(2.0f), y, tw + S(2.0f), m.hdrH), &sfRight, &brSub);
        }
        g.DrawString(c.header.c_str(), (INT)c.header.size(), fSmall.get(),
                     Gdiplus::RectF(x - 1.0f, y, std::max(S(10.0f), w - nozzleW - S(4.0f)), m.hdrH), &sfLeft, &brSub);
        y += m.hdrH + S(6.0f);

        // Remaining filament: 2px track
        {
            Gdiplus::SolidBrush track(m_pal.borderSecondary);
            g.FillRectangle(&track, x, y, w, S(2.0f));
            if (c.hasBar && c.ratio > 0.0f) {
                Gdiplus::SolidBrush fill(c.ratio > 0.15f ? m_pal.primary : m_pal.danger);
                g.FillRectangle(&fill, x, y, std::max(S(2.0f), w * c.ratio), S(2.0f));
            }
        }
        y += S(2.0f) + m.barGap;

        // Spool badge: 44px disc in the filament color with the material type
        {
            float d  = m.badge;
            float cx = rc.X + rc.Width * 0.5f;
            float cy = y + m.badgeZone * 0.5f;
            Gdiplus::RectF disc(cx - d * 0.5f, cy - d * 0.5f, d, d);
            if (c.ring) {
                Gdiplus::Pen ring(Gdiplus::Color(150, m_pal.text.GetR(), m_pal.text.GetG(), m_pal.text.GetB()), S(2.0f));
                float rr = d * 0.5f + S(4.0f);
                g.DrawEllipse(&ring, cx - rr, cy - rr, 2.0f * rr, 2.0f * rr);
            }
            if (c.hasColor) {
                Gdiplus::SolidBrush br(Gdiplus::Color(255, c.color.r, c.color.g, c.color.b));
                g.FillEllipse(&br, disc);
                bool darkText = UseDarkTextOn(c.color);
                double lum = c.color.Luminance();
                if ((!m_pal.dark && lum > 225.0) || (m_pal.dark && lum < 45.0)) {
                    Gdiplus::Pen edge(m_pal.borderSecondary, 1.0f);
                    g.DrawEllipse(&edge, disc);
                }
                float fpx = c.abbr.size() <= 3 ? 13.0f : (c.abbr.size() == 4 ? 12.0f : (c.abbr.size() <= 6 ? 10.0f : 8.5f));
                auto fBadge = MakeUiFont(S(fpx) * (d / S(44.0f)), 700);
                Gdiplus::SolidBrush brBadge(darkText ? Gdiplus::Color(255, 0, 0, 0) : Gdiplus::Color(255, 255, 255, 255));
                g.DrawString(c.abbr.c_str(), (INT)c.abbr.size(), fBadge.get(),
                             Gdiplus::RectF(disc.X - S(2.0f), disc.Y, disc.Width + S(4.0f), disc.Height), &sfCenter, &brBadge);
            } else {
                Gdiplus::SolidBrush br(m_pal.hover);
                g.FillEllipse(&br, disc);
                Gdiplus::Pen edge(m_pal.borderSecondary, 1.0f);
                g.DrawEllipse(&edge, disc);
                std::wstring label = c.abbr.empty() ? std::wstring(L"–") : c.abbr;
                auto fBadge = MakeUiFont(S(label.size() <= 4 ? 12.0f : 10.0f), 700);
                g.DrawString(label.c_str(), (INT)label.size(), fBadge.get(), disc, &sfCenter, &brSub);
            }
        }
        y += m.badgeZone + S(m_layout.compact ? 2.0f : 6.0f);

        // Material name (2 lines max), brand, remaining weight
        g.DrawString(c.name.c_str(), (INT)c.name.size(), fName.get(),
                     Gdiplus::RectF(x - S(2.0f), y, w + S(4.0f), m.nameH), &sfWrap, c.nameMuted ? &brSub : &brText);
        y += m.nameH;
        if (!c.lineA.empty()) {
            g.DrawString(c.lineA.c_str(), (INT)c.lineA.size(), fTiny.get(),
                         Gdiplus::RectF(x - S(2.0f), y, w + S(4.0f), m.lineH), &sfCenter, &brSub);
        }
        y += m.lineH;
        if (!c.lineB.empty()) {
            g.DrawString(c.lineB.c_str(), (INT)c.lineB.size(), fTiny.get(),
                         Gdiplus::RectF(x - S(2.0f), y, w + S(4.0f), m.lineH), &sfCenter, &brSub);
        }
    }

    void PaintWorkspace(HDC hdcOut, int width, int height) {
        if (width < 4 || height < 4) return;
        HDC hdcMem = CreateCompatibleDC(hdcOut);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcOut, width, height);
        HGDIOBJ hOld = SelectObject(hdcMem, hBmp);

        int dpi = GetWindowDpiSafe(m_hWorkspace);
        auto S = [&](float v) { return v * (float)dpi / 96.0f; };
        const PageLayout& L = m_layout;
        Metrics m = ComputeMetrics(L.compact);
        {
            Gdiplus::Graphics g(hdcMem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
            g.Clear(m_pal.bg);
            g.TranslateTransform(0.0f, (float)-m_scrollY);

            auto fTitle = MakeUiFont(S(L.compact ? 18.0f : 20.0f), 700);
            auto fSmall = MakeUiFont(S(12.0f), 400);
            auto fBody  = MakeUiFont(S(14.0f), 400);
            Gdiplus::SolidBrush brText(m_pal.text);
            Gdiplus::SolidBrush brSub(m_pal.textSecondary);

            Gdiplus::StringFormat sfLeft, sfCenter;
            sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfLeft.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sfLeft.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
            sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfCenter.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

            // Section 1: filaments installed in the printer
            std::wstring t1 = Title1();
            g.DrawString(t1.c_str(), (INT)t1.size(), fTitle.get(),
                         Gdiplus::RectF(L.title1.X - S(1.0f), L.title1.Y, L.title1.Width + S(4.0f), L.title1.Height),
                         &sfLeft, &brText);

            if (L.showState) {
                Gdiplus::Color dot;
                std::wstring stateText = MachineStateLabel(dot);
                Gdiplus::SolidBrush brDot(dot);
                float dy = L.state.Y + (L.state.Height - S(8.0f)) * 0.5f;
                g.FillEllipse(&brDot, L.state.X, dy, S(8.0f), S(8.0f));
                g.DrawString(stateText.c_str(), (INT)stateText.size(), fSmall.get(),
                             Gdiplus::RectF(L.state.X + S(13.0f), L.state.Y, L.state.Width, L.state.Height),
                             &sfLeft, &brSub);
            }

            if (L.nMachine <= 0) {
                FillStrokeRoundRect(g, Gdiplus::RectF(L.empty1.X + 0.5f, L.empty1.Y + 0.5f, L.empty1.Width - 1.0f, L.empty1.Height - 1.0f),
                                    S(8.0f), m_pal.bg, m_pal.cardBorder, 1.0f);
                std::wstring hint = m_statusLine.empty() ? std::wstring(Tr(i18n::S_NO_PRINTER)) : m_statusLine;
                Gdiplus::RectF rcHint(L.empty1.X + S(16.0f), L.empty1.Y, L.empty1.Width - S(32.0f), L.empty1.Height);
                g.DrawString(hint.c_str(), (INT)hint.size(), fBody.get(), rcHint, &sfCenter, &brSub);
            }
            for (int i = 0; i < L.nMachine; ++i) {
                DrawCard(g, L.machine[i].rc, MachineCardContent(i), m, dpi);
            }

            // Status line under the sync buttons
            if (!m_statusLine.empty() && L.nMachine > 0) {
                Gdiplus::SolidBrush brStatus(m_statusOk ? m_pal.textSecondary : m_pal.danger);
                g.DrawString(m_statusLine.c_str(), (INT)m_statusLine.size(), fSmall.get(), L.status, &sfCenter, &brStatus);
            }

            // Section 2: filaments active in the slicer
            std::wstring t2 = Title2();
            g.DrawString(t2.c_str(), (INT)t2.size(), fTitle.get(),
                         Gdiplus::RectF(L.title2.X - S(1.0f), L.title2.Y, L.title2.Width, L.title2.Height),
                         &sfLeft, &brText);
            if (L.nSlicer <= 0) {
                FillStrokeRoundRect(g, Gdiplus::RectF(L.empty2.X + 0.5f, L.empty2.Y + 0.5f, L.empty2.Width - 1.0f, L.empty2.Height - 1.0f),
                                    S(8.0f), m_pal.bg, m_pal.cardBorder, 1.0f);
                std::wstring none = L"–";
                g.DrawString(none.c_str(), (INT)none.size(), fBody.get(), L.empty2, &sfCenter, &brSub);
            }
            for (int i = 0; i < L.nSlicer; ++i) {
                DrawCard(g, L.slicer[i].rc, SlicerCardContent(i), m, dpi);
            }

            // Thin scroll indicator when the page is taller than the window
            g.ResetTransform();
            int maxScroll = MaxScroll();
            if (maxScroll > 0) {
                float trackH = (float)height - S(8.0f);
                float thumbH = std::max(S(28.0f), trackH * (float)height / L.contentH);
                float thumbY = S(4.0f) + (trackH - thumbH) * ((float)m_scrollY / (float)maxScroll);
                FillStrokeRoundRect(g, Gdiplus::RectF((float)width - S(7.0f), thumbY, S(4.0f), thumbH), S(2.0f),
                                    m_pal.borderSecondary, Gdiplus::Color(0, 0, 0, 0), 0.0f);
            }
        }

        BitBlt(hdcOut, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOld);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
    }

    void ScrollTo(int newY) {
        newY = std::clamp(newY, 0, MaxScroll());
        if (newY == m_scrollY) return;
        m_scrollY = newY;
        ApplyLayout();
        InvalidateRect(m_hWorkspace, nullptr, FALSE);
    }

    // ------------------------------------------------------------------------
    // Actions
    // ------------------------------------------------------------------------
    POINT PopupAnchor(HWND hBtn) const {
        RECT rc{};
        GetWindowRect(hBtn, &rc);
        return POINT{ rc.left, rc.bottom + 2 };
    }

    void OpenPrinterMenu() {
        if (m_printers.size() < 2) return;
        HMENU hMenu = CreatePopupMenu();
        for (size_t i = 0; i < m_printers.size() && i < 32; ++i) {
            std::wstring lbl = Utf8ToWide(m_printers[i].name.empty() ? m_printers[i].model : m_printers[i].name);
            UINT fl = MF_STRING | ((int)i == m_selectedPrinterIdx ? MF_CHECKED : 0);
            AppendMenuW(hMenu, fl, IDM_PRINTER_SELECT_BASE + (UINT)i, lbl.c_str());
        }
        POINT pt = PopupAnchor(m_btnPrinter);
        UINT cmd = (UINT)TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        pt.x, pt.y, 0, m_hWorkspace, nullptr);
        DestroyMenu(hMenu);
        if (cmd >= IDM_PRINTER_SELECT_BASE && (cmd - IDM_PRINTER_SELECT_BASE) < m_printers.size()) {
            int idx = (int)(cmd - IDM_PRINTER_SELECT_BASE);
            m_userCfg.selectedUuid = m_printers[idx].uuid;
            m_userCfg.Save();
            m_engine.RequestRefresh(m_userCfg);
        }
    }

    // Slicer card "Change": compatible filament profile for a material type
    void OpenSlicerMaterialMenu(int idx) {
        if (idx < 0 || idx >= kToolCount || !m_slicer[idx].valid) return;
        HMENU hMenu = CreatePopupMenu();
        for (int k = 0; k < kBuddyMaterialCount; ++k) {
            UINT fl = MF_STRING | (m_slicer[idx].material == kBuddyMaterials[k].code ? MF_CHECKED : 0);
            AppendMenuW(hMenu, fl, IDM_SLICER_MATERIAL_BASE + (UINT)k, Utf8ToWide(kBuddyMaterials[k].code).c_str());
        }
        POINT pt = PopupAnchor(m_btnSlicerA[idx]);
        UINT cmd = (UINT)TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        pt.x, pt.y, 0, m_hWorkspace, nullptr);
        DestroyMenu(hMenu);
        if (cmd >= IDM_SLICER_MATERIAL_BASE && cmd < IDM_SLICER_MATERIAL_BASE + (UINT)kBuddyMaterialCount) {
            std::string code = kBuddyMaterials[cmd - IDM_SLICER_MATERIAL_BASE].code;
            SetSlicerSlotColorAndMaterial(idx, m_slicer[idx].color, code, true);
            RefreshSlicerSlotsFromHost();
            RelayoutAndRefresh();
        }
    }

    void PickSlicerColor(int idx) {
        if (idx < 0 || idx >= kToolCount || !m_slicer[idx].valid) return;
        CHOOSECOLORW cc{};
        cc.lStructSize  = sizeof(cc);
        cc.hwndOwner    = m_hMainWnd;
        cc.lpCustColors = g_customColors;
        cc.rgbResult    = m_slicer[idx].color.ToColorRef();
        cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
        g_autoChooseColorActive = false;
        if (Hook_ChooseColorW(&cc)) {
            SetSlicerSlotColorAndMaterial(idx, Rgb::FromColorRef(cc.rgbResult, true), "", false);
            RelayoutAndRefresh();
        }
    }

    // Machine card "Change" / "Assign": the Prusa Connect assignment dialog
    void OpenMachineMaterialDialog(int idx) {
        if (idx < 0 || idx >= kToolCount || !HasPrinter() || MaterialDialog::Current()) return;
        MaterialDialog dlg(m_hMainWnd, m_engine, m_pal,
                           Fmt(i18n::S_DLG_ASSIGN_TO, { ToolLabel(idx) }), m_machine.tools[idx]);
        MaterialDialogResult res = dlg.RunModal();

        if (res.kind == MaterialDialogResult::Catalogue) {
            m_engine.RequestAssignMaterial(m_userCfg, idx, res.item);
        } else if (res.kind == MaterialDialogResult::Custom) {
            // Custom material recorded in Prusa Connect (same as its warehouse "custom material")
            SlicerAssignSpec sp;
            sp.toolIdx = idx;
            sp.brand   = "Generic";
            sp.type    = res.customMaterial;
            sp.name    = res.customMaterial;
            sp.color   = res.customColor;
            sp.color.assigned = true;
            m_engine.RequestAssignFromSlicer(m_userCfg, { sp });
        }
        if (m_hWorkspace) SetFocus(m_hWorkspace);
    }

    void UnloadMachineTool(int idx) {
        if (idx < 0 || idx >= kToolCount || !HasPrinter()) return;
        std::wstring question = Fmt(i18n::S_CONFIRM_UNLOAD, { ToolLabel(idx) });
        if (MessageBoxW(m_hMainWnd, question.c_str(), PrinterDisplayName().c_str(),
                        MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) == IDOK) {
            m_engine.RequestUnload(m_userCfg, idx);
        }
    }

    // "Send to OpenSlicer": machine tool N -> slicer filament N (color + compatible profile)
    void ActionMachineToSlicer() {
        int updated = 0;
        int n = std::min(m_layout.nMachine, m_layout.nSlicer);
        for (int i = 0; i < n; ++i) {
            const MachineSlot& ms = m_machine.tools[i];
            if (!ms.loaded) continue;
            Rgb col = ms.color.assigned ? ms.color : GetBuddyMaterialDef(ms.material).fallbackColor;
            col.assigned = true;
            if (SetSlicerSlotColorAndMaterial(i, col, ms.material, true)) {
                ++updated;
            }
        }
        RefreshSlicerSlotsFromHost();
        RelayoutAndRefresh();
        SetLocalStatus(Fmt(i18n::S_ST_SENT_SLICER, { std::to_wstring(updated), kAppName }), updated > 0);
    }

    // "Send to <printer>": slicer filament N -> tool N of the printer in Prusa Connect.
    // Same record as "Assign" in the Connect "Tools" tab; no G-code is ever sent to the printer.
    void ActionSlicerToMachine() {
        RefreshSlicerSlotsFromHost();
        std::vector<SlicerAssignSpec> specs;
        int n = std::min(m_layout.nMachine, m_layout.nSlicer);
        for (int i = 0; i < n; ++i) {
            const SlicerSlot& s = m_slicer[i];
            if (!s.valid) continue;
            const MachineSlot& t = m_machine.tools[i];
            SlicerAssignSpec sp = SpecFromSlicerPreset(i, s.presetName, s.material, s.color);
            // Already identical in Connect: nothing to write
            bool assigned = !t.materialUuid.empty() || !t.materialName.empty();
            if (assigned && t.color.assigned && RedmeanDistance(t.color, s.color) <= 18.0 &&
                NormalizeBuddyMaterial(t.abbrev.empty() ? t.material : t.abbrev) == NormalizeBuddyMaterial(sp.type)) {
                continue;
            }
            specs.push_back(sp);
        }
        if (specs.empty()) {
            SetLocalStatus(Fmt(i18n::S_ST_SENT_MACHINE, { PrinterDisplayName() }), true);
            return;
        }
        m_engine.RequestAssignFromSlicer(m_userCfg, specs);
    }

    void HandleCommand(int id) {
        if (id == IDC_SYNC_REFRESH) {
            RefreshSlicerSlotsFromHost();
            m_engine.RequestRefresh(m_userCfg);
            RelayoutAndRefresh();
        } else if (id == IDC_SYNC_PRINTER) {
            OpenPrinterMenu();
        } else if (id == IDC_SYNC_TO_SLICER) {
            ActionMachineToSlicer();
        } else if (id == IDC_SYNC_TO_MACHINE) {
            ActionSlicerToMachine();
        } else if (id >= IDC_SYNC_MACHINE_A && id < IDC_SYNC_MACHINE_A + kToolCount) {
            OpenMachineMaterialDialog(id - IDC_SYNC_MACHINE_A);
        } else if (id >= IDC_SYNC_MACHINE_B && id < IDC_SYNC_MACHINE_B + kToolCount) {
            UnloadMachineTool(id - IDC_SYNC_MACHINE_B);
        } else if (id >= IDC_SYNC_SLICER_A && id < IDC_SYNC_SLICER_A + kToolCount) {
            OpenSlicerMaterialMenu(id - IDC_SYNC_SLICER_A);
        } else if (id >= IDC_SYNC_SLICER_B && id < IDC_SYNC_SLICER_B + kToolCount) {
            PickSlicerColor(id - IDC_SYNC_SLICER_B);
        }
    }

    // ------------------------------------------------------------------------
    // Workspace Window Procedure
    // ------------------------------------------------------------------------
    static LRESULT CALLBACK WorkspaceWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        SyncTabManager* self = reinterpret_cast<SyncTabManager*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<SyncTabManager*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
            return TRUE;
        }
        if (!self) return DefWindowProcW(hWnd, msg, wParam, lParam);

        switch (msg) {
        case WM_CREATE:
            self->m_hWorkspace = hWnd;
            self->UpdatePalette();
            self->CreateWorkspaceControls(hWnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            self->RelayoutAndRefresh();
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hWnd);
            return 0;

        case WM_MOUSEWHEEL: {
            int dpi = GetWindowDpiSafe(hWnd);
            int step = MulDiv(72, dpi, 96);
            self->ScrollTo(self->m_scrollY - (GET_WHEEL_DELTA_WPARAM(wParam) * step) / WHEEL_DELTA);
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && lParam != 0) {
                self->HandleCommand(LOWORD(wParam));
                return 0;
            }
            break;

        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (dis && dis->CtlType == ODT_BUTTON) {
                DrawConnectButton(dis, self->m_pal);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORBTN:
            return (LRESULT)(self->m_brBg ? self->m_brBg : GetStockObject(WHITE_BRUSH));

        case WM_OPENPS_SYNC_EVENT: {
            if (wParam == SYNC_EVT_MATERIALS_DONE) {
                if (MaterialDialog* dlg = MaterialDialog::Current()) {
                    dlg->OnMaterialsEvent((int)lParam);
                }
                return 0;
            }
            self->m_engine.GetStateCopy(self->m_printers, self->m_selectedPrinterIdx,
                                        self->m_statusLine, self->m_statusOk);
            if (wParam == SYNC_EVT_REFRESH_DONE) {
                if (self->m_selectedPrinterIdx >= 0 && self->m_selectedPrinterIdx < (int)self->m_printers.size()) {
                    self->m_machine = self->m_printers[self->m_selectedPrinterIdx];
                } else {
                    self->m_machine = MachineSnapshot{};
                }
                self->RelayoutAndRefresh();
            } else if (wParam == SYNC_EVT_ACTION_DONE) {
                self->UpdateControls();
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc{};
            GetClientRect(hWnd, &rc);
            self->PaintWorkspace(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_NCDESTROY:
            if (self->m_brBg) {
                DeleteObject(self->m_brBg);
                self->m_brBg      = nullptr;
                self->m_brBgColor = CLR_INVALID;
            }
            self->m_btnRefresh = self->m_btnPrinter = self->m_btnToSlicer = self->m_btnToMachine = nullptr;
            self->m_btnMachineA.fill(nullptr);
            self->m_btnMachineB.fill(nullptr);
            self->m_btnSlicerA.fill(nullptr);
            self->m_btnSlicerB.fill(nullptr);
            break;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
};

// Public C-style entry points for OpenPS_launcher.cpp
inline bool SyncTab_IsActive() {
    return SyncTabManager::Instance().IsActive();
}

inline void SyncTab_EnsureAndSync(HWND hMainWnd) {
    SyncTabManager::Instance().AttachAndSync(hMainWnd);
}

inline void SyncTab_OnLanguageChanged() {
    SyncTabManager::Instance().OnLanguageChanged();
}

inline void SyncTab_Shutdown() {
    SyncTabManager::Instance().Shutdown();
}

} // namespace openps::sync

#endif // OPENPS_SYNC_INDX_TAB_HPP
