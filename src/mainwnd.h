#ifndef __MAINWND__
#define __MAINWND__

#include "frameui/framewnd.h"
#include "frameui/controlframes.h"
#include "registry.h"
#include "d2data.h"
#include "itemlist.h"

#define WM_UPDATEDIR      (WM_USER + 123)

class MainWnd : public RootWindow
{
  struct WndSettings
  {
    int splitter[2];
    uint32 wndShow;
    uint32 wndX;
    uint32 wndY;
    uint32 wndWidth;
    uint32 wndHeight;
  };
  static WndSettings settings;
  int dragPos;
  int dragIndex;
  HCURSOR hVSplit;
  bool loaded;
  bool updating;

  TreeViewFrame* treeMules;
  TreeViewFrame* treeItems;
  ItemList* itemList;

  static void nonEmptyDFS(D2ItemType* type);
  void updateMules(HTREEITEM hItem, D2Container* list);
  void updateItems(HTREEITEM hItem, D2ItemType* type);
  void updateSelectedMules(D2Container* list);
  void doUpdate();
  void onSetMule(D2Container* mule);
  void updateSelectedTypes(D2ItemType* type);
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
  D2Data* d2data;
  D2Container muleRoot;
  HTREEITEM allItemsRoot;
public:
  MainWnd(D2Data* d2data);
  ~MainWnd();
};


#endif // __MAINWND__
