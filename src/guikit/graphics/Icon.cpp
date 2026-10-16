#include "guikit/graphics/Icon.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/backend/ICanvas.hpp"

namespace guikit {

void Icon::Draw(ICanvas& canvas, Rect bounds, Color tint) const {
    if (m_type == SourceType::None || bounds.w <= 0.0f || bounds.h <= 0.0f || tint.a == 0) {
        return;
    }

    if (m_type == SourceType::Builtin) {
        const float s = std::min(bounds.w, bounds.h);
        const float cx = bounds.x + bounds.w * 0.5f;
        const float cy = bounds.y + bounds.h * 0.5f;
        const float stroke = std::max(1.5f, s * 0.12f);
        const float hw = s * 0.5f;

        switch (m_id) {
            case IconId::Close: {
                const float pad = s * 0.24f;
                canvas.DrawLine(Vec2(cx - hw + pad, cy - hw + pad), Vec2(cx + hw - pad, cy + hw - pad), tint, stroke);
                canvas.DrawLine(Vec2(cx + hw - pad, cy - hw + pad), Vec2(cx - hw + pad, cy + hw - pad), tint, stroke);
                break;
            }

            case IconId::Check: {
                canvas.DrawLine(Vec2(cx - s * 0.30f, cy + s * 0.05f), Vec2(cx - s * 0.05f, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.05f, cy + s * 0.30f), Vec2(cx + s * 0.35f, cy - s * 0.25f), tint, stroke);
                break;
            }

            case IconId::Plus: {
                const float arm = s * 0.30f;
                canvas.DrawLine(Vec2(cx - arm, cy), Vec2(cx + arm, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - arm), Vec2(cx, cy + arm), tint, stroke);
                break;
            }

            case IconId::Minus: {
                const float arm = s * 0.30f;
                canvas.DrawLine(Vec2(cx - arm, cy), Vec2(cx + arm, cy), tint, stroke);
                break;
            }

            case IconId::Play: {
                const Vec2 pTop(cx - s * 0.24f, cy - s * 0.32f);
                const Vec2 pBot(cx - s * 0.24f, cy + s * 0.32f);
                const Vec2 pRight(cx + s * 0.34f, cy);
                canvas.DrawTriangle(pTop, pBot, pRight, tint, tint, stroke * 0.65f);
                break;
            }

            case IconId::Pause: {
                const float barW = std::max(2.0f, s * 0.16f);
                const float barH = s * 0.60f;
                const float gap = s * 0.12f;
                canvas.DrawRect(Rect(cx - gap - barW, cy - barH * 0.5f, barW, barH), tint);
                canvas.DrawRect(Rect(cx + gap, cy - barH * 0.5f, barW, barH), tint);
                break;
            }

            case IconId::Stop: {
                const float sz = s * 0.56f;
                canvas.DrawRoundRect(Rect(cx - sz * 0.5f, cy - sz * 0.5f, sz, sz), std::max(1.5f, s * 0.10f), tint);
                break;
            }

            case IconId::Refresh: {
                const float r = s * 0.31f;
                // Draw a 280-degree open circular arc (from +35 deg to +315 deg)
                constexpr int kSegs = 12;
                const float startAng = 0.610865f;  // 35 deg
                const float endAng   = 5.497787f;  // 315 deg (-45 deg, top-right)
                Vec2 prevPt(cx + r * std::cos(startAng), cy + r * std::sin(startAng));
                for (int i = 1; i <= kSegs; ++i) {
                    const float t = static_cast<float>(i) / static_cast<float>(kSegs);
                    const float ang = startAng + t * (endAng - startAng);
                    const Vec2 curPt(cx + r * std::cos(ang), cy + r * std::sin(ang));
                    canvas.DrawLine(prevPt, curPt, tint, stroke);
                    prevPt = curPt;
                }
                // Crisp arrowhead at endAng (top-right, pointing clockwise / down-right)
                const float wing = s * 0.19f;
                canvas.DrawLine(prevPt, Vec2(prevPt.x - wing, prevPt.y - wing * 0.15f), tint, stroke);
                canvas.DrawLine(prevPt, Vec2(prevPt.x + wing * 0.12f, prevPt.y - wing), tint, stroke);
                break;
            }

            case IconId::ArrowLeft: {
                canvas.DrawLine(Vec2(cx - s * 0.30f, cy), Vec2(cx + s * 0.30f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.10f, cy - s * 0.22f), Vec2(cx - s * 0.30f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.10f, cy + s * 0.22f), Vec2(cx - s * 0.30f, cy), tint, stroke);
                break;
            }

            case IconId::ArrowRight: {
                canvas.DrawLine(Vec2(cx - s * 0.30f, cy), Vec2(cx + s * 0.30f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.10f, cy - s * 0.22f), Vec2(cx + s * 0.30f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.10f, cy + s * 0.22f), Vec2(cx + s * 0.30f, cy), tint, stroke);
                break;
            }

            case IconId::ArrowUp: {
                canvas.DrawLine(Vec2(cx, cy - s * 0.30f), Vec2(cx, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.22f, cy - s * 0.10f), Vec2(cx, cy - s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.22f, cy - s * 0.10f), Vec2(cx, cy - s * 0.30f), tint, stroke);
                break;
            }

            case IconId::ArrowDown: {
                canvas.DrawLine(Vec2(cx, cy - s * 0.30f), Vec2(cx, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.22f, cy + s * 0.10f), Vec2(cx, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.22f, cy + s * 0.10f), Vec2(cx, cy + s * 0.30f), tint, stroke);
                break;
            }

            case IconId::ChevronLeft: {
                canvas.DrawLine(Vec2(cx + s * 0.15f, cy - s * 0.28f), Vec2(cx - s * 0.15f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.15f, cy), Vec2(cx + s * 0.15f, cy + s * 0.28f), tint, stroke);
                break;
            }

            case IconId::ChevronRight: {
                canvas.DrawLine(Vec2(cx - s * 0.15f, cy - s * 0.28f), Vec2(cx + s * 0.15f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.15f, cy), Vec2(cx - s * 0.15f, cy + s * 0.28f), tint, stroke);
                break;
            }

            case IconId::ChevronDown: {
                canvas.DrawLine(Vec2(cx - s * 0.28f, cy - s * 0.15f), Vec2(cx, cy + s * 0.15f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy + s * 0.15f), Vec2(cx + s * 0.28f, cy - s * 0.15f), tint, stroke);
                break;
            }

            case IconId::ChevronUp: {
                canvas.DrawLine(Vec2(cx - s * 0.28f, cy + s * 0.15f), Vec2(cx, cy - s * 0.15f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.15f), Vec2(cx + s * 0.28f, cy + s * 0.15f), tint, stroke);
                break;
            }

            case IconId::Settings: {
                const float rHub = s * 0.22f;
                const float rTeeth = s * 0.38f;
                const float toothStroke = std::max(2.0f, stroke * 1.3f);

                // 6 radial gear teeth spaced at 60 degree intervals
                for (int i = 0; i < 6; ++i) {
                    const float angle = static_cast<float>(i) * 1.04719755f; // pi / 3
                    const float ca = std::cos(angle);
                    const float sa = std::sin(angle);
                    canvas.DrawLine(
                        Vec2(cx + rHub * 0.8f * ca, cy + rHub * 0.8f * sa),
                        Vec2(cx + rTeeth * ca, cy + rTeeth * sa),
                        tint,
                        toothStroke
                    );
                }

                // Gear hub ring
                canvas.DrawRoundRect(Rect(cx - rHub, cy - rHub, rHub * 2.0f, rHub * 2.0f), rHub, Color(0, 0, 0, 0), tint, stroke);

                // Central axle hole
                const float rHole = s * 0.10f;
                canvas.DrawRoundRect(Rect(cx - rHole, cy - rHole, rHole * 2.0f, rHole * 2.0f), rHole, tint);
                break;
            }

            case IconId::Save: {
                const float sz = s * 0.60f;
                canvas.DrawRoundRect(Rect(cx - sz * 0.5f, cy - sz * 0.5f, sz, sz), 2.0f, Color(0, 0, 0, 0), tint, stroke);
                canvas.DrawRect(Rect(cx - sz * 0.30f, cy - sz * 0.5f + stroke, sz * 0.60f, sz * 0.32f), tint);
                break;
            }

            case IconId::Search: {
                const float r = s * 0.22f;
                const Vec2 lCenter(cx - s * 0.08f, cy - s * 0.08f);
                canvas.DrawRoundRect(Rect(lCenter.x - r, lCenter.y - r, r * 2.0f, r * 2.0f), r, Color(0, 0, 0, 0), tint, stroke);
                canvas.DrawLine(Vec2(lCenter.x + r * 0.70f, lCenter.y + r * 0.70f), Vec2(cx + s * 0.32f, cy + s * 0.32f), tint, stroke);
                break;
            }

            case IconId::Trash: {
                canvas.DrawLine(Vec2(cx - s * 0.30f, cy - s * 0.22f), Vec2(cx + s * 0.30f, cy - s * 0.22f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.22f, cy - s * 0.22f), Vec2(cx - s * 0.18f, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.22f, cy - s * 0.22f), Vec2(cx + s * 0.18f, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.18f, cy + s * 0.30f), Vec2(cx + s * 0.18f, cy + s * 0.30f), tint, stroke);
                break;
            }

            case IconId::Folder: {
                canvas.DrawRoundRect(Rect(cx - s * 0.32f, cy - s * 0.16f, s * 0.64f, s * 0.44f), 2.0f, Color(0, 0, 0, 0), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.32f, cy - s * 0.16f), Vec2(cx - s * 0.12f, cy - s * 0.26f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.12f, cy - s * 0.26f), Vec2(cx + s * 0.06f, cy - s * 0.26f), tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.06f, cy - s * 0.26f), Vec2(cx + s * 0.16f, cy - s * 0.16f), tint, stroke);
                break;
            }

            case IconId::Eye: {
                const float r = s * 0.16f;
                canvas.DrawRoundRect(Rect(cx - r, cy - r, r * 2.0f, r * 2.0f), r, tint);
                canvas.DrawLine(Vec2(cx - s * 0.35f, cy), Vec2(cx, cy - s * 0.22f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.22f), Vec2(cx + s * 0.35f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.35f, cy), Vec2(cx, cy + s * 0.22f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy + s * 0.22f), Vec2(cx + s * 0.35f, cy), tint, stroke);
                break;
            }

            case IconId::EyeOff: {
                const float r = s * 0.16f;
                canvas.DrawRoundRect(Rect(cx - r, cy - r, r * 2.0f, r * 2.0f), r, tint);
                canvas.DrawLine(Vec2(cx - s * 0.35f, cy), Vec2(cx, cy - s * 0.22f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.22f), Vec2(cx + s * 0.35f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.35f, cy), Vec2(cx, cy + s * 0.22f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy + s * 0.22f), Vec2(cx + s * 0.35f, cy), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.32f, cy - s * 0.32f), Vec2(cx + s * 0.32f, cy + s * 0.32f), tint, stroke);
                break;
            }

