#include "impl.h"
#include "base/gzmemory.h"
#include "zlib/zlib.h"
#include "pklib/pklib.h"
#include "bzip2/bzlib.h"
#include "adpcm/adpcm.h"
#include "huffman/huff.h"
#include "lzma/LzmaEnc.h"
#include "lzma/LzmaDec.h"
#include "sparse/sparse.h"

namespace mpq
{

uint32 zlib_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gzmem)
{
  z_stream z;
  memset (&z, 0, sizeof z);
  z.next_in = (Bytef*) in;
  z.avail_in = in_size;
  z.total_in = in_size;
  z.next_out = (Bytef*) out;
  z.avail_out = *out_size;
  z.total_out = 0;
  z.zalloc = gzalloc;
  z.zfree = gzfree;
  z.opaque = gzmem;

  int window;
  if (in_size <= 0x100)
    window = 8;
  else if (in_size < 0x200)
    window = 9;
  else if (in_size < 0x400)
    window = 10;
  else if (in_size < 0x800)
    window = 11;
  else if (in_size < 0x1000)
    window = 12;
  else if (in_size < 0x2000)
    window = 13;
  else if (in_size < 0x4000)
    window = 14;
  else
    window = 15;

  //memset (out, 0, *out_size);

  int result = deflateInit2(&z, 6, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY);
  if (result == Z_OK)
  {
    result = deflate(&z, Z_FINISH);
    *out_size = z.total_out;
    deflateEnd (&z);
  }
  return error = ((result == Z_OK || result == Z_STREAM_END) ? Error::Ok : Error::Compress);
}
uint32 zlib_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gzmem)
{
  z_stream z;
  memset (&z, 0, sizeof z);
  z.next_in = (Bytef*) in;
  z.avail_in = in_size;
  z.total_in = in_size;
  z.next_out = (Bytef*) out;
  z.avail_out = *out_size;
  z.total_out = 0;
  z.zalloc = gzalloc;
  z.zfree = gzfree;
  z.opaque = gzmem;

  //memset (out, 0, *out_size);

  int result = inflateInit(&z);
  if (result == Z_OK)
  {
    result = inflate(&z, Z_FINISH);
    *out_size = z.total_out;
    inflateEnd(&z);
  }
  return error = (z.avail_out == 0 ? Error::Ok : Error::Decompress);
}

//////////////////////////////////////////////////////////////

struct BUFFERINFO
{
  void* out;
  uint32 outPos;
  uint32 outLen;
  void* in;
  uint32 inPos;
  uint32 inLen;
};
unsigned int FillInput(char* buffer, unsigned int* size, void* param)
{
  BUFFERINFO* bi = (BUFFERINFO*) param;
  uint32 bufSize = *size;
  if (bufSize >= bi->inLen - bi->inPos)
    bufSize = bi->inLen - bi->inPos;
  memcpy (buffer, (char*) bi->in + bi->inPos, bufSize);
  bi->inPos += bufSize;
  return bufSize;
}
void FillOutput (char* buffer, unsigned int* size, void* param)
{
  BUFFERINFO* bi = (BUFFERINFO*) param;
  uint32 bufSize = *size;
  if (bufSize >= bi->outLen - bi->outPos)
    bufSize = bi->outLen - bi->outPos;
  memcpy ((char*) bi->out + bi->outPos, buffer, bufSize);
  bi->outPos += bufSize;
}

uint32 pkzip_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  void* buf = gz->alloc(CMP_BUFFER_SIZE);

  BUFFERINFO bi;
  bi.in = in;
  bi.inPos = 0;
  bi.inLen = in_size;
  bi.out = out;
  bi.outPos = 0;
  bi.outLen = *out_size;

  unsigned int compType = CMP_BINARY;
  unsigned int dictSize;
  if (in_size >= 0xC00)
    dictSize = 0x1000;
  else if (in_size < 0x600)
    dictSize = 0x400;
  else
    dictSize = 0x800;

  implode(FillInput, FillOutput, (char*) buf, &bi, &compType, &dictSize);
  *out_size = bi.outPos;
  gz->free (buf);
  return error = Error::Ok;
}

uint32 pkzip_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  void* buf = gz->alloc(CMP_BUFFER_SIZE);

  BUFFERINFO bi;
  bi.in = in;
  bi.inPos = 0;
  bi.inLen = in_size;
  bi.out = out;
  bi.outPos = 0;
  bi.outLen = *out_size;

  explode(FillInput, FillOutput, (char*) buf, &bi);
  *out_size = bi.outPos;
  gz->free(buf);
  return error = Error::Ok;
}

////////////////////////////////////////////

