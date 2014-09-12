#include "d2mule.h"
#include "d2data.h"
#include "base/json.h"
#include "base/regexp.h"
#include "base/ptr.h"

D2Container::~D2Container()
{
  for (int i = 0; i < items.length(); i++)
    items[i]->release();
  for (int i = 0; i < sub.length(); i++)
    delete sub[i];
}
void D2Container::populate(Array<D2Item*>& itemlist)
{
  itemlist.append(items);
  for (int i = 0; i < sub.length(); i++)
    sub[i]->populate(itemlist);
}
bool D2Container::parseDir(wchar_t const* path, D2Data* d2data)
{
  if (!path) return false;
  items.clear();
  WIN32_FIND_DATA data;
  HANDLE hFind = FindFirstFile(WideString::buildFullName(path, L"*"), &data);
  if (hFind == INVALID_HANDLE_VALUE)
    return false;
  Array<D2Container*> newSub;
  bool changes = false;
  do
  {
    if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
      continue;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
      D2Container* cur = find(data.cFileName);
      if (!cur)
      {
        cur = new D2Container(data.cFileName, this);
        newSub.push(cur);
      }
      if (cur->parseDir(WideString::buildFullName(path, data.cFileName), d2data))
        changes = true;
    }
    else if (!WideString::getExtension(data.cFileName).icompare(L".txt"))
    {
      WideString curName(WideString::getFileTitle(data.cFileName));
      D2Container* cur = find(curName);
      if (!cur)
      {
        cur = new D2Container(curName, this);
        newSub.push(cur);
      }
      if (cur->parseFile(WideString::buildFullName(path, data.cFileName), d2data))
        changes = true;
    }
  } while (FindNextFile(hFind, &data));
  FindClose(hFind);
  sub.append(newSub);
  sub.sort(compare);
  return changes;
}
bool D2Container::parseFile(wchar_t const* path, D2Data* d2data)
{
  sub.clear();
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(path, GetFileExInfoStandard, &attr))
    return false;
  uint64 lastWrite = uint64(attr.ftLastWriteTime.dwLowDateTime) | (uint64(attr.ftLastWriteTime.dwHighDateTime) << 32);
  if (lastWrite <= lastUpdate)
    return false;
  lastUpdate = lastWrite;
  for (int i = 0; i < items.length(); i++)
    items[i]->release();
  items.clear();
  LocalPtr<File> file = File::wopen(path, File::READ);
  String line;
  String header(WideString::format(L"%s / %s", parent->name, name));
  while (file->gets(line))
  {
    line.trim();
    if (line.empty()) continue;
    LocalPtr<json::Value> value = json::Value::parse(TempFile(File::memfile(line.c_str(), line.length(), false)));
    if (value && value->type() == json::Value::tObject
              && value->hasProperty("itemColor", json::Value::tNumber)
              && value->hasProperty("image", json::Value::tString)
              && value->hasProperty("title", json::Value::tString)
              && value->hasProperty("description", json::Value::tString)
              && value->hasProperty("header", json::Value::tString)
              && value->hasProperty("sockets", json::Value::tArray))
    {
      D2Item* item = new D2Item(this);
      item->invColor = value->get("itemColor")->getInteger();
      item->image = value->get("image")->getString();
      item->title = value->get("title")->getString();
      item->description = value->get("description")->getString();
      item->header = value->get("header")->getString();
      if (!item->header.length())
        item->header = header;
      json::Value* sockets = value->get("sockets");
      for (uint32 i = 0; i < sockets->length(); i++)
      {
        json::Value* sock = sockets->at(i);
        if (sock->type() == json::Value::tString)
          item->sockets.push(sock->getString());
      }
      item->parse(d2data);
      items.push(item);
    }
  }
  return true;
}
D2Container* D2Container::find(wchar_t const* subname)
{
  int left = 0;
  int right = sub.length();
  while (left < right)
  {
    int mid = (left + right) / 2;
    int cmp = WideString::smartCompare(sub[mid]->name, subname);
    if (cmp == 0) return sub[mid];
    if (cmp < 0) left = mid + 1;
    else right = mid;
  }
  return NULL;
}

