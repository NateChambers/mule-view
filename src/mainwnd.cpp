#include "mainwnd.h"
#include "resource.h"
#include "app.h"
#include "settingsdlg.h"
#include "imgur.h"
#include <windowsx.h>

#define IDC_TREEMULES         100
#define IDC_TREEITEMS         101

MainWnd::WndSettings MainWnd::settings = {
  {200, 200}, // splitters
  SW_SHOW,
  CW_USEDEFAULT, 0,
  CW_USEDEFAULT, 0,
};

MainWnd::MainWnd(D2Data* _d2data)
  : d2data(_d2data)
  , muleRoot(L"All characters")
  , dragPos(0)
  , dragIndex(-1)
  , loaded(false)
  , updating(false)
{
  cfg.get("wndSettings", settings);

  hVSplit = LoadCursor(getInstance(), MAKEINTRESOURCE(IDC_VSPLIT));

  if (WNDCLASSEX* wcx = createclass(L"MainWndClass"))
  {
    wcx->hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wcx->hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx->hIcon = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWND));
    RegisterClassEx(wcx);
  }
  create(CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, L"MuleView", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, WS_EX_CONTROLPARENT);

  treeMules = new TreeViewFrame(this, IDC_TREEMULES, TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_HSCROLL | WS_TABSTOP);
  treeItems = new TreeViewFrame(this, IDC_TREEITEMS, TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_HSCROLL | WS_TABSTOP);
  itemList = new ItemList(d2data, this);

  TVINSERTSTRUCT tvis;
  memset(&tvis, 0, sizeof tvis);
  tvis.hParent = TVI_ROOT;
  tvis.hInsertAfter = TVI_LAST;
  tvis.item.mask = TVIF_PARAM | TVIF_TEXT | TVIF_STATE;
  tvis.item.lParam = (LPARAM) d2data->getItemRoot();
  tvis.item.pszText = L"All Items";
  allItemsRoot = treeItems->insertItem(&tvis);

  treeMules->setPoint(PT_TOPLEFT, 0, 0);
  treeMules->setPoint(PT_BOTTOM, 0, 0);
  treeMules->setWidth(settings.splitter[0]);

  treeItems->setPoint(PT_TOPLEFT, treeMules, PT_TOPRIGHT, 4, 0);
  treeItems->setPoint(PT_BOTTOM, 0, 0);
  treeItems->setWidth(settings.splitter[1]);

  itemList->setPoint(PT_TOPLEFT, treeItems, PT_TOPRIGHT, 4, 0);
  itemList->setPoint(PT_BOTTOMRIGHT, 0, 0);

  WINDOWPLACEMENT pl;
  memset(&pl, 0, sizeof pl);
  pl.length = sizeof pl;
  GetWindowPlacement(hWnd, &pl);
  pl.flags = 0;
  pl.showCmd = settings.wndShow;
  if (settings.wndX != CW_USEDEFAULT)
  {
    pl.rcNormalPosition.left = settings.wndX;
    pl.rcNormalPosition.top = settings.wndY;
  }
  if (settings.wndWidth != CW_USEDEFAULT)
  {
    pl.rcNormalPosition.right = settings.wndWidth + pl.rcNormalPosition.left;
    pl.rcNormalPosition.bottom = settings.wndHeight + pl.rcNormalPosition.top;
  }
  SetWindowPlacement(hWnd, &pl);
  loaded = true;

  doUpdate();
}
MainWnd::~MainWnd()
{
  cfg.set("wndSettings", settings);
}
uint32 MainWnd::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_UPDATEDIR:
    doUpdate();
    break;
  case WM_NOTIFY:
    if (wParam == IDC_TREEMULES)
    {
      NMTREEVIEW* hdr = (NMTREEVIEW*) lParam;
      if (hdr->hdr.code == TVN_SELCHANGED)
      {
        onSetMule((D2Container*) hdr->itemNew.lParam);
        return TRUE;
      }
    }
    if (wParam == IDC_TREEITEMS)
    {
      NMTREEVIEW* hdr = (NMTREEVIEW*) lParam;
      if (hdr->hdr.code == TVN_SELCHANGED && !updating)
      {
        d2data->clearItemFlags(D2ItemType::fDisplayed);
        updateSelectedTypes(hdr->itemNew.lParam ? (D2ItemType*) hdr->itemNew.lParam : d2data->getItemRoot());
        itemList->updateSelected();
        return TRUE;
      }
    }
    break;
  case WM_SIZE:
  case WM_MOVE:
    if (loaded)
    {
      WINDOWPLACEMENT pl;
      memset(&pl, 0, sizeof pl);
      pl.length = sizeof pl;
      GetWindowPlacement(hWnd, &pl);
      if (pl.showCmd != SW_SHOWMINIMIZED && pl.showCmd != SW_HIDE)
        settings.wndShow = pl.showCmd;
      if (pl.showCmd == SW_SHOWNORMAL)
      {
        settings.wndX = pl.rcNormalPosition.left;
        settings.wndY = pl.rcNormalPosition.top;
        settings.wndWidth = pl.rcNormalPosition.right - pl.rcNormalPosition.left;
        settings.wndHeight = pl.rcNormalPosition.bottom - pl.rcNormalPosition.top;
      }
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  // splitter
  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT)
    {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      if ((pt.x >= treeMules->right() && pt.x < treeItems->left()) ||
          (pt.x >= treeItems->right() && pt.x < itemList->left()))
      {
        SetCursor(hVSplit);
        return TRUE;
      }
    }
    break;
  case WM_LBUTTONDOWN:
    {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      if (pt.x >= treeMules->right() && pt.x < treeItems->left())
      {
        SetCapture(hWnd);
        dragPos = pt.x;
        dragIndex = 0;
        return 0;
      }
      if (pt.x >= treeItems->right() && pt.x < itemList->left())
      {
        SetCapture(hWnd);
        dragPos = pt.x;
        dragIndex = 1;
        return 0;
      }
    }
    break;
  case WM_MOUSEMOVE:
    if ((wParam & MK_LBUTTON) && GetCapture() == hWnd && dragIndex >= 0)
    {
      int pos = GET_X_LPARAM(lParam);
      settings.splitter[dragIndex] += pos - dragPos;
      dragPos = pos;
      RECT rc;
      GetClientRect(hWnd, &rc);
      int oldSplitterPos = settings.splitter[dragIndex];
      if (settings.splitter[dragIndex] < 2) settings.splitter[dragIndex] = 2;
      if (settings.splitter[0] + settings.splitter[1] > rc.right - 100)
        settings.splitter[dragIndex] = rc.right - 100 - settings.splitter[1 - dragIndex];
      dragPos += settings.splitter[dragIndex] - oldSplitterPos;
      if (dragIndex == 0)
        treeMules->setWidth(settings.splitter[0]);
      else
        treeItems->setWidth(settings.splitter[1]);
      return 0;
    }
    break;
  case WM_LBUTTONUP:
    if (GetCapture() == hWnd)
      ReleaseCapture();
    break;
  }
  return M_UNHANDLED;
}

