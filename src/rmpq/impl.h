#ifndef __RMPQ_IMPL__
#define __RMPQ_IMPL__

#include "rmpq2.h"
#include "base/checksum.h"
#include "base/thread.h"
#include "base/wstring.h"

struct gzmemory;

namespace mpq
{

uint32 initialize();

extern uint32 error;
extern uint32 serror;

enum {
  HASH_OFFSET           = 0,
  HASH_NAME1            = 1,
  HASH_NAME2            = 2,
  HASH_KEY              = 3,
  HASH_ENCRYPT          = 4,
  HASH_SIZE             = 1280
};

char const* stripPath(char const* str);
uint32 hashString(char const* str, uint32 hashType);
uint64 hashJenkins(char const* str);
void encryptBlock(void* ptr, uint32 size, uint32 key);
void decryptBlock(void* ptr, uint32 size, uint32 key);

uint32 detectTableSeed(uint32* blocks, uint32 offset, uint32 maxSize);
uint32 detectFileSeed(uint32* data, uint32 size);

#pragma pack(push, 1)
struct MPQUserData
{
  enum {
    signature = 0x1B51504D, // MPQ\x1B
  };
  uint32 id;
  uint32 userDataSize;
  uint32 headerOffs;
  uint32 userDataHeader;
};
struct MPQHeader
{
  enum {
    signature     = 0x1A51504D, // MPQ\x1A
    size_v1       = 32,
    size_v2       = 44,
    size_v3       = 68,
    size_v4       = 208,
  };
  uint32 id;
  uint32 headerSize;
  uint32 archiveSize;
  uint16 formatVersion;
  uint16 blockSize;

  uint32 hashTablePos;
  uint32 blockTablePos;
  uint32 hashTableSize;
  uint32 blockTableSize;

  // v2
  uint64 hiBlockTablePos64;
  uint16 hashTablePosHi;
  uint16 blockTablePosHi;

  // v3
  uint64 archiveSize64;
  uint64 betTablePos64;
  uint64 hetTablePos64;

  // v4
  uint64 hashTableSize64;
  uint64 blockTableSize64;
  uint64 hiBlockTableSize64;
  uint64 hetTableSize64;
  uint64 betTableSize64;

  uint32 rawChunkSize;
  uint8 md5BlockTable[MD5::DIGEST_SIZE];
  uint8 md5HashTable[MD5::DIGEST_SIZE];
  uint8 md5HiBlockTable[MD5::DIGEST_SIZE];
  uint8 md5BetTable[MD5::DIGEST_SIZE];
  uint8 md5HetTable[MD5::DIGEST_SIZE];
  uint8 md5MPQHeader[MD5::DIGEST_SIZE];

  void upgrade(uint64 fileSize);
};
struct MPQHashEntry
{
  enum {
    EMPTY         = 0xFFFFFFFF,
    DELETED       = 0xFFFFFFFE,
  };
  uint32 name1;
  uint32 name2;
  uint16 locale;
  uint16 platform;
  uint32 blockIndex;
};
struct MPQBlockEntry
{
  uint32 filePos;
  uint32 cSize;
  uint32 fSize;
  uint32 flags;
};

struct MPQExtTableHeader
{
  uint32 id;
  uint32 version;
  uint32 dataSize;
};

struct MPQHetTableHeader
{
  enum {
    signature = 0x1A544548, // HET\x1A
    size = 44,
  };

  uint32 tableSize;
  uint32 maxFileCount;
  uint32 hashTableSize;
  uint32 hashEntrySize;
  uint32 totalIndexSize;
  uint32 indexSizeExtra;
  uint32 indexSize;
  uint32 blockTableSize;
};

struct MPQBetTableHeader
{
  enum {
    signature = 0x1A544542, // BET\x1A
    size = 88,
  };

  uint32 tableSize;
  uint32 fileCount;
  uint32 unknown08;
  uint32 tableEntrySize;
  uint32 bitIndex_filePos;
  uint32 bitIndex_fileSize;
  uint32 bitIndex_cmpSize;
  uint32 bitIndex_flagIndex;
  uint32 bitIndex_unknown;
  uint32 bitCount_filePos;
  uint32 bitCount_fileSize;
  uint32 bitCount_cmpSize;
  uint32 bitCount_flagIndex;
  uint32 bitCount_unknown;
  uint32 totalBetHashSize;
  uint32 betHashSizeExtra;
  uint32 betHashSize;
  uint32 betHashArraySize;
  uint32 flagCount;
};
struct PatchInfo
{
  uint32 length;
  uint32 flags;
  uint32 dataSize;
  uint8 md5[MD5::DIGEST_SIZE];
};
struct PatchHeader
{
  enum {
    signature = 0x48435450, // PTCH
    signature_md5 = 0x5F35444D, // MD5_
    signature_xfrm = 0x4D524658, // XFRM

