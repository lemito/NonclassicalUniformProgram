#ifndef FUZZY_INTERNAL_HPP
#define FUZZY_INTERNAL_HPP

#include "fuzzy.h"

#include <string>
#include <vector>

struct fz_rng {
    uint64_t s[4];
};

struct fz_number {
    bool uniform = false;
    double a = 0.0;
    double b = 0.0;
    std::vector<double> left;
    std::vector<double> right;
};

struct fz_set {
    int levels = 21;
    std::vector<fz_number> items;
};

namespace fz {

void set_error(const std::string& message);
void clear_error();
void logf(const char* fmt, ...);
std::string describe(const fz_number& x);

void rng_seed(fz_rng& rng, uint64_t seed);
uint64_t rng_next(fz_rng& rng);
double rng_uniform(fz_rng& rng);

fz_number make_uniform(double a, double b, int levels);
double alpha_of(int i, int levels);
void cut_at(const fz_number& x, double alpha, double& l, double& r);

}

#endif
