/////////////////////////////////////////////////////////////////////////////
// Name:        notebook.cpp
// Purpose:
// Author:      Robert Roebling
// Created:     01/02/97
// Id:
// Copyright:   (c) 1998 Robert Roebling, Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "notebook.h"
#endif

#include "wx/notebook.h"
#include "wx/panel.h"
#include "wx/utils.h"
#include "wx/imaglist.h"
#include "wx/intl.h"
#include "wx/log.h"

//-----------------------------------------------------------------------------
// GTK callbacks
//-----------------------------------------------------------------------------

// page change callback
static void gtk_notebook_page_change_callback(GtkNotebook *widget,
                                              GtkNotebookPage *page,
                                              gint nPage,
                                              gpointer data)
{
  wxNotebook *notebook = (wxNotebook *)data;

  int nOld = notebook->GetSelection();

  // TODO: emulate PAGE_CHANGING event
  wxNotebookEvent event(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,
                        notebook->GetId(),
                        nPage,
                        nOld);
  event.SetEventObject(notebook);
  notebook->ProcessEvent(event);
}

//-----------------------------------------------------------------------------
// wxNotebookPage
//-----------------------------------------------------------------------------

class wxNotebookPage: public wxObject
{
public:
  wxNotebookPage()
  {
    m_id = -1;
    m_text = "";
    m_image = -1;
    m_page = NULL;
    m_clientPanel = NULL;
  };

//private:
  int                m_id;
  wxString           m_text;
  int                m_image;
  GtkNotebookPage   *m_page;
  GtkLabel          *m_label;
  wxWindow          *m_clientPanel;
};

//-----------------------------------------------------------------------------
// wxNotebook
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxNotebook, wxControl)
  EVT_SIZE(wxNotebook::OnSize)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(wxNotebook,wxControl)

void wxNotebook::Init()
{
  m_imageList = NULL;
  m_pages.DeleteContents( TRUE );
  m_idHandler = 0;
}

wxNotebook::wxNotebook()
{
  Init();
};

wxNotebook::wxNotebook( wxWindow *parent, wxWindowID id,
      const wxPoint& pos, const wxSize& size,
      long style, const wxString& name )
{
  Init();
  Create( parent, id, pos, size, style, name );
};

wxNotebook::~wxNotebook()
{
  // don't generate change page events any more
  if ( m_idHandler != 0 )
    gtk_signal_disconnect(GTK_OBJECT(m_widget), m_idHandler);

  if (m_imageList)
    delete m_imageList;
  DeleteAllPages();
};

bool wxNotebook::Create(wxWindow *parent, wxWindowID id,
      const wxPoint& pos, const wxSize& size,
      long style, const wxString& name )
{
  m_needParent = TRUE;

  PreCreation( parent, id, pos, size, style, name );

  m_widget = gtk_notebook_new();
  m_idHandler = gtk_signal_connect
                (
                  GTK_OBJECT(m_widget), "switch_page",
                  GTK_SIGNAL_FUNC(gtk_notebook_page_change_callback),
                  (gpointer)this
                );

  PostCreation();

  Show( TRUE );

  return TRUE;
};

int wxNotebook::GetSelection() const
{
  if (m_pages.Number() == 0)
    return -1;

  GtkNotebookPage *g_page = GTK_NOTEBOOK(m_widget)->cur_page;

  wxNotebookPage *page = NULL;

  wxNode *node = m_pages.First();
  while (node)
  {
    page = (wxNotebookPage*)node->Data();
    if (page->m_page == g_page)
      break;
    node = node->Next();
  };

  wxCHECK_MSG( node != NULL, -1, "wxNotebook: no selection?");

  return page->m_id;
};

int wxNotebook::GetPageCount() const
{
  return m_pages.Number();
};

int wxNotebook::GetRowCount() const
{
  return 1;
};

wxString wxNotebook::GetPageText( int page ) const
{
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (nb_page)
    return nb_page->m_text;
  else
    return "";
};

int wxNotebook::GetPageImage( int page ) const
{
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (nb_page)
    return nb_page->m_image;
  else
    return 0;
};

wxNotebookPage* wxNotebook::GetNotebookPage(int page) const
{
  wxNotebookPage *nb_page = NULL;

  wxNode *node = m_pages.First();
  while (node)
  {
    nb_page = (wxNotebookPage*)node->Data();
    if (nb_page->m_id == page)
      return nb_page;
    node = node->Next();
  };

  wxLogDebug("Notebook page %d not found!", page);

  return NULL;
};

int wxNotebook::SetSelection( int page )
{
  int selOld = GetSelection();
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (!nb_page)
    return -1;

  int page_num = 0;
  GList *child = GTK_NOTEBOOK(m_widget)->children;
  while (child)
  {
    if (nb_page->m_page == (GtkNotebookPage*)child->data)
      break;
    page_num++;
    child = child->next;
  };

  if (!child) return -1;

  gtk_notebook_set_page( GTK_NOTEBOOK(m_widget), page_num );

  return selOld;
};

