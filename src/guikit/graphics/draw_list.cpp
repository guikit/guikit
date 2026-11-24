#include "guikit/graphics/draw_list.hpp"

#include "guikit/style/tokens.hpp"
#include "keel/memory.h"

namespace guikit {

void ICanvas::Submit(const DrawList& list) {
    list.Replay(*this);
}

void DrawList::Reset(keel::Arena* externalArena) noexcept {
    m_commands.clear();
    m_externalArena = externalArena;
    if (!m_externalArena) {
        m_ownedArena.reset();
    }
}

keel::StringRef DrawList::CopyString(keel::StringRef s) {
    if (s.empty()) {
        return {};
    }
    char* buf = ActiveArena().alloc_array<char>(s.size());
    if (!buf) {
        return {};
    }
    keel::mem_copy(buf, s.data(), s.size());
    return {buf, s.size()};
}

void DrawList::PushClear(Color color, keel::Optional<Rect> rect) {
    DrawCommand cmd{};
    cmd.type = DrawCommandType::Clear;
    cmd.color = color;
    cmd.flag = rect.has_value();
    if (rect.has_value()) {
        cmd.rect = *rect;
    }
    m_commands.push_back(cmd);
}

void DrawList::PushClip(Rect clipRect) {
    DrawCommand cmd{};
    cmd.type = DrawCommandType::PushClip;
    cmd.rect = clipRect;
    m_commands.push_back(cmd);
}

void DrawList::PopClip() {
    DrawCommand cmd{};
    cmd.type = DrawCommandType::PopClip;
    m_commands.push_back(cmd);
}

void DrawList::PushTransform(const Transform2D& transform) {
    DrawCommand cmd{};
    cmd.type = DrawCommandType::PushTransform;
    cmd.p0 = transform.scale;
    cmd.p1 = transform.offset.x;
    cmd.p2 = transform.offset.y;
    m_commands.push_back(cmd);
}

void DrawList::PopTransform() {
    DrawCommand cmd{};
    cmd.type = DrawCommandType::PopTransform;
    m_commands.push_back(cmd);
}

void DrawList::PushRect(Rect rect, Color fill) {
    if (fill.a == 0u || rect.w <= 0.0f || rect.h <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawRect;
    cmd.rect = rect;
    cmd.color = fill;
    m_commands.push_back(cmd);
}

void DrawList::PushRoundRect(Rect rect, keel::f32 radius, Color fill, Color stroke, keel::f32 strokeWidth) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    if (fill.a == 0u && (stroke.a == 0u || strokeWidth <= 0.0f)) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawRoundRect;
    cmd.rect = rect;
    cmd.color = fill;
    cmd.strokeColor = stroke;
    cmd.p0 = radius;
    cmd.p1 = strokeWidth;
    m_commands.push_back(cmd);
}

void DrawList::PushShadow(Rect rect, keel::f32 cornerRadius, keel::f32 blurRadius, Vec2 offset, Color color, keel::f32 spread) {
    if (color.a == 0u || rect.w <= 0.0f || rect.h <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawShadow;
    cmd.rect = rect;
    cmd.color = color;
    cmd.p0 = cornerRadius;
    cmd.p1 = blurRadius;
    cmd.p2 = offset.x;
    cmd.p3 = offset.y;
    cmd.auxRect.x = spread;
    m_commands.push_back(cmd);
}

void DrawList::PushText(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0u) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawText;
    cmd.text = CopyString(text);
    cmd.rect = Rect(pos.x, pos.y, 0.0f, 0.0f);
    cmd.p0 = fontSize;
    cmd.color = color;
    m_commands.push_back(cmd);
}

void DrawList::PushTextStyled(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color, FontStyle style, keel::f32 angleDeg) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0u) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawTextStyled;
    cmd.text = CopyString(text);
    cmd.rect = Rect(pos.x, pos.y, 0.0f, 0.0f);
    cmd.p0 = fontSize;
    cmd.p1 = angleDeg;
    cmd.fontStyle = style;
    cmd.color = color;
    m_commands.push_back(cmd);
}

