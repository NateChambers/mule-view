#include "impl.h"
#include "base/regexp.h"

namespace mpq
{

void Archive::postLoad()
{
  if (impl)
  {
    impl->archive = this;
    impl->loadListFile();
  }
}

/////////////////////////////////////////////

Archive::Archive(::File* file, uint32 flags)
{
  impl = Impl::open(file, NULL, ::File::READ, flags);
  postLoad();
}
Archive::Archive(wchar_t const* filename, uint32 mode, uint32 flags)
{
  impl = Impl::open(NULL, filename, mode, flags);
  postLoad();
}
Archive* Archive::open(::File* file, uint32 flags)
{
  Impl* impl = Impl::open(file, NULL, ::File::READ, flags);
  return (impl ? new Archive(impl) : NULL);
}
Archive* Archive::open(wchar_t const* filename, uint32 mode, uint32 flags)
{
  Impl* impl = Impl::open(NULL, filename, mode, flags);
  return (impl ? new Archive(impl) : NULL);
}
Archive::~Archive()
{
  delete impl;
}

uint32 Archive::listFiles(ListFile* list)
{
  if (impl == NULL || list == NULL)
    return serror = error = Error::Params;

  if (impl->fileNames == NULL)
    return error = Error::Ok;

  thread::Lock::Holder locker(impl->mutex);

  for (uint32 i = 0; i < list->size(); i++)
    impl->addName(list->item(i));

  return error = Error::Ok;
}

uint32 Archive::getFlags() const
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  return error = Error::Ok, impl->flags;
}
uint32 Archive::setFlags(uint32 flags, uint32 mask)
{
  if (impl == NULL)
    return serror = error = Error::Params;
  impl->setFlags(flags, mask);
  return error = Error::Ok;
}
uint32 Archive::getCompression() const
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  return error = Error::Ok, impl->compression;
}
uint32 Archive::setCompression(uint32 method)
{
  if (impl == NULL)
    return serror = error = Error::Params;
  impl->compression = method;
  return error = Error::Ok;
}

uint32 Archive::getFileIndex(char const* name, uint16 locale) const
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params;

  thread::Lock::Holder locker(impl->mutex);

  if (impl->hetTable)
  {
    uint64 hash = (hashJenkins(name) & impl->hetTable->hashMask) | impl->hetTable->hashOr;
    uint8 hetHash = uint8(hash >> impl->hetTable->hashShift);
    uint64 betHash = hash & impl->betTable->hashMask;

    uint32 count = 0;
    for (uint32 cur = uint32(hash % impl->hetTable->hashTableSize);
      count < impl->hetTable->hashTableSize && impl->hetTable->hetTable[cur] != HetTable::EMPTY;
      count++, cur = (cur + 1) % impl->hetTable->hashTableSize)
    {
      if (impl->hetTable->hetTable[cur] == hetHash)
      {
        uint32 betIndex = impl->hetTable->betIndexTable[cur];
        if (betIndex < impl->betTable->numFiles && impl->betTable->betTable[betIndex].hash == betHash)
        {
          if (impl->fileNames && impl->fileNames[cur].empty())
            impl->fileNames[cur] = name;
          return cur;
        }
      }
    }
  }
  if (impl->hashTable)
  {
    uint32 name1 = hashString(name, HASH_NAME1);
    uint32 name2 = hashString(name, HASH_NAME2);
    uint32 count = 0;

    uint32 best = Error::NoFile;
    for (uint32 cur = hashString(name, HASH_OFFSET) % impl->hdr.hashTableSize;
      count < impl->hdr.hashTableSize && impl->hashTable[cur].blockIndex != MPQHashEntry::EMPTY;
      count++, cur = (cur + 1) % impl->hdr.hashTableSize)
    {
      if (impl->hashTable[cur].blockIndex != MPQHashEntry::DELETED &&
        impl->hashTable[cur].name1 == name1 && impl->hashTable[cur].name2 == name2)
      {
        if (impl->fileNames && impl->fileNames[cur].empty())
          impl->fileNames[cur] = name;
        if (impl->hashTable[cur].locale == locale)
          return cur;
        if (best >= impl->hdr.hashTableSize || impl->hashTable[cur].locale == Locale::Neutral)
          best = cur;
      }
    }
    return best;
  }

  return error = Error::NoFile;
}

