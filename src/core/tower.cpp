#include "tower.hpp"

#include <algorithm>

#include "rng.hpp"

namespace tj {

namespace {

template <typename Band>
const Band* bandFor(const std::vector<Band>& bands, int index) {
    for (const Band& b : bands)
        if (index >= b.fromIndex && index <= b.toIndex) return &b;
    return bands.empty() ? nullptr : &bands.back();
}

int pickType(const TypeBand* band, const GameplayConfig& g, float roll, int fallback) {
    if (!band) return fallback;
    float total = 0;
    for (const TypeWeight& w : band->weights) total += std::max(0.0f, w.weight);
    if (total <= 0) return fallback;
    float target = roll * total;
    for (const TypeWeight& w : band->weights) {
        target -= std::max(0.0f, w.weight);
        if (target < 0) {
            int t = g.findType(w.typeId);
            return t >= 0 ? t : fallback;
        }
    }
    return fallback;
}

}  // namespace

std::vector<PlatformSpec> generateTower(const TowerConfig& t, const GameplayConfig& g) {
    std::vector<PlatformSpec> out;
    out.reserve(static_cast<size_t>(std::max(0, t.totalPlatformCount)));
    Rng rng(t.seed);
    const int normal = std::max(0, g.findType("normal"));
    float y = t.startHeight;

    for (int i = 1; i <= t.totalPlatformCount; ++i) {
        // Always draw the same four samples per platform, even for landmark floors, so that
        // changing fullWidthInterval never re-rolls the rest of the tower.
        const float gapRoll = rng.next01();
        const float widthRoll = rng.next01();
        const float posRoll = rng.next01();
        const float typeRoll = rng.next01();

        if (i > 1) {
            const GapBand* gb = bandFor(t.gaps, i);
            y += gb ? gb->minGap + (gb->maxGap - gb->minGap) * gapRoll : 1.5f;
        }

        PlatformSpec p;
        p.index = i;
        p.y = y;
        p.landmark = t.fullWidthInterval > 0 && (i - 1) % t.fullWidthInterval == 0;

        if (p.landmark) {
            p.width = t.towerHalfWidth * 2.0f;
            p.x = 0.0f;
            p.typeIndex = normal;
        } else {
            const WidthBand* wb = bandFor(t.widths, i);
            p.width = wb ? wb->minWidth + (wb->maxWidth - wb->minWidth) * widthRoll : 3.0f;
            const float maxOffset = std::max(0.0f, t.towerHalfWidth - p.width * 0.5f);
            p.x = -maxOffset + 2.0f * maxOffset * posRoll;
            p.typeIndex = pickType(bandFor(t.types, i), g, typeRoll, normal);
        }
        out.push_back(p);
    }
    return out;
}

}  // namespace tj