void DrawList::PushTextBox(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, keel::f32 alignX, keel::f32 alignY) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0u || bounds.w <= 0.0f || bounds.h <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawTextBox;
    cmd.text = CopyString(text);
    cmd.rect = bounds;
    cmd.p0 = fontSize;
    cmd.p1 = alignX;
    cmd.p2 = alignY;
    cmd.color = color;
    m_commands.push_back(cmd);
}

void DrawList::PushTextBoxStyled(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, FontStyle style, keel::f32 alignX, keel::f32 alignY) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0u || bounds.w <= 0.0f || bounds.h <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawTextBoxStyled;
    cmd.text = CopyString(text);
    cmd.rect = bounds;
    cmd.p0 = fontSize;
    cmd.p1 = alignX;
    cmd.p2 = alignY;
    cmd.fontStyle = style;
    cmd.color = color;
    m_commands.push_back(cmd);
}

void DrawList::PushLine(Vec2 p1, Vec2 p2, Color color, keel::f32 strokeWidth) {
    if (color.a == 0u || strokeWidth <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawLine;
    cmd.rect = Rect(p1.x, p1.y, p2.x, p2.y);
    cmd.color = color;
    cmd.p0 = strokeWidth;
    m_commands.push_back(cmd);
}

void DrawList::PushArc(Vec2 center, keel::f32 radius, keel::f32 startAngleRad, keel::f32 endAngleRad, Color color, keel::f32 strokeWidth, bool roundCaps) {
    if (color.a == 0u || radius <= 0.0f || strokeWidth <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawArc;
    cmd.rect = Rect(center.x, center.y, radius, strokeWidth);
    cmd.p0 = startAngleRad;
    cmd.p1 = endAngleRad;
    cmd.flag = roundCaps;
    cmd.color = color;
    m_commands.push_back(cmd);
}

void DrawList::PushTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color fill, Color stroke, keel::f32 strokeWidth) {
    if (fill.a == 0u && (stroke.a == 0u || strokeWidth <= 0.0f)) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawTriangle;
    cmd.rect = Rect(p1.x, p1.y, p2.x, p2.y);
    cmd.auxRect = Rect(p3.x, p3.y, 0.0f, 0.0f);
    cmd.color = fill;
    cmd.strokeColor = stroke;
    cmd.p0 = strokeWidth;
    m_commands.push_back(cmd);
}

void DrawList::PushPath(const VectorPath& path, const PathPaint& paint) {
    if (path.Empty()) return;
    keel::Arena& arena = ActiveArena();
    auto* payload = arena.alloc_array<DrawPathPayload>(1u);
    if (!payload) return;

    const keel::usize nVerbs = path.verbs.size();
    const keel::usize nPts = path.points.size();
    PathVerb* verbsBuf = arena.alloc_array<PathVerb>(nVerbs);
    Vec2* ptsBuf = arena.alloc_array<Vec2>(nPts);
    if (!verbsBuf || !ptsBuf) return;

    keel::mem_copy(verbsBuf, path.verbs.data(), nVerbs * sizeof(PathVerb));
    keel::mem_copy(ptsBuf, path.points.data(), nPts * sizeof(Vec2));
    payload->verbs = keel::ArrayRef<PathVerb>(verbsBuf, nVerbs);
    payload->points = keel::ArrayRef<Vec2>(ptsBuf, nPts);
    payload->paint = paint;

    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawPath;
    cmd.payload = payload;
    m_commands.push_back(cmd);
}

void DrawList::PushImage(keel::StringRef source, Rect destRect, keel::f32 cornerRadius, keel::u8 opacity) {
    if (source.empty() || opacity == 0u || destRect.w <= 0.0f || destRect.h <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawImage;
    cmd.text = CopyString(source);
    cmd.rect = destRect;
    cmd.p0 = cornerRadius;
    cmd.opacity = opacity;
    m_commands.push_back(cmd);
}

void DrawList::PushPixels(PixelRef pixels, Rect destRect, keel::f32 cornerRadius, keel::u8 opacity, bool copyPixels) {
    if (pixels.Empty() || opacity == 0u || destRect.w <= 0.0f || destRect.h <= 0.0f) return;
    PixelRef stored = pixels;
    if (copyPixels) {
        const keel::usize count = keel::as<keel::usize>(pixels.Width()) * pixels.Height();
        keel::u32* buf = ActiveArena().alloc_array<keel::u32>(count);
        if (!buf) return;
        for (keel::u32 y = 0u; y < pixels.Height(); ++y) {
            const auto row = pixels.Row(y);
            keel::mem_copy(buf + keel::as<keel::usize>(y) * pixels.Width(), row.data(), pixels.Width() * sizeof(keel::u32));
        }
        stored = PixelRef(keel::ArrayRef<keel::u32>(buf, count), pixels.Width(), pixels.Height());
    }
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawPixels;
    cmd.pixels = stored;
    cmd.rect = destRect;
    cmd.p0 = cornerRadius;
    cmd.opacity = opacity;
    m_commands.push_back(cmd);
}

void DrawList::PushTexture(keel::u64 textureId, Rect destRect, keel::Optional<Rect> srcRect, Color tint) {
    if (textureId == 0u || tint.a == 0u || destRect.w <= 0.0f || destRect.h <= 0.0f) return;
    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawTexture;
    cmd.handle = textureId;
    cmd.rect = destRect;
    cmd.flag = srcRect.has_value();
    if (srcRect.has_value()) {
        cmd.auxRect = *srcRect;
    }
    cmd.color = tint;
    m_commands.push_back(cmd);
}

void DrawList::PushGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole) {
    auto* payload = ActiveArena().alloc_array<DrawGpuPassPayload>(1u);
    if (!payload) return;
    payload->desc = pass;
    payload->desc.shader.shaderId = CopyString(pass.shader.shaderId);
    payload->desc.shader.glslEs2 = CopyString(pass.shader.glslEs2);
    payload->punchHole = punchHole;

    DrawCommand cmd{};
    cmd.type = DrawCommandType::DrawGpuShaderPass;
    cmd.payload = payload;
    m_commands.push_back(cmd);
}

void DrawList::Replay(ICanvas& target) const {
    VectorPath scratchPath;
    for (const DrawCommand& cmd : m_commands) {
        switch (cmd.type) {
            case DrawCommandType::Clear:
                target.Clear(cmd.color, cmd.flag ? keel::Optional<Rect>(cmd.rect) : keel::kNullOpt);
                break;
            case DrawCommandType::PushClip:
                target.PushClip(cmd.rect);
                break;
            case DrawCommandType::PopClip:
                target.PopClip();
                break;
            case DrawCommandType::PushTransform:
                target.PushTransform(Transform2D{cmd.p0, Vec2{cmd.p1, cmd.p2}});
                break;
            case DrawCommandType::PopTransform:
                target.PopTransform();
                break;
            case DrawCommandType::DrawRect:
                target.DrawRect(cmd.rect, cmd.color);
                break;
            case DrawCommandType::DrawRoundRect:
                target.DrawRoundRect(cmd.rect, cmd.p0, cmd.color, cmd.strokeColor, cmd.p1);
                break;
            case DrawCommandType::DrawShadow:
                target.DrawShadow(cmd.rect, cmd.p0, cmd.p1, Vec2{cmd.p2, cmd.p3}, cmd.color, cmd.auxRect.x);
                break;
            case DrawCommandType::DrawText:
                target.DrawText(cmd.text, Vec2{cmd.rect.x, cmd.rect.y}, cmd.p0, cmd.color);
                break;
            case DrawCommandType::DrawTextStyled:
                target.DrawTextStyled(cmd.text, Vec2{cmd.rect.x, cmd.rect.y}, cmd.p0, cmd.color, cmd.fontStyle, cmd.p1);
                break;
            case DrawCommandType::DrawTextBox:
                target.DrawTextBox(cmd.text, cmd.rect, cmd.p0, cmd.color, cmd.p1, cmd.p2);
                break;
            case DrawCommandType::DrawTextBoxStyled:
                target.DrawTextBoxStyled(cmd.text, cmd.rect, cmd.p0, cmd.color, cmd.fontStyle, cmd.p1, cmd.p2);
                break;
            case DrawCommandType::DrawLine:
                target.DrawLine(Vec2{cmd.rect.x, cmd.rect.y}, Vec2{cmd.rect.w, cmd.rect.h}, cmd.color, cmd.p0);
                break;
            case DrawCommandType::DrawArc:
                target.DrawArc(Vec2{cmd.rect.x, cmd.rect.y}, cmd.rect.w, cmd.p0, cmd.p1, cmd.color, cmd.rect.h, cmd.flag);
                break;
            case DrawCommandType::DrawTriangle:
                target.DrawTriangle(
                    Vec2{cmd.rect.x, cmd.rect.y},
                    Vec2{cmd.rect.w, cmd.rect.h},
                    Vec2{cmd.auxRect.x, cmd.auxRect.y},
                    cmd.color,
                    cmd.strokeColor,
                    cmd.p0
                );
                break;
            case DrawCommandType::DrawPath:
                if (const auto* payload = static_cast<const DrawPathPayload*>(cmd.payload)) {
                    scratchPath.Clear();
                    scratchPath.verbs.resize(payload->verbs.size());
                    scratchPath.points.resize(payload->points.size());
                    if (!payload->verbs.empty()) {
                        keel::mem_copy(scratchPath.verbs.data(), payload->verbs.data(), payload->verbs.size() * sizeof(PathVerb));
                    }
                    if (!payload->points.empty()) {
                        keel::mem_copy(scratchPath.points.data(), payload->points.data(), payload->points.size() * sizeof(Vec2));
                    }
                    target.DrawPath(scratchPath, payload->paint);
                }
                break;
            case DrawCommandType::DrawImage:
                (void)target.DrawImage(cmd.text, cmd.rect, cmd.p0, cmd.opacity);
                break;
            case DrawCommandType::DrawPixels:
                (void)target.DrawPixels(cmd.pixels, cmd.rect, cmd.p0, cmd.opacity);
                break;
            case DrawCommandType::DrawTexture:
                target.DrawTexture(cmd.handle, cmd.rect, cmd.flag ? keel::Optional<Rect>(cmd.auxRect) : keel::kNullOpt, cmd.color);
                break;
            case DrawCommandType::DrawGpuShaderPass:
                if (const auto* payload = static_cast<const DrawGpuPassPayload*>(cmd.payload)) {
                    (void)target.DrawGpuShaderPass(payload->desc, payload->punchHole);
                }
                break;
        }
    }
}

// =============================================================================
// DrawListRecorder
// =============================================================================

DrawListRecorder::DrawListRecorder(DrawList& list, ICanvas* target) noexcept
    : m_list(list)
    , m_target(target)
    , m_scale(target ? target->GetScale() : 1.0f) {
}

void DrawListRecorder::FlushToTarget() {
    if (!m_target) {
        return;
    }
    const auto cmds = m_list.Commands();
    if (m_flushedCount >= cmds.size()) {
        return;
    }
    // Replay only the newly recorded tail since the last flush
    DrawList tail;
    VectorPath scratchPath;
    for (keel::usize i = m_flushedCount; i < cmds.size(); ++i) {
        const DrawCommand& cmd = cmds[i];
        switch (cmd.type) {
            case DrawCommandType::Clear:
                m_target->Clear(cmd.color, cmd.flag ? keel::Optional<Rect>(cmd.rect) : keel::kNullOpt);
                break;
            case DrawCommandType::PushClip:
                m_target->PushClip(cmd.rect);
                break;
            case DrawCommandType::PopClip:
                m_target->PopClip();
                break;
            case DrawCommandType::PushTransform:
                m_target->PushTransform(Transform2D{cmd.p0, Vec2{cmd.p1, cmd.p2}});
                break;
            case DrawCommandType::PopTransform:
                m_target->PopTransform();
                break;
            case DrawCommandType::DrawRect:
                m_target->DrawRect(cmd.rect, cmd.color);
                break;
            case DrawCommandType::DrawRoundRect:
                m_target->DrawRoundRect(cmd.rect, cmd.p0, cmd.color, cmd.strokeColor, cmd.p1);
                break;
            case DrawCommandType::DrawShadow:
                m_target->DrawShadow(cmd.rect, cmd.p0, cmd.p1, Vec2{cmd.p2, cmd.p3}, cmd.color, cmd.auxRect.x);
                break;
            case DrawCommandType::DrawText:
                m_target->DrawText(cmd.text, Vec2{cmd.rect.x, cmd.rect.y}, cmd.p0, cmd.color);
                break;
            case DrawCommandType::DrawTextStyled:
                m_target->DrawTextStyled(cmd.text, Vec2{cmd.rect.x, cmd.rect.y}, cmd.p0, cmd.color, cmd.fontStyle, cmd.p1);
                break;
            case DrawCommandType::DrawTextBox:
                m_target->DrawTextBox(cmd.text, cmd.rect, cmd.p0, cmd.color, cmd.p1, cmd.p2);
                break;
            case DrawCommandType::DrawTextBoxStyled:
                m_target->DrawTextBoxStyled(cmd.text, cmd.rect, cmd.p0, cmd.color, cmd.fontStyle, cmd.p1, cmd.p2);
                break;
            case DrawCommandType::DrawLine:
                m_target->DrawLine(Vec2{cmd.rect.x, cmd.rect.y}, Vec2{cmd.rect.w, cmd.rect.h}, cmd.color, cmd.p0);
                break;
            case DrawCommandType::DrawArc:
                m_target->DrawArc(Vec2{cmd.rect.x, cmd.rect.y}, cmd.rect.w, cmd.p0, cmd.p1, cmd.color, cmd.rect.h, cmd.flag);
                break;
            case DrawCommandType::DrawTriangle:
                m_target->DrawTriangle(
                    Vec2{cmd.rect.x, cmd.rect.y},
                    Vec2{cmd.rect.w, cmd.rect.h},
                    Vec2{cmd.auxRect.x, cmd.auxRect.y},
                    cmd.color,
                    cmd.strokeColor,
                    cmd.p0
                );
                break;
            case DrawCommandType::DrawPath:
                if (const auto* payload = static_cast<const DrawPathPayload*>(cmd.payload)) {
                    scratchPath.Clear();
                    scratchPath.verbs.resize(payload->verbs.size());
                    scratchPath.points.resize(payload->points.size());
                    if (!payload->verbs.empty()) {
                        keel::mem_copy(scratchPath.verbs.data(), payload->verbs.data(), payload->verbs.size() * sizeof(PathVerb));
                    }
                    if (!payload->points.empty()) {
                        keel::mem_copy(scratchPath.points.data(), payload->points.data(), payload->points.size() * sizeof(Vec2));
                    }
                    m_target->DrawPath(scratchPath, payload->paint);
                }
                break;
            case DrawCommandType::DrawImage:
                (void)m_target->DrawImage(cmd.text, cmd.rect, cmd.p0, cmd.opacity);
                break;
            case DrawCommandType::DrawPixels:
                (void)m_target->DrawPixels(cmd.pixels, cmd.rect, cmd.p0, cmd.opacity);
                break;
            case DrawCommandType::DrawTexture:
                m_target->DrawTexture(cmd.handle, cmd.rect, cmd.flag ? keel::Optional<Rect>(cmd.auxRect) : keel::kNullOpt, cmd.color);
                break;
            case DrawCommandType::DrawGpuShaderPass:
                if (const auto* payload = static_cast<const DrawGpuPassPayload*>(cmd.payload)) {
                    (void)m_target->DrawGpuShaderPass(payload->desc, payload->punchHole);
                }
                break;
        }
    }
    m_flushedCount = cmds.size();
}

SurfacePixelFormat DrawListRecorder::GetPixelFormat() const noexcept {
    return m_target ? m_target->GetPixelFormat() : SurfacePixelFormat::ABGR8888_Premul;
}

void DrawListRecorder::SetTarget(keel::u32* buffer, keel::u32 width, keel::u32 height, keel::u32 stride) {
    if (m_target) {
        m_target->SetTarget(buffer, width, height, stride);
    }
}

bool DrawListRecorder::CaptureRegion(Rect rect, keel::Array<keel::u32>& outPixels, keel::u32& outW, keel::u32& outH) {
    if (!m_target) return false;
    FlushToTarget();
    return m_target->CaptureRegion(rect, outPixels, outW, outH);
}

bool DrawListRecorder::RestoreRegion(Rect rect, PixelRef pixels) {
    if (!m_target) return false;
    FlushToTarget();
    return m_target->RestoreRegion(rect, pixels);
}

void DrawListRecorder::SetGpuShadersEnabled(bool enabled) noexcept {
    if (m_target) {
        m_target->SetGpuShadersEnabled(enabled);
    }
}

bool DrawListRecorder::SupportsGpuShaders() const noexcept {
    return m_target ? m_target->SupportsGpuShaders() : false;
}

bool DrawListRecorder::HasActiveGpuPasses() const noexcept {
    return m_hadGpuPass || (m_target ? m_target->HasActiveGpuPasses() : false);
}

keel::ArrayRef<GpuShaderPassDesc> DrawListRecorder::GetGpuShaderPasses() const noexcept {
    return m_target ? m_target->GetGpuShaderPasses() : keel::ArrayRef<GpuShaderPassDesc>{};
}

bool DrawListRecorder::DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole) {
    if (!m_target || !m_target->SupportsGpuShaders()) {
        return false;
    }
    if (pass.shader.shaderId.empty() || pass.shader.glslEs2.empty() ||
        pass.dataField.empty() || pass.plotRect.w <= 0.0f || pass.plotRect.h <= 0.0f) {
        return false;
    }
    m_hadGpuPass = true;
    m_list.PushGpuShaderPass(pass, punchHole);
    return true;
}