uint32 Archive::saveAs(::File* file) const
{
  if (impl == NULL)
    return serror = error = Error::Params;
  return impl->flush(file, false);
}

uint32 Archive::saveAs(wchar_t const* filename)
{
  if (impl == NULL || filename == NULL)
    return serror = error = Error::Params;

  thread::Lock::Holder locker(impl->mutex);

  ::File* file = ::File::longwopen(filename, ::File::REWRITE);
  if (file == NULL)
    return serror = error = Error::Access;

  saveAs(file);

  delete file;
  file = ::File::longwopen(filename, ::File::MODIFY);
  if (file)
  {
    delete impl->file;
    if ((impl->flags & Flags::Temporary) && !impl->path.empty())
      ::File::remove(impl->path);
    impl->file = file;
    impl->path = filename;
    impl->flags &= ~(Flags::Temporary | Flags::ReadOnly);
  }
  else
    return serror = error = Error::Access;

  return error = Error::Ok;
}
uint32 Archive::makeTemp()
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->flags & Flags::Temporary)
    return error = Error::Ok;
  thread::Lock::Holder locker(impl->mutex);
  if (saveAs(::File::tempname()))
    return error;
  impl->flags |= Flags::Temporary;
  return error = Error::Ok;
}

bool Archive::fileExists(char const* name) const
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params, false;
  return getFileIndex(name) < getMaxFiles();
}
bool Archive::fileExists(char const* name, uint16 locale) const
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params, false;
  error = Error::Ok;
  if (impl->hetTable)
    return getFileIndex(name) < impl->hetTable->hashTableSize;
  else
  {
    uint32 pos = getFileIndex(name, locale);
    if (pos >= impl->hdr.hashTableSize)
      return false;
    return impl->hashTable[pos].locale == locale;
  }
}
uint32 Archive::getMaxFiles() const
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  if (impl->hetTable)
    return error = Error::Ok, impl->hetTable->hashTableSize;
  else
    return error = Error::Ok, impl->hdr.hashTableSize;
}
bool Archive::fileExists(uint32 pos) const
{
  if (impl == NULL)
    return serror = error = Error::Params, false;
  if (impl->hetTable)
    return error = Error::Ok, impl->hetTable->hetTable[pos] != HetTable::EMPTY &&
      impl->hetTable->betIndexTable[pos] < impl->betTable->numFiles;
  else
    return error = Error::Ok, impl->hashTable[pos].blockIndex != MPQHashEntry::EMPTY &&
      impl->hashTable[pos].blockIndex != MPQHashEntry::DELETED;
}
bool Archive::testFile(uint32 pos) const
{
  if (impl == NULL)
    return serror = error = Error::Params, false;
  uint64 filePos;
  uint32 fileSize, cmpSize, flags;
  if (impl->hetTable)
  {
    if (pos >= impl->hetTable->hashTableSize || impl->hetTable->hetTable[pos] == HetTable::EMPTY)
      return false;
    uint32 bet = impl->hetTable->betIndexTable[pos];
    if (bet >= impl->betTable->numFiles)
      return false;
    filePos = impl->betTable->betTable[bet].filePos;
    fileSize = impl->betTable->betTable[bet].fileSize;
    cmpSize = impl->betTable->betTable[bet].cmpSize;
    flags = impl->betTable->betTable[bet].flags;
  }
  else
  {
    if (pos >= impl->hdr.hashTableSize || impl->hashTable[pos].blockIndex == MPQHashEntry::EMPTY ||
        impl->hashTable[pos].blockIndex == MPQHashEntry::DELETED)
      return false;
    uint32 block = impl->hashTable[pos].blockIndex;
    if (block >= impl->hdr.blockTableSize)
      return false;
    filePos = uint64(impl->blockTable[block].filePos);
    if (impl->hiBlockTable)
      filePos |= uint64(impl->hiBlockTable[block]) << 32;
    fileSize = impl->blockTable[block].fSize;
    cmpSize = impl->blockTable[block].cSize;
    flags = impl->blockTable[block].flags;
  }
  if (!(flags & File::Flags::Encrypted))
    return true;
  if (impl->fileNames && !impl->fileNames[pos].empty())
    return true;
  impl->file->seek(impl->offset + filePos, SEEK_SET);
  if (flags & File::Flags::PatchFile)
  {
    uint32 patchLength = impl->file->read32();
    impl->file->seek(patchLength - sizeof(uint32), SEEK_CUR);
  }
  if ((flags & File::Flags::SingleUnit) || !(flags & File::Flags::Compressed))
  {
    uint32 rSize = (cmpSize > 12 ? 12 : cmpSize);
    memset(impl->buf, 0, 12);
    if (impl->file->read(impl->buf, rSize) != rSize)
      return false;
    if (detectFileSeed((uint32*) impl->buf, fileSize) != 0)
      return true;
  }
  else
  {
    uint32 tableSize = 4 * ((fileSize + impl->blockSize - 1) / impl->blockSize);
    if (flags & File::Flags::SectorCrc)
      tableSize += 4;
    if (impl->file->read(impl->buf, 8) != 8)
      return false;
    if (detectTableSeed((uint32*) impl->buf, tableSize, impl->blockSize) != 0)
      return true;
  }
  return false;
}

