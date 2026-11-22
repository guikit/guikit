#include "guikit/backend/system_theme.hpp"

#include "keel/core.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "keel/dynlib.h"
#elif defined(__APPLE__)
#include "keel/dynlib.h"
#include "keel/string.h"
#else
#include "keel/env.h"
#include "keel/fs.h"
#include "keel/static_array.h"
#include "keel/string.h"
#endif

namespace guikit {

namespace {

#if defined(__EMSCRIPTEN__)

EM_JS(keel::i32, guikit_js_query_os_system_theme, (), {
    if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') return 0;
    if (window.matchMedia('(prefers-color-scheme: dark)').matches) return 1;
    if (window.matchMedia('(prefers-color-scheme: light)').matches) return 2;
    return 0;
});

#elif !defined(_WIN32) && !defined(__APPLE__)

[[nodiscard]] bool ContainsCaseInsensitive(keel::StringRef haystack, keel::StringRef needle) noexcept {
    if (needle.empty() || haystack.size() < needle.size()) {
        return false;
    }
    for (keel::usize i = 0u; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (keel::usize j = 0u; j < needle.size(); ++j) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a = keel::as<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = keel::as<char>(b + ('a' - 'A'));
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] keel::StringRef TrimAscii(keel::StringRef s) noexcept {
    keel::usize start = 0u;
    keel::usize end = s.size();
    while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        ++start;
    }
    while (end > start && (s[end - 1u] == ' ' || s[end - 1u] == '\t' || s[end - 1u] == '\r' || s[end - 1u] == '\n')) {
        --end;
    }
    return s.substr(start, end - start);
}

[[nodiscard]] keel::Optional<ThemeMode> ParseGtkSettingsIni(keel::StringRef content) noexcept {
    keel::Optional<ThemeMode> themeNameHint = keel::kNullOpt;
    keel::usize pos = 0u;
    while (pos < content.size()) {
        keel::usize lineEnd = content.find('\n', pos);
        if (lineEnd == keel::StringRef::npos) {
            lineEnd = content.size();
        }
        const keel::StringRef line = TrimAscii(content.substr(pos, lineEnd - pos));
        pos = (lineEnd < content.size()) ? (lineEnd + 1u) : content.size();
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') {
            continue;
        }
        const keel::usize eq = line.find('=');
        if (eq == keel::StringRef::npos) {
            continue;
        }
        const keel::StringRef key = TrimAscii(line.substr(0u, eq));
        const keel::StringRef val = TrimAscii(line.substr(eq + 1u));
        if (ContainsCaseInsensitive(key, "gtk-application-prefer-dark-theme")) {
            if (val == "1" || ContainsCaseInsensitive(val, "true")) {
                return ThemeMode::Dark;
            }
            if (val == "0" || ContainsCaseInsensitive(val, "false")) {
                return ThemeMode::Light;
            }
        } else if (!themeNameHint.has_value() && ContainsCaseInsensitive(key, "gtk-theme-name")) {
            if (ContainsCaseInsensitive(val, "dark")) {
                themeNameHint = ThemeMode::Dark;
            } else if (ContainsCaseInsensitive(val, "light")) {
                themeNameHint = ThemeMode::Light;
            }
        }
    }
    return themeNameHint;
}

[[nodiscard]] keel::Optional<ThemeMode> ParseKdeGlobals(keel::StringRef content) noexcept {
    keel::usize pos = 0u;
    while (pos < content.size()) {
        keel::usize lineEnd = content.find('\n', pos);
        if (lineEnd == keel::StringRef::npos) {
            lineEnd = content.size();
        }
        const keel::StringRef line = TrimAscii(content.substr(pos, lineEnd - pos));
        pos = (lineEnd < content.size()) ? (lineEnd + 1u) : content.size();
        if (line.empty() || line[0] == '#' || line[0] == '[') {
            continue;
        }
        const keel::usize eq = line.find('=');
        if (eq == keel::StringRef::npos) {
            continue;
        }
        const keel::StringRef key = TrimAscii(line.substr(0u, eq));
        const keel::StringRef val = TrimAscii(line.substr(eq + 1u));
        if (key == "ColorScheme") {
            if (ContainsCaseInsensitive(val, "dark")) {
                return ThemeMode::Dark;
            }
            if (ContainsCaseInsensitive(val, "light")) {
                return ThemeMode::Light;
            }
        }
    }
    return keel::kNullOpt;
}

#endif

} // namespace