void MainWnd::updateMules(HTREEITEM hItem, D2Container* list)
{
  for (int i = 0; i < list->sub.length(); i++)
  {
    D2Container* cur = list->sub[i];
    if (!cur->hItem)
    {
      TVINSERTSTRUCT tvis;
      memset(&tvis, 0, sizeof tvis);
      tvis.hParent = hItem;
      tvis.hInsertAfter = (i == 0 ? TVI_FIRST : (HTREEITEM) list->sub[i - 1]->hItem);
      tvis.item.mask = TVIF_PARAM | TVIF_TEXT;
      tvis.item.lParam = (LPARAM) cur;
      tvis.item.pszText = cur->name.getBuffer();
      cur->hItem = treeMules->insertItem(&tvis);
    }
    updateMules((HTREEITEM) cur->hItem, cur);
  }
}
void MainWnd::updateSelectedMules(D2Container* list)
{
  for (int i = 0; i < list->sub.length(); i++)
    updateSelectedMules(list->sub[i]);
  for (int i = 0; i < list->items.length(); i++)
    if (list->items[i]->base)
      list->items[i]->base->type->flags |= D2ItemType::fNonEmpty;
}
void MainWnd::nonEmptyDFS(D2ItemType* type)
{
  if (type->flags & D2ItemType::fVisited)
    return;
  type->flags |= D2ItemType::fVisited;
  for (int i = 0; i < type->sub.length(); i++)
  {
    nonEmptyDFS(type->sub[i]);
    if (type->sub[i]->flags & D2ItemType::fNonEmpty)
      type->flags |= D2ItemType::fNonEmpty;
  }
}
void MainWnd::updateItems(HTREEITEM hItem, D2ItemType* type)
{
  HTREEITEM prev = TVI_FIRST;
  HTREEITEM next = TreeView_GetChild(treeItems->getHandle(), hItem);
  D2ItemType* nextType = (next ? (D2ItemType*) treeItems->getItemData(next) : NULL);
  for (int i = 0; i < type->sub.length(); i++)
  {
    D2ItemType* cur = type->sub[i];
    if (!(cur->flags & D2ItemType::fNonEmpty))
      continue;
    while (nextType && nextType < cur)
    {
      HTREEITEM temp = TreeView_GetNextSibling(treeItems->getHandle(), next);
      treeItems->deleteItem(next);
      next = temp;
      nextType = (next ? (D2ItemType*) treeItems->getItemData(next) : NULL);
    }
    if (nextType != cur)
    {
      TVINSERTSTRUCT tvis;
      memset(&tvis, 0, sizeof tvis);
      tvis.hParent = hItem;
      tvis.hInsertAfter = prev;
      tvis.item.mask = TVIF_PARAM | TVIF_TEXT;
      tvis.item.lParam = (LPARAM) cur;
      WideString wideName(cur->name);
      tvis.item.pszText = wideName.getBuffer();
      next = treeItems->insertItem(&tvis);
    }
    updateItems(next, cur);
    prev = next;
    next = TreeView_GetNextSibling(treeItems->getHandle(), next);
    nextType = (next ? (D2ItemType*) treeItems->getItemData(next) : NULL);
  }
  while (next)
  {
    HTREEITEM temp = TreeView_GetNextSibling(treeItems->getHandle(), next);
    treeItems->deleteItem(next);
    next = temp;
  }
}
void MainWnd::onSetMule(D2Container* mule)
{
  if (!mule) mule = &muleRoot;
  d2data->clearItemFlags(D2ItemType::fVisited | D2ItemType::fNonEmpty | D2ItemType::fDisplayed);
  updateSelectedMules(mule);
  nonEmptyDFS(d2data->getItemRoot());
  updating = true;
  updateItems(allItemsRoot, d2data->getItemRoot());
  updating = false;

  HTREEITEM typeSel = TreeView_GetSelection(treeItems->getHandle());
  D2ItemType* selType = (typeSel ? (D2ItemType*) treeItems->getItemData(typeSel) : NULL);
  if (!selType) selType = d2data->getItemRoot();
  updateSelectedTypes(selType);

  itemList->setMule(mule);
}
void MainWnd::updateSelectedTypes(D2ItemType* type)
{
  if (type->flags & D2ItemType::fDisplayed)
    return;
  type->flags |= D2ItemType::fDisplayed;
  for (int i = 0; i < type->sub.length(); i++)
    updateSelectedTypes(type->sub[i]);
}
void MainWnd::doUpdate()
{
  if (!cfg.getwstr("muleDir"))
  {
    SettingsWnd settings(this);
    settings.doModal();
  }
  if (muleRoot.parseDir(cfg.getwstr("muleDir"), d2data))
  {
    updateMules(TVI_ROOT, &muleRoot);

    HTREEITEM sel = TreeView_GetSelection(treeMules->getHandle());
    onSetMule(sel ? (D2Container*) treeMules->getItemData(sel) : NULL);
  }
}