File* Archive::openFile(char const* name, uint32 mode)
{
  return openFile(name, Locale::Neutral, mode);
}
File* Archive::openFile(char const* name, uint16 locale, uint32 mode)
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params, NULL;
  thread::Lock::Holder locker(impl->mutex);
  uint32 pos = getFileIndex(name, locale);
  if (pos < getMaxFiles())
  {
    File::Impl* fimpl = File::open(this, pos, mode, hashString(stripPath(name), HASH_KEY), 1);
    return fimpl ? new File(fimpl) : NULL;
  }
  else if (mode == ::File::REWRITE)
  {
    uint32 flags = File::Flags::Exists | File::Flags::CompressMulti;
    if (impl->flags & Flags::Encrypted)
    {
      flags |= File::Flags::Encrypted;
      if (impl->flags & Flags::FixSeedEncryption)
        flags |= File::Flags::FixSeed;
    }
    if (impl->hetTable)
    {
      if (impl->betTable->numFiles >= impl->betTable->maxFiles)
      {
        impl->betTable->maxFiles += BLOCK_GROW_STEP;
        BetTable* temp = (BetTable*) new uint8[sizeof(BetTable) + impl->betTable->maxFiles * sizeof(BetEntry)];
        memcpy(temp, impl->betTable, sizeof(BetTable) + impl->betTable->numFiles * sizeof(BetEntry));
        temp->betTable = (BetEntry*) (temp + 1);
        delete[] (uint8*) impl->betTable;
        impl->betTable = temp;
      }
      uint32 bet = impl->betTable->numFiles++;
      impl->betTable->betTable[bet].flags = flags;
      impl->betTable->betTable[bet].fileSize = 0;
      impl->betTable->betTable[bet].cmpSize = 0;
      impl->betTable->betTable[bet].filePos = impl->allocBlock(impl->compression ? 4 : 0);
      if (impl->compression)
      {
        impl->file->seek(impl->offset + impl->betTable->betTable[bet].filePos, SEEK_SET);
        impl->file->write32(4);
      }
      pos = impl->createEntry(name, bet);
      impl->flushTables();
    }
    else
    {
      if (impl->hdr.blockTableSize >= impl->blockTableSpace)
      {
        impl->blockTableSpace += BLOCK_GROW_STEP;
        MPQBlockEntry* temp = (MPQBlockEntry*) new uint8[impl->blockTableSpace * sizeof(MPQBlockEntry)];
        memcpy(temp, impl->blockTable, impl->blockTableSpace * sizeof(MPQBlockEntry));
        delete[] (uint8*) impl->blockTable;
        impl->blockTable = temp;
      }
      uint32 block = impl->hdr.blockTableSize++;
      impl->blockTable[block].flags = flags;
      impl->blockTable[block].fSize = 0;
      impl->blockTable[block].cSize = 0;
      impl->blockTable[block].filePos = impl->allocBlock(impl->compression ? 4 : 0);
      if (impl->compression)
      {
        impl->file->seek(impl->offset + impl->blockTable[block].filePos, SEEK_SET);
        impl->file->write32(4);
      }
      pos = impl->createEntry(name, block);
      impl->flushTables();
    }
    if (pos >= getMaxFiles())
      return NULL;
    File::Impl* fimpl = File::open(this, pos, mode, hashString(stripPath(name), HASH_KEY), 1);
    return fimpl ? new File(fimpl) : NULL;
  }
  else
    return error = Error::NoFile, NULL;
}
File* Archive::openFile(uint32 pos, uint32 mode)
{
  if (impl == NULL)
    return serror = error = Error::Params, NULL;
  if ((impl->flags & Flags::ReadOnly) && mode != ::File::READ)
    return error = Error::ReadOnly, NULL;
  thread::Lock::Holder locker(impl->mutex);
  if (impl->hetTable)
  {
    if (pos >= impl->hetTable->hashTableSize ||
        impl->hetTable->hetTable[pos] == HetTable::EMPTY ||
        impl->hetTable->betIndexTable[pos] >= impl->betTable->numFiles)
      return error = Error::NoFile, NULL;
  }
  else
  {
    if (pos >= impl->hdr.hashTableSize ||
        impl->hashTable[pos].blockIndex == MPQHashEntry::EMPTY ||
        impl->hashTable[pos].blockIndex == MPQHashEntry::DELETED)
      return error = Error::NoFile, NULL;
  }
  File::Impl* fimpl = File::open(this, pos, mode, 0, 0);
  return fimpl ? new File(fimpl) : NULL;
}

