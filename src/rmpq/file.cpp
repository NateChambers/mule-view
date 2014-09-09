#include "impl.h"
#include "base/ptr.h"
#include "zlib/zlib.h"

namespace mpq
{

File::File(Impl* i)
  : impl(i)
{
  impl->next = impl->archive->impl->firstFile;
  if (impl->next)
    impl->next->impl->prev = this;
  else
    impl->archive->impl->firstFile = this;
}
File::~File()
{
  close();
}
File::Impl* File::open(Archive* archive, uint32 index, uint32 mode, uint32 key, uint32 keyvalid)
{
  if (mode != ::File::READ)
    for (File* file = archive->impl->firstFile; file; file = file->impl->next)
      if (file->impl->index == index && file->impl->mode != ::File::READ)
        return error = Error::Access, NULL;

  LocalPtr<Impl> impl = new Impl(archive);

  impl->index = index;
  impl->mode = mode;
  impl->key = key;
  impl->keyvalid = keyvalid;
  if (!keyvalid && archive->impl->fileNames && !archive->impl->fileNames[index].empty())
  {
    impl->key = hashString(stripPath(archive->impl->fileNames[index]), HASH_KEY);
    impl->keyvalid = 1;
  }

  impl->size = 0;
  impl->capacity = 0;
  impl->pos = 0;
  impl->modified = 0;

  if (mode == ::File::READ || mode == ::File::MODIFY)
  {
    uint64 filePos;
    uint32 fileSize, cmpSize, flags;
    if (archive->impl->hetTable)
    {
      if (index >= archive->impl->hetTable->hashTableSize ||
          archive->impl->hetTable->betIndexTable[index] >= archive->impl->betTable->numFiles)
        return NULL;
      uint32 bet = archive->impl->hetTable->betIndexTable[index];
      filePos = archive->impl->betTable->betTable[bet].filePos;
      fileSize = archive->impl->betTable->betTable[bet].fileSize;
      cmpSize = archive->impl->betTable->betTable[bet].cmpSize;
      flags = archive->impl->betTable->betTable[bet].flags;
    }
    else
    {
      if (index >= archive->impl->hdr.hashTableSize ||
          archive->impl->hashTable[index].blockIndex >= archive->impl->hdr.blockTableSize)
        return NULL;
      uint32 block = archive->impl->hashTable[index].blockIndex;
      filePos = archive->impl->blockTable[block].filePos;
      fileSize = archive->impl->blockTable[block].fSize;
      cmpSize = archive->impl->blockTable[block].cSize;
      flags = archive->impl->blockTable[block].flags;
    }
    uint64 filePosRaw = filePos;
    if (flags & Flags::PatchFile)
    {
      archive->impl->file->seek(archive->impl->offset + filePos, SEEK_SET);
      uint32 patchLength = archive->impl->file->read32();
      impl->patchInfo = (PatchInfo*) new uint8[patchLength];
      archive->impl->file->seek(archive->impl->offset + filePos, SEEK_SET);
      if (archive->impl->file->read(impl->patchInfo, patchLength) != patchLength)
        return serror = error = Error::Read, NULL;
      filePos += patchLength;
      cmpSize -= patchLength;
      fileSize = impl->patchInfo->dataSize;
    }

    impl->size = fileSize;
    impl->capacity = fileSize;
    impl->ptr = new uint8[fileSize];
    if (!(flags & Flags::Compressed))
      cmpSize = fileSize;

    uint32 key = impl->key;
    if (impl->keyvalid && flags & Flags::FixSeed)
      key = (key + uint32(filePosRaw)) ^ fileSize;

    if (flags & Flags::SingleUnit)
    {
      LocalPtr<uint8, true> temp = (flags & Flags::Compressed ? new uint8[cmpSize] : NULL);
      uint8* buf = (flags & Flags::Compressed ? temp.ptr() : impl->ptr);
      archive->impl->file->seek(archive->impl->offset + filePos, SEEK_SET);
      if (archive->impl->file->read(buf, cmpSize) != cmpSize)
        return serror = error = Error::Read, NULL;
      if (flags & Flags::Encrypted)
      {
        if (!impl->keyvalid)
        {
          key = detectFileSeed((uint32*) buf, fileSize);
          if (key)
          {
            impl->key = key;
            if (flags & Flags::FixSeed)
              impl->key = (key ^ fileSize) - uint32(filePosRaw);
            impl->keyvalid = 1;
          }
        }
        if (impl->keyvalid)
          decryptBlock(buf, cmpSize, key);
        else
          return serror = error = Error::Key, NULL;
      }
      if (flags & Flags::CompressMulti)
      {
        uint32 size = fileSize;
        if (multi_decompress(buf, cmpSize, impl->ptr, &size) != Error::Ok || size != fileSize)
          return serror = error = Error::Decompress, NULL;
      }
      else if (flags & Flags::CompressPkWare)
      {
        uint32 size = fileSize;
        if (pkzip_decompress(buf, cmpSize, impl->ptr, &size) != Error::Ok || size != fileSize)
          return serror = error = Error::Decompress, NULL;
      }
    }
    else if (!(flags & Flags::Compressed))
    {
      archive->impl->file->seek(archive->impl->offset + filePos, SEEK_SET);
      if (archive->impl->file->read(impl->ptr, fileSize) != fileSize)
        return serror = error = Error::Read, NULL;
      if (flags & Flags::Encrypted)
      {
        if (!impl->keyvalid)
        {
          key = detectFileSeed((uint32*) impl->ptr, fileSize);
          if (key)
          {
            impl->key = key;
            if (flags & Flags::FixSeed)
              impl->key = (key ^ fileSize) - uint32(filePosRaw);
            impl->keyvalid = 1;
          }
        }
        if (impl->keyvalid)
        {
          for (uint32 offs = 0; offs < fileSize; offs += archive->impl->blockSize)
            decryptBlock(impl->ptr + offs, fileSize - offs < archive->impl->blockSize ?
              fileSize - offs : archive->impl->blockSize, key + offs / archive->impl->blockSize);
        }
        else
          return serror = error = Error::Key, NULL;
      }
    }
    else
    {
      uint32 numBlocks = (fileSize + archive->impl->blockSize - 1) / archive->impl->blockSize;
      uint32 tableSize = numBlocks + 1;
	    if (flags & Flags::SectorCrc)
          tableSize += 1;
      LocalPtr<uint32, true> blocks = new uint32[tableSize];
      archive->impl->file->seek(archive->impl->offset + filePos, SEEK_SET);
      if (archive->impl->file->read(blocks.ptr(), tableSize * sizeof(uint32)) != tableSize * sizeof(uint32))
        return serror = error = Error::Read, NULL;
      if (flags & Flags::Encrypted)
      {
        if (!impl->keyvalid)
        {
          key = detectTableSeed(blocks.ptr(), tableSize * sizeof(uint32), archive->impl->blockSize);
          if (key)
          {
            impl->key = key;
            if (flags & Flags::FixSeed)
              impl->key = (key ^ fileSize) - uint32(filePosRaw);
            impl->keyvalid = 1;
          }
        }
        if (impl->keyvalid)
          decryptBlock(blocks.ptr(), tableSize * sizeof(uint32), key - 1);
        else
          return serror = error = Error::Key, NULL;
      }
      for (uint32 block = 0; block < numBlocks; block++)
      {
        uint32 cSize = blocks[block + 1] - blocks[block];
        uint32 uSize = archive->impl->blockSize;
        if (block * archive->impl->blockSize + uSize > fileSize)
          uSize = fileSize - block * archive->impl->blockSize;
        archive->impl->file->seek(archive->impl->offset + filePos + blocks[block], SEEK_SET);
        if (archive->impl->file->read(archive->impl->buf, cSize) != cSize)
          return serror = error = Error::Read, NULL;
        if (flags & Flags::Encrypted)
          decryptBlock(archive->impl->buf, cSize, key + block);
        if (flags & Flags::CompressMulti)
        {
          uint32 size = uSize;
          if (multi_decompress(archive->impl->buf, cSize, impl->ptr + block * archive->impl->blockSize,
              &size, archive->impl->buf + archive->impl->blockSize) != Error::Ok || size != uSize)
            return serror = error = Error::Decompress, NULL;
        }
        else if (flags & Flags::CompressPkWare)
        {
          uint32 size = fileSize;
          if (pkzip_decompress(archive->impl->buf, cSize, impl->ptr + block * archive->impl->blockSize, &size) != Error::Ok || size != uSize)
            return serror = error = Error::Decompress, NULL;
        }
      }
    }
  }
  else
  {
    impl->capacity = 256;
    impl->ptr = new uint8[impl->capacity];
  }

  return impl.yield();
}

uint32 File::reopen()
{
  if (impl == NULL)
    return serror = error = Error::Params;
  Archive* archive = impl->archive;
  uint32 index = impl->index;
  uint32 key = impl->key;
  uint32 keyvalid = impl->keyvalid;
  uint32 pos = impl->pos;
  uint32 mode = impl->mode;
  close();
  impl = open(archive, index, mode, key, keyvalid);
  if (impl == NULL)
    return error;
  impl->pos = pos;
  if (impl->pos >= impl->size)
    impl->pos = impl->size;
  impl->next = impl->archive->impl->firstFile;
  if (impl->next)
    impl->next->impl->prev = this;
  else
    impl->archive->impl->firstFile = this;
  return error = Error::Ok;
}
uint32 File::attribute(uint32 attr) const
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  return impl->archive->itemAttribute(impl->index, attr);
}
uint32 File::setFlags(uint32 flags, uint32 mask)
{
  if (impl == NULL)
    return serror = error = Error::Params;
  if (impl->mode == ::File::READ)
    return serror = error = Error::ReadOnly, 0;
  uint32 oldFlags = impl->archive->itemAttribute(impl->index, Attribute::Flags);
  uint32 newFlags = (oldFlags & mask) | flags | Flags::Exists;
  if (newFlags & Flags::SingleUnit)
    newFlags &= ~Flags::SectorCrc;
  if (!(newFlags & Flags::Compressed))
    newFlags &= ~Flags::SectorCrc;
  if (~(newFlags & Flags::Encrypted))
    newFlags &= ~Flags::FixSeed;
  if (newFlags != oldFlags)
  {
    if (impl->archive->impl->hetTable)
    {
      uint32 bet = impl->archive->impl->hetTable->betIndexTable[impl->index];
      impl->archive->impl->betTable->betTable[bet].flags = newFlags;
    }
    else
    {
      uint32 block = impl->archive->impl->hashTable[impl->index].blockIndex;
      impl->archive->impl->blockTable[block].flags = newFlags;
    }
    if ((newFlags & ~(Flags::DummyFile | Flags::PatchFile)) != 
        (newFlags & ~(Flags::DummyFile | Flags::PatchFile)))
      flush();
    impl->archive->flushTables();
  }
  return error = Error::Ok;
}