uint32 bzip2_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  bz_stream bz;
  bz.bzalloc = gzalloci;
  bz.bzfree = gzfree;
  bz.opaque = gz;
  if (BZ2_bzCompressInit(&bz, 9, 0, 0x30) != BZ_OK)
    return error = Error::Compress;
  bz.next_in = (char*) in;
  bz.avail_in = in_size;
  bz.next_out = (char*) out;
  bz.avail_out = *out_size;

  int result;
  do
  {
    result = BZ2_bzCompress(&bz, bz.avail_in != 0 ? BZ_RUN : BZ_FINISH);
  } while (result >= 0 && result != BZ_STREAM_END);
  BZ2_bzCompressEnd(&bz);
  *out_size = bz.total_out_lo32;

  return error = (result >= 0 ? Error::Ok : Error::Compress);
}
uint32 bzip2_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  bz_stream bz;
  bz.bzalloc = gzalloci;
  bz.bzfree = gzfree;
  bz.opaque = gz;
  if (BZ2_bzDecompressInit(&bz, 0, 0) != BZ_OK)
    return error = Error::Compress;
  bz.next_in = (char*) in;
  bz.avail_in = in_size;
  bz.next_out = (char*) out;
  bz.avail_out = *out_size;

  int result;
  do
  {
    result = BZ2_bzDecompress(&bz);
  } while (result >= 0 && result != BZ_STREAM_END);
  BZ2_bzDecompressEnd(&bz);
  *out_size = bz.total_out_lo32;

  return error = (result >= 0 ? Error::Ok : Error::Compress);
}

////////////////////////////////////////////

uint32 adpcm_mono_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  *out_size = CompressADPCM(out, *out_size, in, in_size, 1, 5);
  return error = Error::Ok;
}
uint32 adpcm_mono_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  *out_size = DecompressADPCM(out, *out_size, in, in_size, 1);
  return error = Error::Ok;
}

uint32 adpcm_stereo_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  *out_size = CompressADPCM(out, *out_size, in, in_size, 2, 5);
  return error = Error::Ok;
}
uint32 adpcm_stereo_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  *out_size = DecompressADPCM(out, *out_size, in, in_size, 2);
  return error = Error::Ok;
}

////////////////////////////////////////////

uint32 huffmann_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  THuffmannTree ht(true);
  TOutputStream os(out, *out_size);
  *out_size = ht.Compress(&os, in, in_size, 7);
  return error = Error::Ok;
}
uint32 huffmann_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  THuffmannTree ht(false);
  TInputStream is(in, in_size);
  *out_size = ht.Decompress(out, *out_size, &is);
  return error = Error::Ok;
}

////////////////////////////////////////////

#define LZMA_HEADER_SIZE (1 + LZMA_PROPS_SIZE + 8)

static SRes lzma_callback_progress(void* p, UInt64 inSize, UInt64 outSize)
{
  return SZ_OK;
}
struct LzAlloc : public ISzAlloc
{
  gzmemory* gz;
  static void* alloc(void* p, size_t size)
  {
    return gzalloc(((LzAlloc*) p)->gz, 1, size);
  }
  static void free(void* p, void* address)
  {
    gzfree(((LzAlloc*) p)->gz, address);
  }
  LzAlloc(gzmemory* gzmem)
  {
    Alloc = alloc;
    Free = free;
    gz = gzmem;
  }
};

uint32 lzma_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  LzAlloc alloc(gz);

  ICompressProgress progress;
  progress.Progress = lzma_callback_progress;

  CLzmaEncProps props;
  LzmaEncProps_Init(&props);

  Byte encodedProps[LZMA_PROPS_SIZE];
  size_t encodedPropsSize = LZMA_PROPS_SIZE;

  SizeT tmp_size = *out_size - LZMA_HEADER_SIZE;
  if (LzmaEncode((Byte*) out + LZMA_HEADER_SIZE, &tmp_size,
      (Byte*) in, in_size, &props, encodedProps, &encodedPropsSize,
      0, &progress, &alloc, &alloc) != SZ_OK)
    return error = Error::Compress;
  if (tmp_size >= *out_size - LZMA_HEADER_SIZE)
    return error = Error::Compress;

  uint8* ptr = (uint8*) out;
  *ptr++ = 0;
  memcpy(ptr, encodedProps, encodedPropsSize);
  ptr += encodedPropsSize;
  *(uint64*) ptr = in_size;
  *out_size = tmp_size + LZMA_HEADER_SIZE;

  return error = Error::Ok;
}
uint32 lzma_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  LzAlloc alloc(gz);

  ELzmaStatus status;

  SizeT src_size = in_size - LZMA_HEADER_SIZE;
  if (LzmaDecode((Byte*) out, (SizeT*) out_size, (Byte*) in + LZMA_HEADER_SIZE,
      &src_size, (Byte*) in + 1, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &alloc) != SZ_OK)
    return error = Error::Decompress;

  return error = Error::Ok;
}

////////////////////////////////////////////

uint32 sparse_compress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  CompressSparse(out, (int*) out_size, in, in_size);
  return error = Error::Ok;
}
uint32 sparse_decompress(void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz)
{
  DecompressSparse(out, (int*) out_size, in, in_size);
  return error = Error::Ok;
}

////////////////////////////////////////////

