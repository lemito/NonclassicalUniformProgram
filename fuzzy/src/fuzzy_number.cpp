#include "internal.hpp"

#include <algorithm>
#include <cmath>

namespace fz {

double alpha_of(int i, int levels) {
    return levels > 1 ? static_cast<double>(i) / (levels - 1) : 1.0;
}

fz_number make_uniform(double a, double b, int levels) {
    fz_number x;
    x.uniform = true;
    x.a = a;
    x.b = b;
    x.left.resize(levels);
    x.right.resize(levels);
    for (int i = 0; i < levels; ++i) {
        x.left[i] = a + alpha_of(i, levels) * (b - a);
        x.right[i] = b;
    }
    return x;
}

void cut_at(const fz_number& x, double alpha, double& l, double& r) {
    alpha = std::clamp(alpha, 0.0, 1.0);
    if (x.uniform) {
        l = x.a + alpha * (x.b - x.a);
        r = x.b;
        return;
    }
    const int m = static_cast<int>(x.left.size());
    if (m == 1) {
        l = x.left[0];
        r = x.right[0];
        return;
    }
    double pos = alpha * (m - 1);
    int i = std::min(static_cast<int>(pos), m - 2);
    double t = pos - i;
    l = x.left[i] + t * (x.left[i + 1] - x.left[i]);
    r = x.right[i] + t * (x.right[i + 1] - x.right[i]);
}

static bool valid_levels(int levels) {
    if (levels < 2 || levels > 10001) {
        set_error("levels must be in [2, 10001]");
        return false;
    }
    return true;
}

static fz_number combine(int op, const fz_number& x, const fz_number& y) {
    const int m = std::max(static_cast<int>(x.left.size()), static_cast<int>(y.left.size()));
    fz_number z;
    z.left.resize(m);
    z.right.resize(m);
    for (int i = 0; i < m; ++i) {
        double alpha = alpha_of(i, m);
        double xl, xr, yl, yr;
        cut_at(x, alpha, xl, xr);
        cut_at(y, alpha, yl, yr);
        double l = 0.0, r = 0.0;
        switch (op) {
        case FZ_OP_ADD:
            l = xl + yl;
            r = xr + yr;
            break;
        case FZ_OP_SUB:
            l = xl - yr;
            r = xr - yl;
            break;
        case FZ_OP_MUL:
        case FZ_OP_DIV: {
            double cl = yl, cr = yr;
            if (op == FZ_OP_DIV) {
                cl = 1.0 / yr;
                cr = 1.0 / yl;
            }
            double p[4] = {xl * cl, xl * cr, xr * cl, xr * cr};
            l = *std::min_element(p, p + 4);
            r = *std::max_element(p, p + 4);
            break;
        }
        }
        z.left[i] = l;
        z.right[i] = r;
    }
    for (int i = 1; i < m; ++i) {
        z.left[i] = std::max(z.left[i], z.left[i - 1]);
        z.right[i] = std::min(z.right[i], z.right[i - 1]);
    }
    return z;
}

static const char* op_name(int op) {
    switch (op) {
    case FZ_OP_ADD: return "+";
    case FZ_OP_SUB: return "-";
    case FZ_OP_MUL: return "*";
    case FZ_OP_DIV: return "/";
    }
    return "?";
}

}