uint32 Archive::getFirstFileIndex(char const* name) const
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params;

  thread::Lock::Holder locker(impl->mutex);

  if (impl->hetTable)
  {
    uint64 hash = (hashJenkins(name) & impl->hetTable->hashMask) | impl->hetTable->hashOr;
    uint8 hetHash = uint8(hash >> impl->hetTable->hashShift);
    uint64 betHash = hash & impl->betTable->hashMask;

    uint32 count = 0;
    for (uint32 cur = uint32(hash % impl->hetTable->hashTableSize);
      count < impl->hetTable->hashTableSize && impl->hetTable->hetTable[cur] != HetTable::EMPTY;
      count++, cur = (cur + 1) % impl->hetTable->hashTableSize)
    {
      if (impl->hetTable->hetTable[cur] == hetHash)
      {
        uint32 betIndex = impl->hetTable->betIndexTable[cur];
        if (betIndex < impl->betTable->numFiles && impl->betTable->betTable[betIndex].hash == betHash)
        {
          if (impl->fileNames && impl->fileNames[cur].empty())
            impl->fileNames[cur] = name;
          return cur;
        }
      }
    }
  }
  if (impl->hashTable)
  {
    uint32 name1 = hashString(name, HASH_NAME1);
    uint32 name2 = hashString(name, HASH_NAME2);
    uint32 count = 0;

    for (uint32 cur = hashString(name, HASH_OFFSET) % impl->hdr.hashTableSize;
      count < impl->hdr.hashTableSize && impl->hashTable[cur].blockIndex != MPQHashEntry::EMPTY;
      count++, cur = (cur + 1) % impl->hdr.hashTableSize)
    {
      if (impl->hashTable[cur].blockIndex != MPQHashEntry::DELETED &&
        impl->hashTable[cur].name1 == name1 && impl->hashTable[cur].name2 == name2)
      {
        if (impl->fileNames && impl->fileNames[cur].empty())
          impl->fileNames[cur] = name;
        return cur;
      }
    }
  }

  return error = Error::NoFile;
}
uint32 Archive::getNextFileIndex(char const* name, uint32 start) const
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params;

  thread::Lock::Holder locker(impl->mutex);

  if (impl->hetTable)
    return error = Error::NoFile;
  if (impl->hashTable)
  {
    uint32 name1 = hashString(name, HASH_NAME1);
    uint32 name2 = hashString(name, HASH_NAME2);

    for (uint32 cur = (start + 1) % impl->hdr.hashTableSize;
      cur != start && impl->hashTable[cur].blockIndex != MPQHashEntry::EMPTY;
      cur = (cur + 1) % impl->hdr.hashTableSize)
    {
      if (impl->hashTable[cur].blockIndex != MPQHashEntry::DELETED &&
        impl->hashTable[cur].name1 == name1 && impl->hashTable[cur].name2 == name2)
      {
        if (impl->fileNames && impl->fileNames[cur].empty())
          impl->fileNames[cur] = name;
        return cur;
      }
    }
  }

  return error = Error::NoFile;
}