bool File::readonly() const
{
  return impl->mode == ::File::READ;
}

uint32 File::read(void* buf, uint32 count)
{
  if (impl == NULL)
    return 0;
  if (count > impl->size - impl->pos)
    count = impl->size - impl->pos;
  memcpy(buf, impl->ptr + impl->pos, count);
  impl->pos += count;
  return count;
}
uint32 File::write(void const* buf, uint32 count)
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  if (impl->mode == ::File::READ)
    return serror = error = Error::ReadOnly, 0;
  if (impl->pos + count > impl->capacity)
  {
    while (impl->pos + count > impl->capacity)
    {
      if (impl->capacity > 256 * 1024)
        impl->capacity += 256 * 1024;
      else
        impl->capacity *= 2;
    }
    uint8* temp = new uint8[impl->capacity];
    memcpy(temp, impl->ptr, impl->size);
    delete[] impl->ptr;
    impl->ptr = temp;
  }
  impl->modified = 1;
  memcpy(impl->ptr + impl->pos, buf, count);
  impl->pos += count;
  if (impl->pos > impl->size)
    impl->size = impl->pos;
  return count;
}
uint64 File::resize(uint64 newsize)
{
  if (impl == NULL)
    return serror = error = Error::Params, 0;
  if (impl->mode == ::File::READ)
    return serror = error = Error::ReadOnly, 0;
  if (newsize > impl->capacity)
  {
    while (newsize > impl->capacity)
    {
      if (impl->capacity > 256 * 1024)
        impl->capacity += 256 * 1024;
      else
        impl->capacity *= 2;
    }
    uint8* temp = new uint8[impl->capacity];
    memcpy(temp, impl->ptr, impl->size);
    delete[] impl->ptr;
    impl->ptr = temp;
  }
  impl->modified = 1;
  if (newsize > impl->size)
    memset(impl->ptr + impl->size, 0, newsize - impl->size);
  impl->size = newsize;
  return impl->size;
}

