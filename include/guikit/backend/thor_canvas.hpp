#pragma once

#include <cstring>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/lru_cache.h"
#include "keel/string.h"

namespace tvg {
struct SwCanvas;
struct Paint;
struct Text;
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
    void SetTarget(keel::u32* buffer, keel::u32 width, keel::u32 height, keel::u32 stride) override;

    /// Loads a TrueType/OpenType font from a virtual asset path or filesystem path for the specified `FontStyle` slot.
    bool LoadFontFile(keel::StringRef path, FontStyle style = FontStyle::Regular) override;

    /// Loads a TrueType/OpenType font directly from an in-memory buffer for the specified `FontStyle` slot.
    bool LoadFontMemory(
        keel::StringRef name,
        keel::ArrayRef<keel::u8> data,
        FontStyle style = FontStyle::Regular,
        keel::StringRef mimeType = "ttf"
    ) override;

    // ICanvas implementation
    void Begin(keel::u32 width, keel::u32 height, float scale = 1.0f) override;
    void End() override;
    [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
    void SetScale(float scale) override { m_scale = (scale > 0.0f) ? scale : 1.0f; }
    void PushTransform(const Transform2D& transform) override;
    void PopTransform() override;
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
        keel::u8 opacity = 255
    ) override;
    bool DrawPixels(
        PixelRef pixels,
        Rect destRect,
        float cornerRadius = 0.0f,
        keel::u8 opacity = 255
    ) override;

    void SetGpuShadersEnabled(bool enabled) noexcept override {
        m_gpuShadersEnabled = enabled;
        if (!enabled) {
            m_gpuPasses.clear();
        }
    }
    [[nodiscard]] bool SupportsGpuShaders() const noexcept override { return m_gpuShadersEnabled; }
    bool DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole = true) override;
    bool CaptureRegion(Rect rect, keel::Array<keel::u32>& outPixels, keel::u32& outW, keel::u32& outH) override;
    bool RestoreRegion(Rect rect, PixelRef pixels) override;
    void TrimCaches() noexcept override;

    [[nodiscard]] keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept override { return m_gpuPasses; }
    [[nodiscard]] bool HasActiveGpuPasses() const noexcept override {
        return m_gpuShadersEnabled && !m_gpuPasses.empty();
    }

