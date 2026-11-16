#include "server_process.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

int FreePort() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int port = 0;
    socklen_t len = sizeof(addr);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
        getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        port = ntohs(addr.sin_port);
    }
    close(fd);
    return port;
}

bool CanConnect(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    bool ok = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    close(fd);
    return ok;
}

}

ServerProcess::~ServerProcess() { Stop(); }

std::string ServerProcess::ExecutableDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    std::string path(buf, static_cast<size_t>(n));
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

bool ServerProcess::Start(const std::string& binary, const std::string& log_path) {
    if (access(binary.c_str(), X_OK) != 0) {
        error_ = "fuzzy-server not found: " + binary;
        return false;
    }
    port_ = FreePort();
    if (port_ == 0) {
        error_ = "no free port";
        return false;
    }
    std::string addr = "127.0.0.1:" + std::to_string(port_);
    pid_t parent = getpid();
    pid_ = fork();
    if (pid_ < 0) {
        error_ = std::string("fork: ") + std::strerror(errno);
        return false;
    }
    if (pid_ == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent) _exit(1);
        execl(binary.c_str(), binary.c_str(), "-addr", addr.c_str(), "-log", log_path.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (CanConnect(port_)) return true;
        int status = 0;
        if (waitpid(pid_, &status, WNOHANG) == pid_) {
            pid_ = -1;
            error_ = "fuzzy-server exited, code " + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    error_ = "fuzzy-server did not start on " + addr;
    Stop();
    return false;
}

void ServerProcess::Stop() {
    if (pid_ <= 0) return;
    kill(pid_, SIGTERM);
    for (int i = 0; i < 40; ++i) {
        if (waitpid(pid_, nullptr, WNOHANG) == pid_) {
            pid_ = -1;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    kill(pid_, SIGKILL);
    waitpid(pid_, nullptr, 0);
    pid_ = -1;
}

std::string ServerProcess::Url() const {
    return "http://127.0.0.1:" + std::to_string(port_) + "/";
}
