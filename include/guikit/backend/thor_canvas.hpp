#pragma once

#include <cstdint>
#include <cstring>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/lru_cache.h"
#include "keel/string.h"

namespace tvg {
class SwCanvas;
class Paint;
class Text;
} // namespace tvg

namespace guikit {

/// ThorVG-based vector graphics canvas implementation.
class ThorCanvas : public ICanvas {
public:
    explicit ThorCanvas(bool loadDefaultFonts = true);
    ~ThorCanvas() override;

    ThorCanvas(const ThorCanvas&) = delete;
    ThorCanvas& operator=(const ThorCanvas&) = delete;

    [[nodiscard]] SurfacePixelFormat GetPixelFormat() const noexcept override {
        return SurfacePixelFormat::ABGR8888_Premul;
    }

    /// Configures the render target buffer (ABGR8888 premultiplied).
    void SetTarget(uint32_t* buffer, uint32_t width, uint32_t height, uint32_t stride) override;

    /// Loads a TrueType/OpenType font from a virtual asset path or filesystem path for the specified `FontStyle` slot.
    bool LoadFontFile(keel::StringRef path, FontStyle style = FontStyle::Regular) override;

    /// Loads a TrueType/OpenType font directly from an in-memory buffer for the specified `FontStyle` slot.
    bool LoadFontMemory(
        keel::StringRef name,
        keel::ArrayRef<uint8_t> data,
        FontStyle style = FontStyle::Regular,
        keel::StringRef mimeType = "ttf"
    ) override;

    // ICanvas implementation
    void Begin(uint32_t width, uint32_t height, float scale = 1.0f) override;
    void End() override;
    [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
    void SetScale(float scale) override { m_scale = (scale > 0.0f) ? scale : 1.0f; }
    [[nodiscard]] keel::StringRef GetBackendName() const noexcept override { return "ThorVG (Vector Software)"; }
    void Clear(Color color, keel::Optional<Rect> rect = keel::kNullOpt) override;

    void PushClip(Rect clipRect) override;
    void PopClip() override;

    void DrawRect(Rect rect, Color fill) override;
    void DrawRoundRect(
        Rect rect,
        float radius,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        float strokeWidth = 0.0f
    ) override;

    Vec2 MeasureText(keel::StringRef text, float fontSize) override;
    Vec2 MeasureTextStyled(keel::StringRef text, float fontSize, FontStyle style) override;
    [[nodiscard]] float GetLineHeight(float fontSize) override;
    void DrawText(keel::StringRef text, Vec2 pos, float fontSize, Color color) override;
    void DrawTextStyled(
        keel::StringRef text,
        Vec2 pos,
        float fontSize,
        Color color,
        FontStyle style,
        float angleDeg = 0.0f
    ) override;
    void DrawTextBox(
        keel::StringRef text,
        Rect bounds,
        float fontSize,
        Color color,
        float alignX = 0.0f,
        float alignY = 0.5f
    ) override;
    void DrawTextBoxStyled(
        keel::StringRef text,
        Rect bounds,
        float fontSize,
        Color color,
        FontStyle style,
        float alignX = 0.0f,
        float alignY = 0.5f
    ) override;
    void DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth = 1.0f) override;
    void DrawTriangle(
        Vec2 p1,
        Vec2 p2,
        Vec2 p3,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        float strokeWidth = 0.0f
    ) override;
    void DrawPath(const VectorPath& path, const PathPaint& paint) override;

    Vec2 GetImageSize(keel::StringRef source) override;
    bool DrawImage(
        keel::StringRef source,
        Rect destRect,
        float cornerRadius = 0.0f,
        uint8_t opacity = 255
    ) override;
    bool DrawPixels(
        const uint32_t* pixels,
        uint32_t width,
        uint32_t height,
        Rect destRect,
        float cornerRadius = 0.0f,
        uint8_t opacity = 255
    ) override;

