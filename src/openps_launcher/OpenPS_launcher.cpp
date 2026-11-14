// SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// OpenPS_launcher.cpp - OpenSlicer launcher (OpenPS.exe)
//
// OpenSlicer is PrusaSlicer 2.9.7 with a few additions. Instead of rebuilding
// PrusaSlicer, OpenPS.exe loads the official PrusaSlicer.dll, calls its
// slic3r_main() entry point (exactly like prusa-slicer.exe does) and adds:
//   * branding (window titles, splash screen, About dialog, accent colors),
//   * a 3D ViewCube over the plater and a "Set origin" menu (camera pivot),
//   * the "Sync" tab (SyncIndxTab.hpp) to synchronize filaments with Prusa Connect.
//
// Integration techniques, all confined to this process:
//   * IAT hooks on PrusaSlicer.dll imports (CreateWindowExW, SetWindowTextW,
//     TrackPopupMenu(Ex), ChooseColorW),
//   * window subclassing of the main frame and of push buttons,
//   * direct calls into PrusaSlicer.dll and a few in-memory patches, using the
//     addresses of PrusaSlicerDll.hpp. They are enabled only when the loaded DLL
//     is the exact supported build.
// See doc/openslicer/ARCHITECTURE.md.
// ============================================================================
#define _WIN32_WINNT 0x0601
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <algorithm>
#include "PrusaSlicerDll.hpp"
#include "SyncIndxTab.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")

// Command IDs of the "Set origin" submenu
#define ID_ORIGIN_ACTIVE_OBJECT  59901
#define ID_ORIGIN_BED_CENTER     59902

#define WM_OPENPS_SELECT_VIEW    (WM_USER + 777)
#define WM_OPENPS_SET_ORIGIN     (WM_USER + 778)
#define TIMER_MAIN_SYNC          777

#define SUBCLASS_ID_MAIN         100
#define SUBCLASS_ID_BUTTON       202

// ViewCube layout size at 96 DPI; the window is scaled with the DPI of the main window
#define VIEWCUBE_WIDTH           148
#define VIEWCUBE_HEIGHT          158

static const wchar_t* const APP_BRAND_NAME = L"OpenSlicer";
static const wchar_t* const APP_VERSION    = L"0.2";

using openps::sync::i18n::Tr;
using openps::sync::i18n::Fmt;
using openps::sync::i18n::TrApp;
namespace i18n = openps::sync::i18n;
namespace dll  = openps::dll;

// "OpenSlicer 0.2 - " : start of every main window title set by OpenSlicer
static std::wstring AppTitleHead()
{
    return std::wstring(APP_BRAND_NAME) + L" " + APP_VERSION + L" - ";
}

// "OpenSlicer 0.2 - based on PrusaSlicer 2.9.7" in the current application language
static std::wstring AppFullTitle()
{
    return AppTitleHead() + Fmt(i18n::S_TITLE_BASED_ON, { dll::kSupportedVersion });
}

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

extern "C" {
    typedef int (__stdcall *Slic3rMainFunc)(int argc, wchar_t **argv);
}

// Global state
static HWND g_hMainWnd = nullptr;
static HWND g_hViewCubeWnd = nullptr;
static HMODULE g_hPrusaDll = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
static bool g_viewCubeClassRegistered = false;
static bool g_inSyncViewCube = false;
static unsigned int g_syncTickCounter = 0;
static float g_cubeScale = 1.0f; // DPI scale of the ViewCube (1.0 at 96 DPI)

static int CubePx(int v) { return (int)lroundf((float)v * g_cubeScale); }

// Camera origin mode:
// - true  => "Au centre de l'objet actif" (locks & follows active 3D object center)
// - false => "Au centre du plateau" (default / centered on active build plate)
static bool g_originLockActive = false;
static double g_lockedOriginTarget[3] = { 0.0, 0.0, 0.0 };

