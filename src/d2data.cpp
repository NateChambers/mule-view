#include "d2data.h"
#include "base/ptr.h"
#include "base/wstring.h"
#include "d2struct.h"
#include "d2mule.h"
#include "d2stats.h"
#include "registry.h"
#include "base/utils.h"

D2Image::D2Image(int width, int height)
  : m_width(width)
  , m_height(height)
{
  m_bits = new uint8[width * height];
  memset(m_bits, 0, width * height);
}
D2Image::D2Image(D2Image const& img)
  : m_width(img.m_width)
  , m_height(img.m_height)
{
  m_bits = new uint8[m_width * m_height];
  memcpy(m_bits, img.m_bits, m_width * m_height);
}
D2Image::~D2Image()
{
  delete[] m_bits;
}
void D2Image::fill(int left, int top, int right, int bottom, uint8 color, uint8 const* blend)
{
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > m_width) right = m_width;
  if (bottom > m_height) bottom = m_height;
  for (int y = top; y < bottom; y++)
  {
    uint8* ptr = m_bits + y * m_width;
    if (blend)
    {
      for (int x = left; x < right; x++)
        ptr[x] = blend[ptr[x] * 256 + color];
    }
    else
      memset(ptr + left, color, right - left);
  }
}
Image* D2Image::image(int left, int top, int right, int bottom, uint32 const* palette)
{
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > m_width) right = m_width;
  if (bottom > m_height) bottom = m_height;
  Image* img = new Image(right - left, bottom - top);
  for (int y = top; y < bottom; y++)
  {
    uint8* src = m_bits + y * m_width;
    uint32* dst = img->bits() + (y - top) * img->width();
    for (int x = left; x < right; x++)
      dst[x - left] = palette[src[x]];
  }
  return img;
}


D2Excel::D2Excel(File* file)
{
  file->seek(0, SEEK_END);
  int size = file->tell();
  m_data = new char[size + 1];
  file->seek(0, SEEK_SET);
  file->read(m_data, size);
  m_data[size] = 0;
  m_cols = 1;
  m_rows = -1;
  m_offsets = NULL;
  for (int i = 0; i < size && m_data[i] != '\r'; i++)
    m_cols += (m_data[i] == '\t');
  for (int i = 0; i < size; i++)
    m_rows += (m_data[i] == '\n');
  if (m_rows < 0 || m_cols <= 0) return;
  m_offsets = new int[(m_rows + 1) * m_cols];
  int row = 0, col = 0;
  m_offsets[0] = 0;
  for (int i = 0; i < size; i++)
  {
    if (m_data[i] == '\t')
    {
      col++;
      if (row <= m_rows && col < m_cols) m_offsets[row * m_cols + col] = i + 1;
      m_data[i] = 0;
    }
    else if (m_data[i] == '\r')
      m_data[i] = 0;
    else if (m_data[i] == '\n')
    {
      row++;
      col = 0;
      if (row <= m_rows && col < m_cols) m_offsets[row * m_cols + col] = i + 1;
      m_data[i] = 0;
    }
  }
  for (int i = 0; i < m_cols; i++)
    m_index.set(m_data + m_offsets[i], i);
}
D2Excel::~D2Excel()
{
  delete[] m_data;
  delete[] m_offsets;
}

D2Strings::D2Strings()
{
  memset(table, 0, sizeof table);
}
D2Strings::~D2Strings()
{
  for (int i = 0; i < data.length(); i++)
    delete[] data[i];
}
void D2Strings::load(File* file, int base)
{
  if (!file) return;
  TBL_HEADER hdr;
  if (file->read(&hdr, sizeof hdr) != sizeof hdr) return;
  uint32 indexBase = file->tell();
  uint32 hashBase = indexBase + hdr.numElements * sizeof(uint16);
  file->seek(hdr.indexStart, SEEK_SET);
  char* strings = new char[hdr.indexEnd - hdr.indexStart];
  file->read(strings, hdr.indexEnd - hdr.indexStart);
  data.push(strings);
  for (uint16 i = 0; i < hdr.numElements; i++)
  {
    file->seek(indexBase + i * sizeof(uint16), SEEK_SET);
    uint32 hash = file->read16();
    file->seek(hashBase + hash * sizeof(TBL_HASHTBL), SEEK_SET);
    TBL_HASHTBL entry;
    if (file->read(&entry, sizeof entry) == sizeof entry && entry.used)
    {
      char* key = strings + entry.keyOffset - hdr.indexStart;
      char* str = strings + entry.strOffset - hdr.indexStart;
      table[base + entry.index] = str;
      map.set(key, base + entry.index);
    }
  }
}

