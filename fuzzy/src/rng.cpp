#include "internal.hpp"

namespace fz {

static uint64_t rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t splitmix64(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void rng_seed(fz_rng& rng, uint64_t seed) {
    uint64_t st = seed;
    for (auto& v : rng.s) v = splitmix64(st);
}

uint64_t rng_next(fz_rng& rng) {
    uint64_t* s = rng.s;
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

double rng_uniform(fz_rng& rng) {
    return static_cast<double>(rng_next(rng) >> 11) * 0x1.0p-53;
}

}

extern "C" {

fz_rng* fz_rng_create(uint64_t seed) {
    auto* r = new fz_rng;
    fz::rng_seed(*r, seed);
    return r;
}

void fz_rng_free(fz_rng* rng) {
    delete rng;
}

uint64_t fz_rng_next(fz_rng* rng) {
    return rng ? fz::rng_next(*rng) : 0;
}

double fz_rng_uniform(fz_rng* rng) {
    return rng ? fz::rng_uniform(*rng) : 0.0;
}

double fz_rng_range(fz_rng* rng, double lo, double hi) {
    return lo + (hi - lo) * fz_rng_uniform(rng);
}

}