// Forward declarations of original Win32 functions
typedef HWND (WINAPI *CreateWindowExW_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL (WINAPI *SetWindowTextW_t)(HWND, LPCWSTR);
typedef BOOL (WINAPI *TrackPopupMenu_t)(HMENU, UINT, int, int, int, HWND, const RECT*);
typedef BOOL (WINAPI *TrackPopupMenuEx_t)(HMENU, UINT, int, int, HWND, LPTPMPARAMS);

static CreateWindowExW_t  OriginalCreateWindowExW  = nullptr;
static SetWindowTextW_t   OriginalSetWindowTextW   = nullptr;
static TrackPopupMenu_t   OriginalTrackPopupMenu   = nullptr;
static TrackPopupMenuEx_t OriginalTrackPopupMenuEx = nullptr;

// -------------------------------------------------------------------------------------------------
// Direct calls into PrusaSlicer.dll (addresses and layouts: PrusaSlicerDll.hpp).
// dll::At() returns nullptr when the DLL is not the supported build, which disables these features.
// -------------------------------------------------------------------------------------------------
typedef void* (__fastcall *FnWxGetApp)();
typedef void* (__fastcall *FnGuiAppPlater)(void* app);
typedef void  (__fastcall *FnPlaterSelectView)(void* plater, const void* std_string_dir);
typedef void* (__fastcall *FnPlaterGetCamera)(void* plater);
typedef const void* (__fastcall *FnSelectionGetBoundingBox)(void* selection);
typedef void  (__fastcall *FnCanvasZoomToBed)(void* canvas3D);
typedef double* (__fastcall *FnGetBedTranslation)(void* multipleBeds, double outVec2d[2], int bedIdx);
typedef void  (__fastcall *FnCameraSetTarget)(void* camera, const double* target_vec3d);

// MSVC x64 Release std::string layout (Small String Optimization for length <= 15)
struct MsvcSmallString {
    char buf[16];
    size_t size;
    size_t capacity;

    MsvcSmallString(const char* s) {
        memset(buf, 0, sizeof(buf));
        size = strlen(s);
        if (size > 15) size = 15;
        memcpy(buf, s, size);
        buf[size] = '\0';
        capacity = 15;
    }
};

// Slic3r::BoundingBoxf3 layout (min Vec3d at +0x00, max Vec3d at +0x18, bool defined at +0x30)
struct Slic3rBoundingBoxf3 {
    double min_pt[3];
    double max_pt[3];
    bool defined;
    unsigned char pad[7];
};

static void* GetPlaterPtr()
{
    FnWxGetApp fnGetApp = (FnWxGetApp)dll::At(dll::kRvaWxGetApp);
    FnGuiAppPlater fnPlater = (FnGuiAppPlater)dll::At(dll::kRvaGuiAppPlater);
    if (!fnGetApp || !fnPlater) return nullptr;
    __try {
        void* app = fnGetApp();
        if (!app) return nullptr;
        return fnPlater(app);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static void* GetCameraPtr()
{
    void* plater = GetPlaterPtr();
    FnPlaterGetCamera fnGetCamera = (FnPlaterGetCamera)dll::At(dll::kRvaPlaterGetCamera);
    if (!plater || !fnGetCamera) return nullptr;
    __try {
        return fnGetCamera(plater);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static void* GetActiveCanvas3DPtr()
{
    void* plater = GetPlaterPtr();
    if (!plater) return nullptr;
    __try {
        void* priv = *(void**)((BYTE*)plater + dll::kOffPlaterPriv);
        if (!priv) return nullptr;
        void* view3D = *(void**)((BYTE*)priv + dll::kOffPrivView3D);
        if (!view3D) return nullptr;
        void* canvas3D = *(void**)((BYTE*)view3D + dll::kOffView3DCanvas);
        return canvas3D;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// Returns true if at least one object/volume is currently selected in the 3D scene
static bool HasActive3DSelection()
{
    void* canvas3D = GetActiveCanvas3DPtr();
    if (!canvas3D) return false;
    __try {
        // Selection::EType: Invalid=0, Empty=1, WipeTower=2, SingleModifier=3, ...
        BYTE selType = *((BYTE*)canvas3D + dll::kOffCanvasSelType);
        return (selType > 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Computes the exact 3D bounding-box center of the currently selected object(s) in the 3D scene
static bool GetSelectedObjectCenter3D(double outCenter[3])
{
    void* canvas3D = GetActiveCanvas3DPtr();
    FnSelectionGetBoundingBox fnGetSelBox = (FnSelectionGetBoundingBox)dll::At(dll::kRvaSelectionGetBoundingBox);
    if (!canvas3D || !fnGetSelBox) return false;

    __try {
        if (*((BYTE*)canvas3D + dll::kOffCanvasSelType) <= 1) {
            return false;
        }

        void* pSelection = (BYTE*)canvas3D + dll::kOffCanvasSelection;
        const Slic3rBoundingBoxf3* bbox = (const Slic3rBoundingBoxf3*)fnGetSelBox(pSelection);
        if (!bbox || !bbox->defined) return false;

        double cx = 0.5 * (bbox->min_pt[0] + bbox->max_pt[0]);
        double cy = 0.5 * (bbox->min_pt[1] + bbox->max_pt[1]);
        double cz = 0.5 * (bbox->min_pt[2] + bbox->max_pt[2]);

        if (!(cx > -1e6 && cx < 1e6 && cy > -1e6 && cy < 1e6 && cz > -1e6 && cz < 1e6)) {
            return false;
        }

        outCenter[0] = cx;
        outCenter[1] = cy;
        outCenter[2] = cz;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Computes the exact 3D geometric center of the active build plate (Bed3D + active multi-bed translation)
static bool GetActiveBedCenter3D(double outCenter[3])
{
    void* canvas3D = GetActiveCanvas3DPtr();
    FnGetBedTranslation fnGetTrans = (FnGetBedTranslation)dll::At(dll::kRvaGetBedTranslation);
    const int* pActiveBed = (const int*)dll::At(dll::kRvaActiveBedIndex);
    void* multipleBeds = dll::At(dll::kRvaMultipleBeds);
    if (!canvas3D || !fnGetTrans || !pActiveBed || !multipleBeds) return false;

    __try {
        // Bed3D build volume bounding box (layout read from GLCanvas3D::zoom_to_bed)
        void* bed = *(void**)((BYTE*)canvas3D + dll::kOffCanvasBed);
        if (!bed) return false;

        const BYTE defined = *((const BYTE*)bed + dll::kOffBedBoxDefined);
        if (!defined) return false;

        const double minX = *(const double*)((const BYTE*)bed + dll::kOffBedBoxMinX);
        const double minY = *(const double*)((const BYTE*)bed + dll::kOffBedBoxMinY);
        const double maxX = *(const double*)((const BYTE*)bed + dll::kOffBedBoxMaxX);
        const double maxY = *(const double*)((const BYTE*)bed + dll::kOffBedBoxMaxY);

        // Offset of the active bed when several beds are used
        double trans[2] = { 0.0, 0.0 };
        fnGetTrans(multipleBeds, trans, *pActiveBed);

        double cx = 0.5 * (minX + maxX) + trans[0];
        double cy = 0.5 * (minY + maxY) + trans[1];

        if (!(cx > -1e6 && cx < 1e6 && cy > -1e6 && cy < 1e6)) {
            return false;
        }

        outCenter[0] = cx;
        outCenter[1] = cy;
        outCenter[2] = 0.0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool ReadCameraRotationMatrix(double outRot[9])
{
    void* cam = GetCameraPtr();
    if (!cam) return false;
    __try {
        // Camera::m_view_matrix is a column-major 4x4 Transform3d (16 doubles)
        const double* m = (const double*)((const BYTE*)cam + dll::kOffCameraViewMatrix);
        double norm = m[0]*m[0] + m[1]*m[1] + m[2]*m[2];
        if (norm < 0.1 || norm > 10.0) return false;
        outRot[0] = m[0]; outRot[1] = m[4]; outRot[2] = m[8];
        outRot[3] = m[1]; outRot[4] = m[5]; outRot[5] = m[9];
        outRot[6] = m[2]; outRot[7] = m[6]; outRot[8] = m[10];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// -------------------------------------------------------------------------------------------------
// In-memory patches of PrusaSlicer.dll (only applied to the supported build, see wWinMain):
// 1. Splash screen, About dialog and Help/Exit menu items show "OpenSlicer" and "0.2.0". The engine
//    version (2.9.7) used for preset and project compatibility is not touched.
// 2. Splash screen: dark left panel (#181A20) and red loading text (#E02424).
// 3. Disables the startup "new version available" check (GUI_App::app_version_check).
// 4. wxWidgets and ImGui orange accents become the OpenSlicer red (#E02424).
// Every code patch first checks the original instruction bytes and is skipped if they differ.
// -------------------------------------------------------------------------------------------------
static void PatchRgbTriplet(BYTE* instrB, BYTE r, BYTE g, BYTE b)
{
    // Pattern: 41 B1 <B> 41 B0 <G> B2 <R> (8 bytes)
    if (instrB[0] == 0x41 && instrB[1] == 0xB1 &&
        instrB[3] == 0x41 && instrB[4] == 0xB0 &&
        instrB[6] == 0xB2) {
        instrB[2] = b;
        instrB[5] = g;
        instrB[7] = r;
    }
}

static void PatchRipLeaDisp(BYTE* base, DWORD instrRva, DWORD targetRva)
{
    BYTE* p = base + instrRva;
    if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x8D) {
        int disp = (int)(targetRva - (instrRva + 7));
        memcpy(p + 3, &disp, 4);
    }
}

static void PatchBrandingAndModernTheme(HMODULE hDll)
{
    if (!hDll) return;
    BYTE* base = (BYTE*)hDll;

    // 1. Write "OpenSlicer" and "0.2.0" into the dedicated branding slot at 0x48B45D8,
    //    and trim SplashScreen::ConstantText::init's 7 trailing newlines at 0x48B4600 ("\n\n\n\n\n\n\n")
    //    to "\n" so credits sit cleanly at the bottom without overlapping "Version 0.2.0" or the loading status!
    BYTE* strSlot = base + 0x48B45D8;
    DWORD oldProt = 0;
    if (VirtualProtect(strSlot, 0x30, PAGE_READWRITE, &oldProt)) {
        memset(strSlot, 0, 0x20);
        memcpy(strSlot, "OpenSlicer", 11);
        memcpy(strSlot + 0x18, "0.2.0", 6);
        if (strSlot[0x28] == '\n' && strSlot[0x29] == '\n') {
            strSlot[0x28] = '\n';
            strSlot[0x29] = '\0';
        }
        VirtualProtect(strSlot, 0x30, oldProt, &oldProt);
    }

    // 2. Patch SplashScreen::MakeBitmap (RVA 0x16FD3A0) left 400x600 panel background
    //    from RGB(51,51,51) to seamless Anthracite-Black RGB(24, 26, 32) (#181A20)
    //    and SplashScreen::SetText (RVA 0x16FDCA8) loading text to Crimson Red RGB(224, 36, 36) (#E02424)
    if (VirtualProtect(base + 0x16FD390, 0x950, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE* mkBmp = base + 0x16FD3A0;
        // 66 c7 00 33 33 c6 40 02 33 -> mov word ptr [rax], 0x1A18 (R=24, G=26); mov byte ptr [rax+2], 0x20 (B=32)
        if (mkBmp[0] == 0x66 && mkBmp[1] == 0xC7 && mkBmp[2] == 0x00 && mkBmp[5] == 0xC6) {
            mkBmp[3] = 24; // R
            mkBmp[4] = 26; // G
            mkBmp[8] = 32; // B
        }
        PatchRgbTriplet(base + 0x16FDCA8, 224, 36, 36);
        VirtualProtect(base + 0x16FD390, 0x950, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), base + 0x16FD390, 0x950);
    }

    // 3. Silence automatic PrusaSlicer 3.0-alpha version update nag (GUI_App::app_version_check at RVA 0x1702340)
    if (VirtualProtect(base + 0x1702340, 16, PAGE_EXECUTE_READWRITE, &oldProt)) {
        BYTE* fnVerCheck = base + 0x1702340;
        if (fnVerCheck[0] == 0x48 && fnVerCheck[1] == 0x89 && fnVerCheck[2] == 0x5C) {
            fnVerCheck[0] = 0xC3; // ret
        }
        VirtualProtect(base + 0x1702340, 16, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), base + 0x1702340, 16);
    }

    // 4. Patch SplashScreen::ConstantText::init & GUI_App UI colors (0x170BA00..0x170EC00)
    if (VirtualProtect(base + 0x170BA00, 0x3200, PAGE_EXECUTE_READWRITE, &oldProt)) {
        // SplashScreen title & version ("OpenSlicer" & "0.2.0")
        PatchRipLeaDisp(base, 0x170D53E, 0x48B45D8);
        BYTE* verMov4 = base + 0x170D579;
        if (verMov4[0] == 0x8B && verMov4[1] == 0x05) {
            int dispVer4 = (int)(0x48B45F0 - (0x170D579 + 6));
            memcpy(verMov4 + 2, &dispVer4, 4);
        }
        BYTE* verMov1 = base + 0x170D583;
        if (verMov1[0] == 0x0F && verMov1[1] == 0xB6 && verMov1[2] == 0x05) {
            int dispVer1 = (int)(0x48B45F4 - (0x170D583 + 7));
            memcpy(verMov1 + 3, &dispVer1, 4);
        }

        // GUI_App::get_label_default_clr_modified() -> Crimson Red (Dark: #F44336, Light: #E02424)
        PatchRgbTriplet(base + 0x170BAF1, 244, 67, 54);
        PatchRgbTriplet(base + 0x170BB45, 224, 36, 36);

        // GUI_App::init_ui_colours() -> Crimson Red & Charcoal-Crimson highlights
        PatchRgbTriplet(base + 0x170E2D0, 244, 67, 54); // m_color_label_modified (dark)
        PatchRgbTriplet(base + 0x170E330, 224, 36, 36); // m_color_label_modified (light)
        PatchRgbTriplet(base + 0x170E90E, 244, 67, 54); // m_color_hovered_btn_label (dark)
        PatchRgbTriplet(base + 0x170E945, 224, 36, 36); // m_color_hovered_btn_label (light)
        PatchRgbTriplet(base + 0x170E9FE, 248, 113, 113); // m_color_default_btn_label (dark)

        // m_color_default_btn_label (light) at 0x170EA36 uses `45 33 c9` (xor r9d,r9d for B=0): replace with `41 b1 1c` (B=28, G=28, R=185 = #B91C1C)
        BYTE* defLight = base + 0x170EA36;
        if (defLight[0] == 0x45 && defLight[1] == 0x33 && defLight[2] == 0xC9 &&
            defLight[3] == 0x41 && defLight[4] == 0xB0 && defLight[6] == 0xB2) {
            defLight[0] = 0x41; defLight[1] = 0xB1; defLight[2] = 28;
            defLight[5] = 28;
            defLight[7] = 185;
        }

        // m_color_selected_btn_bg (hovered/selected ScalableButton icon background)
        PatchRgbTriplet(base + 0x170EAFF, 118, 30, 34);   // Dark mode: rich crimson-charcoal
        PatchRgbTriplet(base + 0x170EB49, 246, 220, 220); // Light mode: crisp soft-crimson highlight

        VirtualProtect(base + 0x170BA00, 0x3200, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), base + 0x170BA00, 0x3200);
    }

    // 5. Help > About and File > Exit/Quit use the product name slot (Help > Website keeps PrusaSlicer)
    if (VirtualProtect(base + 0x186FA00, 0x9B00, PAGE_EXECUTE_READWRITE, &oldProt)) {
        PatchRipLeaDisp(base, 0x186FA9D, 0x48B45D8); // Help -> About OpenSlicer
        PatchRipLeaDisp(base, 0x18764A0, 0x48B45D8); // File -> Exit OpenSlicer
        PatchRipLeaDisp(base, 0x187948E, 0x48B45D8); // File -> Quit OpenSlicer
        VirtualProtect(base + 0x186FA00, 0x9B00, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), base + 0x186FA00, 0x9B00);
    }

    // 6. About dialog: window title, header and version
    if (VirtualProtect(base + 0x1C92300, 0xA00, PAGE_EXECUTE_READWRITE, &oldProt)) {
        PatchRipLeaDisp(base, 0x1C923D4, 0x48B45D8); // AboutDialog caption
        PatchRipLeaDisp(base, 0x1C92AAA, 0x48B45D8); // AboutDialog big header label
        BYTE* abVer4 = base + 0x1C92C32;
        if (abVer4[0] == 0x8B && abVer4[1] == 0x05) {
            int disp = (int)(0x48B45F0 - (0x1C92C32 + 6));
            memcpy(abVer4 + 2, &disp, 4);
        }
        BYTE* abVer1 = base + 0x1C92C3B;
        if (abVer1[0] == 0x0F && abVer1[1] == 0xB6 && abVer1[2] == 0x05) {
            int disp = (int)(0x48B45F4 - (0x1C92C3B + 7));
            memcpy(abVer1 + 3, &disp, 4);
        }
        VirtualProtect(base + 0x1C92300, 0xA00, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), base + 0x1C92300, 0xA00);
    }

    // 7. Patch all 141 ImGui COL_ORANGE_DARK & COL_ORANGE_LIGHT tables in .data (0x0564E000..0x0582E000)
    //    so all 3D viewport overlays, gizmo highlights, notification banners & links use OpenPS Crimson Red (#E02424)!
    BYTE* dataSec = base + 0x0564E000;
    const size_t dataLen = 0x001E0000;
    if (VirtualProtect(dataSec, dataLen, PAGE_READWRITE, &oldProt)) {
        const float origDark[4]  = { 0.67f,  0.36f,  0.19f,  1.0f };
        const float origLight[4] = { 0.923f, 0.504f, 0.264f, 1.0f };
        const float origBg[4]    = { 0.13f,  0.13f,  0.13f,  0.8f };

        const float newDark[4]   = { 0.72f,  0.11f,  0.11f,  1.0f }; // Deep Crimson (#B81C1C)
        const float newLight[4]  = { 0.878f, 0.141f, 0.141f, 1.0f }; // OpenPS Crimson Red (#E02424)
        const float newBg[4]     = { 0.11f,  0.122f, 0.149f, 0.92f }; // Modern Anthracite overlay (#1C1F26)

        for (size_t off = 0; off + 48 <= dataLen; off += 4) {
            if (memcmp(dataSec + off, origDark, 16) == 0 &&
                memcmp(dataSec + off + 16, origLight, 16) == 0) {
                memcpy(dataSec + off, newDark, 16);
                memcpy(dataSec + off + 16, newLight, 16);
                if (memcmp(dataSec + off + 32, origBg, 16) == 0) {
                    memcpy(dataSec + off + 32, newBg, 16);
                }
            }
        }
        VirtualProtect(dataSec, dataLen, oldProt, &oldProt);
    }
}

static inline bool IsValidStr(LPCWSTR p)
{
    return p != nullptr && ((ULONG_PTR)p > 0xFFFF) && !IsBadReadPtr(p, sizeof(wchar_t));
}

// Strictly identifies the true MainFrame and NEVER matches the SplashScreen or Popup Dialogs
static bool IsMainApplicationFrame(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd)) return false;
    if (GetParent(hWnd) != nullptr) return false;

    wchar_t clsName[128] = { 0 };
    GetClassNameW(hWnd, clsName, 127);
    if (wcscmp(clsName, L"wxWindowNR") != 0) return false;

    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if (style & WS_POPUP) return false;

    const LONG_PTR reqStyles = WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    return ((style & reqStyles) == reqStyles);
}

// Dynamically finds the active, visible 3D OpenGL canvas window (class "wxGLCanvas")
static HWND Find3DCanvasWindow(HWND hParent)
{
    if (!hParent || !IsWindow(hParent)) return nullptr;

    struct CanvasFinder {
        HWND hBest = nullptr;
        int maxArea = 0;
    } finder;

    EnumChildWindows(hParent, [](HWND hChild, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hChild)) return TRUE;

        wchar_t clsName[128] = { 0 };
        GetClassNameW(hChild, clsName, 127);
        if (wcscmp(clsName, L"wxGLCanvas") == 0) {
            RECT rc;
            GetClientRect(hChild, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            int area = w * h;
            if (area > ((CanvasFinder*)lParam)->maxArea && w > 150 && h > 150) {
                ((CanvasFinder*)lParam)->maxArea = area;
                ((CanvasFinder*)lParam)->hBest = hChild;
            }
        }
        return TRUE;
    }, (LPARAM)&finder);

    return finder.hBest;
}

static void Refresh3DViewport()
{
    void* canvas3D = GetActiveCanvas3DPtr();
    if (canvas3D) {
        __try {
            *((BYTE*)canvas3D + dll::kOffCanvasDirty) = 1; // GLCanvas3D::m_dirty = true
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    HWND hCanvas = Find3DCanvasWindow(g_hMainWnd);
    if (hCanvas && IsWindow(hCanvas)) {
        InvalidateRect(hCanvas, nullptr, FALSE);
        UpdateWindow(hCanvas);
    }
}

static bool ApplyCameraTarget3D(const double target[3])
{
    void* camera = GetCameraPtr();
    FnCameraSetTarget fnSetTarget = (FnCameraSetTarget)dll::At(dll::kRvaCameraSetTarget);
    if (!camera || !fnSetTarget) return false;
    __try {
        fnSetTarget(camera, target);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Execute camera view selection directly on the main GUI thread
static void ExecuteSelectViewOnMainThread(int viewCode)
{
    void* plater = GetPlaterPtr();
    FnPlaterSelectView fnSelectView = (FnPlaterSelectView)dll::At(dll::kRvaPlaterSelectView);
    if (!plater || !fnSelectView) return;

    const char* dirStr = "iso";
    switch (viewCode) {
    case 0: dirStr = "iso";    break;
    case 1: dirStr = "top";    break;
    case 2: dirStr = "bottom"; break;
    case 3: dirStr = "front";  break;
    case 4: dirStr = "rear";   break;
    case 5: dirStr = "left";   break;
    case 6: dirStr = "right";  break;
    default: return;
    }

    MsvcSmallString ssoDir(dirStr);
    __try {
        fnSelectView(plater, &ssoDir);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    // Preserve active origin target (object center if locked, or bed center if unlocked)
    if (g_originLockActive && HasActive3DSelection()) {
        double selCenter[3];
        if (GetSelectedObjectCenter3D(selCenter)) {
            memcpy(g_lockedOriginTarget, selCenter, sizeof(selCenter));
            ApplyCameraTarget3D(g_lockedOriginTarget);
        }
    }

    Refresh3DViewport();
}

// "Set origin" > "Center of active object": the camera pivots around the selection and follows it
static void ExecuteOriginActiveObjectOnMainThread()
{
    double selCenter[3] = { 0.0, 0.0, 0.0 };
    if (GetSelectedObjectCenter3D(selCenter)) {
        g_originLockActive = true;
        memcpy(g_lockedOriginTarget, selCenter, sizeof(selCenter));
        ApplyCameraTarget3D(g_lockedOriginTarget);
        Refresh3DViewport();
    }
}

// "Set origin" > "Center of build plate"
static void ExecuteOriginBedCenterOnMainThread()
{
    g_originLockActive = false;

    double bedCenter[3] = { 0.0, 0.0, 0.0 };
    if (GetActiveBedCenter3D(bedCenter)) {
        ApplyCameraTarget3D(bedCenter);
        Refresh3DViewport();
        return;
    }

    // Fallback if the Bed3D bounding box is not initialized yet: use zoom_to_bed
    void* canvas3D = GetActiveCanvas3DPtr();
    FnCanvasZoomToBed fnZoomBed = (FnCanvasZoomToBed)dll::At(dll::kRvaCanvasZoomToBed);
    if (canvas3D && fnZoomBed) {
        __try {
            fnZoomBed(canvas3D);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    Refresh3DViewport();
}

// Called every 30 ms on the main GUI thread. While "Center of active object" is active,
// selecting another object moves the camera target to it, and orbiting keeps it centered.
static void UpdateOriginFollowOnTimer()
{
    if (!g_originLockActive) return;

    bool lBtnDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rBtnDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool mBtnDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    if (lBtnDown || rBtnDown || mBtnDown) return;

    double selCenter[3] = { 0.0, 0.0, 0.0 };
    if (!GetSelectedObjectCenter3D(selCenter)) {
        return;
    }

    double dx = selCenter[0] - g_lockedOriginTarget[0];
    double dy = selCenter[1] - g_lockedOriginTarget[1];
    double dz = selCenter[2] - g_lockedOriginTarget[2];
    double distSq = dx*dx + dy*dy + dz*dz;

    if (distSq > 0.001) {
        memcpy(g_lockedOriginTarget, selCenter, sizeof(selCenter));
        ApplyCameraTarget3D(g_lockedOriginTarget);
        Refresh3DViewport();
    } else {
        void* camera = GetCameraPtr();
        if (camera) {
            __try {
                const double* camTarget = (const double*)((const BYTE*)camera + dll::kOffCameraTarget);
                double tx = camTarget[0] - g_lockedOriginTarget[0];
                double ty = camTarget[1] - g_lockedOriginTarget[1];
                double tz = camTarget[2] - g_lockedOriginTarget[2];
                if (tx*tx + ty*ty + tz*tz > 0.01) {
                    ApplyCameraTarget3D(g_lockedOriginTarget);
                    Refresh3DViewport();
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
}

// Helper to rewrite window titles cleanly while preserving fork filiation on MainFrame
static std::wstring FormatBrandTitle(HWND hWnd, const wchar_t* origText)
{
    if (!IsValidStr(origText) || origText[0] == L'\0') {
        return AppFullTitle();
    }

    std::wstring s(origText);
    bool isMain = (hWnd != nullptr && (hWnd == g_hMainWnd || IsMainApplicationFrame(hWnd)));

    if (isMain) {
        // Title already set by OpenSlicer (possibly in the previous language): keep the project part
        size_t own = s.find(AppTitleHead());
        if (own != std::wstring::npos) {
            return s.substr(0, own) + AppFullTitle();
        }
        if (s.rfind(AppFullTitle()) != std::wstring::npos) {
            if (s.find(L"OpenSlicer - ") == 0 || s.find(L"OpenPS - ") == 0 || s.find(L"PrusaSlicer - ") == 0) {
                return AppFullTitle();
            }
            return s;
        }

        size_t dashPos = s.find(L"-");
        if (dashPos != std::wstring::npos && dashPos > 0) {
            std::wstring prefix = s.substr(0, dashPos);
            while (!prefix.empty() && (prefix.back() == L' ' || prefix.back() == L'\t')) {
                prefix.pop_back();
            }
            if (prefix.empty() || prefix == L"OpenPS" || prefix == L"PrusaSlicer" || prefix == L"OpenSlicer") {
                return AppFullTitle();
            }
            return prefix + L" - " + AppFullTitle();
        }
        return AppFullTitle();
    }

    // Other windows: "PrusaSlicer" becomes "OpenSlicer", except where the text names the upstream
    // project on purpose (our own "based on PrusaSlicer" title, "(Fork)" documentation links)
    if (s.find(AppFullTitle()) == std::wstring::npos &&
        s.find(L"(Fork)") == std::wstring::npos) {
        size_t pos = 0;
        while ((pos = s.find(L"PrusaSlicer", pos)) != std::wstring::npos) {
            s.replace(pos, 11, APP_BRAND_NAME);
            pos += 10;
        }
    }

    return s;
}

// -------------------------------------------------------------------------------------------------
// MODERN BUTTON DESIGN SUBCLASSING (Crimson Red #E02424 & Anthracite-Black #24272E matching Icon)
// -------------------------------------------------------------------------------------------------
static void AddRoundedRectPath(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float radius)
{
    float d = radius * 2.0f;
    if (d > w) d = w;
    if (d > h) d = h;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static bool IsStylableTextPushButton(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd)) return false;

    wchar_t cls[32] = { 0 };
    GetClassNameW(hWnd, cls, 31);
    if (_wcsicmp(cls, L"Button") != 0) return false;

    // Sync tab buttons draw themselves in the Prusa Connect style
    if (GetPropW(hWnd, openps::sync::kPropBtnKind) != nullptr) return false;

    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    DWORD btnType = (DWORD)(style & 0x0F);

    // wxWidgets MSW converts wxButton to BS_OWNERDRAW (0x0B) with BS_FLAT (0x8000) when UpdateDarkUI runs,
    // whereas wxCheckBox / wxRadioButton sets BS_LEFT (0x0100) and BS_GROUPBOX is 0x07.
    bool isPushType = (btnType == BS_PUSHBUTTON) ||
                      (btnType == BS_DEFPUSHBUTTON) ||
                      (btnType == BS_OWNERDRAW && (style & 0x0100) == 0);
    if (!isPushType) return false;
    if (style & (BS_BITMAP | BS_ICON)) return false;

    // Never touch ScalableButton icon-only controls
    if (SendMessageW(hWnd, BM_GETIMAGE, IMAGE_BITMAP, 0) != 0) return false;
    if (SendMessageW(hWnd, BM_GETIMAGE, IMAGE_ICON, 0) != 0) return false;

    int txtLen = GetWindowTextLengthW(hWnd);
    return (txtLen > 0);
}

static std::wstring StripButtonMnemonic(const wchar_t* raw)
{
    std::wstring out;
    if (!raw) return out;
    for (size_t i = 0; raw[i] != L'\0'; ++i) {
        if (raw[i] == L'&') {
            if (raw[i + 1] == L'&') {
                out.push_back(L'&');
                ++i;
            }
            continue;
        }
        out.push_back(raw[i]);
    }
    return out;
}

static std::wstring ToLowerW(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

// True when the label starts with the (translated or English) text of one of the msgids
static bool LabelStartsWithAny(const std::wstring& label, std::initializer_list<const char*> msgids)
{
    std::wstring lower = ToLowerW(label);
    for (const char* id : msgids) {
        std::wstring tr = ToLowerW(TrApp(id));
        std::wstring en = ToLowerW(openps::sync::Utf8ToWide(id));
        if ((!tr.empty() && lower.rfind(tr, 0) == 0) || lower.rfind(en, 0) == 0) return true;
    }
    return false;
}

// Main call-to-action buttons of the sidebar, painted in red
static bool IsPrimaryCtaButton(HWND hWnd, const std::wstring& cleanLabel)
{
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if ((style & 0x0F) == BS_DEFPUSHBUTTON) return true;
    return LabelStartsWithAny(cleanLabel, { "Slice now", "Slicing", "Slice all", "Export G-code", "Export all G-codes" });
}

static void PaintModernButtonToDC(HWND hWnd, HDC hdc, bool isHovered)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 4 || h <= 4) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Fill corner background seamlessly using parent control brush
    HBRUSH hParentBr = nullptr;
    HWND hParent = GetParent(hWnd);
    if (hParent) {
        hParentBr = (HBRUSH)SendMessageW(hParent, WM_CTLCOLORBTN, (WPARAM)memDC, (LPARAM)hWnd);
    }
    FillRect(memDC, &rc, hParentBr ? hParentBr : GetSysColorBrush(COLOR_BTNFACE));

    wchar_t rawBuf[256] = { 0 };
    GetWindowTextW(hWnd, rawBuf, 255);
    std::wstring label = StripButtonMnemonic(rawBuf);

    bool isPrimary = IsPrimaryCtaButton(hWnd, label);
    bool isEnabled = IsWindowEnabled(hWnd) != FALSE;
    LRESULT btnState = SendMessageW(hWnd, BM_GETSTATE, 0, 0);
    bool isPressed = (btnState & BST_PUSHED) != 0;

    Gdiplus::Color bgCol, borderCol, textCol;

    if (!isEnabled) {
        if (isPrimary) {
            // Sleek Anthracite-Black disabled CTA with crisp contrast & Crimson Red accent
            bgCol     = Gdiplus::Color(255, 38, 41, 48);
            borderCol = Gdiplus::Color(255, 64, 70, 82);
            textCol   = Gdiplus::Color(255, 196, 204, 214);
        } else {
            bgCol     = Gdiplus::Color(255, 58, 62, 72);
            borderCol = Gdiplus::Color(255, 82, 88, 100);
            textCol   = Gdiplus::Color(255, 165, 172, 184);
        }
    } else if (isPrimary) {
        // Primary CTA: OpenPS Crimson Red (#E02424) + Anthracite-Black contrast border
        if (isPressed) {
            bgCol     = Gdiplus::Color(255, 153, 27, 27); // #991B1B
            borderCol = Gdiplus::Color(255, 20, 22, 27);
        } else if (isHovered) {
            bgCol     = Gdiplus::Color(255, 198, 28, 28); // #C61C1C
            borderCol = Gdiplus::Color(255, 20, 22, 27);
        } else {
            bgCol     = Gdiplus::Color(255, 224, 36, 36); // #E02424
            borderCol = Gdiplus::Color(255, 36, 39, 46);  // #24272E
        }
        textCol = Gdiplus::Color(255, 255, 255, 255);
    } else {
        // Secondary Action Button: OpenPS Anthracite-Black (#24272E), Crimson Red (#E02424) on hover!
        if (isPressed) {
            bgCol     = Gdiplus::Color(255, 175, 28, 28);
            borderCol = Gdiplus::Color(255, 20, 22, 27);
        } else if (isHovered) {
            bgCol     = Gdiplus::Color(255, 224, 36, 36); // #E02424
            borderCol = Gdiplus::Color(255, 20, 22, 27);
        } else {
            bgCol     = Gdiplus::Color(255, 36, 39, 46);  // #24272E
            borderCol = Gdiplus::Color(255, 20, 22, 27);
        }
        textCol = Gdiplus::Color(255, 248, 250, 252);
    }

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        float radius = (h >= 28) ? 6.0f : 4.5f;
        Gdiplus::GraphicsPath path;
        AddRoundedRectPath(path, 0.6f, 0.6f, (float)w - 1.2f, (float)h - 1.2f, radius);

        Gdiplus::SolidBrush fillBrush(bgCol);
        Gdiplus::Pen borderPen(borderCol, (isHovered && isEnabled) ? 1.5f : 1.1f);
        g.FillPath(&fillBrush, &path);
        g.DrawPath(&borderPen, &path);

        // Red accent bar on a disabled primary button ("Slice now" before slicing is possible)
        if (!isEnabled && isPrimary && h >= 22) {
            Gdiplus::SolidBrush accentBrush(Gdiplus::Color(245, 224, 36, 36));
            g.FillRectangle(&accentBrush, 5.0f, 6.0f, 3.5f, (float)h - 12.0f);
        }

        float fontPx = std::max(11.5f, std::min(15.0f, (float)h * 0.42f));
        Gdiplus::FontFamily fontFam(L"Segoe UI");
        Gdiplus::Font btnFont(&fontFam, fontPx, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        Gdiplus::SolidBrush txtBrush(textCol);
        Gdiplus::RectF textRc(8.0f, isPressed ? 1.0f : 0.0f, (float)w - 16.0f, (float)h);
        g.DrawString(label.c_str(), -1, &btnFont, textRc, &sf, &txtBrush);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK ModernButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    // dwRefData bit 0 = mouse hovered
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE:
    {
        if ((dwRefData & 1) == 0) {
            dwRefData |= 1;
            SetWindowSubclass(hWnd, ModernButtonSubclassProc, uIdSubclass, dwRefData);
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE:
    {
        if ((dwRefData & 1) != 0) {
            dwRefData &= ~1ULL;
            SetWindowSubclass(hWnd, ModernButtonSubclassProc, uIdSubclass, dwRefData);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case BM_SETSTATE:
    {
        LRESULT r = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (IsStylableTextPushButton(hWnd) && IsWindowVisible(hWnd)) {
            HDC hdc = GetDC(hWnd);
            if (hdc) {
                PaintModernButtonToDC(hWnd, hdc, (dwRefData & 1) != 0);
                ReleaseDC(hWnd, hdc);
            }
            ValidateRect(hWnd, nullptr);
        }
        return r;
    }
    case WM_PRINTCLIENT:
    {
        if (IsStylableTextPushButton(hWnd) && wParam != 0) {
            PaintModernButtonToDC(hWnd, (HDC)wParam, (dwRefData & 1) != 0);
            return 0;
        }
        break;
    }
    case WM_PAINT:
    {
        if (!IsStylableTextPushButton(hWnd)) {
            break;
        }
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        PaintModernButtonToDC(hWnd, hdc, (dwRefData & 1) != 0);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, ModernButtonSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static void SubclassButtonsInHierarchy(HWND hRoot)
{
    if (!hRoot || !IsWindow(hRoot)) return;
    EnumChildWindows(hRoot, [](HWND hChild, LPARAM) -> BOOL {
        if (IsStylableTextPushButton(hChild)) {
            DWORD_PTR refData = 0;
            if (!GetWindowSubclass(hChild, ModernButtonSubclassProc, SUBCLASS_ID_BUTTON, &refData)) {
                SetWindowSubclass(hChild, ModernButtonSubclassProc, SUBCLASS_ID_BUTTON, 0);
                InvalidateRect(hChild, nullptr, TRUE);
            }
        }
        return TRUE;
    }, 0);
}

static void SyncModernButtonsOnGuiThread()
{
    EnumThreadWindows(GetCurrentThreadId(), [](HWND hTop, LPARAM) -> BOOL {
        if (IsWindowVisible(hTop) && hTop != g_hViewCubeWnd) {
            SubclassButtonsInHierarchy(hTop);
        }
        return TRUE;
    }, 0);
}

// -------------------------------------------------------------------------------------------------
// "Set origin" submenu (3D view and object list context menus, ViewCube button)
// -------------------------------------------------------------------------------------------------
static bool IsPlater3DContextMenu(HMENU hMenu, HWND hWnd)
{
    if (!hMenu) return false;
    int count = GetMenuItemCount(hMenu);
    if (count < 1) return false;

    // 1. Direct right-click on the 3D OpenGL canvas (covers BOTH Object menu & empty Build Plate menu!)
    if (hWnd && IsWindow(hWnd)) {
        wchar_t cls[64] = { 0 };
        GetClassNameW(hWnd, cls, 63);
        if (wcscmp(cls, L"wxGLCanvas") == 0) {
            return true;
        }
    }

    // 2. Object list / object context menus: recognized by their entries, in the application language
    for (int i = 0; i < count; ++i) {
        wchar_t itemText[256] = { 0 };
        if (GetMenuStringW(hMenu, i, itemText, 255, MF_BYPOSITION) > 0 &&
            LabelStartsWithAny(StripButtonMnemonic(itemText), { "Add Shape", "Add part", "Delete", "Split" })) {
            return true;
        }
    }
    return false;
}

static void EnsureAndRefreshOriginSubmenu(HMENU hMenu)
{
    if (!hMenu) return;

    bool hasSelection = HasActive3DSelection();

    // MenuFactory reuses HMENU handles across right-clicks: attach our submenu only once
    if (GetMenuState(hMenu, ID_ORIGIN_ACTIVE_OBJECT, MF_BYCOMMAND) == (UINT)-1) {
        HMENU hSub = CreatePopupMenu();
        if (!hSub) return;

        AppendMenuW(hSub, MF_STRING, ID_ORIGIN_ACTIVE_OBJECT, Tr(i18n::S_ORIGIN_OBJECT));
        AppendMenuW(hSub, MF_STRING, ID_ORIGIN_BED_CENTER,    Tr(i18n::S_ORIGIN_BED));

        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_POPUP | MF_STRING, (UINT_PTR)hSub, Tr(i18n::S_SET_ORIGIN));
    }

    // Dynamically update enabled & radio-checked states on every popup invocation
    EnableMenuItem(hMenu, ID_ORIGIN_ACTIVE_OBJECT, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, ID_ORIGIN_BED_CENTER,    MF_BYCOMMAND | MF_ENABLED);

    CheckMenuItem(hMenu, ID_ORIGIN_ACTIVE_OBJECT, MF_BYCOMMAND | (g_originLockActive ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_ORIGIN_BED_CENTER,    MF_BYCOMMAND | (!g_originLockActive ? MF_CHECKED : MF_UNCHECKED));
}

// Forward declaration for ViewCube refresh
static void RenderLayeredViewCube(HWND hWnd);

static bool DispatchOriginMenuCommand(UINT cmdId)
{
    if (cmdId == ID_ORIGIN_ACTIVE_OBJECT) {
        ExecuteOriginActiveObjectOnMainThread();
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            RenderLayeredViewCube(g_hViewCubeWnd);
        }
        return true;
    }
    if (cmdId == ID_ORIGIN_BED_CENTER) {
        ExecuteOriginBedCenterOnMainThread();
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            RenderLayeredViewCube(g_hViewCubeWnd);
        }
        return true;
    }
    return false;
}

// Opens the "Set origin" choices below the ViewCube origin button
static void ShowViewCubeOriginMenu(HWND hCubeWnd, const RECT& rcPill)
{
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    bool hasSelection = HasActive3DSelection();
    AppendMenuW(hPopup,
                MF_STRING | (hasSelection ? MF_ENABLED : MF_GRAYED) | (g_originLockActive ? MF_CHECKED : MF_UNCHECKED),
                ID_ORIGIN_ACTIVE_OBJECT,
                Tr(i18n::S_ORIGIN_OBJECT));
    AppendMenuW(hPopup,
                MF_STRING | MF_ENABLED | (!g_originLockActive ? MF_CHECKED : MF_UNCHECKED),
                ID_ORIGIN_BED_CENTER,
                Tr(i18n::S_ORIGIN_BED));

    RECT rcWnd;
    GetWindowRect(hCubeWnd, &rcWnd);
    int screenX = rcWnd.left + CubePx(rcPill.right);
    int screenY = rcWnd.top + CubePx(rcPill.bottom + 4);

    TrackPopupMenu_t fnTrack = OriginalTrackPopupMenu ? OriginalTrackPopupMenu : TrackPopupMenu;
    HWND hOwner = (g_hMainWnd && IsWindow(g_hMainWnd)) ? g_hMainWnd : hCubeWnd;

    UINT selected = (UINT)fnTrack(
        hPopup,
        TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        screenX, screenY, 0, hOwner, nullptr
    );

    DestroyMenu(hPopup);

    if (selected != 0) {
        DispatchOriginMenuCommand(selected);
    }
}

// -------------------------------------------------------------------------------------------------
// 3D VIEWCUBE OVERLAY GIZMO (Single-Instance Guarded + OpenPS Red #E02424 & Anthracite-Black Theme)
// -------------------------------------------------------------------------------------------------

struct ProjectedFace {
    int viewCode;           // 1=Top, 2=Bottom, 3=Front, 4=Rear, 5=Left, 6=Right
    const wchar_t* label;
    Gdiplus::PointF pts[4];
    double avgZ;
    double normalZ;
    bool visible;
};

struct ProjectedAxis {
    int viewCode;
    const wchar_t* label;
    Gdiplus::PointF p0;
    Gdiplus::PointF p1;
    Gdiplus::Color color;
};

static ProjectedFace g_visibleFaces[6];
static int g_numVisibleFaces = 0;
static ProjectedAxis g_projAxes[3];
static int g_hoverCode = -1; // -1=None, 0=Iso, 1..6=Cube faces, 10=Origine submenu pill
static double g_lastRot[9] = { 0 };
static bool g_lastLockRendered = false;

static const RECT g_rcPillIso     = { 6,  126, 58,  150 };
static const RECT g_rcPillOrigine = { 64, 126, 142, 150 };

// Upper-case label for a cube face (locale aware)
static std::wstring ViewCubeLabel(const std::wstring& text)
{
    std::wstring out = text;
    if (!out.empty()) {
        LCMapStringEx(LOCALE_NAME_USER_DEFAULT, LCMAP_UPPERCASE, text.c_str(), (int)text.size(),
                      &out[0], (int)out.size(), nullptr, nullptr, 0);
    }
    return out;
}

// Draws text centered at (cx, cy), shrinking the font so that it fits in maxWidth
static void DrawFittedText(Gdiplus::Graphics& g, const wchar_t* text, Gdiplus::FontFamily& family, float sizePx,
                           float minPx, float maxWidth, float cx, float cy, const Gdiplus::Brush& brush)
{
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    for (float px = sizePx; px >= minPx; px -= 0.5f) {
        Gdiplus::Font font(&family, px, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::RectF box;
        g.MeasureString(text, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &sf, &box);
        if (box.Width <= maxWidth || px - 0.5f < minPx) {
            if (box.Width <= maxWidth * 1.15f) {
                g.DrawString(text, -1, &font, Gdiplus::PointF(cx, cy), &sf, &brush);
            }
            return;
        }
    }
}

static bool IsPointInPolygonF(const Gdiplus::PointF* pts, int n, float x, float y)
{
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((pts[i].Y > y) != (pts[j].Y > y)) &&
            (x < (pts[j].X - pts[i].X) * (y - pts[i].Y) / (pts[j].Y - pts[i].Y) + pts[i].X)) {
            inside = !inside;
        }
    }
    return inside;
}

static void ComputeCubeGeometry(const double R[9])
{
    const float cx = 74.0f;
    const float cy = 62.0f;
    const float scale = 31.0f;

    const double verts[8][3] = {
        {-1, -1, -1}, {+1, -1, -1}, {+1, +1, -1}, {-1, +1, -1},
        {-1, -1, +1}, {+1, -1, +1}, {+1, +1, +1}, {-1, +1, +1}
    };

    double vx[8], vy[8], vz[8];
    Gdiplus::PointF sp[8];
    for (int i = 0; i < 8; ++i) {
        vx[i] = R[0]*verts[i][0] + R[1]*verts[i][1] + R[2]*verts[i][2];
        vy[i] = R[3]*verts[i][0] + R[4]*verts[i][1] + R[5]*verts[i][2];
        vz[i] = R[6]*verts[i][0] + R[7]*verts[i][1] + R[8]*verts[i][2];
        double persp = 1.0 / (1.0 - vz[i] * 0.12);
        sp[i].X = cx + (float)(vx[i] * scale * persp);
        sp[i].Y = cy - (float)(vy[i] * scale * persp);
    }

    struct FaceDef {
        int v[4];
        double nx, ny, nz;
        int viewCode;
        const char* msgid; // same names as PrusaSlicer's View menu, translated by its catalog
    };

    static const FaceDef defs[6] = {
        { {4, 5, 6, 7},  0,  0, +1, 1, "Top"    },
        { {3, 2, 1, 0},  0,  0, -1, 2, "Bottom" },
        { {0, 1, 5, 4},  0, -1,  0, 3, "Front"  },
        { {2, 3, 7, 6},  0, +1,  0, 4, "Rear"   },
        { {3, 0, 4, 7}, -1,  0,  0, 5, "Left"   },
        { {1, 2, 6, 5}, +1,  0,  0, 6, "Right"  }
    };
    static std::wstring labels[6];
    static int labelsGeneration = -1;
    if (labelsGeneration != i18n::LanguageGeneration()) {
        for (int f = 0; f < 6; ++f) labels[f] = ViewCubeLabel(TrApp(defs[f].msgid));
        labelsGeneration = i18n::LanguageGeneration();
    }

    g_numVisibleFaces = 0;
    for (int f = 0; f < 6; ++f) {
        double nz = R[6]*defs[f].nx + R[7]*defs[f].ny + R[8]*defs[f].nz;
        if (nz > -0.05) {
            ProjectedFace pf;
            pf.viewCode = defs[f].viewCode;
            pf.label = labels[f].c_str();
            pf.normalZ = nz;
            pf.avgZ = 0.25 * (vz[defs[f].v[0]] + vz[defs[f].v[1]] + vz[defs[f].v[2]] + vz[defs[f].v[3]]);
            for (int k = 0; k < 4; ++k) {
                pf.pts[k] = sp[defs[f].v[k]];
            }
            pf.visible = true;
            g_visibleFaces[g_numVisibleFaces++] = pf;
        }
    }

    std::sort(g_visibleFaces, g_visibleFaces + g_numVisibleFaces, [](const ProjectedFace& a, const ProjectedFace& b) {
        return a.avgZ < b.avgZ;
    });

    const double axisLen = 2.15;
    const double axisDirs[3][3] = {
        { -1 + axisLen, -1, -1 }, // +X (Right)
        { -1, -1 + axisLen, -1 }, // +Y (Rear)
        { -1, -1, -1 + axisLen }  // +Z (Top)
    };
    const int axisCodes[3] = { 6, 4, 1 };
    const wchar_t* axisLabels[3] = { L"X", L"Y", L"Z" };
    const Gdiplus::Color axisColors[3] = {
        Gdiplus::Color(255, 224, 36, 36),  // X OpenPS Crimson Red (#E02424)
        Gdiplus::Color(255, 46, 160, 67),  // Y CAD Green
        Gdiplus::Color(255, 30, 136, 229)  // Z CAD Blue
    };

    for (int a = 0; a < 3; ++a) {
        double ax = R[0]*axisDirs[a][0] + R[1]*axisDirs[a][1] + R[2]*axisDirs[a][2];
        double ay = R[3]*axisDirs[a][0] + R[4]*axisDirs[a][1] + R[5]*axisDirs[a][2];
        double az = R[6]*axisDirs[a][0] + R[7]*axisDirs[a][1] + R[8]*axisDirs[a][2];
        double persp = 1.0 / (1.0 - az * 0.12);
        g_projAxes[a].p0 = sp[0];
        g_projAxes[a].p1 = Gdiplus::PointF(cx + (float)(ax * scale * persp), cy - (float)(ay * scale * persp));
        g_projAxes[a].viewCode = axisCodes[a];
        g_projAxes[a].label = axisLabels[a];
        g_projAxes[a].color = axisColors[a];
    }
}

static void RenderLayeredViewCube(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd)) return;

    double R[9];
    if (!ReadCameraRotationMatrix(R)) {
        R[0] =  0.7071; R[1] =  0.7071; R[2] = 0.0000;
        R[3] = -0.4082; R[4] =  0.4082; R[5] = 0.8165;
        R[6] =  0.5774; R[7] = -0.5774; R[8] = 0.5774;
    }
    memcpy(g_lastRot, R, sizeof(R));
    g_lastLockRendered = g_originLockActive;
    ComputeCubeGeometry(R);

    const int cubeW = CubePx(VIEWCUBE_WIDTH);
    const int cubeH = CubePx(VIEWCUBE_HEIGHT);
    Gdiplus::Bitmap bmp(cubeW, cubeH, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics g(&bmp);
        g.ScaleTransform(g_cubeScale, g_cubeScale); // everything below is laid out at 96 DPI
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // Modern CAD circular compass housing (Anthracite-Black + subtle Crimson Red ring accent)
        Gdiplus::SolidBrush haloBrush(Gdiplus::Color(115, 24, 26, 32));
        Gdiplus::Pen haloPen(Gdiplus::Color(185, 54, 60, 72), 1.4f);
        g.FillEllipse(&haloBrush, 20.0f, 8.0f, 108.0f, 108.0f);
        g.DrawEllipse(&haloPen, 20.0f, 8.0f, 108.0f, 108.0f);

        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font fontAxis(&fontFamily, 10.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

        Gdiplus::StringFormat sfCenter;
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        for (int i = 0; i < g_numVisibleFaces; ++i) {
            const ProjectedFace& pf = g_visibleFaces[i];
            bool isHovered = (g_hoverCode == pf.viewCode);

            int shade = (int)(192.0 + 56.0 * std::max(0.0, std::min(1.0, pf.normalZ)));
            // Hover = OpenPS Crimson Red (#E02424); Normal = Crisp CAD Satin White/Slate with Anthracite edges
            Gdiplus::Color fillCol = isHovered
                ? Gdiplus::Color(250, 224, 36, 36)
                : Gdiplus::Color(246, (BYTE)(shade - 8), (BYTE)(shade - 4), (BYTE)shade);

            Gdiplus::Color edgeCol = isHovered
                ? Gdiplus::Color(255, 255, 220, 220)
                : Gdiplus::Color(250, 36, 39, 46);

            Gdiplus::SolidBrush faceBrush(fillCol);
            Gdiplus::Pen edgePen(edgeCol, isHovered ? 1.8f : 1.35f);
            edgePen.SetLineJoin(Gdiplus::LineJoinRound);

            g.FillPolygon(&faceBrush, pf.pts, 4);
            g.DrawPolygon(&edgePen, pf.pts, 4);

            if (pf.normalZ > 0.22) {
                float fcx = 0.25f * (pf.pts[0].X + pf.pts[1].X + pf.pts[2].X + pf.pts[3].X);
                float fcy = 0.25f * (pf.pts[0].Y + pf.pts[1].Y + pf.pts[2].Y + pf.pts[3].Y);
                Gdiplus::SolidBrush textBrush(
                    isHovered ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 28, 32, 40)
                );
                // Face width along its center line, to fit long translations
                float midL = 0.5f * (pf.pts[0].X + pf.pts[3].X), midR = 0.5f * (pf.pts[1].X + pf.pts[2].X);
                float faceW = fabsf(midR - midL);
                DrawFittedText(g, pf.label, fontFamily, 8.5f, 6.0f, std::max(14.0f, faceW - 4.0f), fcx, fcy, textBrush);
            }
        }

        // 3D Coordinate Axes (X Crimson Red, Y Green, Z Blue)
        for (int a = 0; a < 3; ++a) {
            const ProjectedAxis& ax = g_projAxes[a];
            Gdiplus::Pen axisPen(ax.color, 2.2f);
            axisPen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&axisPen, ax.p0, ax.p1);

            float dx = ax.p1.X - ax.p0.X;
            float dy = ax.p1.Y - ax.p0.Y;
            float len = sqrtf(dx*dx + dy*dy);
            float tx = ax.p1.X + (len > 1.0f ? (dx / len) * 8.0f : 0.0f);
            float ty = ax.p1.Y + (len > 1.0f ? (dy / len) * 8.0f : 0.0f);

            Gdiplus::SolidBrush tipBrush(ax.color);
            g.FillEllipse(&tipBrush, ax.p1.X - 3.0f, ax.p1.Y - 3.0f, 6.0f, 6.0f);
            g.DrawString(ax.label, -1, &fontAxis, Gdiplus::PointF(tx, ty), &sfCenter, &tipBrush);
        }

        // Buttons under the cube: [ISO] and [ORIGIN v]
        auto drawPill = [&](const RECT& rc, int code, const wchar_t* text, bool activeLock) {
            bool hovered = (g_hoverCode == code);
            Gdiplus::GraphicsPath path;
            AddRoundedRectPath(path, (float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top), 5.5f);

            Gdiplus::Color bgCol = hovered
                ? Gdiplus::Color(252, 224, 36, 36) // Crimson Red (#E02424) on hover
                : (activeLock ? Gdiplus::Color(248, 175, 24, 24) : Gdiplus::Color(246, 30, 33, 40)); // Active Crimson vs Anthracite-Black (#1E2128)
            Gdiplus::Color borderCol = (hovered || activeLock)
                ? Gdiplus::Color(255, 224, 36, 36)
                : Gdiplus::Color(235, 76, 84, 98);

            Gdiplus::SolidBrush bgBrush(bgCol);
            Gdiplus::Pen borderPen(borderCol, 1.3f);
            g.FillPath(&bgBrush, &path);
            g.DrawPath(&borderPen, &path);

            Gdiplus::SolidBrush txtBrush(Gdiplus::Color(255, 250, 252, 255));
            DrawFittedText(g, text, fontFamily, 9.0f, 6.5f, (float)(rc.right - rc.left) - 6.0f,
                           0.5f * (rc.left + rc.right), 0.5f * (rc.top + rc.bottom), txtBrush);
        };

        drawPill(g_rcPillIso,     0,  L"ISO",            false);
        const std::wstring originLabel = std::wstring(Tr(i18n::S_ORIGIN_PILL)) + L" \u25BE";
        drawPill(g_rcPillOrigine, 10, originLabel.c_str(), g_originLockActive);
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cubeW;
    bmi.bmiHeader.biHeight = -cubeH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    if (pvBits) {
        Gdiplus::Rect lockRect(0, 0, cubeW, cubeH);
        Gdiplus::BitmapData bmpData;
        if (bmp.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bmpData) == Gdiplus::Ok) {
            for (int y = 0; y < cubeH; ++y) {
                memcpy((BYTE*)pvBits + y * (cubeW * 4),
                       (const BYTE*)bmpData.Scan0 + y * bmpData.Stride,
                       cubeW * 4);
            }
            bmp.UnlockBits(&bmpData);
        }
    }

    RECT rcWnd;
    GetWindowRect(hWnd, &rcWnd);
    POINT ptDst = { rcWnd.left, rcWnd.top };
    SIZE szWnd  = { cubeW, cubeH };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &szWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdcScreen);
}

// (x, y) in window pixels; the geometry is stored at 96 DPI
static int HitTestViewCube(int x, int y)
{
    x = (int)((float)x / g_cubeScale);
    y = (int)((float)y / g_cubeScale);
    POINT pt = { x, y };
    if (PtInRect(&g_rcPillIso, pt)) return 0;
    if (PtInRect(&g_rcPillOrigine, pt)) return 10;

    for (int a = 0; a < 3; ++a) {
        float dx = (float)x - g_projAxes[a].p1.X;
        float dy = (float)y - g_projAxes[a].p1.Y;
        if (dx*dx + dy*dy <= 100.0f) {
            return g_projAxes[a].viewCode;
        }
    }

    for (int i = g_numVisibleFaces - 1; i >= 0; --i) {
        if (g_visibleFaces[i].normalZ > 0.05 && IsPointInPolygonF(g_visibleFaces[i].pts, 4, (float)x, (float)y)) {
            return g_visibleFaces[i].viewCode;
        }
    }

    return -1;
}

LRESULT CALLBACK ViewCubeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_MOUSEMOVE:
    {
        int x = (short)LOWORD(lParam);
        int y = (short)HIWORD(lParam);
        int hit = HitTestViewCube(x, y);

        if (hit != g_hoverCode) {
            g_hoverCode = hit;
            RenderLayeredViewCube(hWnd);
        }

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
    {
        if (g_hoverCode != -1) {
            g_hoverCode = -1;
            RenderLayeredViewCube(hWnd);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        int x = (short)LOWORD(lParam);
        int y = (short)HIWORD(lParam);
        int hit = HitTestViewCube(x, y);

        if (hit >= 0 && hit <= 6 && uMsg == WM_LBUTTONDOWN) {
            ExecuteSelectViewOnMainThread(hit);
            RenderLayeredViewCube(hWnd);
        } else if (hit == 10) {
            g_hoverCode = -1;
            RenderLayeredViewCube(hWnd);
            ShowViewCubeOriginMenu(hWnd, g_rcPillOrigine);
        }
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// Sweeps and destroys any orphaned OpenSlicer_ViewCube3D window != keepWnd
static void DestroyOrphanViewCubeWindows(HWND keepWnd)
{
    EnumThreadWindows(GetCurrentThreadId(), [](HWND hTop, LPARAM lParam) -> BOOL {
        HWND hKeep = (HWND)lParam;
        if (hTop != hKeep && IsWindow(hTop)) {
            wchar_t cls[64] = { 0 };
            GetClassNameW(hTop, cls, 63);
            if (wcscmp(cls, L"OpenSlicer_ViewCube3D") == 0) {
                DestroyWindow(hTop);
            }
        }
        return TRUE;
    }, (LPARAM)keepWnd);
}

// Executed on the main GUI thread with strict re-entrancy protection:
// Guarantees AT MOST ONE ViewCube gizmo ever exists!
static void EnsureAndSyncViewCubeOnMainThread()
{
    if (g_inSyncViewCube) return;
    g_inSyncViewCube = true;

    struct ReentrancyRelease {
        ~ReentrancyRelease() { g_inSyncViewCube = false; }
    } guardRelease;

    if (!g_hMainWnd || !IsWindow(g_hMainWnd)) return;

    // Hidden when the main window is minimized, hidden or disabled by a modal dialog, when the Sync tab
    // is active, or when PrusaSlicer.dll is not the supported build (the cube needs its camera)
    if (!dll::Supported() || openps::sync::SyncTab_IsActive() || IsIconic(g_hMainWnd) || !IsWindowVisible(g_hMainWnd) || !IsWindowEnabled(g_hMainWnd)) {
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            ShowWindow(g_hViewCubeWnd, SW_HIDE);
        }
        DestroyOrphanViewCubeWindows(g_hViewCubeWnd);
        return;
    }

    HWND hCanvas = Find3DCanvasWindow(g_hMainWnd);
    if (!hCanvas || !IsWindow(hCanvas) || !IsWindowVisible(hCanvas)) {
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            ShowWindow(g_hViewCubeWnd, SW_HIDE);
        }
        DestroyOrphanViewCubeWindows(g_hViewCubeWnd);
        return;
    }

    RECT rcCanvas;
    GetClientRect(hCanvas, &rcCanvas);
    int cw = rcCanvas.right - rcCanvas.left;
    int ch = rcCanvas.bottom - rcCanvas.top;
    if (cw <= 250 || ch <= 250) {
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            ShowWindow(g_hViewCubeWnd, SW_HIDE);
        }
        DestroyOrphanViewCubeWindows(g_hViewCubeWnd);
        return;
    }

    float scale = (float)openps::sync::GetWindowDpiSafe(g_hMainWnd) / 96.0f;
    bool rescaled = fabsf(scale - g_cubeScale) > 0.01f;
    g_cubeScale = scale;
    const int cubeW = CubePx(VIEWCUBE_WIDTH);
    const int cubeH = CubePx(VIEWCUBE_HEIGHT);
    POINT pt = { rcCanvas.right - cubeW - CubePx(14), rcCanvas.top + CubePx(14) };
    ClientToScreen(hCanvas, &pt);

    if (!g_hViewCubeWnd || !IsWindow(g_hViewCubeWnd)) {
        DestroyOrphanViewCubeWindows(nullptr);

        HINSTANCE hInst = GetModuleHandleW(nullptr);
        if (!g_viewCubeClassRegistered) {
            WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
            wc.lpfnWndProc = ViewCubeWndProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"OpenSlicer_ViewCube3D";
            wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            RegisterClassExW(&wc);
            g_viewCubeClassRegistered = true;
        }

        // CRITICAL: Create WITHOUT WS_VISIBLE first so WM_WINDOWPOSCHANGED cannot fire before g_hViewCubeWnd is assigned!
        CreateWindowExW_t fnCreate = OriginalCreateWindowExW ? OriginalCreateWindowExW : CreateWindowExW;
        HWND hCreated = fnCreate(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"OpenSlicer_ViewCube3D", L"",
            WS_POPUP,
            pt.x, pt.y, cubeW, cubeH,
            g_hMainWnd, nullptr, hInst, nullptr
        );

        g_hViewCubeWnd = hCreated;
        if (g_hViewCubeWnd) {
            RenderLayeredViewCube(g_hViewCubeWnd);
            SetWindowPos(g_hViewCubeWnd, HWND_TOP, pt.x, pt.y, cubeW, cubeH,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        DestroyOrphanViewCubeWindows(g_hViewCubeWnd);
        return;
    }

    DestroyOrphanViewCubeWindows(g_hViewCubeWnd);

    RECT rcCur;
    GetWindowRect(g_hViewCubeWnd, &rcCur);
    bool moved = (rcCur.left != pt.x || rcCur.top != pt.y);
    bool wasHidden = !IsWindowVisible(g_hViewCubeWnd);

    if (moved || wasHidden || rescaled) {
        SetWindowPos(g_hViewCubeWnd, HWND_TOP, pt.x, pt.y, cubeW, cubeH,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    double curRot[9];
    bool needRender = moved || wasHidden || rescaled || (g_lastLockRendered != g_originLockActive);
    if (ReadCameraRotationMatrix(curRot)) {
        double diff = 0.0;
        for (int i = 0; i < 9; ++i) diff += fabs(curRot[i] - g_lastRot[i]);
        if (diff > 0.002) {
            needRender = true;
        }
    }

    if (needRender) {
        RenderLayeredViewCube(g_hViewCubeWnd);
    }
}

// Subclass window procedure for the main top-level window (executes on main wxWidgets thread)
LRESULT CALLBACK MainWndSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_SETTEXT:
    {
        const wchar_t* newText = (const wchar_t*)lParam;
        if (IsValidStr(newText)) {
            std::wstring rebranded = FormatBrandTitle(hWnd, newText);
            if (rebranded != newText) {
                return DefSubclassProc(hWnd, WM_SETTEXT, 0, (LPARAM)rebranded.c_str());
            }
        }
        break;
    }
    case WM_TIMER:
    {
        if (wParam == TIMER_MAIN_SYNC) {
            UpdateOriginFollowOnTimer();
            EnsureAndSyncViewCubeOnMainThread();
            if ((++g_syncTickCounter % 5) == 1) {
                SyncModernButtonsOnGuiThread();
                openps::sync::SyncTab_EnsureAndSync(hWnd);
            }
            // About every 2 s: follow a language change made in PrusaSlicer's preferences
            if ((g_syncTickCounter % 60) == 0 && i18n::RefreshLanguage()) {
                wchar_t title[512] = { 0 };
                GetWindowTextW(hWnd, title, 511);
                SetWindowTextW(hWnd, FormatBrandTitle(hWnd, title).c_str());
                if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
                    RenderLayeredViewCube(g_hViewCubeWnd);
                }
                openps::sync::SyncTab_OnLanguageChanged();
            }
            return 0;
        }
        break;
    }
    case WM_SIZE:
    case WM_MOVE:
    case WM_WINDOWPOSCHANGED:
    case WM_ACTIVATE:
    {
        LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        EnsureAndSyncViewCubeOnMainThread();
        openps::sync::SyncTab_EnsureAndSync(hWnd);
        return res;
    }
    case WM_OPENPS_SELECT_VIEW:
        ExecuteSelectViewOnMainThread((int)wParam);
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            RenderLayeredViewCube(g_hViewCubeWnd);
        }
        return 0;

    case WM_OPENPS_SET_ORIGIN:
        if (wParam == 1) {
            ExecuteOriginBedCenterOnMainThread();
        } else {
            ExecuteOriginActiveObjectOnMainThread();
        }
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            RenderLayeredViewCube(g_hViewCubeWnd);
        }
        return 0;

    case WM_COMMAND:
    {
        if (DispatchOriginMenuCommand(LOWORD(wParam))) {
            return 0;
        }
        break;
    }
    case WM_NCDESTROY:
        KillTimer(hWnd, TIMER_MAIN_SYNC);
        openps::sync::SyncTab_Shutdown();
        if (g_hViewCubeWnd && IsWindow(g_hViewCubeWnd)) {
            DestroyWindow(g_hViewCubeWnd);
            g_hViewCubeWnd = nullptr;
        }
        RemoveWindowSubclass(hWnd, MainWndSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static void AttachMainFrameHooks(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd)) return;
    g_hMainWnd = hWnd;
    DWORD_PTR existingRef = 0;
    if (!GetWindowSubclass(hWnd, MainWndSubclassProc, SUBCLASS_ID_MAIN, &existingRef)) {
        SetWindowSubclass(hWnd, MainWndSubclassProc, SUBCLASS_ID_MAIN, 0);
        SetTimer(hWnd, TIMER_MAIN_SYNC, 30, nullptr);
    }
    SubclassButtonsInHierarchy(hWnd);
}

// --- Win32 IAT Hook Implementations ---
HWND WINAPI Hook_CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
                                int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    if (!OriginalCreateWindowExW) {
        OriginalCreateWindowExW = (CreateWindowExW_t)GetProcAddress(GetModuleHandleW(L"USER32.dll"), "CreateWindowExW");
    }

    LPCWSTR finalTitle = lpWindowName;
    std::wstring rebranded;
    if (IsValidStr(lpWindowName) && wcsstr(lpWindowName, L"PrusaSlicer") != nullptr) {
        rebranded = FormatBrandTitle(nullptr, lpWindowName);
        finalTitle = rebranded.c_str();
    }

    HWND hCreated = OriginalCreateWindowExW(dwExStyle, lpClassName, finalTitle, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

    if (hCreated && IsValidStr(lpClassName) && _wcsicmp(lpClassName, L"Button") == 0) {
        if (IsStylableTextPushButton(hCreated)) {
            SetWindowSubclass(hCreated, ModernButtonSubclassProc, SUBCLASS_ID_BUTTON, 0);
        }
    }

    return hCreated;
}

BOOL WINAPI Hook_SetWindowTextW(HWND hWnd, LPCWSTR lpString)
{
    if (!OriginalSetWindowTextW) {
        OriginalSetWindowTextW = (SetWindowTextW_t)GetProcAddress(GetModuleHandleW(L"USER32.dll"), "SetWindowTextW");
    }

    if (IsValidStr(lpString)) {
        if (IsMainApplicationFrame(hWnd)) {
            AttachMainFrameHooks(hWnd);
            std::wstring rebranded = FormatBrandTitle(hWnd, lpString);
            return OriginalSetWindowTextW(hWnd, rebranded.c_str());
        } else if (wcsstr(lpString, L"PrusaSlicer") != nullptr) {
            std::wstring s = FormatBrandTitle(hWnd, lpString);
            BOOL ok = OriginalSetWindowTextW(hWnd, s.c_str());
            if (IsStylableTextPushButton(hWnd)) {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return ok;
        }
    }

    BOOL ok = OriginalSetWindowTextW(hWnd, lpString);
    if (hWnd && IsStylableTextPushButton(hWnd)) {
        DWORD_PTR refData = 0;
        if (!GetWindowSubclass(hWnd, ModernButtonSubclassProc, SUBCLASS_ID_BUTTON, &refData)) {
            SetWindowSubclass(hWnd, ModernButtonSubclassProc, SUBCLASS_ID_BUTTON, 0);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
    }
    return ok;
}

BOOL WINAPI Hook_TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT* prcRect)
{
    if (!OriginalTrackPopupMenu) {
        OriginalTrackPopupMenu = (TrackPopupMenu_t)GetProcAddress(GetModuleHandleW(L"USER32.dll"), "TrackPopupMenu");
    }

    if (hMenu && dll::Supported() && IsPlater3DContextMenu(hMenu, hWnd)) {
        EnsureAndRefreshOriginSubmenu(hMenu);
    }

    BOOL res = OriginalTrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

    if (uFlags & TPM_RETURNCMD) {
        if (DispatchOriginMenuCommand((UINT)res)) {
            return 0;
        }
    } else {
        // Intercept WM_COMMAND before wxWindowMSW::DoPopupMenu discards custom menu IDs
        MSG msgCmd;
        if (PeekMessageW(&msgCmd, nullptr, WM_COMMAND, WM_COMMAND, PM_NOREMOVE)) {
            UINT cmdId = LOWORD(msgCmd.wParam);
            if (cmdId == ID_ORIGIN_ACTIVE_OBJECT || cmdId == ID_ORIGIN_BED_CENTER) {
                PeekMessageW(&msgCmd, nullptr, WM_COMMAND, WM_COMMAND, PM_REMOVE);
                DispatchOriginMenuCommand(cmdId);
                return 0;
            }
        }
    }

    return res;
}

BOOL WINAPI Hook_TrackPopupMenuEx(HMENU hMenu, UINT uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm)
{
    if (!OriginalTrackPopupMenuEx) {
        OriginalTrackPopupMenuEx = (TrackPopupMenuEx_t)GetProcAddress(GetModuleHandleW(L"USER32.dll"), "TrackPopupMenuEx");
    }

    if (hMenu && dll::Supported() && IsPlater3DContextMenu(hMenu, hWnd)) {
        EnsureAndRefreshOriginSubmenu(hMenu);
    }

    BOOL res = OriginalTrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm);

    if (uFlags & TPM_RETURNCMD) {
        if (DispatchOriginMenuCommand((UINT)res)) {
            return 0;
        }
    } else {
        MSG msgCmd;
        if (PeekMessageW(&msgCmd, nullptr, WM_COMMAND, WM_COMMAND, PM_NOREMOVE)) {
            UINT cmdId = LOWORD(msgCmd.wParam);
            if (cmdId == ID_ORIGIN_ACTIVE_OBJECT || cmdId == ID_ORIGIN_BED_CENTER) {
                PeekMessageW(&msgCmd, nullptr, WM_COMMAND, WM_COMMAND, PM_REMOVE);
                DispatchOriginMenuCommand(cmdId);
                return 0;
            }
        }
    }

    return res;
}

// IAT Hooking Engine
static bool InstallIATHook(HMODULE hModule, const char* targetDll, const char* functionName, void* newFunction, void** oldFunction)
{
    if (!hModule || !targetDll || !functionName || !newFunction) {
        return false;
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    DWORD importRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importRVA == 0) {
        return false;
    }

    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule + importRVA);

    while (importDesc->Name != 0) {
        const char* dllName = (const char*)((BYTE*)hModule + importDesc->Name);
        if (_stricmp(dllName, targetDll) == 0) {
            PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + importDesc->FirstThunk);
            PIMAGE_THUNK_DATA origThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + (importDesc->OriginalFirstThunk ? importDesc->OriginalFirstThunk : importDesc->FirstThunk));

            while (origThunk->u1.AddressOfData != 0) {
                if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + origThunk->u1.AddressOfData);
                    if (strcmp((const char*)importByName->Name, functionName) == 0) {
                        if (oldFunction && *oldFunction == nullptr) {
                            *oldFunction = (void*)thunk->u1.Function;
                        }

                        DWORD oldProtect = 0;
                        if (VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                            thunk->u1.Function = (ULONG_PTR)newFunction;
                            VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                            FlushInstructionCache(GetCurrentProcess(), &thunk->u1.Function, sizeof(void*));
                            return true;
                        }
                    }
                }
                thunk++;
                origThunk++;
            }
        }
        importDesc++;
    }

    return false;
}

// Background thread monitoring main window to ensure title and OpenPS icons stay applied
DWORD WINAPI WindowMonitorThread(LPVOID lpParam)
{
    HINSTANCE hInstance = (HINSTANCE)lpParam;
    HICON hIconBig   = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(2), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(2), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    for (int iteration = 0; iteration < 1200; ++iteration) {
        Sleep(250);

        if (g_hMainWnd == nullptr || !IsWindow(g_hMainWnd) || !IsMainApplicationFrame(g_hMainWnd)) {
            HWND hWnd = FindWindowW(L"wxWindowNR", nullptr);
            while (hWnd != nullptr) {
                if (IsMainApplicationFrame(hWnd)) {
                    g_hMainWnd = hWnd;
                    break;
                }
                hWnd = FindWindowExW(nullptr, hWnd, L"wxWindowNR", nullptr);
            }
        }

        if (g_hMainWnd != nullptr && IsWindow(g_hMainWnd) && IsWindowVisible(g_hMainWnd)) {
            wchar_t currentTitle[512] = { 0 };
            GetWindowTextW(g_hMainWnd, currentTitle, 511);
            std::wstring expectedTitle = FormatBrandTitle(g_hMainWnd, currentTitle);
            if (expectedTitle != currentTitle) {
                SetWindowTextW(g_hMainWnd, expectedTitle.c_str());
            }

            if (hIconBig)   SendMessageW(g_hMainWnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
            if (hIconSmall) SendMessageW(g_hMainWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
    }
    return 0;
}

// Tells the user once per DLL build that the PrusaSlicer.dll specific features are off
static void WarnUnsupportedDllOnce(HMODULE hDll)
{
    uint32_t ts = 0, size = 0;
    dll::ReadPeIdentity(hDll, ts, size);
    wchar_t id[32] = { 0 };
    swprintf_s(id, L"%08X-%08X", ts, size);
    std::wstring ini = openps::sync::SyncUserConfig::GetIniPath();
    wchar_t seen[32] = { 0 };
    GetPrivateProfileStringW(L"Launcher", L"WarnedDll", L"", seen, 31, ini.c_str());
    if (wcscmp(seen, id) == 0) return;
    WritePrivateProfileStringW(L"Launcher", L"WarnedDll", id, ini.c_str());
    std::wstring msg = Fmt(i18n::S_DLL_MISMATCH, { dll::kSupportedVersion });
    ::MessageBoxW(nullptr, msg.c_str(), APP_BRAND_NAME, MB_ICONWARNING | MB_OK);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

    int argc = 0;
    wchar_t **argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

    std::vector<wchar_t*> argv_extended;
    for (int i = 0; i < argc; ++i) {
        argv_extended.emplace_back(argv[i]);
    }
    argv_extended.emplace_back(nullptr);

    wchar_t exe_dir[MAX_PATH + 1] = { 0 };
    ::GetModuleFileNameW(nullptr, exe_dir, MAX_PATH);
    wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
    _wsplitpath_s(exe_dir, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);
    _wmakepath_s(exe_dir, drive, dir, nullptr, nullptr);

    ::SetDllDirectoryW(exe_dir);

    wchar_t dll_path[MAX_PATH + 1] = { 0 };
    wcscpy_s(dll_path, exe_dir);
    wcscat_s(dll_path, L"PrusaSlicer.dll");

    g_hPrusaDll = ::LoadLibraryExW(dll_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_hPrusaDll) {
        g_hPrusaDll = ::LoadLibraryW(dll_path);
    }

    if (!g_hPrusaDll) {
        DWORD err = ::GetLastError();
        std::wstring msg = Fmt(i18n::S_ERR_LOAD_DLL, { dll_path, std::to_wstring((unsigned long)err) });
        ::MessageBoxW(nullptr, msg.c_str(), Tr(i18n::S_ERR_TITLE), MB_ICONERROR | MB_OK);
        return -1;
    }

    // Features that call into PrusaSlicer.dll are enabled only for the build they were made for
    dll::Module()    = g_hPrusaDll;
    dll::Supported() = dll::IsSupportedPrusaSlicerDll(g_hPrusaDll);
    if (dll::Supported()) {
        PatchBrandingAndModernTheme(g_hPrusaDll);
    } else {
        WarnUnsupportedDllOnce(g_hPrusaDll);
    }

    // Install Win32 IAT Hooks on PrusaSlicer.dll AND wxmsw32u_core_vc_custom.dll (if separate)
    InstallIATHook(g_hPrusaDll, "USER32.dll",   "CreateWindowExW",  (void*)Hook_CreateWindowExW,            (void**)&OriginalCreateWindowExW);
    InstallIATHook(g_hPrusaDll, "USER32.dll",   "SetWindowTextW",   (void*)Hook_SetWindowTextW,             (void**)&OriginalSetWindowTextW);
    InstallIATHook(g_hPrusaDll, "USER32.dll",   "TrackPopupMenu",   (void*)Hook_TrackPopupMenu,             (void**)&OriginalTrackPopupMenu);
    InstallIATHook(g_hPrusaDll, "USER32.dll",   "TrackPopupMenuEx", (void*)Hook_TrackPopupMenuEx,           (void**)&OriginalTrackPopupMenuEx);
    InstallIATHook(g_hPrusaDll, "COMDLG32.dll", "ChooseColorW",     (void*)openps::sync::Hook_ChooseColorW, (void**)&openps::sync::OriginalChooseColorW);

    Slic3rMainFunc slic3r_main = (Slic3rMainFunc)::GetProcAddress(g_hPrusaDll, "slic3r_main");
    if (!slic3r_main) {
        std::wstring msg = Fmt(i18n::S_ERR_ENTRY, { dll_path });
        ::MessageBoxW(nullptr, msg.c_str(), Tr(i18n::S_ERR_TITLE), MB_ICONERROR | MB_OK);
        return -1;
    }

    HANDLE hThread = CreateThread(nullptr, 0, WindowMonitorThread, (LPVOID)hInstance, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    }

    int result = slic3r_main((int)argv_extended.size() - 1, argv_extended.data());
    openps::sync::SyncTab_Shutdown();
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
    }
    return result;
}
