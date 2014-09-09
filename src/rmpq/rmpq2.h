#ifndef __RMPQ_RMPQ2__
#define __RMPQ_RMPQ2__

#include "base/string.h"
#include "base/file.h"
#include "base/thread.h"

namespace mpq
{

namespace Error
{
  enum {
    Ok                          = 0,    // The operation was completed successfully
    Init                        = -1,   // The MPQ library was not initialized
    NoFile                      = -2,   // File was not found
    NonMPQ                      = -3,   // MPQ header not found
    Corrupted                   = -4,   // Invalid/corrupted archive
    Read                        = -5,   // Generic read failure
    Params                      = -6,   // Invalid arguments
    Decompress                  = -7,   // Unknown compression method or invalid data
    Eof                         = -8,   // End of file reached
    Key                         = -9,   // Could not generate file encryption seed
    Write                       = -10,  // Generic write failure
    Full                        = -11,  // Hash table full
    Access                      = -12,  // File access denied
    ReadOnly                    = -13,  // Can not modify a read-only archive/file
    Compress                    = -14,  // Failed to compress data
  };
  inline bool isError(uint32 val)
  {
    return val >= uint32(-13);
  }
  uint32 lastError();
  uint32 savedError();
  void resetError();
  char const* toString(uint32 error);
}
namespace Locale
{
  enum {
    Neutral                     = 0x0000,
    Chinese                     = 0x0404,
    Czech                       = 0x0405,
    German                      = 0x0407,
    English                     = 0x0409,
    Spanish                     = 0x040A,
    French                      = 0x040C,
    Italian                     = 0x0410,
    Japanese                    = 0x0411,
    Korean                      = 0x0412,
    Polish                      = 0x0415,
    Portuguese                  = 0x0416,
    Russian                     = 0x0419,
    EnglishUK                   = 0x0809,
  };
  char const* toString(uint16 locale);
}

namespace Compression
{
  enum {
    BZIP2                       = 0x10,
    PKZIP                       = 0x08,
    ZLIB                        = 0x02,
    Huffman                     = 0x01,
    WaveStereo                  = 0x80,
    WaveMono                    = 0x40,
    Sparse                      = 0x20,
    LZMA                        = 0x12,
  };
}

class ListFile
{
  struct Impl;
  Impl* impl;
public:
  ListFile(::File* file = NULL);
  ~ListFile();

  uint32 insert(char const* name, bool check = false);
  uint32 size() const;
  char const* item(uint32 pos) const;
  uint32 flush(::File* file);
  void sort();
  void merge(ListFile* list);
  bool has(char const* name);
};

class File : public ::File
{
  friend class Archive;
  struct Impl;
  Impl* impl;
  File(Impl* i);
  static Impl* open(Archive* archive, uint32 index, uint32 mode, uint32 key, uint32 keyvalid);
public:
  struct Flags
  {
    enum {
      CompressPkWare              = 0x00000100,
      CompressMulti               = 0x00000200,
      Compressed                  = 0x0000FF00,
      Encrypted                   = 0x00010000,
      FixSeed                     = 0x00020000,
      PatchFile                   = 0x00100000,
      SingleUnit                  = 0x01000000,
      DummyFile                   = 0x02000000,
      SectorCrc                   = 0x04000000,
      Exists                      = 0x80000000,
    };
  };
  struct Attribute
  {
    enum {
      Flags                       = 0,
      CompressedSize              = 1,
      Size                        = 2,
      Locale                      = 3,
      Platform                    = 4,
      Offset                      = 5,
    };
  };

  ~File();

  uint32 reopen();
  uint32 attribute(uint32 attr) const;
  uint32 setFlags(uint32 flags, uint32 mask);

  uint64 resize(uint64 newsize);

  bool readonly() const;

  uint32 read(void* buf, uint32 count);
  uint32 write(void const* buf, uint32 count);

  uint8 getc();
  uint32 putc(uint8 c);

  void seek(uint64 pos, uint32 rel);
  uint64 tell() const;

  bool eof() const;

  bool ok() const;
  void close();

  void flush();

  bool isPatchFile() const;
  ::File* patchFile(::File* file);
};

class Archive : public FileLoader
{
  friend class File;
  struct Impl;
  Impl* impl;
  void postLoad();
  Archive(Impl* i)
    : impl(i)
  {
    postLoad();
  }
public:
  struct Flags
  {
    enum {
      // mpq type
      FormatVersion1              = 0x00000001, // default value
      FormatVersion2              = 0x00000002,
      FormatVersion3              = 0x00000003,
      FormatVersion4              = 0x00000004,
      FormatVersionMask           = 0x0000000F,
      HetTable                    = 0x00000020, // FormatVersion >= 3
      ListFile                    = 0x00000040, // enabled by default
      Attributes                  = 0x00000080,

      // this flags are read-only
      ReadOnly                    = 0x00001000,
      Temporary                   = 0x00002000,

