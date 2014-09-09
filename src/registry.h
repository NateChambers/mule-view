#ifndef __CORE_REGISTRY_H__
#define __CORE_REGISTRY_H__

#include "base/string.h"
#include "base/types.h"
#include "base/dictionary.h"
#include "base/utils.h"

class Config
{
  struct ItemData
  {
    int size;
    uint8* ptr;
    ItemData()
      : size(0)
      , ptr(NULL)
    {}
    ItemData(uint32 s)
      : size(s)
      , ptr(new uint8[s])
    {}
    ~ItemData()
    {
      delete[] ptr;
    }
  };
  Dictionary<ItemData> items;
  void* getitem(char const* name, int size, bool create);
public:
  Config();
  ~Config();

  template<class T>
  bool get(char const* name, T& obj)
  {
    void* ptr = getitem(name, sizeof obj, false);
    if (ptr)
      memcpy(&obj, ptr, sizeof obj);
    return (ptr != NULL);
  }
  template<class T>
  void set(char const* name, T const& obj)
  {
    void* ptr = getitem(name, sizeof obj, true);
    memcpy(ptr, &obj, sizeof obj);
  }
  char const* getstr(char const* name)
  {
    return (char*) getitem(name, -1, false);
  }
  void setstr(char const* name, char const* str)
  {
    char* ptr = (char*) getitem(name, strlen(str) + 1, true);
    strcpy(ptr, str);
  }
  wchar_t const* getwstr(char const* name)
  {
    return (wchar_t*) getitem(name, -1, false);
  }
  void setwstr(char const* name, wchar_t const* str)
  {
    wchar_t* ptr = (wchar_t*) getitem(name, sizeof(wchar_t) * (wcslen(str) + 1), true);
    wcscpy(ptr, str);
  }
};
extern Config cfg;

#endif // __CORE_REGISTRY_H__