void DrawListRecorder::Begin(keel::u32 /*width*/, keel::u32 /*height*/, keel::f32 scale) {
    m_scale = (scale > 0.0f) ? scale : 1.0f;
    m_xfScale = 1.0f;
    m_xfOffset = {0.0f, 0.0f};
    m_transformStack.clear();
    m_flushedCount = 0u;
    m_hadGpuPass = false;
}

void DrawListRecorder::End() {
    if (m_target && m_flushedCount > 0u) {
        FlushToTarget();
    }
}

void DrawListRecorder::PushTransform(const Transform2D& transform) {
    m_transformStack.push_back(TransformState{m_xfScale, m_xfOffset});
    const Transform2D combined = Transform2D{m_xfScale, m_xfOffset}.Then(transform);
    m_xfScale = combined.scale;
    m_xfOffset = combined.offset;
    m_list.PushTransform(transform);
}

void DrawListRecorder::PopTransform() {
    if (!m_transformStack.empty()) {
        const TransformState prev = m_transformStack.back();
        m_transformStack.pop_back();
        m_xfScale = prev.scale;
        m_xfOffset = prev.offset;
    }
    m_list.PopTransform();
}

void DrawListRecorder::Clear(Color color, keel::Optional<Rect> rect) {
    m_list.PushClear(color, rect);
}

