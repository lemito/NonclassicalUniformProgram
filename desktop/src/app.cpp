#include "app.h"

#include <string>

#include "client.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace {

class FuzzyWindowDelegate : public CefWindowDelegate {
public:
    explicit FuzzyWindowDelegate(CefRefPtr<CefBrowserView> browser_view) : browser_view_(browser_view) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        window->AddChildView(browser_view_);
        window->SetTitle("Нечёткие величины U(a,b)");
        window->Show();
        browser_view_->RequestFocus();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override { browser_view_ = nullptr; }

    bool CanClose(CefRefPtr<CefWindow> window) override {
        CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
        return browser ? browser->GetHost()->TryCloseBrowser() : true;
    }

    CefSize GetPreferredSize(CefRefPtr<CefView> view) override { return CefSize(1400, 900); }

    CefSize GetMinimumSize(CefRefPtr<CefView> view) override { return CefSize(800, 600); }

private:
    CefRefPtr<CefBrowserView> browser_view_;

    IMPLEMENT_REFCOUNTING(FuzzyWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(FuzzyWindowDelegate);
};

class FuzzyBrowserViewDelegate : public CefBrowserViewDelegate {
public:
    FuzzyBrowserViewDelegate() = default;

    bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                   CefRefPtr<CefBrowserView> popup_browser_view,
                                   bool is_devtools) override {
        CefWindow::CreateTopLevelWindow(new FuzzyWindowDelegate(popup_browser_view));
        return true;
    }

private:
    IMPLEMENT_REFCOUNTING(FuzzyBrowserViewDelegate);
    DISALLOW_COPY_AND_ASSIGN(FuzzyBrowserViewDelegate);
};

}

FuzzyApp::FuzzyApp(ServerProcess* server) : server_(server) {}

void FuzzyApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
    CefRefPtr<FuzzyClient> client(new FuzzyClient());

    std::string url = command_line->GetSwitchValue("url");
    if (url.empty()) {
        std::string binary = command_line->GetSwitchValue("server");
        if (binary.empty()) binary = ServerProcess::ExecutableDir() + "/fuzzy-server";
        std::string log_path = command_line->GetSwitchValue("log");
        if (log_path.empty()) log_path = "fuzzy.log";
        if (server_->Start(binary, log_path)) {
            url = server_->Url();
        } else {
            url = FuzzyClient::MessageUrl("Сервер не запущен", server_->Error());
        }
    }

    CefBrowserSettings browser_settings;
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client, url, browser_settings, nullptr, nullptr, new FuzzyBrowserViewDelegate());
    CefWindow::CreateTopLevelWindow(new FuzzyWindowDelegate(browser_view));
}

CefRefPtr<CefClient> FuzzyApp::GetDefaultClient() { return FuzzyClient::GetInstance(); }