char const* Archive::getFileName(uint32 pos) const
{
  if (impl == NULL)
    return serror = error = Error::Params, NULL;

  error = Error::Ok;
  if (impl->fileNames && pos < getMaxFiles())
    return impl->fileNames[pos].empty() ? NULL : impl->fileNames[pos].c_str();
  return NULL;
}
uint32 Archive::findFile(char const* mask) const
{
  if (impl == NULL || mask == NULL)
    return serror = error = Error::Params;
  if (impl->fileNames == NULL)
    return error = Error::NoFile;
  re::Prog re(mask, re::Prog::CaseInsensitive);
  uint32 count = getMaxFiles();
  for (uint32 i = 0; i < count; i++)
    if (!impl->fileNames[i].empty() && re.match(impl->fileNames[i]))
      return i;
  return error = Error::NoFile;
}
uint32 Archive::findNextFile(char const* mask, uint32 cur) const
{
  if (impl == NULL || mask == NULL)
    return serror = error = Error::Params;
  if (impl->fileNames == NULL || cur >= getMaxFiles())
    return error = Error::NoFile;
  re::Prog re(mask, re::Prog::CaseInsensitive);
  uint32 count = getMaxFiles();
  for (uint32 i = cur + 1; i < count; i++)
    if (!impl->fileNames[i].empty() && re.match(impl->fileNames[i]))
      return i;
  return error = Error::NoFile;
}

