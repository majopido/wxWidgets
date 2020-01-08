/////////////////////////////////////////////////////////////////////////////
// Name:        source/msw/webview_edgec.cpp
// Purpose:     wxMSW Edge Chromium wxWebView backend implementation
// Author:      Markus Pingel
// Created:     2019-12-15
// Copyright:   (c) 2019 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include "wx/msw/webview_edgec.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_EDGE_C

#include "wx/module.h"
#include "wx/msw/rt/utils.h"
#include "wx/msw/wrapshl.h"
#include "wx/msw/ole/comimpl.h"

#include <wrl/event.h>

#include "wx/filename.h"

namespace rt = wxWinRT;

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Web::UI;
using namespace ABI::Windows::Web::UI::Interop;

using namespace Microsoft::WRL;


wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewEdgeChromium, wxWebView);

wxBEGIN_EVENT_TABLE(wxWebViewEdgeChromium, wxControl)
EVT_SIZE(wxWebViewEdgeChromium::OnSize)
wxEND_EVENT_TABLE()

/*#define WX_ERROR_CASE(error, wxerror) \
        case ABI::Windows::Web::error: \
            event.SetString(#error); \
            event.SetInt(wxerror); \
            break;
*/
#define WX_ERROR2_CASE(error, wxerror) \
        case error: \
            event.SetString(#error); \
            event.SetInt(wxerror); \
            break;

wxCOMPtr<IWebViewControlProcess> wxWebViewEdgeChromium::ms_webViewCtrlProcess;
int wxWebViewEdgeChromium::ms_isAvailable = -1;

static ABI::Windows::Foundation::Rect wxRectToFoundationRect(const wxRect& rect)
{
    return { (FLOAT)rect.x, (FLOAT)rect.y, (FLOAT)rect.width, (FLOAT)rect.height };
}

static wxString wxStringFromFoundationURI(IUriRuntimeClass* uri)
{
    HSTRING uriStr;
    if (uri && SUCCEEDED(uri->get_RawUri(&uriStr)))
        return rt::wxStringFromHSTRING(uriStr);
    else
        return wxString();
}

bool wxWebViewEdgeChromium::IsAvailable()
{
    if (ms_isAvailable == -1)
        Initialize();

    return (ms_isAvailable == 1);
}

void wxWebViewEdgeChromium::Initialize()
{
    // Initialize control process
    wxCOMPtr<IWebViewControlProcessFactory> processFactory;
    if (!rt::GetActivationFactory(RuntimeClass_Windows_Web_UI_Interop_WebViewControlProcess,
        wxIID_PPV_ARGS(IWebViewControlProcessFactory, &processFactory)))
    {
        wxLogDebug("Could not create WebViewControlProcessFactory");
        ms_isAvailable = 0;
        return;
    }

    wxCOMPtr<IWebViewControlProcessOptions> procOptions;
    wxCOMPtr<IInspectable> insp;
    if (!rt::ActivateInstance(RuntimeClass_Windows_Web_UI_Interop_WebViewControlProcessOptions, &insp) ||
        FAILED(insp->QueryInterface(wxIID_PPV_ARGS(IWebViewControlProcessOptions, &procOptions))))
    {
        wxLogDebug("Could not create WebViewControlProcessOptions");
        ms_isAvailable = 0;
        return;
    }

    if (!SUCCEEDED(processFactory->CreateWithOptions(procOptions, &ms_webViewCtrlProcess)))
    {
        wxLogDebug("Could not create WebViewControlProcess");
        ms_isAvailable = 0;
        return;
    }

    ms_isAvailable = 1;
}

void wxWebViewEdgeChromium::Uninitalize()
{
    if (ms_isAvailable == 1)
    {
        ms_webViewCtrlProcess.reset();
        ms_isAvailable = -1;
    }
}

wxWebViewEdgeChromium::~wxWebViewEdgeChromium()
{
    if (m_webView)
    {
        // TOOD: Remove additional events
        m_webView->remove_NavigationCompleted(m_navigationCompletedToken);
        m_webView->remove_NavigationStarting(m_navigationStartingToken);
        m_webView->remove_DocumentStateChanged(m_documentStateChangedToken);
        m_webView->remove_NewWindowRequested(m_newWindowRequestedToken);
    }
}

bool wxWebViewEdgeChromium::Create(wxWindow* parent,
    wxWindowID id,
    const wxString& url,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
{
    m_initialized = false;
    m_isBusy = false;

    m_historyLoadingFromList = false;
    m_historyEnabled = true;
    m_historyPosition = -1;

    if (!wxControl::Create(parent, id, pos, size, style,
        wxDefaultValidator, name))
    {
        return false;
    }

    if (!IsAvailable())
        return false;

    m_pendingURL = url;

    LPCWSTR subFolder = nullptr;
    LPCWSTR additionalBrowserSwitches = nullptr;

    HRESULT hr = CreateWebView2EnvironmentWithDetails(
        subFolder, nullptr, additionalBrowserSwitches,
        Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, IWebView2Environment* environment) -> HRESULT
            {
                UNREFERENCED_PARAMETER(result);
                environment->QueryInterface(IID_PPV_ARGS(&m_webViewEnvironment));
                m_webViewEnvironment->CreateWebView(
                    GetHWND(), Callback<IWebView2CreateWebViewCompletedHandler>(
                        [this](HRESULT result, IWebView2WebView* webview) -> HRESULT
                        {
                            UNREFERENCED_PARAMETER(result);
                            webview->QueryInterface(IID_PPV_ARGS(&m_webView));
                            UpdateBounds();
                            InitWebViewCtrl();
                            return S_OK;
                        })
                    .Get());
                return S_OK;
            }).Get());
    return SUCCEEDED(hr);
}

