#include <cstdlib>
#include <string>

#include "app.h"
#include "include/cef_app.h"
#include "server_process.h"

int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);

    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) return exit_code;

    CefSettings settings;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;
    const char* home = std::getenv("HOME");
    std::string cache = std::string(home ? home : "/tmp") + "/.cache/fuzzy-desktop";
    CefString(&settings.root_cache_path) = cache;

    ServerProcess server;
    CefRefPtr<FuzzyApp> app(new FuzzyApp(&server));

    if (!CefInitialize(main_args, settings, app.get(), nullptr)) return CefGetExitCode();

    CefRunMessageLoop();
    CefShutdown();
    server.Stop();
    return 0;
}
