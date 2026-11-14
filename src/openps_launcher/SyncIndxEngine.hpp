// SPDX-License-Identifier: AGPL-3.0-or-later
// ============================================================================
// SyncIndxEngine.hpp - Data model and Prusa Connect client of the "Sync" tab
//
// Everything that talks to the network lives here and runs on one worker thread;
// results are posted to the Sync tab window with WM_OPENPS_SYNC_EVENT.
//
// Prusa Connect is used exactly like its own web application does:
//   REST    GET  https://connect.prusa3d.com/slicer/v2/printers      printers of the account
//           GET  https://connect.prusa3d.com/app/printers/{uuid}     printer details
//           GET  https://connect.prusa3d.com/app/users/teams/{id}    organization of a team
//           POST https://connect.prusa3d.com/app/printers/{uuid}/commands/sync   UNLOAD_FILAMENT
//   GraphQL POST https://connect-api.prusa3d.com/graphql
//           GetPrinterFffTelemetry   tools, nozzles and assigned materials ("Tools" tab)
//           WarehouseMaterials       material catalogue ("Assign a material" dialog)
//           MaterialBrands           brand identifiers
//           CreateCustomMaterial     custom material (same identifiers as the Connect form)
//           UpdatePrinterMaterial    assigns a material to a tool
//
// Authentication reuses the Prusa Account session of PrusaSlicer: the access token
// that PrusaSlicer stores in the Windows Credential Manager is read, never written.
// PrusaSlicer itself keeps it refreshed while it runs.
// ============================================================================
#pragma once
#ifndef OPENPS_SYNC_INDX_ENGINE_HPP
#define OPENPS_SYNC_INDX_ENGINE_HPP

#include <windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <shlobj.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "SyncIndxI18n.hpp"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace openps::sync {

using i18n::Tr;
using i18n::Fmt;

inline constexpr int  kToolCount           = 8; // CORE One INDX: up to 8 tools
inline constexpr UINT WM_OPENPS_SYNC_EVENT = (WM_APP + 0x530);

// Event codes posted with WM_OPENPS_SYNC_EVENT (wParam)
inline constexpr WPARAM SYNC_EVT_REFRESH_DONE   = 1;
inline constexpr WPARAM SYNC_EVT_STATUS_TEXT    = 3;
inline constexpr WPARAM SYNC_EVT_MATERIALS_DONE = 4; // lParam = request sequence number
inline constexpr WPARAM SYNC_EVT_ACTION_DONE    = 5; // lParam = 1 on success

// ----------------------------------------------------------------------------
// String helpers
// ----------------------------------------------------------------------------
inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n);
    return out;
}

inline std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], n, nullptr, nullptr);
    return out;
}

inline std::string ToUpperAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    }
    return s;
}

inline std::string TrimAscii(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= ' ') ++a;
    while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
    return s.substr(a, b - a);
}

inline bool EqualsNoCase(const std::string& a, const std::string& b) {
    return ToUpperAscii(TrimAscii(a)) == ToUpperAscii(TrimAscii(b));
}

// ----------------------------------------------------------------------------
// Color
// ----------------------------------------------------------------------------
struct Rgb {
    uint8_t r = 216;
    uint8_t g = 222;
    uint8_t b = 230;
    bool    assigned = false; // false = no color known

    Rgb() = default;
    constexpr Rgb(uint8_t R, uint8_t G, uint8_t B, bool isAssigned = true)
        : r(R), g(G), b(B), assigned(isAssigned) {}

    COLORREF ToColorRef() const { return RGB(r, g, b); }

    static Rgb FromColorRef(COLORREF c, bool isAssigned = true) {
        return Rgb(GetRValue(c), GetGValue(c), GetBValue(c), isAssigned);
    }

    std::string ToHex() const {
        char buf[16]{};
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", (unsigned)r, (unsigned)g, (unsigned)b);
        return buf;
    }

    std::wstring ToHexW() const { return Utf8ToWide(ToHex()); }

    static int HexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // "#RRGGBB" or "#RRGGBBAA" (alpha ignored)
    static Rgb FromHex(const std::string& raw, bool* okOut = nullptr) {
        std::string s = TrimAscii(raw);
        if (!s.empty() && s[0] == '#') s.erase(0, 1);
        if (s.size() == 6 || s.size() == 8) {
            int v[6];
            bool ok = true;
            for (int i = 0; i < 6; ++i) {
                v[i] = HexVal(s[(size_t)i]);
                if (v[i] < 0) ok = false;
            }
            if (ok) {
                if (okOut) *okOut = true;
                return Rgb((uint8_t)((v[0] << 4) | v[1]), (uint8_t)((v[2] << 4) | v[3]), (uint8_t)((v[4] << 4) | v[5]), true);
            }
        }
        if (okOut) *okOut = false;
        return Rgb(216, 222, 230, false);
    }

    double Luminance() const { return 0.299 * r + 0.587 * g + 0.114 * b; }
};

// "Redmean" perceptual color distance (0 = identical, ~765 = black vs white)
inline double RedmeanDistance(const Rgb& a, const Rgb& b) {
    double rbar = 0.5 * ((double)a.r + (double)b.r);
    double dr = (double)a.r - (double)b.r;
    double dg = (double)a.g - (double)b.g;
    double db = (double)a.b - (double)b.b;
    return std::sqrt((2.0 + rbar / 256.0) * dr * dr + 4.0 * dg * dg + (2.0 + (255.0 - rbar) / 256.0) * db * db);
}

// Two colors closer than this are considered the same filament color
inline constexpr double kSameColorDistance = 18.0;

// ----------------------------------------------------------------------------
// Material types known by the Prusa firmware (Buddy) and their fallback colors
// ----------------------------------------------------------------------------
struct BuddyMaterialDef {
    const char* code;
    Rgb         fallbackColor;
};

inline constexpr int kBuddyMaterialCount = 11;
inline constexpr BuddyMaterialDef kBuddyMaterials[kBuddyMaterialCount] = {
    { "PLA",  Rgb(0xEA, 0x67, 0x0C) },
    { "PETG", Rgb(0x1E, 0x1E, 0x1E) },
    { "ASA",  Rgb(0xF0, 0xF3, 0xF8) },
    { "ABS",  Rgb(0x84, 0x84, 0x84) },
    { "PC",   Rgb(0xB8, 0xBC, 0xCA) },
    { "FLEX", Rgb(0x0F, 0x62, 0xFE) },
    { "PA",   Rgb(0xE2, 0xE8, 0xF0) },
    { "PVA",  Rgb(0xF5, 0xD0, 0x00) },
    { "HIPS", Rgb(0xFF, 0xFF, 0xFF) },
    { "PP",   Rgb(0xE2, 0xE8, 0xF0) },
    { "PVB",  Rgb(0x7C, 0x3A, 0xED) }
};

inline const BuddyMaterialDef& GetBuddyMaterialDef(const std::string& code) {
    std::string u = ToUpperAscii(TrimAscii(code));
    for (const auto& m : kBuddyMaterials) {
        if (u == m.code) return m;
    }
    return kBuddyMaterials[0];
}

// Maps an abbreviation ("TPU", "PLA+") or a full PrusaSlicer preset name
// ("Prusament PETG @COREONEINDX HF0.4") to one of the Buddy material types
inline std::string NormalizeBuddyMaterial(const std::string& rawInput) {
    std::string u = ToUpperAscii(TrimAscii(rawInput));
    if (u.empty() || u == "---" || u == "NONE" || u == "UNLOADED") return "";

    if (u == "PLA" || u == "PLA+" || u == "PLA-CF" || u == "SILK" || u == "RPLA") return "PLA";
    if (u == "PETG" || u == "PET" || u == "PETG-CF" || u == "RPETG" || u == "PCTG") return "PETG";
    if (u == "ASA" || u == "ASA-CF") return "ASA";
    if (u == "ABS" || u == "ABS+" || u == "ABS-T") return "ABS";
    if (u == "PC" || u == "PC-CF" || u == "PCCF") return "PC";
    if (u == "FLEX" || u == "TPU" || u == "TPE" || u == "PEBA") return "FLEX";
    if (u == "PA" || u == "PA11" || u == "PA12" || u == "PA6" || u == "NYLON" || u == "PA-CF" || u == "PA11-CF") return "PA";
    if (u == "PVA" || u == "BVOH") return "PVA";
    if (u == "HIPS") return "HIPS";
    if (u == "PP" || u == "PP-CF") return "PP";
    if (u == "PVB") return "PVB";

    // Whole-word scan inside preset names (order matters: PETG before PET, PVA/PVB before PA)
    auto hasWord = [&](const char* token) -> bool {
        size_t tlen = strlen(token);
        size_t pos = 0;
        while ((pos = u.find(token, pos)) != std::string::npos) {
            bool leftOk = (pos == 0) || !isalnum((unsigned char)u[pos - 1]);
            size_t end = pos + tlen;
            bool rightOk = (end >= u.size()) || !isalpha((unsigned char)u[end]);
            if (leftOk && rightOk) return true;
            pos += 1;
        }
        return false;
    };

    if (hasWord("PETG") || hasWord("PCTG") || hasWord("PET")) return "PETG";
    if (hasWord("ASA")) return "ASA";
    if (hasWord("ABS")) return "ABS";
    if (hasWord("TPU") || hasWord("TPE") || hasWord("FLEX") || hasWord("PEBA")) return "FLEX";
    if (hasWord("PVA") || hasWord("BVOH")) return "PVA";
    if (hasWord("PVB")) return "PVB";
    if (hasWord("HIPS")) return "HIPS";
    if (hasWord("PC") || u.find("POLYCARBONATE") != std::string::npos) return "PC";
    if (hasWord("PA11") || hasWord("PA12") || hasWord("PA6") || hasWord("PA") ||
        u.find("NYLON") != std::string::npos || u.find("POLYAMIDE") != std::string::npos) return "PA";
    if (hasWord("PP") || u.find("POLYPROPYLENE") != std::string::npos) return "PP";
    return "PLA";
}

