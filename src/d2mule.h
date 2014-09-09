#ifndef __D2MULE__
#define __D2MULE__

#include "base/string.h"
#include "base/wstring.h"
#include "base/ptr.h"
#include "d2stats.h"

class D2Data;
struct D2BaseItem;
struct D2UniqueItem;
struct D2Stat;

struct D2Container;
struct D2Item
{
  enum {fEthereal       = 0x01,
        fUnidentified   = 0x02,
  };
  enum {tMisc, tSocket, tCharm, tWhite, tMagic, tRare, tSet, tUnique, tRuneword, tCrafted, tAuto = tWhite};
  enum {qNone, qLow, qNormal, qSuperior, qMagic, qSet, qRare, qUnique, qCrafted};
  D2Container* parent;
  D2Item* socketParent;
  int invColor;
  String image;
  String title;
  String description;
  String header;
  Array<String> sockets;

  int itemLevel;
  int flags;
  int type;
  int quality;
  int subType;
  int colorCode;
  D2BaseItem* base;
  D2UniqueItem* unique;

  Array<D2ItemStat> stats;

  String statText;

  Array<LocalPtr<D2Item> > socketItems;

  void parse(D2Data* data);

  D2ItemStat* getStat(int id);

  D2Item(D2Container* p = NULL)
    : parent(p)
    , socketParent(NULL)
    , base(NULL)
    , unique(NULL)
    , itemLevel(0)
    , flags(0)
    , type(0)
    , subType(0)
    , colorCode(0)
    , quality(0)
    , invColor(-1)
  {}
};
struct D2Container
{
  D2Container* parent;
  WideString name;
  uint64 lastUpdate;
  void* hItem;
  Array<D2Item*> items;
  Array<D2Container*> sub;

  D2Container(WideString theName, D2Container* p = NULL)
    : parent(p)
    , lastUpdate(0)
    , name(theName)
    , hItem(NULL)
  {}
  ~D2Container();

  static int compare(D2Container* const& a, D2Container* const& b)
  {
    return WideString::smartCompare(a->name, b->name);
  }
  void populate(Array<D2Item*>& itemlist);

  bool parseDir(wchar_t const* path, D2Data* data);
  bool parseFile(wchar_t const* path, D2Data* data);

  D2Container* find(wchar_t const* subname);
};

#endif // __D2MULE__
