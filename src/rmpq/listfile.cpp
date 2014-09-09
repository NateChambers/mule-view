#include "impl.h"

namespace mpq
{

ListFile::ListFile(::File* file)
{
  impl = new Impl;
  impl->sorted = false;
  if (file)
  {
    String line;
    while (file->gets(line))
    {
      line.trim();
      if (!line.empty())
        impl->filenames.push(line);
    }
  }
}
ListFile::~ListFile()
{
  delete impl;
}

static int StringComp(String const& a, String const& b)
{
  return a.icompare(b);
}
void ListFile::sort()
{
  if (!impl->sorted)
  {
    impl->filenames.sort(StringComp);
    impl->sorted = true;
  }
}
bool ListFile::has(char const* name)
{
  if (impl->sorted)
  {
    int left = 0;
    int right = impl->filenames.length();
    while (left < right)
    {
      int mid = (left + right) / 2;
      int cmp = impl->filenames[mid].icompare(name);
      if (cmp == 0)
        return true;
      if (cmp < 0)
        left = mid + 1;
      else
        right = mid;
    }
    return false;
  }
  else
  {
    for (int i = 0; i < impl->filenames.length(); i++)
      if (!impl->filenames[i].icompare(name))
        return true;
    return false;
  }
}
void ListFile::merge(ListFile* list)
{
  sort();
  list->sort();
  Array<String> a = impl->filenames;
  Array<String> b = list->impl->filenames;
  impl->filenames.clear();
  for (int ia = 0, ib = 0; ia < a.length() || ib < b.length();)
  {
    if (ia < a.length() && ib < b.length() && a[ia].icompare(b[ib]) == 0)
      impl->filenames.push(a[ia++]), ib++;
    else if (ia == a.length() || (ib < b.length() && a[ia].icompare(b[ib]) > 0))
      impl->filenames.push(b[ib++]);
    else
      impl->filenames.push(a[ia++]);
  }
}

uint32 ListFile::insert(char const* name, bool check)
{
  impl->sorted = false;
  if (check)
  {
    for (int i = 0; i < impl->filenames.length(); i++)
      if (!impl->filenames[i].icompare(name))
        return error = Error::Ok;
  }
  impl->filenames.push(name);
  return error = Error::Ok;
}
uint32 ListFile::size() const
{
  return impl->filenames.length();
}
char const* ListFile::item(uint32 pos) const
{
  return impl->filenames[pos];
}
uint32 ListFile::flush(::File* file)
{
  for (int i = 0; i < impl->filenames.length(); i++)
    file->printf("%s\r\n", impl->filenames[i]);
  return error = Error::Ok;
}

}
