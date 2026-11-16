#pragma once

#include <string>
#include <sys/types.h>

class ServerProcess {
public:
    ServerProcess() = default;
    ~ServerProcess();

    ServerProcess(const ServerProcess&) = delete;
    ServerProcess& operator=(const ServerProcess&) = delete;

    bool Start(const std::string& binary, const std::string& log_path);
    void Stop();
    std::string Url() const;
    const std::string& Error() const { return error_; }

    static std::string ExecutableDir();

private:
    pid_t pid_ = -1;
    int port_ = 0;
    std::string error_;
};
