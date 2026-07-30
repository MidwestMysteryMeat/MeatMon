#pragma once

// Pure stat math (published formulas, integer arithmetic throughout).
// Header-only and side-effect free so tests can pin exact values.

namespace mm::battle::calc {

inline int hpStat(int base, int iv, int ev, int level) {
    return (2 * base + iv + ev / 4) * level / 100 + level + 10;
}

// natureNum: 110 boosted, 90 hindered, 100 neutral.
inline int otherStat(int base, int iv, int ev, int level, int natureNum) {
    int s = (2 * base + iv + ev / 4) * level / 100 + 5;
    return s * natureNum / 100;
}

// Stat stage multiplier: stage in [-6, +6]; +1 = 3/2, -1 = 2/3, etc.
inline int applyStage(int stat, int stage) {
    if (stage >= 0) return stat * (2 + stage) / 2;
    return stat * 2 / (2 - stage);
}

} // namespace mm::battle::calc