uint8 File::getc()
{
  if (impl && impl->pos < impl->size)
    return impl->ptr[impl->pos++];
  else
    return 0;
}
uint32 File::putc(uint8 c)
{
  return write(&c, 1);
}

void File::seek(uint64 pos, uint32 rel)
{
  sint32 newPos = pos;
  if (rel == SEEK_CUR)
    newPos += impl->pos;
  else if (rel == SEEK_END)
    newPos += impl->size;
  if (newPos < 0) newPos = 0;
  if (newPos >= impl->size) newPos = impl->size;
  impl->pos = newPos;
}
uint64 File::tell() const
{
  return impl->pos;
}

bool File::eof() const
{
  return impl->pos >= impl->size;
}

bool File::ok() const
{
  return impl != NULL;
}
void File::close()
{
  if (impl)
  {
    if (impl->modified)
      flush();
    if (impl->prev)
      impl->prev->impl->next = impl->next;
    else
      impl->archive->impl->firstFile = impl->next;
    if (impl->next)
      impl->next->impl->prev = impl->prev;
    delete impl;
    impl = NULL;
  }
}

void File::flush()
{
  if (impl == NULL)
  {serror = error = Error::Params; return;}
  if (impl->mode == ::File::READ)
  {serror = error = Error::ReadOnly; return;}
  thread::Lock::Holder locker(impl->archive->impl->mutex);
  impl->modified = 0;
  uint32 flags = impl->archive->itemAttribute(impl->index, Attribute::Flags);
  uint64 offset = 0;
  uint32 cmpSize = 0;
  uint32 patchLength = 0;
  if ((flags & Flags::PatchFile) && impl->patchInfo)
    patchLength = impl->patchInfo->length;
  if (flags & Flags::SingleUnit)
  {
    uint8* out = impl->ptr;
    cmpSize = impl->size;
    if (flags & (Flags::Compressed | Flags::Encrypted))
    {
      out = new uint8[cmpSize];
      if (flags & Flags::CompressMulti)
        multi_compress(impl->ptr, impl->size, out, &cmpSize, impl->archive->impl->compression);
      else if (flags & Flags::CompressPkWare)
        pkzip_compress(impl->ptr, impl->size, out, &cmpSize);
      else
        memcpy(out, impl->ptr, cmpSize);
    }
    offset = impl->archive->impl->allocBlock(cmpSize, impl->index);
    if ((flags & Flags::Encrypted) && impl->keyvalid)
    {
      uint32 key = impl->key;
      if (flags & Flags::FixSeed)
        key = (key + uint32(offset)) ^ impl->size;
      encryptBlock(out, cmpSize, key);
    }
    impl->archive->impl->file->seek(offset + patchLength + impl->archive->impl->offset, SEEK_SET);
    impl->archive->impl->file->write(out, cmpSize);
    if (out != impl->ptr)
      delete[] out;
  }
  else if (!(flags & Flags::Compressed))
  {
    offset = impl->archive->impl->allocBlock(impl->size, impl->index);
    cmpSize = impl->size;
    impl->archive->impl->file->seek(offset + patchLength + impl->archive->impl->offset, SEEK_SET);
    if ((flags & Flags::Encrypted) && impl->keyvalid)
    {
      uint32 key = impl->key;
      if (flags & Flags::FixSeed)
        key = (key + uint32(offset)) ^ impl->size;
      for (uint32 cur = 0; cur < impl->size; cur += impl->archive->impl->blockSize)
      {
        uint32 size = impl->archive->impl->blockSize;
        if (size > impl->size - cur)
          size = impl->size - cur;
        memcpy(impl->archive->impl->buf, impl->ptr + cur, size);
        encryptBlock(impl->archive->impl->buf, size, key + (cur / impl->archive->impl->blockSize));
        impl->archive->impl->file->write(impl->archive->impl->buf, size);
      }
    }
    else
      impl->archive->impl->file->write(impl->ptr, impl->size);
  }
  else
  {
    uint32 blockSize = impl->archive->impl->blockSize;
    uint32 numBlocks = (impl->size + blockSize - 1) / blockSize;
    uint32* blockSizes = new uint32[numBlocks + 1];
    uint8** blocks = new uint8*[numBlocks];
    uint32 tableSize = numBlocks + 1;
    uint32* crcTable = NULL;
    uint8* crcData = NULL;
    if (flags & Flags::SectorCrc)
    {
      tableSize += 1;
      crcTable = new uint32[numBlocks];
    }
    uint32* blockTable = new uint32[tableSize];
    blockTable[0] = tableSize * sizeof(uint32);
    for (uint32 block = 0; block < numBlocks; block++)
    {
      blockSizes[block] = blockSize;
      if (blockSizes[block] > impl->size - block * blockSize)
        blockSizes[block] = impl->size - block * blockSize;
      blocks[block] = new uint8[blockSizes[block]];
      if (flags & Flags::CompressMulti)
        multi_compress(impl->ptr + block * blockSize, blockSizes[block],
          blocks[block], &blockSizes[block],
          impl->archive->impl->compression, impl->archive->impl->buf);
      else if (flags & Flags::CompressPkWare)
        pkzip_compress(impl->ptr + block * blockSize, blockSizes[block],
          blocks[block], &blockSizes[block]);
      else
        memcpy(blocks[block], impl->ptr + block * blockSize, blockSizes[block]);
      if (crcTable)
        crcTable[block] = adler32(0, blocks[block], blockSizes[block]);
      blockTable[block + 1] = blockTable[block] + blockSizes[block];
    }
    if (flags & Flags::SectorCrc)
    {
      blockSizes[numBlocks] = numBlocks * sizeof(uint32);
      crcData = new uint8[blockSizes[numBlocks]];
      multi_compress(crcTable, blockSizes[numBlocks], crcData, &blockSizes[numBlocks], Compression::ZLIB);
      blockTable[numBlocks + 1] = blockTable[numBlocks] + blockSizes[numBlocks];
    }
    offset = impl->archive->impl->allocBlock(blockTable[numBlocks], impl->index);
    cmpSize = blockTable[tableSize - 1];
    if ((flags & Flags::Encrypted) && impl->keyvalid)
    {
      uint32 key = impl->key;
      if (flags & Flags::FixSeed)
        key = (key + uint32(offset)) ^ impl->size;
      encryptBlock(blockTable, tableSize * sizeof(uint32), key - 1);
      for (uint32 block = 0; block < numBlocks; block++)
        encryptBlock(blocks[block], blockSizes[block], key + block);
    }
    impl->archive->impl->file->seek(offset + patchLength + impl->archive->impl->offset, SEEK_SET);
    impl->archive->impl->file->write(blockTable, tableSize * sizeof(uint32));
    for (uint32 block = 0; block < tableSize - 1; block++)
    {
      impl->archive->impl->file->write(blocks[block], blockSizes[block]);
      delete[] blocks[block];
    }
    delete[] blocks;
    delete[] blockSizes;
    delete[] blockTable;
    delete[] crcTable;
    delete[] crcData;
  }
  if ((flags & Flags::PatchFile) && impl->patchInfo)
  {
    impl->archive->impl->file->seek(offset + impl->archive->impl->offset, SEEK_SET);
    impl->archive->impl->file->write(impl->patchInfo, impl->patchInfo->length);
    cmpSize += impl->patchInfo->length;
  }
  if (impl->archive->impl->hetTable)
  {
    uint32 bet = impl->archive->impl->hetTable->betIndexTable[impl->index];
    impl->archive->impl->betTable->betTable[bet].filePos = offset;
    impl->archive->impl->betTable->betTable[bet].cmpSize = cmpSize;
  }
  else
  {
    uint32 block = impl->archive->impl->hashTable[impl->index].blockIndex;
    impl->archive->impl->blockTable[block].filePos = uint32(offset);
    impl->archive->impl->blockTable[block].cSize = cmpSize;
    if (impl->archive->impl->hiBlockTable)
      impl->archive->impl->hiBlockTable[block] = uint16(offset >> 32);
  }
  impl->archive->flushTables();
  error = Error::Ok;
}