void DrawListRecorder::PushClip(Rect clipRect) {
    m_list.PushClip(clipRect);
}

void DrawListRecorder::PopClip() {
    m_list.PopClip();
}

void DrawListRecorder::DrawRect(Rect rect, Color fill) {
    m_list.PushRect(rect, fill);
}

void DrawListRecorder::DrawRoundRect(Rect rect, keel::f32 radius, Color fill, Color stroke, keel::f32 strokeWidth) {
    m_list.PushRoundRect(rect, radius, fill, stroke, strokeWidth);
}

void DrawListRecorder::DrawShadow(Rect rect, keel::f32 cornerRadius, keel::f32 blurRadius, Vec2 offset, Color color, keel::f32 spread) {
    m_list.PushShadow(rect, cornerRadius, blurRadius, offset, color, spread);
}

Vec2 DrawListRecorder::MeasureText(keel::StringRef text, keel::f32 fontSize) {
    if (m_target) return m_target->MeasureText(text, fontSize);
    return {keel::as<keel::f32>(text.size()) * fontSize * kDefaultCharAdvanceRatio, fontSize * kDefaultLineHeightMultiplier};
}

Vec2 DrawListRecorder::MeasureTextStyled(keel::StringRef text, keel::f32 fontSize, FontStyle style) {
    if (m_target) return m_target->MeasureTextStyled(text, fontSize, style);
    return MeasureText(text, fontSize);
}

