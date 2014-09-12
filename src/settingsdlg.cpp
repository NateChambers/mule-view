#include "settingsdlg.h"
#include "app.h"
#include "resource.h"
#include <shlobj.h>
#include "mainwnd.h"

#define IDC_ID2PATH       301
#define IDC_IMULEPATH     302
#define IDC_ID2BROWSE     303
#define IDC_IMULEBROWSE   304

StaticFrame* SettingsWnd::addTip(Frame* frame, wchar_t const* text)
{
  StaticFrame* tip = new StaticFrame(text, this);
  tip->setPoint(PT_RIGHT, frame, PT_LEFT, -5, -1);
  return tip;
}
SettingsWnd::SettingsWnd(Window* parent)
{
  if (WNDCLASSEX* wcx = createclass(L"SettingsWndClass"))
  {
    wcx->hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wcx->hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx->hIcon = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWND));
    RegisterClassEx(wcx);
  }
  int cxScreen = GetSystemMetrics(SM_CXSCREEN);
  int cyScreen = GetSystemMetrics(SM_CYSCREEN);
  create(cxScreen / 2 - 200, cyScreen / 2 - 65, 400, 130, L"Settings",
    WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU, WS_EX_CONTROLPARENT, parent->getHandle());

  iD2Path = new EditFrame(this, IDC_ID2PATH);
  iD2Path->setPoint(PT_TOPLEFT, 130, 8);
  iD2Path->setPoint(PT_RIGHT, -72, 0);
  iD2Path->setHeight(21);
  addTip(iD2Path, L"Diablo II path (optional):");
  if (cfg.getwstr("d2path")) iD2Path->setText(WideString::normpath(cfg.getwstr("d2path")));

  ButtonFrame* browseButton = new ButtonFrame(L"Browse", this, IDC_ID2BROWSE);
  browseButton->setSize(60, 21);
  browseButton->setPoint(PT_TOPLEFT, iD2Path, PT_TOPRIGHT, 4, 0);

  iMulePath = new EditFrame(this, IDC_IMULEPATH);
  iMulePath->setPoint(PT_TOPLEFT, iD2Path, PT_BOTTOMLEFT, 0, 6);
  iMulePath->setPoint(PT_RIGHT, -72, 0);
  iMulePath->setHeight(21);
  addTip(iMulePath, L"Mule folder:");
  if (cfg.getwstr("muleDir")) iMulePath->setText(WideString::normpath(cfg.getwstr("muleDir")));

  browseButton = new ButtonFrame(L"Browse", this, IDC_IMULEBROWSE);
  browseButton->setSize(60, 21);
  browseButton->setPoint(PT_TOPLEFT, iMulePath, PT_TOPRIGHT, 4, 0);

  ButtonFrame* okButton = new ButtonFrame(L"Ok", this, IDOK);
  okButton->setSize(100, 21);
  okButton->setPoint(PT_BOTTOMRIGHT, this, PT_BOTTOM, -4, -8);
  ButtonFrame* cancelButton = new ButtonFrame(L"Cancel", this, IDCANCEL);
  cancelButton->setSize(100, 21);
  cancelButton->setPoint(PT_BOTTOMLEFT, this, PT_BOTTOM, 4, -8);
}
SettingsWnd::~SettingsWnd()
{
}
uint32 SettingsWnd::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_COMMAND:
    {
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      switch (id)
      {
      case IDC_ID2BROWSE:
        {
          OPENFILENAME ofn;
          memset(&ofn, 0, sizeof ofn);
          ofn.lStructSize = sizeof ofn;
          ofn.hwndOwner = hWnd;
          ofn.lpstrFilter = L"d2data.mpq\0d2data.mpq;resources.mpq\0\0";
          wchar_t buf[512];
          wcscpy(buf, iD2Path->getText());
          ofn.lpstrFile = buf;
          ofn.nMaxFile = sizeof buf;
          ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
          if (GetOpenFileName(&ofn))
            iD2Path->setText(WideString::getPath(buf));
          else
          {
            buf[0] = 0;
            if (GetOpenFileName(&ofn))
              iD2Path->setText(WideString::getPath(buf));
          }
        }
        break;
      case IDC_IMULEBROWSE:
        {
          BROWSEINFO bi;
          memset(&bi, 0, sizeof bi);
          bi.lpszTitle = L"Mule Folder";
          bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON | BIF_RETURNONLYFSDIRS;
          ITEMIDLIST* list = SHBrowseForFolder(&bi);
          if (list == NULL)
            break;
          wchar_t buf[MAX_PATH];
          SHGetPathFromIDList(list, buf);
          iMulePath->setText(buf);

          LPMALLOC ml;
          SHGetMalloc(&ml);
          ml->Free(list);
          ml->Release();
        }
        break;
      case IDOK:
        cfg.setwstr("d2path", iD2Path->getText());
        cfg.setwstr("muleDir", iMulePath->getText());
        PostMessage(getApp()->getMainWindow(), WM_UPDATEDIR, 0, 0);
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

uint32 AboutDlg::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_COMMAND && LOWORD(wParam) == IDOK)
    endModal();
  return M_UNHANDLED;
}
AboutDlg::AboutDlg(Window* parent)
{
  if (WNDCLASSEX* wcx = createclass(L"AboutWndClass"))
  {
    wcx->hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wcx->hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx->hIcon = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWND));
    RegisterClassEx(wcx);
  }

  RECT rc = {0, 0, 300, 60};
  D2Data* d2data = getApp()->getD2Data();
  LocalPtr<D2Image> d2img = d2data->getImage("data\\global\\ui\\CharSelect\\CreateNewCharImage.dc6");
  if (d2img) rc.bottom = d2img->height() + 16;
  AdjustWindowRectEx(&rc, WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU, FALSE, WS_EX_CONTROLPARENT);

  int cxScreen = GetSystemMetrics(SM_CXSCREEN);
  int cyScreen = GetSystemMetrics(SM_CYSCREEN);
  int wWidth = rc.right - rc.left;
  int wHeight = rc.bottom - rc.top;
  create((cxScreen - wWidth) / 2, (cyScreen - wHeight) / 2, wWidth, wHeight, L"About MuleView",
    WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU, WS_EX_CONTROLPARENT, parent->getHandle());

  StaticFrame* imgFrame = NULL;
  if (d2img)
  {
    uint32 palette[256];
    d2data->getPalette(palette, "data\\global\\palette\\Units\\pal.dat");
    LocalPtr<Image> image = d2img->image(palette);
    imgFrame = new StaticFrame(this, 0, SS_BITMAP | SS_CENTERIMAGE);
    imgFrame->setPoint(PT_TOPLEFT, 8, 8);
    imgFrame->setSize(image->width(), image->height());
    HDC hDC = GetDC(imgFrame->getHandle());
    HBITMAP hBitmap = image->createBitmap(hDC);
    ReleaseDC(imgFrame->getHandle(), hDC);
    imgFrame->setImage(hBitmap);
  }

  StaticFrame* textFrame = new StaticFrame(L"MuleView v0.2 © RivSoft 2014", this);
  if (imgFrame)
    textFrame->setPoint(PT_TOPLEFT, imgFrame, PT_TOPRIGHT, 16, 0);
  else
    textFrame->setPoint(PT_TOPLEFT, 8, 8);

  ButtonFrame* okButton = new ButtonFrame(L"Ok", this, IDOK);
  okButton->setSize(120, 21);
  okButton->setPoint(PT_BOTTOM, imgFrame ? d2img->width() / 2 : 0, -8);
};
