#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace mm::battle {

// Seeded 64-bit LCG using the publicly documented Gen-5 constants
// (x' = x * 0x5D588B656C078965 + 0x269EC3, output = high 32 bits).
// This is the same generator family Pokemon Showdown's PRNG uses, so a
// Showdown-compatible mode only has to match *call order*, not the math.
// Clean-room implementation from published constants; no source copied.
class Prng {
public:
    enum class Mode { Showdown, Cartridge };

    explicit Prng(uint64_t seed, Mode mode = Mode::Showdown)
        : state_(seed), mode_(mode) {}

    uint64_t state() const { return state_; }
    Mode mode() const { return mode_; }

    // Advance one step, return the high 32 bits.
    uint32_t next32() {
        state_ = state_ * 0x5D588B656C078965ULL + 0x269EC3ULL;
        return static_cast<uint32_t>(state_ >> 32);
    }

    // Uniform in [0, n). Matches Showdown's floor(top32 * n / 2^32).
    uint32_t next(uint32_t n) {
        return static_cast<uint32_t>((static_cast<uint64_t>(next32()) * n) >> 32);
    }

    // Uniform in [from, to).
    int range(int from, int to) {
        return from + static_cast<int>(next(static_cast<uint32_t>(to - from)));
    }

    // True with probability num/den.
    bool chance(uint32_t num, uint32_t den) { return next(den) < num; }

    // Damage roll multiplier, 85..100 inclusive.
    int damageRoll() { return 85 + static_cast<int>(next(16)); }

private:
    uint64_t state_;
    Mode mode_;
};

} // namespace mm::battle