void* D2MatchTreeBase::baseadd(char const* str)
{
  int len;
  int alen = 0;
  Node* node = root->next[0];
  for (len = 0; str[len]; len++)
  {
    int chr = c2i(str[len]);
    if (chr < 0) continue;
    alen++;
    if (node->next[chr] == NULL)
    {
      Node* temp = (Node*) pool.alloc();
      memset(temp, 0, sizeof(Node) + dataSize);
      temp->parent = node;
      temp->chr = chr;
      node->nexteq[chr] = node->next[chr] = temp;
    }
    node = node->next[chr];
  }
  node->len = alen;
  return node + 1;
}
void D2MatchTreeBase::build()
{
  Node* head = root;
  Node* tail = root;
  while (head)
  {
    Node* link;
    if (!head->parent || head->parent == root)
      link = root;
    else
      link = head->parent->link->next[head->chr];
    if (link->len > head->len)
    {
      memcpy(head + 1, link + 1, dataSize);
      head->len = link->len;
    }
    for (int i = 0; i < maxChar; i++)
    {
      if (!head->next[i])
      {
        if (head->parent)
          head->next[i] = link->next[i];
        else
          head->next[i] = root;
      }
      else
      {
        tail->link = head->next[i];
        tail = tail->link;
      }
    }
    Node* temp = head->link;
    head->link = link;
    head = temp;
  }
}
void* D2MatchTreeBase::baseget(char const* str, bool matchOf, Pair<int, int>* info)
{
  int len = strlen(str);
  void* bestval = NULL;
  int bestpos = -1;
  int bestlen = 0;
  void* bestof = NULL;
  int bestofpos = -1;
  int bestoflen = 0;
  Node* node = root->next[0];
  for (int i = 0; i <= len; i++)
  {
    int chr = c2i(str[i]);
    if (chr < 0) continue;
    if (chr == 0 && node->len > bestlen)
    {
      bestval = node + 1;
      bestpos = i - node->len;
      bestlen = node->len;
    }
    if (matchOf && (!str[i] || !strncmp(str + i, " of ", 4)) && node->len > bestoflen)
    {
      bestof = node + 1;
      bestofpos = i - node->len;
      bestoflen = node->len;
    }
    node = node->next[chr];
  }
  if (bestof)
  {
    bestval = bestof;
    bestpos = bestofpos;
    bestlen = bestoflen;
  }
  if (info)
  {
    info->first = bestpos;
    if (bestpos >= 0) info->second = bestpos + bestlen;
  }
  return bestval;
}
void* D2MatchTreeBase::basegeteq(char const* str)
{
  Node* node = root->next[0];
  for (int i = 0; str[i] && node; i++)
  {
    int chr = c2i(str[i]);
    if (chr < 0) continue;
    node = node->nexteq[chr];
  }
  return (node ? node + 1 : NULL);
}