keel::f32 DrawListRecorder::GetLineHeight(keel::f32 fontSize) {
    if (m_target) return m_target->GetLineHeight(fontSize);
    return fontSize * kDefaultLineHeightMultiplier;
}

void DrawListRecorder::DrawText(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color) {
    m_list.PushText(text, pos, fontSize, color);
}

void DrawListRecorder::DrawTextStyled(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color, FontStyle style, keel::f32 angleDeg) {
    m_list.PushTextStyled(text, pos, fontSize, color, style, angleDeg);
}

void DrawListRecorder::DrawTextBox(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, keel::f32 alignX, keel::f32 alignY) {
    m_list.PushTextBox(text, bounds, fontSize, color, alignX, alignY);
}

void DrawListRecorder::DrawTextBoxStyled(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, FontStyle style, keel::f32 alignX, keel::f32 alignY) {
    m_list.PushTextBoxStyled(text, bounds, fontSize, color, style, alignX, alignY);
}

void DrawListRecorder::DrawLine(Vec2 p1, Vec2 p2, Color color, keel::f32 strokeWidth) {
    m_list.PushLine(p1, p2, color, strokeWidth);
}

void DrawListRecorder::DrawArc(Vec2 center, keel::f32 radius, keel::f32 startAngleRad, keel::f32 endAngleRad, Color color, keel::f32 strokeWidth, bool roundCaps) {
    m_list.PushArc(center, radius, startAngleRad, endAngleRad, color, strokeWidth, roundCaps);
}