void wxWebViewEdgeChromium::InitWebViewCtrl()
{
    m_initialized = true;
    UpdateBounds();

    // Connect and handle the various WebView events

    m_webView->add_NavigationStarting(
        Callback<IWebView2NavigationStartingEventHandler>(
            [this](IWebView2WebView* sender, IWebView2NavigationStartingEventArgs* args) -> HRESULT
            {
                UNREFERENCED_PARAMETER(sender);
                m_isBusy = true;
                wxString evtURL;
                PWSTR uri;
                if (SUCCEEDED(args->get_Uri(&uri)))
                    evtURL = wxString(uri);
                wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATING, GetId(), evtURL, wxString());
                event.SetEventObject(this);
                HandleWindowEvent(event);

                if (!event.IsAllowed())
                    args->put_Cancel(true);

                return S_OK;
            })
        .Get(), &m_navigationStartingToken);

    m_webView->add_DocumentStateChanged(
        Callback<IWebView2DocumentStateChangedEventHandler>(
            [this](IWebView2WebView* sender, IWebView2DocumentStateChangedEventArgs* args) -> HRESULT
            {
                UNREFERENCED_PARAMETER(args);
                PWSTR uri;
                sender->get_Source(&uri);
                wxString evtURL(uri);
                if (evtURL.Cmp(L"about:blank") == 0)              
                {
                    evtURL = L"";                    
                }
                
                // AddPendingEvent(wxWebViewEvent(wxEVT_WEBVIEW_NAVIGATED, GetId(), uri, wxString()));
                // SetWindowText(m_toolbar->addressBarWindow, uri.get());
                return S_OK;
            })
        .Get(),
                &m_documentStateChangedToken);

    m_webView->add_NavigationCompleted(
        Callback<IWebView2NavigationCompletedEventHandler>(
            [this](IWebView2WebView* sender, IWebView2NavigationCompletedEventArgs* args) -> HRESULT
            {
                UNREFERENCED_PARAMETER(sender);
                BOOL isSuccess;
                if (FAILED(args->get_IsSuccess(&isSuccess)))
                    isSuccess = false;
                m_isBusy = false;
                PWSTR _uri;
                sender->get_Source(&_uri);
                wxString uri(_uri);

                if (!isSuccess)
                {
                    WEBVIEW2_WEB_ERROR_STATUS status;

                    wxWebViewEvent event(wxEVT_WEBVIEW_ERROR, GetId(), uri, wxString());
                    event.SetEventObject(this);

                    if (SUCCEEDED(args->get_WebErrorStatus(&status)))
                    {
                        switch (status)
                        {
                            WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_UNKNOWN, wxWEBVIEW_NAV_ERR_OTHER)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_COMMON_NAME_IS_INCORRECT, wxWEBVIEW_NAV_ERR_CERTIFICATE)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_EXPIRED, wxWEBVIEW_NAV_ERR_CERTIFICATE)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CLIENT_CERTIFICATE_CONTAINS_ERRORS, wxWEBVIEW_NAV_ERR_CERTIFICATE)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_REVOKED, wxWEBVIEW_NAV_ERR_CERTIFICATE)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_IS_INVALID, wxWEBVIEW_NAV_ERR_CERTIFICATE)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_SERVER_UNREACHABLE, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_TIMEOUT, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_ERROR_HTTP_INVALID_SERVER_RESPONSE, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CONNECTION_RESET, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_DISCONNECTED, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_CANNOT_CONNECT, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_HOST_NAME_NOT_RESOLVED, wxWEBVIEW_NAV_ERR_CONNECTION)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED, wxWEBVIEW_NAV_ERR_USER_CANCELLED)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_REDIRECT_FAILED, wxWEBVIEW_NAV_ERR_OTHER)
                                WX_ERROR2_CASE(WEBVIEW2_WEB_ERROR_STATUS_UNEXPECTED_ERROR, wxWEBVIEW_NAV_ERR_OTHER)
                        }
                    }
                    HandleWindowEvent(event);
                }
                else
                {
                    AddPendingEvent(wxWebViewEvent(wxEVT_WEBVIEW_NAVIGATED, GetId(), uri, wxString()));
                    if (m_historyEnabled && !m_historyLoadingFromList &&
                        (uri == GetCurrentURL()) ||
                        (GetCurrentURL().substr(0, 4) == "file" &&
                            wxFileName::URLToFileName(GetCurrentURL()).GetFullPath() == uri))
                    {
                        // If we are not at the end of the list, then erase everything
                        // between us and the end before adding the new page
                        if (m_historyPosition != static_cast<int>(m_historyList.size()) - 1)
                        {
                            m_historyList.erase(m_historyList.begin() + m_historyPosition + 1,
                                m_historyList.end());
                        }
                        wxSharedPtr<wxWebViewHistoryItem> item(new wxWebViewHistoryItem(uri, GetCurrentTitle()));
                        m_historyList.push_back(item);
                        m_historyPosition++;
                    }
                    //Reset as we are done now
                    m_historyLoadingFromList = false;
                }
                return S_OK;
            })
        .Get(), &m_navigationCompletedToken);
    m_webView->add_NewWindowRequested(
        Callback<IWebView2NewWindowRequestedEventHandler>(
            [this](IWebView2WebView* sender, IWebView2NewWindowRequestedEventArgs* args) -> HRESULT
            {
                UNREFERENCED_PARAMETER(sender);
                PWSTR uri;
                args->get_Uri(&uri);
                wxString evtURL(uri);
                AddPendingEvent(wxWebViewEvent(wxEVT_WEBVIEW_NEWWINDOW, GetId(), evtURL, wxString()));
                args->put_Handled(true);
                return S_OK;
            }).Get(), &m_newWindowRequestedToken);
    LoadURL(m_pendingURL);
}