void D2Data::loadTypes(File* file)
{
  D2Excel excel(file);
  int invgfx[6] = {excel.colByName("InvGfx1"),
                   excel.colByName("InvGfx2"),
                   excel.colByName("InvGfx3"),
                   excel.colByName("InvGfx4"),
                   excel.colByName("InvGfx5"),
                   excel.colByName("InvGfx6")};
  for (int i = 0; i < excel.rows(); i++)
  {
    char const* name = excel.value(i, 0);
    char const* code = excel.value(i, 1);
    if (*name && *code)
    {
      D2ItemType& type = itemTypes.create(code);
      type.code = code;
      type.name = name;
      type.parent = &rootType;
      for (int j = 0; j < 6; j++)
      {
        char const* gfx = (invgfx[j] ? excel.value(i, invgfx[j]) : "");
        if (*gfx) type.invgfx.push(gfx);
      }
    }
  }
  for (int i = 0; i < excel.rows(); i++)
  {
    D2ItemType* type = itemTypes.getptr(excel.value(i, 1));
    D2ItemType* equiv1 = itemTypes.getptr(excel.value(i, 2));
    D2ItemType* equiv2 = itemTypes.getptr(excel.value(i, 3));
    if (type && equiv1)
    {
      equiv1->sub.push(type);
      type->parent = equiv1;
    }
    if (type && equiv2)
    {
      equiv2->sub.push(type);
      if (type->parent)
        type->parent2 = equiv2;
      else
        type->parent = equiv2;
    }
    if (type && !equiv1 && !equiv2)
      rootType.sub.push(type);
  }
  for (uint32 cur = itemTypes.enumStart(); cur; cur = itemTypes.enumNext(cur))
    itemTypes.enumGetValue(cur).sub.sort();
}
void D2Data::clearItemFlags(int flags)
{
  rootType.flags &= ~flags;
  for (uint32 cur = itemTypes.enumStart(); cur; cur = itemTypes.enumNext(cur))
    itemTypes.enumGetValue(cur).flags &= ~flags;
}
void D2Data::loadBase(File* file)
{
  D2Excel excel(file);
  int cCode = excel.colByName("code");
  int cNameStr = excel.colByName("namestr");
  int cInvWidth = excel.colByName("invwidth");
  int cInvHeight = excel.colByName("invheight");
  int cType = excel.colByName("type");
  int cInvTrans = excel.colByName("InvTrans");
  int cInvFile = excel.colByName("invfile");
  int cLevelReq = excel.colByName("levelreq");
  for (int i = 0; i < excel.rows(); i++)
  {
    char const* code = excel.value(i, cCode);
    char const* name = strings.byName(excel.value(i, cNameStr));
    if (*code && name)
    {
      while (true)
      {
        char const* endl = strchr(name, '\n');
        if (!endl) break;
        name = endl + 1;
      }
      D2BaseItem& base = baseItems.create(code);
      base.name = name;
      base.invwidth = atoi(excel.value(i, cInvWidth));
      base.invheight = atoi(excel.value(i, cInvHeight));
      base.invtrans = atoi(excel.value(i, cInvTrans));
      base.type = itemTypes.getptr(excel.value(i, cType));
      base.invfile = excel.value(i, cInvFile);
      base.levelreq = (cLevelReq ? atoi(excel.value(i, cLevelReq)) : 0);
      base.numGemMods = 0;
      baseMatch.add(base.name, &base);

      if (base.type) base.type->bases++;
    }
  }
}
D2ItemType* D2Data::mergeType(D2ItemType* type)
{
  for (int i = 0; i < type->sub.length(); i++)
  {
    type->sub[i] = mergeType(type->sub[i]);
    if (!type->sub[i])
      type->sub.remove(i--, 1);
  }
  if (!type->bases && type->sub.length() <= 1)
    return (type->sub.length() ? type->sub[0] : NULL);
  return type;
}
static int uniqueComp(LocalPtr<D2UniqueItem> const& a, LocalPtr<D2UniqueItem> const& b)
{
  if (a->base != b->base)
    return int(a->base) - int(b->base);
  return a->type - b->type;
}
void D2Data::loadUnique()
{
  Dictionary<int> colorCodes;
  {
    D2Excel colors(TempFile(loader.load("data\\global\\excel\\colors.txt")));
    for (int i = 0; i < colors.rows(); i++)
      colorCodes.set(colors.value(i, 1), i);
  }

  for (int type = 2; type <= 6; type += 2)
  {
    D2Excel excel(TempFile(loader.load(type == 2 ? "data\\global\\excel\\SetItems.txt" : (
                                       type == 4 ? "data\\global\\excel\\UniqueItems.txt" :
                                                   "data\\global\\excel\\Runes.txt"))));
    int cEnabled = excel.colByName(type == 6 ? "complete" : "enabled");
    int cCode = excel.colByName(type == 2 ? "item" : "code");
    int cInvColor = excel.colByName("invtransform");
    int cInvFile = excel.colByName("invfile");
    int cProps = excel.colByName(type < 6 ? "prop1" : "T1Code1");
    for (int i = 0; i < excel.rows(); i++)
    {
      if (type == 2 ? *excel.value(i, cCode) : atoi(excel.value(i, cEnabled)))
      {
        D2UniqueItem* item = new D2UniqueItem;
        item->name = strings.byName(excel.value(i, 0));
        if (type != 6)
        {
          char const* invColor = excel.value(i, cInvColor);
          item->invcolor = (colorCodes.has(invColor) ? colorCodes.get(invColor) : -1);
          item->invfile = excel.value(i, cInvFile);
          item->base = baseItems.getptr(excel.value(i, cCode));
        }
        else
        {
          item->invcolor = -1;
          item->base = NULL;
        }
        item->type = type;

        for (int p = 0; p < (type < 6 ? 12 : 7); p++)
        {
          char const* propName = excel.value(i, cProps + p * 4);
          if (!*propName) continue;
          char const* par = excel.value(i, cProps + p * 4 + 1);
          int parI;
          if (String(par).isDigits())
            parI = atoi(par);
          else
            parI = statData->getSkill(par);
          statData->addPreset(item->preset, propName, parI,
             atoi(excel.value(i, cProps + p * 4 + 2)), atoi(excel.value(i, cProps + p * 4 + 3)));
        }

        uniqueItems.push() = item;
        uniqueMatch.add(item->name, item);
      }
    }
  }
  uniqueItems.sort(uniqueComp);
  uniqueMatch.build();
}
D2UniqueItem* D2Data::getUniqueByBase(D2BaseItem* base, int type)
{
  int left = 0;
  int right = uniqueItems.length();
  while (left < right)
  {
    int mid = (left + right) / 2;
    int diff = (base == uniqueItems[mid]->base ? type - uniqueItems[mid]->type : int(base) - int(uniqueItems[mid]->base));
    if (diff == 0)
    {
      if (mid > 0 && uniqueComp(uniqueItems[mid - 1], uniqueItems[mid]) == 0) return NULL;
      if (mid < uniqueItems.length() - 1 && uniqueComp(uniqueItems[mid + 1], uniqueItems[mid]) == 0) return NULL;
      return uniqueItems[mid];
    }
    if (diff < 0) right = mid;
    else left = mid + 1;
  }
  return NULL;
}
void D2Data::loadGems()
{
  D2Excel gems(TempFile(loader.load("data\\global\\excel\\gems.txt")));
  int cCode = gems.colByName("code");
  int cCols[3] = {gems.colByName("weaponMod1Code"),
                  gems.colByName("helmMod1Code"),
                  gems.colByName("shieldMod1Code")};
  for (int r = 0; r < gems.rows(); r++)
  {
    D2BaseItem* base = baseItems.getptr(gems.value(r, cCode));
    if (!base) continue;
    int mods = atoi(gems.value(r, cCode + 1));
    base->numGemMods = 3;

    for (int t = 0; t < 3; t++)
    {
      for (int c = 0; c < 3; c++)
      {
        char const* propName = gems.value(r, cCols[t] + c * 4);
        if (!*propName) continue;
        char const* par = gems.value(r, cCols[t] + c * 4 + 1);
        int parI;
        if (String(par).isDigits())
          parI = atoi(par);
        else
          parI = statData->getSkill(par);
        statData->addPreset(base->gemMods[t], propName, parI,
          atoi(gems.value(r, cCols[t] + c * 4 + 2)), atoi(gems.value(r, cCols[t] + c * 4 + 3)));
      }
      statData->group(base->gemMods[t]);
    }

    if (mods == 1)
      base->gemDesc = "\\xffc8";
    else
      base->gemDesc = "\\xffc0";
    base->gemDesc.printf("%s (1)\\n\\xffc0%s\\n\\xffc0\\n", base->name, strings.byIndex(21874));
    base->gemDesc.printf("\\xffc0%s ", strings.byIndex(21263));
    for (int i = 0; i < base->gemMods[0].length(); i++)
    {
      if (i) base->gemDesc += "\\xffc0";
      base->gemDesc.printf("%s", statData->format(base->gemMods[0][i]).trim());
      if (i < base->gemMods[0].length() - 1) base->gemDesc += ',';
      base->gemDesc += "\\n";
    }
    base->gemDesc.printf("\\xffc0%s ", strings.byIndex(21264));
    for (int i = 0; i < base->gemMods[1].length(); i++)
    {
      if (i) base->gemDesc += "\\xffc0";
      base->gemDesc.printf("%s", statData->format(base->gemMods[1][i]).trim());
      if (i < base->gemMods[1].length() - 1) base->gemDesc += ',';
      base->gemDesc += "\\n";
    }
    base->gemDesc.printf("\\xffc0%s ", strings.byIndex(21261));
    for (int i = 0; i < base->gemMods[1].length(); i++)
    {
      if (i) base->gemDesc += "\\xffc0";
      base->gemDesc.printf("%s", statData->format(base->gemMods[1][i]).trim());
      if (i < base->gemMods[1].length() - 1) base->gemDesc += ',';
      base->gemDesc += "\\n";
    }
    base->gemDesc.printf("\\xffc0%s ", strings.byIndex(21262));
    for (int i = 0; i < base->gemMods[2].length(); i++)
    {
      if (i) base->gemDesc += "\\xffc0";
      base->gemDesc.printf("%s", statData->format(base->gemMods[2][i]).trim());
      if (i < base->gemMods[2].length() - 1) base->gemDesc += ',';
      base->gemDesc += "\\n";
    }
    base->gemDesc.printf("\\xffc0\\n\\xffc1%s %d", strings.byIndex(3469), base->levelreq);
  }
}
D2Point const* D2Data::getSocketPos(int width, int height, int sox)
{
  static D2Point socketPos[77] = {
    // thx kolton for putting sockets in the wrong order
    // 2x1
    /*  0 */ { 0, 14},
    /*  1 */ { 0,  0}, { 0, 29},
    // 3x1
    /*  3 */ { 0, 29},
    /*  4 */ { 0, 14}, { 0, 43},
    /*  6 */ { 0,  0}, { 0, 29}, { 0, 58},
    // 4x1
    /*  9 */ { 0, 43},
    /* 10 */ { 0, 29}, { 0, 58},
    /* 12 */ { 0, 14}, { 0, 43}, { 0, 72},
    /* 15 */ { 0,  0}, { 0, 29}, { 0, 58}, { 0, 87},
    // 2x2
    /* 19 */ {14, 14},
    /* 20 */ {14,  0}, {14, 29},
    /* 22 */ { 0,  0}, {29,  0}, {14, 29},
    /* 25 */ { 0,  0}, {29, 29}, { 0, 29}, {29,  0},
    // 3x2
    /* 29 */ {14, 29},
    /* 30 */ {14, 14}, {14, 43},
    /* 32 */ {14,  0}, {14, 29}, {14, 58},
    /* 35 */ { 0, 14}, {29, 14}, { 0, 43}, {29, 43},
    /* 39 */ { 0,  0}, {14, 29}, {29,  0}, { 0, 58}, {29, 58},
    /* 44 */ { 0,  0}, {29,  0}, { 0, 29}, {29, 29}, { 0, 58}, {29, 58},
    // 4x2
    /* 50 */ {14, 43},
    /* 51 */ {14, 14}, {14, 72},
    /* 53 */ {14, 14}, {14, 43}, {14, 72},
    /* 56 */ {14,  0}, {14, 29}, {14, 58}, {14, 87},
    /* 60 */ { 0, 14}, {14, 43}, {29, 14}, { 0, 72}, {29, 72},
    /* 65 */ { 0, 14}, {29, 14}, { 0, 43}, {29, 43}, { 0, 72}, {29, 72},
    // ?
    /* 71 */ { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0},
  };
  switch (sox)
  {
  case 1:
    if (height == 2 && width == 1) return socketPos + 0;
    else if (height == 3 && width == 1) return socketPos + 3;
    else if (height == 4 && width == 1) return socketPos + 9;
    else if (height == 2 && width == 2) return socketPos + 19;
    else if (height == 3 && width == 2) return socketPos + 29;
    else if (height == 4 && width == 2) return socketPos + 50;
    break;
  case 2:
    if (height == 2 && width == 1) return socketPos + 1;
    else if (height == 3 && width == 1) return socketPos + 4;
    else if (height == 4 && width == 1) return socketPos + 10;
    else if (height == 2 && width == 2) return socketPos + 20;
    else if (height == 3 && width == 2) return socketPos + 30;
    else if (height == 4 && width == 2) return socketPos + 51;
    break;
  case 3:
    if (height == 3 && width == 1) return socketPos + 6;
    else if (height == 4 && width == 1) return socketPos + 12;
    else if (height == 2 && width == 2) return socketPos + 22;
    else if (height == 3 && width == 2) return socketPos + 32;
    else if (height == 4 && width == 2) return socketPos + 53;
    break;
  case 4:
    if (height == 4 && width == 1) return socketPos + 15;
    else if (height == 2 && width == 2) return socketPos + 25;
    else if (height == 3 && width == 2) return socketPos + 35;
    else if (height == 4 && width == 2) return socketPos + 56;
    break;
  case 5:
    if (height == 3 && width == 2) return socketPos + 39;
    else if (height == 4 && width == 2) return socketPos + 60;
    break;
  case 6:
    if (height == 3 && width == 2) return socketPos + 44;
    else if (height == 4 && width == 2) return socketPos + 65;
    break;
  }
  return socketPos + 71;
}
static void unpack(mpq::Loader* loader, mpq::ListFile* list, char const* spath)
{
  String path = String::format("data\\global\\items\\%s.dc6", spath);
  if (list->has(path)) return;
  LocalPtr<File> src = loader->load(path);
  if (!src) return;
  LocalPtr<File> dst = File::create(WideString(path));
  if (dst) dst->copy(src);
}
D2Data::D2Data()
  : itemTypes(DictionaryMap::alNum)
  , baseItems(DictionaryMap::alNum)
  , images(DictionaryMap::pathName)
  , fonts(DictionaryMap::pathName)
{
  WideString path = cfg.getwstr("d2path");
  if (path.empty()) path = getAppPath();
  int loaded = !!loader.loadArchive(WideString::buildFullName(path, L"resources.mpq"));
  loaded += !!loader.loadArchive(WideString::buildFullName(path, L"d2data.mpq"));
  loaded += !!loader.loadArchive(WideString::buildFullName(path, L"d2exp.mpq"));
  loaded += !!loader.loadArchive(WideString::buildFullName(path, L"patch_d2.mpq"));
  if (!loaded)
  {
    path = getAppPath();
    loaded = !!loader.loadArchive(WideString::buildFullName(path, L"resources.mpq"));
  }
  if (!loaded) return;
  cfg.setwstr("d2path", path);

  for (int f = 0; f < 10; f++)
    for (int t = 0; t < 21; t++)
      for (int i = 0; i < 256; i++)
        tints[f][t][i] = i;
  TempFile(loader.load("data\\global\\items\\Palette\\grey.dat"))->read(tints[1], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\grey2.dat"))->read(tints[2], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\gold.dat"))->read(tints[3], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\brown.dat"))->read(tints[4], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\greybrown.dat"))->read(tints[5], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\invgrey.dat"))->read(tints[6], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\invgrey2.dat"))->read(tints[7], 256 * 21);
  TempFile(loader.load("data\\global\\items\\Palette\\invgreybrown.dat"))->read(tints[8], 256 * 21);
  LocalPtr<File> pal = loader.load("data\\global\\palette\\ACT1\\pal.dat");
  for (int i = 0; i < 256; i++)
  {
    int b = pal->getc(), g = pal->getc(), r = pal->getc();//, a = pal->getc();
    palette[i] = Image::clr(r, g, b);
  }

  memset(textColor, 0xFF, sizeof textColor);
  {
    LocalPtr<File> pl2 = loader.load("data\\global\\palette\\ACT1\\Pal.PL2");
    pl2->seek(1024 + 49 * 256, SEEK_SET);
    pl2->read(blendTable, sizeof blendTable);
    pl2->seek(1024 + 1714 * 256, SEEK_SET);
    for (int i = 0; i < 13; i++)
    {
      int r = pl2->getc(), g = pl2->getc(), b = pl2->getc();
      textColor[i] = Image::clr(r, g, b);
    }
    pl2->seek(256, SEEK_CUR);
    for (int i = 1; i < 13; i++)
      pl2->read(tints[9][i], 256);
  }

  strings.load(TempFile(loader.load("data\\local\\lng\\eng\\string.tbl")), 0);
  strings.load(TempFile(loader.load("data\\local\\lng\\eng\\expansionstring.tbl")), 20000);
  strings.load(TempFile(loader.load("data\\local\\lng\\eng\\patchstring.tbl")), 10000);

  rootType.name = "All Items";
  loadTypes(TempFile(loader.load("data\\global\\excel\\ItemTypes.txt")));
  loadBase(TempFile(loader.load("data\\global\\excel\\weapons.txt")));
  loadBase(TempFile(loader.load("data\\global\\excel\\armor.txt")));
  loadBase(TempFile(loader.load("data\\global\\excel\\misc.txt")));
  baseMatch.build();

  itemTypes.get("h2h2").bases = 0;
  itemTypes.get("mcha").name = strings.byIndex(20436);
  itemTypes.get("lcha").name = strings.byIndex(20437);
  itemTypes.get("ques").setType(D2Item::tMisc, 0, 0);
  itemTypes.get("key").setType(D2Item::tMisc, 0, 0);
  itemTypes.get("weap").setType(D2Item::tAuto, 0);
  itemTypes.get("armo").setType(D2Item::tAuto, 1);
  itemTypes.get("ring").setType(D2Item::tAuto, 2);
  itemTypes.get("amul").setType(D2Item::tAuto, 2);
  itemTypes.get("char").setType(D2Item::tCharm, 0);
  itemTypes.get("gem").setType(D2Item::tSocket, 0, 0);
  itemTypes.get("rune").setType(D2Item::tSocket, 1, 0);
  itemTypes.get("jewl").setType(D2Item::tSocket, 2);
  for (uint32 cur = baseItems.enumStart(); cur; cur = baseItems.enumNext(cur))
  {
    D2BaseItem* base = &baseItems.enumGetValue(cur);
    while (base->type && base->type->bases == 0)
      base->type = base->type->parent;
  }
  mergeType(&rootType);

  {
    D2Excel table(TempFile(loader.load("data\\global\\excel\\PlayerClass.txt")));
    int id = 0;
    for (int i = 0; i < table.rows(); i++)
    {
      if (!*table.value(i, 1))
        continue;
      charClass.set(table.value(i, 1), id);
      classNames[id] = strings.byName(table.value(i, 0));
      id++;
    }
  }
  statData = new D2StatData(this);

  loadUnique();
  loadGems();

  loadBackground();

  LocalPtr<mpq::ListFile> list = loader.buildListFile();
  list->sort();
  for (uint32 cur = baseItems.enumStart(); cur; cur = baseItems.enumNext(cur))
    unpack(&loader, list, baseItems.enumGetValue(cur).invfile);
  for (uint32 cur = itemTypes.enumStart(); cur; cur = itemTypes.enumNext(cur))
  {
    D2ItemType& type = itemTypes.enumGetValue(cur);
    for (int i = 0; i < type.invgfx.length(); i++)
      unpack(&loader, list, type.invgfx[i]);
  }
  for (int i = 0; i < uniqueItems.length(); i++)
    unpack(&loader, list, uniqueItems[i]->invfile);
}
D2Data::~D2Data()
{
}

