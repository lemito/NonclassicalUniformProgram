#ifndef FUZZY_H
#define FUZZY_H

#include <stdint.h>

#if defined(_WIN32) && defined(FUZZY_BUILD_SHARED)
#define FZ_API __declspec(dllexport)
#elif defined(__GNUC__)
#define FZ_API __attribute__((visibility("default")))
#else
#define FZ_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fz_number fz_number;
typedef struct fz_set fz_set;
typedef struct fz_rng fz_rng;

enum fz_op {
    FZ_OP_ADD = 0,
    FZ_OP_SUB = 1,
    FZ_OP_MUL = 2,
    FZ_OP_DIV = 3
};

FZ_API const char* fz_version(void);
FZ_API const char* fz_last_error(void);

FZ_API fz_rng* fz_rng_create(uint64_t seed);
FZ_API void fz_rng_free(fz_rng* rng);
FZ_API uint64_t fz_rng_next(fz_rng* rng);
FZ_API double fz_rng_uniform(fz_rng* rng);
FZ_API double fz_rng_range(fz_rng* rng, double lo, double hi);

FZ_API fz_number* fz_create(double a, double b, int levels);
FZ_API fz_number* fz_create_from_cuts(const double* left, const double* right, int levels);
FZ_API fz_number* fz_clone(const fz_number* x);
FZ_API void fz_free(fz_number* x);
FZ_API int fz_levels(const fz_number* x);
FZ_API int fz_is_uniform(const fz_number* x, double* a, double* b);
FZ_API int fz_support(const fz_number* x, double* lo, double* hi);
FZ_API int fz_alpha_cut(const fz_number* x, double alpha, double* left, double* right);
FZ_API int fz_cuts(const fz_number* x, double* alphas, double* left, double* right);
FZ_API double fz_membership(const fz_number* x, double v);
FZ_API double fz_centroid(const fz_number* x);

FZ_API fz_number* fz_add(const fz_number* x, const fz_number* y);
FZ_API fz_number* fz_sub(const fz_number* x, const fz_number* y);
FZ_API fz_number* fz_mul(const fz_number* x, const fz_number* y);
FZ_API fz_number* fz_div(const fz_number* x, const fz_number* y);
FZ_API fz_number* fz_apply(int op, const fz_number* x, const fz_number* y);
FZ_API int fz_compare(const fz_number* x, const fz_number* y, double* degree);

FZ_API fz_set* fz_set_create(int levels);
FZ_API fz_set* fz_set_generate(uint64_t seed, int n, double lo, double hi, double wmin, double wmax, int levels);
FZ_API void fz_set_free(fz_set* s);
FZ_API int fz_set_add(fz_set* s, double a, double b);
FZ_API int fz_set_size(const fz_set* s);
FZ_API int fz_set_get(const fz_set* s, int i, double* a, double* b);
FZ_API const fz_number* fz_set_item(const fz_set* s, int i);
FZ_API int fz_set_intervals(const fz_set* s, double alpha, double* left, double* right);

FZ_API double fz_interval_distance(double a1, double b1, double a2, double b2);
FZ_API int fz_select_width(const double* left, const double* right, int n, double wmin, double wmax, int* out_idx);
FZ_API int fz_select_window(const double* left, const double* right, int n, double x1, double x2, int* out_idx);
FZ_API int fz_kmeans(const double* left, const double* right, int n, int k, uint64_t seed, int max_iter,
                     int* labels, double* center_left, double* center_right, double* spread);
FZ_API int fz_threshold_cluster(const double* left, const double* right, int n, double delta, int* labels);
FZ_API int fz_cluster_stats(const double* left, const double* right, int n, const int* labels, int k,
                            double* center_left, double* center_right, int* counts, double* spread);

FZ_API int fz_log_open(const char* path);
FZ_API void fz_log_close(void);
FZ_API void fz_log_message(const char* message);

#ifdef __cplusplus
}
#endif

#endif
