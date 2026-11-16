#pragma once

#include <list>
#include <string>

#include "include/cef_client.h"

class FuzzyClient : public CefClient,
                    public CefDisplayHandler,
                    public CefLifeSpanHandler,
                    public CefLoadHandler,
                    public CefDownloadHandler {
public:
    FuzzyClient();
    ~FuzzyClient() override;

    static FuzzyClient* GetInstance();

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override;

    bool OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefDownloadItem> download_item,
                          const CefString& suggested_name,
                          CefRefPtr<CefBeforeDownloadCallback> callback) override;

    static std::string MessageUrl(const std::string& title, const std::string& text);

private:
    std::list<CefRefPtr<CefBrowser>> browsers_;

    IMPLEMENT_REFCOUNTING(FuzzyClient);
    DISALLOW_COPY_AND_ASSIGN(FuzzyClient);
};