uint32 Archive::renameFile(char const* source, char const* dest)
{
  if (impl == NULL || source == NULL)
    return serror = error = Error::Params;
  uint32 pos = getFileIndex(source);
  if (pos >= getMaxFiles())
    return error = Error::NoFile;
  uint32 npos;
  if (impl->hetTable)
  {
    uint32 bet = impl->hetTable->betIndexTable[pos];
    impl->hetTable->hetTable[pos] = HetTable::DELETED;
    impl->hetTable->betIndexTable[pos] = -1;
    if (impl->fileNames)
      impl->fileNames[pos] = "";
    npos = impl->createEntry(dest, bet);
    if (npos >= impl->hetTable->hashTableSize)
    {
      // how is this possible
      return npos;
    }
    if (impl->fileNames)
      impl->fileNames[npos] = dest;
  }
  else
  {
    uint32 block = impl->hashTable[pos].blockIndex;
    impl->hashTable[pos].blockIndex = MPQHashEntry::DELETED;
    if (impl->fileNames)
      impl->fileNames[pos] = "";
    npos = impl->createEntry(dest, block);
    if (npos >= impl->hdr.hashTableSize)
    {
      // how is this possible
      return npos;
    }
    impl->hashTable[npos].locale = impl->hashTable[pos].locale;
    impl->hashTable[npos].platform = impl->hashTable[pos].platform;
    if (impl->fileNames)
      impl->fileNames[npos] = dest;
  }
  for (File* file = impl->firstFile; file; file = file->impl->next)
    if (file->impl->index == pos)
      file->impl->index = npos;
  impl->flushTables();
  return error = Error::Ok;
}
uint32 Archive::deleteFile(char const* name)
{
  if (impl == NULL || name == NULL)
    return serror = error = Error::Params;
  uint32 pos = getFileIndex(name);
  if (pos < getMaxFiles())
    return deleteFile(pos);
  return error = Error::NoFile;
}
uint32 Archive::deleteFile(uint32 pos)
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (pos >= getMaxFiles())
    return error = Error::NoFile;

  for (File* file = impl->firstFile; file;)
  {
    File* next = file->impl->next;
    if (file->impl->index == pos)
      file->close();
    file = next;
  }
  if (impl->hetTable)
  {
    if (impl->hetTable->hetTable[pos] == HetTable::EMPTY ||
        impl->hetTable->betIndexTable[pos] >= getMaxFiles())
      return error = Error::NoFile;
    uint32 bet = impl->hetTable->betIndexTable[pos];
    if (impl->fileNames)
      impl->fileNames[pos] = "";
    impl->hetTable->hetTable[pos] = HetTable::DELETED;
    impl->hetTable->betIndexTable[pos] = -1;
    impl->betTable->betTable[bet] = impl->betTable->betTable[--impl->betTable->numFiles];
    for (uint32 i = 0; i < impl->hetTable->hashTableSize; i++)
      if (impl->hetTable->hetTable[i] != HetTable::EMPTY &&
          impl->hetTable->betIndexTable[i] == impl->betTable->numFiles)
        impl->hetTable->betIndexTable[i] = bet;
  }
  else
  {
    if (impl->hashTable[pos].blockIndex == MPQHashEntry::EMPTY ||
        impl->hashTable[pos].blockIndex == MPQHashEntry::DELETED)
      return error = Error::NoFile;
    uint32 block = impl->hashTable[pos].blockIndex;
    if (impl->fileNames)
      impl->fileNames[pos] = "";
    impl->hashTable[pos].blockIndex = MPQHashEntry::DELETED;
    impl->blockTable[block] = impl->blockTable[--impl->hdr.blockTableSize];
    for (uint32 i = 0; i < impl->hdr.hashTableSize; i++)
      if (impl->hashTable[i].blockIndex == impl->hdr.blockTableSize)
        impl->hashTable[i].blockIndex = block;
  }
  impl->flushTables();

  return error = Error::Ok;
}

uint32 Archive::itemAttribute(uint32 pos, uint32 attr)
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  if (!fileExists(pos))
    return error = Error::NoFile, 0;
  if (impl->hetTable)
  {
    uint32 bet = impl->hetTable->betIndexTable[pos];
    switch(attr)
    {
    case File::Attribute::Flags:
      return impl->betTable->betTable[bet].flags;
    case File::Attribute::CompressedSize:
      return impl->betTable->betTable[bet].cmpSize;
    case File::Attribute::Size:
      return impl->betTable->betTable[bet].fileSize;
    case File::Attribute::Locale:
      return Locale::Neutral;
    case File::Attribute::Platform:
      return 0;
    case File::Attribute::Offset:
      return impl->betTable->betTable[bet].filePos;
    default:
      return serror = error = Error::Params, 0;
    }
  }
  else
  {
    uint32 block = impl->hashTable[pos].blockIndex;
    switch(attr)
    {
    case File::Attribute::Flags:
      return impl->blockTable[block].flags;
    case File::Attribute::CompressedSize:
      return impl->blockTable[block].cSize;
    case File::Attribute::Size:
      return impl->blockTable[block].fSize;
    case File::Attribute::Locale:
      return impl->hashTable[pos].locale;
    case File::Attribute::Platform:
      return impl->hashTable[pos].platform;
    case File::Attribute::Offset:
      return impl->blockTable[block].filePos;
    default:
      return serror = error = Error::Params, 0;
    }
  }
}