private:
    struct ImageCacheEntry {
        keel::String resolvedDataOrPath;
        const keel::u8* embeddedData{nullptr};
        keel::usize embeddedSize{0};
        keel::SmallString<16> embeddedMimeType;
        Vec2 size{0.0f, 0.0f};
        bool isInlineSvg{false};
        bool isEmbedded{false};
        bool valid{false};
        keel::u32 cachedRasterW{0};
        keel::u32 cachedRasterH{0};
        keel::Array<keel::u32> cachedPixels;
    };

    ImageCacheEntry& ResolveImageEntry(keel::StringRef source);
    bool LoadFontMemoryInternal(
        keel::StringRef name,
        keel::ArrayRef<keel::u8> data,
        FontStyle style,
        keel::StringRef mimeType,
        bool copyData
    );
    [[nodiscard]] bool NeedsSyntheticBold(FontStyle style) const noexcept;
    [[nodiscard]] bool NeedsSyntheticItalic(FontStyle style) const noexcept;
    void ApplyClip(tvg::Paint* paint, keel::Optional<Rect> bounds = keel::kNullOpt);
    void ApplyFont(tvg::Text* txt, FontStyle style) const;

    // Logical -> physical pixel mapping: p * m_scale + m_offset (scale and translation only)
    [[nodiscard]] float PxX(float x) const noexcept { return x * m_scale + m_offset.x; }
    [[nodiscard]] float PxY(float y) const noexcept { return y * m_scale + m_offset.y; }
    [[nodiscard]] Vec2 Px(Vec2 p) const noexcept { return {PxX(p.x), PxY(p.y)}; }
    [[nodiscard]] Rect Px(Rect r) const noexcept { return Rect(PxX(r.x), PxY(r.y), r.w * m_scale, r.h * m_scale); }
    [[nodiscard]] float FromPxX(float px) const noexcept { return (px - m_offset.x) / m_scale; }
    [[nodiscard]] float FromPxY(float px) const noexcept { return (px - m_offset.y) / m_scale; }
    void PunchHole(Rect rect);
    void FlushCanvas();
    void SyncViewportClip();

    tvg::SwCanvas* m_canvas{nullptr};
    tvg::SwCanvas* m_stampCanvas{nullptr};
    keel::Array<keel::u32> m_stampScratch;
    tvg::Text* m_measureText{nullptr};
    keel::u32* m_targetBuffer{nullptr};
    keel::u32 m_width{0};
    keel::u32 m_height{0};
    keel::u32 m_stride{0};
    float m_scale{1.0f};
    Vec2 m_offset{0.0f, 0.0f};
    struct TransformState {
        float scale;
        Vec2 offset;
    };
    keel::Array<TransformState> m_transformStack;
    bool m_gpuShadersEnabled{false};
    bool m_hasPendingPaints{false};

    keel::Array<GpuShaderPassDesc> m_gpuPasses;

    keel::Array<Rect> m_clipStack;
    keel::Array<Rect> m_gpuClipStack;

    struct FontSlot {
        keel::String name;
        bool loaded{false};
    };
    static constexpr keel::usize kFontStyleCount = 5;
    keel::StaticArray<FontSlot, kFontStyleCount> m_fonts{};

    struct TextMeasureKey {
        keel::u64 hash;
        keel::u32 fontSizeBits;
        keel::u8 style;

        bool operator==(const TextMeasureKey& other) const noexcept {
            return hash == other.hash && fontSizeBits == other.fontSizeBits && style == other.style;
        }
    };

    struct TextMeasureKeyHash {
        keel::u64 operator()(const TextMeasureKey& k) const noexcept {
            keel::u64 mixed = k.hash ^ (static_cast<keel::u64>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL);
            mixed ^= static_cast<keel::u64>(k.style) * 0x517cc1b727220a95ULL;
            return mixed;
        }
    };

    struct TextStampKey {
        keel::u64 textHash{0};
        keel::u32 fontSizeBits{0};
        keel::u32 colorRgba{0};
        keel::u16 textLen{0};
        keel::u8 style{0};

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
            keel::u64 h = k.textHash;
            h ^= static_cast<keel::u64>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL;
            h ^= static_cast<keel::u64>(k.colorRgba) * 0xbf58476d1ce4e5b9ULL;
            h ^= (static_cast<keel::u64>(k.textLen) << 8 | k.style) * 0x94d049bb133111ebULL;
            return h;
        }
    };

    struct TextStampEntry {
        keel::Array<keel::u32> pixels;
        keel::u16 width{0};
        keel::u16 height{0};
        keel::i16 offsetX{0};
        keel::i16 offsetY{0};
        bool empty{false};
    };

    struct ShapeStampKey {
        keel::u32 fillRgba{0};
        keel::u32 strokeRgba{0};
        keel::u16 qW{0};
        keel::u16 qH{0};
        keel::u16 qRad{0};
        keel::u16 qStrokeW{0};

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
            keel::u64 h = static_cast<keel::u64>(k.fillRgba) | (static_cast<keel::u64>(k.strokeRgba) << 32);
            h ^= (static_cast<keel::u64>(k.qW) | (static_cast<keel::u64>(k.qH) << 16) |
                  (static_cast<keel::u64>(k.qRad) << 32) | (static_cast<keel::u64>(k.qStrokeW) << 48)) * 0x9e3779b97f4a7c15ULL;
            h ^= h >> 33;
            return h * 0xff51afd7ed558ccdULL;
        }
    };

    template <typename Key, keel::usize MaxEntries, keel::usize MaxPixelBytes, typename HashFn>
    class StampLruCache {
        static constexpr keel::u32 kNil = 0xFFFFFFFFu;

        struct Node {
            Key key{};
            TextStampEntry entry{};
            keel::u32 prev{kNil};
            keel::u32 next{kNil};
        };

    public:
        [[nodiscard]] const TextStampEntry* Get(const Key& key) noexcept {
            const keel::u32* idxPtr = m_map.get(key);
            if (!idxPtr) {
                return nullptr;
            }
            const keel::u32 idx = *idxPtr;
            MoveToFront(idx);
            return &m_nodes[idx].entry;
        }

        const TextStampEntry& Insert(const Key& key, TextStampEntry&& entry) {
            const keel::usize newBytes = entry.pixels.capacity() * sizeof(keel::u32);
            if (const keel::u32* existingPtr = m_map.get(key)) {
                const keel::u32 idx = *existingPtr;
                const keel::usize oldBytes = m_nodes[idx].entry.pixels.capacity() * sizeof(keel::u32);
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

            keel::u32 slot = kNil;
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
                slot = static_cast<keel::u32>(m_nodes.size());
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
            const keel::u32 slot = m_tail;
            Node& victim = m_nodes[slot];
            const keel::usize oldBytes = victim.entry.pixels.capacity() * sizeof(keel::u32);
            m_totalPixelBytes = (m_totalPixelBytes >= oldBytes) ? (m_totalPixelBytes - oldBytes) : 0u;
            (void)m_map.remove(victim.key);
            Detach(slot);
            victim.entry.pixels = keel::Array<keel::u32>{};
            victim.next = m_freeHead;
            m_freeHead = slot;
        }

        void Detach(keel::u32 idx) noexcept {
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

        void AttachFront(keel::u32 idx) noexcept {
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

        void MoveToFront(keel::u32 idx) noexcept {
            if (idx == m_head) {
                return;
            }
            Detach(idx);
            AttachFront(idx);
        }

        keel::Array<Node> m_nodes;
        keel::HashMap<Key, keel::u32, HashFn> m_map;
        keel::usize m_totalPixelBytes{0};
        keel::u32 m_head{kNil};
        keel::u32 m_tail{kNil};
        keel::u32 m_freeHead{kNil};
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
        keel::i32 dstX,
        keel::i32 dstY,
        keel::Optional<Rect> extraClip = keel::kNullOpt
    );

    static constexpr keel::usize kMaxTextStampBytes = 2u * 1024u * 1024u;
    static constexpr keel::usize kMaxShapeStampBytes = 1u * 1024u * 1024u;
    static constexpr keel::usize kMaxImageCacheEntries = 48u;
    static constexpr keel::usize kMaxImageCacheBytes = 4u * 1024u * 1024u;

    keel::LruCache<TextMeasureKey, Vec2, 1024, TextMeasureKeyHash> m_textMeasureCache;
    StampLruCache<TextStampKey, 512, kMaxTextStampBytes, TextStampKeyHash> m_textStampCache;
    StampLruCache<ShapeStampKey, 192, kMaxShapeStampBytes, ShapeStampKeyHash> m_shapeStampCache;
    keel::HashMap<keel::String, ImageCacheEntry> m_imageCache;
    keel::usize m_imageCacheBytes{0};
};

/// Creates a new `ThorCanvas` instance.
[[nodiscard]] keel::UniquePtr<ICanvas> CreateThorVectorCanvas(bool loadDefaultFonts = true);

/// Registers the ThorVG vector canvas factory and image decoder as the default vector backend.
void RegisterThorVectorBackend() noexcept;

} // namespace guikit
