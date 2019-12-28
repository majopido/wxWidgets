/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/msw/webview_edge.h
// Purpose:     wxMSW Edge Chromium wxWebView backend
// Author:      Markus Pingel
// Created:     2019-12-15
// Copyright:   (c) 2019 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef wxWebViewEdgeC_H
#define wxWebViewEdgeC_H

#define wxUSE_WEBVIEW_EDGE_C 1
#include "wx/setup.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_EDGE_C && defined(__WXMSW__)

#include "wx/control.h"
#include "wx/webview.h"
#include "wx/msw/private/comptr.h"

#import <wx/msw/webview2/webview2.tlb>
#include <wx/msw/webview2/webview2.h>
// #include <Windows.Web.UI.h>
#include <Windows.Web.UI.Interop.h>


class WXDLLIMPEXP_WEBVIEW wxWebViewEdgeChromium : public wxWebView
{
public:

    wxWebViewEdgeChromium() {}

    wxWebViewEdgeChromium(wxWindow* parent,
        wxWindowID id,
        const wxString& url = wxWebViewDefaultURLStr,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxWebViewNameStr)
    {
        Create(parent, id, url, pos, size, style, name);
    }

    ~wxWebViewEdgeChromium();

    bool Create(wxWindow* parent,
        wxWindowID id,
        const wxString& url = wxWebViewDefaultURLStr,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxWebViewNameStr) wxOVERRIDE;

    virtual void LoadURL(const wxString& url) wxOVERRIDE;
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item) wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory() wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory() wxOVERRIDE;

    virtual bool CanGoForward() const wxOVERRIDE;
    virtual bool CanGoBack() const wxOVERRIDE;
    virtual void GoBack() wxOVERRIDE;
    virtual void GoForward() wxOVERRIDE;
    virtual void ClearHistory() wxOVERRIDE;
    virtual void EnableHistory(bool enable = true) wxOVERRIDE;
    virtual void Stop() wxOVERRIDE;
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT) wxOVERRIDE;

    virtual wxString GetPageSource() const wxOVERRIDE;
    virtual wxString GetPageText() const wxOVERRIDE;

    virtual bool IsBusy() const wxOVERRIDE;
    virtual wxString GetCurrentURL() const wxOVERRIDE;
    virtual wxString GetCurrentTitle() const wxOVERRIDE;

    virtual void SetZoomType(wxWebViewZoomType) wxOVERRIDE;
    virtual wxWebViewZoomType GetZoomType() const wxOVERRIDE;
    virtual bool CanSetZoomType(wxWebViewZoomType) const wxOVERRIDE;

    virtual void Print() wxOVERRIDE;

    virtual wxWebViewZoom GetZoom() const wxOVERRIDE;
    virtual void SetZoom(wxWebViewZoom zoom) wxOVERRIDE;

    //Clipboard functions
    virtual bool CanCut() const wxOVERRIDE;
    virtual bool CanCopy() const wxOVERRIDE;
    virtual bool CanPaste() const wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Copy() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    //Undo / redo functionality
    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;
    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;

    //Find function
    virtual long Find(const wxString& text, int flags = wxWEBVIEW_FIND_DEFAULT) wxOVERRIDE;

    //Editing functions
    virtual void SetEditable(bool enable = true) wxOVERRIDE;
    virtual bool IsEditable() const wxOVERRIDE;

    //Selection
    virtual void SelectAll() wxOVERRIDE;
    virtual bool HasSelection() const wxOVERRIDE;
    virtual void DeleteSelection() wxOVERRIDE;
    virtual wxString GetSelectedText() const wxOVERRIDE;
    virtual wxString GetSelectedSource() const wxOVERRIDE;
    virtual void ClearSelection() wxOVERRIDE;

    virtual bool RunScript(const wxString& javascript, wxString* output = NULL) wxOVERRIDE;

    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler) wxOVERRIDE;

    virtual void* GetNativeBackend() const wxOVERRIDE { return m_webView; }

    // ---- Edge-specific methods

    static bool IsAvailable();

    wxDECLARE_EVENT_TABLE();

protected:
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl) wxOVERRIDE;

private:
    bool m_initialized = false;
    bool m_isBusy = false;
    wxString m_pendingURL;

    wxCOMPtr<IWebView2Environment3> m_webViewEnvironment;
    wxCOMPtr<IWebView2WebView5> m_webView;

    EventRegistrationToken m_navigationStartingToken = { };
    EventRegistrationToken m_navigationCompletedToken = { };
    EventRegistrationToken m_newWindowRequestedToken = { };
    EventRegistrationToken m_documentStateChangedToken = { };

    void OnSize(wxSizeEvent& event);

    void UpdateBounds();

    void InitWebViewCtrl();

    static wxCOMPtr<ABI::Windows::Web::UI::Interop::IWebViewControlProcess> ms_webViewCtrlProcess;
    static int ms_isAvailable;

    static void Initialize();

    static void Uninitalize();

    friend class wxWebViewEdgeModule;

    wxDECLARE_DYNAMIC_CLASS(wxWebViewEdgeChromium);
};

class WXDLLIMPEXP_WEBVIEW wxWebViewFactoryEdge : public wxWebViewFactory
{
public:
    virtual wxWebView* Create() wxOVERRIDE { return new wxWebViewEdgeChromium; }
    virtual wxWebView* Create(wxWindow* parent,
        wxWindowID id,
        const wxString& url = wxWebViewDefaultURLStr,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxWebViewNameStr) wxOVERRIDE
    {
        return new wxWebViewEdgeChromium(parent, id, url, pos, size, style, name);
    }
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_EDGE && defined(__WXMSW__)

#endif // wxWebViewEdgeC_H
