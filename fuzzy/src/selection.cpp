#include "internal.hpp"

extern "C" {

int fz_select_width(const double* left, const double* right, int n, double wmin, double wmax, int* out_idx) {
    int c = 0;
    for (int i = 0; i < n; ++i) {
        double w = right[i] - left[i];
        if (w >= wmin && w <= wmax) out_idx[c++] = i;
    }
    fz::logf("select width in [%.4f, %.4f]: %d of %d", wmin, wmax, c, n);
    return c;
}

int fz_select_window(const double* left, const double* right, int n, double x1, double x2, int* out_idx) {
    int c = 0;
    for (int i = 0; i < n; ++i) {
        if (left[i] >= x1 && right[i] <= x2) out_idx[c++] = i;
    }
    fz::logf("select inside window [%.4f, %.4f]: %d of %d", x1, x2, c, n);
    return c;
}

}