uint32 Archive::resize(uint32 maxFiles)
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->flags & Flags::ReadOnly)
    return serror = error = Error::ReadOnly;
  if (impl->hetTable)
  {
    if (maxFiles < impl->hetTable->hashTableSize)
      return error = Error::Ok;
    HetTable* newTable = (HetTable*) new uint8[sizeof(HetTable) + maxFiles * (sizeof(uint32) + sizeof(uint8))];
    memset(newTable, 0, sizeof(HetTable) + maxFiles * (sizeof(uint32) + sizeof(uint8)));
    newTable->hashTableSize = maxFiles;
    newTable->hetTable = (uint8*) (newTable + 1);
    newTable->betIndexTable = (uint32*) (newTable->hetTable + maxFiles);
    newTable->hashMask = impl->hetTable->hashMask;
    newTable->hashOr = impl->hetTable->hashOr;
    newTable->hashShift = impl->hetTable->hashShift;
    String* newNames = NULL;
    if (impl->fileNames)
      newNames = new String[maxFiles];
    uint32* moves = new uint32[impl->hetTable->hashTableSize];
    for (uint32 cur = 0; cur < impl->hetTable->hashTableSize; cur++)
    {
      moves[cur] = -1;
      if (impl->hetTable->hetTable[cur] = HetTable::EMPTY)
        continue;
      uint32 bet = impl->hetTable->betIndexTable[cur];
      if (bet >= impl->betTable->numFiles)
        continue;
      uint64 hash = (uint64(impl->hetTable->hetTable[cur]) << impl->hetTable->hashShift) | impl->betTable->betTable[bet].hash;

      uint32 count = 0;
      for (uint32 pos = uint32(hash % maxFiles); count < maxFiles; count++, pos = (pos + 1) % maxFiles)
      {
        if (newTable->hetTable[pos] == HetTable::EMPTY)
        {
          moves[cur] = pos;
          if (newNames && impl->fileNames)
            newNames[pos] = impl->fileNames[cur];
          newTable->hetTable[pos] = impl->hetTable->hetTable[cur];
          newTable->betIndexTable[pos] = impl->hetTable->betIndexTable[cur];
          break;
        }
      }
    }
    for (File* file = impl->firstFile; file;)
    {
      File* next = file->impl->next;
      if (file->impl->index >= impl->hetTable->hashTableSize || moves[file->impl->index] >= newTable->hashTableSize)
        file->close();
      else
        file->impl->index = moves[file->impl->index];
      file = next;
    }
    delete[] moves;
    delete[] impl->fileNames;
    impl->fileNames = newNames;
    delete[] (uint8*) impl->hetTable;
    impl->hetTable = newTable;
  }
  else
  {
    if (maxFiles < impl->hdr.hashTableSize)
      return error = Error::Ok;
    uint32 omx = maxFiles;
    maxFiles = 1;
    while (maxFiles < omx)
      maxFiles *= 2;
    MPQHashEntry* newTable = (MPQHashEntry*) new uint8[maxFiles * sizeof(MPQHashEntry)];
    memset(newTable, 0xFF, maxFiles * sizeof(MPQHashEntry));
    String* newNames = NULL;
    if (impl->fileNames)
      newNames = new String[maxFiles];
    uint32* moves = new uint32[impl->hdr.hashTableSize];
    for (uint32 cur = 0; cur < impl->hdr.hashTableSize; cur++)
    {
      moves[cur] = -1;
      if (impl->hashTable[cur].blockIndex == MPQHashEntry::EMPTY ||
          impl->hashTable[cur].blockIndex == MPQHashEntry::DELETED)
        continue;
      uint32 block = impl->hashTable[cur].blockIndex;

      uint32 start = cur;
      if (impl->fileNames && !impl->fileNames[cur].empty())
        start = hashString(impl->fileNames[cur], HASH_OFFSET) % maxFiles;

      uint32 count = 0;
      for (uint32 pos = start; count < maxFiles; count++, pos = (pos + 1) % maxFiles)
      {
        if (newTable[pos].blockIndex == MPQHashEntry::EMPTY)
        {
          moves[cur] = pos;
          if (newNames && impl->fileNames)
            newNames[pos] = impl->fileNames[cur];
          newTable[pos] = impl->hashTable[cur];
          break;
        }
      }
    }
    for (File* file = impl->firstFile; file;)
    {
      File* next = file->impl->next;
      if (file->impl->index >= impl->hdr.hashTableSize || moves[file->impl->index] >= maxFiles)
        file->close();
      else
        file->impl->index = moves[file->impl->index];
      file = next;
    }
    delete[] moves;
    delete[] impl->fileNames;
    impl->fileNames = newNames;
    delete[] (uint8*) impl->hashTable;
    impl->hashTable = newTable;
    impl->hdr.hashTableSize = maxFiles;
  }
  return impl->flushTables();
}