void wxWebViewEdgeChromium::OnSize(wxSizeEvent& event)
{
    UpdateBounds();
    event.Skip();
}

void wxWebViewEdgeChromium::UpdateBounds()
{
    RECT r;
    wxCopyRectToRECT(GetClientRect(), r);
    if (m_webView)
        m_webView->put_Bounds(r);
}

void wxWebViewEdgeChromium::LoadURL(const wxString& url)
{
    if (!m_webView)
    {
        m_pendingURL = url;
        return;
    }
    if (FAILED(m_webView->Navigate(url)))
        wxLogError("Could not navigate to URL");
}

void wxWebViewEdgeChromium::LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item)
{
    int pos = -1;
    for (unsigned int i = 0; i < m_historyList.size(); i++)
    {
        //We compare the actual pointers to find the correct item
        if (m_historyList[i].get() == item.get())
            pos = i;
    }
    // TODO: wxASSERT_MSG(pos != static_cast<int>(m_impl->m_historyList.size()), "invalid history item");
    m_historyLoadingFromList = true;
    LoadURL(item->GetUrl());
    m_historyPosition = pos;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewEdgeChromium::GetBackwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > backhist;
    //As we don't have std::copy or an iterator constructor in the wxwidgets
    //native vector we construct it by hand
    for (int i = 0; i < m_historyPosition; i++)
    {
        backhist.push_back(m_historyList[i]);
    }
    return backhist;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewEdgeChromium::GetForwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > forwardhist;
    //As we don't have std::copy or an iterator constructor in the wxwidgets
    //native vector we construct it by hand
    for (int i = m_historyPosition + 1; i < static_cast<int>(m_historyList.size()); i++)
    {
        forwardhist.push_back(m_historyList[i]);
    }
    return forwardhist;
}

