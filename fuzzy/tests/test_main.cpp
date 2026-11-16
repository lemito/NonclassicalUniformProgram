#include "fuzzy.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_total;                                                           \
        if (!(cond)) {                                                       \
            ++g_failed;                                                      \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                    \
    } while (0)

static bool near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

static void test_alpha_cut() {
    fz_number* x = fz_create(0.0, 10.0, 21);
    double l, r;
    fz_alpha_cut(x, 0.7, &l, &r);
    CHECK(near(l, 7.0));
    CHECK(near(r, 10.0));
    CHECK(near(fz_membership(x, 5.0), 0.5));
    CHECK(near(fz_membership(x, -1.0), 0.0));
    CHECK(near(fz_membership(x, 11.0), 0.0));
    CHECK(near(fz_centroid(x), 20.0 / 3.0));
    CHECK(fz_create(5.0, 5.0, 21) == nullptr);
    fz_free(x);
}

static void test_arithmetic() {
    fz_number* x = fz_create(1.0, 3.0, 11);
    fz_number* y = fz_create(2.0, 6.0, 11);
    fz_number* s = fz_add(x, y);
    double l, r;
    fz_alpha_cut(s, 0.0, &l, &r);
    CHECK(near(l, 3.0) && near(r, 9.0));
    fz_alpha_cut(s, 0.5, &l, &r);
    CHECK(near(l, 6.0) && near(r, 9.0));
    fz_number* d = fz_sub(y, x);
    fz_alpha_cut(d, 0.0, &l, &r);
    CHECK(near(l, -1.0) && near(r, 5.0));
    fz_number* m = fz_mul(x, y);
    fz_alpha_cut(m, 0.0, &l, &r);
    CHECK(near(l, 2.0) && near(r, 18.0));
    fz_number* q = fz_div(y, x);
    fz_alpha_cut(q, 0.0, &l, &r);
    CHECK(near(l, 2.0 / 3.0) && near(r, 6.0));
    CHECK(near(fz_membership(s, 9.0), 1.0));
    CHECK(near(fz_membership(s, 4.5), 0.25, 1e-6));

    fz_number* z = fz_create(-1.0, 1.0, 11);
    CHECK(fz_div(x, z) == nullptr);
    CHECK(fz_last_error()[0] != '\0');

    double deg;
    CHECK(fz_compare(x, y, &deg) == -1);
    CHECK(fz_compare(y, x, &deg) == 1);
    CHECK(near(deg, 1.0));
    fz_number* far = fz_create(10.0, 12.0, 11);
    fz_compare(x, far, &deg);
    CHECK(near(deg, 0.0));

    fz_free(x);
    fz_free(y);
    fz_free(s);
    fz_free(d);
    fz_free(m);
    fz_free(q);
    fz_free(z);
    fz_free(far);
}

static void test_rng() {
    fz_rng* r = fz_rng_create(42);
    double sum = 0.0;
    const int n = 100000;
    for (int i = 0; i < n; ++i) {
        double u = fz_rng_uniform(r);
        CHECK(u >= 0.0 && u < 1.0);
        sum += u;
    }
    CHECK(std::fabs(sum / n - 0.5) < 0.01);
    fz_rng_free(r);
    fz_rng* a = fz_rng_create(7);
    fz_rng* b = fz_rng_create(7);
    CHECK(fz_rng_next(a) == fz_rng_next(b));
    fz_rng_free(a);
    fz_rng_free(b);
}

static void test_generation_and_selection() {
    fz_set* s = fz_set_generate(1, 300, 0.0, 100.0, 5.0, 20.0, 21);
    CHECK(fz_set_size(s) == 300);
    std::vector<double> L(300), R(300);
    fz_set_intervals(s, 0.7, L.data(), R.data());
    for (int i = 0; i < 300; ++i) {
        double a, b;
        fz_set_get(s, i, &a, &b);
        CHECK(a >= 0.0 && a <= 100.0 && b - a >= 5.0 && b - a <= 20.0);
        CHECK(near(L[i], a + 0.7 * (b - a)) && near(R[i], b));
    }
    std::vector<int> idx(300);
    int c = fz_select_width(L.data(), R.data(), 300, 3.0, 4.0, idx.data());
    for (int i = 0; i < c; ++i) CHECK(R[idx[i]] - L[idx[i]] >= 3.0 && R[idx[i]] - L[idx[i]] <= 4.0);
    c = fz_select_window(L.data(), R.data(), 300, 20.0, 60.0, idx.data());
    for (int i = 0; i < c; ++i) CHECK(L[idx[i]] >= 20.0 && R[idx[i]] <= 60.0);
    fz_set_free(s);
}

static void test_clustering() {
    std::vector<double> L, R;
    fz_rng* r = fz_rng_create(3);
    const double centers[3] = {0.0, 50.0, 100.0};
    for (double c : centers) {
        for (int i = 0; i < 30; ++i) {
            double a = c + fz_rng_range(r, -2.0, 2.0);
            L.push_back(a);
            R.push_back(a + 3.0 + fz_rng_range(r, -1.0, 1.0));
        }
    }
    fz_rng_free(r);
    const int n = static_cast<int>(L.size());
    std::vector<int> labels(n);
    std::vector<double> cl(3), cr(3), sp(3);
    int it = fz_kmeans(L.data(), R.data(), n, 3, 11, 100, labels.data(), cl.data(), cr.data(), sp.data());
    CHECK(it > 0);
    for (int g = 0; g < 3; ++g)
        for (int i = 1; i < 30; ++i) CHECK(labels[g * 30 + i] == labels[g * 30]);
    CHECK(labels[0] != labels[30] && labels[30] != labels[60] && labels[0] != labels[60]);

    std::vector<int> tl(n);
    int k = fz_threshold_cluster(L.data(), R.data(), n, 10.0, tl.data());
    CHECK(k == 3);
    for (int g = 0; g < 3; ++g)
        for (int i = 1; i < 30; ++i) CHECK(tl[g * 30 + i] == tl[g * 30]);
    CHECK(fz_threshold_cluster(L.data(), R.data(), n, 1000.0, tl.data()) == 1);
    CHECK(fz_kmeans(L.data(), R.data(), n, n + 1, 1, 10, labels.data(), nullptr, nullptr, nullptr) == -1);
}

int main() {
    test_alpha_cut();
    test_arithmetic();
    test_rng();
    test_generation_and_selection();
    test_clustering();
    std::printf("%d/%d checks passed\n", g_total - g_failed, g_total);
    return g_failed ? 1 : 0;
}