void DrawListRecorder::DrawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color fill, Color stroke, keel::f32 strokeWidth) {
    m_list.PushTriangle(p1, p2, p3, fill, stroke, strokeWidth);
}

void DrawListRecorder::DrawPath(const VectorPath& path, const PathPaint& paint) {
    m_list.PushPath(path, paint);
}

Vec2 DrawListRecorder::GetImageSize(keel::StringRef source) {
    return m_target ? m_target->GetImageSize(source) : Vec2{0.0f, 0.0f};
}

bool DrawListRecorder::DrawImage(keel::StringRef source, Rect destRect, keel::f32 cornerRadius, keel::u8 opacity) {
    if (source.empty() || opacity == 0u || destRect.w <= 0.0f || destRect.h <= 0.0f) {
        return false;
    }
    if (m_target && m_target->GetBackendName() != "NullCanvas (Headless)") {
        const Vec2 sz = m_target->GetImageSize(source);
        if (sz.x <= 0.0f || sz.y <= 0.0f) {
            return false;
        }
    }
    m_list.PushImage(source, destRect, cornerRadius, opacity);
    return true;
}

bool DrawListRecorder::DrawPixels(PixelRef pixels, Rect destRect, keel::f32 cornerRadius, keel::u8 opacity) {
    if (pixels.Empty() || opacity == 0u || destRect.w <= 0.0f || destRect.h <= 0.0f) {
        return false;
    }
    m_list.PushPixels(pixels, destRect, cornerRadius, opacity, false);
    return true;
}

void DrawListRecorder::DrawTexture(keel::u64 textureId, Rect destRect, keel::Optional<Rect> srcRect, Color tint) {
    m_list.PushTexture(textureId, destRect, srcRect, tint);
}

keel::StringRef DrawListRecorder::GetTextEngineName() const noexcept {
    return m_target ? m_target->GetTextEngineName() : keel::StringRef("DrawList");
}

bool DrawListRecorder::SupportsFontHinting() const noexcept {
    return m_target ? m_target->SupportsFontHinting() : false;
}

void DrawListRecorder::SetTextRenderSettings(const TextRenderSettings& settings) noexcept {
    if (m_target) {
        m_target->SetTextRenderSettings(settings);
    }
}

const TextRenderSettings& DrawListRecorder::GetTextRenderSettings() const noexcept {
    if (m_target) {
        return m_target->GetTextRenderSettings();
    }
    static constexpr TextRenderSettings kDefault{};
    return kDefault;
}

} // namespace guikit