    type_copy = 0x59504F43, // COPY
    type_bsd0 = 0x30445342, // BSD0

    size_xfrm = 12,
  };
  uint32 id;
  uint32 patchSize;
  uint32 sizeBeforePatch;
  uint32 sizeAfterPatch;

  uint32 md5Id;
  uint32 md5BlockSize;
  uint8 md5BeforePatch[MD5::DIGEST_SIZE];
  uint8 md5AfterPatch[MD5::DIGEST_SIZE];

  uint32 xfrmId;
  uint32 xfrmBlockSize;
  uint32 xfrmPatchType;
};
#pragma pack(pop)

uint64 bitRead(uint8 const* ptr, uint32 pos, uint32 size);
void bitWrite(uint8* ptr, uint32 pos, uint32 size, uint64 value);

bool validateHash(void const* buf, uint32 size, void const* expected);
bool validateHash(::File* file, void const* expected);

struct BetEntry
{
  uint64 hash;
  uint64 filePos;
  uint32 fileSize;
  uint32 cmpSize;
  uint32 flags;
};
struct HetTable
{
  enum {
    EMPTY             = 0x00,
    DELETED           = 0x80,
  };
  uint64 hashMask;
  uint64 hashOr;
  uint32 hashShift;
  uint32 hashTableSize;
  uint8* hetTable;
  uint32* betIndexTable;
};
struct BetTable
{
  uint32 maxFiles;
  uint32 numFiles;
  uint64 hashMask;
  BetEntry* betTable;
};

uint32 pkzip_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz = NULL);
uint32 pkzip_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz = NULL);
uint32 multi_compress(void* in, uint32 in_size, void* out, uint32* out_size, uint8 mask, void* temp = NULL);
uint32 multi_decompress(void* in, uint32 in_size, void* out, uint32* out_size, void* temp = NULL);

inline uint64 offset64(uint32 offset, uint16 offsetHi)
{
  return uint64(offset) | (uint64(offsetHi) << 32);
}

///////////////////////////////////////////////////////

struct ListFile::Impl
{
  Array<String> filenames;
  bool sorted;
};

enum {
  BLOCK_GROW_STEP     = 16,
};

struct Archive::Impl
{
  friend class File;
  Archive* archive;
  ::File* file;
  uint32 flags;
  uint32 compression;
  WideString path;
  thread::Lock* mutex;
  uint64 offset;

  uint32 blockSize; // do not trust the header
  uint8* buf; // 2x blockSize

  MPQHeader hdr;
  String* fileNames; // x hashSize

  MPQHashEntry* hashTable;
  MPQBlockEntry* blockTable;
  uint16* hiBlockTable;

  HetTable* hetTable;
  BetTable* betTable;

  uint32 blockTableSpace;

  File* firstFile;

  uint8* loadTable(uint64 pos, uint64 size, uint64 compressedSize, uint32 key);

  Impl()
    : archive(NULL)
    , file(NULL)
    , buf(NULL)
    , fileNames(NULL)
    , hashTable(NULL)
    , blockTable(NULL)
    , hiBlockTable(NULL)
    , hetTable(NULL)
    , betTable(NULL)
    , firstFile(NULL)
    , mutex(NULL)
  {}
  ~Impl();

  void closeFiles();
  uint32 setFlags(uint32 value, uint32 mask);
  uint32 createEntry(char const* name, uint32 block);

  uint32 addName(char const* name);
  uint32 loadListFile();

  void listBlocks(Array<BetEntry>& blocks);
  uint32 transferFile(::File* dest, BetEntry const& block, uint64 newPos, char const* name = NULL);
  uint32 writeChecksums(::File* dest, uint64 start, uint32 size, uint8* mem = NULL);
  uint32 cmpAdjust(uint32 cmpSize);
  uint64 allocBlock(uint32 size, uint32 except = 0xFFFFFFFF);

  uint32 flush(::File* dest, bool modify);
  uint32 flushTables();
  uint32 flushTablesRaw(::File* dest, uint64 start);

  static Impl* open(::File* file, wchar_t const* filename, uint32 mode, uint32 flags);
  static Impl* create(wchar_t const* filename, uint32 flags, uint64 offset,
    uint32 hashSize, uint32 blockSize);
};

struct File::Impl
{
  friend class Archive;
  Archive* archive;
  File* next;
  File* prev;
  uint32 mode;
  uint32 index;
  uint32 key;
  uint32 keyvalid;
  uint32 modified;

  uint32 size;
  uint32 capacity;
  uint8* ptr;
  uint32 pos;

  PatchInfo* patchInfo;

  Impl(Archive* arc)
    : archive(arc)
    , next(NULL)
    , prev(NULL)
    , ptr(NULL)
    , patchInfo(NULL)
  {}
  ~Impl()
  {
    delete[] ptr;
    delete[] (uint8*) patchInfo;
  }
};

}

#endif // __RMPQ_IMPL__
