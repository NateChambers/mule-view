#include "itemlist.h"
#include "frameui/fontsys.h"
#include <windowsx.h>
#include "resource.h"
#include "app.h"
#include "frameui/basicdlg.h"
#include "imgur.h"
#include "frameui/dragdrop.h"

#define TYPE_GAP        20
#define GROUP_GAP       6

uint32 ItemTooltip::onWndMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_PAINT)
  {
    PAINTSTRUCT ps;
    RECT rc;
    GetClientRect(hWnd, &rc);

    HDC hDC = BeginPaint(hWnd, &ps);

    BitBlt(hDC, 0, 0, rc.right, imageHeight, dc, 0, 0, SRCCOPY);
    if (header.length())
    {
      SelectObject(hDC, FontSys::getSysFont());
      rc.top = imageHeight;
      DrawText(hDC, header, header.length(), &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    EndPaint(hWnd, &ps);

    return 0;
  }
  return Window::onWndMessage(message, wParam, lParam);
}
ItemTooltip::ItemTooltip(HWND hParent, int x, int y, Image* image, char const* _header)
  : header(_header ? _header : "")
  , imageHeight(image->height())
{
  if (WNDCLASSEX* wcx = createclass(L"ItemTooltip"))
  {
    wcx->style = CS_DROPSHADOW;
    wcx->hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    RegisterClassEx(wcx);
  }
  POINT pt = {x, y};
  ClientToScreen(hParent, &pt);
  int scrWidth = GetSystemMetrics(SM_CXSCREEN);
  int scrHeight = GetSystemMetrics(SM_CYSCREEN);
  //if (pt.x + image->width() > scrWidth)
  //  pt.x = scrWidth - image->width();
  int height = image->height();
  if (_header && _header[0])
    height += FontSys::getTextSize(FontSys::getSysFont(), header).cy;
  if (pt.y + height > scrHeight)
    pt.y = scrHeight - height;
  create(pt.x, pt.y, image->width(), height, L"",
    WS_POPUP, WS_EX_TOOLWINDOW, NULL);
  HDC hDC = GetDC(hWnd);
  dc = CreateCompatibleDC(hDC);
  bitmap = image->createBitmap(hDC);
  SelectObject(dc, bitmap);
  ReleaseDC(hWnd, hDC);
}
ItemTooltip::~ItemTooltip()
{
  DeleteDC(dc);
  DeleteObject(bitmap);
}

template<>
class Hash<ItemList::ItemGroup>
{
public:
  static uint32 hash(ItemList::ItemGroup const& group, uint32 initval = 0)
  {
    initval = hashlittle(&group.type, sizeof group.type, initval);
    initval = hashlittle(&group.unique, sizeof group.unique, initval);
    if (!group.unique)
      initval = hashlittle(&group.base, sizeof group.base, initval);
    initval = hashlittle(&group.quality, sizeof group.quality, initval);
    if (group.quality == D2Item::qMagic)
      initval = hashlittle(group.title.c_str(), group.title.length(), initval);
    return initval;
  }
  static int compare(ItemList::ItemGroup const& a, ItemList::ItemGroup const& b)
  {
    if (a.type != b.type) return a.type - b.type;
    if (a.unique != b.unique) return int(a.unique) - int(b.unique);
    if (!a.unique && a.base != b.base) return int(a.base) - int(b.base);
    if (a.quality != b.quality) return a.quality - b.quality;
    if (a.quality == D2Item::qMagic)
      return a.title.icompare(b.title);
    return 0;
  }
};

void ItemList::updateSelected(bool reset)
{
  HFONT hFont = FontSys::getSysFont();
  HashMap<ItemGroup, int> openGroups;
  if (groups.length() > 1)
  {
    for (int i = 0; i < groups.length(); i++)
    {
      if (groups[i].quality != D2Item::qRare && groups[i].quality != D2Item::qCrafted)
        openGroups.set(groups[i], groups[i].state);
    }
  }
  groups.clear();
  int cg = -1;
  int y = 10;
  for (int i = 0; i < allItems.length(); i++)
  {
    if (allItems[i]->unique && !strcmp(allItems[i]->unique->name, "Rainbow Facet"))
      int asdf = 0;
    if (allItems[i]->base && allItems[i]->base->type->flags & D2ItemType::fDisplayed)
    {
      D2Item* item = allItems[i];
      if (cg < 0 || item->type   != groups[cg].type                   ||
                    item->unique != groups[cg].unique                 ||
                    (!item->unique && item->base != groups[cg].base)  ||
                    item->quality != groups[cg].quality               ||
                    (item->quality == D2Item::qMagic && item->title.icompare(groups[cg].title)) ||
                    item->quality == D2Item::qRare || item->quality == D2Item::qCrafted)
      {
        if (cg >= 0)
        {
          if (item->type != groups[cg].type)
            y += TYPE_GAP;
          y += (groups[cg].state ? groups[cg].hopen : groups[cg].hclosed);
        }
        groups.push(); cg++;
        groups[cg].type = item->type;
        groups[cg].state = -1;
        groups[cg].hopen = 0;
        groups[cg].ypos = y;
        groups[cg].title = item->title;
        groups[cg].unique = item->unique;
        groups[cg].base = item->base;
        groups[cg].quality = item->quality;
        if (item->unique)
          groups[cg].wclosed = FontSys::getTextSize(hFont, WideString(item->unique->name)).cx;
        else
          groups[cg].wclosed = 0;
      }
      ItemData& data = groups[cg].items.push();
      data.group = cg;
      data.item = item;
      SIZE sz = FontSys::getTextSize(hFont, WideString(item->title));
      data.rc.left = 24;
      data.rc.top = groups[cg].hopen;
      data.rc.right = data.rc.left + sz.cx;
      data.rc.bottom = data.rc.top + sz.cy;
      int sox = 0;
      for (int i = 0; i < item->socketItems.length(); i++)
        if (item->socketItems[i])
          sox++;
      data.state = (sox ? 0 : -1);
      groups[cg].hopen += sz.cy;
      if (groups[cg].items.length() == 1)
        groups[cg].hclosed = sz.cy;
      else
        groups[cg].state = 0;
    }
  }

  if (cg == 0)
    groups[cg].state = 1;
  if (groups.length())
    y += (groups[cg].state ? groups[cg].hopen : groups[cg].hclosed);
  for (int i = 0; i < groups.length(); i++)
  {
    if (groups[i].state != -1 && openGroups.has(groups[i]))
    {
      int state = openGroups.get(groups[i]);
      if (state != -1 && state != groups[i].state)
        y += toggleGroup(i);
    }
  }

  RECT rc;
  GetClientRect(hWnd, &rc);
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_RANGE | SIF_PAGE;
  si.nMin = 0;
  si.nMax = y + 9;
  si.nPage = rc.bottom;
  if (reset)
  {
    si.fMask |= SIF_POS;
    si.nPos = 0;
    scrollPos = scrollAccum = 0;
  }
  SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
  doScroll(scrollPos);

  invalidate();
}
int ItemList::itemCompare(D2Item* const& a, D2Item* const& b)
{
  if (a->type != b->type)
    return a->type - b->type;
  if (a->subType != b->subType)
    return a->subType - b->subType;
  if (!a->base || !b->base)
    return 0;
  if (a->unique != b->unique)
    return int(a->unique) - int(b->unique);
  if (a->base->type != b->base->type)
    return int(a->base->type) - int(b->base->type);
  if (a->base != b->base)
    return int(a->base) - int(b->base);
  if (a->quality != b->quality)
    return a->quality - b->quality;
  int cmp = a->title.icompare(b->title);
  if (cmp) return cmp;
  cmp = String::smartCompare(a->statText, b->statText);
  if (cmp) return -cmp;
  return String::smartCompare(a->header, b->header);
}
ItemList::ItemData* ItemList::getItem(int x, int y, int* zone)
{
  y += scrollPos;
  int left = 0;
  int right = groups.length();
  while (left < right)
  {
    int mid = (left + right) / 2;
    if (y >= groups[mid].ypos && y < groups[mid].ypos + (groups[mid].state ? groups[mid].hopen : groups[mid].hclosed))
    {
      y -= groups[mid].ypos;
      Array<ItemData>& items = groups[mid].items;
      int ileft = 0;
      int iright = (groups[mid].state ? items.length() : 1);
      while (ileft < iright)
      {
        int imid = (ileft + iright) / 2;
        int rcbottom = items[imid].rc.bottom;
        if (items[imid].state > 0)
          rcbottom += (items[imid].rc.bottom - items[imid].rc.top) * items[imid].state;
        if (y >= items[imid].rc.top && y < rcbottom)
        {
          int index = (y - items[imid].rc.top) / (items[imid].rc.bottom - items[imid].rc.top);
          RECT rc = items[imid].rc;
          if (index)
          {
            int oindex = index - 1; index = 0;
            while (oindex >= 0 && index < items[imid].item->socketItems.length())
            {
              if (items[imid].item->socketItems[index])
                oindex--;
              if (oindex >= 0)
                index++;
            }
            rc.left += 12;
            rc.right = rc.left + FontSys::getTextSize(FontSys::getSysFont(),
              WideString(items[imid].item->socketItems[index]->title)).cx;
            index++;
          }
          else if (groups[mid].state == 0 && groups[mid].unique)
            rc.right = items[imid].rc.left + groups[mid].wclosed;
          if (x >= rc.left && x < rc.right)
            *zone = -index;
          else if (imid == 0 && x >= 11 && x < 20 && y >= items[imid].rc.top + 3 && y < items[imid].rc.top + 12 &&
              groups[mid].state >= 0)
            *zone = 1;
          else
            return NULL;
          return &items[imid];
        }
        if (y < items[imid].rc.top)
          iright = imid;
        else
          ileft = imid + 1;
      }
      return NULL;
    }
    if (y < groups[mid].ypos)
      right = mid;
    else
      left = mid + 1;
  }
  return NULL;
}
void ItemList::setTooltip(D2Item* item, int x, int y)
{
  if (item != tooltipItem)
  {
    delete tooltip;
    tooltip = NULL;
    if (item)
    {
      tooltipImage = d2data->renderItem(item);
      tooltip = new ItemTooltip(hWnd, x, y, tooltipImage, item->header);
      ShowWindow(tooltip->getHandle(), SW_SHOWNA);
    }
    tooltipItem = item;
  }
}
int ItemList::toggleItem(ItemData* item)
{
  int cg = item->group;
  int index = 0;
  while (index < groups[cg].items.length() && item->item != groups[cg].items[index].item)
    index++;
  if (index >= groups[cg].items.length())
    return 0;
  if (!groups[cg].state)
    return 0;
  if (item->state < 0)
    return 0;

  item->state = !item->state;
  int delta = 0;
  for (int i = 0; i < item->item->socketItems.length(); i++)
    if (item->item->socketItems[i])
      delta++;
  if (item->state)
    item->state = delta;
  delta *= (item->rc.bottom - item->rc.top);
  if (!item->state)
    delta = -delta;
  while (++index < groups[cg].items.length())
  {
    groups[cg].items[index].rc.top += delta;
    groups[cg].items[index].rc.bottom += delta;
  }
  groups[cg].hopen += delta;
  while (++cg < groups.length())
    groups[cg].ypos += delta;
  return delta;
}
int ItemList::toggleGroup(int cg)
{
  int ogroup = cg;
  int delta = groups[cg].hopen - groups[cg].hclosed;
  groups[cg].state = !groups[cg].state;
  if (groups[cg].state)
  {
    if (cg > 0 && groups[cg - 1].type == groups[cg].type && groups[cg - 1].state != 1)
      groups[cg].ypos += GROUP_GAP, delta += GROUP_GAP;
  }
  else
  {
    delta = -delta;
    if (cg > 0 && groups[cg - 1].type == groups[cg].type && groups[cg - 1].state != 1)
      groups[cg].ypos -= GROUP_GAP, delta -= GROUP_GAP;
  }
  while (++cg < groups.length())
  {
    if (cg == ogroup + 1 && groups[cg].type == groups[cg - 1].type && groups[cg].state != 1)
      delta += (groups[cg - 1].state ? GROUP_GAP : -GROUP_GAP);
    groups[cg].ypos += delta;
  }
  return delta;
}

ItemList::ItemList(D2Data* data, Frame* parent)
  : WindowFrame(parent)
  , d2data(data)
  , tooltip(NULL)
  , tooltipItem(NULL)
  , scrollPos(0)
  , scrollAccum(0)
{
  stateIcons[0] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_TPLUS), IMAGE_ICON, 16, 16, 0);
  stateIcons[1] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_TMINUS), IMAGE_ICON, 16, 16, 0);

  if (WNDCLASSEX* wcx = createclass(L"ItemListClass"))
  {
    wcx->hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wcx->hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(wcx);
  }
  create(L"", WS_CHILD | WS_VSCROLL, WS_EX_CLIENTEDGE);
}
uint32 ItemList::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = BeginPaint(hWnd, &ps);

      RECT wrc;
      GetClientRect(hWnd, &wrc);

      HFONT hFont = FontSys::getSysFont();
      HFONT hItFont = FontSys::changeFlags(FONT_ITALIC, hFont);
      SelectObject(hDC, hFont);

      int left = 0;
      int right = groups.length() - 1;
      while (left < right)
      {
        int mid = (left + right) / 2;
        if (groups[mid].ypos + (groups[mid].state ? groups[mid].hopen : groups[mid].hclosed)
            <= ps.rcPaint.top + scrollPos)
          left = mid + 1;
        else
          right = mid;
      }

      int first = left;

      left = 0;
      right = groups.length() - 1;
      while (left < right)
      {
        int mid = (left + right) / 2;
        if (groups[mid].ypos < ps.rcPaint.bottom + scrollPos)
          left = mid + 1;
        else
          right = mid;
      }

      int last = right;

      static uint32 textColors[] = {
        0x000000, // 0xFFFFFF, white
        0x0000AA, // 0xFF4D4D, broken
        0x008000, // 0x00FF00, set
        0xFF0000, // 0x6969FF, magic
        0x13458B, // 0xC7B377, unique
        0x484848, // 0x696969, gray
        0x13458B, // 0x000000, (runewords.. not really)
        0x3E6168, // 0xD0C27D, no idea
        0x0066AA, // 0xFFA800, crafted
        0x338888, // 0xFFFF64, rare
        0x004000, // 0x008000, no idea
        0x800057, // 0xAE00FF, no idea
        0x006400, // 0x00C800, no idea
      };

      HPEN hPen = CreatePen(PS_SOLID, 1, 0xA5A5A5);
      SelectObject(hDC, hPen);

      for (int i = first; i <= last; i++)
      {
        int ypos = groups[i].ypos - scrollPos;
        int count = (groups[i].state ? groups[i].items.length() : 1);
        if (groups[i].state == 1)
        {
          MoveToEx(hDC, 16, ypos + 8, NULL);
          LineTo(hDC, 16, ypos + groups[i].items[count - 1].rc.bottom - 2);
          LineTo(hDC, 20, ypos + groups[i].items[count - 1].rc.bottom - 2);
        }
        for (int j = 0; j < count; j++)
        {
          ItemData* item = &groups[i].items[j];
          if (item->rc.bottom + ypos <= ps.rcPaint.top)
            continue;
          if (item->rc.top + ypos >= ps.rcPaint.bottom)
            break;

          WideString text(item->item->title);
          RECT rc = item->rc;
          if (groups[i].state == 0 && groups[i].unique)
          {
            text = WideString(groups[i].unique->name);
            rc.right = rc.left + groups[i].wclosed;
          }
          rc.top += ypos; rc.bottom += ypos;
          SetTextColor(hDC, textColors[item->item->colorCode]);
          DrawText(hDC, text, text.length(), &rc, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
          RECT orc = rc;

          if (groups[i].state == 0)
          {
            rc.left += FontSys::getTextSize(hFont, text).cx;
            text = WideString::format(L" x%d", groups[i].items.length());
            rc.right = rc.left + 200;
            SetTextColor(hDC, textColors[0]);
            DrawText(hDC, text, text.length(), &rc, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
          }
          else
          {
            if (item->item->statText.length())
            {
              rc.left += FontSys::getTextSize(hFont, text).cx + 5;
              rc.right = wrc.right;
              SetTextColor(hDC, textColors[0]);
              SelectObject(hDC, hItFont);
              DrawText(hDC, WideString(item->item->statText), -1, &rc,
                DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
              SelectObject(hDC, hFont);
            }
            if (item->state > 0)
            {
              int height = rc.bottom - rc.top;
              MoveToEx(hDC, orc.left + 4, orc.bottom, NULL);
              LineTo(hDC, orc.left + 4, orc.bottom + item->state * height - 2);
              LineTo(hDC, orc.left + 8, orc.bottom + item->state * height - 2);

              rc = orc;
              rc.left += 12;
              for (int s = 0; s < item->item->socketItems.length(); s++)
              {
                D2Item* sub = item->item->socketItems[s];
                if (!sub) continue;
                rc.top += height; rc.bottom += height;
                WideString text(sub->title);
                rc.right = rc.left + FontSys::getTextSize(hFont, text).cx;
                SetTextColor(hDC, textColors[sub->colorCode]);
                DrawText(hDC, text, text.length(), &rc, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
                if (sub->statText.length())
                {
                  RECT trc = rc;
                  trc.left = trc.right + 5;
                  trc.right = wrc.right;
                  SetTextColor(hDC, textColors[0]);
                  SelectObject(hDC, hItFont);
                  DrawText(hDC, WideString(sub->statText), -1, &trc,
                    DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
                  SelectObject(hDC, hFont);
                }
              }
            }
          }
          if (groups[i].state >= 0 && j == 0)
            DrawIconEx(hDC, 8, orc.top, stateIcons[groups[i].state], 16, 16, 0, NULL, DI_IMAGE | DI_MASK);
        }
      }

      DeleteObject(hPen);

      EndPaint(hWnd, &ps);
    }
    return 0;

  case WM_SIZE:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_PAGE;
      si.nPage = HIWORD(lParam);
      SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
      doScroll(scrollPos);
    }
    break;
  case WM_VSCROLL:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_ALL;
      GetScrollInfo(hWnd, SB_VERT, &si);
      switch (LOWORD(wParam))
      {
      case SB_TOP:
        si.nPos = si.nMin;
        break;
      case SB_BOTTOM:
        si.nPos = si.nMax;
        break;
      case SB_LINEUP:
        si.nPos -= 16;
        break;
      case SB_LINEDOWN:
        si.nPos += 16;
        break;
      case SB_PAGEUP:
        si.nPos -= si.nPage;
        break;
      case SB_PAGEDOWN:
        si.nPos += si.nPage;
        break;
      case SB_THUMBTRACK:
        si.nPos = si.nTrackPos;
        break;
      }
      doScroll(si.nPos);
      SetFocus(hWnd);
    }
    return 0;
  case WM_MOUSEWHEEL:
    {
      int step;
      SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &step, 0);
      if (step < 0)
        step = 3;
      scrollAccum += GET_WHEEL_DELTA_WPARAM(wParam) * step * 16;
      doScroll(scrollPos - scrollAccum / WHEEL_DELTA);
      scrollAccum %= WHEEL_DELTA;
    }
    return 0;

  case WM_RBUTTONDOWN:
    {
      SetFocus(hWnd);
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);
      int zone;
      ItemData* item = getItem(x, y, &zone);
      if (!item || zone != 0)
        break;
      HMENU hMenu = CreatePopupMenu();
      MENUITEMINFO mii;
      memset(&mii, 0, sizeof mii);
      mii.cbSize = sizeof mii;
      mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_STATE | MIIM_ID;
      mii.fType = MFT_STRING;
      mii.fState = MFS_DEFAULT;
      mii.wID = 100;
      mii.dwTypeData = L"Upload image";
      InsertMenuItem(hMenu, 0, TRUE, &mii);
      mii.fMask &= ~MIIM_STATE;
      mii.wID = 101;
      mii.dwTypeData = L"Save image";
      InsertMenuItem(hMenu, 1, TRUE, &mii);
      mii.wID = 102;
      mii.dwTypeData = L"Copy image";
      InsertMenuItem(hMenu, 2, TRUE, &mii);
      if (item->state == 0)
      {
        mii.wID = 105;
        mii.dwTypeData = L"Show sockets";
        InsertMenuItem(hMenu, 3, TRUE, &mii);
      }
      if (item->state > 0)
      {
        mii.wID = 105;
        mii.dwTypeData = L"Hide sockets";
        InsertMenuItem(hMenu, 3, TRUE, &mii);
      }
      if (GetMenuItemCount(hMenu))
      {
        POINT pt;
        GetCursorPos(&pt);
        int result = TrackPopupMenuEx(hMenu, TPM_HORIZONTAL | TPM_LEFTALIGN |
          TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, hWnd, NULL);
        switch (result)
        {
        case 100:
          {
            setTooltip(item->item, pt.x + 60, pt.y);
            String response;
            if (uploadToImgur(tooltipImage, response))
            {
              BasicDialog dlg(this, L"Upload successful", 250, 100);
              EditFrame* input = new EditFrame(&dlg, 0, ES_READONLY);
              input->setText(WideString::format(L"http://i.imgur.com/%S.png", response));
              input->setPoint(PT_TOPLEFT, 8, 8);
              input->setPoint(PT_TOPRIGHT, -8, 8);
              input->setHeight(21);
              ButtonFrame* ok = new ButtonFrame(L"Ok", &dlg, IDOK);
              ok->setSize(100, 21);
              ok->setPoint(PT_BOTTOM, 0, -8);
              SetFocus(input->getHandle());
              Edit_SetSel(input->getHandle(), 0, -1);
              dlg.doModal();
            }
            else
              MessageBox(hWnd, WideString(response), L"Upload failed", MB_OK | MB_ICONERROR);
            //if (id.empty())
            //  MessageBox(hWnd, )
            //WideString id = WideString::format()
            //BasicDialog dlg(this, L"Upload ")
            //MessageBox(hWnd, WideString(), L"Upload", MB_OK);
          }
          break;
        case 101:
          {
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof ofn);
            ofn.lStructSize = sizeof ofn;
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"PNG Images (*.png)\0*.png\0\0";
            wchar_t buf[512] = L"";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = sizeof buf;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetSaveFileName(&ofn))
            {
              setTooltip(item->item, pt.x + 60, pt.y);
              tooltipImage->writePNG(TempFile(File::create(buf)));
            }
          }
          break;
        case 102:
          {
            setTooltip(item->item, pt.x + 60, pt.y);
            HBITMAP hBitmap = tooltipImage->createBitmap();
            OpenClipboard(hWnd);
            EmptyClipboard();
            SetClipboardData(CF_BITMAP, hBitmap);
            CloseClipboard();
          }
          break;
        case 105:
          {
            RECT rc;
            GetClientRect(hWnd, &rc);
            rc.top = item->rc.top + groups[item->group].ypos - scrollPos;
            int delta = toggleItem(item);
            if (delta)
            {
              if (rc.top < 0) rc.top = 0;
              if (rc.top < rc.bottom)
                InvalidateRect(hWnd, &rc, TRUE);

              SCROLLINFO si;
              memset(&si, 0, sizeof si);
              si.cbSize = sizeof si;
              si.fMask = SIF_RANGE;
              GetScrollInfo(hWnd, SB_VERT, &si);
              si.nMax += delta;
              SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
            }
          }
          break;
        }
      }

      DestroyMenu(hMenu);
    }
    break;
  case WM_LBUTTONDOWN:
    {
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);
      int zone;
      ItemData* item = getItem(x, y, &zone);
      if (item && zone == 1)
      {
        RECT rc;
        GetClientRect(hWnd, &rc);
        int cg = item->group;
        if (groups[cg].ypos - scrollPos > 0)
          rc.top = groups[cg].ypos - scrollPos;

        int delta = toggleGroup(cg);

        rc.top -= GROUP_GAP;
        if (rc.top < 0) rc.top = 0;

        if (rc.top < rc.bottom)
          InvalidateRect(hWnd, &rc, TRUE);

        SCROLLINFO si;
        memset(&si, 0, sizeof si);
        si.cbSize = sizeof si;
        si.fMask = SIF_RANGE;
        GetScrollInfo(hWnd, SB_VERT, &si);
        si.nMax += delta;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
      }
    }
    return 0;
  case WM_MOUSEMOVE:
    {
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);
      int zone;
      ItemData* itemdata = getItem(x, y, &zone);
      if (zone > 0) itemdata = NULL;
      D2Item* item = NULL;
      if (itemdata)
      {
        if (zone < 0)
          item = itemdata->item->socketItems[-zone - 1];
        else
          item = itemdata->item;
      }
      if (item && tooltipItem)
      {
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof tme);
        tme.cbSize = sizeof tme;
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
      }
      else if (tooltipItem && !item)
      {
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof tme);
        tme.cbSize = sizeof tme;
        tme.dwFlags = TME_CANCEL | TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
      }
      setTooltip(item, x + 60, y);
    }
    return 0;
  case WM_MOUSELEAVE:
    if (tooltipItem)
    {
      TRACKMOUSEEVENT tme;
      memset(&tme, 0, sizeof tme);
      tme.cbSize = sizeof tme;
      tme.dwFlags = TME_CANCEL | TME_LEAVE;
      tme.hwndTrack = hWnd;
      TrackMouseEvent(&tme);
      setTooltip(NULL);
    }
    return 0;
  }
  return M_UNHANDLED;
}
void ItemList::doScroll(int pos)
{
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_RANGE | SIF_PAGE;
  GetScrollInfo(hWnd, SB_VERT, &si);
  if (pos < si.nMin) pos = si.nMin;
  if (pos > si.nMax - si.nPage + 1) pos = si.nMax - si.nPage + 1;
  si.fMask = SIF_POS;
  if (pos != scrollPos)
  {
    si.nPos = pos;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

    int deltaY = scrollPos - pos;
    scrollPos = pos;
    RECT rc;
    GetClientRect(hWnd, &rc);
    ScrollWindowEx(hWnd, 0, deltaY, &rc, &rc, NULL, NULL, SW_ERASE | SW_INVALIDATE);
  }
}