void D2Data::drawDC6(D2Image* image, int x, int y, DC6_FRAME* frame, File* file, uint8 const* repl, uint8 const* blend)
{
  static DC6_FRAME loc_frame;
  if (!frame)
  {
    file->read(&loc_frame, sizeof loc_frame);
    frame = &loc_frame;
  }
  uint8* bits = image->bits() + x + y * image->width();
  if (!frame->flip) bits += image->width() * (frame->height - 1);
  uint8* base = bits;
  if (x >= 0 && y >= 0 && x + frame->width <= image->width() && y + frame->height <= image->height())
  {
    for (uint32 i = 0; i < frame->length; i++)
    {
      uint8 c = file->getc();
      if (c == 0x80)
        base = bits = base + (frame->flip ? image->width() : -image->width());
      else if (c & 0x80)
        bits += (c & 0x7F);
      else if (repl || blend)
      {
        if (!repl)
        {
          while (c--)
          {
            uint8 t = file->getc(); i++;
            *bits = blend[*bits * 256 + t]; bits++;
          }
        }
        else if (!blend)
        {
          while (c--)
          {
            uint8 t = file->getc(); i++;
            *bits++ = repl[t];
          }
        }
        else
        {
          while (c--)
          {
            uint8 t = file->getc(); i++;
            *bits = blend[*bits * 256 + repl[t]]; bits++;
          }
        }
      }
      else
      {
        file->read(bits, c);
        i += c;
        bits += c;
      }
    }
  }
  else
  {
    int cy = y + (frame->flip ? 0 : frame->height - 1);
    int cx = x;
    int dy = (frame->flip ? 1 : -1);
    for (uint32 i = 0; i < frame->length; i++)
    {
      uint8 c = file->getc();
      if (c == 0x80)
        base = bits = base + dy * image->width(), cy += dy, cx = x;
      else if (c & 0x80)
        bits += (c & 0x7F), cx += (c & 0x7F);
      else if (cy >= 0 && cy < image->height())
      {
        while (c--)
        {
          uint8 t = file->getc(); i++;
          if (cx >= 0 && cx < image->width())
          {
            if (repl) t = repl[t];
            if (blend)
              *bits = blend[*bits * 256 + t];
            else
              *bits = t;
          }
          cx++; bits++;
        }
      }
    }
  }
}
File* D2Data::loadImage(char const* path)
{
  File* result = images.get(path);
  if (result)
  {
    result->seek(0, SEEK_SET);
    return result;
  }

  LocalPtr<File> file = loader.load(path);
  if (!file) return NULL;
  DC6_HEADER hdr;
  if (file->read(&hdr, sizeof hdr) != sizeof hdr) return NULL;
  if (hdr.version != 6) return NULL;
  if (hdr.frames_per_dir <= 0) return NULL;
  uint32 offset = file->read32();
  file->seek(offset, SEEK_SET);
  DC6_FRAME frame;
  if (file->read(&frame, sizeof frame) != sizeof frame) return NULL;
  uint32 size = sizeof frame + frame.length;
  LocalPtr<uint8> filebuf = new uint8[size];
  file->seek(offset, SEEK_SET);
  file->read(filebuf, size);
  result = File::memfile(filebuf.yield(), size);
  images.create(path) = result;
  return result;
}
void D2Data::drawFrames(D2Image* image, char const* path, int start, int count, int* x, int* y, uint8 const* repl)
{
  uint32 offsets[32];
  DC6_HEADER hdr;
  LocalPtr<File> dc6 = loader.load(path);
  if (!dc6 || dc6->read(&hdr, sizeof hdr) != sizeof hdr) return;
  dc6->seek(sizeof(uint32) * start, SEEK_CUR);
  if (dc6->read(offsets, sizeof(uint32) * count) != sizeof(uint32) * count) return;
  for (int i = 0; i < count; i++)
  {
    dc6->seek(offsets[i], SEEK_SET);
    drawDC6(image, x[i], y[i], NULL, dc6, repl);
  }
}
void D2Data::loadBackground()
{
  struct ClassInfo
  {
    char const* invleft;
    int invleftx, invlefty;
    char const* invright;
    int invrightx, invrighty;
    int twohanded;
    int mana;
    char const* skill;
    int skillid;
  } classes[8] = {
    {"jav", 433, 120, "buc", 649, 135, 0, 15, NULL,          0},
    {"sst", 433, 120, "sst", 664, 120, 1, 35, "SoSkillicon", 0},
    {"wnd", 433, 136, NULL,    0,   0, 0, 25, "NeSkillicon", 8},
    {"ssd", 433, 120, "buc", 649, 135, 0, 15, NULL,          0},
    {"hax", 433, 120, "buc", 649, 135, 0, 10, NULL,          0},
    {"clb", 433, 120, "buc", 649, 135, 0, 20, NULL,          0},
    {"ktr", 433, 120, "buc", 649, 135, 0, 25, NULL,          0},
  };
  ClassInfo* info = &classes[GetTickCount() % 7];

  invBackground = new D2Image(800, 600);

  int stashx[] = { 80, 336,  80, 336};
  int stashy[] = { 61,  61, 317, 317};
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\TradeStash.dc6", 0, 4, stashx, stashy);

  int invenx[] = {400, 656, 400, 656};
  int inveny[] = { 61,  61, 317, 317};
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\invchar6.dc6", 4, 4, invenx, inveny);

  int framex[] = {  0, 256,   0,   0, 256, 400, 544, 713, 544, 400};
  int framey[] = { -2,  -2, 254, 483, 483,  -2,  -2, 254, 483, 483};
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\800BorderFrame.dc6", 0, 10, framex, framey);

  int ctrlpx[] = {  0, 165, 293, 421, 549, 683,  29, 689,  28, 690};
  int ctrlpy[] = {497, 546, 546, 546, 546, 497, 508, 508, 508, 504};
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\800ctrlpnl7.dc6", 0, 6, ctrlpx + 0, ctrlpy + 0);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\hlthmana.dc6",    0, 2, ctrlpx + 6, ctrlpy + 6);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\overlap.dc6",     0, 2, ctrlpx + 8, ctrlpy + 8);

  int miscx[] = {117, 635, 206, 563, 155, 484, 392, 255, 352, 418};
  int miscy[] = {553, 553, 563, 563,  83, 452, 561, 571, 445, 445};
  drawFrames(invBackground, "data\\global\\ui\\SPELLS\\Skillicon.dc6",  2, 1, miscx + 0, miscy + 0, tints[9][1]);
  if (info->skill)
  {
    drawFrames(invBackground, String::format("data\\global\\ui\\SPELLS\\%s.dc6", info->skill),
      info->skillid, 1, miscx + 1, miscy + 1, tints[9][1]);
  }
  else
    drawFrames(invBackground, "data\\global\\ui\\SPELLS\\Skillicon.dc6",  2, 1, miscx + 1, miscy + 1, tints[9][1]);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\level.dc6",       2, 1, miscx + 2, miscy + 2);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\level.dc6",       2, 1, miscx + 3, miscy + 3);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\goldcoinbtn.dc6", 0, 1, miscx + 4, miscy + 4);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\goldcoinbtn.dc6", 0, 1, miscx + 5, miscy + 5);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\menubutton.dc6",  0, 1, miscx + 6, miscy + 6);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\runbutton.dc6",   2, 1, miscx + 7, miscy + 7);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\buysellbtn.dc6", 10, 1, miscx + 8, miscy + 8);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\buysellbtn.dc6", 10, 1, miscx + 9, miscy + 9);

  int invx[] = {535, 536, 651, 420, 535, 609, 495, 609, 420, 651};
  int invy[] = {136, 239, 239, 240,  66,  95, 240, 240, 109, 109};
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_armor.dc6",       0, 1, invx + 0, invy + 0);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_belt.dc6",        0, 1, invx + 1, invy + 1);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_boots.dc6",       0, 1, invx + 2, invy + 2);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_helm_glove.dc6",  0, 2, invx + 3, invy + 3);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_ring_amulet.dc6", 0, 2, invx + 5, invy + 5);
  drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_ring_amulet.dc6", 1, 1, invx + 7, invy + 7);

  int itemsx[] = {423, 454, 485, 516};
  int itemsy[] = {563, 563, 563, 563};
  drawFrames(invBackground, "data\\global\\items\\invhp1.dc6", 0, 1, itemsx + 0, itemsy + 0);
  drawFrames(invBackground, "data\\global\\items\\invhp1.dc6", 0, 1, itemsx + 1, itemsy + 1);
  drawFrames(invBackground, "data\\global\\items\\invhp1.dc6", 0, 1, itemsx + 2, itemsy + 2);
  drawFrames(invBackground, "data\\global\\items\\invhp1.dc6", 0, 1, itemsx + 3, itemsy + 3);

  if (info->invleft)
  {
    drawFrames(invBackground, String::format("data\\global\\items\\inv%s.dc6", info->invleft), 0, 1,
      &info->invleftx, &info->invlefty);
  }
  else
    drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_weapons.dc6", 0, 1, invx + 8, invy + 8);
  if (info->invright)
  {
    drawFrames(invBackground, String::format("data\\global\\items\\inv%s.dc6", info->invright), 0, 1,
      &info->invrightx, &info->invrighty);
  }
  else
    drawFrames(invBackground, "data\\global\\ui\\PANEL\\inv_weapons.dc6", 0, 1, invx + 9, invy + 9);
  if (info->twohanded)
    invBackground->fill(651, 107, 706, 219, 8, blendTable[2]);

  invBackground->fill(273, 573, 375, 591, 109, blendTable[0]);

  D2Font* font = getFont();
  drawText(invBackground, 158, 120, "Gold Max: 2500000", -1, font, 0);
  drawText(invBackground, 245,  95, "0", -1, font, 0);
  drawText(invBackground, 508, 463, "0", -1, font, 0);
  drawText(invBackground, 664, 500, String::format("Mana: %d / %d", info->mana, info->mana), -1, font, 0);
  drawText(invBackground, 425, 584, "1", -1, font, 4);
  drawText(invBackground, 456, 584, "2", -1, font, 4);
  drawText(invBackground, 487, 584, "3", -1, font, 4);
  drawText(invBackground, 518, 584, "4", -1, font, 4);
}