// ----------------------------------------------------------------------------
// Data model
// ----------------------------------------------------------------------------
// One tool of the printer, as shown in the Prusa Connect "Tools" tab
struct MachineSlot {
    bool        loaded           = false;  // a material is reported or assigned
    std::string material         = "PLA";  // Buddy material type (NormalizeBuddyMaterial)
    std::string abbrev;                    // type displayed by Connect ("PLA", "PA11CF"...)
    Rgb         color            = Rgb(216, 222, 230, false);
    double      nozzleDiameter   = 0.4;
    bool        highFlow         = false;
    bool        hardened         = false;
    bool        hasWeight        = false;  // remaining weight known
    double      nominalWeightG   = 1000.0;
    double      consumedWeightG  = 0.0;
    double      remainingWeightG = 1000.0;
    std::string toolId;                    // Connect tool id exactly as returned by the API
    std::string materialName;              // "PLA Blend Royal Blue"
    std::string brandName;                 // "Prusament"
    std::string materialUuid;              // empty = no material assigned in Connect
    bool        isFromNfc        = false;  // read from the spool NFC tag (cannot be changed)
    bool        active           = false;  // currently in the print head
};

struct MachineSnapshot {
    std::string uuid;
    std::string name;
    std::string model;
    std::string firmware;
    std::string connectState;   // IDLE, READY, PRINTING, BUSY, ATTENTION, OFFLINE...
    std::string teamId;
    std::string organizationId;
    bool        cloudOnline = false;
    int         toolCount   = kToolCount;
    std::array<MachineSlot, kToolCount> tools{};
};

// One filament (extruder) of the active PrusaSlicer printer profile
struct SlicerSlot {
    bool        valid = false;
    std::string presetName;
    std::string material = "PLA";
    Rgb         color    = Rgb(234, 103, 12, true);
};

// One entry of the Prusa Connect material catalogue
struct MaterialItem {
    std::string uuid;
    std::string name;
    std::string brand;
    std::string type;
    std::string abbreviation;
    Rgb         color      = Rgb(216, 222, 230, false);
    bool        isCustom   = false;
    int         spoolCount = 0;
};

// One slicer filament to record on a printer tool in Prusa Connect
struct SlicerAssignSpec {
    int         toolIdx = -1;
    std::string brand;   // "Prusament", "Generic"...
    std::string type;    // "PLA", "PETG"...
    std::string name;    // material name without the brand ("PLA", "PLA Silk")
    Rgb         color;
};

// "Prusament PLA Blend @COREONEINDX HF0.4" -> brand "Prusament", type "PLA", name "PLA Blend"
inline SlicerAssignSpec SpecFromSlicerPreset(int toolIdx, const std::string& presetName,
                                             const std::string& normMaterial, const Rgb& color) {
    SlicerAssignSpec sp;
    sp.toolIdx = toolIdx;
    sp.color   = color;
    sp.color.assigned = true;

    std::string base = presetName;
    size_t at = base.find(" @");
    if (at != std::string::npos) base = base.substr(0, at);
    base = TrimAscii(base);

    std::vector<std::string> words;
    std::istringstream iss(base);
    for (std::string w; iss >> w;) words.push_back(w);

    static const char* kTypes[] = { "PETG", "PCTG", "PLA", "ASA", "ABS", "PC", "PA11", "PA12", "PA6", "PA",
                                    "TPU", "TPE", "FLEX", "PVA", "BVOH", "HIPS", "PP", "PVB", "PET" };
    int typeIdx = -1;
    for (size_t i = 0; i < words.size() && typeIdx < 0; ++i) {
        std::string u = ToUpperAscii(words[i]);
        size_t dash = u.find('-');
        std::string head = (dash == std::string::npos) ? u : u.substr(0, dash);
        for (const char* t : kTypes) {
            if (head == t) { typeIdx = (int)i; sp.type = t; break; }
        }
    }
    if (sp.type.empty()) sp.type = normMaterial.empty() ? "PLA" : normMaterial;

    if (typeIdx > 0) {
        for (int i = 0; i < typeIdx; ++i) sp.brand += (i ? " " : "") + words[(size_t)i];
        for (size_t i = (size_t)typeIdx; i < words.size(); ++i) sp.name += (i > (size_t)typeIdx ? " " : "") + words[i];
    } else {
        sp.brand = "Generic";
        sp.name  = base.empty() ? sp.type : base;
    }
    if (sp.brand.empty()) sp.brand = "Generic";
    if (sp.name.empty())  sp.name  = sp.type;
    return sp;
}

// ----------------------------------------------------------------------------
// Minimal JSON parser (RFC 8259), locale independent
// ----------------------------------------------------------------------------
struct JsonVal {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool                           bVal = false;
    double                         nVal = 0.0;
    std::string                    sVal;
    std::vector<JsonVal>           aVal;
    std::map<std::string, JsonVal> oVal;

    bool IsNull()   const { return type == Null; }
    bool IsObject() const { return type == Object; }
    bool IsArray()  const { return type == Array; }
    bool IsString() const { return type == String; }
    bool IsNumber() const { return type == Number; }

    const JsonVal& Get(const char* key) const {
        static const JsonVal kEmpty{};
        if (type != Object) return kEmpty;
        auto it = oVal.find(key);
        return (it != oVal.end()) ? it->second : kEmpty;
    }

    std::string Str(const char* key, const std::string& def = "") const {
        const JsonVal& v = Get(key);
        return (v.type == String) ? v.sVal : def;
    }

    double Num(const char* key, double def = 0.0) const {
        const JsonVal& v = Get(key);
        return (v.type == Number) ? v.nVal : def;
    }

    bool Boolean(const char* key, bool def = false) const {
        const JsonVal& v = Get(key);
        return (v.type == Bool) ? v.bVal : def;
    }
};

class MiniJsonParser {
public:
    static JsonVal Parse(const std::string& text) {
        MiniJsonParser p(text);
        return p.ParseValue(0);
    }

private:
    static constexpr int kMaxDepth = 128;
    const std::string& m_s;
    size_t             m_pos = 0;

    explicit MiniJsonParser(const std::string& s) : m_s(s) {}

