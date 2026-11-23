#pragma once
// Deterministic PCG32 random generator. Same seed => same sequence on every platform and compiler,
// which std::uniform_real_distribution does not guarantee. Tower layouts depend on this.

#include <cstdint>

namespace tj {

class Rng {
public:
    explicit Rng(uint64_t seed, uint64_t stream = 54u) { reseed(seed, stream); }

    void reseed(uint64_t seed, uint64_t stream = 54u) {
        state_ = 0u;
        inc_ = (stream << 1u) | 1u;
        nextU32();
        state_ += seed;
        nextU32();
    }

    uint32_t nextU32() {
        uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Uniform in [0, 1).
    float next01() { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }

    float range(float lo, float hi) { return lo + (hi - lo) * next01(); }

private:
    uint64_t state_ = 0;
    uint64_t inc_ = 0;
};

}  // namespace tj
