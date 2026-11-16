#include "internal.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace {

double dist(double a1, double b1, double a2, double b2) {
    return std::max(std::fabs(a1 - a2), std::fabs(b1 - b2));
}

}

extern "C" {

double fz_interval_distance(double a1, double b1, double a2, double b2) {
    return dist(a1, b1, a2, b2);
}

int fz_cluster_stats(const double* left, const double* right, int n, const int* labels, int k,
                     double* center_left, double* center_right, int* counts, double* spread) {
    std::vector<double> sl(k, 0.0), sr(k, 0.0);
    std::vector<int> cnt(k, 0);
    for (int i = 0; i < n; ++i) {
        int c = labels[i];
        if (c < 0 || c >= k) continue;
        sl[c] += left[i];
        sr[c] += right[i];
        ++cnt[c];
    }
    std::vector<double> sp(k, 0.0);
    for (int c = 0; c < k; ++c) {
        if (cnt[c]) {
            sl[c] /= cnt[c];
            sr[c] /= cnt[c];
        }
    }
    for (int i = 0; i < n; ++i) {
        int c = labels[i];
        if (c < 0 || c >= k) continue;
        double d = dist(left[i], right[i], sl[c], sr[c]);
        sp[c] += d * d;
    }
    for (int c = 0; c < k; ++c) {
        if (center_left) center_left[c] = sl[c];
        if (center_right) center_right[c] = sr[c];
        if (counts) counts[c] = cnt[c];
        if (spread) spread[c] = cnt[c] ? std::sqrt(sp[c] / cnt[c]) : 0.0;
    }
    return k;
}

int fz_kmeans(const double* left, const double* right, int n, int k, uint64_t seed, int max_iter, int* labels,
              double* center_left, double* center_right, double* spread) {
    fz::clear_error();
    if (n <= 0 || k <= 0 || k > n) {
        fz::set_error("k-means requires 1 <= k <= n");
        return -1;
    }
    if (max_iter <= 0) max_iter = 100;
    fz_rng rng;
    fz::rng_seed(rng, seed);
    std::vector<double> cl(k), cr(k), d2(n, 0.0);
    int first = static_cast<int>(fz::rng_uniform(rng) * n);
    if (first >= n) first = n - 1;
    cl[0] = left[first];
    cr[0] = right[first];
    for (int i = 0; i < n; ++i) {
        double d = dist(left[i], right[i], cl[0], cr[0]);
        d2[i] = d * d;
    }
    for (int c = 1; c < k; ++c) {
        double total = std::accumulate(d2.begin(), d2.end(), 0.0);
        int pick = n - 1;
        if (total > 0) {
            double target = fz::rng_uniform(rng) * total;
            double acc = 0.0;
            for (int i = 0; i < n; ++i) {
                acc += d2[i];
                if (acc >= target) {
                    pick = i;
                    break;
                }
            }
        } else {
            pick = static_cast<int>(fz::rng_uniform(rng) * n);
            if (pick >= n) pick = n - 1;
        }
        cl[c] = left[pick];
        cr[c] = right[pick];
        for (int i = 0; i < n; ++i) {
            double d = dist(left[i], right[i], cl[c], cr[c]);
            d2[i] = std::min(d2[i], d * d);
        }
    }
    std::fill(labels, labels + n, -1);
    int iter = 0;
    for (; iter < max_iter; ++iter) {
        bool changed = false;
        for (int i = 0; i < n; ++i) {
            int best = 0;
            double bd = dist(left[i], right[i], cl[0], cr[0]);
            for (int c = 1; c < k; ++c) {
                double d = dist(left[i], right[i], cl[c], cr[c]);
                if (d < bd) {
                    bd = d;
                    best = c;
                }
            }
            if (labels[i] != best) {
                labels[i] = best;
                changed = true;
            }
        }
        std::vector<double> sl(k, 0.0), sr(k, 0.0);
        std::vector<int> cnt(k, 0);
        for (int i = 0; i < n; ++i) {
            sl[labels[i]] += left[i];
            sr[labels[i]] += right[i];
            ++cnt[labels[i]];
        }
        for (int c = 0; c < k; ++c) {
            if (cnt[c]) {
                cl[c] = sl[c] / cnt[c];
                cr[c] = sr[c] / cnt[c];
                continue;
            }
            int far = 0;
            double fd = -1.0;
            for (int i = 0; i < n; ++i) {
                double d = dist(left[i], right[i], cl[labels[i]], cr[labels[i]]);
                if (d > fd) {
                    fd = d;
                    far = i;
                }
            }
            cl[c] = left[far];
            cr[c] = right[far];
            labels[far] = c;
            changed = true;
        }
        if (!changed) break;
    }
    fz_cluster_stats(left, right, n, labels, k, center_left, center_right, nullptr, spread);
    fz::logf("k-means n=%d k=%d seed=%llu iterations=%d", n, k, static_cast<unsigned long long>(seed), iter + 1);
    return iter + 1;
}

int fz_threshold_cluster(const double* left, const double* right, int n, double delta, int* labels) {
    fz::clear_error();
    if (n <= 0 || !(delta >= 0)) {
        fz::set_error("threshold clustering requires n > 0 and delta >= 0");
        return -1;
    }
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int p, int q) {
        return left[p] < left[q] || (left[p] == left[q] && right[p] < right[q]);
    });
    std::vector<double> cl, cr, sl, sr;
    std::vector<int> cnt;
    auto nearest = [&](int i, double& best) {
        int bc = -1;
        best = 0.0;
        for (size_t c = 0; c < cl.size(); ++c) {
            double d = dist(left[i], right[i], cl[c], cr[c]);
            if (bc < 0 || d < best) {
                best = d;
                bc = static_cast<int>(c);
            }
        }
        return bc;
    };
    auto open_cluster = [&](int i) {
        cl.push_back(left[i]);
        cr.push_back(right[i]);
        sl.push_back(0.0);
        sr.push_back(0.0);
        cnt.push_back(0);
        return static_cast<int>(cl.size()) - 1;
    };
    for (int i : order) {
        double d;
        int c = nearest(i, d);
        if (c < 0 || d > delta) c = open_cluster(i);
        labels[i] = c;
        sl[c] += left[i];
        sr[c] += right[i];
        ++cnt[c];
        cl[c] = sl[c] / cnt[c];
        cr[c] = sr[c] / cnt[c];
    }
    int passes = 0;
    for (; passes < 50; ++passes) {
        bool changed = false;
        for (int i : order) {
            double d;
            int c = nearest(i, d);
            if (d > delta) {
                c = open_cluster(i);
            }
            if (c != labels[i]) {
                labels[i] = c;
                changed = true;
            }
        }
        std::fill(sl.begin(), sl.end(), 0.0);
        std::fill(sr.begin(), sr.end(), 0.0);
        std::fill(cnt.begin(), cnt.end(), 0);
        for (int i = 0; i < n; ++i) {
            sl[labels[i]] += left[i];
            sr[labels[i]] += right[i];
            ++cnt[labels[i]];
        }
        for (size_t c = 0; c < cl.size(); ++c) {
            if (cnt[c]) {
                cl[c] = sl[c] / cnt[c];
                cr[c] = sr[c] / cnt[c];
            }
        }
        if (!changed) break;
    }
    std::vector<int> remap(cl.size(), -1);
    int k = 0;
    for (int i : order) {
        if (remap[labels[i]] < 0) remap[labels[i]] = k++;
    }
    for (int i = 0; i < n; ++i) labels[i] = remap[labels[i]];
    fz::logf("threshold clustering n=%d delta=%.4f passes=%d -> %d clusters", n, delta, passes + 1, k);
    return k;
}

}
