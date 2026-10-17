#include "guikit/widgets/SignalView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/graphics/Colormap.hpp"

namespace guikit {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kMinLogFreqHz = 20.0f;

struct PeakBucket {
    float minVal{0.0f};
    float maxVal{0.0f};
    float rmsVal{0.0f};
};

struct WaveformPyramid {
    uint64_t fingerprint{0};
    size_t sampleCount{0};
    static constexpr std::array<size_t, 4> kBlockSizes = {64, 512, 4096, 32768};
    std::array<std::vector<PeakBucket>, 4> levels{};
    std::vector<PeakBucket> overview256{};

    void Build(std::span<const float> samples, uint64_t fp) {
        fingerprint = fp;
        sampleCount = samples.size();
        for (auto& lvl : levels) {
            lvl.clear();
        }
        overview256.clear();
        if (samples.empty()) {
            return;
        }

        // Level 0 from raw samples (block size 64)
        const size_t b0 = kBlockSizes[0];
        const size_t n0 = (sampleCount + b0 - 1) / b0;
        levels[0].resize(n0);
        for (size_t i = 0; i < n0; ++i) {
            const size_t start = i * b0;
            const size_t end = std::min(sampleCount, start + b0);
            float mn = samples[start];
            float mx = samples[start];
            double sumSq = 0.0;
            for (size_t s = start; s < end; ++s) {
                const float v = samples[s];
                if (v < mn) mn = v;
                if (v > mx) mx = v;
                sumSq += static_cast<double>(v) * static_cast<double>(v);
            }
            const float count = static_cast<float>(end - start);
            levels[0][i] = PeakBucket{
                .minVal = mn,
                .maxVal = mx,
                .rmsVal = static_cast<float>(std::sqrt(sumSq / std::max(1.0f, count)))
            };
        }

        // Subsequent levels (each aggregates 8 buckets from previous level)
        for (size_t lvl = 1; lvl < levels.size(); ++lvl) {
            const auto& prev = levels[lvl - 1];
            const size_t n = (prev.size() + 7) / 8;
            levels[lvl].resize(n);
            for (size_t i = 0; i < n; ++i) {
                const size_t start = i * 8;
                const size_t end = std::min(prev.size(), start + 8);
                float mn = prev[start].minVal;
                float mx = prev[start].maxVal;
                double sumSq = 0.0;
                for (size_t k = start; k < end; ++k) {
                    if (prev[k].minVal < mn) mn = prev[k].minVal;
                    if (prev[k].maxVal > mx) mx = prev[k].maxVal;
                    sumSq += static_cast<double>(prev[k].rmsVal) * static_cast<double>(prev[k].rmsVal);
                }
                const float cnt = static_cast<float>(end - start);
                levels[lvl][i] = PeakBucket{
                    .minVal = mn,
                    .maxVal = mx,
                    .rmsVal = static_cast<float>(std::sqrt(sumSq / std::max(1.0f, cnt)))
                };
            }
        }

        // Precompute 256-bin overview envelope for the top minimap navigator
        constexpr size_t kOverviewBins = 256;
        overview256.resize(kOverviewBins);
        for (size_t b = 0; b < kOverviewBins; ++b) {
            const size_t s0 = (b * sampleCount) / kOverviewBins;
            const size_t s1 = std::max(s0 + 1, ((b + 1) * sampleCount) / kOverviewBins);
            overview256[b] = QueryRange(samples, s0, std::min(sampleCount, s1));
        }
    }

    [[nodiscard]] PeakBucket QueryRange(std::span<const float> samples, size_t s0, size_t s1) const noexcept {
        if (samples.empty() || s0 >= samples.size()) {
            return {};
        }
        s1 = std::min(samples.size(), std::max(s0 + 1, s1));
        const size_t span = s1 - s0;

        if (span < kBlockSizes[0] * 2) {
            float mn = samples[s0];
            float mx = samples[s0];
            double sumSq = 0.0;
            for (size_t i = s0; i < s1; ++i) {
                const float v = samples[i];
                if (v < mn) mn = v;
                if (v > mx) mx = v;
                sumSq += static_cast<double>(v) * static_cast<double>(v);
            }
            return PeakBucket{
                .minVal = mn,
                .maxVal = mx,
                .rmsVal = static_cast<float>(std::sqrt(sumSq / static_cast<double>(span)))
            };
        }

        size_t chosenLvl = 0;
        for (size_t lvl = levels.size(); lvl > 0; --lvl) {
            if (span >= kBlockSizes[lvl - 1] * 2 && !levels[lvl - 1].empty()) {
                chosenLvl = lvl - 1;
                break;
            }
        }

        const size_t blk = kBlockSizes[chosenLvl];
        const auto& arr = levels[chosenLvl];
        const size_t b0 = std::min(arr.size() - 1, s0 / blk);
        const size_t b1 = std::min(arr.size(), std::max(b0 + 1, (s1 + blk - 1) / blk));

        float mn = arr[b0].minVal;
        float mx = arr[b0].maxVal;
        double sumSq = 0.0;
        for (size_t b = b0; b < b1; ++b) {
            if (arr[b].minVal < mn) mn = arr[b].minVal;
            if (arr[b].maxVal > mx) mx = arr[b].maxVal;
            sumSq += static_cast<double>(arr[b].rmsVal) * static_cast<double>(arr[b].rmsVal);
        }
        const float cnt = static_cast<float>(b1 - b0);
        return PeakBucket{
            .minVal = mn,
            .maxVal = mx,
            .rmsVal = static_cast<float>(std::sqrt(sumSq / std::max(1.0f, cnt)))
        };
    }
};

[[nodiscard]] uint64_t ComputeSignalFingerprint(std::span<const float> samples, float sampleRate) noexcept {
    uint64_t h = 14695981039346656037ULL;
    auto mix = [&](uint64_t v) noexcept {
        h ^= v;
        h *= 1099511628211ULL;
    };
    mix(samples.size());
    uint32_t srBits = 0;
    std::memcpy(&srBits, &sampleRate, sizeof(float));
    mix(srBits);

    if (!samples.empty()) {
        const size_t probes = std::min<size_t>(64, samples.size());
        for (size_t i = 0; i < probes; ++i) {
            const size_t idx = (i * (samples.size() - 1)) / std::max<size_t>(1, probes - 1);
            uint32_t bits = 0;
            std::memcpy(&bits, &samples[idx], sizeof(float));
            mix(bits);
        }
    }
    return h;
}

// In-place iterative Cooley-Tukey Radix-2 FFT
void Radix2Fft(std::vector<float>& re, std::vector<float>& im) noexcept {
    const size_t n = re.size();
    if (n <= 1) return;

    // Bit-reversal permutation
    size_t j = 0;
    for (size_t i = 1; i < n; ++i) {
        size_t bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    // Butterfly stages
    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -kTwoPi / static_cast<float>(len);
        const float wLenRe = std::cos(angle);
        const float wLenIm = std::sin(angle);
        const size_t half = len >> 1;

        for (size_t i = 0; i < n; i += len) {
            float wRe = 1.0f;
            float wIm = 0.0f;
            for (size_t k = 0; k < half; ++k) {
                const size_t u = i + k;
                const size_t v = i + k + half;
                const float tRe = re[v] * wRe - im[v] * wIm;
                const float tIm = re[v] * wIm + im[v] * wRe;
                re[v] = re[u] - tRe;
                im[v] = im[u] - tIm;
                re[u] += tRe;
                im[u] += tIm;

                const float nextWRe = wRe * wLenRe - wIm * wLenIm;
                wIm = wRe * wLenIm + wIm * wLenRe;
                wRe = nextWRe;
            }
        }
    }
}

[[nodiscard]] uint32_t SanitizeFftSize(uint32_t requested) noexcept {
    if (requested <= 256) return 256;
    if (requested <= 512) return 512;
    if (requested <= 1024) return 1024;
    return 2048;
}

void BuildFftWindow(uint32_t n, FftWindowType type, std::vector<float>& outWin, float& outNorm) {
    outWin.resize(n);
    double sum = 0.0;
    const float denom = static_cast<float>(std::max(1u, n - 1));
    for (uint32_t i = 0; i < n; ++i) {
        const float phase = static_cast<float>(i) / denom;
        float w = 1.0f;
        switch (type) {
            case FftWindowType::Hann:
                w = 0.5f * (1.0f - std::cos(kTwoPi * phase));
                break;
            case FftWindowType::Hamming:
                w = 0.54f - 0.46f * std::cos(kTwoPi * phase);
                break;
            case FftWindowType::BlackmanHarris:
                w = 0.35875f
                  - 0.48829f * std::cos(kTwoPi * phase)
                  + 0.14128f * std::cos(2.0f * kTwoPi * phase)
                  - 0.01168f * std::cos(3.0f * kTwoPi * phase);
                break;
            case FftWindowType::Rectangular:
                w = 1.0f;
                break;
        }
        outWin[i] = w;
        sum += static_cast<double>(w);
    }
    // Coherent gain normalization for single-sided spectrum (peak 1.0 sine -> 0 dBFS)
    outNorm = (sum > 1e-6) ? static_cast<float>(2.0 / sum) : (2.0f / static_cast<float>(n));
}

[[nodiscard]] float NormalizedYToFreqHz(
    float normFromBottom,
    float minFreqHz,
    float maxFreqHz,
    FrequencyScale scale
) noexcept {
    normFromBottom = std::clamp(normFromBottom, 0.0f, 1.0f);
    if (scale == FrequencyScale::Logarithmic) {
        const float safeMin = std::max(kMinLogFreqHz, minFreqHz);
        const float safeMax = std::max(safeMin + 10.0f, maxFreqHz);
        return safeMin * std::pow(safeMax / safeMin, normFromBottom);
    }
    return minFreqHz + normFromBottom * (maxFreqHz - minFreqHz);
}

[[nodiscard]] float FreqHzToNormalizedY(
    float freqHz,
    float minFreqHz,
    float maxFreqHz,
    FrequencyScale scale
) noexcept {
    if (maxFreqHz <= minFreqHz + 1e-3f) return 0.0f;
    if (scale == FrequencyScale::Logarithmic) {
        const float safeMin = std::max(kMinLogFreqHz, minFreqHz);
        const float safeMax = std::max(safeMin + 10.0f, maxFreqHz);
        const float clamped = std::clamp(freqHz, safeMin, safeMax);
        return std::log(clamped / safeMin) / std::log(safeMax / safeMin);
    }
    return std::clamp((freqHz - minFreqHz) / (maxFreqHz - minFreqHz), 0.0f, 1.0f);
}

struct SpectrogramCache {
    static constexpr uint32_t kTexW = 280;
    static constexpr uint32_t kTexH = 140;

    uint64_t signalFp{0};
    double viewStartSec{-1.0};
    double viewEndSec{-1.0};
    float minFreqHz{-1.0f};
    float maxFreqHz{-1.0f};
    uint32_t fftSize{0};
    FftWindowType windowType{FftWindowType::Hann};
    FrequencyScale freqScale{FrequencyScale::Linear};
    Colormap colormap{Colormap::Inferno};
    float dbFloor{0.0f};
    float dbCeil{0.0f};
    uint64_t dbGridGeneration{1};

    std::vector<uint32_t> pixels{};
    std::vector<float> stftBinsDb{}; // kTexW * halfBins cached raw STFT magnitude in dB
    bool stftValid{false};
    std::vector<float> dbGrid{}; // kTexW * kTexH (row 0 = top / maxFreq, row kTexH-1 = bottom / minFreq)
    bool pixelsValid{false};

    // Reusable scratch buffers
    std::vector<float> winWeights{};
    float winNorm{1.0f};
    uint32_t cachedWinSize{0};
    FftWindowType cachedWinType{FftWindowType::Hann};
    std::vector<float> re{};
    std::vector<float> im{};