struct CompressionType
{
  uint8 id;
  uint32 (*func) (void* in, uint32 in_size, void* out, uint32* out_size, gzmemory* gz);
};
static CompressionType comp_table[] = {
  {Compression::LZMA, lzma_compress},
  {Compression::Sparse, sparse_compress},
  {Compression::WaveMono, adpcm_mono_compress},
  {Compression::WaveStereo, adpcm_stereo_compress},
  {Compression::Huffman, huffmann_compress},
  {Compression::ZLIB, zlib_compress},
  {Compression::PKZIP, pkzip_compress},
  {Compression::BZIP2, bzip2_compress},
};
static CompressionType decomp_table[] = {
  {Compression::LZMA, lzma_decompress},
  {Compression::BZIP2, bzip2_decompress},
  {Compression::PKZIP, pkzip_decompress},
  {Compression::ZLIB, zlib_decompress},
  {Compression::Huffman, huffmann_decompress},
  {Compression::WaveStereo, adpcm_stereo_decompress},
  {Compression::WaveMono, adpcm_mono_decompress},
  {Compression::Sparse, sparse_decompress},
};

uint32 multi_compress(void* in, uint32 in_size, void* out, uint32* out_size, uint8 mask, void* temp)
{
  uint32 func_count = 0;
  uint8 cur_mask = mask;
  for (uint32 i = 0; i < sizeof comp_table / sizeof comp_table[0]; i++)
  {
    if ((cur_mask & comp_table[i].id) == comp_table[i].id)
    {
      func_count++;
      cur_mask &= ~comp_table[i].id;
    }
  }
  if (in_size <= 32 || func_count == 0)
  {
    if (*out_size < in_size)
      return error = Error::Compress;
    if (in != out)
      memcpy(out, in, in_size);
    *out_size = in_size;
    return error = Error::Ok;
  }

  gzmemory gz;
  gz.reset();

  void* tmp = (temp ? temp : gz.alloc(*out_size));

  void* out_ptr = (uint8*) out + 1;

  uint32 cur_size = in_size;
  void* cur_ptr = in;

  void* cur_out = ((func_count & 1) && (in != out) ? out : tmp);
  cur_mask = mask;
  uint8 method = 0;
  for (uint32 i = 0; i < sizeof comp_table / sizeof comp_table[0]; i++)
  {
    if ((cur_mask & comp_table[i].id) == comp_table[i].id)
    {
      uint32 size = *out_size - 2;
      uint32 res = comp_table[i].func(cur_ptr, cur_size, cur_out, &size, &gz);
      if (res == Error::Ok && size < cur_size)
      {
        cur_size = size;
        cur_ptr = cur_out;
        cur_out = (cur_out == tmp ? out_ptr : tmp);
        method |= comp_table[i].id;
      }
      cur_mask &= ~comp_table[i].id;
    }
  }
  if (method == 0)
  {
    if (tmp != temp)
      gz.free(tmp);
    if (*out_size == in_size)
    {
      if (in != out)
        memcpy(out, in, in_size);
      return error = Error::Ok;
    }
    else
      return error = Error::Compress;
  }
  if (cur_ptr != out_ptr)
    memcpy(out_ptr, cur_ptr, cur_size);
  *(uint8*) out = method;
  *out_size = cur_size + 1;
  if (tmp != temp)
    gz.free(tmp);
  return error = Error::Ok;
}
uint32 multi_decompress(void* in, uint32 in_size, void* out, uint32* out_size, void* temp)
{
  if (in_size == *out_size)
  {
    if (in != out)
      memcpy(out, in, in_size);
    return error = Error::Ok;
  }

  uint8 method = *(uint8*) in;
  void* in_ptr = (uint8*) in + 1;
  in_size--;
  uint8 cur_method = method;
  uint32 func_count = 0;
  for (uint32 i = 0; i < sizeof decomp_table / sizeof decomp_table[0]; i++)
  {
    if ((cur_method & decomp_table[i].id) == decomp_table[i].id)
    {
      func_count++;
      cur_method &= ~decomp_table[i].id;
    }
  }
  if (cur_method != 0)
    return error = Error::Decompress;

  if (func_count == 0)
  {
    if (in_size > *out_size)
      return error = Error::Decompress;
    if (in != out)
      memcpy(out, in_ptr, in_size);
    *out_size = in_size;
    return error = Error::Ok;
  }

  gzmemory gz;
  gz.reset();

  void* tmp = (temp ? temp : gz.alloc(*out_size));
  uint32 cur_size = in_size;
  void* cur_ptr = in_ptr;
  void* cur_out = ((func_count & 1) && (out != in) ? out : tmp);
  cur_method = method;
  for (uint32 i = 0; i < sizeof decomp_table / sizeof decomp_table[0]; i++)
  {
    if ((cur_method & decomp_table[i].id) == decomp_table[i].id)
    {
      uint32 size = *out_size;
      uint32 res = decomp_table[i].func(cur_ptr, cur_size, cur_out, &size, &gz);
      if (res != Error::Ok)
      {
        if (tmp != temp)
          gz.free(tmp);
        return res;
      }
      cur_size = size;
      cur_ptr = cur_out;
      cur_out = (cur_out == tmp ? out : tmp);
      cur_method &= ~comp_table[i].id;
    }
  }
  if (cur_ptr != out)
    memcpy(out, cur_ptr, cur_size);
  *out_size = cur_size;
  if (tmp != temp)
    gz.free(tmp);
  return error = Error::Ok;
}

}