bool wxWebViewEdgeChromium::CanGoForward() const
{
    if (m_historyEnabled)
        return m_historyPosition != static_cast<int>(m_historyList.size()) - 1;
    else
        return false;

    /*
        BOOL result = false;
        if (m_webView && SUCCEEDED(m_webView->get_CanGoForward(&result)))
            return result;
        else
            return false;
    */
}

bool wxWebViewEdgeChromium::CanGoBack() const
{
    if (m_historyEnabled)
        return m_historyPosition > 0;
    else
        return false;
    /*
        BOOL result = false;

        if (m_webView && SUCCEEDED(m_webView->get_CanGoBack(&result)))
            return result;
        else
            return false;
    */
}

void wxWebViewEdgeChromium::GoBack()
{
    LoadHistoryItem(m_historyList[m_historyPosition - 1]);
    /* if (m_webView)
        m_webView->GoBack(); */
}

void wxWebViewEdgeChromium::GoForward()
{
    LoadHistoryItem(m_historyList[m_historyPosition + 1]);
    /* if (m_webView)
        m_webView->GoForward(); */
}

void wxWebViewEdgeChromium::ClearHistory()
{
    m_historyList.clear();
    m_historyPosition = -1;
}

void wxWebViewEdgeChromium::EnableHistory(bool enable)
{
    m_historyEnabled = enable;
    m_historyList.clear();
    m_historyPosition = -1;
}

void wxWebViewEdgeChromium::Stop()
{
    if (m_webView)
        m_webView->Stop();
}

void wxWebViewEdgeChromium::Reload(wxWebViewReloadFlags flags)
{
    UNREFERENCED_PARAMETER(flags);
    if (m_webView)
        m_webView->Reload();
}

wxString wxWebViewEdgeChromium::GetPageSource() const
{
    return wxString();
}

wxString wxWebViewEdgeChromium::GetPageText() const
{
    return wxString();
}

bool wxWebViewEdgeChromium::IsBusy() const
{
    return m_isBusy;
}

wxString wxWebViewEdgeChromium::GetCurrentURL() const
{
    LPWSTR uri;
    if (m_webView && SUCCEEDED(m_webView->get_Source(&uri)))
        return wxString(uri);
    else
        return wxString();
}

wxString wxWebViewEdgeChromium::GetCurrentTitle() const
{
    PWSTR _title;
    if (m_webView && SUCCEEDED(m_webView->get_DocumentTitle(&_title)))
        return wxString(_title);
    else
        return wxString();
}

void wxWebViewEdgeChromium::SetZoomType(wxWebViewZoomType)
{

}

wxWebViewZoomType wxWebViewEdgeChromium::GetZoomType() const
{
    return wxWEBVIEW_ZOOM_TYPE_LAYOUT;
}

bool wxWebViewEdgeChromium::CanSetZoomType(wxWebViewZoomType) const
{
    return false;
}

void wxWebViewEdgeChromium::Print()
{

}

