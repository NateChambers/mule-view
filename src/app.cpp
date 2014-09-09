#include "app.h"
#include <commctrl.h>
#include <afxres.h>

#include "base/utils.h"
#include "base/dictionary.h"
#include "base/args.h"
//#include "resource.h"
#include "frameui/image.h"
#include "base/file.h"
#include "base/ptr.h"
#include "rmpq/rmpq2.h"

#include "mainwnd.h"
#include "d2data.h"

Application* Application::instance = NULL;

Application::Application(HINSTANCE _hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  srand(GetTickCount());
  instance = this;
  mainWindow = NULL;
  hInstance = _hInstance;

  root = WideString::getPath(getAppPath());

  d2data = new D2Data();

  INITCOMMONCONTROLSEX iccex;
  iccex.dwSize = sizeof iccex;
  iccex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS |
      ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES |
      ICC_TAB_CLASSES | ICC_UPDOWN_CLASS | ICC_DATE_CLASSES |
      ICC_HOTKEY_CLASS;
  InitCommonControlsEx(&iccex);
  LoadLibrary(L"Riched20.dll");
  OleInitialize(NULL);

  mainWindow = new MainWnd(d2data);
  ShowWindow(mainWindow->getHandle(), SW_SHOW);
}
Application::~Application()
{
  instance = NULL;
  delete mainWindow;
  delete d2data;
  OleFlushClipboard();
  OleUninitialize();
}

int Application::run()
{
  if (mainWindow)
  {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
      if (!IsDialogMessage(mainWindow->getHandle(), &msg))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    return msg.wParam;
  }
  else
    return 0;
}

HWND Application::getMainWindow() const
{
  return (mainWindow ? mainWindow->getHandle() : NULL);
}
