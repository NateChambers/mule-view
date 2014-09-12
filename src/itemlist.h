#ifndef __ITEMLIST__
#define __ITEMLIST__

#include "d2data.h"
#include "d2mule.h"
#include "frameui/framewnd.h"
#include "base/regexp.h"

class ItemTooltip : public Window
{
  HDC dc;
  HBITMAP bitmap;
  int imageHeight;
  WideString header;
  uint32 onWndMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  ItemTooltip(HWND hParent, int x, int y, Image* image, char const* header);
  ~ItemTooltip();
};

struct ItemGroup;
class ItemList : public WindowFrame
{
protected:
  struct ItemData
  {
    int state;
    int group;
    RefPtr<D2Item> item;
    RECT rc;
  };
  struct ItemGroup
  {
    int type;
    int ypos;
    int hclosed;
    int hopen;
    int state;
    int wclosed;
    String title;
    D2BaseItem* base;
    D2UniqueItem* unique;
    int quality;
    Array<ItemData> items;
  };
  friend class Hash<ItemGroup>;
  D2Data* d2data;
  Array<D2Item*> allItems;
  Array<ItemGroup> groups;
  D2Item* tooltipItem;
  LocalPtr<Image> tooltipImage;
  ItemTooltip* tooltip;
  HICON stateIcons[2];
  int scrollPos;
  int scrollAccum;
  void doScroll(int pos);

  int toggleItem(ItemData* item);
  int toggleGroup(int index);
  void setTooltip(D2Item* item, int x = 0, int y = 0);
  ItemData* getItem(int x, int y, int* zone);
  static int itemCompare(D2Item* const& a, D2Item* const& b);
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);

  Window* filter;
public:
  ItemList(D2Data* data, Frame* parent, Window* filter);

  void setMule(D2Container* mule, bool reset = false)
  {
    allItems.clear();
    mule->populate(allItems);
    allItems.sort(itemCompare);
    updateSelected(reset);
  }
  void updateSelected(bool reset = false);
};

#endif // __ITEMLIST__