D2Font* D2Data::getFont(char const* path)
{
  if (!path) path = "data\\local\\font\\latin\\font16";
  if (fonts.has(path))
    return fonts.get(path);
  String dc6path(path); String::setExtension(dc6path, ".dc6");
  String tblpath(path); String::setExtension(tblpath, ".tbl");
  LocalPtr<File> dc6 = loader.load(dc6path);
  LocalPtr<File> tbl = loader.load(tblpath);
  if (!dc6 || !tbl) return NULL;
  DC6_HEADER hdr;
  if (dc6->read(&hdr, sizeof hdr) != sizeof hdr || hdr.version != 6) return NULL;
  uint32 offsetStart = dc6->tell();

  dc6->seek(0, SEEK_END);
  uint32 dc6size = dc6->tell();
  LocalPtr<uint8> dc6data = new uint8[dc6size];
  dc6->seek(0, SEEK_SET);
  dc6->read(dc6data, dc6size);

  LocalPtr<D2Font> font = new D2Font;
  tbl->seek(10, SEEK_SET);
  font->m_height = tbl->getc();
  font->m_width = tbl->getc();
  font->m_glyphFile = File::memfile(dc6data.yield(), dc6size);
  memset(font->m_glyphOffset, 0, sizeof font->m_glyphOffset);

  for (int i = 0; i < 256; i++)
  {
    int chr = tbl->getc();
    tbl->seek(2, SEEK_CUR);
    font->m_gwidth[chr] = tbl->getc();
    font->m_gheight[chr] = tbl->getc();
    tbl->seek(3, SEEK_CUR);
    uint32 offset = tbl->getc();
    tbl->seek(5, SEEK_CUR);

    dc6->seek(offsetStart + offset * sizeof(uint32), SEEK_SET);
    font->m_glyphOffset[chr] = dc6->read32();
  }

  D2Font* result = font.yield();
  fonts.create(path) = result;
  return result;
}
void D2Data::drawText(D2Image* image, int x, int y, char const* text, int length, D2Font* font, int color)
{
  if (length < 0) length = strlen(text);
  for (int i = 0; i < length; i++)
  {
    if (!strncmp(text + i, "\\xffc", 5) && text[i + 5])
    {
      i += 5;
      if (text[i] >= '0' && text[i] <= '<')
        color = text[i] - '0';
    }
    else
    {
      int chr = int((uint8) text[i]);
      if (!font->m_glyphOffset[chr]) continue;
      font->m_glyphFile->seek(font->m_glyphOffset[chr], SEEK_SET);
      drawDC6(image, x, y - font->m_gheight[chr], NULL, font->m_glyphFile, color ? tints[9][color] : NULL);
      x += font->m_gwidth[chr];
    }
  }
}
int D2Font::width(char const* text, int length) const
{
  int result = 0;
  if (length < 0) length = strlen(text);
  int maxlen = 0;
  for (int i = 0; i < length; i++)
  {
    if (!strncmp(text + i, "\\xffc", 5) && text[i + 5])
      i += 5;
    else if (!strncmp(text + i, "\\n", 2))
    {
      maxlen = max(maxlen, result);
      result = 0;
      i += 1;
    }
    else
    {
      int chr = int((uint8) text[i]);
      if (!m_glyphOffset[chr]) continue;
      result += m_gwidth[chr];
    }
  }
  return max(result, maxlen);
}

D2Image* D2Data::getImage(char const* path)
{
  File* file = loadImage(path);
  DC6_FRAME frame;
  if (file->read(&frame, sizeof frame) != sizeof frame) return NULL;
  LocalPtr<D2Image> image = new D2Image(frame.width, frame.height);
  drawDC6(image, 0, 0, &frame, file);
  return image.yield();
}
  
File* D2Data::loadImageByName(char const* name)
{
  if (!name[0]) return NULL;
  File* invfile = loadImage(String::format("data\\global\\items\\%s.dc6", name));
  if (invfile) return invfile;
  int index = -1;
  D2BaseItem* base = baseItems.getptr(name);
  if (!base)
  {
    String tname(name);
    if (!s_isdigit(tname[tname.length() - 1])) return NULL;
    index = (tname[tname.length() - 1] - '1');
    tname.resize(tname.length() - 1);
    base = baseItems.getptr(tname);
  }
  if (!base) return NULL;
  if (index >= 0 && base->type && index < base->type->invgfx.length())
    invfile = loadImage(String::format("data\\global\\items\\%s.dc6", base->type->invgfx[index]));
  if (!invfile)
    invfile = loadImage(String::format("data\\global\\items\\%s.dc6", base->invfile));
  return invfile;
}
