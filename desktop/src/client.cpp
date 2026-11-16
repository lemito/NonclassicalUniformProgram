#include "client.h"

#include <cstdlib>
#include <string>

#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace {

FuzzyClient* g_instance = nullptr;

std::string EscapeHtml(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string DownloadsDir() {
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/Downloads";
}

}

FuzzyClient::FuzzyClient() { g_instance = this; }

FuzzyClient::~FuzzyClient() { g_instance = nullptr; }

FuzzyClient* FuzzyClient::GetInstance() { return g_instance; }

void FuzzyClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
    auto view = CefBrowserView::GetForBrowser(browser);
    if (!view) return;
    auto window = view->GetWindow();
    if (window) window->SetTitle(title);
}

void FuzzyClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    browsers_.push_back(browser);
}

bool FuzzyClient::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    return false;
}

void FuzzyClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    for (auto it = browsers_.begin(); it != browsers_.end(); ++it) {
        if ((*it)->IsSame(browser)) {
            browsers_.erase(it);
            break;
        }
    }
    if (browsers_.empty()) CefQuitMessageLoop();
}

void FuzzyClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              ErrorCode errorCode,
                              const CefString& errorText,
                              const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();
    if (errorCode == ERR_ABORTED) return;
    if (!frame->IsMain()) return;
    frame->LoadURL(MessageUrl("Ошибка загрузки",
                              "Не удалось открыть " + failedUrl.ToString() + ": " + errorText.ToString() +
                                  " (" + std::to_string(errorCode) + ")"));
}

bool FuzzyClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefDownloadItem> download_item,
                                   const CefString& suggested_name,
                                   CefRefPtr<CefBeforeDownloadCallback> callback) {
    CEF_REQUIRE_UI_THREAD();
    callback->Continue(DownloadsDir() + "/" + suggested_name.ToString(), true);
    return true;
}

std::string FuzzyClient::MessageUrl(const std::string& title, const std::string& text) {
    std::string html =
        "<!doctype html><html lang=\"ru\"><head><meta charset=\"utf-8\"><title>" + EscapeHtml(title) +
        "</title></head><body style=\"font-family:sans-serif;padding:2em\"><h2>" + EscapeHtml(title) +
        "</h2><p>" + EscapeHtml(text) + "</p></body></html>";
    return "data:text/html;base64," + CefURIEncode(CefBase64Encode(html.data(), html.size()), false).ToString();
}