    void SkipWs() {
        while (m_pos < m_s.size()) {
            char c = m_s[m_pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++m_pos;
            else break;
        }
    }

    bool Match(const char* kw) {
        size_t len = strlen(kw);
        if (m_pos + len <= m_s.size() && memcmp(m_s.data() + m_pos, kw, len) == 0) {
            m_pos += len;
            return true;
        }
        return false;
    }

    static void AppendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back((char)cp);
        } else if (cp <= 0x7FF) {
            out.push_back((char)(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back((char)(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    uint32_t ReadHex4() {
        uint32_t cp = 0;
        if (m_pos + 4 > m_s.size()) { m_pos = m_s.size(); return 0xFFFD; }
        for (int k = 0; k < 4; ++k) {
            int hv = Rgb::HexVal(m_s[m_pos + (size_t)k]);
            cp = (cp << 4) | (hv >= 0 ? (uint32_t)hv : 0u);
        }
        m_pos += 4;
        return cp;
    }

    std::string ParseRawString() {
        std::string out;
        if (m_pos >= m_s.size() || m_s[m_pos] != '"') return out;
        ++m_pos;
        while (m_pos < m_s.size()) {
            char c = m_s[m_pos++];
            if (c == '"') break;
            if (c != '\\' || m_pos >= m_s.size()) {
                out.push_back(c);
                continue;
            }
            char e = m_s[m_pos++];
            switch (e) {
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                uint32_t cp = ReadHex4();
                // UTF-16 surrogate pair
                if (cp >= 0xD800 && cp <= 0xDBFF && m_pos + 6 <= m_s.size() &&
                    m_s[m_pos] == '\\' && m_s[m_pos + 1] == 'u') {
                    m_pos += 2;
                    uint32_t lo = ReadHex4();
                    cp = (lo >= 0xDC00 && lo <= 0xDFFF) ? (0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00)) : 0xFFFD;
                } else if (cp >= 0xD800 && cp <= 0xDFFF) {
                    cp = 0xFFFD;
                }
                AppendUtf8(out, cp);
                break;
            }
            default: out.push_back(e); break; // \" \\ \/
            }
        }
        return out;
    }

    JsonVal ParseValue(int depth) {
        SkipWs();
        JsonVal v;
        if (m_pos >= m_s.size() || depth > kMaxDepth) return v;
        char c = m_s[m_pos];
        if (c == '{') {
            v.type = JsonVal::Object;
            ++m_pos;
            SkipWs();
            if (m_pos < m_s.size() && m_s[m_pos] == '}') { ++m_pos; return v; }
            while (m_pos < m_s.size()) {
                SkipWs();
                if (m_pos >= m_s.size() || m_s[m_pos] != '"') break;
                std::string key = ParseRawString();
                SkipWs();
                if (m_pos < m_s.size() && m_s[m_pos] == ':') ++m_pos;
                v.oVal[key] = ParseValue(depth + 1);
                SkipWs();
                if (m_pos < m_s.size() && m_s[m_pos] == ',') { ++m_pos; continue; }
                if (m_pos < m_s.size() && m_s[m_pos] == '}') ++m_pos;
                break;
            }
            return v;
        }
        if (c == '[') {
            v.type = JsonVal::Array;
            ++m_pos;
            SkipWs();
            if (m_pos < m_s.size() && m_s[m_pos] == ']') { ++m_pos; return v; }
            while (m_pos < m_s.size()) {
                v.aVal.push_back(ParseValue(depth + 1));
                SkipWs();
                if (m_pos < m_s.size() && m_s[m_pos] == ',') { ++m_pos; continue; }
                if (m_pos < m_s.size() && m_s[m_pos] == ']') ++m_pos;
                break;
            }
            return v;
        }
        if (c == '"') {
            v.type = JsonVal::String;
            v.sVal = ParseRawString();
            return v;
        }
        if (Match("true"))  { v.type = JsonVal::Bool; v.bVal = true;  return v; }
        if (Match("false")) { v.type = JsonVal::Bool; v.bVal = false; return v; }
        if (Match("null"))  { return v; }

        // Number, parsed by hand so that the process locale (',' decimal separator) does not matter
        bool neg = false;
        if (m_s[m_pos] == '-') { neg = true; ++m_pos; }
        double val = 0.0;
        bool digits = false;
        while (m_pos < m_s.size() && m_s[m_pos] >= '0' && m_s[m_pos] <= '9') {
            val = val * 10.0 + (m_s[m_pos++] - '0');
            digits = true;
        }
        if (m_pos < m_s.size() && m_s[m_pos] == '.') {
            ++m_pos;
            double place = 0.1;
            while (m_pos < m_s.size() && m_s[m_pos] >= '0' && m_s[m_pos] <= '9') {
                val += (m_s[m_pos++] - '0') * place;
                place *= 0.1;
                digits = true;
            }
        }
        if (digits && m_pos < m_s.size() && (m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
            ++m_pos;
            bool expNeg = false;
            if (m_pos < m_s.size() && (m_s[m_pos] == '+' || m_s[m_pos] == '-')) expNeg = (m_s[m_pos++] == '-');
            int expVal = 0;
            while (m_pos < m_s.size() && m_s[m_pos] >= '0' && m_s[m_pos] <= '9') {
                expVal = std::min(400, expVal * 10 + (m_s[m_pos++] - '0'));
            }
            val *= std::pow(10.0, expNeg ? -expVal : expVal);
        }
        if (digits) {
            v.type = JsonVal::Number;
            v.nVal = neg ? -val : val;
        } else {
            m_pos = m_s.size(); // invalid token: stop parsing
        }
        return v;
    }
};

inline std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8]{};
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                out += buf;
            } else {
                out.push_back((char)c);
            }
        }
    }
    return out;
}

inline std::string JsonQuote(const std::string& s) { return "\"" + JsonEscape(s) + "\""; }

inline std::string JsonScalarToString(const JsonVal& v) {
    if (v.IsString()) return v.sVal;
    if (v.IsNumber()) {
        char buf[32]{};
        snprintf(buf, sizeof(buf), "%lld", (long long)std::llround(v.nVal));
        return buf;
    }
    return "";
}

// ----------------------------------------------------------------------------
// Settings (%APPDATA%\PrusaSlicer\openps_sync.ini)
// ----------------------------------------------------------------------------
struct SyncUserConfig {
    std::string selectedUuid; // printer shown in the Sync tab

    static std::wstring GetIniPath() {
        wchar_t appData[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
            std::wstring dir = std::wstring(appData) + L"\\PrusaSlicer";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir + L"\\openps_sync.ini";
        }
        return L"openps_sync.ini";
    }

    void Load() {
        wchar_t buf[256]{};
        GetPrivateProfileStringW(L"Sync", L"SelectedUuid", L"", buf, 255, GetIniPath().c_str());
        selectedUuid = WideToUtf8(buf);
    }

    void Save() const {
        WritePrivateProfileStringW(L"Sync", L"SelectedUuid", Utf8ToWide(selectedUuid).c_str(), GetIniPath().c_str());
    }
};

// ----------------------------------------------------------------------------
// HTTPS client (WinHTTP)
// ----------------------------------------------------------------------------
struct HttpResponse {
    DWORD       statusCode = 0; // 0 = network error
    std::string body;
    bool Ok() const { return statusCode >= 200 && statusCode < 300; }
};

inline HttpResponse HttpsRequest(const std::wstring& host, const std::wstring& method, const std::wstring& path,
                                 const std::wstring& extraHeaders, const std::string& requestBody,
                                 int connectTimeoutMs = 4000, int recvTimeoutMs = 9000)
{
    HttpResponse res;
    HINTERNET hSession = WinHttpOpen(L"OpenSlicer-Sync/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return res;
    WinHttpSetTimeouts(hSession, connectTimeoutMs, connectTimeoutMs, recvTimeoutMs, recvTimeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, method.c_str(), path.c_str(), nullptr,
                                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                       WINHTTP_FLAG_SECURE)
                                  : nullptr;
    if (hRequest) {
        LPCWSTR hdrs   = extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str();
        DWORD   hdrLen = extraHeaders.empty() ? 0 : (DWORD)-1L;
        LPVOID  body   = requestBody.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)requestBody.data();
        DWORD   len    = (DWORD)requestBody.size();
        if (WinHttpSendRequest(hRequest, hdrs, hdrLen, body, len, len, 0) && WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD code = 0, codeSz = sizeof(code);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeSz, WINHTTP_NO_HEADER_INDEX)) {
                res.statusCode = code;
            }
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                std::string chunk(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, &chunk[0], avail, &read) || read == 0) break;
                res.body.append(chunk.data(), read);
            }
        }
        WinHttpCloseHandle(hRequest);
    }
    if (hConnect) WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return res;
}

inline constexpr wchar_t kConnectHost[]    = L"connect.prusa3d.com";
inline constexpr wchar_t kConnectApiHost[] = L"connect-api.prusa3d.com";

// ----------------------------------------------------------------------------
// Prusa Account session of PrusaSlicer (read only)
//
// PrusaSlicer stores "access_token|refresh_token|next_timeout|pid" in the Windows
// Credential Manager (wxSecretStore, generic credential "<AppKey>/PrusaAccount/tokens").
// ----------------------------------------------------------------------------
struct PrusaSession {
    std::string accessToken;
    long long   expiresAt = 0; // unix time
};

inline bool ReadCredential(const wchar_t* targetName, PrusaSession& out) {
    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(targetName, CRED_TYPE_GENERIC, 0, &pCred) || !pCred) return false;
    std::string blob;
    if (pCred->CredentialBlob && pCred->CredentialBlobSize > 0) {
        blob.assign(reinterpret_cast<const char*>(pCred->CredentialBlob), pCred->CredentialBlobSize);
    }
    CredFree(pCred);
    while (!blob.empty() && blob.back() == '\0') blob.pop_back();

    size_t p1 = blob.find('|');
    std::string token = TrimAscii(blob.substr(0, p1));
    if (token.size() < 8) return false;
    for (unsigned char c : token) {
        if (c < 0x21 || c > 0x7E) return false; // not the UTF-8 layout written by wxSecretStore
    }
    out.accessToken = token;
    out.expiresAt   = 0;
    if (p1 != std::string::npos) {
        size_t p2 = blob.find('|', p1 + 1);
        if (p2 != std::string::npos) {
            out.expiresAt = std::atoll(blob.c_str() + p2 + 1);
        }
    }
    return true;
}