      // flags for new files
      Encrypted                   = 0x00004000,
      FixSeedEncryption           = 0x00008000,

      // enforce thread safety
      ThreadSafety                = 0x00020000,

      // these two flags are only valid for opening archives
      // ignore version field - useful for wc3 mpq's
      ForceFormatVersion1         = 0x01000000,
      // list files are enabled by default
      // set this flag at creation to disable this behavior
      NoListFile                  = 0x02000000,
      // check md5 hashes
      Validate                    = 0x04000000,
    };
  };

  // first version opens the archive as read-only
  Archive(::File* file, uint32 flags = 0);
  Archive(wchar_t const* filename, uint32 mode = ::File::READ, uint32 flags = 0);
  ~Archive();

  // same as constructors, better error handling (return NULL)
  // File::REWRITE is identical to Archive::create with default parameters
  static Archive* open(::File* file, uint32 flags = 0);
  static Archive* open(wchar_t const* filename, uint32 mode = ::File::READ, uint32 flags = 0);
  // create archive with extended parameters
  static Archive* create(wchar_t const* filename, uint32 flags = 0,
    uint64 offset = 0, uint32 hashSize = 1024, uint32 blockSize = 131072);

  // add files to internal list
  uint32 listFiles(ListFile* list);

  // get/set flags
  uint32 getFlags() const;
  // most flags implicitly modify the archive
  uint32 setFlags(uint32 flags, uint32 mask);
  uint32 getFormatVersion() const
  {
    return getFlags() & Flags::FormatVersionMask;
  }
  uint32 setFormatVersion(uint32 version)
  {
    return setFlags(version, Flags::FormatVersionMask);
  }
  // get/set the default compression method for new files
  uint32 getCompression() const;
  uint32 setCompression(uint32 method);

  // note the difference between these two functions
  // this version creates a copy of the archive, does not affect the current state
  uint32 saveAs(::File* file) const;
  // this version creates a copy of the archive and sets the new file as current
  // (closes the previous file)
  uint32 saveAs(wchar_t const* filename);

  // same as the second version of saveAs with a unique temporary name
  // also marks the (new) archive file as temporary
  uint32 makeTemp();

  // check if a file with a given (exact) name [and locale id] exists
  bool fileExists(char const* name) const;
  bool fileExists(char const* name, uint16 locale) const;

  // get the internal file table size
  uint32 getMaxFiles() const;
  // check if a file exists at given internal position (0.. getMaxFiles-1)
  bool fileExists(uint32 pos) const;
  // check if a file at given position can be opened
  bool testFile(uint32 pos) const;

  // open file with given name [and locale id]
  File* openFile(char const* name, uint32 mode);
  File* openFile(char const* name, uint16 locale, uint32 mode);
  // open file at given position
  File* openFile(uint32 pos, uint32 mode);

  ::File* load(char const* name)
  {
    return openFile(name, ::File::READ);
  }

  // get first/next file with given name
  uint32 getFirstFileIndex(char const* name) const;
  uint32 getNextFileIndex(char const* name, uint32 cur) const;
  // get index of the file with given name [and locale id]
  uint32 getFileIndex(char const* name, uint16 locale = Locale::Neutral) const;

  // following functions require a listfile
  // get the name of the file at given position
  char const* getFileName(uint32 pos) const;
  // find first file matching a regexp
  uint32 findFile(char const* mask) const;
  // find next file matching a regexp
  uint32 findNextFile(char const* mask, uint32 cur) const;

  // rename/delete file
  uint32 renameFile(char const* source, char const* dest);
  uint32 deleteFile(char const* name);
  uint32 deleteFile(uint32 pos);

  // get attributes of a file at given position
  uint32 itemAttribute(uint32 pos, uint32 attr);

  // change internal table size
  uint32 resize(uint32 maxFiles);

  // check if there are files with unknown name
  bool hasUnknowns() const;
  // fill empty table entries with deleted marker
  // required when resizing an archive with unknown file names
  uint32 fillHashTable();

  // flush/compact the entire archive
  uint32 flush();
  // flush internal tables/headers
  uint32 flushTables();
  // flush list file
  uint32 flushListfile();
};

class Loader : public FileLoader
{
  mutable thread::Lock mutex;
  struct ArchiveItem
  {
    FileLoader* loader;
    String base;
  };
  Array<ArchiveItem> archives;
public:
  Loader();
  Loader(Loader const& loader);
  ~Loader();

  void lock();
  void unlock();

  void clear();
  void addLoader(FileLoader* loader);
  void removeLoader(FileLoader* archive);
  void addArchive(Archive* archive, char const* base = NULL);
  Archive* loadArchive(wchar_t const* path, char const* base = NULL);

  ListFile* buildListFile();

  ::File* load(char const* name);
};

}

#endif // __RMPQ_RMPQ2__