wxWebViewZoom wxWebViewEdgeChromium::GetZoom() const
{
    double old_zoom_factor = 0.0f;
    m_webView->get_ZoomFactor(&old_zoom_factor);
    if (old_zoom_factor > 1.7f)
        return wxWEBVIEW_ZOOM_LARGEST;
    if (old_zoom_factor > 1.3f)
        return wxWEBVIEW_ZOOM_LARGE;
    if (old_zoom_factor > 0.8f)
        return wxWEBVIEW_ZOOM_MEDIUM;
    if (old_zoom_factor > 0.6f)
        return wxWEBVIEW_ZOOM_SMALL;
    return wxWEBVIEW_ZOOM_TINY;
}

void wxWebViewEdgeChromium::SetZoom(wxWebViewZoom zoom)
{
    double old_zoom_factor = 0.0f;
    m_webView->get_ZoomFactor(&old_zoom_factor);
    double zoom_factor = 1.0f;
    switch (zoom)
    {
    case wxWEBVIEW_ZOOM_LARGEST:
        zoom_factor = 2.0f;
        break;
    case wxWEBVIEW_ZOOM_LARGE:
        zoom_factor = 1.5f;
        break;
    case wxWEBVIEW_ZOOM_MEDIUM:
        zoom_factor = 1.0f;
        break;
    case wxWEBVIEW_ZOOM_SMALL:
        zoom_factor = 0.75f;
        break;
    case wxWEBVIEW_ZOOM_TINY:
        zoom_factor = 0.5f;
        break;
    default:
        break;
    }
    m_webView->put_ZoomFactor(zoom_factor);
    // UNREFERENCED_PARAMETER(zoom);
}

bool wxWebViewEdgeChromium::CanCut() const
{
    return false;
}

bool wxWebViewEdgeChromium::CanCopy() const
{
    return false;
}

bool wxWebViewEdgeChromium::CanPaste() const
{
    return false;
}

void wxWebViewEdgeChromium::Cut()
{

}

void wxWebViewEdgeChromium::Copy()
{

}

void wxWebViewEdgeChromium::Paste()
{

}

bool wxWebViewEdgeChromium::CanUndo() const
{
    return false;
}

bool wxWebViewEdgeChromium::CanRedo() const
{
    return false;
}

void wxWebViewEdgeChromium::Undo()
{

}

void wxWebViewEdgeChromium::Redo()
{

}

long wxWebViewEdgeChromium::Find(const wxString& text, int flags)
{
    UNREFERENCED_PARAMETER(text);
    UNREFERENCED_PARAMETER(flags);
    return -1;
}

//Editing functions
void wxWebViewEdgeChromium::SetEditable(bool WXUNUSED(enable))
{

}

bool wxWebViewEdgeChromium::IsEditable() const
{
    return false;
}

void wxWebViewEdgeChromium::SelectAll()
{

}

bool wxWebViewEdgeChromium::HasSelection() const
{
    return false;
}

void wxWebViewEdgeChromium::DeleteSelection()
{

}

wxString wxWebViewEdgeChromium::GetSelectedText() const
{
    return wxString();
}

wxString wxWebViewEdgeChromium::GetSelectedSource() const
{
    return wxString();
}

void wxWebViewEdgeChromium::ClearSelection()
{

}

bool wxWebViewEdgeChromium::RunScript(const wxString& javascript, wxString* output)
{
    UNREFERENCED_PARAMETER(javascript);
    UNREFERENCED_PARAMETER(output);
    return false;
}

void wxWebViewEdgeChromium::RegisterHandler(wxSharedPtr<wxWebViewHandler> handler)
{

}

void wxWebViewEdgeChromium::DoSetPage(const wxString& html, const wxString& baseUrl)
{
    UNREFERENCED_PARAMETER(baseUrl);
    if (m_webView)
        m_webView->NavigateToString(html);
}

// ----------------------------------------------------------------------------
// Module ensuring all global/singleton objects are destroyed on shutdown.
// ----------------------------------------------------------------------------

class wxWebViewEdgeModule : public wxModule
{
public:
    wxWebViewEdgeModule()
    {
    }

    virtual bool OnInit() wxOVERRIDE
    {
        return true;
    }

    virtual void OnExit() wxOVERRIDE
    {
        wxWebViewEdgeChromium::Uninitalize();
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxWebViewEdgeModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewEdgeModule, wxModule);

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_EDGE
