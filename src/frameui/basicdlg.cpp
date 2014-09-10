#include "basicdlg.h"
#include "app.h"
#include "resource.h"

StaticFrame* BasicDialog::addTip(Frame* frame, wchar_t const* text)
{
  StaticFrame* tip = new StaticFrame(text, this);
  tip->setPoint(PT_RIGHT, frame, PT_LEFT, -5, -1);
  return tip;
}
BasicDialog::BasicDialog(Window* parent, wchar_t const* title, int width, int height)
{
  if (WNDCLASSEX* wcx = createclass(L"BasicWndClass"))
  {
    wcx->hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wcx->hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx->hIcon = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWND));
    RegisterClassEx(wcx);
  }
  int cxScreen = GetSystemMetrics(SM_CXSCREEN);
  int cyScreen = GetSystemMetrics(SM_CYSCREEN);
  create(cxScreen / 2 - width / 2, cyScreen / 2 - height / 2, width, height, title,
    WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU, WS_EX_CONTROLPARENT, parent->getHandle());
}
uint32 BasicDialog::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
    case WM_COMMAND:
    {
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      switch (id)
      {
      case IDOK:
        for (int i = 0; i < strings.length(); i++)
          *strings[i].second = strings[i].first->getText();
        endModal();
        break;
      case IDCANCEL:
        endModal();
        break;
      }
    }
    break;
  default:
    return M_UNHANDLED;
  }
  return 0;
}