keel::Optional<ThemeMode> QueryOsSystemTheme() noexcept {
#if defined(__EMSCRIPTEN__)
    const keel::i32 r = guikit_js_query_os_system_theme();
    if (r == 1) return ThemeMode::Dark;
    if (r == 2) return ThemeMode::Light;
    return keel::kNullOpt;

#elif defined(_WIN32)
    using PfnRegGetValueW = LONG(WINAPI*)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD); // keel-lint: ignore
    auto lib = keel::DynLib::open("advapi32.dll");
    if (!lib.is_ok()) {
        return keel::kNullOpt;
    }
    const auto regGetValueW = lib->symbol<PfnRegGetValueW>("RegGetValueW");
    if (!regGetValueW) {
        return keel::kNullOpt;
    }
    DWORD value = 1u;
    DWORD size = sizeof(value);
    static constexpr DWORD kRrfRtRegDword = 0x00000010u;
    const LONG status = regGetValueW( // keel-lint: ignore
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        kRrfRtRegDword,
        nullptr,
        &value,
        &size
    );
    if (status == ERROR_SUCCESS) {
        return (value == 0u) ? ThemeMode::Dark : ThemeMode::Light;
    }
    return keel::kNullOpt;

#elif defined(__APPLE__)
    // Dynamic CoreFoundation lookup (zero extra link flags):
    // AppleInterfaceStyle == "Dark" -> Dark, unset/other -> Light.
    using CFTypeRef = const void*;
    using CFStringRef = const void*;
    using PfnCFStringCreateWithCString = CFStringRef (*)(const void*, const char*, keel::u32);
    using PfnCFPreferencesCopyAppValue = CFTypeRef (*)(CFStringRef, CFStringRef);
    using PfnCFStringGetCString = keel::u8 (*)(CFStringRef, char*, keel::isize, keel::u32);
    using PfnCFGetTypeID = keel::usize (*)(CFTypeRef);
    using PfnCFStringGetTypeID = keel::usize (*)();
    using PfnCFRelease = void (*)(CFTypeRef);

    auto cf = keel::DynLib::open("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation");
    if (!cf.is_ok()) {
        return keel::kNullOpt;
    }
    const auto cfStrCreate = cf->symbol<PfnCFStringCreateWithCString>("CFStringCreateWithCString");
    const auto cfPrefCopy = cf->symbol<PfnCFPreferencesCopyAppValue>("CFPreferencesCopyAppValue");
    const auto cfStrGet = cf->symbol<PfnCFStringGetCString>("CFStringGetCString");
    const auto cfGetTypeId = cf->symbol<PfnCFGetTypeID>("CFGetTypeID");
    const auto cfStrTypeId = cf->symbol<PfnCFStringGetTypeID>("CFStringGetTypeID");
    const auto cfRelease = cf->symbol<PfnCFRelease>("CFRelease");
    if (!cfStrCreate || !cfPrefCopy || !cfStrGet || !cfGetTypeId || !cfStrTypeId || !cfRelease) {
        return keel::kNullOpt;
    }

    static constexpr keel::u32 kCFStringEncodingUTF8 = 0x08000100u;
    const CFStringRef key = cfStrCreate(nullptr, "AppleInterfaceStyle", kCFStringEncodingUTF8);
    const CFStringRef app = cfStrCreate(nullptr, "kCFPreferencesAnyApplication", kCFStringEncodingUTF8);
    if (!key || !app) {
        if (key) cfRelease(key);
        if (app) cfRelease(app);
        return keel::kNullOpt;
    }

    ThemeMode result = ThemeMode::Light;
    if (const CFTypeRef val = cfPrefCopy(key, app); val != nullptr) {
        if (cfGetTypeId(val) == cfStrTypeId()) {
            char buf[64]{};
            if (cfStrGet(val, buf, keel::as<keel::isize>(sizeof(buf)), kCFStringEncodingUTF8) != 0u) {
                const keel::StringRef styleStr(buf);
                if (styleStr == "Dark") {
                    result = ThemeMode::Dark;
                }
            }
        }
        cfRelease(val);
    }
    cfRelease(app);
    cfRelease(key);
    return result;

#else
    if (const auto gtkTheme = keel::env_get("GTK_THEME"); gtkTheme.has_value() && !gtkTheme->empty()) {
        if (ContainsCaseInsensitive(*gtkTheme, "dark")) {
            return ThemeMode::Dark;
        }
        if (ContainsCaseInsensitive(*gtkTheme, "light")) {
            return ThemeMode::Light;
        }
    }

    if (const auto cfgDir = keel::fs_config_dir(); cfgDir.is_ok() && !cfgDir->empty()) {
        static constexpr keel::StaticArray<keel::StringRef, 2> kGtkIniRelPaths = {{
            "gtk-3.0/settings.ini",
            "gtk-4.0/settings.ini"
        }};
        for (keel::usize i = 0u; i < kGtkIniRelPaths.size(); ++i) {
            const keel::String iniPath = keel::path_join(*cfgDir, kGtkIniRelPaths[i]);
            if (const auto text = keel::fs_read_text(iniPath); text.is_ok() && !text->empty()) {
                if (const auto mode = ParseGtkSettingsIni(*text); mode.has_value()) {
                    return mode;
                }
            }
        }

        const keel::String kdePath = keel::path_join(*cfgDir, "kdeglobals");
        if (const auto text = keel::fs_read_text(kdePath); text.is_ok() && !text->empty()) {
            if (const auto mode = ParseKdeGlobals(*text); mode.has_value()) {
                return mode;
            }
        }
    }

    return keel::kNullOpt;
#endif
}

} // namespace guikit
