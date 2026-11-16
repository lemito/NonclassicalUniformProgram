#include "internal.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace {

std::mutex g_log_mutex;
FILE* g_log = nullptr;
thread_local std::string g_error;

}

namespace fz {

void set_error(const std::string& message) {
    g_error = message;
    logf("ERROR %s", message.c_str());
}

void clear_error() {
    g_error.clear();
}

void logf(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log) return;
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&now, &tmv);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);
    std::fprintf(g_log, "[%s] ", stamp);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_log, fmt, args);
    va_end(args);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

std::string describe(const fz_number& x) {
    char buf[160];
    if (x.uniform) {
        std::snprintf(buf, sizeof(buf), "U(%.4f, %.4f)", x.a, x.b);
    } else {
        std::snprintf(buf, sizeof(buf), "F{supp=[%.4f, %.4f], core=[%.4f, %.4f]}",
                      x.left.front(), x.right.front(), x.left.back(), x.right.back());
    }
    return buf;
}

}

extern "C" {

const char* fz_last_error(void) {
    return g_error.c_str();
}

const char* fz_version(void) {
    return "1.0.0";
}

int fz_log_open(const char* path) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log) std::fclose(g_log);
    g_log = std::fopen(path, "a");
    return g_log ? 1 : 0;
}

void fz_log_close(void) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log) std::fclose(g_log);
    g_log = nullptr;
}

void fz_log_message(const char* message) {
    fz::logf("%s", message ? message : "");
}

}
