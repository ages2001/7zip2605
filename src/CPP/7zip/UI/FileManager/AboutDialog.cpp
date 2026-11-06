// AboutDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../MyVersion.h"

#include "../Common/LoadCodecs.h"

#include "AboutDialog.h"
#include "PropertyNameRes.h"

#include "HelpUtils.h"
#include "LangUtils.h"
#include "OriginalLogo.h"

extern HINSTANCE g_hInstance;

static WNDPROC g_OriginalLogoProc;
static LRESULT CALLBACK OriginalLogoProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_PAINT)
  {
    PAINTSTRUCT paint;
    HDC dc = ::BeginPaint(window, &paint);
    RECT bounds;
    HBRUSH brush = (HBRUSH)::SendMessage(::GetParent(window), WM_CTLCOLORSTATIC,
        (WPARAM)dc, (LPARAM)window);
    if (!brush) brush = (HBRUSH)(COLOR_BTNFACE + 1);
    ::GetClientRect(window, &bounds);
    ::FillRect(dc, &bounds, brush);
    DrawOriginalLogo(g_hInstance, IDI_LOGO, dc, 0, 0);
    ::EndPaint(window, &paint);
    return 0;
  }
  return ::CallWindowProc(g_OriginalLogoProc, window, message, wParam, lParam);
}

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDT_ABOUT_INFO
};
#endif

#define kHomePageURL TEXT("https://www.7-zip.org/")
#define kHelpTopic "start.htm"

#define LLL_(quote) L##quote
#define LLL(quote) LLL_(quote)

extern CCodecs *g_CodecsObj;

bool CAboutDialog::OnInit()
{
  HWND logo = ::GetDlgItem(*this, IDT_ABOUT_LOGO);
  // NT 3.1 does not implement owner-drawn static controls. Handle painting
  // in the control itself while retaining its original lifecycle procedure.
  g_OriginalLogoProc = (WNDPROC)::SetWindowLongPtr(logo, GWLP_WNDPROC,
      (LONG_PTR)OriginalLogoProc);
  ::SetWindowPos(logo, NULL, 0, 0, Z7_LOGO_WIDTH, Z7_LOGO_HEIGHT,
      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  #ifdef Z7_EXTERNAL_CODECS
  if (g_CodecsObj)
  {
    UString s;
    g_CodecsObj->GetCodecsErrorMessage(s);
    if (!s.IsEmpty())
      MessageBoxW(GetParent(), s, L"7-Zip", MB_ICONERROR);
  }
  #endif

  #ifdef Z7_LANG
  LangSetWindowText(*this, IDD_ABOUT);
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  #endif
  #ifdef Z7_LEGACY_WIN32
  SetItemText(IDT_ABOUT_VERSION, UString("7-Zip " MY_VERSION " (x86-32)"));
  #else
  SetItemText(IDT_ABOUT_VERSION, UString("7-Zip " MY_VERSION_CPU));
  #endif
  SetItemText(IDT_ABOUT_DATE, LLL(MY_DATE));
  
  NormalizePosition();
  return CModalDialog::OnInit();
}

void CAboutDialog::OnHelp()
{
  ShowHelpWindow(kHelpTopic);
}

bool CAboutDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  LPCTSTR url;
  switch (buttonID)
  {
    case IDB_ABOUT_HOMEPAGE: url = kHomePageURL; break;
    default:
      return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
  }

  #ifdef UNDER_CE
  SHELLEXECUTEINFO s;
  memset(&s, 0, sizeof(s));
  s.cbSize = sizeof(s);
  s.lpFile = url;
  ::ShellExecuteEx(&s);
  #else
  ::ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
  #endif

  return true;
}