    void EnsureValid(
        std::span<const float> samples,
        uint64_t fp,
        float sampleRate,
        double startSec,
        double endSec,
        float fMin,
        float fMax,
        uint32_t reqFftSize,
        FftWindowType reqWin,
        FrequencyScale reqScale,
        Colormap reqCmap,
        float reqFloor,
        float reqCeil,
        bool skipCpuPixels = false
    ) {
        const uint32_t effFft = SanitizeFftSize(reqFftSize);
        const uint32_t halfBins = effFft / 2 + 1;
        const float safeFloor = std::min(reqFloor, reqCeil - 6.0f);
        const float safeCeil = reqCeil;

        const bool stftSame =
            stftValid &&
            signalFp == fp &&
            std::abs(viewStartSec - startSec) < 1e-7 &&
            std::abs(viewEndSec - endSec) < 1e-7 &&
            fftSize == effFft &&
            windowType == reqWin &&
            stftBinsDb.size() == static_cast<size_t>(kTexW * halfBins);

        const bool geometrySame =
            stftSame &&
            std::abs(minFreqHz - fMin) < 0.05f &&
            std::abs(maxFreqHz - fMax) < 0.05f &&
            freqScale == reqScale &&
            dbGrid.size() == static_cast<size_t>(kTexW * kTexH);

        const bool colorSame =
            geometrySame &&
            colormap == reqCmap &&
            std::abs(dbFloor - safeFloor) < 0.05f &&
            std::abs(dbCeil - safeCeil) < 0.05f &&
            (skipCpuPixels || (pixelsValid && pixels.size() == static_cast<size_t>(kTexW * kTexH)));

        if (colorSame) {
            return;
        }

        signalFp = fp;
        viewStartSec = startSec;
        viewEndSec = endSec;
        minFreqHz = fMin;
        maxFreqHz = fMax;
        fftSize = effFft;
        windowType = reqWin;
        freqScale = reqScale;
        colormap = reqCmap;
        dbFloor = safeFloor;
        dbCeil = safeCeil;

        dbGrid.resize(static_cast<size_t>(kTexW * kTexH), -120.0f);

        if (samples.empty() || sampleRate <= 0.0f) {
            dbGridGeneration++;
            stftValid = false;
            std::fill(dbGrid.begin(), dbGrid.end(), -120.0f);
            if (!skipCpuPixels) {
                pixels.resize(static_cast<size_t>(kTexW * kTexH));
                const uint32_t bgPx = SampleColormapAbgr(colormap, 0.0f);
                std::fill(pixels.begin(), pixels.end(), bgPx);
                pixelsValid = true;
            } else {
                pixelsValid = false;
            }
            return;
        }

        if (!stftSame) {
            if (cachedWinSize != effFft || cachedWinType != reqWin) {
                BuildFftWindow(effFft, reqWin, winWeights, winNorm);
                cachedWinSize = effFft;
                cachedWinType = reqWin;
            }

            re.resize(effFft);
            im.resize(effFft);
            stftBinsDb.resize(static_cast<size_t>(kTexW * halfBins));

            const double spanSec = std::max(1e-6, endSec - startSec);
            const int64_t totalSamples = static_cast<int64_t>(samples.size());
            const int64_t halfFft = static_cast<int64_t>(effFft / 2);
            const float winNormSq = winNorm * winNorm;

            for (uint32_t x = 0; x < kTexW; ++x) {
                const double colTimeSec = startSec + (static_cast<double>(x) + 0.5) / static_cast<double>(kTexW) * spanSec;
                const int64_t centerSample = static_cast<int64_t>(std::llround(colTimeSec * static_cast<double>(sampleRate)));
                const int64_t frameStart = centerSample - halfFft;

                for (uint32_t k = 0; k < effFft; ++k) {
                    const int64_t sIdx = frameStart + static_cast<int64_t>(k);
                    const float sVal = (sIdx >= 0 && sIdx < totalSamples) ? samples[static_cast<size_t>(sIdx)] : 0.0f;
                    re[k] = sVal * winWeights[k];
                    im[k] = 0.0f;
                }

                Radix2Fft(re, im);

                float* colOut = stftBinsDb.data() + static_cast<size_t>(x) * halfBins;
                for (uint32_t b = 0; b < halfBins; ++b) {
                    const float magSq = (re[b] * re[b] + im[b] * im[b]) * winNormSq;
                    colOut[b] = 10.0f * std::log10(std::max(1e-12f, magSq));
                }
            }
            stftValid = true;
        }

        if (!geometrySame) {
            dbGridGeneration++;
            const float nyquist = 0.5f * sampleRate;
            const float maxBinF = static_cast<float>(halfBins - 1);
            const float invNyquistBins = maxBinF / std::max(1.0f, nyquist);

            for (uint32_t y = 0; y < kTexH; ++y) {
                // y = 0 is top (maxFreqHz), y = kTexH - 1 is bottom (minFreqHz)
                const float normFromBottom = 1.0f - (static_cast<float>(y) + 0.5f) / static_cast<float>(kTexH);
                const float freq = NormalizedYToFreqHz(normFromBottom, fMin, fMax, reqScale);
                const float binFloat = std::clamp(freq * invNyquistBins, 0.0f, maxBinF);
                const uint32_t b0 = static_cast<uint32_t>(binFloat);
                const uint32_t b1 = std::min(halfBins - 1, b0 + 1);
                const float frac = binFloat - static_cast<float>(b0);

                float* rowOut = dbGrid.data() + static_cast<size_t>(y) * kTexW;
                const float* stftPtr = stftBinsDb.data();
                for (uint32_t x = 0; x < kTexW; ++x) {
                    const float c0 = stftPtr[b0];
                    const float c1 = stftPtr[b1];
                    rowOut[x] = c0 + (c1 - c0) * frac;
                    stftPtr += halfBins;
                }
            }
        }

        if (skipCpuPixels) {
            pixelsValid = false;
            return;
        }

        // Map dbGrid to ABGR8888 pixels using [dbFloor, dbCeil] and active colormap
        pixels.resize(static_cast<size_t>(kTexW * kTexH));
        const auto& lutAbgr = GetColormapLutAbgr(colormap);
        const float invRange = 255.0f / std::max(1.0f, safeCeil - safeFloor);
        for (size_t i = 0; i < dbGrid.size(); ++i) {
            const size_t lutIdx = static_cast<size_t>(std::clamp((dbGrid[i] - safeFloor) * invRange, 0.0f, 255.0f));
            pixels[i] = lutAbgr[lutIdx];
        }
        pixelsValid = true;
    }

    [[nodiscard]] float SampleDbAt(float normX, float normYFromTop) const noexcept {
        if (dbGrid.size() != static_cast<size_t>(kTexW * kTexH)) {
            return -120.0f;
        }
        const uint32_t x = std::min(kTexW - 1, static_cast<uint32_t>(std::clamp(normX, 0.0f, 0.9999f) * static_cast<float>(kTexW)));
        const uint32_t y = std::min(kTexH - 1, static_cast<uint32_t>(std::clamp(normYFromTop, 0.0f, 0.9999f) * static_cast<float>(kTexH)));
        return dbGrid[static_cast<size_t>(y) * kTexW + x];
    }
};

[[nodiscard]] Color BlendColorOverOpaque(Color bg, Color fg) noexcept {
    const uint32_t a = fg.a;
    const uint32_t invA = 255u - a;
    return Color(
        static_cast<uint8_t>((fg.r * a + bg.r * invA + 127u) / 255u),
        static_cast<uint8_t>((fg.g * a + bg.g * invA + 127u) / 255u),
        static_cast<uint8_t>((fg.b * a + bg.b * invA + 127u) / 255u),
        255
    );
}

[[nodiscard]] float AmplitudeToNormY(float sampleVal, float gain, AmplitudeScale scale) noexcept {
    const float v = sampleVal * gain;
    if (scale == AmplitudeScale::Linear) {
        return std::clamp(0.5f - 0.5f * v, 0.0f, 1.0f);
    }
    // In dBFS mode, waveform trace remains bipolar around center 0.5, with soft logarithmic compression for low-level visibility
    return std::clamp(0.5f - 0.5f * v, 0.0f, 1.0f);
}

struct OverviewBitmapCache {
    uint64_t signalFp{0};
    uint32_t width{0};
    uint32_t height{0};
    bool isDark{true};
    uint32_t accentAbgr{0};
    bool valid{false};
    std::vector<uint32_t> pixels{};

    void EnsureValid(
        const WaveformPyramid& pyramid,
        uint64_t fp,
        uint32_t w,
        uint32_t h,
        bool dark,
        Color accent
    ) {
        if (w == 0 || h == 0) return;
        const uint32_t acc = PackAbgr(accent);
        if (valid && signalFp == fp && width == w && height == h && isDark == dark && accentAbgr == acc &&
            pixels.size() == static_cast<size_t>(w) * h) {
            return;
        }
        signalFp = fp;
        width = w;
        height = h;
        isDark = dark;
        accentAbgr = acc;
        valid = true;

        const Color bgCol = dark ? Color(8, 12, 20, 255) : Color(230, 236, 244, 255);
        const uint32_t bgPx = PackAbgr(bgCol);
        const uint32_t barPx = PackAbgr(BlendColorOverOpaque(bgCol, accent.WithAlpha(165)));
        pixels.assign(static_cast<size_t>(w) * h, bgPx);

        if (pyramid.overview256.empty()) return;
        const size_t bins = pyramid.overview256.size();
        const float midY = static_cast<float>(h) * 0.5f;
        const float halfH = static_cast<float>(h) * 0.44f;

        for (uint32_t x = 0; x < w; ++x) {
            const size_t binIdx = std::min(bins - 1, (static_cast<size_t>(x) * bins) / w);
            const auto& pk = pyramid.overview256[binIdx];
            const float yMin = midY - std::clamp(pk.maxVal, -1.0f, 1.0f) * halfH;
            const float yMax = midY - std::clamp(pk.minVal, -1.0f, 1.0f) * halfH;
            const int iy0 = std::clamp(static_cast<int>(std::floor(yMin)), 0, static_cast<int>(h) - 1);
            const int iy1 = std::clamp(static_cast<int>(std::ceil(std::max(yMin + 1.0f, yMax))), iy0, static_cast<int>(h) - 1);
            for (int y = iy0; y <= iy1; ++y) {
                pixels[static_cast<size_t>(y) * w + x] = barPx;
            }
        }
    }
};

struct WaveformBitmapCache {
    uint64_t signalFp{0};
    double viewStartSec{-1.0};
    double viewEndSec{-1.0};
    float waveformGain{-1.0f};
    AmplitudeScale ampScale{AmplitudeScale::dBFS};
    bool showRms{true};
    bool showGrid{true};
    bool isDark{true};
    uint32_t width{0};
    uint32_t height{0};
    bool valid{false};
    std::vector<uint32_t> pixels{};

    void EnsureValid(
        std::span<const float> samples,
        const WaveformPyramid& pyramid,
        uint64_t fp,
        float sampleRate,
        double startSec,
        double endSec,
        float gain,
        AmplitudeScale reqScale,
        bool reqShowRms,
        bool reqShowGrid,
        bool dark,
        double firstMajorTime,
        double majorTimeStep,
        float uiScale,
        Color gridCol,
        Color subGridCol,
        Color zeroLineCol,
        uint32_t w,
        uint32_t h
    ) {
        if (w == 0 || h == 0) return;
        if (valid &&
            signalFp == fp &&
            width == w &&
            height == h &&
            isDark == dark &&
            showRms == reqShowRms &&
            showGrid == reqShowGrid &&
            ampScale == reqScale &&
            std::abs(viewStartSec - startSec) < 1e-7 &&
            std::abs(viewEndSec - endSec) < 1e-7 &&
            std::abs(waveformGain - gain) < 1e-4f &&
            pixels.size() == static_cast<size_t>(w) * h) {
            return;
        }

        signalFp = fp;
        viewStartSec = startSec;
        viewEndSec = endSec;
        waveformGain = gain;
        ampScale = reqScale;
        showRms = reqShowRms;
        showGrid = reqShowGrid;
        isDark = dark;
        width = w;
        height = h;
        valid = true;

        const Color bgCol = dark ? Color(10, 14, 23, 255) : Color(252, 253, 255, 255);
        const uint32_t bgPx = PackAbgr(bgCol);
        const uint32_t gridPx = PackAbgr(BlendColorOverOpaque(bgCol, gridCol));
        const uint32_t subGridPx = PackAbgr(BlendColorOverOpaque(bgCol, subGridCol));
        const uint32_t zeroLinePx = PackAbgr(BlendColorOverOpaque(bgCol, zeroLineCol));

        pixels.assign(static_cast<size_t>(w) * h, bgPx);

        const double viewSpan = std::max(1e-6, endSec - startSec);

        // 1. Vertical time grid lines
        if (reqShowGrid && majorTimeStep > 1e-9) {
            for (double t = firstMajorTime; t <= endSec + 1e-9; t += majorTimeStep) {
                const int tx = static_cast<int>(std::lround(((t - startSec) / viewSpan) * static_cast<double>(w)));
                if (tx >= 0 && tx < static_cast<int>(w)) {
                    uint32_t* colPtr = pixels.data() + tx;
                    for (uint32_t y = 0; y < h; ++y) {
                        *colPtr = gridPx;
                        colPtr += w;
                    }
                }
            }
        }

        // 2. Horizontal amplitude grid lines
        static constexpr std::array<float, 5> kNormLevels = {0.9f, 0.5f, 0.0f, -0.5f, -0.9f};
        for (float lv : kNormLevels) {
            if (!reqShowGrid && lv != 0.0f) continue;
            const int gy = std::clamp(static_cast<int>(std::lround((0.5f - 0.5f * lv) * static_cast<float>(h))), 0, static_cast<int>(h) - 1);
            std::fill_n(pixels.data() + static_cast<size_t>(gy) * w, w, (lv == 0.0f) ? zeroLinePx : subGridPx);
        }

        if (samples.empty()) return;

        // 3. Pixel-column Mipmap Pyramid envelope (Peak + RMS + Clipping ticks)
        const Color peakRaw = dark ? Color(56, 189, 248, 215) : Color(2, 132, 199, 220);
        const Color rmsRaw  = dark ? Color(14, 165, 233, 255) : Color(3, 105, 161, 255);
        const uint32_t peakPx = PackAbgr(BlendColorOverOpaque(bgCol, peakRaw));
        const uint32_t rmsPx  = PackAbgr(rmsRaw);
        const uint32_t clipPx = PackAbgr(Color(239, 68, 68, 255));

        const float hf = static_cast<float>(h);
        float prevMidY = hf * 0.5f;
        const int clipTickH = std::max(1, static_cast<int>(std::lround(3.0f * uiScale)));

        for (uint32_t col = 0; col < w; ++col) {
            const double t0 = startSec + (static_cast<double>(col) / static_cast<double>(w)) * viewSpan;
            const double t1 = startSec + (static_cast<double>(col + 1) / static_cast<double>(w)) * viewSpan;
            const size_t s0 = static_cast<size_t>(std::clamp(t0 * sampleRate, 0.0, static_cast<double>(samples.size() - 1)));
            const size_t s1 = static_cast<size_t>(std::clamp(std::ceil(t1 * sampleRate), static_cast<double>(s0 + 1), static_cast<double>(samples.size())));

            const PeakBucket pk = pyramid.QueryRange(samples, s0, s1);
            float yTop = AmplitudeToNormY(pk.maxVal, gain, reqScale) * hf;
            float yBot = AmplitudeToNormY(pk.minVal, gain, reqScale) * hf;
            if (yBot < yTop + 1.0f) {
                const float avgY = 0.5f * (yTop + yBot);
                yTop = std::min(avgY, prevMidY);
                yBot = std::max(avgY + 1.0f, prevMidY);
                prevMidY = avgY;
            } else {
                prevMidY = 0.5f * (yTop + yBot);
            }

            const int iy0 = std::clamp(static_cast<int>(std::floor(yTop)), 0, static_cast<int>(h) - 1);
            const int iy1 = std::clamp(static_cast<int>(std::ceil(yBot)), iy0, static_cast<int>(h) - 1);
            for (int y = iy0; y <= iy1; ++y) {
                pixels[static_cast<size_t>(y) * w + col] = peakPx;
            }

            if (reqShowRms && pk.rmsVal > 1e-4f) {
                const float rmsTop = AmplitudeToNormY(pk.rmsVal, gain, reqScale) * hf;
                const float rmsBot = AmplitudeToNormY(-pk.rmsVal, gain, reqScale) * hf;
                if (rmsBot > rmsTop + 1.0f) {
                    const int ir0 = std::clamp(static_cast<int>(std::floor(rmsTop)), 0, static_cast<int>(h) - 1);
                    const int ir1 = std::clamp(static_cast<int>(std::ceil(rmsBot)), ir0, static_cast<int>(h) - 1);
                    for (int y = ir0; y <= ir1; ++y) {
                        pixels[static_cast<size_t>(y) * w + col] = rmsPx;
                    }
                }
            }

            if (pk.maxVal >= 0.999f) {
                for (int y = 0; y < std::min(clipTickH, static_cast<int>(h)); ++y) {
                    pixels[static_cast<size_t>(y) * w + col] = clipPx;
                }
            } else if (pk.minVal <= -0.999f) {
                for (int y = std::max(0, static_cast<int>(h) - clipTickH); y < static_cast<int>(h); ++y) {
                    pixels[static_cast<size_t>(y) * w + col] = clipPx;
                }
            }
        }
    }
};

struct StaticBaseLayerCache {
    uint64_t cacheKey{0};
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint32_t> pixels{};
    bool valid{false};
};

enum class DragMode : uint8_t {
    None = 0,
    Splitter,
    OverviewPan,
    OverviewResizeLeft,
    OverviewResizeRight,
    TimeRulerScrub,
    MarkerMove,
    AmplitudeGain,
    FrequencyPan,
    ColorbarFloor,
    PlotPan,
    WaveSelectNew,
    SpecSelectNew,
    SelectResizeLeft,
    SelectResizeRight,
    SelectResizeLowFreq,
    SelectResizeHighFreq,
    SelectMoveBox
};

struct SignalViewInternalState {
    bool initialized{false};
    uint64_t lastSignalFp{0};

