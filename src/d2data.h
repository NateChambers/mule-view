#ifndef __D2DATA__
#define __D2DATA__

#include "rmpq/rmpq2.h"
#include "frameui/image.h"
#include "base/dictionary.h"
#include "base/hashmap.h"
#include "base/ptr.h"
#include "base/actree.h"
#include "base/regexp.h"

class D2Excel
{
  Dictionary<int> m_index;
  char* m_data;
  int m_cols;
  int m_rows;
  int* m_offsets;
public:
  D2Excel(File* file);
  ~D2Excel();

  int rows() const
  {
    return m_rows;
  }
  int cols() const
  {
    return m_cols;
  }
  char const* colName(int i) const
  {
    return m_data + m_offsets[i];
  }
  int colByName(char const* name) const
  {
    return m_index.get(name);
  }
  char const* value(int row, int col) const
  {
    return m_data + m_offsets[(row + 1) * m_cols + col];
  }
};
class D2Strings
{
  HashMap<char const*, int> map;
  char* table[32768];
  Array<char*> data;
public:
  D2Strings();
  ~D2Strings();
  void load(File* file, int base);

  char const* byIndex(int index)
  {
    return table[index];
  }
  char const* byName(char const* name)
  {
    if (!map.has(name)) return NULL;
    return table[map.get(name)];
  }
};

struct D2Item;
struct D2ItemType
{
  D2ItemType* parent;
  D2ItemType* parent2;
  String code;
  String name;
  Array<String> invgfx;
  Array<D2ItemType*> sub;

  enum {fVisited = 1, fNonEmpty = 2, fDisplayed = 4};
  int flags;
  int type;
  int subType;
  int quality;
  int bases;
  void setType(int t, int s, int q = -1)
  {
    type = t;
    subType = s;
    quality = q;
  }
  D2ItemType* getInfo()
  {
    if (type >= 0) return this;
    D2ItemType* info = (parent ? parent->getInfo() : NULL);
    if (info) return info;
    return (parent2 ? parent2->getInfo() : NULL);
  }
  bool isType(char const* type)
  {
    if (code.compare(type) == 0) return true;
    if (parent && parent->isType(type)) return true;
    if (parent2 && parent2->isType(type)) return true;
    return false;
  }
  D2ItemType()
    : flags(0)
    , type(-1)
    , subType(-1)
    , quality(-1)
    , bases(0)
    , parent(NULL)
    , parent2(NULL)
  {}
};

struct D2Stat;
struct D2ItemStat
{
  D2Stat* stat;
  int param;
  int value1;
  int value2;

  D2ItemStat()
    : stat(NULL)
    , param(0)
    , value1(0)
    , value2(0)
  {}
};

struct D2BaseItem
{
  char const* name;
  int invwidth;
  int invheight;
  int invtrans;
  int levelreq;
  String invfile;
  D2ItemType* type;

  int numGemMods;
  Array<D2ItemStat> gemMods[3];
  String gemDesc;
};
struct D2UniqueItem
{
  char const* name;
  D2BaseItem* base;
  int invcolor;
  String invfile;
  int type; // 2 = set, 4 = unique/runeword

  Array<D2ItemStat> preset;
};
class D2MatchTreeBase
{
  enum {maxChar = 56};
  static inline int c2i(char c)
  {
    switch (c)
    {
    case 0:
    case ' ': return 0;
    case '(': return 53;
    case ')': return 54;
    case ':': return 55;
    default:
      if (c >= 'a' && c <= 'z') return c - 'a' + 1;
      if (c >= 'A' && c <= 'Z') return c - 'A' + 27;
    }
    return -1;
  }
  FixedMemoryPool pool;
  int dataSize;
  struct Node
  {
    Node* parent;
    Node* next[maxChar];
    Node* nexteq[maxChar];
    int chr;
    Node* link;
    int len;
  };
  Node* root;
protected:
  D2MatchTreeBase(int size)
    : pool(sizeof(Node) + size)
    , dataSize(size)
  {
    root = (Node*) pool.alloc();
    memset(root, 0, sizeof(Node));
    root->next[0] = (Node*) pool.alloc();
    memset(root->next[0], 0, sizeof(Node));
    root->next[0]->parent = root;
  }

  void* baseadd(char const* str);
  void* baseget(char const* str, bool matchOf = false, Pair<int, int>* info = NULL);
  void* basegeteq(char const* str);
public:
  void build();
};
template<class T>
class D2MatchTree : public D2MatchTreeBase
{
  T nil;
public:
  D2MatchTree()
    : D2MatchTreeBase(sizeof(T))
  {
    memset(&nil, 0, sizeof nil);
  }