inline PrusaSession ReadPrusaSession() {
    PrusaSession best;
    auto consider = [&](const wchar_t* target) {
        PrusaSession s;
        if (ReadCredential(target, s) && (best.accessToken.empty() || s.expiresAt > best.expiresAt)) {
            best = s;
        }
    };
    consider(L"PrusaSlicer/PrusaAccount/tokens");

    DWORD count = 0;
    PCREDENTIALW* creds = nullptr;
    if (CredEnumerateW(L"*/PrusaAccount/tokens", 0, &count, &creds) && creds) {
        for (DWORD i = 0; i < count; ++i) {
            if (creds[i] && creds[i]->TargetName) consider(creds[i]->TargetName);
        }
        CredFree(creds);
    }
    return best;
}

// Current access token. When it is about to expire, waits a little for PrusaSlicer
// to refresh it (PrusaSlicer owns the session; OpenSlicer never refreshes it itself).
inline std::string CurrentAccessToken() {
    PrusaSession s = ReadPrusaSession();
    for (int i = 0; i < 6 && !s.accessToken.empty() && s.expiresAt > 0 &&
                    s.expiresAt <= (long long)std::time(nullptr) + 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        s = ReadPrusaSession();
    }
    return s.accessToken;
}

inline std::wstring BearerHeaders(const std::string& token, bool json) {
    std::wstring h = L"Authorization: Bearer " + Utf8ToWide(token) + L"\r\nAccept: application/json\r\n";
    if (json) h += L"Content-Type: application/json\r\n";
    return h;
}

inline HttpResponse ConnectGet(const std::string& token, const std::wstring& path) {
    return HttpsRequest(kConnectHost, L"GET", path, BearerHeaders(token, false), "");
}

// ----------------------------------------------------------------------------
// GraphQL operations (same documents as the Prusa Connect web application)
// ----------------------------------------------------------------------------
inline constexpr const char* kGqlToolsQuery =
    "query GetPrinterFffTelemetry($organizationId: UUID!, $printerId: UUID!) {"
    " printer { printer(organizationId: $organizationId, printerId: $printerId) {"
    " __typename"
    " ... on Printer { id fff { tools { id class type"
    " features { nozzleDiameter nozzleHighFlow nozzleHardened }"
    " telemetry { active }"
    " materialPackageInstance { __typename"
    " ... on FFFToolMaterialPackageInstance { uuid actualNettoFullWeight isFromNfc"
    " dynamicData { consumedWeight }"
    " package { nominalNettoFullWeight"
    " material { uuid name abbreviation brand { name } primaryColor { colorRgba } } } }"
    " ... on ToolMaterialPackageInstanceEmpty { reason } } } } }"
    " ... on ConnectError { message } } } }";

inline constexpr const char* kGqlMaterialsQuery =
    "query WarehouseMaterials($organizationId: String!, $first: Int, $materialClass: [MaterialClass!],"
    " $materialTypeOrAbbreviation: [String!], $search: String) {"
    " warehouse { materials(organizationId: $organizationId, first: $first, materialClass: $materialClass,"
    " materialTypeOrAbbreviation: $materialTypeOrAbbreviation, search: $search) {"
    " nodes { uuid isCustom spoolCount"
    " material { uuid name type abbreviation primaryColor { colorRgba } brand { name } } } } } }";

inline constexpr const char* kGqlBrandsQuery =
    "query MaterialBrands($first: Int, $after: String) { materialdb { brands(first: $first, after: $after) {"
    " pageInfo { hasNextPage endCursor } edges { node { uuid name } } } } }";

inline constexpr const char* kGqlCreateCustomMutation =
    "mutation CreateCustomMaterial($material: WarehouseMaterialInput!) {"
    " warehouse { createCustomMaterial(material: $material) { id } } }";

inline constexpr const char* kGqlAssignMutation =
    "mutation UpdatePrinterMaterial($organizationId: UUID!, $printerId: UUID!, $printer: PrinterUpdateInput!) {"
    " printer { updatePrinter(organizationId: $organizationId, printerId: $printerId, printer: $printer) {"
    " __typename"
    " ... on Printer { id }"
    " ... on ConnectError { message }"
    " ... on DuplicateMaterialPackageInstanceError { message } } } }";

inline constexpr const char* kGqlMeQuery = "query GetMe { authorization { me { id organizations { id } } } }";

inline HttpResponse ConnectGraphQl(const std::string& token, const char* query, const std::string& variablesJson) {
    std::string body = std::string("{\"query\":") + JsonQuote(query) + ",\"variables\":" +
                       (variablesJson.empty() ? std::string("{}") : variablesJson) + "}";
    return HttpsRequest(kConnectApiHost, L"POST", L"/graphql", BearerHeaders(token, true), body);
}

inline std::string GraphQlErrorMessage(const JsonVal& root) {
    const JsonVal& errs = root.Get("errors");
    return (errs.IsArray() && !errs.aVal.empty()) ? errs.aVal[0].Str("message") : "";
}

// ----------------------------------------------------------------------------
// Custom material identifiers, computed like the Prusa Connect warehouse form:
//   brand uuid    = UUIDv5(brandNamespace, utf8(brandName))
//   material uuid = UUIDv5(materialNamespace, bytes(brandUuid) + utf8(materialName))
// ----------------------------------------------------------------------------
inline constexpr const char* kConnectBrandNamespace    = "5269dfb7-1559-440a-85be-aba5f3eff2d2";
inline constexpr const char* kConnectMaterialNamespace = "616fc86d-7d99-4953-96c7-46d2836b9be9";

inline bool Sha1Digest(const std::string& data, unsigned char out[20]) {
    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0) return false;
    bool ok = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) >= 0 &&
              BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0) >= 0 &&
              BCryptFinishHash(hHash, out, 20, 0) >= 0;
    if (hHash) BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

inline std::string UuidToBytes(const std::string& uuid) {
    std::string hex;
    for (char c : uuid) {
        if (c != '-') hex.push_back(c);
    }
    std::string bytes;
    for (size_t i = 0; i + 1 < hex.size() && bytes.size() < 16; i += 2) {
        int hi = Rgb::HexVal(hex[i]), lo = Rgb::HexVal(hex[i + 1]);
        bytes.push_back((char)(((hi < 0 ? 0 : hi) << 4) | (lo < 0 ? 0 : lo)));
    }
    bytes.resize(16, '\0');
    return bytes;
}

inline std::string UuidV5(const char* ns, const std::string& name) {
    unsigned char h[20]{};
    if (!Sha1Digest(UuidToBytes(ns) + name, h)) return "";
    h[6] = (unsigned char)((h[6] & 0x0F) | 0x50);
    h[8] = (unsigned char)((h[8] & 0x3F) | 0x80);
    char buf[40]{};
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[11], h[12], h[13], h[14], h[15]);
    return buf;
}

// Printer states in which tools must not be changed (same rule as the Connect UI)
inline bool IsPrinterBusyState(const std::string& state) {
    std::string u = ToUpperAscii(TrimAscii(state));
    return u == "PRINTING" || u == "BUSY" || u == "PAUSED";
}

inline bool IsPrinterIdleState(const std::string& state) {
    std::string u = ToUpperAscii(TrimAscii(state));
    return u == "IDLE" || u == "FINISHED" || u == "READY" || u == "STOPPED";
}

// Nozzle / bed temperatures sent by Prusa Connect with UNLOAD_FILAMENT
inline void ConnectUnloadTemperatures(const std::string& materialType, int& nozzleC, int& bedC) {
    struct Row { const char* mat; int nozzle; int bed; };
    static const Row kRows[] = {
        { "PLA", 215, 60 }, { "PETG", 230, 85 }, { "ASA", 260, 100 }, { "ABS", 255, 100 },
        { "PC", 275, 100 }, { "FLEX", 240, 50 }, { "HIPS", 220, 100 }, { "PP", 240, 100 }
    };
    std::string u = ToUpperAscii(TrimAscii(materialType));
    for (const auto& r : kRows) {
        if (u == r.mat) { nozzleC = r.nozzle; bedC = r.bed; return; }
    }
    nozzleC = 230;
    bedC    = 0;
}

// ============================================================================
// SyncEngine: background worker doing every network call of the Sync tab
// ============================================================================
class SyncEngine {
public:
    SyncEngine() = default;
    ~SyncEngine() { Stop(); }
    SyncEngine(const SyncEngine&) = delete;
    SyncEngine& operator=(const SyncEngine&) = delete;