void D2Item::parse(D2Data* data)
{
  int underscore = title.indexOf('_');
  if (underscore >= 0 && title.substr(0, underscore).isDigits())
    title.cut(0, underscore + 1);

  String desc = description;
  String gidInfo;
  int gidSep = desc.lastIndexOf('$');
  if (gidSep >= 0)
  {
    gidInfo = desc.substring(gidSep + 1);
    gid = atoi(gidInfo);
    desc.resize(gidSep);
  }
  Array<String> lines;
  desc.split(lines, "\\n");
  if (!lines.length())
    return;

  int ilvlSep = lines[0].lastIndexOf('(');
  if (ilvlSep >= 0)
  {
    itemLevel = atoi(lines[0].c_str() + ilvlSep + 1);
    lines[0].resize(ilvlSep);
    lines[0].trim();
  }
  while (!strncmp(lines[0], "\\xffc", 5))
  {
    colorCode = (lines[0][5] - '0');
    lines[0].cut(0, 6);
  }
  re::Prog remover("\\\\xffc[0-9:;<]");
  for (int i = 0; i < lines.length(); i++)
    lines[i] = remover.replace(lines[i], "");

  if (desc.find(data->getLocalizedString(5203)) >= 0)
    flags |= D2Item::fEthereal;
  if (desc.find(data->getLocalizedString(3455)) >= 0)
    flags |= D2Item::fUnidentified;

  if (!(flags & D2Item::fUnidentified) && (colorCode == 2 || colorCode == 4 || colorCode == 1))
    unique = data->getUniqueItem(lines[0]);
  if (!base && lines.length() > 1 && colorCode != 0 && colorCode != 3 && colorCode != 5)
    base = data->getBaseItem(lines[1]);
  if (!base && unique)
    base = unique->base;
  if (!base)
    base = data->getBaseItem(lines[0], true);
  if (!base)
    return;

  if (!unique && (flags & D2Item::fUnidentified))
  {
    unique = data->getUniqueByBase(base, colorCode);
    if (unique)
      title = String::format("[%s] %s", unique->name, base->name);
  }

  D2ItemType* info = base->type->getInfo();
  if (info)
  {
    type = info->type;
    subType = info->subType;
    quality = info->quality;
  }
  static int colorToQuality[] = {qNormal, qNormal, qSet, qMagic, qUnique, qNormal, 0, 0, qCrafted, qRare, 0, 0, 0};
  static int qualityToType[] = {tMisc, tWhite, tWhite, tWhite, tMagic, tSet, tRare, tUnique, tCrafted};
  if (quality < 0)
  {
    if (unique)
      quality = (unique->type == 6 ? qUnique : colorToQuality[unique->type]);
    else
      quality = colorToQuality[colorCode];
  }
  if (type == tAuto)
  {
    if (unique && unique->type == 6)
      type = tRuneword;
    else
      type = qualityToType[quality];
  }

  if (colorCode == 1)
  {
    if (unique)
      colorCode = (unique->type == 2 ? 2 : 4);
    // more broken detection?
  }

  if (!title.length())
  {
    title = lines[0];
    if (quality == D2Item::qSet || quality == D2Item::qRare ||
      quality == D2Item::qCrafted || quality == D2Item::qUnique)
      title = String::format("%s %s", lines[0], base->name);
  }

  if (!base || base->type->isType("rune") || base->type->isType("gem"))
    return;

  if (sockets.length())
  {
    Array<String> jewDesc;
    gidInfo.split(jewDesc, "\n\n");
    int curJew = 1;
    for (int sox = 0; sox < sockets.length(); sox++)
    {
      D2BaseItem* soxBase = data->getBaseByCode(sockets[sox]);
      if (!soxBase)
      {
        String tname(sockets[sox]);
        if (s_isdigit(tname[tname.length() - 1]))
        {
          tname.resize(tname.length() - 1);
          soxBase = data->getBaseByCode(tname);
        }
      }
      if (!soxBase)
        socketItems.push(NULL);
      else
      {
        D2Item* sock = new D2Item;
        sock->socketParent = this;
        sock->image = sockets[sox];
        sock->base = soxBase;
        if (soxBase->type->isType("jewl") && curJew < jewDesc.length())
          sock->description = jewDesc[curJew++];
        else
        {
          sock->title = soxBase->name;
          sock->description = soxBase->gemDesc;
        }
        sock->header = String::format("%s (Socketed)", header);
        sock->parse(data);
        socketItems.push(sock);
      }
    }
  }

  D2StatData* statData = data->getStatData();
  for (int i = 0; i < lines.length(); i++)
    statData->parse(stats, lines[i]);
  statText = statData->process(this);
}