            case IconId::Edit: {
                // Diagonal pencil: body shaft + tip point + paper base
                canvas.DrawLine(Vec2(cx - s * 0.18f, cy + s * 0.18f), Vec2(cx + s * 0.20f, cy - s * 0.20f), tint, stroke * 1.4f);
                canvas.DrawLine(Vec2(cx - s * 0.18f, cy + s * 0.18f), Vec2(cx - s * 0.30f, cy + s * 0.30f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.30f, cy + s * 0.32f), Vec2(cx + s * 0.30f, cy + s * 0.32f), tint, stroke);
                break;
            }

            case IconId::Copy: {
                // Two overlapping document outlines
                canvas.DrawRoundRect(Rect(cx - s * 0.10f, cy - s * 0.30f, s * 0.40f, s * 0.46f), 1.5f, Color(0, 0, 0, 0), tint, stroke);
                canvas.DrawRoundRect(Rect(cx - s * 0.30f, cy - s * 0.10f, s * 0.40f, s * 0.46f), 1.5f, Color(0, 0, 0, 0), tint, stroke);
                break;
            }

            case IconId::Info: {
                const float r = s * 0.40f;
                canvas.DrawRoundRect(Rect(cx - r, cy - r, r * 2.0f, r * 2.0f), r, Color(0, 0, 0, 0), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.04f), Vec2(cx, cy + s * 0.24f), tint, stroke * 1.2f);
                canvas.DrawRoundRect(Rect(cx - stroke * 0.6f, cy - s * 0.24f, stroke * 1.2f, stroke * 1.2f), stroke * 0.6f, tint);
                break;
            }

            case IconId::Warning: {
                canvas.DrawLine(Vec2(cx, cy - s * 0.38f), Vec2(cx - s * 0.40f, cy + s * 0.34f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.38f), Vec2(cx + s * 0.40f, cy + s * 0.34f), tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.40f, cy + s * 0.34f), Vec2(cx + s * 0.40f, cy + s * 0.34f), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.12f), Vec2(cx, cy + s * 0.12f), tint, stroke * 1.15f);
                canvas.DrawRoundRect(Rect(cx - stroke * 0.6f, cy + s * 0.20f, stroke * 1.2f, stroke * 1.2f), stroke * 0.6f, tint);
                break;
            }

            case IconId::Flash: {
                canvas.DrawLine(Vec2(cx + s * 0.08f, cy - s * 0.38f), Vec2(cx - s * 0.22f, cy + s * 0.04f), tint, stroke * 1.1f);
                canvas.DrawLine(Vec2(cx - s * 0.22f, cy + s * 0.04f), Vec2(cx + s * 0.14f, cy + s * 0.04f), tint, stroke * 1.1f);
                canvas.DrawLine(Vec2(cx + s * 0.14f, cy + s * 0.04f), Vec2(cx - s * 0.08f, cy + s * 0.38f), tint, stroke * 1.1f);
                break;
            }

            case IconId::Error: {
                const float r = s * 0.40f;
                canvas.DrawRoundRect(Rect(cx - r, cy - r, r * 2.0f, r * 2.0f), r, Color(0, 0, 0, 0), tint, stroke);
                canvas.DrawLine(Vec2(cx, cy - s * 0.22f), Vec2(cx, cy + s * 0.06f), tint, stroke * 1.2f);
                canvas.DrawRoundRect(Rect(cx - stroke * 0.6f, cy + s * 0.16f, stroke * 1.2f, stroke * 1.2f), stroke * 0.6f, tint);
                break;
            }

            case IconId::DebugContinue: {
                // VS Code Continue: Left vertical bar + Right filled play triangle
                const float barW = std::max(1.8f, stroke * 1.15f);
                const float barH = s * 0.60f;
                canvas.DrawRoundRect(Rect(cx - s * 0.36f, cy - barH * 0.5f, barW, barH), barW * 0.5f, tint);
                const Vec2 pTop(cx - s * 0.12f, cy - s * 0.30f);
                const Vec2 pBot(cx - s * 0.12f, cy + s * 0.30f);
                const Vec2 pRight(cx + s * 0.36f, cy);
                canvas.DrawTriangle(pTop, pBot, pRight, tint, tint, stroke * 0.55f);
                break;
            }

            case IconId::StepOver: {
                // VS Code Step Over: Bottom dot + curved arc jumping over the dot with arrowhead on the right
                const float dotR = std::max(1.6f, s * 0.11f);
                canvas.DrawRoundRect(Rect(cx - dotR, cy + s * 0.24f - dotR, dotR * 2.0f, dotR * 2.0f), dotR, tint);

                const float arcCy = cy + s * 0.08f;
                const float arcR = s * 0.33f;
                constexpr int kSegs = 8;
                const float startAng = -2.65f; // ~ -152 deg (left-bottom)
                const float endAng   = -0.49f; // ~ -28 deg (right-bottom)
                Vec2 prevPt(cx + arcR * std::cos(startAng), arcCy + arcR * std::sin(startAng));
                for (int i = 1; i <= kSegs; ++i) {
                    const float t = static_cast<float>(i) / static_cast<float>(kSegs);
                    const float ang = startAng + t * (endAng - startAng);
                    const Vec2 curPt(cx + arcR * std::cos(ang), arcCy + arcR * std::sin(ang));
                    canvas.DrawLine(prevPt, curPt, tint, stroke);
                    prevPt = curPt;
                }
                const float wing = s * 0.19f;
                canvas.DrawLine(prevPt, Vec2(prevPt.x - wing, prevPt.y - wing * 0.10f), tint, stroke);
                canvas.DrawLine(prevPt, Vec2(prevPt.x + wing * 0.08f, prevPt.y - wing), tint, stroke);
                break;
            }

            case IconId::StepInto: {
                // VS Code Step Into: Vertical arrow pointing down toward a bottom dot
                const float dotR = std::max(1.6f, s * 0.11f);
                canvas.DrawRoundRect(Rect(cx - dotR, cy + s * 0.27f - dotR, dotR * 2.0f, dotR * 2.0f), dotR, tint);

                const Vec2 tip(cx, cy + s * 0.07f);
                canvas.DrawLine(Vec2(cx, cy - s * 0.35f), tip, tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.21f, cy - s * 0.12f), tip, tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.21f, cy - s * 0.12f), tip, tint, stroke);
                break;
            }

            case IconId::StepOut: {
                // VS Code Step Out: Vertical arrow pointing up away from a bottom dot
                const float dotR = std::max(1.6f, s * 0.11f);
                canvas.DrawRoundRect(Rect(cx - dotR, cy + s * 0.27f - dotR, dotR * 2.0f, dotR * 2.0f), dotR, tint);

                const Vec2 tip(cx, cy - s * 0.35f);
                canvas.DrawLine(Vec2(cx, cy + s * 0.07f), tip, tint, stroke);
                canvas.DrawLine(Vec2(cx - s * 0.21f, cy - s * 0.15f), tip, tint, stroke);
                canvas.DrawLine(Vec2(cx + s * 0.21f, cy - s * 0.15f), tip, tint, stroke);
                break;
            }

            case IconId::Replace: {
                // Replace single: Top-left box + curved arrow pointing to bottom-right box
                canvas.DrawRoundRect(Rect(cx - s * 0.36f, cy - s * 0.34f, s * 0.34f, s * 0.24f), 1.5f, Color(0, 0, 0, 0), tint, stroke * 0.9f);
                canvas.DrawRoundRect(Rect(cx + s * 0.02f, cy + s * 0.10f, s * 0.34f, s * 0.24f), 1.5f, Color(0, 0, 0, 0), tint, stroke * 0.9f);
                canvas.DrawLine(Vec2(cx + s * 0.06f, cy - s * 0.22f), Vec2(cx + s * 0.22f, cy - s * 0.22f), tint, stroke * 0.9f);
                canvas.DrawLine(Vec2(cx + s * 0.22f, cy - s * 0.22f), Vec2(cx + s * 0.22f, cy + s * 0.02f), tint, stroke * 0.9f);
                canvas.DrawLine(Vec2(cx + s * 0.12f, cy - s * 0.08f), Vec2(cx + s * 0.22f, cy + s * 0.02f), tint, stroke * 0.9f);
                canvas.DrawLine(Vec2(cx + s * 0.32f, cy - s * 0.08f), Vec2(cx + s * 0.22f, cy + s * 0.02f), tint, stroke * 0.9f);
                break;
            }

            case IconId::ReplaceAll: {
                // Replace All: Stacked double boxes + arrow
                canvas.DrawRoundRect(Rect(cx - s * 0.36f, cy - s * 0.34f, s * 0.30f, s * 0.22f), 1.5f, Color(0, 0, 0, 0), tint, stroke * 0.85f);
                canvas.DrawRoundRect(Rect(cx - s * 0.02f, cy + s * 0.06f, s * 0.30f, s * 0.22f), 1.5f, Color(0, 0, 0, 0), tint, stroke * 0.85f);
                canvas.DrawRoundRect(Rect(cx + s * 0.08f, cy + s * 0.14f, s * 0.30f, s * 0.22f), 1.5f, Color(0, 0, 0, 0), tint, stroke * 0.85f);
                canvas.DrawLine(Vec2(cx + s * 0.02f, cy - s * 0.22f), Vec2(cx + s * 0.18f, cy - s * 0.22f), tint, stroke * 0.85f);
                canvas.DrawLine(Vec2(cx + s * 0.18f, cy - s * 0.22f), Vec2(cx + s * 0.18f, cy - s * 0.02f), tint, stroke * 0.85f);
                break;
            }

            default:
                break;
        }
    } else if (m_type == SourceType::Asset && !m_path.empty()) {
        canvas.DrawImage(m_path, bounds, 0.0f, tint.a);
    } else if (m_type == SourceType::Texture && m_textureId != 0) {
        canvas.DrawTexture(m_textureId, bounds, std::nullopt, tint);
    }
}

} // namespace guikit