  void add(char const* str, T const& t)
  {
    memcpy(baseadd(str), &t, sizeof(T));
  }
  T const& get(char const* str, bool matchOf = false, Pair<int, int>* info = NULL)
  {
    T* ptr = (T*) baseget(str, matchOf, info);
    return (ptr ? *ptr : nil);
  }
  T const& geteq(char const* str)
  {
    T* ptr = (T*) basegeteq(str);
    return (ptr ? *ptr : nil);
  }
};

class D2Font
{
  friend class D2Data;
  LocalPtr<File> m_glyphFile;
  uint32 m_glyphOffset[256];
  int m_gwidth[256];
  int m_gheight[256];
  int m_width;
  int m_height;
  D2Font() {}
public:
  int width(char const* text, int length = -1) const;
};

class D2Image
{
  uint8* m_bits;
  int m_width;
  int m_height;
public:
  D2Image(int width, int height);
  D2Image(D2Image const& img);
  ~D2Image();

  int width() const
  {return m_width;}
  int height() const
  {return m_height;}
  uint8 const* bits() const
  {return m_bits;}
  uint8* bits()
  {return m_bits;}

  void fill(int left, int top, int right, int bottom, uint8 color, uint8 const* blend = NULL);
  Image* image(int left, int top, int right, int bottom, uint32 const* palette);
  Image* image(uint32 const* palette)
  {
    return image(0, 0, m_width, m_height, palette);
  }
};

class D2StatData;

struct DC6_FRAME;
struct D2Point
{int x, y;};
class D2Data
{
  mpq::Loader loader;
  uint8 tints[10][21][256];
  uint8 blendTable[3][256 * 256];
  uint32 textColor[16];
  uint32 palette[256];
  D2Strings strings;

  D2ItemType rootType;
  Dictionary<D2ItemType> itemTypes;
  Dictionary<D2BaseItem> baseItems;
  Array<LocalPtr<D2UniqueItem> > uniqueItems;
  D2MatchTree<D2BaseItem*> baseMatch;
  D2MatchTree<D2UniqueItem*> uniqueMatch;
  Dictionary<LocalPtr<File> > images;
  Dictionary<LocalPtr<D2Font> > fonts;
  LocalPtr<D2Image> invBackground;
  void drawDC6(D2Image* image, int x, int y, DC6_FRAME* frame, File* file, uint8 const* repl = NULL, uint8 const* blend = NULL);
  File* loadImage(char const* path);
  File* loadImageByName(char const* name);
  void drawFrames(D2Image* image, char const* path, int start, int count, int* x, int* y, uint8 const* repl = NULL);
  void loadBackground();
  D2Image* composeImage(char const* path, int row);
  void loadTypes(File* file);
  void loadBase(File* file);
  D2ItemType* mergeType(D2ItemType* type);
  void loadUnique();
  void loadGems();
  LocalPtr<D2StatData> statData;
  Dictionary<int> charClass;
  char const* classNames[7];
public:
  D2Data();
  ~D2Data();

  char const* getLocalizedString(int id)
  {return strings.byIndex(id);}
  char const* getLocalizedString(char const* name)
  {return strings.byName(name);}

  D2Font* getFont(char const* path = NULL);
  void drawText(D2Image* image, int x, int y, char const* text, int length, D2Font* font, int color = 0);

  void getPalette(uint32* ptr, char const* path = NULL);

  D2BaseItem* getBaseByCode(char const* code)
  {
    return baseItems.getptr(code);
  }
  D2BaseItem* getBaseItem(char const* name, bool matchOf = false)
  {
    return baseMatch.get(name, matchOf);
  }
  D2UniqueItem* getUniqueItem(char const* name)
  {
    return uniqueMatch.get(name);
  }
  D2UniqueItem* getUniqueByBase(D2BaseItem* base, int color);

  void clearItemFlags(int flags);
  D2ItemType* getItemRoot()
  {
    return &rootType;
  }

  File* getFile(char const* path)
  {
    return loader.load(path);
  }
  D2Image* getImage(char const* path);
  Image* renderItem(D2Item* item);

  uint32 const* getPalette() const
  {
    return palette;
  }

  D2StatData* getStatData()
  {
    return statData;
  }

  int getCharClass(char const* cls)
  {
    if (charClass.has(cls))
      return charClass.get(cls);
    return -1;
  }
  char const* getClassName(int id)
  {
    return classNames[id];
  }

  static D2Point const* getSocketPos(int width, int height, int sox);
};

#endif // __D2DATA__