void wxNotebook::AdvanceSelection(bool bForward)
{
  int nSel = GetSelection(),
      nMax = GetPageCount();

  if ( bForward ) {
    SetSelection(nSel == nMax ? 0 : nSel + 1);
  }
  else {
    SetSelection(nSel == 0 ? nMax : nSel - 1);
  }
}

void wxNotebook::SetImageList( wxImageList* imageList )
{
  m_imageList = imageList;
};

bool wxNotebook::SetPageText( int page, const wxString &text )
{
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (!nb_page)
    return FALSE;

  nb_page->m_text = text;

  return TRUE;
};

bool wxNotebook::SetPageImage( int page, int image )
{
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (!nb_page)
    return FALSE;

  nb_page->m_image = image;

  return TRUE;
};

void wxNotebook::SetPageSize( const wxSize &WXUNUSED(size) )
{
  wxFAIL_MSG("wxNotebook::SetPageSize not implemented");
};

void wxNotebook::SetPadding( const wxSize &WXUNUSED(padding) )
{
  wxFAIL_MSG("wxNotebook::SetPadding not implemented");
};

bool wxNotebook::DeleteAllPages()
{
  wxNode *page_node = m_pages.First();
  while (page_node)
  {
    wxNotebookPage *page = (wxNotebookPage*)page_node->Data();

    DeletePage( page->m_id );

    page_node = m_pages.First();
  };

  return TRUE;
};

bool wxNotebook::DeletePage( int page )
{
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (!nb_page) return FALSE;

  int page_num = 0;
  GList *child = GTK_NOTEBOOK(m_widget)->children;
  while (child)
  {
    if (nb_page->m_page == (GtkNotebookPage*)child->data) break;
    page_num++;
    child = child->next;
  };

  wxASSERT( child );

  delete nb_page->m_clientPanel;

//  Amazingly, this is not necessary
//  gtk_notebook_remove_page( GTK_NOTEBOOK(m_widget), page_num );

  m_pages.DeleteObject( nb_page );

  return TRUE;
};

bool wxNotebook::AddPage(wxWindow* win, const wxString& text,
                         bool bSelect, int imageId)
{
  // we've created the notebook page in AddChild(). Now we just have to set
  // the caption for the page and set the others parameters.

  // first, find the page
  wxNotebookPage *page = NULL;

  wxNode *node = m_pages.First();
  while (node)
  {
    page = (wxNotebookPage*)node->Data();
    if ( page->m_clientPanel == win )
      break; // found
    node = node->Next();
  };

  wxCHECK_MSG(page != NULL, FALSE,
              "Can't add a page whose parent is not the notebook!");

  // then set the attributes
  page->m_text = text;
  if ( page->m_text.IsEmpty() )
    page->m_text = "";
  page->m_image = imageId;
  gtk_label_set(page->m_label, page->m_text);

  if ( bSelect ) {
    SetSelection(GetPageCount());
  }

  return TRUE;
};

wxWindow *wxNotebook::GetPage( int page ) const
{
  wxNotebookPage* nb_page = GetNotebookPage(page);
  if (!nb_page)
    return NULL;
  else
    return nb_page->m_clientPanel;
};

void wxNotebook::AddChild( wxWindow *win )
{
  // @@@ normally done in wxWindow::AddChild but for some reason wxNotebook
  // case is special there (Robert?)
  m_children.Append(win);

  wxNotebookPage *page = new wxNotebookPage();

  page->m_id = GetPageCount();
  page->m_label = (GtkLabel *)gtk_label_new("no caption");
  page->m_clientPanel = win;
  gtk_notebook_append_page(GTK_NOTEBOOK(m_widget), win->m_widget,
                           (GtkWidget *)page->m_label);
  gtk_misc_set_alignment(GTK_MISC(page->m_label), 0.0, 0.5);

  page->m_page = (GtkNotebookPage*)
                 (
                    g_list_last(GTK_NOTEBOOK(m_widget)->children)->data
                 );

  if (!page->m_page)
  {
     wxLogFatalError( "Notebook page creation error" );
     return;
  }

  m_pages.Append( page );
};

void wxNotebook::OnSize(wxSizeEvent& event)
{
  // forward this event to all pages
  wxNode *node = m_pages.First();
  while (node)
  {
    wxNotebookPage *page = (wxNotebookPage*)node->Data();
    // @@@@ This -50 is completely wrong - instead, we should substract
    //      the height of the tabs
    page->m_clientPanel->SetSize(event.GetSize().GetX(),
                                 event.GetSize().GetY() - 50);

    node = node->Next();
  };
}

//-----------------------------------------------------------------------------
// wxNotebookEvent
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxNotebookEvent, wxCommandEvent)