struct BSDFile
{
  uint64 signature;
  uint64 ctrlBlockSize;
  uint64 dataBlockSize;
  uint64 newFileSize;
};

bool File::isPatchFile() const
{
  return impl->patchInfo != NULL;
}
::File* File::patchFile(::File* file)
{
  if (impl == NULL || file == NULL)
    return serror = error = Error::Params, NULL;
  PatchHeader hdr;
  seek(0, SEEK_SET);
  if (read(&hdr, sizeof hdr) != sizeof hdr)
    return serror = error = Error::Read, NULL;
  if (hdr.id != PatchHeader::signature ||
      hdr.md5Id != PatchHeader::signature_md5 ||
      hdr.xfrmId != PatchHeader::signature_xfrm)
    return serror = error = Error::Corrupted, NULL;
  if (hdr.sizeBeforePatch && !validateHash(file, hdr.md5BeforePatch))
    return serror = error = Error::Corrupted, NULL;
  ::File* result = NULL;
  if (hdr.xfrmPatchType == PatchHeader::type_copy)
  {
    uint32 newSize = hdr.xfrmBlockSize - PatchHeader::size_xfrm;
    LocalPtr<uint8, true> data = new uint8[newSize];
    if (read(data.ptr(), newSize) != newSize)
      return serror = error = Error::Read, NULL;
    result = File::memfile(data.yield(), newSize);
  }
  else if (hdr.xfrmPatchType == PatchHeader::type_bsd0)
  {
    uint32 patchSize = hdr.patchSize - sizeof(PatchHeader);
    LocalPtr<uint8, true> patch = new uint8[patchSize];
    memset(patch.ptr(), 0, patchSize);
    seek(4, SEEK_CUR);
    if (hdr.xfrmBlockSize - PatchHeader::size_xfrm < patchSize)
    {
      uint32 pos = 0;
      while (pos < patchSize)
      {
        uint8 byte = getc();
        if (byte & 0x80)
        {
          uint32 repeat = (byte & 0x7F) + 1;
          while (pos < patchSize && repeat--)
            patch[pos++] = getc();
        }
        else
          pos += byte + 1;
      }
    }
    else if (read(patch.ptr(), hdr.patchSize - sizeof(PatchHeader)) != hdr.patchSize - sizeof(PatchHeader))
      return serror = error = Error::Read, NULL;

    BSDFile* bsd = (BSDFile*) patch.ptr();
    uint32* patchCtrl = (uint32*) (bsd + 1);
    uint8* patchData = ((uint8*) patchCtrl) + bsd->ctrlBlockSize;
    uint8* patchExtra = patchData + bsd->dataBlockSize;

    uint32 newSize = bsd->newFileSize;
    LocalPtr<uint8, true> data = new uint8[newSize];

    file->seek(0, SEEK_SET);

    uint32 newPos = 0;
    while (newPos < newSize)
    {
      uint32 addLength = patchCtrl[0];
      uint32 moveLength = patchCtrl[1];
      uint32 oldLength = patchCtrl[2];
      patchCtrl += 3;

      if (newPos + addLength > newSize)
        return serror = error = Error::Corrupted, NULL;
      memcpy(data.ptr() + newPos, patchData, addLength);
      patchData += addLength;
      for (uint32 i = 0; i < addLength; i++)
        data[newPos++] += file->getc();

      if (newPos + moveLength > newSize)
        return serror = error = Error::Corrupted, NULL;
      memcpy(data.ptr() + newPos, patchExtra, moveLength);
      newPos += moveLength;
      patchExtra += moveLength;

      if (oldLength & 0x80000000)
        oldLength = 0x80000000 - oldLength;
      file->seek(oldLength, SEEK_CUR);
    }

    result = File::memfile(data.yield(), newSize);
  }
  else
    return serror = error = Error::Corrupted, NULL;
  if (result && hdr.sizeAfterPatch && !validateHash(result, hdr.md5AfterPatch))
  {
    delete result;
    return serror = error = Error::Corrupted, NULL;
  }
  return result;
}

}
