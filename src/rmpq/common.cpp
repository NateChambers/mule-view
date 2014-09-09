#include "impl.h"

namespace mpq
{

uint32 error = 0;
uint32 serror = 0;

uint32 Error::lastError()
{
  return error;
}
uint32 Error::savedError()
{
  return serror;
}
void Error::resetError()
{
  serror = 0;
}
char const* Error::toString(uint32 err)
{
  switch (err)
  {
  case Ok:
    return "The operation was completed successfully";
  case Init:
    return "The MPQ library was not initialized";
  case NoFile:
    return "File was not found";
  case NonMPQ:
    return "MPQ header not found";
  case Corrupted:
    return "Invalid/corrupted archive";
  case Read:
    return "Generic read failure";
  case Params:
    return "Invalid arguments";
  case Decompress:
    return "Unknown compression method or invalid data";
  case Eof:
    return "End of file reached";
  case Key:
    return "Could not generate file encryption seed";
  case Write:
    return "Generic write failure";
  case Full:
    return "Hash table full";
  case Access:
    return "File access denied";
  case ReadOnly:
    return "Can not modify a read-only archive/file";
  case Compress:
    return "Failed to compress data";
  default:
    return "Unknown error";
  }
}
char const* Locale::toString(uint16 locale)
{
  switch (locale)
  {
  case Neutral:
    return "Neutral";
  case Chinese:
    return "Chinese";
  case Czech:
    return "Czech";
  case German:
    return "German";
  case English:
    return "English";
  case Spanish:
    return "Spanish";
  case French:
    return "French";
  case Italian:
    return "Italian";
  case Japanese:
    return "Japanese";
  case Korean:
    return "Korean";
  case Polish:
    return "Polish";
  case Portuguese:
    return "Portuguese";
  case Russian:
    return "Russian";
  case EnglishUK:
    return "English (UK)";
  default:
    return "Unknown";
  }
}

uint32 cryptTable[HASH_SIZE];

bool initialized = false;
uint32 initialize()
{
  if (!initialized)
  {
    uint32 seed = 0x00100001;
    for (int i = 0; i < 256; i++)
    {
      for (int j = i; j < HASH_SIZE; j += 256)
      {
        seed = (seed * 125 + 3) % 0x2AAAAB;
        uint32 a = (seed & 0xFFFF) << 16;
        seed = (seed * 125 + 3) % 0x2AAAAB;
        uint32 b = (seed & 0xFFFF);
        cryptTable[j] = a | b;
      }
    }
  }
  initialized = true;
  return error = Error::Ok;
}

char const* stripPath(char const* str)
{
  int pos = strlen(str);
  while (pos && str[pos - 1] != '\\') pos--;
  return str + pos;
}
uint32 hashString(char const* str, uint32 hashType)
{
  uint32 seed1 = 0x7FED7FED;
  uint32 seed2 = 0xEEEEEEEE;
  for (int i = 0; str[i]; i++)
  {
    char ch = str[i];
    if (ch >= 'a' && ch <= 'z')
      ch = ch - 'a' + 'A';
    if (ch == '/')
      ch = '\\';
    seed1 = cryptTable[hashType * 256 + ch] ^ (seed1 + seed2);
    seed2 = ch + seed1 + seed2 * 33 + 3;
  }
  return seed1;
}
uint64 hashJenkins(char const* str)
{
  char temp[264];
  int length = 0;
  while (str[length])
  {
    if (str[length] >= 'A' && str[length] <= 'Z')
      temp[length] = str[length] - 'A' + 'a';
    else if (str[length] == '/')
      temp[length] = '\\';
    else
      temp[length] = str[length];
    length++;
  }
  return jenkins(temp, length);
}
void encryptBlock(void* ptr, uint32 size, uint32 key)
{
  uint32 seed = 0xEEEEEEEE;
  uint32* lptr = (uint32*) ptr;
  size /= sizeof (uint32);
  for (uint32 i = 0; i < size; i++)
  {
    seed += cryptTable[HASH_ENCRYPT * 256 + (key & 0xFF)];
    uint32 orig = lptr[i];
    lptr[i] ^= key + seed;
    key = ((~key << 21) + 0x11111111) | (key >> 11);
    seed += orig + seed * 32 + 3;
  }
}
void decryptBlock(void* ptr, uint32 size, uint32 key)
{
  uint32 seed = 0xEEEEEEEE;
  uint32* lptr = (uint32*) ptr;
  size /= sizeof (uint32);
  for (uint32 i = 0; i < size; i++)
  {
    seed += cryptTable[HASH_ENCRYPT * 256 + (key & 0xFF)];
    lptr[i] ^= key + seed;
    key = ((~key << 21) + 0x11111111) | (key >> 11);
    seed += lptr[i] + seed * 32 + 3;
  }
}
uint32 detectTableSeed(uint32* blocks, uint32 offset, uint32 maxSize)
{
  uint32 temp = (blocks[0] ^ offset) - 0xEEEEEEEE;
  for (uint32 i = 0; i < 256; i++)
  {
    uint32 key = temp - cryptTable[HASH_ENCRYPT * 256 + i];
    if ((key & 0xFF) != i)
      continue;

    uint32 seed = 0xEEEEEEEE + cryptTable[HASH_ENCRYPT * 256 + (key & 0xFF)];
    if (blocks[0] ^ (key + seed) != offset)
      continue; // sanity check

    uint32 saved = key + 1;
    key = ((~key << 21) + 0x11111111) | (key >> 11);
    seed += offset + seed * 32 + 3;

    seed += cryptTable[HASH_ENCRYPT * 256 + (key & 0xFF)];
    if (blocks[1] ^ (key + seed) <= offset + maxSize)
      return saved;
  }
  return 0;
}
uint32 detectFileSeed(uint32* data, uint32 size)
{
  uint32 table[3][3] = {
    {0x46464952 /*RIFF*/, size - 8, 0x45564157 /*WAVE*/},
    {0x00905A4D, 0x00000003},
    {0x34E1F3B9, 0xD5B0DBFA},
  };
  uint32 tSize[3] = {3, 2, 2};
  for (uint32 set = 0; set < 3; set++)
  {
    uint32 temp = (data[0] ^ table[set][0]) - 0xEEEEEEEE;
    for (uint32 i = 0; i < 256; i++)
    {
      uint32 key = temp - cryptTable[HASH_ENCRYPT * 256 + i];
      if ((key & 0xFF) != i)
        continue;
      uint32 seed = 0xEEEEEEEE + cryptTable[HASH_ENCRYPT * 256 + (key & 0xFF)];
      if (data[0] ^ (key + seed) != table[set][0])
        continue;

      uint32 saved = key;
      for (uint32 j = 1; j < tSize[set]; j++)
      {
        key = ((~key << 21) + 0x11111111) | (key >> 11);
        seed += table[set][j - 1] + seed * 32 + 3;
        seed += cryptTable[HASH_ENCRYPT * 256 + (key & 0xFF)];
        if (data[j] ^ (key + seed) != table[set][j])
          break;
        if (j == tSize[set] - 1)
          return saved;
      }
    }
  }
  return 0;
}

uint64 bitRead(uint8 const* ptr, uint32 pos, uint32 size)
{
  uint64 result = 0;
  ptr += (pos >> 3);
  pos &= 7;
  uint32 read = 0;
  while (size)
  {
    if (size >= 8 - pos)
    {
      result |= uint64(*ptr >> pos) << read;
      read += 8 - pos;
      size -= 8 - pos;
      ptr++;
      pos = 0;
    }
    else
    {
      result |= uint64((*ptr >> pos) & ((1 << size) - 1)) << read;
      break;
    }
  }
  return result;
}
void bitWrite(uint8* ptr, uint32 pos, uint32 size, uint64 value)
{
  ptr += (pos >> 3);
  pos &= 7;
  uint32 write = 0;
  while (size)
  {
    if (size >= 8 - pos)
    {
      *ptr = (*ptr & (~(0xFF << pos))) | ((value >> write) << pos);
      write += 8 - pos;
      size -= 8 - pos;
      ptr++;
      pos = 0;
    }
    else
    {
      *ptr = (*ptr & (~(((1 << size) - 1) << pos))) | (((value >> write) & ((1 << size) - 1)) << pos);
      break;
    }
  }
}

bool validateHash(void const* buf, uint32 size, void const* expected)
{
  uint32 sum = ((uint32*) expected)[0] | ((uint32*) expected)[1] | ((uint32*) expected)[2] | ((uint32*) expected)[3];
  if (sum == 0)
    return true;
  uint8 digest[MD5::DIGEST_SIZE];
  MD5::checksum(buf, size, digest);
  return memcmp(digest, expected, MD5::DIGEST_SIZE) == 0;
}
bool validateHash(::File* file, void const* expected)
{
  uint32 sum = ((uint32*) expected)[0] | ((uint32*) expected)[1] | ((uint32*) expected)[2] | ((uint32*) expected)[3];
  if (sum == 0)
    return true;
  uint8 digest[MD5::DIGEST_SIZE];
  file->md5(digest);
  return memcmp(digest, expected, MD5::DIGEST_SIZE) == 0;
}

}