    void Start(HWND hNotifyWnd) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_hNotify = hNotifyWnd;
        if (m_running) return;
        m_stop    = false;
        m_running = true;
        m_worker  = std::thread([this]() { WorkerLoop(); });
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (!m_running) return;
            m_stop = true;
            m_queue.clear();
        }
        m_cv.notify_all();
        if (m_worker.joinable()) m_worker.join();
        std::lock_guard<std::mutex> lk(m_mutex);
        m_running = false;
    }

    bool IsBusy() const { return m_busy.load(); }

    void RequestRefresh(const SyncUserConfig& cfg) {
        Job j;
        j.kind   = Job::Refresh;
        j.config = cfg;
        Enqueue(std::move(j));
    }

    // Material catalogue of the "Assign a material" dialog; answer = SYNC_EVT_MATERIALS_DONE(seq)
    void RequestMaterials(const std::string& search, int seq) {
        Job j;
        j.kind   = Job::FetchMaterials;
        j.search = search;
        j.seq    = seq;
        Enqueue(std::move(j));
    }

    // "Assign" / "Change" of the Connect "Tools" tab; answer = SYNC_EVT_ACTION_DONE
    void RequestAssignMaterial(const SyncUserConfig& cfg, int toolIdx, const MaterialItem& item) {
        Job j;
        j.kind     = Job::AssignMaterial;
        j.config   = cfg;
        j.toolIdx  = toolIdx;
        j.material = item;
        Enqueue(std::move(j));
    }

    // Records slicer filaments on the printer tools in Prusa Connect: a catalogue material
    // when brand, type and color match, otherwise a custom material. Nothing is sent to the printer.
    void RequestAssignFromSlicer(const SyncUserConfig& cfg, const std::vector<SlicerAssignSpec>& specs) {
        Job j;
        j.kind   = Job::AssignFromSlicer;
        j.config = cfg;
        j.specs  = specs;
        Enqueue(std::move(j));
    }

    // "Unload" of the Connect "Tools" tab; answer = SYNC_EVT_ACTION_DONE
    void RequestUnload(const SyncUserConfig& cfg, int toolIdx) {
        Job j;
        j.kind    = Job::UnloadFilament;
        j.config  = cfg;
        j.toolIdx = toolIdx;
        Enqueue(std::move(j));
    }

    bool GetMaterialsCopy(int seq, std::vector<MaterialItem>& outItems, bool& outOk) const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (seq != m_materialsSeq) return false;
        outItems = m_materials;
        outOk    = m_materialsOk;
        return true;
    }

    void GetStateCopy(std::vector<MachineSnapshot>& outPrinters, int& outSelectedIdx,
                      std::wstring& outLastStatus, bool& outLastOk) const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        outPrinters    = m_printers;
        outSelectedIdx = m_selectedIdx;
        outLastStatus  = m_lastStatus;
        outLastOk      = m_lastOk;
    }