    void SetGpuShadersEnabled(bool enabled) noexcept override {
        m_gpuShadersEnabled = enabled;
        if (!enabled) {
            m_gpuPasses.clear();
        }
    }
    [[nodiscard]] bool SupportsGpuShaders() const noexcept override { return m_gpuShadersEnabled; }
    bool DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole = true) override;
    bool CaptureRegion(Rect rect, keel::Array<uint32_t>& outPixels, uint32_t& outW, uint32_t& outH) override;
    bool RestoreRegion(Rect rect, keel::ArrayRef<uint32_t> inPixels, uint32_t inW, uint32_t inH) override;
    void TrimCaches() noexcept override;

    [[nodiscard]] keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept override { return m_gpuPasses; }
    [[nodiscard]] bool HasActiveGpuPasses() const noexcept override {
        return m_gpuShadersEnabled && !m_gpuPasses.empty();
    }

private:
    struct ImageCacheEntry {
        keel::String resolvedDataOrPath;
        const uint8_t* embeddedData{nullptr};
        size_t embeddedSize{0};
        keel::SmallString<16> embeddedMimeType;
        Vec2 size{0.0f, 0.0f};
        bool isInlineSvg{false};
        bool isEmbedded{false};
        bool valid{false};
        uint32_t cachedRasterW{0};
        uint32_t cachedRasterH{0};
        keel::Array<uint32_t> cachedPixels;
    };

    ImageCacheEntry& ResolveImageEntry(keel::StringRef source);
    bool LoadFontMemoryInternal(
        keel::StringRef name,
        keel::ArrayRef<uint8_t> data,
        FontStyle style,
        keel::StringRef mimeType,
        bool copyData
    );
    [[nodiscard]] bool NeedsSyntheticBold(FontStyle style) const noexcept;
    [[nodiscard]] bool NeedsSyntheticItalic(FontStyle style) const noexcept;
    void ApplyClip(tvg::Paint* paint, keel::Optional<Rect> bounds = keel::kNullOpt);
    void ApplyFont(tvg::Text* txt, FontStyle style) const;
    void PunchHole(Rect rect);
    void FlushCanvas();
    void SyncViewportClip();

