// SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// PrusaSlicerDll.hpp - Everything OpenSlicer knows about the internals of PrusaSlicer.dll
//
// OpenSlicer does not rebuild PrusaSlicer: OpenPS.exe loads the official PrusaSlicer.dll
// (Windows x64 release 2.9.7) and calls a few of its C++ functions directly. Their
// addresses (RVA = offset from the DLL base) and the object layouts below were read
// from that exact binary and are only valid for it.
//
// IsSupportedPrusaSlicerDll() compares the PE header of the loaded DLL with the
// supported build. When it does not match, every feature that uses this file is
// switched off and PrusaSlicer runs unmodified. See doc/openslicer/UPDATING_PRUSASLICER.md
// to port OpenSlicer to another PrusaSlicer release.
// ============================================================================
#pragma once
#ifndef OPENPS_PRUSASLICER_DLL_HPP
#define OPENPS_PRUSASLICER_DLL_HPP

#include <windows.h>
#include <cstdint>

namespace openps::dll {

// --- Supported build ------------------------------------------------------------
inline constexpr wchar_t  kSupportedVersion[]   = L"2.9.7";
inline constexpr uint32_t kSupportedTimeStamp   = 0x6AE86654; // IMAGE_FILE_HEADER::TimeDateStamp
inline constexpr uint32_t kSupportedSizeOfImage = 0x05C9F000; // IMAGE_OPTIONAL_HEADER::SizeOfImage

// --- Functions (RVA) ------------------------------------------------------------
inline constexpr uintptr_t kRvaWxGetApp                   = 0x1722E30; // GUI_App& wxGetApp()
inline constexpr uintptr_t kRvaGuiAppPlater               = 0x1718D10; // Plater* GUI_App::plater()
inline constexpr uintptr_t kRvaGuiAppSelectFilamentConnect = 0x171C5F0; // GUI_App::select_filament_from_connect(const std::string&)
inline constexpr uintptr_t kRvaPlaterSelectView           = 0x1927C80; // Plater::select_view(const std::string&)
inline constexpr uintptr_t kRvaPlaterGetCamera            = 0x19034C0; // Camera& Plater::get_camera()
inline constexpr uintptr_t kRvaCanvasZoomToBed            = 0x19A7B60; // GLCanvas3D::zoom_to_bed()
inline constexpr uintptr_t kRvaSelectionGetBoundingBox    = 0x1BE5E10; // const BoundingBoxf3& Selection::get_bounding_box()
inline constexpr uintptr_t kRvaCameraSetTarget            = 0x1D80190; // Camera::set_target(const Vec3d&)
inline constexpr uintptr_t kRvaGetBedTranslation          = 0x02E03C0; // Vec2d MultipleBeds::get_bed_translation(int)

// --- Global data (RVA) ----------------------------------------------------------
inline constexpr uintptr_t kRvaMultipleBeds               = 0x564F580; // Slic3r::s_multiple_beds
inline constexpr uintptr_t kRvaActiveBedIndex             = 0x564F584; // s_multiple_beds.m_active_bed

// --- Object layouts (byte offsets) ----------------------------------------------
inline constexpr size_t kOffPlaterPriv        = 0x270;  // Plater::p
inline constexpr size_t kOffPrivView3D        = 0x1640; // Plater::priv::view3D
inline constexpr size_t kOffView3DCanvas      = 0x278;  // View3D::m_canvas
inline constexpr size_t kOffCanvasBed         = 0x78;   // GLCanvas3D::m_bed (Bed3D&)
inline constexpr size_t kOffCanvasSelection   = 0x1660; // GLCanvas3D::m_selection
inline constexpr size_t kOffCanvasSelType     = 0x1673; // Selection::m_type (Invalid=0, Empty=1, ...)
inline constexpr size_t kOffCanvasDirty       = 0x1EE4; // GLCanvas3D::m_dirty
inline constexpr size_t kOffBedBoxMinX        = 0x60;   // Bed3D build volume bounding box (BoundingBoxf3)
inline constexpr size_t kOffBedBoxMinY        = 0x68;
inline constexpr size_t kOffBedBoxMaxX        = 0x78;
inline constexpr size_t kOffBedBoxMaxY        = 0x80;
inline constexpr size_t kOffBedBoxDefined     = 0x90;
inline constexpr size_t kOffCameraTarget      = 0x08;   // Camera::m_target (Vec3d)
inline constexpr size_t kOffCameraViewMatrix  = 0x50;   // Camera::m_view_matrix (column-major 4x4 doubles)

inline bool ReadPeIdentity(HMODULE hDll, uint32_t& timeStamp, uint32_t& sizeOfImage) {
    if (!hDll) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(hDll);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(hDll) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    timeStamp   = nt->FileHeader.TimeDateStamp;
    sizeOfImage = nt->OptionalHeader.SizeOfImage;
    return true;
}

inline bool IsSupportedPrusaSlicerDll(HMODULE hDll) {
    uint32_t ts = 0, size = 0;
    return ReadPeIdentity(hDll, ts, size) && ts == kSupportedTimeStamp && size == kSupportedSizeOfImage;
}

// Set once by OpenPS_launcher.cpp right after LoadLibrary, read everywhere else
inline HMODULE& Module() {
    static HMODULE h = nullptr;
    return h;
}

inline bool& Supported() {
    static bool ok = false;
    return ok;
}

// Address inside the DLL, or nullptr when the DLL is missing or not the supported build
inline BYTE* At(uintptr_t rva) {
    return (Module() && Supported()) ? reinterpret_cast<BYTE*>(Module()) + rva : nullptr;
}

} // namespace openps::dll

#endif // OPENPS_PRUSASLICER_DLL_HPP