private:
    struct Job {
        enum Kind { Refresh, FetchMaterials, AssignMaterial, AssignFromSlicer, UnloadFilament } kind = Refresh;
        SyncUserConfig                config;
        std::string                   search;
        int                           seq     = 0;
        int                           toolIdx = -1;
        MaterialItem                  material;
        std::vector<SlicerAssignSpec> specs;
    };

    // Worker queue
    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
    std::deque<Job>         m_queue;
    std::thread             m_worker;
    HWND                    m_hNotify = nullptr;
    bool                    m_running = false;
    bool                    m_stop    = false;
    std::atomic<bool>       m_busy{ false };

    // State shared with the UI thread
    mutable std::mutex           m_stateMutex;
    std::vector<MachineSnapshot> m_printers;
    int                          m_selectedIdx  = -1;
    std::wstring                 m_lastStatus;
    bool                         m_lastOk       = true;
    std::vector<MaterialItem>    m_materials;
    int                          m_materialsSeq = -1;
    bool                         m_materialsOk  = false;

    // Worker thread only
    std::map<std::string, std::string> m_orgByTeam;
    std::map<std::string, std::string> m_brandUuids; // key = upper-case brand name
    bool                               m_brandsLoaded = false;

    void Enqueue(Job&& j) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            // A newer refresh / catalogue request replaces a pending one
            if (j.kind == Job::Refresh || j.kind == Job::FetchMaterials) {
                Job::Kind k = j.kind;
                m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                             [k](const Job& x) { return x.kind == k; }), m_queue.end());
            }
            m_queue.push_back(std::move(j));
        }
        m_cv.notify_one();
    }

    void PostUiEvent(WPARAM code, LPARAM lParam = 0) {
        HWND h = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            h = m_hNotify;
        }
        if (h && IsWindow(h)) PostMessageW(h, WM_OPENPS_SYNC_EVENT, code, lParam);
    }

    void SetStatus(const std::wstring& msg, bool ok) {
        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            m_lastStatus = msg;
            m_lastOk     = ok;
        }
        PostUiEvent(SYNC_EVT_STATUS_TEXT, ok ? 1 : 0);
    }

    bool GetActivePrinterCopy(MachineSnapshot& out) const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (m_selectedIdx < 0 || m_selectedIdx >= (int)m_printers.size()) return false;
        out = m_printers[(size_t)m_selectedIdx];
        return true;
    }

    static std::wstring PrinterLabel(const MachineSnapshot& p) {
        return Utf8ToWide(p.name.empty() ? p.model : p.name);
    }

    static std::wstring HttpErrorText(const HttpResponse& r) {
        return (r.statusCode == 0) ? std::wstring(Tr(i18n::S_ST_UNREACHABLE))
                                   : (L"HTTP " + std::to_wstring((unsigned long)r.statusCode));
    }

    // ------------------------------------------------------------------------
    // REST printer list / details
    // ------------------------------------------------------------------------
    static void PopulateToolsFromRest(const JsonVal& toolsObj, std::array<MachineSlot, kToolCount>& outTools) {
        for (int i = 0; i < kToolCount; ++i) {
            const JsonVal* tVal = nullptr;
            if (toolsObj.IsArray()) {
                if (i < (int)toolsObj.aVal.size()) tVal = &toolsObj.aVal[(size_t)i];
            } else if (toolsObj.IsObject()) {
                // Tools are keyed "1".."8" (or "0".."7" on some firmware versions)
                bool zeroBased = toolsObj.oVal.count("0") > 0;
                auto it = toolsObj.oVal.find(std::to_string(zeroBased ? i : i + 1));
                if (it != toolsObj.oVal.end()) tVal = &it->second;
            }
            if (!tVal || !tVal->IsObject()) continue;

            MachineSlot& slot = outTools[(size_t)i];
            const JsonVal& feat = tVal->Get("features").IsObject() ? tVal->Get("features") : *tVal;
            slot.nozzleDiameter = feat.Num("nozzle_diameter", slot.nozzleDiameter);
            slot.highFlow       = feat.Boolean("nozzle_high_flow", feat.Boolean("high_flow", slot.highFlow));
            slot.hardened       = feat.Boolean("nozzle_hardened", feat.Boolean("hardened", slot.hardened));

            // Material type reported by the printer itself
            std::string mat = tVal->Str("material");
            if (mat.empty()) mat = tVal->Get("filament").Str("material");
            if (!mat.empty() && mat != "---") {
                slot.loaded   = true;
                slot.abbrev   = ToUpperAscii(TrimAscii(mat));
                slot.material = NormalizeBuddyMaterial(mat);
                if (slot.material.empty()) slot.material = "PLA";
                bool okHex = false;
                Rgb c = Rgb::FromHex(tVal->Str("color", tVal->Get("filament").Str("color")), &okHex);
                if (okHex) slot.color = c;
            }
        }
    }

    static MachineSnapshot ParsePrinter(const JsonVal& p) {
        MachineSnapshot snap;
        snap.uuid           = p.Str("uuid", p.Str("printer_uuid"));
        snap.model          = p.Str("model", p.Str("printer_model", p.Str("type_name")));
        snap.name           = p.Str("name", snap.model);
        snap.firmware       = p.Str("firmware");
        snap.connectState   = p.Str("connect_state", p.Str("state", p.Str("printer_state", "IDLE")));
        snap.teamId         = JsonScalarToString(p.Get("team_id"));
        snap.organizationId = JsonScalarToString(p.Get("organization_id"));
        std::string st      = ToUpperAscii(snap.connectState);
        snap.cloudOnline    = (st != "OFFLINE" && st != "DISCONNECTED");

        bool indx = ToUpperAscii(snap.model).find("INDX") != std::string::npos;
        snap.toolCount = std::clamp((int)p.Num("tool_count", indx ? kToolCount : 1), 1, kToolCount);

        const JsonVal& tools = p.Get("tools").IsObject() || p.Get("tools").IsArray() ? p.Get("tools")
                                                                                     : p.Get("slot").Get("slots");
        if (tools.IsObject() || tools.IsArray()) {
            int n = (int)(tools.IsObject() ? tools.oVal.size() : tools.aVal.size());
            snap.toolCount = std::max(snap.toolCount, std::min(n, kToolCount));
            PopulateToolsFromRest(tools, snap.tools);
        }
        return snap;
    }

    static void ExtractPrinterList(const JsonVal& root, std::vector<MachineSnapshot>& out) {
        auto push = [&](const JsonVal& item, const std::string& fallbackUuid) {
            if (!item.IsObject()) return;
            MachineSnapshot s = ParsePrinter(item);
            if (s.uuid.empty()) s.uuid = fallbackUuid;
            if (s.uuid.empty()) return;
            for (const auto& e : out) {
                if (e.uuid == s.uuid) return;
            }
            out.push_back(std::move(s));
        };

        if (root.IsArray()) {
            for (const auto& item : root.aVal) push(item, "");
            return;
        }
        if (!root.IsObject()) return;

        // Multi-team wrapper: { "teams": [ { "printers": [...] } ] }
        const JsonVal& teams = root.Get("teams");
        if (teams.IsArray()) {
            for (const auto& t : teams.aVal) ExtractPrinterList(t, out);
        } else if (teams.IsObject()) {
            for (const auto& kv : teams.oVal) ExtractPrinterList(kv.second, out);
        }
        for (const char* key : { "printers", "team_printers", "items", "data", "results" }) {
            const JsonVal& sub = root.Get(key);
            if (sub.IsArray()) {
                for (const auto& item : sub.aVal) push(item, "");
            } else if (sub.IsObject()) {
                for (const auto& kv : sub.oVal) push(kv.second, kv.first);
            }
        }
    }

    void ApplyPrinterDetails(const std::string& token, MachineSnapshot& p) {
        HttpResponse r = ConnectGet(token, L"/app/printers/" + Utf8ToWide(p.uuid));
        if (!r.Ok()) return;
        const JsonVal root = MiniJsonParser::Parse(r.body);
        const JsonVal& d = root.Get("printer").IsObject() ? root.Get("printer") : root;
        p.name     = d.Str("name", p.name);
        p.model    = d.Str("model", d.Str("printer_model", p.model));
        p.firmware = d.Str("firmware", p.firmware);
        if (!d.Get("team_id").IsNull())         p.teamId         = JsonScalarToString(d.Get("team_id"));
        if (!d.Get("organization_id").IsNull()) p.organizationId = JsonScalarToString(d.Get("organization_id"));
        if (d.Get("tool_count").IsNumber()) {
            p.toolCount = std::clamp((int)d.Num("tool_count"), 1, kToolCount);
        }
        std::string st = d.Str("state", d.Str("printer_state"));
        if (!st.empty()) {
            p.connectState = st;
            std::string u  = ToUpperAscii(st);
            p.cloudOnline  = (u != "OFFLINE" && u != "DISCONNECTED");
        }
        const JsonVal& tools = d.Get("tools");
        if (tools.IsObject() || tools.IsArray()) PopulateToolsFromRest(tools, p.tools);
    }

    // Organization of a Connect team, cached per team
    std::string ResolveOrganizationId(const std::string& token, MachineSnapshot& snap) {
        if (!snap.organizationId.empty()) return snap.organizationId;
        auto it = m_orgByTeam.find(snap.teamId);
        if (it != m_orgByTeam.end() && !it->second.empty()) return snap.organizationId = it->second;

        std::string org;
        if (!snap.teamId.empty()) {
            HttpResponse r = ConnectGet(token, L"/app/users/teams/" + Utf8ToWide(snap.teamId));
            if (r.Ok()) org = JsonScalarToString(MiniJsonParser::Parse(r.body).Get("organization_id"));
        }
        if (org.empty()) {
            HttpResponse r = ConnectGraphQl(token, kGqlMeQuery, "{}");
            if (r.Ok()) {
                const JsonVal root = MiniJsonParser::Parse(r.body);
                const JsonVal& orgs = root.Get("data").Get("authorization").Get("me").Get("organizations");
                if (orgs.IsArray() && !orgs.aVal.empty()) org = JsonScalarToString(orgs.aVal[0].Get("id"));
            }
        }
        if (!org.empty()) m_orgByTeam[snap.teamId] = snap.organizationId = org;
        return org;
    }

    // ------------------------------------------------------------------------
    // GraphQL "Tools" data (names, brands, colors, weights, tool ids)
    // ------------------------------------------------------------------------
    static bool MergeToolsFromGraphQl(const JsonVal& root, MachineSnapshot& snap) {
        const JsonVal& prn = root.Get("data").Get("printer").Get("printer");
        if (prn.Str("__typename") != "Printer") return false;
        const JsonVal& tools = prn.Get("fff").Get("tools");
        if (!tools.IsArray()) return false;

        // Print heads only (MMU feeder slots have ids like "1.3"), ordered by id
        std::vector<std::pair<int, const JsonVal*>> heads;
        for (size_t k = 0; k < tools.aVal.size(); ++k) {
            const JsonVal& t = tools.aVal[k];
            std::string id = t.Str("id");
            if (!t.IsObject() || ToUpperAscii(t.Str("type")) == "FEEDER" || id.find('.') != std::string::npos) continue;
            bool numeric = !id.empty() && id.find_first_not_of("0123456789") == std::string::npos;
            heads.push_back({ numeric ? std::atoi(id.c_str()) : (int)k, &t });
        }
        if (heads.empty()) return false;
        std::stable_sort(heads.begin(), heads.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        for (size_t i = 0; i < heads.size() && i < (size_t)kToolCount; ++i) {
            const JsonVal& t = *heads[i].second;
            MachineSlot& slot = snap.tools[i];
            slot.toolId = t.Str("id");
            slot.active = t.Get("telemetry").Boolean("active", false);

            const JsonVal& feat = t.Get("features");
            if (feat.Get("nozzleDiameter").IsNumber()) slot.nozzleDiameter = feat.Num("nozzleDiameter");
            slot.highFlow = feat.Boolean("nozzleHighFlow", slot.highFlow);
            slot.hardened = feat.Boolean("nozzleHardened", slot.hardened);

            const JsonVal& mpi = t.Get("materialPackageInstance");
            const JsonVal& pkg = mpi.Get("package");
            const JsonVal& mat = pkg.Get("material");
            bool assigned = mpi.Str("__typename") == "FFFToolMaterialPackageInstance" && mat.IsObject() &&
                            (!mat.Str("uuid").empty() || !mat.Str("name").empty() || !mat.Str("abbreviation").empty());
            if (!assigned) {
                // Nothing recorded in Connect (the printer may still report a material type)
                slot.materialName.clear();
                slot.materialUuid.clear();
                slot.brandName.clear();
                slot.hasWeight = false;
                slot.isFromNfc = false;
                continue;
            }

            slot.loaded       = true;
            slot.isFromNfc    = mpi.Boolean("isFromNfc", false);
            slot.materialName = mat.Str("name");
            slot.materialUuid = mat.Str("uuid");
            slot.brandName    = mat.Get("brand").Str("name");

            std::string abbr = ToUpperAscii(TrimAscii(mat.Str("abbreviation")));
            if (!abbr.empty()) {
                slot.abbrev   = abbr;
                slot.material = NormalizeBuddyMaterial(abbr);
                if (slot.material.empty()) slot.material = "PLA";
            }
            bool okHex = false;
            Rgb c = Rgb::FromHex(mat.Get("primaryColor").Str("colorRgba"), &okHex);
            if (okHex) slot.color = c;

            // Same rule as Connect: full weight = actual weight, or nominal weight when read from NFC
            double full = mpi.Get("actualNettoFullWeight").IsNumber() ? mpi.Num("actualNettoFullWeight") : -1.0;
            if (full <= 0.0 && slot.isFromNfc && pkg.Get("nominalNettoFullWeight").IsNumber()) {
                full = pkg.Num("nominalNettoFullWeight");
            }
            const JsonVal& consumed = mpi.Get("dynamicData").Get("consumedWeight");
            slot.hasWeight = (full > 0.0 && consumed.IsNumber());
            if (slot.hasWeight) {
                slot.nominalWeightG   = full;
                slot.consumedWeightG  = std::max(0.0, consumed.nVal);
                slot.remainingWeightG = std::max(0.0, full - slot.consumedWeightG);
            }
        }
        snap.toolCount = std::clamp((int)heads.size(), 1, kToolCount);
        return true;
    }

    void EnrichFromGraphQl(const std::string& token, MachineSnapshot& snap) {
        std::string org = ResolveOrganizationId(token, snap);
        if (org.empty()) return;
        std::string vars = "{\"organizationId\":" + JsonQuote(org) + ",\"printerId\":" + JsonQuote(snap.uuid) + "}";
        HttpResponse r = ConnectGraphQl(token, kGqlToolsQuery, vars);
        if (r.Ok()) MergeToolsFromGraphQl(MiniJsonParser::Parse(r.body), snap);
    }

    // ------------------------------------------------------------------------
    // Jobs
    // ------------------------------------------------------------------------
    void ExecuteRefresh(const SyncUserConfig& cfg) {
        SetStatus(Tr(i18n::S_ST_READING), true);

        std::string token = CurrentAccessToken();
        std::vector<MachineSnapshot> printers;
        int chosen = -1;
        DWORD httpCode = 0;

        if (!token.empty()) {
            HttpResponse r = ConnectGet(token, L"/slicer/v2/printers");
            httpCode = r.statusCode;
            if (r.Ok()) ExtractPrinterList(MiniJsonParser::Parse(r.body), printers);
            if (printers.empty() && r.statusCode != 401 && r.statusCode != 403) {
                HttpResponse r2 = ConnectGet(token, L"/app/printers");
                if (r2.Ok()) {
                    httpCode = r2.statusCode;
                    ExtractPrinterList(MiniJsonParser::Parse(r2.body), printers);
                }
            }
        }

        if (!printers.empty()) {
            // Printer chosen by the user, else the first INDX, else the one with the most tools
            for (size_t i = 0; i < printers.size() && chosen < 0; ++i) {
                if (!cfg.selectedUuid.empty() && printers[i].uuid == cfg.selectedUuid) chosen = (int)i;
            }
            for (size_t i = 0; i < printers.size() && chosen < 0; ++i) {
                if (ToUpperAscii(printers[i].model + " " + printers[i].name).find("INDX") != std::string::npos) chosen = (int)i;
            }
            if (chosen < 0) {
                int best = -1;
                for (size_t i = 0; i < printers.size(); ++i) {
                    if (printers[i].toolCount > best) { best = printers[i].toolCount; chosen = (int)i; }
                }
            }
            MachineSnapshot& active = printers[(size_t)chosen];
            ApplyPrinterDetails(token, active);
            EnrichFromGraphQl(token, active);
        }

        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            m_printers    = printers;
            m_selectedIdx = chosen;
        }

        if (chosen >= 0) {
            const MachineSnapshot& p = printers[(size_t)chosen];
            int total = std::clamp(p.toolCount, 1, kToolCount);
            int loaded = 0;
            for (int i = 0; i < total; ++i) {
                if (p.tools[(size_t)i].loaded) ++loaded;
            }
            SetStatus(Fmt(i18n::S_ST_UPDATED, { PrinterLabel(p), std::to_wstring(loaded), std::to_wstring(total) }), true);
        } else if (token.empty()) {
            SetStatus(Tr(i18n::S_ST_NOT_LOGGED), false);
        } else if (httpCode == 0) {
            SetStatus(Tr(i18n::S_ST_UNREACHABLE), false);
        } else if (httpCode == 401 || httpCode == 403) {
            SetStatus(Tr(i18n::S_ST_EXPIRED), false);
        } else if (httpCode < 200 || httpCode >= 300) {
            SetStatus(Fmt(i18n::S_ST_HTTP, { std::to_wstring((unsigned long)httpCode) }), false);
        } else {
            SetStatus(Tr(i18n::S_ST_NO_PRINTER), false);
        }
        PostUiEvent(SYNC_EVT_REFRESH_DONE, 1);
    }

    static MaterialItem MaterialFromNode(const JsonVal& node) {
        const JsonVal& mat = node.Get("material");
        MaterialItem it;
        it.uuid         = node.Str("uuid", mat.Str("uuid"));
        it.isCustom     = node.Boolean("isCustom", false);
        it.spoolCount   = (int)node.Num("spoolCount", 0.0);
        it.name         = mat.Str("name");
        it.type         = mat.Str("type");
        it.abbreviation = mat.Str("abbreviation");
        it.brand        = mat.Get("brand").Str("name");
        bool okHex = false;
        Rgb c = Rgb::FromHex(mat.Get("primaryColor").Str("colorRgba"), &okHex);
        if (okHex) it.color = c;
        return it;
    }

    // FFF materials of the catalogue, optionally filtered by type and free text
    bool SearchMaterials(const std::string& token, const std::string& org, const std::string& type,
                         const std::string& search, std::vector<MaterialItem>& out) {
        std::string vars = "{\"organizationId\":" + JsonQuote(org) + ",\"first\":100,\"materialClass\":[\"FFF\"]";
        if (!type.empty())   vars += ",\"materialTypeOrAbbreviation\":[" + JsonQuote(type) + "]";
        if (!search.empty()) vars += ",\"search\":" + JsonQuote(search);
        vars += "}";
        HttpResponse r = ConnectGraphQl(token, kGqlMaterialsQuery, vars);
        if (!r.Ok()) return false;
        const JsonVal root = MiniJsonParser::Parse(r.body);
        const JsonVal& nodes = root.Get("data").Get("warehouse").Get("materials").Get("nodes");
        if (!nodes.IsArray()) return false;
        for (const auto& node : nodes.aVal) {
            MaterialItem it = MaterialFromNode(node);
            if (!it.uuid.empty()) out.push_back(std::move(it));
        }
        return true;
    }

    void ExecuteFetchMaterials(const Job& job) {
        std::vector<MaterialItem> items;
        bool ok = false;
        MachineSnapshot active;
        std::string token = CurrentAccessToken();
        if (GetActivePrinterCopy(active) && !token.empty()) {
            std::string org = ResolveOrganizationId(token, active);
            if (!org.empty()) ok = SearchMaterials(token, org, "", TrimAscii(job.search), items);
        }
        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            m_materials    = std::move(items);
            m_materialsSeq = job.seq;
            m_materialsOk  = ok;
        }
        PostUiEvent(SYNC_EVT_MATERIALS_DONE, (LPARAM)job.seq);
    }

    // UpdatePrinterMaterial for one tool. Custom materials carry their description,
    // catalogue materials only their uuid (same payload as Connect).
    bool AssignToolMaterial(const std::string& token, const std::string& org, const std::string& printerUuid,
                            const std::string& toolId, const MaterialItem& item, std::wstring& reason) {
        std::string mat = "{\"uuid\":" + JsonQuote(item.uuid);
        if (item.isCustom) {
            if (!item.name.empty())         mat += ",\"name\":" + JsonQuote(item.name);
            if (!item.type.empty())         mat += ",\"type\":" + JsonQuote(item.type);
            if (!item.abbreviation.empty()) mat += ",\"abbreviation\":" + JsonQuote(item.abbreviation);
            if (item.color.assigned) mat += ",\"primaryColor\":{\"colorRgba\":" + JsonQuote(item.color.ToHex()) + "}";
        }
        mat += "}";
        std::string vars = "{\"organizationId\":" + JsonQuote(org) + ",\"printerId\":" + JsonQuote(printerUuid) +
                           ",\"printer\":{\"fffTools\":[{\"id\":" + JsonQuote(toolId) +
                           ",\"materialPackageInstance\":{\"package\":{\"material\":" + mat + "}}}]}}";

        HttpResponse r = ConnectGraphQl(token, kGqlAssignMutation, vars);
        if (!r.Ok()) {
            reason = HttpErrorText(r);
            return false;
        }
        const JsonVal root = MiniJsonParser::Parse(r.body);
        const JsonVal& res = root.Get("data").Get("printer").Get("updatePrinter");
        if (res.Str("__typename") == "Printer") return true;
        std::string msg = res.Str("message", GraphQlErrorMessage(root));
        if (!msg.empty()) reason = Utf8ToWide(msg);
        return false;
    }

    std::string BrandUuid(const std::string& token, const std::string& brand) {
        if (!m_brandsLoaded) {
            std::string after;
            for (int page = 0; page < 20; ++page) {
                std::string vars = "{\"first\":100" + (after.empty() ? std::string() : ",\"after\":" + JsonQuote(after)) + "}";
                HttpResponse r = ConnectGraphQl(token, kGqlBrandsQuery, vars);
                if (!r.Ok()) break;
                const JsonVal root = MiniJsonParser::Parse(r.body);
                const JsonVal& brands = root.Get("data").Get("materialdb").Get("brands");
                const JsonVal& edges = brands.Get("edges");
                if (!edges.IsArray()) break;
                for (const auto& e : edges.aVal) {
                    std::string name = e.Get("node").Str("name"), uuid = e.Get("node").Str("uuid");
                    if (!name.empty() && !uuid.empty()) m_brandUuids[ToUpperAscii(TrimAscii(name))] = uuid;
                }
                m_brandsLoaded = true;
                if (!brands.Get("pageInfo").Boolean("hasNextPage", false)) break;
                after = brands.Get("pageInfo").Str("endCursor");
                if (after.empty()) break;
            }
        }
        auto it = m_brandUuids.find(ToUpperAscii(TrimAscii(brand)));
        return (it != m_brandUuids.end()) ? it->second : UuidV5(kConnectBrandNamespace, TrimAscii(brand));
    }

    // Catalogue material of the same brand and type with (almost) the same color, otherwise
    // a custom material named "<name> #RRGGBB" (created once, reused afterwards)
    bool ResolveSpecMaterial(const std::string& token, const std::string& org, const SlicerAssignSpec& sp,
                             MaterialItem& out, std::wstring& reason) {
        std::string hex = sp.color.ToHex();
        std::string customName = sp.name + " " + hex;

        std::vector<MaterialItem> cands;
        SearchMaterials(token, org, sp.type, EqualsNoCase(sp.brand, "Generic") ? "" : sp.brand, cands);
        const MaterialItem* best = nullptr;
        double bestDist = 1e9;
        for (const auto& c : cands) {
            if (!c.color.assigned || (!c.brand.empty() && !EqualsNoCase(c.brand, sp.brand))) continue;
            if (c.isCustom && EqualsNoCase(c.name, customName)) { best = &c; bestDist = 0.0; break; }
            double d = RedmeanDistance(c.color, sp.color);
            if (d < bestDist) { bestDist = d; best = &c; }
        }
        if (best && bestDist <= kSameColorDistance) {
            out = *best;
            return true;
        }

        std::string brandUuid = BrandUuid(token, sp.brand);
        std::string matUuid   = UuidV5(kConnectMaterialNamespace, UuidToBytes(brandUuid) + customName);
        if (brandUuid.empty() || matUuid.empty()) return false;
        std::string abbr = ToUpperAscii(sp.type).substr(0, 7);

        bool exists = std::any_of(cands.begin(), cands.end(), [&](const MaterialItem& c) { return c.uuid == matUuid; });
        if (!exists) {
            std::string material =
                "{\"uuid\":" + JsonQuote(matUuid) +
                ",\"brand\":{\"uuid\":" + JsonQuote(brandUuid) + ",\"name\":" + JsonQuote(sp.brand) + "}" +
                ",\"name\":" + JsonQuote(customName) + ",\"abbreviation\":" + JsonQuote(abbr) +
                ",\"class\":\"FFF\",\"type\":" + JsonQuote(sp.type) +
                ",\"primaryColor\":{\"colorRgba\":" + JsonQuote(hex) + "}}";
            HttpResponse r = ConnectGraphQl(token, kGqlCreateCustomMutation, "{\"material\":" + material + "}");
            if (r.Ok()) {
                // An error here usually means "already exists": the assignment tells whether it is usable
                std::string msg = GraphQlErrorMessage(MiniJsonParser::Parse(r.body));
                if (!msg.empty()) reason = Utf8ToWide(msg);
            } else {
                reason = HttpErrorText(r);
            }
        }

        out = MaterialItem{};
        out.uuid         = matUuid;
        out.isCustom     = true;
        out.name         = customName;
        out.brand        = sp.brand;
        out.type         = sp.type;
        out.abbreviation = abbr;
        out.color        = sp.color;
        return true;
    }

    void FinishAction(const SyncUserConfig& cfg, bool ok, const std::wstring& message) {
        if (ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
            ExecuteRefresh(cfg);
        }
        SetStatus(message, ok);
        PostUiEvent(SYNC_EVT_ACTION_DONE, ok ? 1 : 0);
    }

    void ExecuteAssignMaterial(const Job& job) {
        MachineSnapshot active;
        std::wstring reason = Tr(i18n::S_ST_UNREACHABLE);
        bool ok = false;
        std::string token = CurrentAccessToken();
        if (GetActivePrinterCopy(active) && !token.empty() && job.toolIdx >= 0 && job.toolIdx < kToolCount &&
            !job.material.uuid.empty()) {
            std::string org    = ResolveOrganizationId(token, active);
            std::string toolId = active.tools[(size_t)job.toolIdx].toolId;
            if (!org.empty() && !toolId.empty()) {
                ok = AssignToolMaterial(token, org, active.uuid, toolId, job.material, reason);
            }
        }
        std::wstring label = Utf8ToWide(job.material.name.empty() ? job.material.abbreviation : job.material.name);
        FinishAction(job.config, ok, ok ? Fmt(i18n::S_ST_ASSIGNED, { label }) : Fmt(i18n::S_ST_ASSIGN_FAILED, { reason }));
    }

    void ExecuteAssignFromSlicer(const Job& job) {
        MachineSnapshot active;
        std::wstring reason = Tr(i18n::S_ST_UNREACHABLE);
        int done = 0, total = 0;

        std::string token = CurrentAccessToken();
        if (GetActivePrinterCopy(active) && !token.empty()) {
            std::string org = ResolveOrganizationId(token, active);
            if (IsPrinterBusyState(active.connectState)) {
                reason = Fmt(i18n::S_ST_BUSY, { Utf8ToWide(active.connectState) });
            } else if (!org.empty()) {
                SetStatus(Fmt(i18n::S_ST_SENDING, { PrinterLabel(active) }), true);
                for (const auto& sp : job.specs) {
                    if (sp.toolIdx < 0 || sp.toolIdx >= kToolCount) continue;
                    const std::string& toolId = active.tools[(size_t)sp.toolIdx].toolId;
                    if (toolId.empty()) continue;
                    ++total;
                    MaterialItem item;
                    std::wstring why;
                    if (ResolveSpecMaterial(token, org, sp, item, why) &&
                        AssignToolMaterial(token, org, active.uuid, toolId, item, why)) {
                        ++done;
                    } else if (!why.empty()) {
                        reason = why;
                    }
                }
            }
        }

        bool ok = (total > 0 && done == total);
        std::wstring msg;
        if (ok) {
            msg = Fmt(i18n::S_ST_SENT_MACHINE, { PrinterLabel(active) });
        } else if (total > 0) {
            msg = Fmt(i18n::S_ST_ASSIGN_FAILED, { std::to_wstring(done) + L"/" + std::to_wstring(total) + L" – " + reason });
        } else {
            msg = Fmt(i18n::S_ST_ASSIGN_FAILED, { reason });
        }
        if (done > 0 && !ok) {
            // Some tools were updated: show them
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
            ExecuteRefresh(job.config);
        }
        FinishAction(job.config, ok, msg);
    }

    void ExecuteUnload(const Job& job) {
        MachineSnapshot active;
        std::wstring reason = Tr(i18n::S_ST_UNREACHABLE);
        bool ok = false;
        std::wstring toolLabel = std::wstring(Tr(i18n::S_TOOL)) + L" " + std::to_wstring(job.toolIdx + 1);

        std::string token = CurrentAccessToken();
        if (GetActivePrinterCopy(active) && !token.empty() && job.toolIdx >= 0 && job.toolIdx < kToolCount) {
            const MachineSlot& slot = active.tools[(size_t)job.toolIdx];
            bool numericId = !slot.toolId.empty() && slot.toolId.find_first_not_of("0123456789") == std::string::npos;
            if (!IsPrinterIdleState(active.connectState)) {
                reason = Fmt(i18n::S_ST_BUSY, { Utf8ToWide(active.connectState) });
            } else if (numericId && slot.loaded) {
                std::string filament = !slot.abbrev.empty() ? slot.abbrev : slot.material;
                int nozzleC = 230, bedC = 0;
                ConnectUnloadTemperatures(filament, nozzleC, bedC);
                std::string body =
                    "{\"command\":\"UNLOAD_FILAMENT\",\"kwargs\":{\"filament\":" + JsonQuote(filament) +
                    ",\"nozzle_temperature\":" + std::to_string(nozzleC) +
                    ",\"bed_temperature\":" + std::to_string(bedC) +
                    ",\"tool\":" + std::to_string(std::atoi(slot.toolId.c_str())) + "}}";
                HttpResponse r = HttpsRequest(kConnectHost, L"POST",
                                              L"/app/printers/" + Utf8ToWide(active.uuid) + L"/commands/sync",
                                              BearerHeaders(token, true), body, 4000, 20000);
                ok = r.Ok();
                if (!ok) {
                    std::string msg = MiniJsonParser::Parse(r.body).Str("message");
                    reason = msg.empty() ? HttpErrorText(r) : Utf8ToWide(msg);
                }
            }
        }
        // The printer reports the change by itself: no refresh needed here
        SetStatus(ok ? Fmt(i18n::S_ST_UNLOAD_SENT, { toolLabel }) : Fmt(i18n::S_ST_UNLOAD_FAILED, { reason }), ok);
        PostUiEvent(SYNC_EVT_ACTION_DONE, ok ? 1 : 0);
    }

    void WorkerLoop() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cv.wait(lk, [this]() { return m_stop || !m_queue.empty(); });
                if (m_stop) break;
                job = std::move(m_queue.front());
                m_queue.pop_front();
            }
            m_busy.store(true);
            switch (job.kind) {
            case Job::Refresh:          ExecuteRefresh(job.config); break;
            case Job::FetchMaterials:   ExecuteFetchMaterials(job); break;
            case Job::AssignMaterial:   ExecuteAssignMaterial(job); break;
            case Job::AssignFromSlicer: ExecuteAssignFromSlicer(job); break;
            case Job::UnloadFilament:   ExecuteUnload(job); break;
            }
            m_busy.store(false);
        }
    }
};

} // namespace openps::sync

#endif // OPENPS_SYNC_INDX_ENGINE_HPP
