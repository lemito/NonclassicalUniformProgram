#include "internal.hpp"

#include <cmath>

extern "C" {

fz_set* fz_set_create(int levels) {
    fz::clear_error();
    if (levels < 2 || levels > 10001) {
        fz::set_error("levels must be in [2, 10001]");
        return nullptr;
    }
    auto* s = new fz_set;
    s->levels = levels;
    return s;
}

fz_set* fz_set_generate(uint64_t seed, int n, double lo, double hi, double wmin, double wmax, int levels) {
    fz::clear_error();
    if (n <= 0 || n > 100000) {
        fz::set_error("n must be in [1, 100000]");
        return nullptr;
    }
    if (!(lo <= hi) || !(wmin > 0) || !(wmin <= wmax)) {
        fz::set_error("invalid generation ranges: need lo <= hi and 0 < wmin <= wmax");
        return nullptr;
    }
    fz_set* s = fz_set_create(levels);
    if (!s) return nullptr;
    fz_rng rng;
    fz::rng_seed(rng, seed);
    s->items.reserve(n);
    for (int i = 0; i < n; ++i) {
        double a = lo + (hi - lo) * fz::rng_uniform(rng);
        double w = wmin + (wmax - wmin) * fz::rng_uniform(rng);
        s->items.push_back(fz::make_uniform(a, a + w, levels));
    }
    fz::logf("generate n=%d seed=%llu a~U[%.4f, %.4f] width~U[%.4f, %.4f] levels=%d", n,
             static_cast<unsigned long long>(seed), lo, hi, wmin, wmax, levels);
    return s;
}

void fz_set_free(fz_set* s) {
    delete s;
}

int fz_set_add(fz_set* s, double a, double b) {
    fz::clear_error();
    if (!s) return -1;
    if (!std::isfinite(a) || !std::isfinite(b) || !(a < b)) {
        fz::set_error("uniform parameters must satisfy a < b");
        return -1;
    }
    s->items.push_back(fz::make_uniform(a, b, s->levels));
    return static_cast<int>(s->items.size()) - 1;
}

int fz_set_size(const fz_set* s) {
    return s ? static_cast<int>(s->items.size()) : 0;
}

int fz_set_get(const fz_set* s, int i, double* a, double* b) {
    if (!s || i < 0 || i >= static_cast<int>(s->items.size())) return 0;
    if (a) *a = s->items[i].a;
    if (b) *b = s->items[i].b;
    return 1;
}

const fz_number* fz_set_item(const fz_set* s, int i) {
    if (!s || i < 0 || i >= static_cast<int>(s->items.size())) return nullptr;
    return &s->items[i];
}

int fz_set_intervals(const fz_set* s, double alpha, double* left, double* right) {
    if (!s) return 0;
    const int n = static_cast<int>(s->items.size());
    for (int i = 0; i < n; ++i) fz::cut_at(s->items[i], alpha, left[i], right[i]);
    fz::logf("alpha-cut alpha=%.4f for %d numbers", alpha, n);
    return n;
}

}