    double viewStartSec{0.0};
    double viewEndSec{1.0};
    float splitRatio{0.46f};
    float waveformGain{1.0f};
    float viewMinFreqHz{0.0f};
    float viewMaxFreqHz{24000.0f};
    float dbFloorOverride{0.0f};
    bool hasDbFloorOverride{false};

    double playheadSec{0.0};
    SignalSelection selection{};
    std::vector<SignalMarker> markers{};

    DragMode dragMode{DragMode::None};
    Vec2 dragStartMouse{0.0f, 0.0f};
    double dragStartView0{0.0};
    double dragStartView1{1.0};
    float dragStartSplit{0.46f};
    float dragStartGain{1.0f};
    float dragStartFreq0{0.0f};
    float dragStartFreq1{24000.0f};
    float dragStartDbFloor{-90.0f};
    double dragAnchorTimeSec{0.0};
    float dragAnchorFreqHz{0.0f};
    SignalSelection dragStartSelection{};
    int activeMarkerIdx{-1};

    double lastClickTimeSec{-10.0};
    Vec2 lastClickPos{0.0f, 0.0f};

    WaveformPyramid pyramid{};
    SpectrogramCache specCache{};
    OverviewBitmapCache overviewCache{};
    WaveformBitmapCache waveCache{};
    StaticBaseLayerCache baseLayerCache{};
};

struct SignalViewGeometry {
    Rect controlRect{};
    Rect overviewRect{};
    Rect timeRulerRect{};
    Rect topPanelRect{};
    Rect splitterRect{};
    Rect bottomPanelRect{};

    Rect wavePlotRect{};
    Rect waveRulerRect{};
    Rect specPlotRect{};
    Rect specRulerRect{};
    Rect colorbarRect{};
};

[[nodiscard]] SignalViewGeometry ComputeSignalGeometry(
    Rect ctrlBox,
    const SignalViewOptions& opts,
    float splitRatio,
    const UiTheme& theme
) noexcept {
    SignalViewGeometry g{};
    g.controlRect = ctrlBox;
    const float s = theme.Scale(1.0f);

    const float overviewH = opts.showOverviewBar ? std::round(26.0f * s) : 0.0f;
    const float timeRulerH = opts.showTimeRuler ? std::round(22.0f * s) : 0.0f;
    const float leftGutterW = (opts.showAmplitudeRuler || opts.showFrequencyRuler) ? std::round(52.0f * s) : 0.0f;
    const float rightGutterW = opts.showColorbar ? std::round(42.0f * s) : 0.0f;
    const float splitterH = std::round(6.0f * s);

    const float plotX = ctrlBox.x + leftGutterW;
    const float plotW = std::max(20.0f, ctrlBox.w - leftGutterW - rightGutterW);

    float curY = ctrlBox.y;
    if (overviewH > 0.0f) {
        g.overviewRect = Rect(plotX, curY, plotW, overviewH);
        curY += overviewH;
    }
    if (timeRulerH > 0.0f) {
        g.timeRulerRect = Rect(plotX, curY, plotW, timeRulerH);
        curY += timeRulerH;
    }

    const float bodyH = std::max(20.0f, ctrlBox.Bottom() - curY);
    const float clampedRatio = std::clamp(splitRatio, 0.0f, 1.0f);
    const float usableH = std::max(0.0f, bodyH - splitterH);
    const float topH = std::round(usableH * clampedRatio);
    const float botH = std::max(0.0f, usableH - topH);

    g.topPanelRect = Rect(ctrlBox.x, curY, ctrlBox.w, topH);
    g.splitterRect = Rect(ctrlBox.x, curY + topH, ctrlBox.w, splitterH);
    g.bottomPanelRect = Rect(ctrlBox.x, curY + topH + splitterH, ctrlBox.w, botH);

    const Rect topPlot(plotX, g.topPanelRect.y, plotW, g.topPanelRect.h);
    const Rect topRuler(ctrlBox.x, g.topPanelRect.y, leftGutterW, g.topPanelRect.h);
    const Rect botPlot(plotX, g.bottomPanelRect.y, plotW, g.bottomPanelRect.h);
    const Rect botRuler(ctrlBox.x, g.bottomPanelRect.y, leftGutterW, g.bottomPanelRect.h);

    if (opts.panelOrder == SignalPanelOrder::WaveformTop) {
        g.wavePlotRect = topPlot;
        g.waveRulerRect = topRuler;
        g.specPlotRect = botPlot;
        g.specRulerRect = botRuler;
    } else {
        g.specPlotRect = topPlot;
        g.specRulerRect = topRuler;
        g.wavePlotRect = botPlot;
        g.waveRulerRect = botRuler;
    }

    if (rightGutterW > 0.0f) {
        g.colorbarRect = Rect(plotX + plotW, g.specPlotRect.y, rightGutterW, g.specPlotRect.h);
    }

    return g;
}

[[nodiscard]] std::string FormatTimeTick(double timeSec, double spanSec) {
    char buf[64]{};
    if (timeSec < 0.0) timeSec = 0.0;
    if (spanSec >= 60.0) {
        const int mins = static_cast<int>(timeSec / 60.0);
        const double secs = timeSec - static_cast<double>(mins) * 60.0;
        std::snprintf(buf, sizeof(buf), "%d:%04.1fs", mins, secs);
    } else if (spanSec >= 2.0) {
        std::snprintf(buf, sizeof(buf), "%.2fs", timeSec);
    } else if (spanSec >= 0.05) {
        std::snprintf(buf, sizeof(buf), "%.1f ms", timeSec * 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.3f ms", timeSec * 1000.0);
    }
    return std::string(buf);
}

[[nodiscard]] std::string FormatFreqTick(float freqHz) {
    char buf[32]{};
    if (freqHz >= 1000.0f) {
        if (std::fmod(freqHz, 1000.0f) < 1.0f) {
            std::snprintf(buf, sizeof(buf), "%.0fk", freqHz * 0.001f);
        } else {
            std::snprintf(buf, sizeof(buf), "%.1fk", freqHz * 0.001f);
        }
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f", freqHz);
    }
    return std::string(buf);
}

[[nodiscard]] std::string FreqToPitchNote(float freqHz) {
    if (freqHz < 16.35f || freqHz > 16000.0f) return {};
    static constexpr std::array<const char*, 12> kNoteNames = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const float midi = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
    const int midiInt = static_cast<int>(std::lround(midi));
    if (midiInt < 12 || midiInt > 127) return {};
    const int noteIdx = ((midiInt % 12) + 12) % 12;
    const int octave = (midiInt / 12) - 1;
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%s%d", kNoteNames[static_cast<size_t>(noteIdx)], octave);
    return std::string(buf);
}

[[nodiscard]] double NiceTimeStep(double spanSec, float maxTicks) noexcept {
    if (spanSec <= 0.0 || maxTicks <= 1.0f) return 1.0;
    const double raw = spanSec / static_cast<double>(maxTicks);
    const double exp10 = std::pow(10.0, std::floor(std::log10(raw)));
    const double frac = raw / exp10;
    double mult = 1.0;
    if (frac > 5.0) mult = 10.0;
    else if (frac > 2.5) mult = 5.0;
    else if (frac > 2.0) mult = 2.5;
    else if (frac > 1.0) mult = 2.0;
    return mult * exp10;
}

[[nodiscard]] ElementSpec MakeSignalViewElementSpec(
    const SignalViewOptions& opts,
    UiSize size,
    const UiTheme& theme,
    bool disabled
) noexcept {
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .variant = opts.variant,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch),
        .disabled = disabled
    };
}

struct SignalViewPayload {
    std::span<const float> samples{};
    SignalViewOptions opts{};
    SignalViewInternalState* state{nullptr};
    Vec2 mousePos{0.0f, 0.0f};
    bool isHovered{false};
};

} // namespace

Vec2 SignalView::Measure(
    std::span<const float> /*samples*/,
    const SignalViewOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const float defaultW = opts.width.has_value()
        ? theme.Scale(*opts.width)
        : (availableWidth > 0.0f ? availableWidth : theme.Scale(560.0f));
    const float defaultH = opts.height.has_value()
        ? theme.Scale(*opts.height)
        : theme.Scale(340.0f);
    const ElementSpec spec = MakeSignalViewElementSpec(opts, size, theme, opts.disabled);
    return Element::Measure({defaultW, defaultH}, spec, canvas, theme, availableWidth);
}