extern "C" {

fz_number* fz_create(double a, double b, int levels) {
    fz::clear_error();
    if (!fz::valid_levels(levels)) return nullptr;
    if (!std::isfinite(a) || !std::isfinite(b) || !(a < b)) {
        fz::set_error("uniform parameters must satisfy a < b");
        return nullptr;
    }
    auto* x = new fz_number(fz::make_uniform(a, b, levels));
    fz::logf("create %s levels=%d", fz::describe(*x).c_str(), levels);
    return x;
}

fz_number* fz_create_from_cuts(const double* left, const double* right, int levels) {
    fz::clear_error();
    if (!fz::valid_levels(levels)) return nullptr;
    if (!left || !right) {
        fz::set_error("null cut arrays");
        return nullptr;
    }
    for (int i = 0; i < levels; ++i) {
        if (!(left[i] <= right[i]) || (i > 0 && (left[i] < left[i - 1] || right[i] > right[i - 1]))) {
            fz::set_error("cuts must be nested intervals");
            return nullptr;
        }
    }
    auto* x = new fz_number;
    x->left.assign(left, left + levels);
    x->right.assign(right, right + levels);
    fz::logf("create %s levels=%d", fz::describe(*x).c_str(), levels);
    return x;
}

fz_number* fz_clone(const fz_number* x) {
    return x ? new fz_number(*x) : nullptr;
}

void fz_free(fz_number* x) {
    delete x;
}

int fz_levels(const fz_number* x) {
    return x ? static_cast<int>(x->left.size()) : 0;
}

int fz_is_uniform(const fz_number* x, double* a, double* b) {
    if (!x || !x->uniform) return 0;
    if (a) *a = x->a;
    if (b) *b = x->b;
    return 1;
}

int fz_support(const fz_number* x, double* lo, double* hi) {
    if (!x) return 0;
    if (lo) *lo = x->left.front();
    if (hi) *hi = x->right.front();
    return 1;
}

int fz_alpha_cut(const fz_number* x, double alpha, double* left, double* right) {
    if (!x) return 0;
    double l, r;
    fz::cut_at(*x, alpha, l, r);
    if (left) *left = l;
    if (right) *right = r;
    return 1;
}

int fz_cuts(const fz_number* x, double* alphas, double* left, double* right) {
    if (!x) return 0;
    const int m = static_cast<int>(x->left.size());
    for (int i = 0; i < m; ++i) {
        if (alphas) alphas[i] = fz::alpha_of(i, m);
        if (left) left[i] = x->left[i];
        if (right) right[i] = x->right[i];
    }
    return m;
}

double fz_membership(const fz_number* x, double v) {
    if (!x) return 0.0;
    if (x->uniform) {
        if (v < x->a || v > x->b) return 0.0;
        return (v - x->a) / (x->b - x->a);
    }
    const auto& L = x->left;
    const auto& R = x->right;
    const int m = static_cast<int>(L.size());
    if (v < L[0] || v > R[0]) return 0.0;
    for (int i = 0; i + 1 < m; ++i) {
        if (v >= L[i + 1] && v <= R[i + 1]) continue;
        double a0 = fz::alpha_of(i, m);
        double a1 = fz::alpha_of(i + 1, m);
        if (v < L[i + 1]) {
            double d = L[i + 1] - L[i];
            return d > 0 ? a0 + (a1 - a0) * (v - L[i]) / d : a0;
        }
        double d = R[i] - R[i + 1];
        return d > 0 ? a0 + (a1 - a0) * (R[i] - v) / d : a0;
    }
    return 1.0;
}

double fz_centroid(const fz_number* x) {
    if (!x) return 0.0;
    if (x->uniform) return x->a + 2.0 * (x->b - x->a) / 3.0;
    double lo = x->left.front(), hi = x->right.front();
    if (hi - lo <= 0) return lo;
    const int steps = 2000;
    double h = (hi - lo) / steps;
    double num = 0.0, den = 0.0;
    for (int i = 0; i <= steps; ++i) {
        double v = lo + i * h;
        double w = (i == 0 || i == steps) ? 0.5 : 1.0;
        double mu = fz_membership(x, v) * w;
        num += v * mu;
        den += mu;
    }
    return den > 0 ? num / den : 0.5 * (lo + hi);
}

fz_number* fz_apply(int op, const fz_number* x, const fz_number* y) {
    fz::clear_error();
    if (!x || !y) {
        fz::set_error("null operand");
        return nullptr;
    }
    if (op < FZ_OP_ADD || op > FZ_OP_DIV) {
        fz::set_error("unknown operation");
        return nullptr;
    }
    if (op == FZ_OP_DIV && y->left.front() <= 0.0 && y->right.front() >= 0.0) {
        fz::set_error("division by fuzzy number whose support contains zero");
        return nullptr;
    }
    auto* z = new fz_number(fz::combine(op, *x, *y));
    fz::logf("op %s %s %s = %s", fz::describe(*x).c_str(), fz::op_name(op), fz::describe(*y).c_str(),
             fz::describe(*z).c_str());
    return z;
}

fz_number* fz_add(const fz_number* x, const fz_number* y) {
    return fz_apply(FZ_OP_ADD, x, y);
}

fz_number* fz_sub(const fz_number* x, const fz_number* y) {
    return fz_apply(FZ_OP_SUB, x, y);
}

fz_number* fz_mul(const fz_number* x, const fz_number* y) {
    return fz_apply(FZ_OP_MUL, x, y);
}

fz_number* fz_div(const fz_number* x, const fz_number* y) {
    return fz_apply(FZ_OP_DIV, x, y);
}

int fz_compare(const fz_number* x, const fz_number* y, double* degree) {
    if (!x || !y) return 0;
    const int m = std::max(fz_levels(x), fz_levels(y));
    double poss = 0.0;
    for (int i = 0; i < m; ++i) {
        double alpha = fz::alpha_of(i, m);
        double xl, xr, yl, yr;
        fz::cut_at(*x, alpha, xl, xr);
        fz::cut_at(*y, alpha, yl, yr);
        if (xr >= yl) poss = alpha;
    }
    double cx = fz_centroid(x), cy = fz_centroid(y);
    double eps = 1e-9 * std::max({1.0, std::fabs(cx), std::fabs(cy)});
    int sign = std::fabs(cx - cy) <= eps ? 0 : (cx < cy ? -1 : 1);
    if (degree) *degree = poss;
    fz::logf("compare %s vs %s -> %d, Poss(X>=Y)=%.4f", fz::describe(*x).c_str(), fz::describe(*y).c_str(), sign,
             poss);
    return sign;
}

}