bool Archive::hasUnknowns() const
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->fileNames == NULL)
    return true;
  uint32 count = getMaxFiles();
  for (uint32 pos = 0; pos < count; pos++)
    if (impl->fileNames[pos].empty())
      return true;
  return false;
}
uint32 Archive::fillHashTable()
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->flags & Flags::ReadOnly)
    return serror = error = Error::ReadOnly;
  if (impl->hetTable)
  {
    for (uint32 cur = 0; cur < impl->hetTable->hashTableSize; cur++)
    {
      if (impl->hetTable->hetTable[cur] == HetTable::EMPTY)
      {
        impl->hetTable->hetTable[cur] = HetTable::DELETED;
        impl->hetTable->betIndexTable[cur] = -1;
      }
    }
  }
  else
  {
    for (uint32 cur = 0; cur < impl->hdr.hashTableSize; cur++)
      if (impl->hashTable[cur].blockIndex == MPQHashEntry::EMPTY)
        impl->hashTable[cur].blockIndex = MPQHashEntry::DELETED;
  }
  return impl->flushTables();
}

uint32 Archive::flush()
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->flags & Flags::ReadOnly)
    return serror = error = Error::ReadOnly;
  if (impl->path.empty())
    return serror = error = Error::ReadOnly;
  if (flushListfile() != Error::Ok)
    return error;
  thread::Lock::Holder locker(impl->mutex);

  WideString tempname = ::File::tempname();

  ::File* temp = ::File::longwopen(tempname, ::File::REWRITE);
  if (temp == NULL)
    return serror = error = Error::Access;
  if (impl->flush(temp, true) != Error::Ok)
    return error;
  delete temp;

  delete impl->file;
  ::File::rename(tempname, impl->path);
  impl->file = ::File::longwopen(impl->path, ::File::MODIFY);
  return error = Error::Ok;
}
uint32 Archive::flushTables()
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->flags & Flags::ReadOnly)
    return serror = error = Error::ReadOnly;
  return impl->flushTables();
}
uint32 Archive::flushListfile()
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->flags & Flags::ReadOnly)
    return serror = error = Error::ReadOnly;
  error = Error::Ok;
  if (impl->fileNames)
  {
    File* list = openFile("(listfile)", ::File::REWRITE);
    if (list)
    {
      uint32 count = getMaxFiles();
      for (uint32 cur = 0; cur < count; cur++)
        if (!impl->fileNames[cur].empty())
          list->printf("%s\r\n", impl->fileNames[cur]);
      delete list;
    }
  }
  return error;
}

}
