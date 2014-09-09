#include "impl.h"
#include "base/ptr.h"

namespace mpq
{

Loader::Loader()
{
}
Loader::Loader(Loader const& loader)
{
  thread::Lock::Holder locker(&loader.mutex);
  for (int i = 0; i < loader.archives.length(); i++)
  {
    ArchiveItem& it = archives.push();
    it.loader = loader.archives[i].loader->retain();
    it.base = loader.archives[i].base;
  }
}
Loader::~Loader()
{
  for (int i = 0; i < archives.length(); i++)
    archives[i].loader->release();
}

void Loader::lock()
{
  mutex.acquire();
}
void Loader::unlock()
{
  mutex.release();
}

void Loader::clear()
{
  thread::Lock::Holder locker(&mutex);
  for (int i = 0; i < archives.length(); i++)
    archives[i].loader->release();
  archives.clear();
}
void Loader::addLoader(FileLoader* loader)
{
  thread::Lock::Holder locker(&mutex);
  ArchiveItem& it = archives.push();
  it.loader = loader->retain();
}
void Loader::removeLoader(FileLoader* loader)
{
  thread::Lock::Holder locker(&mutex);
  for (int i = 0; i < archives.length(); i++)
  {
    if (archives[i].loader == loader)
    {
      archives[i].loader->release();
      archives.remove(i--);
    }
  }
}
void Loader::addArchive(Archive* archive, char const* base)
{
  thread::Lock::Holder locker(&mutex);
  ArchiveItem& it = archives.push();
  it.loader = archive->retain();
  it.base = (base ? base : "");
}
Archive* Loader::loadArchive(wchar_t const* path, char const* base)
{
  Archive* archive = Archive::open(path, ::File::READ);
  if (archive)
  {
    thread::Lock::Holder locker(&mutex);
    ArchiveItem& it = archives.push();
    it.loader = archive;
    it.base = (base ? base : "");
  }
  return archive;
}

::File* Loader::load(char const* name)
{
  thread::Lock::Holder locker(&mutex);
  Array<LocalPtr<File> > patches;
  for (int i = archives.length() - 1; i >= 0; i--)
  {
    String curName = (archives[i].base.empty() ? name : String::buildFullName(archives[i].base, name));
    LocalPtr<::File> file = archives[i].loader->load(curName);
    if (file)
    {
      File* mpqFile = dynamic_cast<File*>(file.ptr());
      if (mpqFile)
      {
        if (mpqFile->attribute(File::Attribute::Flags) & File::Flags::DummyFile)
          return NULL;
        if (mpqFile->isPatchFile())
          patches.push((File*) file.yield());
        else
        {
          for (int p = patches.length() - 1; p >= 0; p--)
          {
            ::File* patched = patches[p]->patchFile(file);
            if (patched)
              file = patched;
          }
          return file.yield();
        }
      }
      else
        return file.yield();
    }
  }
  return NULL;
}
ListFile* Loader::buildListFile()
{
  ListFile* list = new ListFile;
  for (int i = 0; i < archives.length(); i++)
  {
    LocalPtr<::File> file = archives[i].loader->load("(listfile)");
    if (file)
    {
      ListFile temp(file);
      list->merge(&temp);
    }
  }
  return list;
}

}
