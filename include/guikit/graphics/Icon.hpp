#pragma once

#include <cstdint>
#include <string_view>
#include "guikit/core/Types.hpp"

namespace guikit {

class ICanvas;

/// Catalog of standard procedural vector icon glyphs.
enum class IconId : uint16_t {
    None = 0,

    // Actions & Navigation
    Close,
    Check,
    Plus,
    Minus,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    ChevronLeft,
    ChevronRight,
    ChevronUp,
    ChevronDown,

    // Media & Control
    Play,
    Pause,
    Stop,
    Refresh,

    // System & Tools
    Settings,
    Search,
    Edit,
    Trash,
    Copy,
    Folder,
    Save,
    Eye,
    EyeOff,
    Info,
    Warning,
    Flash,
    Error
};

/// First-class Value Object representing an icon (procedural vector, asset file, or texture).
class Icon {
public:
    enum class SourceType : uint8_t {
        None = 0,
        Builtin,  ///< Procedural vector shape drawn directly via ICanvas
        Asset,    ///< SVG / file path
        Texture   ///< Loaded GPU texture handle
    };

    constexpr Icon() noexcept = default;

    constexpr Icon(IconId id) noexcept
        : m_id(id)
        , m_type(id == IconId::None ? SourceType::None : SourceType::Builtin)
    {}

    // Factory methods
    [[nodiscard]] static Icon File(std::string_view path) noexcept {
        Icon ic;
        if (!path.empty()) {
            ic.m_type = SourceType::Asset;
            ic.m_path = path;
        }
        return ic;
    }

    [[nodiscard]] static Icon Texture(uint64_t textureId, float width, float height) noexcept {
        Icon ic;
        ic.m_type = SourceType::Texture;
        ic.m_textureId = textureId;
        ic.m_texSize = {width, height};
        return ic;
    }

    // Ergonomic static helpers for built-in vector icons
    [[nodiscard]] static constexpr Icon Play()         noexcept { return Icon(IconId::Play); }
    [[nodiscard]] static constexpr Icon Pause()        noexcept { return Icon(IconId::Pause); }
    [[nodiscard]] static constexpr Icon Stop()         noexcept { return Icon(IconId::Stop); }
    [[nodiscard]] static constexpr Icon Refresh()      noexcept { return Icon(IconId::Refresh); }
    [[nodiscard]] static constexpr Icon Close()        noexcept { return Icon(IconId::Close); }
    [[nodiscard]] static constexpr Icon Check()        noexcept { return Icon(IconId::Check); }
    [[nodiscard]] static constexpr Icon Plus()         noexcept { return Icon(IconId::Plus); }
    [[nodiscard]] static constexpr Icon Minus()        noexcept { return Icon(IconId::Minus); }
    [[nodiscard]] static constexpr Icon Settings()     noexcept { return Icon(IconId::Settings); }
    [[nodiscard]] static constexpr Icon Search()       noexcept { return Icon(IconId::Search); }
    [[nodiscard]] static constexpr Icon Edit()         noexcept { return Icon(IconId::Edit); }
    [[nodiscard]] static constexpr Icon Trash()        noexcept { return Icon(IconId::Trash); }
    [[nodiscard]] static constexpr Icon Copy()         noexcept { return Icon(IconId::Copy); }
    [[nodiscard]] static constexpr Icon Folder()       noexcept { return Icon(IconId::Folder); }
    [[nodiscard]] static constexpr Icon Save()         noexcept { return Icon(IconId::Save); }
    [[nodiscard]] static constexpr Icon ArrowLeft()    noexcept { return Icon(IconId::ArrowLeft); }
    [[nodiscard]] static constexpr Icon ArrowRight()   noexcept { return Icon(IconId::ArrowRight); }
    [[nodiscard]] static constexpr Icon ArrowUp()      noexcept { return Icon(IconId::ArrowUp); }
    [[nodiscard]] static constexpr Icon ArrowDown()    noexcept { return Icon(IconId::ArrowDown); }
    [[nodiscard]] static constexpr Icon ChevronLeft()  noexcept { return Icon(IconId::ChevronLeft); }
    [[nodiscard]] static constexpr Icon ChevronRight() noexcept { return Icon(IconId::ChevronRight); }
    [[nodiscard]] static constexpr Icon ChevronUp()    noexcept { return Icon(IconId::ChevronUp); }
    [[nodiscard]] static constexpr Icon ChevronDown()  noexcept { return Icon(IconId::ChevronDown); }
    [[nodiscard]] static constexpr Icon Eye()          noexcept { return Icon(IconId::Eye); }
    [[nodiscard]] static constexpr Icon EyeOff()       noexcept { return Icon(IconId::EyeOff); }
    [[nodiscard]] static constexpr Icon Info()         noexcept { return Icon(IconId::Info); }
    [[nodiscard]] static constexpr Icon Warning()      noexcept { return Icon(IconId::Warning); }
    [[nodiscard]] static constexpr Icon Flash()        noexcept { return Icon(IconId::Flash); }
    [[nodiscard]] static constexpr Icon Error()        noexcept { return Icon(IconId::Error); }

    // State queries
    [[nodiscard]] constexpr bool IsEmpty() const noexcept { return m_type == SourceType::None; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return m_type != SourceType::None; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return IsValid(); }

    [[nodiscard]] constexpr SourceType GetType() const noexcept { return m_type; }
    [[nodiscard]] constexpr IconId GetId() const noexcept { return m_id; }
    [[nodiscard]] constexpr std::string_view GetPath() const noexcept { return m_path; }

    [[nodiscard]] constexpr bool operator==(const Icon& other) const noexcept {
        if (m_type != other.m_type) return false;
        if (m_type == SourceType::None) return true;
        if (m_type == SourceType::Builtin) return m_id == other.m_id;
        if (m_type == SourceType::Asset) return m_path == other.m_path;
        return m_textureId == other.m_textureId;
    }

    [[nodiscard]] constexpr bool operator!=(const Icon& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator==(IconId id) const noexcept {
        return m_type == SourceType::Builtin && m_id == id;
    }

    [[nodiscard]] constexpr bool operator!=(IconId id) const noexcept {
        return !(*this == id);
    }

    /// Renders this icon within the specified rectangular bounds using the given tint color.
    void Draw(ICanvas& canvas, Rect bounds, Color tint) const;
    void Render(ICanvas& canvas, Rect bounds, Color tint) const { Draw(canvas, bounds, tint); }

private:
    IconId m_id{IconId::None};
    SourceType m_type{SourceType::None};
    std::string_view m_path{};
    uint64_t m_textureId{0};
    Vec2 m_texSize{0.0f, 0.0f};
};

} // namespace guikit
