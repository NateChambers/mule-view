#include "impl.h"
#include "base/ptr.h"

namespace mpq
{

uint8* Archive::Impl::loadTable(uint64 pos, uint64 size, uint64 compressedSize, uint32 key)
{
  if (compressedSize == 0 || compressedSize > size)
    compressedSize = size;
  uint8* ptr = new uint8[compressedSize];
  file->seek(pos, SEEK_SET);
  if (file->read(ptr, compressedSize) != compressedSize)
  {
    delete[] ptr;
    return NULL;
  }
  if (key)
    decryptBlock(ptr, compressedSize, key);
  if (compressedSize < size)
  {
    uint8* unc = new uint8[size];
    uint32 unc_size = size;
    if (multi_decompress(ptr, compressedSize, unc, &unc_size) || size != unc_size)
    {
      delete unc;
      delete ptr;
      return NULL;
    }
    delete ptr;
    ptr = unc;
  }
  return ptr;
}
Archive::Impl::~Impl()
{
  closeFiles();
  delete file;
  delete[] buf;
  delete[] fileNames;
  delete[] ((uint8*) hashTable);
  delete[] ((uint8*) blockTable);
  delete[] ((uint8*) hiBlockTable);
  delete[] ((uint8*) hetTable);
  delete[] ((uint8*) betTable);
  delete mutex;
}

void MPQHeader::upgrade(uint64 fileSize)
{
  if (formatVersion <= 1)
  {
    formatVersion = 1;
    headerSize = size_v1;
    archiveSize = fileSize;
    hiBlockTablePos64 = 0;
    hashTablePosHi = 0;
    blockTablePosHi = 0;
  }
  if (formatVersion <= 3)
  {
    if (headerSize < size_v3)
    {
      archiveSize64 = archiveSize;
      if (fileSize >> 32)
      {
        uint64 pos = offset64(hashTablePos, hashTablePosHi) + hashTableSize * sizeof(MPQHashEntry);
        if (pos > archiveSize64)
          archiveSize64 = pos;
        pos = offset64(blockTablePos, blockTablePosHi) + blockTableSize * sizeof(MPQBlockEntry);
        if (pos > archiveSize64)
          archiveSize64 = pos;
        if (hiBlockTablePos64)
        {
          pos = hiBlockTablePos64 + blockTableSize * sizeof(uint16);
          if (pos > archiveSize64)
            archiveSize64 = pos;
        }
      }

      hetTablePos64 = 0;
      betTablePos64 = 0;
    }

    hetTableSize64 = 0;
    betTableSize64 = 0;
    if (hetTablePos64)
    {
      hetTableSize64 = betTablePos64 - hetTablePos64;
      betTableSize64 = offset64(hashTablePos, hashTablePosHi) - betTablePos64;
    }

    if (formatVersion >= 2)
    {
      hashTableSize64 = offset64(blockTablePos, blockTablePosHi) - offset64(hashTablePos, hashTablePosHi);
      if (hiBlockTablePos64)
      {
        blockTableSize64 = hiBlockTablePos64 - offset64(blockTablePos, blockTablePosHi);
        hiBlockTableSize64 = archiveSize64 - hiBlockTablePos64;
      }
      else
      {
        blockTableSize64 = archiveSize64 - offset64(blockTablePos, blockTablePosHi);
        hiBlockTableSize64 = 0;
      }
    }
    else
    {
      hashTableSize64 = uint64(hashTableSize) * sizeof(MPQHashEntry);
      blockTableSize64 = uint64(blockTableSize) * sizeof(MPQBlockEntry);
      hiBlockTableSize64 = 0;
    }

    rawChunkSize = 0;
    memset(md5BlockTable, 0, sizeof md5BlockTable);
    memset(md5HashTable, 0, sizeof md5HashTable);
    memset(md5HiBlockTable, 0, sizeof md5HiBlockTable);
    memset(md5BetTable, 0, sizeof md5BetTable);
    memset(md5HetTable, 0, sizeof md5HetTable);
    memset(md5MPQHeader, 0, sizeof md5MPQHeader);
  }
}

Archive::Impl* Archive::Impl::create(wchar_t const* filename, uint32 flags, uint64 offset,
    uint32 hashSize, uint32 blockSize)
{
  error = initialize();
  return NULL;
}

Archive::Impl* Archive::Impl::open(::File* file, wchar_t const* filename, uint32 mode, uint32 flags)
{
  if ((mode & 0x03) == ::File::REWRITE)
    return create(filename, flags, 0, 1024, 131072);

  error = initialize();

  bool readonly = ((mode & 0x03) == ::File::READ);
  if (!file) file = ::File::longwopen(filename, readonly ? ::File::READ : ::File::MODIFY);
  if (file == NULL)
  {
    if ((mode & 0x03) == ::File::MODIFY)
      return create(filename, flags, 0, 1024, 131072);
    return serror = error = Error::NoFile, NULL;
  }
  LocalPtr<::File> file_ptr(file);

  // locate mpq header
  file->seek(0, SEEK_END);
  uint64 size = file->tell();
  uint64 offset = 0;
  while (offset < size)
  {
    file->seek(offset, SEEK_SET);
    uint32 id;
    if (file->read(&id, sizeof id) == sizeof id)
    {
      if (id == MPQHeader::signature)
        break;
      if (id == MPQUserData::signature && (flags & Flags::ForceFormatVersion1) == 0)
      {
        MPQUserData userdata;
        file->seek(offset, SEEK_SET);
        if (file->read(&userdata, sizeof userdata) == sizeof userdata)
        {
          offset += userdata.headerOffs;
          break;
        }
      }
    }
    offset += 512;
  }
  if (offset >= size)
    return serror = error = Error::NonMPQ, NULL;
  file->seek(offset, SEEK_SET);
  LocalPtr<Impl> impl = new Impl;
  if (file->read(&impl->hdr, MPQHeader::size_v1) != MPQHeader::size_v1 || impl->hdr.id != MPQHeader::signature)
    return serror = error = Error::Corrupted, NULL;
  if (flags & Flags::ForceFormatVersion1)
    impl->hdr.formatVersion = 1;
  if (impl->hdr.formatVersion > 1 && impl->hdr.headerSize > MPQHeader::size_v1)
  {
    if (file->read((uint8*) &impl->hdr + MPQHeader::size_v1, impl->hdr.headerSize - MPQHeader::size_v1) !=
      impl->hdr.headerSize - MPQHeader::size_v1)
      return serror = error = Error::Corrupted, NULL;
  }
  impl->hdr.upgrade(size - offset);

  if ((flags & Flags::Validate) && !validateHash(&impl->hdr, MPQHeader::size_v4 - MD5::DIGEST_SIZE, impl->hdr.md5MPQHeader))
    return serror = error = Error::Corrupted, NULL;

  impl->file = file_ptr.yield();
  impl->flags = impl->hdr.formatVersion | (flags & Flags::NoListFile ? 0 : Flags::ListFile) |
    (flags & (Flags::Attributes | Flags::ThreadSafety)) | (readonly ? Flags::ReadOnly : 0);
  impl->compression = Compression::ZLIB;
  impl->path = (filename ? filename : L"");
  impl->offset = offset;

  if (impl->hdr.hetTablePos64 == 0 || impl->hdr.betTablePos64 == 0)
  {
    impl->blockTableSpace = impl->hdr.blockTableSize + BLOCK_GROW_STEP;
    uint64 hashTablePos = offset64(impl->hdr.hashTablePos, impl->hdr.hashTablePosHi) + offset;
    uint64 blockTablePos = offset64(impl->hdr.blockTablePos, impl->hdr.blockTablePosHi) + offset;
    impl->hashTable = (MPQHashEntry*) impl->loadTable(hashTablePos,
      impl->hdr.hashTableSize * sizeof(MPQHashEntry), impl->hdr.hashTableSize64,
      hashString("(hash table)", HASH_KEY));
    if (impl->hashTable == NULL)
      return serror = error = Error::Read, NULL;
    impl->blockTable = (MPQBlockEntry*) impl->loadTable(blockTablePos,
      impl->hdr.blockTableSize * sizeof(MPQBlockEntry), impl->hdr.blockTableSize64,
      hashString("(block table)", HASH_KEY));
    if (impl->blockTable == NULL)
      return serror = error = Error::Read, NULL;
    if (impl->hdr.hiBlockTablePos64)
    {
      impl->hiBlockTable = (uint16*) impl->loadTable(impl->hdr.hiBlockTablePos64 + offset,
        impl->hdr.blockTableSize * sizeof(uint16), impl->hdr.hiBlockTableSize64, 0);
      if (impl->hiBlockTable == NULL)
        return serror = error = Error::Read, NULL;
    }
    impl->flags &= ~Flags::HetTable;
    impl->fileNames = (impl->flags & Flags::ListFile ? new String[impl->hdr.hashTableSize] : NULL);
  }
  else
  {
    MPQExtTableHeader hetExtHeader;
    file->seek(offset + impl->hdr.hetTablePos64, SEEK_SET);
    if (file->read(&hetExtHeader, sizeof hetExtHeader) != sizeof hetExtHeader)
      return serror = error = Error::Corrupted, NULL;
    if (hetExtHeader.id != MPQHetTableHeader::signature)
      return serror = error = Error::Corrupted, NULL;
    LocalPtr<uint8, true> hetHeaderRaw = impl->loadTable(
      impl->hdr.hetTablePos64 + sizeof hetExtHeader, hetExtHeader.dataSize,
      impl->hdr.hetTableSize64 - sizeof hetExtHeader,
      hashString("(hash table)", HASH_KEY));
    MPQHetTableHeader* hetHeader = (MPQHetTableHeader*) hetHeaderRaw.ptr();
    if (hetHeader == NULL)
      return serror = error = Error::Read, NULL;

    MPQExtTableHeader betExtHeader;
    file->seek(offset + impl->hdr.betTablePos64, SEEK_SET);
    if (file->read(&betExtHeader, sizeof betExtHeader) != sizeof betExtHeader)
      return serror = error = Error::Corrupted, NULL;
    if (betExtHeader.id != MPQBetTableHeader::signature)
      return serror = error = Error::Corrupted, NULL;
    LocalPtr<uint8, true> betHeaderRaw = impl->loadTable(
      impl->hdr.betTablePos64 + sizeof betExtHeader, betExtHeader.dataSize,
      impl->hdr.betTableSize64 - sizeof betExtHeader,
      hashString("(block table)", HASH_KEY));
    MPQBetTableHeader* betHeader = (MPQBetTableHeader*) betHeaderRaw.ptr();
    if (betHeader == NULL)
      return serror = error = Error::Read, NULL;

    uint8* hetHashTable = (uint8*) (hetHeader + 1);
    uint8* betIndexTable = hetHashTable + hetHeader->hashTableSize;

    uint32* flagsArray = (uint32*) (betHeader + 1);
    uint8* fileTable = (uint8*) (flagsArray + betHeader->flagCount);
    uint8* betHashTable = fileTable + (betHeader->tableEntrySize * hetHeader->maxFileCount + 7) / 8;

    impl->hetTable = (HetTable*) new uint8[sizeof(HetTable) + hetHeader->hashTableSize * (sizeof(uint8) + sizeof(uint32))];
    impl->hetTable->hetTable = (uint8*) (impl->hetTable + 1);
    impl->hetTable->betIndexTable = (uint32*) (impl->hetTable->hetTable + hetHeader->hashTableSize);
    impl->hetTable->hashTableSize = hetHeader->hashTableSize;
    impl->hetTable->hashMask = (1ULL << hetHeader->hashEntrySize) - 1;
    impl->hetTable->hashOr = (1ULL << (hetHeader->hashEntrySize - 1));
    impl->hetTable->hashShift = hetHeader->hashEntrySize - 8;
    for (uint32 i = 0; i < hetHeader->hashTableSize; i++)
    {
      impl->hetTable->hetTable[i] = hetHashTable[i];
      impl->hetTable->betIndexTable[i] = bitRead(betIndexTable, i * hetHeader->totalIndexSize, hetHeader->totalIndexSize);
    }

    impl->blockTableSpace = betHeader->fileCount + BLOCK_GROW_STEP;

    impl->betTable = (BetTable*) new uint8[sizeof(BetTable) + impl->blockTableSpace * sizeof(BetEntry)];
    impl->betTable->betTable = (BetEntry*) (impl->betTable + 1);
    impl->betTable->maxFiles = hetHeader->maxFileCount;
    impl->betTable->numFiles = betHeader->fileCount;
    impl->betTable->hashMask = ((1ULL << (hetHeader->hashEntrySize - 8)) - 1);
    for (uint32 i = 0; i < betHeader->fileCount; i++)
    {
      impl->betTable->betTable[i].hash = bitRead(betHashTable, i * betHeader->totalBetHashSize, betHeader->totalBetHashSize);
      uint32 base = i * betHeader->tableEntrySize;
      uint32 flagIndex = bitRead(fileTable, base + betHeader->bitIndex_flagIndex, betHeader->bitCount_flagIndex);
      impl->betTable->betTable[i].flags = (flagIndex >= betHeader->flagCount ? 0 : flagsArray[flagIndex]);
      impl->betTable->betTable[i].filePos = bitRead(fileTable, base + betHeader->bitIndex_filePos, betHeader->bitCount_filePos);
      impl->betTable->betTable[i].fileSize = bitRead(fileTable, base + betHeader->bitIndex_fileSize, betHeader->bitCount_fileSize);
      impl->betTable->betTable[i].cmpSize = bitRead(fileTable, base + betHeader->bitIndex_cmpSize, betHeader->bitCount_cmpSize);
    }

    impl->flags |= Flags::HetTable;
    impl->fileNames = (impl->flags & Flags::ListFile ? new String[impl->hetTable->hashTableSize] : NULL);
  }

  impl->blockSize = (1 << (9 + impl->hdr.blockSize));
  if (impl->blockSize > 0x200000)
  {
    impl->blockSize = 0x100000;
    if (impl->blockTable)
    {
      for (uint32 i = 0; i < impl->hdr.blockTableSize; i++)
        while (impl->blockTable[i].fSize > impl->blockSize)
          impl->blockSize *= 2;
    }
    else if (impl->betTable)
    {
      for (uint32 i = 0; i < impl->betTable->numFiles; i++)
        while (impl->betTable->betTable[i].fileSize > impl->blockSize)
          impl->blockSize *= 2;
    }
  }
  impl->buf = new uint8[impl->blockSize * 2];

  if (impl->flags & Flags::ThreadSafety)
    impl->mutex = new thread::Lock;

  uint32 setFlagsMask = 0;
  if (flags & Flags::FormatVersionMask)
    setFlagsMask |= Flags::FormatVersionMask;
  if (flags & Flags::HetTable)
    setFlagsMask |= Flags::HetTable;
  if (flags & Flags::Attributes)
    setFlagsMask |= Flags::Attributes;
  if (flags & Flags::Encrypted)
    setFlagsMask |= Flags::Encrypted;
  if (flags & Flags::FixSeedEncryption)
    setFlagsMask |= Flags::FixSeedEncryption;
  impl->setFlags(flags, setFlagsMask);

  return impl.yield();
}

struct HashTablePtr
{
  uint32 hashSize;
  uint32 numFiles;
  uint32 maxFiles;
  MPQHashEntry* hashTable;
  MPQBlockEntry* blockTable;
  uint16* hiBlockTable;
};
struct HetTablePtr
{
  HetTable* hetTable;
  BetTable* betTable;
};
static uint32 HashToHet(HashTablePtr const& in, HetTablePtr& out, String* fileNames, uint32* moves)
{
  uint32 hetSize = in.maxFiles * 4 / 3;

  out.hetTable = (HetTable*) new uint8[sizeof(HetTable) + hetSize * (sizeof(uint8) + sizeof(uint32))];
  out.hetTable->hashMask = 0xFFFFFFFFFFFFFFFFULL;
  out.hetTable->hashOr = 0x8000000000000000ULL;
  out.hetTable->hashShift = 56;
  out.hetTable->hashTableSize = hetSize;
  out.hetTable->hetTable = (uint8*) (out.hetTable + 1);
  out.hetTable->betIndexTable = (uint32*) (out.hetTable->hetTable + hetSize);
  memset(out.hetTable->hetTable, HetTable::EMPTY, hetSize * sizeof(uint8));

  out.betTable = (BetTable*) new uint8[sizeof(BetTable) + in.maxFiles * sizeof(BetEntry)];
  out.betTable->numFiles = 0;
  out.betTable->maxFiles = in.maxFiles;
  out.betTable->hashMask = 0x00FFFFFFFFFFFFFFULL;
  out.betTable->betTable = (BetEntry*) (out.betTable + 1);
  for (uint32 h = 0; h < in.hashSize; h++)
  {
    if (moves)
      moves[h] = -1;
    if (fileNames == NULL || fileNames[h].empty() ||
        in.hashTable[h].blockIndex == MPQHashEntry::EMPTY ||
        in.hashTable[h].blockIndex == MPQHashEntry::DELETED)
      continue;
    uint32 b = in.hashTable[h].blockIndex;

    uint64 hash = (hashJenkins(fileNames[h]) & out.hetTable->hashMask) |
      out.hetTable->hashOr;

    uint32 het = uint32(hash % hetSize);
    while (out.hetTable->hetTable[het] != HetTable::EMPTY)
      het = (het + 1) % hetSize;

    uint32 bet = out.betTable->numFiles++;

    if (moves)
      moves[h] = het;

    out.hetTable->hetTable[het] = uint8(hash >> out.hetTable->hashShift);
    out.hetTable->betIndexTable[het] = bet;

    out.betTable->betTable[bet].hash = (hash & out.betTable->hashMask);
    out.betTable->betTable[bet].flags = in.blockTable[b].flags;
    out.betTable->betTable[bet].filePos = in.blockTable[b].filePos;
    if (in.hiBlockTable)
      out.betTable->betTable[bet].filePos |= uint64(in.hiBlockTable[b]) << 32;
    out.betTable->betTable[bet].fileSize = in.blockTable[b].fSize;
    out.betTable->betTable[bet].cmpSize = in.blockTable[b].cSize;
  }
  return 0;
}
static uint32 HetToHash(HetTablePtr const& in, HashTablePtr& out, String* fileNames, uint32* moves)
{
  out.hashSize = 1;
  while (out.hashSize < in.betTable->maxFiles * 4 / 3)
    out.hashSize *= 2;
  out.maxFiles = in.betTable->maxFiles;

  out.hashTable = (MPQHashEntry*) new uint8[out.hashSize * sizeof(MPQHashEntry)];
  out.blockTable = (MPQBlockEntry*) new uint8[out.maxFiles * sizeof(MPQBlockEntry)];
  for (uint32 i = 0; i < out.hashSize; i++)
    out.hashTable[i].blockIndex = MPQHashEntry::EMPTY;
  out.numFiles = 0;
  out.hiBlockTable = NULL;
  for (uint32 i = 0; i < in.betTable->numFiles; i++)
  {
    if (in.betTable->betTable[i].filePos >> 32)
    {
      out.hiBlockTable = (uint16*) new uint8[out.maxFiles * sizeof(uint16)];
      break;
    }
  }
  for (uint32 h = 0; h < in.hetTable->hashTableSize; h++)
  {
    if (moves)
      moves[h] = -1;
    if (fileNames == NULL || fileNames[h].empty() ||
        in.hetTable->hetTable[h] == HetTable::EMPTY ||
        in.hetTable->betIndexTable[h] >= in.betTable->numFiles)
      continue;
    uint32 b = in.hetTable->betIndexTable[h];

    uint32 hash = hashString(fileNames[h], HASH_OFFSET) % out.hashSize;
    uint32 name1 = hashString(fileNames[h], HASH_NAME1);
    uint32 name2 = hashString(fileNames[h], HASH_NAME2);

    while (out.hashTable[hash].blockIndex != MPQHashEntry::EMPTY)
      hash = (hash + 1) % out.hashSize;

    uint32 block = out.numFiles++;

    if (moves)
      moves[h] = hash;

    out.hashTable[hash].name1 = name1;
    out.hashTable[hash].name2 = name2;
    out.hashTable[hash].locale = Locale::Neutral;
    out.hashTable[hash].platform = 0;
    out.hashTable[hash].blockIndex = block;

    out.blockTable[block].flags = in.betTable->betTable[b].flags;
    out.blockTable[block].filePos = uint32(in.betTable->betTable[b].filePos);
    out.blockTable[block].fSize = in.betTable->betTable[b].fileSize;
    out.blockTable[block].cSize = in.betTable->betTable[b].cmpSize;

    if (out.hiBlockTable)
      out.hiBlockTable[block] = uint32(in.betTable->betTable[b].filePos >> 32);
  }
  return 0;
}

void Archive::Impl::closeFiles()
{
  while (firstFile)
    firstFile->close();
}
uint32 Archive::Impl::addName(char const* name)
{
  if (hetTable)
  {
    uint64 hash = (hashJenkins(name) & hetTable->hashMask) | hetTable->hashOr;
    uint8 hetHash = uint8(hash >> hetTable->hashShift);
    uint64 betHash = hash & betTable->hashMask;

    uint32 count = 0;
    for (uint32 cur = uint32(hash % hetTable->hashTableSize);
      count < hetTable->hashTableSize && hetTable->hetTable[cur] != HetTable::EMPTY;
      count++, cur = (cur + 1) % hetTable->hashTableSize)
    {
      if (hetTable->hetTable[cur] == hetHash)
      {
        uint32 betIndex = hetTable->betIndexTable[cur];
        if (betIndex < betTable->numFiles && betTable->betTable[betIndex].hash == betHash)
        {
          fileNames[cur] = name;
          break; // het/bet table has no duplicate files, I hope
        }
      }
    }
  }
  else
  {
    uint32 name1 = hashString(name, HASH_NAME1);
    uint32 name2 = hashString(name, HASH_NAME2);
    uint32 count = 0;

    for (uint32 cur = hashString(name, HASH_OFFSET) % hdr.hashTableSize;
      count < hdr.hashTableSize && hashTable[cur].blockIndex != MPQHashEntry::EMPTY;
      count++, cur = (cur + 1) % hdr.hashTableSize)
    {
      if (hashTable[cur].blockIndex != MPQHashEntry::DELETED &&
          hashTable[cur].name1 == name1 && hashTable[cur].name2 == name2)
        fileNames[cur] = name;
    }
  }
  return error = Error::Ok;
}
uint32 Archive::Impl::loadListFile()
{
  if (fileNames == NULL)
    return 0;
  addName("(listfile)");
  addName("(attributes)");
  addName("(signature)");
  addName("(patch_metadata)");
  File* list = archive->openFile("(listfile)", ::File::READ);
  if (list)
  {
    String name;
    while (list->gets(name))
    {
      int length = name.length();
      while (length > 0 && (name[length - 1] == '\r' || name[length - 1] == '\n'))
        length--;
      name.resize(length);

      addName(name);
    }
    delete list;
  }
  return 0;
}
uint32 Archive::Impl::setFlags(uint32 value, uint32 mask)
{
  uint32 newFlags = (flags & (~mask)) | (value & mask);

  uint32 version = (newFlags & Flags::FormatVersionMask);
  if (version < 3)
    newFlags &= ~Flags::HetTable;

  if ((newFlags & Flags::ThreadSafety) != (mutex != NULL))
  {
    if (newFlags & Flags::ThreadSafety)
      mutex = new thread::Lock;
    else
    {
      delete mutex;
      mutex = NULL;
    }
  }

  thread::Lock::Holder locker(mutex);

  if (bool(newFlags & Flags::HetTable) != bool(hetTable != NULL))
  {
    if (fileNames == NULL)
    {
      fileNames = new String[hetTable ? hetTable->hashTableSize : hdr.hashTableSize];
      loadListFile();
    }
    String* oldNames = fileNames;
    fileNames = NULL;
    if (newFlags & Flags::HetTable)
    {
      HashTablePtr hash;
      hash.hashSize = hdr.hashTableSize;
      hash.numFiles = hdr.blockTableSize;
      hash.maxFiles = blockTableSpace;
      hash.hashTable = hashTable;
      hash.blockTable = blockTable;
      hash.hiBlockTable = hiBlockTable;
      HetTablePtr het;
      uint32* moves = new uint32[hdr.hashTableSize];
      HashToHet(hash, het, oldNames, moves);
      for (File* file = firstFile; file;)
      {
        File* next = file->impl->next;
        if (file->impl->pos >= hdr.hashTableSize || moves[file->impl->pos] >= het.hetTable->hashTableSize)
          file->close();
        else
          file->impl->pos = moves[file->impl->pos];
        file = next;
      }
      if (newFlags & Flags::ListFile)
      {
        fileNames = new String[het.hetTable->hashTableSize];
        for (uint32 cur = 0; cur < hdr.hashTableSize; cur++)
          if (moves[cur] < het.hetTable->hashTableSize)
            fileNames[moves[cur]] = oldNames[cur];
      }
      delete[] moves;

      delete[] (uint8*) hashTable;
      delete[] (uint8*) blockTable;
      delete[] (uint8*) hiBlockTable;
      hashTable = NULL;
      blockTable = NULL;
      hiBlockTable = NULL;
      hetTable = het.hetTable;
      betTable = het.betTable;
    }
    else
    {
      HetTablePtr het;
      het.hetTable = hetTable;
      het.betTable = betTable;
      HashTablePtr hash;
      uint32* moves = new uint32[hetTable->hashTableSize];
      HetToHash(het, hash, fileNames, moves);
      for (File* file = firstFile; file;)
      {
        File* next = file->impl->next;
        if (file->impl->pos >= hetTable->hashTableSize || moves[file->impl->pos] >= hash.hashSize)
          file->close();
        else
          file->impl->pos = moves[file->impl->pos];
        file = next;
      }
      if (newFlags & Flags::ListFile)
      {
        fileNames = new String[hash.hashSize];
        for (uint32 cur = 0; cur < hetTable->hashTableSize; cur++)
          if (moves[cur] < hash.hashSize)
            fileNames[moves[cur]] = oldNames[cur];
      }
      delete[] moves;

      delete[] (uint8*) hetTable;
      delete[] (uint8*) betTable;
      hetTable = NULL;
      betTable = NULL;

      hdr.hashTableSize = hash.hashSize;
      hdr.blockTableSize = hash.numFiles;
      blockTableSpace = hash.maxFiles;
      hashTable = hash.hashTable;
      blockTable = hash.blockTable;
      hiBlockTable = hash.hiBlockTable;
    }
    delete[] oldNames;
    flushTables();
  }

  if (bool(newFlags & Flags::ListFile) != bool(fileNames != NULL))
  {
    if (newFlags & Flags::ListFile)
    {
      fileNames = new String[hetTable ? hetTable->hashTableSize : hdr.hashTableSize];
      loadListFile();
    }
    else
    {
      delete[] fileNames;
      fileNames = NULL;
    }
  }

  //TODO: attributes

  flags = newFlags;
  if (version != hdr.formatVersion)
  {
    hdr.formatVersion = version;
    if (version == 1)
      hdr.headerSize = MPQHeader::size_v1;
    if (version == 2)
      hdr.headerSize = MPQHeader::size_v2;
    if (version == 3)
      hdr.headerSize = MPQHeader::size_v3;
    if (version == 4)
      hdr.headerSize = MPQHeader::size_v4;
    flushTables();
  }

  return error = Error::Ok;
}

uint32 Archive::Impl::createEntry(char const* name, uint32 block)
{
  if (hetTable)
  {
    uint64 hash = (hashJenkins(name) & hetTable->hashMask) | hetTable->hashOr;

    uint32 count = 0;
    for (uint32 cur = uint32(hash % hetTable->hashTableSize);
      count < hetTable->hashTableSize;
      count++, cur = (cur + 1) % hetTable->hashTableSize)
    {
      if (hetTable->hetTable[cur] == HetTable::EMPTY ||
          hetTable->betIndexTable[cur] >= betTable->numFiles)
      {
        hetTable->hetTable[cur] = uint8(hash >> hetTable->hashShift);
        hetTable->betIndexTable[cur] = block;
        betTable->betTable[block].hash = hash & betTable->hashMask;
        return cur;
      }
    }
  }
  else
  {
    uint32 count = 0;
    for (uint32 cur = hashString(name, HASH_OFFSET) % hdr.hashTableSize;
      count < hdr.hashTableSize; count++, cur = (cur + 1) % hdr.hashTableSize)
    {
      if (hashTable[cur].blockIndex == MPQHashEntry::EMPTY ||
          hashTable[cur].blockIndex == MPQHashEntry::DELETED)
      {
        hashTable[cur].name1 = hashString(name, HASH_NAME1);
        hashTable[cur].name2 = hashString(name, HASH_NAME2);
        hashTable[cur].blockIndex = block;
        return cur;
      }
    }
  }
  return error = Error::Full;
}

static int blockComp(BetEntry const& a, BetEntry const& b)
{
  return a.filePos < b.filePos ? -1 : 1;
}
uint32 Archive::Impl::flushTables()
{
  thread::Lock::Holder locker(mutex);
  uint64 fileMin = hdr.archiveSize64;
  uint64 fileMax = 0;
  if (hetTable)
  {
    for (uint32 cur = 0; cur < hetTable->hashTableSize; cur++)
    {
      if (hetTable->hetTable[cur] == HetTable::EMPTY ||
          hetTable->betIndexTable[cur] >= betTable->numFiles)
        continue;
      BetEntry& bet = betTable->betTable[hetTable->betIndexTable[cur]];
      if (bet.filePos < fileMin)
        fileMin = bet.filePos;
      if (bet.filePos + bet.cmpSize > fileMax)
        fileMax = bet.filePos + cmpAdjust(bet.cmpSize);
    }
  }
  else
  {
    for (uint32 cur = 0; cur < hdr.hashTableSize; cur++)
    {
      if (hashTable[cur].blockIndex == MPQHashEntry::EMPTY ||
          hashTable[cur].blockIndex == MPQHashEntry::DELETED)
        continue;
      MPQBlockEntry& block = blockTable[hashTable[cur].blockIndex];
      if (block.filePos < fileMin)
        fileMin = block.filePos;
      if (block.filePos + block.cSize > fileMax)
        fileMax = block.filePos + cmpAdjust(block.cSize);
    }
  }
  if (fileMin < hdr.headerSize)
  {
    uint32 shift = hdr.headerSize - fileMin;
    Array<BetEntry> blocks;
    listBlocks(blocks);
    blocks.sort(blockComp);
    for (uint32 cur = blocks.length() - 1; cur < blocks.length(); cur--) // unsigned!
    {
      transferFile(file, blocks[cur], blocks[cur].filePos + shift,
        fileNames ? (fileNames[blocks[cur].hash].empty() ? NULL : fileNames[blocks[cur].hash].c_str()) : NULL);
    }
    if (hetTable)
    {
      for (uint32 cur = 0; cur < betTable->numFiles; cur++)
        betTable->betTable[cur].filePos += shift;
    }
    else
    {
      for (uint32 cur = 0; cur < hdr.blockTableSize; cur++)
      {
        uint64 newPos = uint64(blockTable[cur].filePos);
        if (hiBlockTable)
          newPos |= uint64(hiBlockTable[cur]) << 32;
        newPos += shift;
        blockTable[cur].filePos = uint32(newPos);
        if (hiBlockTable)
          hiBlockTable[cur] = uint16(newPos >> 16);
      }
    }
    fileMax += shift;
  }
  if (fileMax < hdr.headerSize)
    fileMax = hdr.headerSize;
  uint64 tableBase = offset64(hdr.hashTablePos, hdr.hashTablePosHi);
  if (offset64(hdr.blockTablePos, hdr.blockTablePosHi) < tableBase)
    tableBase = offset64(hdr.blockTablePos, hdr.blockTablePosHi);
  if (hdr.hiBlockTablePos64 && hdr.hiBlockTablePos64 < tableBase)
    tableBase = hdr.hiBlockTablePos64;
  if (hdr.hetTablePos64 && hdr.hetTablePos64 < tableBase)
    tableBase = hdr.hetTablePos64;
  if (hdr.betTablePos64 && hdr.betTablePos64 < tableBase)
    tableBase = hdr.betTablePos64;
  if (fileMax > tableBase)
    tableBase = fileMax;
  return flushTablesRaw(file, tableBase);
}
uint32 Archive::Impl::transferFile(::File* dest, BetEntry const& block, uint64 newPos, char const* name)
{
  uint32 patchLength = 0;
  if (block.flags & File::Flags::PatchFile)
  {
    file->seek(offset + block.filePos, SEEK_SET);
    patchLength = file->read32();
    file->seek(offset + block.filePos, SEEK_SET);
    dest->seek(offset + newPos, SEEK_SET);
    dest->copy(file, patchLength);
  }
  if (block.flags & File::Flags::SingleUnit)
  {
    LocalPtr<uint8, true> buf = new uint8[block.cmpSize - patchLength];
    file->seek(offset + block.filePos + patchLength, SEEK_SET);
    if (file->read(buf.ptr(), block.cmpSize - patchLength) != block.cmpSize - patchLength)
      return serror = error = Error::Read;
    if ((block.flags & File::Flags::Encrypted) && (block.flags & File::Flags::FixSeed))
    {
      uint32 key;
      if (name)
      {
        key = hashString(stripPath(name), HASH_KEY);
        key = (key + uint32(block.filePos)) ^ block.fileSize;
      }
      else
        key = detectFileSeed((uint32*) buf.ptr(), block.fileSize);
      decryptBlock(buf.ptr(), block.cmpSize - patchLength, key);
      key = (key ^ block.fileSize) - uint32(block.filePos);
      key = (key + uint32(newPos)) ^ block.fileSize;
      encryptBlock(buf.ptr(), block.cmpSize - patchLength, key);
    }
    dest->seek(offset + newPos + patchLength, SEEK_SET);
    if (dest->write(buf.ptr(), block.cmpSize - patchLength) != block.cmpSize - patchLength)
      return serror = error = Error::Write;
  }
  else
  {
    uint32 numBlocks = (block.fileSize + blockSize - 1) / blockSize;
    if (!(block.flags & File::Flags::Compressed))
    {
      uint32 key = 0;
      uint32 rekey = 0;
      if (block.flags & File::Flags::Encrypted)
      {
        if (name)
        {
          key = hashString(stripPath(name), HASH_KEY);
          if (block.flags & File::Flags::FixSeed)
            key = (key + uint32(block.filePos)) ^ block.fileSize;
        }
        else
        {
          uint32 temp[3];
          memset(temp, 0, sizeof temp);
          file->seek(offset + block.filePos + patchLength, SEEK_SET);
          file->read(temp, sizeof temp);
          key = detectFileSeed(temp, block.fileSize);
        }
        rekey = key;
        if (block.flags & File::Flags::FixSeed)
        {
          rekey = (rekey ^ block.fileSize) - uint32(block.filePos);
          rekey = (rekey + uint32(newPos)) ^ block.fileSize;
        }
      }
      for (uint32 cur = numBlocks - 1; cur < numBlocks; cur--) // unsigned!
      {
        uint32 size = blockSize;
        if (size > block.fileSize - cur * blockSize)
          size = block.fileSize - cur * blockSize;
        file->seek(offset + block.filePos + patchLength + cur * blockSize, SEEK_SET);
        if (file->read(buf, size) != size)
          return serror = error = Error::Read;
        if (key != rekey)
        {
          decryptBlock(buf, size, key + cur);
          encryptBlock(buf, size, rekey + cur);
        }
        dest->seek(offset + newPos + patchLength + cur * blockSize, SEEK_SET);
        if (dest->write(buf, size) != size)
          return serror = error = Error::Write;
      }
    }
    else
    {
      uint32 tableSize = numBlocks + 1;
      if (block.flags & File::Flags::SectorCrc)
        tableSize += 1;
      LocalPtr<uint32, true> table = new uint32[tableSize];
      file->seek(offset + block.filePos + patchLength, SEEK_SET);
      if (file->read(table.ptr(), tableSize * sizeof(uint32)) != tableSize * sizeof(uint32))
        return serror = error = Error::Read;
      uint32 key = 0;
      uint32 rekey = 0;
      if (block.flags & File::Flags::Encrypted)
      {
        if (name)
        {
          key = hashString(stripPath(name), HASH_KEY);
          if (block.flags & File::Flags::FixSeed)
            key = (key + uint32(block.filePos)) ^ block.fileSize;
        }
        else
          key = detectTableSeed(table.ptr(), tableSize * sizeof(uint32), blockSize);
        rekey = key;
        if (block.flags & File::Flags::FixSeed)
        {
          rekey = (rekey ^ block.fileSize) - uint32(block.filePos);
          rekey = (rekey + uint32(newPos)) ^ block.fileSize;
        }

        decryptBlock(table.ptr(), tableSize * sizeof(uint32), key - 1);
      }
      if (block.flags & File::Flags::SectorCrc)
      {
        file->seek(offset + block.filePos + patchLength + table[numBlocks], SEEK_SET);
        dest->seek(offset + newPos + patchLength + table[numBlocks], SEEK_SET);
        dest->copy(file, table[numBlocks + 1] - table[numBlocks]);
      }
      for (uint32 cur = numBlocks - 1; cur < numBlocks; cur--) // unsigned!
      {
        uint32 size = table[cur + 1] - table[cur];
        file->seek(offset + block.filePos + patchLength + table[cur], SEEK_SET);
        if (file->read(buf, size) != size)
          return serror = error = Error::Read;
        if (key != rekey)
        {
          decryptBlock(buf, size, key + cur);
          encryptBlock(buf, size, rekey + cur);
        }
        dest->seek(offset + newPos + patchLength + table[cur], SEEK_SET);
        if (dest->write(buf, size) != size)
          return serror = error = Error::Write;
      }
      if (block.flags & File::Flags::Encrypted)
        encryptBlock(table.ptr(), tableSize * sizeof(uint32), rekey - 1);
      dest->seek(offset + newPos + patchLength, SEEK_SET);
      if (dest->write(table.ptr(), tableSize * sizeof(uint32)) != tableSize * sizeof(uint32))
        return serror = error = Error::Write;
    }
  }
  return writeChecksums(dest, newPos, block.cmpSize);
}
uint32 Archive::Impl::cmpAdjust(uint32 cmpSize)
{
  if (!hdr.rawChunkSize)
    return cmpSize;
  return cmpSize + (cmpSize + hdr.rawChunkSize - 1) / hdr.rawChunkSize * MD5::DIGEST_SIZE;
}
uint32 Archive::Impl::writeChecksums(::File* dest, uint64 start, uint32 size, uint8* mem)
{
  if (hdr.rawChunkSize == 0 || size == 0)
    return error = Error::Ok;
  if (mem)
  {
    uint8 md5_buf[MD5::DIGEST_SIZE];
    dest->seek(offset + start + size, SEEK_SET);
    while (size > 0)
    {
      uint32 count = hdr.rawChunkSize;
      if (count > size)
        count = size;
      MD5::checksum(mem, count, md5_buf);
      if (dest->write(md5_buf, MD5::DIGEST_SIZE) != MD5::DIGEST_SIZE)
        return serror = error = Error::Write;
      mem += count;
      size -= count;
    }
  }
  else
  {
    uint32 numChunks = (size + hdr.rawChunkSize - 1) / hdr.rawChunkSize;
    LocalPtr<uint8, true> md5_buf = new uint8[MD5::DIGEST_SIZE * numChunks];
    LocalPtr<uint8, true> md5_chunk = new uint8[hdr.rawChunkSize];
    uint8* md5_ptr = md5_buf.ptr();
    dest->seek(offset + start, SEEK_SET);
    while (size > 0)
    {
      uint32 count = hdr.rawChunkSize;
      if (count > size)
        count = size;
      if (dest->read(md5_chunk.ptr(), count) != count)
        return serror = error = Error::Read;
      MD5::checksum(md5_chunk.ptr(), count, md5_ptr);
      md5_ptr += MD5::DIGEST_SIZE;
      size -= count;
    }
    if (dest->write(md5_buf.ptr(), MD5::DIGEST_SIZE * numChunks) != MD5::DIGEST_SIZE * numChunks)
      return serror = error = Error::Write;
  }
  return error = Error::Ok;
}
uint32 Archive::Impl::flushTablesRaw(::File* dest, uint64 start)
{
  MPQHeader newHeader = hdr;
  newHeader.hetTablePos64 = 0;
  newHeader.betTablePos64 = 0;
  newHeader.hashTablePos = 0;
  newHeader.blockTablePos = 0;
  newHeader.hashTablePosHi = 0;
  newHeader.blockTablePosHi = 0;
  newHeader.hiBlockTablePos64 = 0;
  newHeader.hetTableSize64 = 0;
  newHeader.betTableSize64 = 0;
  newHeader.hashTableSize64 = 0;
  newHeader.blockTableSize64 = 0;
  newHeader.hiBlockTableSize64 = 0;
  memset(newHeader.md5BlockTable, 0, sizeof newHeader.md5BlockTable);
  memset(newHeader.md5HashTable, 0, sizeof newHeader.md5HashTable);
  memset(newHeader.md5HiBlockTable, 0, sizeof newHeader.md5HiBlockTable);
  memset(newHeader.md5HetTable, 0, sizeof newHeader.md5HetTable);
  memset(newHeader.md5BetTable, 0, sizeof newHeader.md5BetTable);
  memset(newHeader.md5MPQHeader, 0, sizeof newHeader.md5MPQHeader);
  if (hetTable)
  {
    uint32 totalIndexSize = 0;
    for (uint32 cur = 0; cur < hetTable->hashTableSize; cur++)
      if (hetTable->hetTable[cur] != HetTable::EMPTY &&
          hetTable->betIndexTable[cur] < betTable->numFiles)
        while ((1 << totalIndexSize) <= hetTable->betIndexTable[cur])
          totalIndexSize++;

    uint32 hetTableSize = sizeof(MPQHetTableHeader) + hetTable->hashTableSize +
      (hetTable->hashTableSize * totalIndexSize + 7) / 8;
    LocalPtr<uint8, true> hetData = new uint8[sizeof(MPQExtTableHeader) + hetTableSize];
    memset(hetData.ptr(), 0, sizeof(MPQExtTableHeader) + hetTableSize);
    MPQExtTableHeader* hetExtHeader = (MPQExtTableHeader*) hetData.ptr();
    MPQHetTableHeader* hetHeader = (MPQHetTableHeader*) (hetExtHeader + 1);
    uint8* hetHashTable = (uint8*) (hetHeader + 1);
    uint8* betIndexTable = (uint8*) (hetHashTable + hetTable->hashTableSize);

    hetExtHeader->id = MPQHetTableHeader::signature;
    hetExtHeader->version = 1;
    hetExtHeader->dataSize = hetTableSize;
    hetHeader->tableSize = hetTableSize;
    hetHeader->maxFileCount = betTable->numFiles;
    hetHeader->hashTableSize = hetTable->hashTableSize;
    hetHeader->hashEntrySize = hetTable->hashShift + 8;
    hetHeader->totalIndexSize = totalIndexSize;
    hetHeader->indexSizeExtra = 0;
    hetHeader->indexSize = totalIndexSize;
    hetHeader->blockTableSize = (hetTable->hashTableSize * totalIndexSize + 7) / 8;
    memcpy(hetHashTable, hetTable->hetTable, hetTable->hashTableSize);
    for (uint32 cur = 0; cur < hetTable->hashTableSize; cur++)
      bitWrite(betIndexTable, cur * totalIndexSize, totalIndexSize, hetTable->betIndexTable[cur]);

    encryptBlock(hetHeader, hetTableSize, hashString("(hash table)", HASH_KEY));
    MD5::checksum(hetData.ptr(), sizeof(MPQExtTableHeader) + hetTableSize, newHeader.md5HetTable);

    newHeader.hetTablePos64 = start;
    newHeader.hetTableSize64 = sizeof(hetExtHeader) + hetTableSize;
    dest->seek(offset + start, SEEK_SET);
    if (dest->write(hetData.ptr(), newHeader.hetTableSize64) != newHeader.hetTableSize64)
      return serror = error = Error::Write;
    writeChecksums(dest, start, newHeader.hetTableSize64, hetData.ptr());
    start += cmpAdjust(newHeader.hetTableSize64);

    Array<uint32> flagsArray;
    uint32 bitCount_filePos = 0;
    uint32 bitCount_fileSize = 0;
    uint32 bitCount_cmpSize = 0;
    uint32 bitCount_flagIndex = 0;
    for (uint32 cur = 0; cur < betTable->numFiles; cur++)
    {
      while ((1 << bitCount_filePos) <= betTable->betTable[cur].filePos)
        bitCount_filePos++;
      while ((1 << bitCount_fileSize) <= betTable->betTable[cur].fileSize)
        bitCount_fileSize++;
      while ((1 << bitCount_cmpSize) <= betTable->betTable[cur].cmpSize)
        bitCount_cmpSize++;

      uint32 flag = betTable->betTable[cur].flags;
      uint32 pos = 0;
      while (pos < flagsArray.length() && flagsArray[pos] != flag)
        pos++;
      if (pos >= flagsArray.length())
        flagsArray.push(flag);
    }
    while ((1 << bitCount_flagIndex) < flagsArray.length())
      bitCount_flagIndex++;

    uint32 tableEntrySize = bitCount_filePos + bitCount_fileSize + bitCount_cmpSize + bitCount_flagIndex;
    uint32 betTableSize = sizeof(MPQBetTableHeader) + flagsArray.length() * sizeof(uint32) +
      (tableEntrySize * betTable->numFiles + 7) / 8 + (hetTable->hashShift * betTable->numFiles + 7) / 8;
    LocalPtr<uint8, true> betData = new uint8[sizeof(MPQExtTableHeader) + betTableSize];
    memset(betData.ptr(), 0, betTableSize);
    MPQExtTableHeader* betExtHeader = (MPQExtTableHeader*) betData.ptr();
    MPQBetTableHeader* betHeader = (MPQBetTableHeader*) (betExtHeader + 1);
    uint32* flagsArrayPtr = (uint32*) (betHeader + 1);
    uint8* betFileTable = (uint8*) (flagsArrayPtr + flagsArray.length());
    uint8* betHashTable = (uint8*) (betFileTable + (tableEntrySize * betTable->numFiles + 7) / 8);
    betExtHeader->id = MPQBetTableHeader::signature;
    betExtHeader->version = 1;
    betExtHeader->dataSize = betTableSize;
    betHeader->tableSize = betTableSize;
    betHeader->fileCount = betTable->numFiles;
    betHeader->unknown08 = 0x10;
    betHeader->tableEntrySize = tableEntrySize;
    betHeader->bitIndex_filePos = 0;
    betHeader->bitIndex_fileSize = betHeader->bitIndex_filePos + bitCount_filePos;
    betHeader->bitIndex_cmpSize = betHeader->bitIndex_fileSize + bitCount_fileSize;
    betHeader->bitIndex_flagIndex = betHeader->bitIndex_cmpSize + bitCount_cmpSize;
    betHeader->bitIndex_unknown = betHeader->bitIndex_flagIndex + bitCount_flagIndex;
    betHeader->bitCount_filePos = bitCount_filePos;
    betHeader->bitCount_fileSize = bitCount_fileSize;
    betHeader->bitCount_cmpSize = bitCount_cmpSize;
    betHeader->bitCount_flagIndex = bitCount_flagIndex;
    betHeader->bitCount_unknown = 0;
    betHeader->totalBetHashSize = hetTable->hashShift;
    betHeader->betHashSizeExtra = 0;
    betHeader->betHashSize = hetTable->hashShift;
    betHeader->betHashArraySize = (hetTable->hashShift * betTable->numFiles + 7) / 8;
    betHeader->flagCount = flagsArray.length();
    for (uint32 cur = 0; cur < flagsArray.length(); cur++)
      flagsArrayPtr[cur] = flagsArray[cur];
    for (uint32 cur = 0; cur < betTable->numFiles; cur++)
    {
      uint32 flag = betTable->betTable[cur].flags;
      uint32 flagIndex = 0;
      while (flagIndex < flagsArray.length() - 1 && flagsArray[flagIndex] != flag)
        flagIndex++;

      bitWrite(betFileTable, cur * tableEntrySize + betHeader->bitIndex_filePos, bitCount_filePos, betTable->betTable[cur].filePos);
      bitWrite(betFileTable, cur * tableEntrySize + betHeader->bitIndex_fileSize, bitCount_fileSize, betTable->betTable[cur].fileSize);
      bitWrite(betFileTable, cur * tableEntrySize + betHeader->bitIndex_cmpSize, bitCount_cmpSize, betTable->betTable[cur].cmpSize);
      bitWrite(betFileTable, cur * tableEntrySize + betHeader->bitIndex_flagIndex, bitCount_flagIndex, flagIndex);
      bitWrite(betHashTable, cur * hetTable->hashShift, hetTable->hashShift, betTable->betTable[cur].hash);
    }

    encryptBlock(betHeader, betTableSize, hashString("(block table)", HASH_KEY));
    MD5::checksum(betData.ptr(), sizeof(MPQExtTableHeader) + betTableSize, newHeader.md5BetTable);

    newHeader.betTablePos64 = start;
    newHeader.betTableSize64 = sizeof(betExtHeader) + betTableSize;
    dest->seek(offset + start, SEEK_SET);
    if (dest->write(betData.ptr(), newHeader.betTableSize64) != newHeader.betTableSize64)
      return serror = error = Error::Write;
    writeChecksums(dest, start, newHeader.betTableSize64, betData.ptr());
    start += cmpAdjust(newHeader.betTableSize64);
  }

  {
    HashTablePtr ht;
    if (hetTable)
    {
      HetTablePtr het;
      het.hetTable = hetTable;
      het.betTable = betTable;
      HetToHash(het, ht, fileNames, NULL);
    }
    else
    {
      ht.hashSize = hdr.hashTableSize;
      ht.numFiles = hdr.blockTableSize;
      ht.maxFiles = hdr.blockTableSize;
      ht.hashTable = (MPQHashEntry*) new uint8[hdr.hashTableSize * sizeof(MPQHashEntry)];
      memcpy(ht.hashTable, hashTable, hdr.hashTableSize * sizeof(MPQHashEntry));
      ht.blockTable = (MPQBlockEntry*) new uint8[hdr.blockTableSize * sizeof(MPQBlockEntry)];
      memcpy(ht.blockTable, blockTable, hdr.blockTableSize * sizeof(MPQBlockEntry));
      ht.hiBlockTable = NULL;
      if (hiBlockTable)
      {
        ht.hiBlockTable = (uint16*) new uint8[hdr.blockTableSize * sizeof(uint16)];
        memcpy(ht.hiBlockTable, hiBlockTable, hdr.blockTableSize * sizeof(uint16));
      }
    }
    LocalPtr<uint8, true> keeper_hash = (uint8*) ht.hashTable;
    LocalPtr<uint8, true> keeper_block = (uint8*) ht.blockTable;
    LocalPtr<uint8, true> keeper_hiblock = (uint8*) ht.hiBlockTable;

    newHeader.hashTablePos = uint32(start);
    newHeader.hashTablePosHi = uint16(start >> 32);
    newHeader.hashTableSize64 = sizeof(MPQHashEntry) * ht.hashSize;
    newHeader.hashTableSize = ht.hashSize;

    encryptBlock(ht.hashTable, newHeader.hashTableSize64, hashString("(hash table)", HASH_KEY));
    MD5::checksum(ht.hashTable, newHeader.hashTableSize64, newHeader.md5HashTable);

    dest->seek(offset + start, SEEK_SET);
    if (dest->write(ht.hashTable, newHeader.hashTableSize64) != newHeader.hashTableSize64)
      return serror = error = Error::Write;
    start += newHeader.hashTableSize64;

    newHeader.blockTablePos = uint32(start);
    newHeader.blockTablePosHi = uint16(start >> 32);
    newHeader.blockTableSize64 = sizeof(MPQBlockEntry) * ht.numFiles;
    newHeader.blockTableSize = ht.numFiles;

    encryptBlock(ht.blockTable, newHeader.blockTableSize64, hashString("(block table)", HASH_KEY));
    MD5::checksum(ht.blockTable, newHeader.blockTableSize64, newHeader.md5BlockTable);

    dest->seek(offset + start, SEEK_SET);
    if (dest->write(ht.blockTable, newHeader.blockTableSize64) != newHeader.blockTableSize64)
      return serror = error = Error::Write;
    start += newHeader.blockTableSize64;

    if (ht.hiBlockTable)
    {
      newHeader.hiBlockTablePos64 = start;
      newHeader.hiBlockTableSize64 = sizeof(uint16) * ht.numFiles;
      MD5::checksum(ht.hiBlockTable, newHeader.hiBlockTableSize64, newHeader.md5HiBlockTable);
      dest->seek(offset + start, SEEK_SET);
      if (dest->write(ht.hiBlockTable, newHeader.hiBlockTableSize64) != newHeader.hiBlockTableSize64)
        return serror = error = Error::Write;
      start += newHeader.hiBlockTableSize64;
    }
  }

  newHeader.archiveSize64 = start;
  newHeader.archiveSize = uint32(start);
  MD5::checksum(&newHeader, MPQHeader::size_v4 - MD5::DIGEST_SIZE, newHeader.md5MPQHeader);

  dest->seek(offset, SEEK_SET);
  if (dest->write(&newHeader, newHeader.headerSize) != newHeader.headerSize)
    return serror = error = Error::Write;

  return error = Error::Ok;
}
uint32 Archive::Impl::flush(::File* dest, bool modify)
{
  thread::Lock::Holder locker(mutex);

  uint32 blockCount = (hetTable ? betTable->numFiles : hdr.blockTableSize);
  LocalPtr<uint64, true> oldPos = (modify ? NULL : new uint64[blockCount]);
  if (!modify)
  {
    if (hetTable)
    {
      for (uint32 cur = 0; cur < blockCount; cur++)
        oldPos[cur] = betTable->betTable[cur].filePos;
    }
    else
    {
      for (uint32 cur = 0; cur < blockCount; cur++)
      {
        oldPos[cur] = blockTable[cur].filePos;
        if (hiBlockTable)
          oldPos[cur] |= uint64(hiBlockTable[cur]) << 32;
      }
    }
  }

  error = Error::Ok;
  dest->seek(0, SEEK_SET);
  file->seek(0, SEEK_SET);
  if (!dest->copy(file, offset) != offset)
    return serror = error = Error::Read;
  Array<BetEntry> blocks;
  listBlocks(blocks);
  uint64 curPos = hdr.headerSize;
  for (uint32 cur = 0; cur < blocks.length(); cur++)
  {
    transferFile(dest, blocks[cur], curPos,
      fileNames ? (fileNames[blocks[cur].hash].empty() ? NULL : fileNames[blocks[cur].hash].c_str()) : NULL);
    if (hetTable)
    {
      uint32 bet = hetTable->betIndexTable[blocks[cur].hash];
      betTable->betTable[bet].filePos = curPos;
    }
    else
    {
      uint32 block = hashTable[blocks[cur].hash].blockIndex;
      blockTable[block].filePos = uint32(curPos);
      if (hiBlockTable)
        hiBlockTable[block] = uint16(curPos >> 16);
    }
    curPos += cmpAdjust(blocks[cur].cmpSize);
  }
  flushTablesRaw(dest, curPos);
  if (!modify)
  {
    if (hetTable)
    {
      for (uint32 cur = 0; cur < blockCount; cur++)
        betTable->betTable[cur].filePos = oldPos[cur];
    }
    else
    {
      for (uint32 cur = 0; cur < blockCount; cur++)
      {
        blockTable[cur].filePos = uint32(oldPos[cur]);
        if (hiBlockTable)
          hiBlockTable[cur] = uint16(oldPos[cur] >> 32);
      }
    }
  }
  return error;
}

void Archive::Impl::listBlocks(Array<BetEntry>& blocks)
{
  if (hetTable)
  {
    for (uint32 het = 0; het < hetTable->hashTableSize; het++)
    {
      if (hetTable->hetTable[het] == HetTable::EMPTY ||
          hetTable->betIndexTable[het] >= betTable->numFiles)
        continue;
      BetEntry bet = betTable->betTable[hetTable->betIndexTable[het]];
      bet.hash = het;
      blocks.push(bet);
    }
  }
  else
  {
    for (uint32 hash = 0; hash < hdr.hashTableSize; hash++)
    {
      if (hashTable[hash].blockIndex == MPQHashEntry::EMPTY ||
          hashTable[hash].blockIndex == MPQHashEntry::DELETED)
        continue;
      MPQBlockEntry& block = blockTable[hashTable[hash].blockIndex];
      BetEntry bet;
      bet.filePos = block.filePos;
      if (hiBlockTable)
        bet.filePos |= uint64(hiBlockTable[hashTable[hash].blockIndex]) << 32;
      bet.fileSize = block.fSize;
      bet.cmpSize = block.cSize;
      bet.flags = block.flags;
      bet.hash = hash;
      blocks.push(bet);
    }
  }
}
uint64 Archive::Impl::allocBlock(uint32 size, uint32 except)
{
  Array<BetEntry> blocks;
  listBlocks(blocks);
  blocks.sort(blockComp);
  uint64 prev = hdr.headerSize;
  for (uint32 i = 0; i < blocks.length(); i++)
  {
    if (blocks[i].hash == except)
      continue;
    if (blocks[i].filePos - prev >= size)
      return prev;
    prev = blocks[i].filePos + cmpAdjust(blocks[i].cmpSize);
  }
  return prev;
}

}
