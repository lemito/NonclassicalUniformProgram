#pragma once

#include "include/cef_app.h"
#include "server_process.h"

class FuzzyApp : public CefApp, public CefBrowserProcessHandler {
public:
    explicit FuzzyApp(ServerProcess* server);

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    void OnContextInitialized() override;
    CefRefPtr<CefClient> GetDefaultClient() override;

private:
    ServerProcess* server_;

    IMPLEMENT_REFCOUNTING(FuzzyApp);
    DISALLOW_COPY_AND_ASSIGN(FuzzyApp);
};