    tvg::SwCanvas* m_canvas{nullptr};
    tvg::SwCanvas* m_stampCanvas{nullptr};
    keel::Array<uint32_t> m_stampScratch;
    tvg::Text* m_measureText{nullptr};
    uint32_t* m_targetBuffer{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_stride{0};
    float m_scale{1.0f};
    bool m_gpuShadersEnabled{false};
    bool m_hasPendingPaints{false};

    keel::Array<GpuShaderPassDesc> m_gpuPasses;

    keel::Array<Rect> m_clipStack;
    keel::Array<Rect> m_gpuClipStack;

    struct FontSlot {
        keel::String name;
        bool loaded{false};
    };
    static constexpr size_t kFontStyleCount = 5;
    keel::StaticArray<FontSlot, kFontStyleCount> m_fonts{};

    struct TextMeasureKey {
        uint64_t hash;
        uint32_t fontSizeBits;
        uint8_t style;

        bool operator==(const TextMeasureKey& other) const noexcept {
            return hash == other.hash && fontSizeBits == other.fontSizeBits && style == other.style;
        }
    };

    struct TextMeasureKeyHash {
        keel::u64 operator()(const TextMeasureKey& k) const noexcept {
            uint64_t mixed = k.hash ^ (static_cast<uint64_t>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL);
            mixed ^= static_cast<uint64_t>(k.style) * 0x517cc1b727220a95ULL;
            return mixed;
        }
    };

    struct TextStampKey {
        uint64_t textHash{0};
        uint32_t fontSizeBits{0};
        uint32_t colorRgba{0};
        uint16_t textLen{0};
        uint8_t style{0};

        bool operator==(const TextStampKey& o) const noexcept {
            return textHash == o.textHash &&
                   fontSizeBits == o.fontSizeBits &&
                   colorRgba == o.colorRgba &&
                   textLen == o.textLen &&
                   style == o.style;
        }
    };

    struct TextStampKeyHash {
        keel::u64 operator()(const TextStampKey& k) const noexcept {
            uint64_t h = k.textHash;
            h ^= static_cast<uint64_t>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL;
            h ^= static_cast<uint64_t>(k.colorRgba) * 0xbf58476d1ce4e5b9ULL;
            h ^= (static_cast<uint64_t>(k.textLen) << 8 | k.style) * 0x94d049bb133111ebULL;
            return h;
        }
    };

    struct TextStampEntry {
        keel::Array<uint32_t> pixels;
        uint16_t width{0};
        uint16_t height{0};
        int16_t offsetX{0};
        int16_t offsetY{0};
        bool empty{false};
    };

    struct ShapeStampKey {
        uint32_t fillRgba{0};
        uint32_t strokeRgba{0};
        uint16_t qW{0};
        uint16_t qH{0};
        uint16_t qRad{0};
        uint16_t qStrokeW{0};

        bool operator==(const ShapeStampKey& o) const noexcept {
            return fillRgba == o.fillRgba &&
                   strokeRgba == o.strokeRgba &&
                   qW == o.qW &&
                   qH == o.qH &&
                   qRad == o.qRad &&
                   qStrokeW == o.qStrokeW;
        }
    };

    struct ShapeStampKeyHash {
        keel::u64 operator()(const ShapeStampKey& k) const noexcept {
            uint64_t h = static_cast<uint64_t>(k.fillRgba) | (static_cast<uint64_t>(k.strokeRgba) << 32);
            h ^= (static_cast<uint64_t>(k.qW) | (static_cast<uint64_t>(k.qH) << 16) |
                  (static_cast<uint64_t>(k.qRad) << 32) | (static_cast<uint64_t>(k.qStrokeW) << 48)) * 0x9e3779b97f4a7c15ULL;
            h ^= h >> 33;
            return h * 0xff51afd7ed558ccdULL;
        }
    };

    template <typename Key, size_t MaxEntries, size_t MaxPixelBytes, typename HashFn>
    class StampLruCache {
        static constexpr uint32_t kNil = 0xFFFFFFFFu;

        struct Node {
            Key key{};
            TextStampEntry entry{};
            uint32_t prev{kNil};
            uint32_t next{kNil};
        };

    public:
        [[nodiscard]] const TextStampEntry* Get(const Key& key) noexcept {
            const uint32_t* idxPtr = m_map.get(key);
            if (!idxPtr) {
                return nullptr;
            }
            const uint32_t idx = *idxPtr;
            MoveToFront(idx);
            return &m_nodes[idx].entry;
        }

        const TextStampEntry& Insert(const Key& key, TextStampEntry&& entry) {
            const size_t newBytes = entry.pixels.capacity() * sizeof(uint32_t);
            if (const uint32_t* existingPtr = m_map.get(key)) {
                const uint32_t idx = *existingPtr;
                const size_t oldBytes = m_nodes[idx].entry.pixels.capacity() * sizeof(uint32_t);
                m_totalPixelBytes = (m_totalPixelBytes >= oldBytes) ? (m_totalPixelBytes - oldBytes) : 0u;
                m_nodes[idx].entry = keel::move(entry);
                m_totalPixelBytes += newBytes;
                MoveToFront(idx);
                while (m_map.size() > 1u && m_totalPixelBytes > MaxPixelBytes) {
                    EvictTail();
                }
                return m_nodes[idx].entry;
            }

            while (!m_map.empty() &&
                   (m_map.size() >= MaxEntries ||
                    (newBytes > 0u && m_totalPixelBytes + newBytes > MaxPixelBytes))) {
                EvictTail();
            }

            uint32_t slot = kNil;
            if (m_freeHead != kNil) {
                slot = m_freeHead;
                m_freeHead = m_nodes[slot].next;
                m_nodes[slot].key = key;
                m_nodes[slot].entry = keel::move(entry);
                m_nodes[slot].prev = kNil;
                m_nodes[slot].next = kNil;
            } else {
                if (m_nodes.capacity() == 0u) {
                    m_nodes.reserve(MaxEntries);
                }
                slot = static_cast<uint32_t>(m_nodes.size());
                m_nodes.push_back(Node{
                    .key = key,
                    .entry = keel::move(entry),
                    .prev = kNil,
                    .next = kNil
                });
            }

            m_totalPixelBytes += newBytes;
            AttachFront(slot);
            m_map.put(key, slot);
            return m_nodes[slot].entry;
        }

        void Clear() noexcept {
            m_map = {};
            m_nodes = {};
            m_totalPixelBytes = 0u;
            m_head = kNil;
            m_tail = kNil;
            m_freeHead = kNil;
        }

    private:
        void EvictTail() noexcept {
            if (m_tail == kNil) {
                return;
            }
            const uint32_t slot = m_tail;
            Node& victim = m_nodes[slot];
            const size_t oldBytes = victim.entry.pixels.capacity() * sizeof(uint32_t);
            m_totalPixelBytes = (m_totalPixelBytes >= oldBytes) ? (m_totalPixelBytes - oldBytes) : 0u;
            (void)m_map.remove(victim.key);
            Detach(slot);
            victim.entry.pixels = keel::Array<uint32_t>{};
            victim.next = m_freeHead;
            m_freeHead = slot;
        }

        void Detach(uint32_t idx) noexcept {
            Node& n = m_nodes[idx];
            if (n.prev != kNil) {
                m_nodes[n.prev].next = n.next;
            } else {
                m_head = n.next;
            }
            if (n.next != kNil) {
                m_nodes[n.next].prev = n.prev;
            } else {
                m_tail = n.prev;
            }
            n.prev = kNil;
            n.next = kNil;
        }

        void AttachFront(uint32_t idx) noexcept {
            Node& n = m_nodes[idx];
            n.prev = kNil;
            n.next = m_head;
            if (m_head != kNil) {
                m_nodes[m_head].prev = idx;
            } else {
                m_tail = idx;
            }
            m_head = idx;
        }

        void MoveToFront(uint32_t idx) noexcept {
            if (idx == m_head) {
                return;
            }
            Detach(idx);
            AttachFront(idx);
        }

        keel::Array<Node> m_nodes;
        keel::HashMap<Key, uint32_t, HashFn> m_map;
        size_t m_totalPixelBytes{0};
        uint32_t m_head{kNil};
        uint32_t m_tail{kNil};
        uint32_t m_freeHead{kNil};
    };

    const TextStampEntry& ResolveTextStamp(
        keel::StringRef text,
        float scaledFontSize,
        Color color,
        FontStyle style
    );
    const TextStampEntry& ResolveShapeStamp(
        float scaledW,
        float scaledH,
        float scaledRadius,
        Color fill,
        Color stroke,
        float scaledStrokeWidth
    );
    void BlitTextStamp(
        const TextStampEntry& stamp,
        int dstX,
        int dstY,
        keel::Optional<Rect> extraClip = keel::kNullOpt
    );

    static constexpr size_t kMaxTextStampBytes = 2u * 1024u * 1024u;
    static constexpr size_t kMaxShapeStampBytes = 1u * 1024u * 1024u;
    static constexpr size_t kMaxImageCacheEntries = 48u;
    static constexpr size_t kMaxImageCacheBytes = 4u * 1024u * 1024u;

    keel::LruCache<TextMeasureKey, Vec2, 1024, TextMeasureKeyHash> m_textMeasureCache;
    StampLruCache<TextStampKey, 512, kMaxTextStampBytes, TextStampKeyHash> m_textStampCache;
    StampLruCache<ShapeStampKey, 192, kMaxShapeStampBytes, ShapeStampKeyHash> m_shapeStampCache;
    keel::HashMap<keel::String, ImageCacheEntry> m_imageCache;
    size_t m_imageCacheBytes{0};
};

/// Creates a new `ThorCanvas` instance.
[[nodiscard]] keel::UniquePtr<ICanvas> CreateThorVectorCanvas(bool loadDefaultFonts = true);

/// Registers the ThorVG vector canvas factory and image decoder as the default vector backend.
void RegisterThorVectorBackend() noexcept;

} // namespace guikit