SignalViewResponse SignalView::Run(
    Context& ctx,
    std::span<const float> samples,
    const SignalViewOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    auto& st = ctx.GetOrCreateState<SignalViewInternalState>(id);

    const float sampleRate = std::max(1.0f, opts.sampleRate);
    const float nyquistHz = 0.5f * sampleRate;
    const double totalDurationSec = !samples.empty()
        ? (static_cast<double>(samples.size()) / static_cast<double>(sampleRate))
        : 1.0;

    const uint64_t fp = ComputeSignalFingerprint(samples, sampleRate);
    if (!st.initialized || st.lastSignalFp != fp) {
        st.pyramid.Build(samples, fp);
        if (!st.initialized || st.lastSignalFp != fp) {
            st.viewStartSec = 0.0;
            st.viewEndSec = totalDurationSec;
            st.viewMinFreqHz = (opts.frequencyScale == FrequencyScale::Logarithmic) ? kMinLogFreqHz : 0.0f;
            st.viewMaxFreqHz = nyquistHz;
            st.playheadSec = 0.0;
            if (!st.initialized) {
                st.splitRatio = std::clamp(opts.splitRatio, 0.0f, 1.0f);
                st.waveformGain = std::clamp(opts.waveformGain, 0.1f, 64.0f);
                st.markers = {
                    SignalMarker{totalDurationSec * 0.18, "Cue 1", Color(56, 189, 248, 255)},
                    SignalMarker{totalDurationSec * 0.52, "Burst", Color(245, 158, 11, 255)},
                    SignalMarker{totalDurationSec * 0.80, "Tail",  Color(16, 185, 129, 255)}
                };
            }
        }
        st.initialized = true;
        st.lastSignalFp = fp;
    }

    // Synchronize external bindings (before input processing)
    if (opts.splitRatioBinding && st.dragMode != DragMode::Splitter) {
        st.splitRatio = std::clamp(*opts.splitRatioBinding, 0.0f, 1.0f);
    } else if (!opts.splitRatioBinding && st.dragMode != DragMode::Splitter && std::abs(opts.splitRatio - 0.46f) > 1e-4f) {
        // Respect option changes when no pointer binding is used
    }
    if (opts.waveformGainBinding && st.dragMode != DragMode::AmplitudeGain) {
        st.waveformGain = std::clamp(*opts.waveformGainBinding, 0.1f, 64.0f);
    }
    if (opts.playheadTimeSec && st.dragMode != DragMode::TimeRulerScrub) {
        st.playheadSec = std::clamp(*opts.playheadTimeSec, 0.0, totalDurationSec);
    }
    if (opts.selection && st.dragMode == DragMode::None) {
        st.selection = *opts.selection;
    }
    if (opts.markers && st.dragMode != DragMode::MarkerMove) {
        st.markers = *opts.markers;
    }

    // Apply programmatic camera requests
    if (opts.requestResetView) {
        st.viewStartSec = 0.0;
        st.viewEndSec = totalDurationSec;
        st.viewMinFreqHz = (opts.frequencyScale == FrequencyScale::Logarithmic) ? kMinLogFreqHz : 0.0f;
        st.viewMaxFreqHz = nyquistHz;
        st.waveformGain = 1.0f;
        st.hasDbFloorOverride = false;
    }
    if (opts.requestViewStartSec.has_value()) {
        st.viewStartSec = std::clamp(*opts.requestViewStartSec, 0.0, totalDurationSec);
    }
    if (opts.requestViewEndSec.has_value()) {
        st.viewEndSec = std::clamp(*opts.requestViewEndSec, st.viewStartSec + 1e-5, totalDurationSec);
    }
    if (opts.requestFreqRangeHz.has_value()) {
        st.viewMinFreqHz = std::clamp(opts.requestFreqRangeHz->x, 0.0f, nyquistHz - 10.0f);
        st.viewMaxFreqHz = std::clamp(opts.requestFreqRangeHz->y, st.viewMinFreqHz + 10.0f, nyquistHz);
    }

    // Clamp frequency window to valid Nyquist bounds
    const float absMinFreq = (opts.frequencyScale == FrequencyScale::Logarithmic) ? kMinLogFreqHz : 0.0f;
    st.viewMinFreqHz = std::clamp(st.viewMinFreqHz, absMinFreq, std::max(absMinFreq, nyquistHz - 20.0f));
    st.viewMaxFreqHz = std::clamp(st.viewMaxFreqHz, st.viewMinFreqHz + 20.0f, nyquistHz);

    const double minTimeSpan = std::min(totalDurationSec, std::max(16.0 / static_cast<double>(sampleRate), 1e-4));
    if (st.viewEndSec - st.viewStartSec < minTimeSpan) {
        st.viewEndSec = std::min(totalDurationSec, st.viewStartSec + minTimeSpan);
        st.viewStartSec = std::max(0.0, st.viewEndSec - minTimeSpan);
    }

    Response baseRes = ctx.Interact(id);
    SignalViewResponse res{};
    static_cast<Response&>(res) = baseRes;

    const Rect prevBounds = baseRes.rect;
    const ElementSpec elemSpec = MakeSignalViewElementSpec(opts, activeSize, theme, effectiveDisabled);
    const Vec2 prevCtrlSize = {prevBounds.w, prevBounds.h};
    const ElementGeometry prevElemGeom = Element::Resolve(prevBounds, prevCtrlSize, elemSpec, canvas, theme);
    const SignalViewGeometry geom = ComputeSignalGeometry(prevElemGeom.controlRect, opts, st.splitRatio, theme);

    const Vec2 mouse = ctx.GetMousePos();
    const bool mouseDown = ctx.IsMouseDown();
    const bool mouseClicked = ctx.IsMouseClicked();
    const bool rightDown = false;
    const bool middleDown = false;
    const KeyModifiers mods = ctx.GetKeyModifiers();
    const Vec2 wheel = ctx.GetMouseWheel();

    auto& markerList = opts.markers ? *opts.markers : st.markers;

    // Hit-test cue markers on time ruler
    int hoveredMarker = -1;
    if (geom.timeRulerRect.w > 0.0f && geom.timeRulerRect.Contains(mouse)) {
        const double viewSpan = std::max(1e-6, st.viewEndSec - st.viewStartSec);
        for (size_t i = 0; i < markerList.size(); ++i) {
            const float mx = geom.timeRulerRect.x +
                static_cast<float>((markerList[i].timeSec - st.viewStartSec) / viewSpan) * geom.timeRulerRect.w;
            if (std::abs(mouse.x - mx) <= theme.Scale(7.0f)) {
                hoveredMarker = static_cast<int>(i);
                break;
            }
        }
    }
    res.hoveredMarkerIndex = hoveredMarker;

    // Interactive Input Processing
    if (!effectiveDisabled && prevBounds.w > 0.0f && prevBounds.h > 0.0f) {
        const double viewSpan = std::max(1e-6, st.viewEndSec - st.viewStartSec);

        // 1. Mouse Wheel Zoom (Time, Amplitude Gain, Frequency Band, or Colorbar Dynamic Range)
        if (baseRes.hovered && (std::abs(wheel.y) > 0.001f || std::abs(wheel.x) > 0.001f)) {
            const float zoomStep = (wheel.y > 0.0f) ? 0.80f : 1.25f;

            if (geom.waveRulerRect.Contains(mouse) || (geom.wavePlotRect.Contains(mouse) && mods.ctrl)) {
                // Vertical waveform gain zoom
                st.waveformGain = std::clamp(st.waveformGain * (wheel.y > 0.0f ? 1.25f : 0.80f), 0.1f, 64.0f);
                if (opts.waveformGainBinding) *opts.waveformGainBinding = st.waveformGain;
                res.changed = true;
            } else if (geom.specRulerRect.Contains(mouse) || (geom.specPlotRect.Contains(mouse) && mods.ctrl)) {
                // Vertical frequency band zoom anchored at cursor frequency
                const float normY = 1.0f - std::clamp((mouse.y - geom.specPlotRect.y) / std::max(1.0f, geom.specPlotRect.h), 0.0f, 1.0f);
                const float anchorFreq = NormalizedYToFreqHz(normY, st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale);
                const float curSpanF = st.viewMaxFreqHz - st.viewMinFreqHz;
                const float newSpanF = std::clamp(curSpanF * zoomStep, 100.0f, nyquistHz - absMinFreq);
                const float ratio = (curSpanF > 1e-3f) ? ((anchorFreq - st.viewMinFreqHz) / curSpanF) : 0.5f;
                float newF0 = anchorFreq - ratio * newSpanF;
                float newF1 = newF0 + newSpanF;
                if (newF0 < absMinFreq) { newF0 = absMinFreq; newF1 = newF0 + newSpanF; }
                if (newF1 > nyquistHz)  { newF1 = nyquistHz;  newF0 = std::max(absMinFreq, newF1 - newSpanF); }
                st.viewMinFreqHz = newF0;
                st.viewMaxFreqHz = newF1;
                res.changed = true;
            } else if (geom.colorbarRect.Contains(mouse)) {
                const float curFloor = st.hasDbFloorOverride ? st.dbFloorOverride : opts.dbFloor;
                st.dbFloorOverride = std::clamp(curFloor + (wheel.y > 0.0f ? 4.0f : -4.0f), -130.0f, opts.dbCeil - 12.0f);
                st.hasDbFloorOverride = true;
                res.changed = true;
            } else if (geom.wavePlotRect.Contains(mouse) || geom.specPlotRect.Contains(mouse) ||
                       geom.timeRulerRect.Contains(mouse) || geom.overviewRect.Contains(mouse)) {
                // Horizontal Time Zoom anchored at mouse X
                const float plotX = geom.wavePlotRect.w > 0.0f ? geom.wavePlotRect.x : geom.specPlotRect.x;
                const float plotW = std::max(1.0f, geom.wavePlotRect.w > 0.0f ? geom.wavePlotRect.w : geom.specPlotRect.w);
                const double relX = std::clamp(static_cast<double>((mouse.x - plotX) / plotW), 0.0, 1.0);
                const double anchorTime = st.viewStartSec + relX * viewSpan;
                const double newSpan = std::clamp(viewSpan * static_cast<double>(zoomStep), minTimeSpan, totalDurationSec);
                double newT0 = anchorTime - relX * newSpan;
                double newT1 = newT0 + newSpan;
                if (newT0 < 0.0) { newT0 = 0.0; newT1 = newSpan; }
                if (newT1 > totalDurationSec) { newT1 = totalDurationSec; newT0 = std::max(0.0, newT1 - newSpan); }
                st.viewStartSec = newT0;
                st.viewEndSec = newT1;
                res.changed = true;
            }
        }

        // 2. Initiate Drag or Double-Click Actions on Mouse Press
        const Rect splitterHitRect = geom.splitterRect.Inset(-3.0f);
        if (mouseClicked && prevBounds.Contains(mouse)) {
            const double nowSec = ctx.GetCurrentTime();
            const bool isDoubleClick = (nowSec - st.lastClickTimeSec < 0.32) &&
                                       (std::hypot(mouse.x - st.lastClickPos.x, mouse.y - st.lastClickPos.y) < 8.0f);
            st.lastClickTimeSec = nowSec;
            st.lastClickPos = mouse;

            st.dragStartMouse = mouse;
            st.dragStartView0 = st.viewStartSec;
            st.dragStartView1 = st.viewEndSec;
            st.dragStartSplit = st.splitRatio;
            st.dragStartGain = st.waveformGain;
            st.dragStartFreq0 = st.viewMinFreqHz;
            st.dragStartFreq1 = st.viewMaxFreqHz;
            st.dragStartDbFloor = st.hasDbFloorOverride ? st.dbFloorOverride : opts.dbFloor;
            st.dragStartSelection = st.selection;

            if (splitterHitRect.Contains(mouse)) {
                if (isDoubleClick) {
                    st.splitRatio = 0.5f;
                    if (opts.splitRatioBinding) *opts.splitRatioBinding = st.splitRatio;
                    res.changed = true;
                } else {
                    st.dragMode = DragMode::Splitter;
                }
            } else if (geom.overviewRect.Contains(mouse)) {
                const float x0 = geom.overviewRect.x + static_cast<float>(st.viewStartSec / totalDurationSec) * geom.overviewRect.w;
                const float x1 = geom.overviewRect.x + static_cast<float>(st.viewEndSec / totalDurationSec) * geom.overviewRect.w;
                const float handleTol = theme.Scale(6.0f);
                if (std::abs(mouse.x - x0) <= handleTol) {
                    st.dragMode = DragMode::OverviewResizeLeft;
                } else if (std::abs(mouse.x - x1) <= handleTol) {
                    st.dragMode = DragMode::OverviewResizeRight;
                } else if (mouse.x >= x0 && mouse.x <= x1) {
                    st.dragMode = DragMode::OverviewPan;
                } else {
                    // Jump lens center to clicked overview position and pan
                    const double clickT = std::clamp(static_cast<double>((mouse.x - geom.overviewRect.x) / std::max(1.0f, geom.overviewRect.w)), 0.0, 1.0) * totalDurationSec;
                    double newT0 = clickT - 0.5 * viewSpan;
                    double newT1 = newT0 + viewSpan;
                    if (newT0 < 0.0) { newT0 = 0.0; newT1 = viewSpan; }
                    if (newT1 > totalDurationSec) { newT1 = totalDurationSec; newT0 = std::max(0.0, newT1 - viewSpan); }
                    st.viewStartSec = newT0;
                    st.viewEndSec = newT1;
                    st.dragStartView0 = newT0;
                    st.dragStartView1 = newT1;
                    st.dragMode = DragMode::OverviewPan;
                    res.changed = true;
                }
            } else if (geom.timeRulerRect.Contains(mouse)) {
                const double clickTime = st.viewStartSec +
                    std::clamp(static_cast<double>((mouse.x - geom.timeRulerRect.x) / std::max(1.0f, geom.timeRulerRect.w)), 0.0, 1.0) * viewSpan;

                if (isDoubleClick && opts.allowMarkerEditing) {
                    if (hoveredMarker >= 0 && static_cast<size_t>(hoveredMarker) < markerList.size()) {
                        markerList.erase(markerList.begin() + hoveredMarker);
                    } else {
                        char lbl[32]{};
                        std::snprintf(lbl, sizeof(lbl), "M%zu", markerList.size() + 1);
                        markerList.push_back(SignalMarker{clickTime, lbl, Color(245, 158, 11, 255)});
                    }
                    res.changed = true;
                } else if (hoveredMarker >= 0 && opts.allowMarkerEditing) {
                    st.activeMarkerIdx = hoveredMarker;
                    st.dragMode = DragMode::MarkerMove;
                } else {
                    st.playheadSec = clickTime;
                    if (opts.playheadTimeSec) *opts.playheadTimeSec = st.playheadSec;
                    st.dragMode = DragMode::TimeRulerScrub;
                    res.changed = true;
                }
            } else if (geom.waveRulerRect.Contains(mouse)) {
                if (isDoubleClick) {
                    st.waveformGain = 1.0f;
                    if (opts.waveformGainBinding) *opts.waveformGainBinding = st.waveformGain;
                    res.changed = true;
                } else {
                    st.dragMode = DragMode::AmplitudeGain;
                }
            } else if (geom.specRulerRect.Contains(mouse)) {
                if (isDoubleClick) {
                    st.viewMinFreqHz = absMinFreq;
                    st.viewMaxFreqHz = nyquistHz;
                    res.changed = true;
                } else {
                    st.dragMode = DragMode::FrequencyPan;
                }
            } else if (geom.colorbarRect.Contains(mouse)) {
                if (isDoubleClick) {
                    st.hasDbFloorOverride = false;
                    res.changed = true;
                } else {
                    st.dragMode = DragMode::ColorbarFloor;
                }
            } else if (geom.wavePlotRect.Contains(mouse) || geom.specPlotRect.Contains(mouse)) {
                const bool inSpec = geom.specPlotRect.Contains(mouse);
                const Rect activePlot = inSpec ? geom.specPlotRect : geom.wavePlotRect;
                const double clickTime = st.viewStartSec +
                    std::clamp(static_cast<double>((mouse.x - activePlot.x) / std::max(1.0f, activePlot.w)), 0.0, 1.0) * viewSpan;
                const float normYBottom = inSpec
                    ? (1.0f - std::clamp((mouse.y - activePlot.y) / std::max(1.0f, activePlot.h), 0.0f, 1.0f))
                    : 0.5f;
                const float clickFreq = inSpec
                    ? NormalizedYToFreqHz(normYBottom, st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale)
                    : 0.0f;

                if (isDoubleClick) {
                    if (st.selection.active) {
                        st.selection = {};
                    } else {
                        st.viewStartSec = 0.0;
                        st.viewEndSec = totalDurationSec;
                        st.viewMinFreqHz = absMinFreq;
                        st.viewMaxFreqHz = nyquistHz;
                    }
                    if (opts.selection) *opts.selection = st.selection;
                    res.changed = true;
                } else if (mods.alt) {
                    st.dragMode = DragMode::PlotPan;
                } else {
                    // Check if grabbing selection edges or interior
                    const float edgeTol = theme.Scale(6.0f);
                    bool grabbedHandle = false;
                    if (st.selection.active && st.selection.DurationSec() > 1e-6) {
                        const float selX0 = activePlot.x + static_cast<float>((st.selection.MinTimeSec() - st.viewStartSec) / viewSpan) * activePlot.w;
                        const float selX1 = activePlot.x + static_cast<float>((st.selection.MaxTimeSec() - st.viewStartSec) / viewSpan) * activePlot.w;

                        if (inSpec && st.selection.hasSpectralBox) {
                            const float selYHigh = activePlot.Bottom() - FreqHzToNormalizedY(st.selection.MaxFreqHz(), st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale) * activePlot.h;
                            const float selYLow  = activePlot.Bottom() - FreqHzToNormalizedY(st.selection.MinFreqHz(), st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale) * activePlot.h;
                            const bool inX = (mouse.x >= selX0 - edgeTol && mouse.x <= selX1 + edgeTol);
                            const bool inY = (mouse.y >= selYHigh - edgeTol && mouse.y <= selYLow + edgeTol);

                            if (inY && std::abs(mouse.x - selX0) <= edgeTol) {
                                st.dragMode = DragMode::SelectResizeLeft;
                                grabbedHandle = true;
                            } else if (inY && std::abs(mouse.x - selX1) <= edgeTol) {
                                st.dragMode = DragMode::SelectResizeRight;
                                grabbedHandle = true;
                            } else if (inX && std::abs(mouse.y - selYHigh) <= edgeTol) {
                                st.dragMode = DragMode::SelectResizeHighFreq;
                                grabbedHandle = true;
                            } else if (inX && std::abs(mouse.y - selYLow) <= edgeTol) {
                                st.dragMode = DragMode::SelectResizeLowFreq;
                                grabbedHandle = true;
                            } else if (mouse.x > selX0 && mouse.x < selX1 && mouse.y > selYHigh && mouse.y < selYLow) {
                                st.dragMode = DragMode::SelectMoveBox;
                                grabbedHandle = true;
                            }
                        } else {
                            if (std::abs(mouse.x - selX0) <= edgeTol) {
                                st.dragMode = DragMode::SelectResizeLeft;
                                grabbedHandle = true;
                            } else if (std::abs(mouse.x - selX1) <= edgeTol) {
                                st.dragMode = DragMode::SelectResizeRight;
                                grabbedHandle = true;
                            }
                        }
                    }

                    if (!grabbedHandle) {
                        st.playheadSec = clickTime;
                        if (opts.playheadTimeSec) *opts.playheadTimeSec = st.playheadSec;
                        st.dragAnchorTimeSec = clickTime;
                        st.dragAnchorFreqHz = clickFreq;
                        st.dragMode = (inSpec && opts.allowSpectralSelection) ? DragMode::SpecSelectNew : DragMode::WaveSelectNew;
                        res.changed = true;
                    }
                }
            }
        }

        // Middle or Right mouse button drag on plot -> Pan
        if (st.dragMode == DragMode::None && (middleDown || rightDown) &&
            (geom.wavePlotRect.Contains(mouse) || geom.specPlotRect.Contains(mouse))) {
            st.dragStartMouse = mouse;
            st.dragStartView0 = st.viewStartSec;
            st.dragStartView1 = st.viewEndSec;
            st.dragStartFreq0 = st.viewMinFreqHz;
            st.dragStartFreq1 = st.viewMaxFreqHz;
            st.dragMode = DragMode::PlotPan;
        }

        // 3. Process Active Drag
        if (st.dragMode != DragMode::None) {
            const bool keepDragging = (st.dragMode == DragMode::PlotPan)
                ? (mouseDown || middleDown || rightDown)
                : mouseDown;

            if (!keepDragging) {
                st.dragMode = DragMode::None;
                st.activeMarkerIdx = -1;
            } else {
                const float dx = mouse.x - st.dragStartMouse.x;
                const float dy = mouse.y - st.dragStartMouse.y;
                const float plotW = std::max(1.0f, geom.wavePlotRect.w > 0.0f ? geom.wavePlotRect.w : geom.specPlotRect.w);
                const double startSpan = std::max(1e-6, st.dragStartView1 - st.dragStartView0);

                switch (st.dragMode) {
                    case DragMode::Splitter: {
                        const float bodyTop = geom.topPanelRect.y;
                        const float bodyH = std::max(1.0f, geom.bottomPanelRect.Bottom() - bodyTop - geom.splitterRect.h);
                        st.splitRatio = std::clamp((mouse.y - bodyTop) / bodyH, 0.0f, 1.0f);
                        if (opts.splitRatioBinding) *opts.splitRatioBinding = st.splitRatio;
                        res.changed = true;
                        break;
                    }
                    case DragMode::OverviewPan: {
                        const double dt = static_cast<double>(dx / std::max(1.0f, geom.overviewRect.w)) * totalDurationSec;
                        double t0 = st.dragStartView0 + dt;
                        double t1 = st.dragStartView1 + dt;
                        if (t0 < 0.0) { t0 = 0.0; t1 = startSpan; }
                        if (t1 > totalDurationSec) { t1 = totalDurationSec; t0 = std::max(0.0, t1 - startSpan); }
                        st.viewStartSec = t0;
                        st.viewEndSec = t1;
                        res.changed = true;
                        break;
                    }
                    case DragMode::OverviewResizeLeft: {
                        const double dt = static_cast<double>(dx / std::max(1.0f, geom.overviewRect.w)) * totalDurationSec;
                        st.viewStartSec = std::clamp(st.dragStartView0 + dt, 0.0, st.viewEndSec - minTimeSpan);
                        res.changed = true;
                        break;
                    }
                    case DragMode::OverviewResizeRight: {
                        const double dt = static_cast<double>(dx / std::max(1.0f, geom.overviewRect.w)) * totalDurationSec;
                        st.viewEndSec = std::clamp(st.dragStartView1 + dt, st.viewStartSec + minTimeSpan, totalDurationSec);
                        res.changed = true;
                        break;
                    }
                    case DragMode::TimeRulerScrub: {
                        const double rel = std::clamp(static_cast<double>((mouse.x - geom.timeRulerRect.x) / std::max(1.0f, geom.timeRulerRect.w)), 0.0, 1.0);
                        st.playheadSec = st.viewStartSec + rel * viewSpan;
                        if (opts.playheadTimeSec) *opts.playheadTimeSec = st.playheadSec;
                        res.changed = true;
                        break;
                    }
                    case DragMode::MarkerMove: {
                        if (st.activeMarkerIdx >= 0 && static_cast<size_t>(st.activeMarkerIdx) < markerList.size()) {
                            const double rel = std::clamp(static_cast<double>((mouse.x - geom.timeRulerRect.x) / std::max(1.0f, geom.timeRulerRect.w)), 0.0, 1.0);
                            markerList[static_cast<size_t>(st.activeMarkerIdx)].timeSec =
                                std::clamp(st.viewStartSec + rel * viewSpan, 0.0, totalDurationSec);
                            res.activeMarkerIndex = st.activeMarkerIdx;
                            res.changed = true;
                        }
                        break;
                    }
                    case DragMode::AmplitudeGain: {
                        const float factor = std::pow(2.0f, -dy / theme.Scale(45.0f));
                        st.waveformGain = std::clamp(st.dragStartGain * factor, 0.1f, 64.0f);
                        if (opts.waveformGainBinding) *opts.waveformGainBinding = st.waveformGain;
                        res.changed = true;
                        break;
                    }
                    case DragMode::FrequencyPan: {
                        const float fSpan = st.dragStartFreq1 - st.dragStartFreq0;
                        const float df = (dy / std::max(1.0f, geom.specRulerRect.h)) * fSpan;
                        float f0 = st.dragStartFreq0 + df;
                        float f1 = st.dragStartFreq1 + df;
                        if (f0 < absMinFreq) { f0 = absMinFreq; f1 = f0 + fSpan; }
                        if (f1 > nyquistHz)  { f1 = nyquistHz;  f0 = std::max(absMinFreq, f1 - fSpan); }
                        st.viewMinFreqHz = f0;
                        st.viewMaxFreqHz = f1;
                        res.changed = true;
                        break;
                    }
                    case DragMode::ColorbarFloor: {
                        st.dbFloorOverride = std::clamp(st.dragStartDbFloor + dy * 0.4f, -130.0f, opts.dbCeil - 12.0f);
                        st.hasDbFloorOverride = true;
                        res.changed = true;
                        break;
                    }
                    case DragMode::PlotPan: {
                        const double dt = -static_cast<double>(dx / plotW) * startSpan;
                        double t0 = st.dragStartView0 + dt;
                        double t1 = st.dragStartView1 + dt;
                        if (t0 < 0.0) { t0 = 0.0; t1 = startSpan; }
                        if (t1 > totalDurationSec) { t1 = totalDurationSec; t0 = std::max(0.0, t1 - startSpan); }
                        st.viewStartSec = t0;
                        st.viewEndSec = t1;

                        if (geom.specPlotRect.h > 0.0f && st.dragStartMouse.y >= geom.specPlotRect.y && st.dragStartMouse.y <= geom.specPlotRect.Bottom()) {
                            const float fSpan = st.dragStartFreq1 - st.dragStartFreq0;
                            const float df = (dy / std::max(1.0f, geom.specPlotRect.h)) * fSpan;
                            float f0 = st.dragStartFreq0 + df;
                            float f1 = st.dragStartFreq1 + df;
                            if (f0 < absMinFreq) { f0 = absMinFreq; f1 = f0 + fSpan; }
                            if (f1 > nyquistHz)  { f1 = nyquistHz;  f0 = std::max(absMinFreq, f1 - fSpan); }
                            st.viewMinFreqHz = f0;
                            st.viewMaxFreqHz = f1;
                        }
                        res.changed = true;
                        break;
                    }
                    case DragMode::WaveSelectNew: {
                        if (std::abs(dx) > 3.0f) {
                            const double curT = st.viewStartSec +
                                std::clamp(static_cast<double>((mouse.x - geom.wavePlotRect.x) / plotW), 0.0, 1.0) * viewSpan;
                            st.selection.active = true;
                            st.selection.startTimeSec = std::min(st.dragAnchorTimeSec, curT);
                            st.selection.endTimeSec = std::max(st.dragAnchorTimeSec, curT);
                            st.selection.hasSpectralBox = false;
                            if (opts.selection) *opts.selection = st.selection;
                            res.changed = true;
                        }
                        break;
                    }
                    case DragMode::SpecSelectNew: {
                        if (std::hypot(dx, dy) > 3.0f) {
                            const double curT = st.viewStartSec +
                                std::clamp(static_cast<double>((mouse.x - geom.specPlotRect.x) / plotW), 0.0, 1.0) * viewSpan;
                            const float normYBottom = 1.0f - std::clamp((mouse.y - geom.specPlotRect.y) / std::max(1.0f, geom.specPlotRect.h), 0.0f, 1.0f);
                            const float curF = NormalizedYToFreqHz(normYBottom, st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale);
                            st.selection.active = true;
                            st.selection.startTimeSec = std::min(st.dragAnchorTimeSec, curT);
                            st.selection.endTimeSec = std::max(st.dragAnchorTimeSec, curT);
                            st.selection.hasSpectralBox = true;
                            st.selection.lowFreqHz = std::min(st.dragAnchorFreqHz, curF);
                            st.selection.highFreqHz = std::max(st.dragAnchorFreqHz, curF);
                            if (opts.selection) *opts.selection = st.selection;
                            res.changed = true;
                        }
                        break;
                    }
                    case DragMode::SelectResizeLeft: {
                        const double curT = st.viewStartSec +
                            std::clamp(static_cast<double>((mouse.x - (geom.wavePlotRect.w > 0.0f ? geom.wavePlotRect.x : geom.specPlotRect.x)) / plotW), 0.0, 1.0) * viewSpan;
                        st.selection.startTimeSec = std::min(curT, st.dragStartSelection.MaxTimeSec() - 1e-5);
                        st.selection.endTimeSec = st.dragStartSelection.MaxTimeSec();
                        if (opts.selection) *opts.selection = st.selection;
                        res.changed = true;
                        break;
                    }
                    case DragMode::SelectResizeRight: {
                        const double curT = st.viewStartSec +
                            std::clamp(static_cast<double>((mouse.x - (geom.wavePlotRect.w > 0.0f ? geom.wavePlotRect.x : geom.specPlotRect.x)) / plotW), 0.0, 1.0) * viewSpan;
                        st.selection.startTimeSec = st.dragStartSelection.MinTimeSec();
                        st.selection.endTimeSec = std::max(curT, st.dragStartSelection.MinTimeSec() + 1e-5);
                        if (opts.selection) *opts.selection = st.selection;
                        res.changed = true;
                        break;
                    }
                    case DragMode::SelectResizeHighFreq: {
                        const float normYBottom = 1.0f - std::clamp((mouse.y - geom.specPlotRect.y) / std::max(1.0f, geom.specPlotRect.h), 0.0f, 1.0f);
                        const float curF = NormalizedYToFreqHz(normYBottom, st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale);
                        st.selection.lowFreqHz = st.dragStartSelection.MinFreqHz();
                        st.selection.highFreqHz = std::max(curF, st.dragStartSelection.MinFreqHz() + 10.0f);
                        if (opts.selection) *opts.selection = st.selection;
                        res.changed = true;
                        break;
                    }
                    case DragMode::SelectResizeLowFreq: {
                        const float normYBottom = 1.0f - std::clamp((mouse.y - geom.specPlotRect.y) / std::max(1.0f, geom.specPlotRect.h), 0.0f, 1.0f);
                        const float curF = NormalizedYToFreqHz(normYBottom, st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale);
                        st.selection.lowFreqHz = std::min(curF, st.dragStartSelection.MaxFreqHz() - 10.0f);
                        st.selection.highFreqHz = st.dragStartSelection.MaxFreqHz();
                        if (opts.selection) *opts.selection = st.selection;
                        res.changed = true;
                        break;
                    }
                    case DragMode::SelectMoveBox: {
                        const double dt = static_cast<double>(dx / plotW) * viewSpan;
                        const double selDur = st.dragStartSelection.DurationSec();
                        double newT0 = std::clamp(st.dragStartSelection.MinTimeSec() + dt, 0.0, std::max(0.0, totalDurationSec - selDur));
                        st.selection.startTimeSec = newT0;
                        st.selection.endTimeSec = newT0 + selDur;

                        if (st.dragStartSelection.hasSpectralBox && geom.specPlotRect.h > 0.0f) {
                            const float df = -(dy / std::max(1.0f, geom.specPlotRect.h)) * (st.viewMaxFreqHz - st.viewMinFreqHz);
                            const float bw = st.dragStartSelection.BandwidthHz();
                            const float newF0 = std::clamp(st.dragStartSelection.MinFreqHz() + df, absMinFreq, std::max(absMinFreq, nyquistHz - bw));
                            st.selection.lowFreqHz = newF0;
                            st.selection.highFreqHz = newF0 + bw;
                        }
                        if (opts.selection) *opts.selection = st.selection;
                        res.changed = true;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    // Ensure spectrogram cache is updated for current view parameters
    const float effectiveDbFloor = st.hasDbFloorOverride ? st.dbFloorOverride : opts.dbFloor;
    const bool gpuShadersActive = (canvas && canvas->SupportsGpuShaders());
    st.specCache.EnsureValid(
        samples,
        fp,
        sampleRate,
        st.viewStartSec,
        st.viewEndSec,
        st.viewMinFreqHz,
        st.viewMaxFreqHz,
        opts.fftSize,
        opts.fftWindow,
        opts.frequencyScale,
        opts.colormap,
        effectiveDbFloor,
        opts.dbCeil,
        gpuShadersActive
    );

    // Populate live response telemetry
    res.viewStartSec = st.viewStartSec;
    res.viewEndSec = st.viewEndSec;
    res.totalDurationSec = totalDurationSec;
    res.totalSamples = samples.size();
    res.splitRatio = st.splitRatio;
    res.waveformGain = st.waveformGain;
    res.viewMinFreqHz = st.viewMinFreqHz;
    res.viewMaxFreqHz = st.viewMaxFreqHz;
    res.playheadSec = st.playheadSec;
    res.selection = st.selection;

    if (st.selection.active && !samples.empty()) {
        const size_t s0 = static_cast<size_t>(std::clamp(st.selection.MinTimeSec() * sampleRate, 0.0, static_cast<double>(samples.size() - 1)));
        const size_t s1 = static_cast<size_t>(std::clamp(st.selection.MaxTimeSec() * sampleRate, static_cast<double>(s0 + 1), static_cast<double>(samples.size())));
        const PeakBucket pb = st.pyramid.QueryRange(samples, s0, s1);
        const float peakAbs = std::max(std::abs(pb.minVal), std::abs(pb.maxVal));
        res.selectionPeakDbFs = 20.0f * std::log10(std::max(1e-6f, peakAbs));
        res.selectionRmsDbFs = 20.0f * std::log10(std::max(1e-6f, pb.rmsVal));
        res.selectionCentroidHz = st.selection.hasSpectralBox
            ? 0.5f * (st.selection.MinFreqHz() + st.selection.MaxFreqHz())
            : 0.5f * (st.viewMinFreqHz + st.viewMaxFreqHz);
    }

    const bool inWave = geom.wavePlotRect.w > 0.0f && geom.wavePlotRect.h > 0.0f && geom.wavePlotRect.Contains(mouse);
    const bool inSpec = geom.specPlotRect.w > 0.0f && geom.specPlotRect.h > 0.0f && geom.specPlotRect.Contains(mouse);
    res.cursorInsidePlot = (inWave || inSpec) && baseRes.hovered;
    if (res.cursorInsidePlot) {
        const Rect activePlot = inSpec ? geom.specPlotRect : geom.wavePlotRect;
        const float normX = std::clamp((mouse.x - activePlot.x) / std::max(1.0f, activePlot.w), 0.0f, 1.0f);
        res.cursorTimeSec = st.viewStartSec + static_cast<double>(normX) * (st.viewEndSec - st.viewStartSec);
        if (!samples.empty()) {
            res.cursorSampleIndex = std::min(
                samples.size() - 1,
                static_cast<size_t>(std::max(0.0, std::round(res.cursorTimeSec * static_cast<double>(sampleRate))))
            );
            res.cursorAmplitude = samples[res.cursorSampleIndex];
            res.cursorDbFs = 20.0f * std::log10(std::max(1e-6f, std::abs(res.cursorAmplitude)));
        }
        if (inSpec) {
            const float normYFromTop = std::clamp((mouse.y - geom.specPlotRect.y) / std::max(1.0f, geom.specPlotRect.h), 0.0f, 1.0f);
            res.cursorFreqHz = NormalizedYToFreqHz(1.0f - normYFromTop, st.viewMinFreqHz, st.viewMaxFreqHz, opts.frequencyScale);
            res.cursorSpectralDb = st.specCache.SampleDbAt(normX, normYFromTop);
        } else {
            res.cursorFreqHz = 0.0f;
            res.cursorSpectralDb = st.specCache.SampleDbAt(normX, 0.5f);
        }
    }

    if (baseRes.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    // Create Scene Graph Node
    Node* node = ctx.CreateNode(NodeType::SignalView, id);
    auto* payload = ctx.AllocPayload<SignalViewPayload>();
    payload->samples = samples;
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.tooltip = ctx.AllocString(opts.tooltip);
    payload->state = &st;
    payload->mousePos = mouse;
    payload->isHovered = baseRes.hovered;

    node->text = payload->opts.label.text.empty() ? "SignalView" : payload->opts.label.text;
    node->SetPayload(payload);

    uint64_t markerFp = markerList.size();
    for (const auto& mk : markerList) {
        markerFp = detail::HashArgs(markerFp, mk.timeSec, mk.label);
    }

    node->visualHash = detail::HashArgs(
        fp,
        payload->opts.label,
        payload->opts.description,
        payload->opts.badge,
        static_cast<uint8_t>(opts.panelOrder),
        static_cast<uint8_t>(opts.amplitudeScale),
        static_cast<uint8_t>(opts.frequencyScale),
        static_cast<uint8_t>(opts.colormap),
        static_cast<uint8_t>(opts.fftWindow),
        opts.fftSize,
        effectiveDbFloor,
        opts.dbCeil,
        opts.showOverviewBar,
        opts.showTimeRuler,
        opts.showAmplitudeRuler,
        opts.showFrequencyRuler,
        opts.showColorbar,
        opts.showGrid,
        opts.showRms,
        opts.showCrosshair,
        st.viewStartSec,
        st.viewEndSec,
        st.splitRatio,
        st.waveformGain,
        st.viewMinFreqHz,
        st.viewMaxFreqHz,
        st.playheadSec,
        st.selection.active,
        st.selection.startTimeSec,
        st.selection.endTimeSec,
        st.selection.hasSpectralBox,
        st.selection.lowFreqHz,
        st.selection.highFreqHz,
        markerFp,
        res.cursorInsidePlot ? mouse.x : -1.0f,
        res.cursorInsidePlot ? mouse.y : -1.0f,
        static_cast<uint8_t>(st.dragMode),
        activeSize,
        effectiveDisabled,
        gpuShadersActive
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<SignalViewPayload>();
        if (!p) return {0.0f, 0.0f};
        return SignalView::Measure(p->samples, p->opts, n->uiSize, c, th, availableWidth);
    };

    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& th) noexcept {
        const auto* p = n->As<SignalViewPayload>();
        if (!p || !p->state) return;

        const SignalViewOptions& o = p->opts;
        SignalViewInternalState& s = *p->state;
        const float scale = th.Scale(1.0f);
        const ElementSpec spec = MakeSignalViewElementSpec(o, n->uiSize, th, n->disabled);
        const Vec2 ctrlSize = {n->computedRect.w, n->computedRect.h};
        const ElementGeometry elemGeom = Element::Resolve(n->computedRect, ctrlSize, spec, &c, th);
        Element::RenderText(c, elemGeom, spec, th);

        const Rect box = elemGeom.controlRect;
        if (box.w <= 4.0f || box.h <= 4.0f) return;

        const float rad = o.borderRadius.has_value() ? th.Scale(*o.borderRadius) : th.GetRadius(n->uiSize);
        const bool isDark = (th.mode == ThemeMode::Dark);
        const Color bgSurface = isDark ? Color(11, 15, 25, 255) : Color(248, 250, 252, 255);
        const Color rulerBg   = isDark ? Color(17, 24, 39, 255) : Color(238, 242, 247, 255);
        const Color gridCol   = isDark ? Color(51, 65, 85, 95)  : Color(203, 213, 225, 140);
        const Color subGridCol = isDark ? Color(30, 41, 59, 90) : Color(226, 232, 240, 120);
        const Color borderCol = th.palette.borderSubtle;
        const Color textMuted = th.palette.textMuted;
        const Color textSec   = th.palette.textSecondary;

        const SignalViewGeometry g = ComputeSignalGeometry(box, o, s.splitRatio, th);
        const float sampleRate = std::max(1.0f, o.sampleRate);
        const double totalDur = !p->samples.empty()
            ? (static_cast<double>(p->samples.size()) / static_cast<double>(sampleRate))
            : 1.0;
        const double viewSpan = std::max(1e-6, s.viewEndSec - s.viewStartSec);
        const float smallFs = th.GetFontSize(TextRole::LabelSmall);
        const auto& markers = o.markers ? *o.markers : s.markers;
        const float effFloor = s.hasDbFloorOverride ? s.dbFloorOverride : o.dbFloor;

        uint64_t markerFp = markers.size();
        for (const auto& mk : markers) {
            markerFp = detail::HashArgs(markerFp, mk.timeSec, mk.label, mk.color.ToUint32());
        }

        const uint64_t staticKey = detail::HashArgs(
            s.lastSignalFp,
            std::round(box.x * 2.0f),
            std::round(box.y * 2.0f),
            std::round(box.w * 2.0f),
            std::round(box.h * 2.0f),
            scale,
            c.GetScale(),
            isDark,
            th.palette.accent.ToUint32(),
            c.SupportsGpuShaders(),
            static_cast<uint8_t>(o.panelOrder),
            static_cast<uint8_t>(o.amplitudeScale),
            static_cast<uint8_t>(o.frequencyScale),
            static_cast<uint8_t>(o.colormap),
            static_cast<uint8_t>(o.fftWindow),
            o.fftSize,
            effFloor,
            o.dbCeil,
            o.showOverviewBar,
            o.showTimeRuler,
            o.showAmplitudeRuler,
            o.showFrequencyRuler,
            o.showColorbar,
            o.showGrid,
            o.showRms,
            s.viewStartSec,
            s.viewEndSec,
            s.splitRatio,
            s.waveformGain,
            s.viewMinFreqHz,
            s.viewMaxFreqHz,
            markerFp
        );

        auto makeSpecGpuDesc = [&]() {
            ICanvas::SpectrogramGpuFieldDesc gpuDesc{};
            gpuDesc.widgetId = n->id.value;
            gpuDesc.dataGeneration = s.specCache.dbGridGeneration;
            gpuDesc.dbGrid = s.specCache.dbGrid.data();
            gpuDesc.texWidth = SpectrogramCache::kTexW;
            gpuDesc.texHeight = SpectrogramCache::kTexH;
            gpuDesc.colormapId = static_cast<uint8_t>(o.colormap);
            CopyColormapLutAbgr(o.colormap, gpuDesc.colormapAbgr);
            gpuDesc.plotRect = g.specPlotRect;
            gpuDesc.dbFloor = s.specCache.dbFloor;
            gpuDesc.dbCeil = s.specCache.dbCeil;
            gpuDesc.bgColor = Color(8, 6, 22, 255);
            return gpuDesc;
        };

        bool restoredFromCache = false;
        if (s.baseLayerCache.valid && s.baseLayerCache.cacheKey == staticKey) {
            if (c.RestoreRegion(box, s.baseLayerCache.pixels, s.baseLayerCache.width, s.baseLayerCache.height)) {
                restoredFromCache = true;
                if (g.specPlotRect.w > 0.0f && g.specPlotRect.h > 2.0f && !s.specCache.dbGrid.empty()) {
                    c.PushClip(g.specPlotRect);
                    const auto gpuDesc = makeSpecGpuDesc();
                    c.DrawSpectrogramGpuField(gpuDesc, false);
                    c.PopClip();
                }
            }
        }

        if (!restoredFromCache) {
            // =====================================================================
            // PASS A: Render Static Base Layer (Overview, Rulers, Waveform, Spec Grid, Markers)
            // =====================================================================
            c.DrawRoundRect(box, rad, bgSurface, borderCol, 1.0f);
            c.PushClip(box.Inset(1.0f));

            // 1. Overview Minimap Bar (Static part)
            if (o.showOverviewBar && g.overviewRect.w > 0.0f && g.overviewRect.h > 0.0f) {
                c.DrawRect(Rect(box.x, g.overviewRect.y, box.w, g.overviewRect.h), rulerBg);
                c.DrawTextBox("OVERVIEW", Rect(box.x + 6.0f * scale, g.overviewRect.y, std::max(0.0f, g.overviewRect.x - box.x - 8.0f * scale), g.overviewRect.h),
                              smallFs * 0.85f, textMuted, 0.0f, 0.5f);

                const Rect ov = g.overviewRect;
                const uint32_t ovW = std::max(1u, static_cast<uint32_t>(std::lround(ov.w)));
                const uint32_t ovH = std::max(1u, static_cast<uint32_t>(std::lround(ov.h)));
                s.overviewCache.EnsureValid(s.pyramid, s.lastSignalFp, ovW, ovH, isDark, th.palette.accent);
                if (!s.overviewCache.pixels.empty()) {
                    c.DrawPixels(s.overviewCache.pixels.data(), s.overviewCache.width, s.overviewCache.height, ov);
                } else {
                    c.DrawRect(ov, isDark ? Color(8, 12, 20, 255) : Color(230, 236, 244, 255));
                }

                // Viewport lens [viewStartSec .. viewEndSec]
                const float lx0 = ov.x + static_cast<float>(s.viewStartSec / totalDur) * ov.w;
                const float lx1 = ov.x + static_cast<float>(s.viewEndSec / totalDur) * ov.w;
                const Rect lensRect(lx0, ov.y + 1.0f, std::max(4.0f, lx1 - lx0), ov.h - 2.0f);
                c.DrawRoundRect(lensRect, 3.0f * scale,
                                th.palette.accent.WithAlpha(38),
                                th.palette.accent,
                                1.5f);
                c.DrawRect(Rect(lensRect.x, lensRect.y + 3.0f * scale, 2.5f * scale, std::max(2.0f, lensRect.h - 6.0f * scale)), th.palette.accent);
                c.DrawRect(Rect(lensRect.Right() - 2.5f * scale, lensRect.y + 3.0f * scale, 2.5f * scale, std::max(2.0f, lensRect.h - 6.0f * scale)), th.palette.accent);
                c.DrawLine({box.x, ov.Bottom()}, {box.Right(), ov.Bottom()}, borderCol, 1.0f);
            }

            // 2. Compute Shared Adaptive Time Grid Ticks
            const float maxTimeTicks = std::max(3.0f, std::floor(g.timeRulerRect.w / (88.0f * scale)));
            const double majorTimeStep = NiceTimeStep(viewSpan, maxTimeTicks);
            const double minorTimeStep = majorTimeStep * 0.2;
            const double firstMajorTime = std::ceil(s.viewStartSec / majorTimeStep) * majorTimeStep;
            const double firstMinorTime = std::ceil(s.viewStartSec / minorTimeStep) * minorTimeStep;

            // 3. Time Ruler & Cue Markers
            if (o.showTimeRuler && g.timeRulerRect.w > 0.0f && g.timeRulerRect.h > 0.0f) {
                const Rect tr = g.timeRulerRect;
                c.DrawRect(Rect(box.x, tr.y, box.w, tr.h), rulerBg);
                c.DrawTextBox(o.amplitudeScale == AmplitudeScale::dBFS ? "dBFS / Hz" : "LIN / Hz",
                              Rect(box.x + 6.0f * scale, tr.y, std::max(0.0f, tr.x - box.x - 8.0f * scale), tr.h),
                              smallFs * 0.85f, textMuted, 0.0f, 0.5f);

                for (double t = firstMinorTime; t <= s.viewEndSec + 1e-9; t += minorTimeStep) {
                    const float tx = tr.x + static_cast<float>((t - s.viewStartSec) / viewSpan) * tr.w;
                    if (tx >= tr.x && tx <= tr.Right()) {
                        c.DrawLine({tx, tr.Bottom() - 4.0f * scale}, {tx, tr.Bottom()}, borderCol, 1.0f);
                    }
                }

                for (double t = firstMajorTime; t <= s.viewEndSec + 1e-9; t += majorTimeStep) {
                    const float tx = tr.x + static_cast<float>((t - s.viewStartSec) / viewSpan) * tr.w;
                    if (tx >= tr.x && tx <= tr.Right()) {
                        c.DrawLine({tx, tr.Bottom() - 8.0f * scale}, {tx, tr.Bottom()}, textMuted, 1.0f);
                        const std::string lbl = FormatTimeTick(t, viewSpan);
                        c.DrawText(lbl, {tx + 3.0f * scale, tr.y + 3.0f * scale}, smallFs * 0.88f, textSec);
                    }
                }

                for (size_t i = 0; i < markers.size(); ++i) {
                    const auto& mk = markers[i];
                    if (mk.timeSec < s.viewStartSec || mk.timeSec > s.viewEndSec) continue;
                    const float mx = tr.x + static_cast<float>((mk.timeSec - s.viewStartSec) / viewSpan) * tr.w;
                    const float flagW = std::max(28.0f * scale, c.MeasureText(mk.label, smallFs * 0.82f).x + 8.0f * scale);
                    const float flagH = 13.0f * scale;
                    const Rect flagRect(std::min(mx, tr.Right() - flagW), tr.y + 1.0f, flagW, flagH);
                    c.DrawRoundRect(flagRect, 2.5f * scale, mk.color);
                    c.DrawTriangle({mx, tr.Bottom()}, {mx - 4.0f * scale, tr.Bottom() - 5.0f * scale}, {mx + 4.0f * scale, tr.Bottom() - 5.0f * scale}, mk.color);
                    c.DrawTextBox(mk.label, flagRect, smallFs * 0.80f, Color(15, 23, 42, 255), 0.5f, 0.5f);
                }

                c.DrawLine({box.x, tr.Bottom()}, {box.Right(), tr.Bottom()}, borderCol, 1.0f);
            }

            // 4. Waveform Panel + Amplitude Ruler
            if (g.wavePlotRect.w > 0.0f && g.wavePlotRect.h > 2.0f) {
                const Rect wp = g.wavePlotRect;
                const Rect wr = g.waveRulerRect;

                if (wr.w > 0.0f) {
                    c.DrawRect(wr, rulerBg);
                }

                c.PushClip(wp);

                const double samplesInView = viewSpan * static_cast<double>(sampleRate);
                const double samplesPerPixel = samplesInView / static_cast<double>(std::max(1.0f, wp.w));

                if (!p->samples.empty() && samplesPerPixel <= 0.35) {
                    c.DrawRect(wp, isDark ? Color(10, 14, 23, 255) : Color(252, 253, 255, 255));
                    if (o.showGrid) {
                        for (double t = firstMajorTime; t <= s.viewEndSec + 1e-9; t += majorTimeStep) {
                            const float tx = wp.x + static_cast<float>((t - s.viewStartSec) / viewSpan) * wp.w;
                            c.DrawLine({tx, wp.y}, {tx, wp.Bottom()}, gridCol, 1.0f);
                        }
                    }
                    static constexpr std::array<float, 5> kNormLevels = {0.9f, 0.5f, 0.0f, -0.5f, -0.9f};
                    for (float lv : kNormLevels) {
                        const float gy = wp.y + (0.5f - 0.5f * lv) * wp.h;
                        if (o.showGrid || lv == 0.0f) {
                            c.DrawLine({wp.x, gy}, {wp.Right(), gy}, (lv == 0.0f) ? textMuted.WithAlpha(120) : subGridCol, 1.0f);
                        }
                    }

                    const Color peakCol = isDark ? Color(56, 189, 248, 215) : Color(2, 132, 199, 220);
                    const Color rmsCol  = isDark ? Color(14, 165, 233, 255) : Color(3, 105, 161, 255);
                    const int64_t idxStart = std::max<int64_t>(0, static_cast<int64_t>(std::floor(s.viewStartSec * sampleRate)) - 1);
                    const int64_t idxEnd   = std::min<int64_t>(static_cast<int64_t>(p->samples.size()) - 1,
                                                               static_cast<int64_t>(std::ceil(s.viewEndSec * sampleRate)) + 1);
                    Vec2 prevPt{};
                    bool hasPrev = false;
                    for (int64_t idx = idxStart; idx <= idxEnd; ++idx) {
                        const double tSec = static_cast<double>(idx) / static_cast<double>(sampleRate);
                        const float sx = wp.x + static_cast<float>((tSec - s.viewStartSec) / viewSpan) * wp.w;
                        const float sy = wp.y + AmplitudeToNormY(p->samples[static_cast<size_t>(idx)], s.waveformGain, o.amplitudeScale) * wp.h;
                        if (hasPrev) {
                            c.DrawLine(prevPt, {sx, sy}, rmsCol, 1.5f);
                        }
                        if (samplesPerPixel <= 0.18) {
                            const float dotR = 2.5f * scale;
                            c.DrawRoundRect(Rect(sx - dotR, sy - dotR, dotR * 2.0f, dotR * 2.0f), dotR, peakCol);
                        }
                        prevPt = {sx, sy};
                        hasPrev = true;
                    }
                } else {
                    const uint32_t wpW = std::max(1u, static_cast<uint32_t>(std::lround(wp.w)));
                    const uint32_t wpH = std::max(1u, static_cast<uint32_t>(std::lround(wp.h)));
                    s.waveCache.EnsureValid(
                        p->samples,
                        s.pyramid,
                        s.lastSignalFp,
                        sampleRate,
                        s.viewStartSec,
                        s.viewEndSec,
                        s.waveformGain,
                        o.amplitudeScale,
                        o.showRms,
                        o.showGrid,
                        isDark,
                        firstMajorTime,
                        majorTimeStep,
                        scale,
                        gridCol,
                        subGridCol,
                        textMuted.WithAlpha(120),
                        wpW,
                        wpH
                    );
                    if (!s.waveCache.pixels.empty()) {
                        c.DrawPixels(s.waveCache.pixels.data(), s.waveCache.width, s.waveCache.height, wp);
                    }
                }

                for (const auto& mk : markers) {
                    if (mk.timeSec < s.viewStartSec || mk.timeSec > s.viewEndSec) continue;
                    const float mx = wp.x + static_cast<float>((mk.timeSec - s.viewStartSec) / viewSpan) * wp.w;
                    c.DrawLine({mx, wp.y}, {mx, wp.Bottom()}, mk.color.WithAlpha(160), 1.0f);
                }

                c.PopClip();

                if (o.showAmplitudeRuler && wr.w > 0.0f && wr.h > 24.0f) {
                    constexpr float kNormLevels[] = {1.0f, 0.5f, 0.0f, -0.5f, -1.0f};
                    c.PushClip(wr);
                    for (float lv : kNormLevels) {
                        const float gy = wr.y + (0.5f - 0.5f * lv) * wr.h;
                        c.DrawLine({wr.Right() - 5.0f * scale, gy}, {wr.Right(), gy}, textMuted, 1.0f);
                        const float effAmp = std::abs(lv) / std::max(0.01f, s.waveformGain);
                        char lbl[24]{};
                        if (o.amplitudeScale == AmplitudeScale::dBFS) {
                            if (std::abs(lv) < 1e-3f) {
                                std::snprintf(lbl, sizeof(lbl), "-inf");
                            } else {
                                const float db = 20.0f * std::log10(std::max(1e-5f, effAmp));
                                std::snprintf(lbl, sizeof(lbl), "%+.0f dB", db);
                            }
                        } else {
                            std::snprintf(lbl, sizeof(lbl), "%+.2f", lv / std::max(0.01f, s.waveformGain));
                        }
                        c.DrawTextBox(lbl, Rect(wr.x + 2.0f * scale, gy - 7.0f * scale, wr.w - 8.0f * scale, 14.0f * scale),
                                      smallFs * 0.82f, textSec, 1.0f, 0.5f);
                    }
                    c.PopClip();
                    c.DrawLine({wr.Right(), wr.y}, {wr.Right(), wr.Bottom()}, borderCol, 1.0f);
                }
            }

            // 5. Spectrogram Panel + Frequency Ruler + Colorbar
            if (g.specPlotRect.w > 0.0f && g.specPlotRect.h > 2.0f) {
                const Rect sp = g.specPlotRect;
                const Rect sr = g.specRulerRect;
                const Rect cb = g.colorbarRect;

                if (sr.w > 0.0f) {
                    c.DrawRect(sr, rulerBg);
                }
                if (cb.w > 0.0f) {
                    c.DrawRect(cb, rulerBg);
                }

                c.PushClip(sp);
                if (!s.specCache.dbGrid.empty()) {
                    const auto gpuDesc = makeSpecGpuDesc();
                    if (!c.DrawSpectrogramGpuField(gpuDesc, true)) {
                        if (!s.specCache.pixels.empty()) {
                            c.DrawPixels(s.specCache.pixels.data(), SpectrogramCache::kTexW, SpectrogramCache::kTexH, sp);
                        } else {
                            c.DrawRect(sp, Color(8, 6, 22, 255));
                        }
                    }
                } else {
                    c.DrawRect(sp, Color(8, 6, 22, 255));
                }

                if (o.showGrid) {
                    for (double t = firstMajorTime; t <= s.viewEndSec + 1e-9; t += majorTimeStep) {
                        const float tx = sp.x + static_cast<float>((t - s.viewStartSec) / viewSpan) * sp.w;
                        c.DrawLine({tx, sp.y}, {tx, sp.Bottom()}, Color(255, 255, 255, 38), 1.0f);
                    }
                }

                const int numFreqTicks = std::max(2, static_cast<int>(std::floor(sp.h / (32.0f * scale))));
                for (int i = 0; i <= numFreqTicks; ++i) {
                    const float normFromBottom = static_cast<float>(i) / static_cast<float>(numFreqTicks);
                    const float gy = sp.Bottom() - normFromBottom * sp.h;
                    if (o.showGrid && i > 0 && i < numFreqTicks) {
                        c.DrawLine({sp.x, gy}, {sp.Right(), gy}, Color(255, 255, 255, 38), 1.0f);
                    }
                }

                for (const auto& mk : markers) {
                    if (mk.timeSec < s.viewStartSec || mk.timeSec > s.viewEndSec) continue;
                    const float mx = sp.x + static_cast<float>((mk.timeSec - s.viewStartSec) / viewSpan) * sp.w;
                    c.DrawLine({mx, sp.y}, {mx, sp.Bottom()}, mk.color.WithAlpha(170), 1.0f);
                }

                c.PopClip();

                if (o.showFrequencyRuler && sr.w > 0.0f && sr.h > 24.0f) {
                    c.PushClip(sr);
                    for (int i = 0; i <= numFreqTicks; ++i) {
                        const float normFromBottom = static_cast<float>(i) / static_cast<float>(numFreqTicks);
                        const float gy = std::clamp(sr.Bottom() - normFromBottom * sr.h, sr.y + 6.0f * scale, sr.Bottom() - 6.0f * scale);
                        const float freq = NormalizedYToFreqHz(normFromBottom, s.viewMinFreqHz, s.viewMaxFreqHz, o.frequencyScale);
                        c.DrawLine({sr.Right() - 5.0f * scale, sr.Bottom() - normFromBottom * sr.h},
                                   {sr.Right(), sr.Bottom() - normFromBottom * sr.h}, textMuted, 1.0f);
                        const std::string lbl = FormatFreqTick(freq) + "Hz";
                        c.DrawTextBox(lbl, Rect(sr.x + 2.0f * scale, gy - 7.0f * scale, sr.w - 8.0f * scale, 14.0f * scale),
                                      smallFs * 0.80f, textSec, 1.0f, 0.5f);
                    }
                    c.PopClip();
                    c.DrawLine({sr.Right(), sr.y}, {sr.Right(), sr.Bottom()}, borderCol, 1.0f);
                }

                if (o.showColorbar && cb.w > 0.0f && cb.h > 24.0f) {
                    c.PushClip(cb);
                    c.DrawLine({cb.x, cb.y}, {cb.x, cb.Bottom()}, borderCol, 1.0f);
                    const Rect barStrip(cb.x + 5.0f * scale, cb.y + 6.0f * scale, 8.0f * scale, std::max(8.0f, cb.h - 12.0f * scale));
                    DrawColormapBar(c, barStrip, o.colormap, true, false, 2.0f * scale, borderCol);

                    const int dbTicks = std::max(2, static_cast<int>(std::floor(barStrip.h / (28.0f * scale))));
                    for (int i = 0; i <= dbTicks; ++i) {
                        const float frac = static_cast<float>(i) / static_cast<float>(dbTicks);
                        const float dbVal = o.dbCeil - frac * (o.dbCeil - effFloor);
                        const float ty = std::clamp(barStrip.y + frac * barStrip.h, cb.y + 6.0f * scale, cb.Bottom() - 6.0f * scale);
                        char lbl[16]{};
                        std::snprintf(lbl, sizeof(lbl), "%.0f", dbVal);
                        c.DrawTextBox(lbl, Rect(barStrip.Right() + 2.0f * scale, ty - 6.0f * scale, cb.Right() - barStrip.Right() - 3.0f * scale, 12.0f * scale),
                                      smallFs * 0.78f, textMuted, 0.0f, 0.5f);
                    }
                    c.PopClip();
                }
            }

            // 6. Horizontal Splitter Bar (Static base state)
            if (g.splitterRect.w > 0.0f && g.splitterRect.h > 0.0f) {
                const Color splitBg = isDark ? Color(24, 34, 52, 255) : Color(218, 226, 236, 255);
                c.DrawRect(g.splitterRect, splitBg);
                c.DrawLine({g.splitterRect.x, g.splitterRect.y}, {g.splitterRect.Right(), g.splitterRect.y}, borderCol, 1.0f);
                c.DrawLine({g.splitterRect.x, g.splitterRect.Bottom()}, {g.splitterRect.Right(), g.splitterRect.Bottom()}, borderCol, 1.0f);
                const float pillW = 36.0f * scale;
                const float pillH = 2.5f * scale;
                const Rect pillRect(
                    g.splitterRect.x + (g.splitterRect.w - pillW) * 0.5f,
                    g.splitterRect.y + (g.splitterRect.h - pillH) * 0.5f,
                    pillW,
                    pillH
                );
                c.DrawRoundRect(pillRect, pillH * 0.5f, textMuted);
            }

            c.PopClip();

            if (c.CaptureRegion(box, s.baseLayerCache.pixels, s.baseLayerCache.width, s.baseLayerCache.height)) {
                s.baseLayerCache.cacheKey = staticKey;
                s.baseLayerCache.valid = true;
            }
        }

        // =====================================================================
        // PASS B: Dynamic Overlay Layer (Selection, Playhead, Splitter Hover, Crosshair HUD)
        // =====================================================================
        c.PushClip(box.Inset(1.0f));

        // 1. Overview dynamic overlays (Selection + Playhead)
        if (o.showOverviewBar && g.overviewRect.w > 0.0f && g.overviewRect.h > 0.0f) {
            const Rect ov = g.overviewRect;
            if (s.selection.active && s.selection.DurationSec() > 1e-6) {
                const float sx0 = ov.x + static_cast<float>(s.selection.MinTimeSec() / totalDur) * ov.w;
                const float sx1 = ov.x + static_cast<float>(s.selection.MaxTimeSec() / totalDur) * ov.w;
                c.DrawRect(Rect(sx0, ov.y, std::max(1.0f, sx1 - sx0), ov.h), Color(56, 189, 248, 45));
            }
            const float phX = ov.x + static_cast<float>(s.playheadSec / totalDur) * ov.w;
            c.DrawLine({phX, ov.y}, {phX, ov.Bottom()}, Color(239, 68, 68, 230), 1.5f);
        }

        // 2. Time Ruler Playhead triangle
        if (o.showTimeRuler && g.timeRulerRect.w > 0.0f && g.timeRulerRect.h > 0.0f) {
            const Rect tr = g.timeRulerRect;
            if (s.playheadSec >= s.viewStartSec && s.playheadSec <= s.viewEndSec) {
                const float px = tr.x + static_cast<float>((s.playheadSec - s.viewStartSec) / viewSpan) * tr.w;
                const Color phCol(239, 68, 68, 255);
                c.DrawTriangle(
                    {px - 5.5f * scale, tr.Bottom() - 9.0f * scale},
                    {px + 5.5f * scale, tr.Bottom() - 9.0f * scale},
                    {px, tr.Bottom()},
                    phCol
                );
            }
        }

        // 3. Waveform dynamic overlays (Selection + Playhead)
        if (g.wavePlotRect.w > 0.0f && g.wavePlotRect.h > 2.0f) {
            const Rect wp = g.wavePlotRect;
            c.PushClip(wp);
            if (s.selection.active && s.selection.DurationSec() > 1e-6) {
                const float sx0 = wp.x + static_cast<float>((s.selection.MinTimeSec() - s.viewStartSec) / viewSpan) * wp.w;
                const float sx1 = wp.x + static_cast<float>((s.selection.MaxTimeSec() - s.viewStartSec) / viewSpan) * wp.w;
                const Rect selR = Rect(sx0, wp.y, std::max(1.0f, sx1 - sx0), wp.h).Intersect(wp);
                if (selR.w > 0.0f) {
                    c.DrawRect(selR, Color(56, 189, 248, isDark ? 36 : 45));
                    c.DrawLine({sx0, wp.y}, {sx0, wp.Bottom()}, Color(56, 189, 248, 220), 1.5f);
                    c.DrawLine({sx1, wp.y}, {sx1, wp.Bottom()}, Color(56, 189, 248, 220), 1.5f);
                }
            }
            if (s.playheadSec >= s.viewStartSec && s.playheadSec <= s.viewEndSec) {
                const float px = wp.x + static_cast<float>((s.playheadSec - s.viewStartSec) / viewSpan) * wp.w;
                c.DrawLine({px, wp.y}, {px, wp.Bottom()}, Color(239, 68, 68, 240), 1.5f);
            }
            c.PopClip();
        }

        // 4. Spectrogram dynamic overlays (2D Selection Box + Playhead)
        if (g.specPlotRect.w > 0.0f && g.specPlotRect.h > 2.0f) {
            const Rect sp = g.specPlotRect;
            c.PushClip(sp);
            if (s.selection.active && s.selection.DurationSec() > 1e-6) {
                const float sx0 = sp.x + static_cast<float>((s.selection.MinTimeSec() - s.viewStartSec) / viewSpan) * sp.w;
                const float sx1 = sp.x + static_cast<float>((s.selection.MaxTimeSec() - s.viewStartSec) / viewSpan) * sp.w;
                float sy0 = sp.y;
                float sy1 = sp.Bottom();
                if (s.selection.hasSpectralBox) {
                    sy0 = sp.Bottom() - FreqHzToNormalizedY(s.selection.MaxFreqHz(), s.viewMinFreqHz, s.viewMaxFreqHz, o.frequencyScale) * sp.h;
                    sy1 = sp.Bottom() - FreqHzToNormalizedY(s.selection.MinFreqHz(), s.viewMinFreqHz, s.viewMaxFreqHz, o.frequencyScale) * sp.h;
                }
                const Rect boxSel(sx0, std::min(sy0, sy1), std::max(1.0f, sx1 - sx0), std::max(1.0f, std::abs(sy1 - sy0)));
                c.DrawRoundRect(boxSel, 2.0f * scale,
                                Color(56, 189, 248, 36),
                                Color(56, 189, 248, 235),
                                1.5f);
                if (s.selection.hasSpectralBox) {
                    const float hs = 4.0f * scale;
                    const Color hc(255, 255, 255, 240);
                    c.DrawRect(Rect(boxSel.x - hs * 0.5f, boxSel.y - hs * 0.5f, hs, hs), hc);
                    c.DrawRect(Rect(boxSel.Right() - hs * 0.5f, boxSel.y - hs * 0.5f, hs, hs), hc);
                    c.DrawRect(Rect(boxSel.x - hs * 0.5f, boxSel.Bottom() - hs * 0.5f, hs, hs), hc);
                    c.DrawRect(Rect(boxSel.Right() - hs * 0.5f, boxSel.Bottom() - hs * 0.5f, hs, hs), hc);
                }
            }
            if (s.playheadSec >= s.viewStartSec && s.playheadSec <= s.viewEndSec) {
                const float px = sp.x + static_cast<float>((s.playheadSec - s.viewStartSec) / viewSpan) * sp.w;
                c.DrawLine({px, sp.y}, {px, sp.Bottom()}, Color(239, 68, 68, 240), 1.5f);
            }
            c.PopClip();
        }

        // 5. Splitter active hover highlight (only drawn when hovered or dragged)
        if (g.splitterRect.w > 0.0f && g.splitterRect.h > 0.0f) {
            const bool splitHover = (s.dragMode == DragMode::Splitter) ||
                                    (p->isHovered && g.splitterRect.Inset(-2.0f).Contains(p->mousePos));
            if (splitHover) {
                c.DrawRect(g.splitterRect, th.palette.accent.WithAlpha(65));
                c.DrawLine({g.splitterRect.x, g.splitterRect.y}, {g.splitterRect.Right(), g.splitterRect.y}, borderCol, 1.0f);
                c.DrawLine({g.splitterRect.x, g.splitterRect.Bottom()}, {g.splitterRect.Right(), g.splitterRect.Bottom()}, borderCol, 1.0f);
                const float pillW = 36.0f * scale;
                const float pillH = 2.5f * scale;
                const Rect pillRect(
                    g.splitterRect.x + (g.splitterRect.w - pillW) * 0.5f,
                    g.splitterRect.y + (g.splitterRect.h - pillH) * 0.5f,
                    pillW,
                    pillH
                );
                c.DrawRoundRect(pillRect, pillH * 0.5f, th.palette.accent);
            }
        }

        // 6. Synchronized Crosshair & Live Telemetry HUD
        if (o.showCrosshair && p->isHovered) {
            const bool inWave = g.wavePlotRect.w > 0.0f && g.wavePlotRect.h > 0.0f && g.wavePlotRect.Contains(p->mousePos);
            const bool inSpec = g.specPlotRect.w > 0.0f && g.specPlotRect.h > 0.0f && g.specPlotRect.Contains(p->mousePos);
            if (inWave || inSpec) {
                const Rect activePlot = inSpec ? g.specPlotRect : g.wavePlotRect;
                const Color crossCol(248, 250, 252, 165);

                if (g.wavePlotRect.h > 2.0f) {
                    c.DrawLine({p->mousePos.x, g.wavePlotRect.y}, {p->mousePos.x, g.wavePlotRect.Bottom()}, crossCol, 1.0f);
                }
                if (g.specPlotRect.h > 2.0f) {
                    c.DrawLine({p->mousePos.x, g.specPlotRect.y}, {p->mousePos.x, g.specPlotRect.Bottom()}, crossCol, 1.0f);
                }
                c.DrawLine({activePlot.x, p->mousePos.y}, {activePlot.Right(), p->mousePos.y}, crossCol, 1.0f);

                const float normX = std::clamp((p->mousePos.x - activePlot.x) / std::max(1.0f, activePlot.w), 0.0f, 1.0f);
                const double curT = s.viewStartSec + static_cast<double>(normX) * viewSpan;
                const size_t sampleIdx = !p->samples.empty()
                    ? std::min(p->samples.size() - 1, static_cast<size_t>(std::max(0.0, std::round(curT * sampleRate))))
                    : 0;
                const float amp = !p->samples.empty() ? p->samples[sampleIdx] : 0.0f;
                const float ampDb = 20.0f * std::log10(std::max(1e-6f, std::abs(amp)));

                char hudBuf[192]{};
                if (inSpec) {
                    const float normYTop = std::clamp((p->mousePos.y - g.specPlotRect.y) / std::max(1.0f, g.specPlotRect.h), 0.0f, 1.0f);
                    const float freqHz = NormalizedYToFreqHz(1.0f - normYTop, s.viewMinFreqHz, s.viewMaxFreqHz, o.frequencyScale);
                    const float specDb = s.specCache.SampleDbAt(normX, normYTop);
                    const std::string note = FreqToPitchNote(freqHz);
                    std::snprintf(hudBuf, sizeof(hudBuf),
                                  "t: %.4fs (#%zu)  |  f: %.0f Hz%s%s  |  P: %+.1f dB",
                                  curT, sampleIdx, freqHz,
                                  note.empty() ? "" : " (",
                                  note.empty() ? "" : (note + ")").c_str(),
                                  specDb);
                } else {
                    std::snprintf(hudBuf, sizeof(hudBuf),
                                  "t: %.4fs (#%zu)  |  Amp: %+.4f (%+.1f dBFS)  |  Gain: %.1fx",
                                  curT, sampleIdx, amp, ampDb, s.waveformGain);
                }

                const float hudFs = smallFs * 0.88f;
                const float textW = c.MeasureText(hudBuf, hudFs).x;
                const float hudW = textW + 16.0f * scale;
                const float hudH = 20.0f * scale;
                const Rect hudRect(activePlot.Right() - hudW - 8.0f * scale, activePlot.y + 6.0f * scale, hudW, hudH);
                if (hudRect.x >= activePlot.x + 4.0f) {
                    c.DrawRoundRect(hudRect, 4.0f * scale, Color(15, 23, 42, 225), Color(71, 85, 105, 200), 1.0f);
                    c.DrawTextBox(hudBuf, hudRect, hudFs, Color(241, 245, 249, 255), 0.5f, 0.5f);
                }
            }
        }

        c.PopClip();
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->pressed = baseRes.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = opts.heightMode;
    if (opts.height.has_value() && *opts.height > 0.0f &&
        !opts.label.HasContentOrReserve() && !opts.description.HasContentOrReserve()) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    }
    node->minHeight = opts.height.has_value() ? theme.Scale(*opts.height) : theme.Scale(220.0f);

    ctx.AdaptChildToContainer(node);
    if (opts.heightMode == SizeMode::Fit && node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }

    return res;
}

} // namespace guikit
