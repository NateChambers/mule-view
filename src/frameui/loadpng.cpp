#include <stdlib.h>

#include "zlib/zlib.h"

#include "base/file.h"
#include "image.h"
#include "base/gzmemory.h"

namespace _png
{

unsigned char const pngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

struct PNGChunk
{
  uint32 length;
  uint32 type;
  uint8* data;
  uint32 crc;
  PNGChunk()
  {
    data = NULL;
  }
  ~PNGChunk()
  {
    delete[] data;
  }
};

#pragma pack(push, 1)
struct PNGHeader
{
  uint32 width;
  uint32 height;
  uint8 bitDepth;
  uint8 colorType;
  uint8 compressionMethod;
  uint8 filterMethod;
  uint8 interlaceMethod;
};
#pragma	pack(pop)

uint32 flip_int(uint32 i)
{
  return ((i >> 24) & 0x000000FF) |
         ((i >>  8) & 0x0000FF00) |
         ((i <<  8) & 0x00FF0000) |
         ((i << 24) & 0xFF000000);
}
uint32 update_crc(uint32 crc, void const* vbuf, uint32 length)
{
  static uint32 crc_table[256];
  static bool table_computed = false;
  uint8 const* buf = (uint8*) vbuf;
  if (!table_computed)
  {
    for (uint32 i = 0; i < 256; i++)
    {
      uint32 c = i;
      for (int k = 0; k < 8; k++)
      {
        if (c & 1)
          c = 0xEDB88320L ^ (c >> 1);
        else
          c = c >> 1;
      }
      crc_table[i] = c;
    }
    table_computed = true;
  }
  for (uint32 i = 0; i < length; i++)
    crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}
bool read_chunk(PNGChunk& ch, File* f)
{
  delete[] ch.data;
  ch.data = NULL;
  if (f->read(&ch.length, 4) != 4)
    return false;
  if (f->read(&ch.type, 4) != 4)
    return false;
  ch.length = flip_int(ch.length);
  ch.data = new uint8[ch.length];
  if (f->read(ch.data, ch.length) != ch.length)
    return false;
  if (f->read(&ch.crc, 4) != 4)
    return false;
  ch.crc = flip_int(ch.crc);
  uint32 crc = update_crc(0xFFFFFFFF, &ch.type, 4);
  crc = update_crc(crc, ch.data, ch.length);
  crc = ~crc;
  if (crc != ch.crc)
    return false;
  ch.type = flip_int(ch.type);
  return true;
}

void write_chunk(uint32 type, uint32 length, void const* data, File* f)
{
  type = flip_int(type);
  f->write32(flip_int(length));
  f->write32(type);
  f->write(data, length);
  uint32 crc = update_crc(0xFFFFFFFF, &type, 4);
  crc = ~update_crc(crc, data, length);
  f->write32(flip_int(crc));
}

struct PNGPalette
{
  uint32 rgba[256];
  uint32 size;
  bool useColorKey;
  uint32 colorKey[3];
};

uint32 get_image_size(uint32 width, uint32 height, PNGHeader const& hdr)
{
  uint32 bpp;
  if (hdr.colorType == 0 || hdr.colorType == 3)
    bpp = hdr.bitDepth;
  else if (hdr.colorType == 4)
    bpp = hdr.bitDepth * 2;
  else if (hdr.colorType == 2)
    bpp = hdr.bitDepth * 3;
  else if (hdr.colorType == 6)
    bpp = hdr.bitDepth * 4;
  uint32 bpl = (bpp * width + 7) / 8;
  return height * (bpl + 1);
}

uint8 paethPredictor(uint8 a, uint8 b, uint8 c)
{
  int ia = int(a) & 0xFF;
  int ib = int(b) & 0xFF;
  int ic = int(c) & 0xFF;
  int p = ia + ib - ic;
  int pa = abs(p - ia);
  int pb = abs(p - ib);
  int pc = abs(p - ic);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

class BitStream
{
  uint8 const* buf;
  uint8 cur;
public:
  BitStream (uint8 const* src);
  uint32 read (uint32 count);
};
BitStream::BitStream(uint8 const* src)
{
  buf = src;
  cur = 8;
}
uint32 BitStream::read(uint32 count)
{
  if (count == 16)
  {
    uint32 res = buf[1] + (buf[0] << 8);
    buf += 2;
    return res;
  }
  else if (count == 8)
    return *buf++;
  else if (count == 4)
  {
    uint32 res = (buf[0] >> (cur - 4)) & 0x0F;
    cur -= 4;
    if (cur == 0)
    {
      cur = 8;
      buf++;
    }
    return res;
  }
  else if (count == 2)
  {
    uint32 res = (buf[0] >> (cur - 2)) & 0x03;
    cur -= 2;
    if (cur == 0)
    {
      cur = 8;
      buf++;
    }
    return res;
  }
  else if (count == 1)
  {
    uint32 res = (buf[0] >> (cur - 1)) & 0x01;
    cur -= 1;
    if (cur == 0)
    {
      cur = 8;
      buf++;
    }
    return res;
  }
  return 0;
}
uint32 fix_color(uint32 src, uint32 count)
{
  if (count == 16)
    return src >> 8;
  else if (count < 8)
    return (src * 255) / ((1 << count) - 1);
  else
    return src;
}

uint8 fix_sample(uint8 cur, uint8 a, uint8 b, uint8 c, uint8 filter)
{
  if (filter == 0)
    return cur;
  else if (filter == 1)
    return cur + a;
  else if (filter == 2)
    return cur + b;
  else if (filter == 3)
    return cur + (a + b) / 2;
  else if (filter == 4)
    return cur + paethPredictor(a, b, c);
  return cur;
}

bool read_sub_image(uint32 width, uint32 height, uint32* data,
                    uint8 const* src, PNGHeader const& hdr, PNGPalette const& pal)
{
  uint32 bpp;
  if (hdr.colorType == 0 || hdr.colorType == 3)
    bpp = hdr.bitDepth;
  else if (hdr.colorType == 4)
    bpp = hdr.bitDepth * 2;
  else if (hdr.colorType == 2)
    bpp = hdr.bitDepth * 3;
  else if (hdr.colorType == 6)
    bpp = hdr.bitDepth * 4;
  uint32 bpl = (bpp * width + 7) / 8;
  bpp = (bpp + 7) / 8;

  uint8* prevLine = new uint8[bpl];
  uint8* curLine = new uint8[bpl];
  memset (prevLine, 0, bpl);

  for (uint32 y = 0; y < height; y++)
  {
    uint8 filter = *src++;
    if (filter > 4)
    {
      delete[] prevLine;
      delete[] curLine;
      return false;
    }
    for (uint32 i = 0; i < bpl; i++)
    {
      uint8 cur = src[i];
      uint8 a = i < bpp ? 0 : curLine[i - bpp];
      uint8 b = prevLine[i];
      uint8 c = i < bpp ? 0 : prevLine[i - bpp];
      curLine[i] = fix_sample(cur, a, b, c, filter);
    }
    BitStream buf (curLine);
    for (uint32 x = 0; x < width; x++)
    {
      uint32 cur = 0;
      if (hdr.colorType == 0)
      {
        uint32 gray = buf.read(hdr.bitDepth);
        if (pal.useColorKey && gray == pal.colorKey[0])
          cur = 0;
        else
        {
          gray = fix_color(gray, hdr.bitDepth);
          cur = Image::clr(gray, gray, gray);
        }
      }
      else if (hdr.colorType == 2)
      {
        uint32 red = buf.read(hdr.bitDepth);
        uint32 green = buf.read(hdr.bitDepth);
        uint32 blue = buf.read(hdr.bitDepth);
        if (pal.useColorKey && red == pal.colorKey[0] &&
            green == pal.colorKey[1] && blue == pal.colorKey[2])
          cur = 0;
        else
          cur = Image::clr(fix_color(red, hdr.bitDepth),
            fix_color(green, hdr.bitDepth), fix_color(blue, hdr.bitDepth));
      }
      else if (hdr.colorType == 3)
      {
        uint32 index = buf.read(hdr.bitDepth);
        if (index >= pal.size)
        {
          delete[] prevLine;
          delete[] curLine;
          return false;
        }
        cur = pal.rgba[index];
      }
      else if (hdr.colorType == 4)
      {
        uint32 gray = fix_color(buf.read(hdr.bitDepth), hdr.bitDepth);
        uint32 alpha = fix_color(buf.read(hdr.bitDepth), hdr.bitDepth);
        cur = Image::clr(gray, gray, gray, alpha);
      }
      else if (hdr.colorType == 6)
      {
        uint32 red = fix_color(buf.read(hdr.bitDepth), hdr.bitDepth);
        uint32 green = fix_color(buf.read(hdr.bitDepth), hdr.bitDepth);
        uint32 blue = fix_color(buf.read(hdr.bitDepth), hdr.bitDepth);
        uint32 alpha = fix_color(buf.read(hdr.bitDepth), hdr.bitDepth);
        cur = Image::clr(red, green, blue, alpha);
      }
      *data++ = cur;
    }
    memcpy(prevLine, curLine, bpl);
    src += bpl;
  }
  delete[] prevLine;
  delete[] curLine;
  return true;
}

struct SubImage
{
  uint32 width;
  uint32 height;
  uint32 src_size;
  uint8* src;
  uint32* data;
  int ref;
};

void write_image(uint32* src, uint32 srcw, uint32 srch,
                 uint32* dst, uint32 dstw, uint32 dsth,
                 uint32 sx, uint32 sy, uint32 dx, uint32 dy)
{
  for (uint32 y = 0; y < srch; y++)
  {
    uint32* srcl = src + y * srcw;
    uint32* dstl = dst + (sy + y * dy) * dstw + sx;
    for (uint32 x = 0; x < srcw; x++)
    {
      *dstl = *srcl;
      srcl++;
      dstl += dx;
    }
  }
}

inline uint8 unalpha(uint32 c, uint32 a)
{
  //uint32 res = (c * 255) / a;
  //if (res > 255) res = 255;
  //return (uint8) res;
  return c;
}

}

using namespace _png;

void Image::writePNG(File* f)
{
  f->write(pngSignature, 8);
  
  PNGHeader hdr;
  hdr.width = flip_int(_width);
  hdr.height = flip_int(_height);
  hdr.bitDepth = 8;
  hdr.colorType = 6;
  hdr.compressionMethod = 0;
  hdr.filterMethod = 0;
  hdr.interlaceMethod = 0;
  write_chunk ('IHDR', sizeof hdr, &hdr, f);

  uint32 usize = (_width * 4 + 1) * _height;
  uint8* udata = new uint8[usize];
  uint8* _dst = udata;
  uint32* _src = _bits;
  for (int y = 0; y < _height; y++)
  {
    *_dst++ = 0;
    for (int x = 0; x < _width; x++)
    {
      uint32 color = *_src++;
      uint32 alpha = (color >> 24);
      if (alpha)
      {
        *_dst++ = unalpha((color >> 16) & 0xFF, alpha);
        *_dst++ = unalpha((color >> 8) & 0xFF, alpha);
        *_dst++ = unalpha(color & 0xFF, alpha);
        *_dst++ = (uint8) alpha;
      }
      else
      {
        *(uint32*) _dst = 0;
        _dst += 4;
      }
    }
  }
  uint32 csize = (usize * 11) / 10 + 32;
  uint8* cdata = new uint8[csize];
  if (gzdeflate(udata, usize, cdata, &csize))
  {
    delete udata;
    delete cdata;
    return;
  }
  delete udata;

  write_chunk('IDAT', csize, cdata, f);
  write_chunk('IEND', 0, NULL, f);

  delete cdata;
}

bool Image::loadPNG(File* f)
{
  delete[] _bits;
  _bits = NULL;
  _width = 0;
  _height = 0;
  _flags = 0;

  uint8 sig[8];
  f->seek(0, SEEK_SET);
  if (f->read(sig, 8) != 8)
    return false;
  if (memcmp(sig, pngSignature, 8))
    return false;

  PNGChunk ch;

  PNGHeader hdr;
  if (!read_chunk(ch, f))
    return false;
  if (ch.type != 'IHDR' || ch.length != sizeof hdr)
    return false;
  memcpy(&hdr, ch.data, sizeof hdr);
  hdr.width = flip_int(hdr.width);
  hdr.height = flip_int(hdr.height);
  if (hdr.width == 0 || hdr.height == 0)
    return false; // zero sized images not allowed
  if (hdr.bitDepth < 1 || hdr.bitDepth > 16 || (hdr.bitDepth & (hdr.bitDepth - 1)) != 0)
    return false; // unknown bit depth (not 1 2 4 8 or 16)
  if (hdr.colorType != 0 && hdr.colorType != 2 && hdr.colorType != 3 &&
    hdr.colorType != 4 && hdr.colorType != 6)
    return false; // unknown color type (not 0 2 3 4 or 6)
  if (hdr.colorType != 0 && hdr.colorType != 3 && hdr.bitDepth < 8)
    return false; // bit depth of 1 2 or 4 only allowed for color type 0 or 3
  if (hdr.colorType == 3 && hdr.bitDepth == 16)
    return false; // bit depth of 16 not allowed for color type 3
  if (hdr.compressionMethod != 0)
    return false; // unknown compression method
  if (hdr.filterMethod != 0)
    return false; // unknown filter method
  if (hdr.interlaceMethod != 0 && hdr.interlaceMethod != 1)
    return false; // unknown interlace method

  _width = hdr.width;
  _height = hdr.height;
  _bits = NULL;

  SubImage passes[7];
  int numPasses;
  uint32 total_src_size = 0;
  uint32 total_data_size = 0;
  if (hdr.interlaceMethod == 0)
  {
    numPasses = 1;
    passes[0].width = _width;
    passes[0].height = _height;
    passes[0].src_size = get_image_size(_width, _height, hdr);
    passes[0].ref = 0;
    total_src_size += passes[0].src_size;
    total_data_size += _width * _height;
  }
  else
  {
    numPasses = 0;
    passes[0].width = (_width + 7) / 8;
    passes[0].height = (_height + 7) / 8;
    passes[1].width = (_width + 3) / 8;
    passes[1].height = (_height + 7) / 8;
    passes[2].width = (_width + 3) / 4;
    passes[2].height = (_height + 3) / 8;
    passes[3].width = (_width + 1) / 4;
    passes[3].height = (_width + 3) / 4;
    passes[4].width = (_width + 1) / 2;
    passes[4].height = (_height + 1) / 4;
    passes[5].width = _width / 2;
    passes[5].height = (_height + 1) / 2;
    passes[6].width = _width;
    passes[6].height = _height / 2;

    for (int i = 0; i < 7; i++)
    {
      passes[i].ref = -1;
      if (passes[i].width && passes[i].height)
      {
        if (i > numPasses)
        {
          passes[numPasses].width = passes[i].width;
          passes[numPasses].height = passes[i].height;
        }
        passes[i].ref = numPasses;
        passes[numPasses].src_size = get_image_size (passes[numPasses].width, passes[numPasses].height, hdr);
        total_src_size += passes[numPasses].src_size;
        total_data_size += passes[numPasses].width * passes[numPasses].height;
        numPasses++;
      }
    }
  }
  uint8* src = new uint8[total_src_size];
  uint32* data = new uint32[total_data_size];

  uint32 cur_src = 0;
  uint32 cur_data = 0;
  for (int i = 0; i < numPasses; i++)
  {
    passes[i].src = src + cur_src;
    passes[i].data = data + cur_data;
    cur_src += passes[i].src_size;
    cur_data += passes[i].width * passes[i].height;
  }

  PNGPalette pal;
  pal.size = 0;
  pal.useColorKey = false;

  uint32 buf_size = 65536;
  uint32 buf_count = 0;
  uint8* buf = new uint8[buf_size];
  while (true)
  {
    if (!read_chunk(ch, f))
    {
      delete[] src;
      delete[] data;
      delete[] buf;
      return false;
    }
    if (ch.type == 'IEND')
      break;
    else if (ch.type == 'PLTE')
    {
      pal.size = ch.length / 3;
      if (ch.length != pal.size * 3)
      {
        delete[] src;
        delete[] data;
        delete[] buf;
        return false;
      }
      for (uint32 i = 0; i < pal.size; i++)
      {
        uint8 red = ch.data[i * 3 + 0];
        uint8 green = ch.data[i * 3 + 1];
        uint8 blue = ch.data[i * 3 + 2];
        pal.rgba[i] = clr(red, green, blue);
      }
    }
    else if (ch.type == 'tRNS')
    {
      if (hdr.colorType == 3)
      {
        if (ch.length > pal.size)
        {
          delete[] src;
          delete[] data;
          delete[] buf;
          return false;
        }
        for (uint32 i = 0; i < pal.size && i < ch.length; i++)
          pal.rgba[i] = clr_noflip(pal.rgba[i], ch.data[i]);
      }
      else if (hdr.colorType == 0)
      {
        if (ch.length != 2)
        {
          delete[] src;
          delete[] data;
          delete[] buf;
          return false;
        }
        pal.useColorKey = true;
        pal.colorKey[0] = ch.data[1] + (ch.data[0] << 8);
      }
      else if (hdr.colorType == 2)
      {
        if (ch.length != 6)
        {
          delete[] src;
          delete[] data;
          delete[] buf;
          return false;
        }
        pal.useColorKey = true;
        pal.colorKey[0] = ch.data[1] + (ch.data[0] << 8);
        pal.colorKey[1] = ch.data[3] + (ch.data[2] << 8);
        pal.colorKey[2] = ch.data[5] + (ch.data[4] << 8);
      }
      else
      {
        delete[] src;
        delete[] data;
        delete[] buf;
        return false;
      }
    }
    else if (ch.type == 'IDAT')
    {
      if (buf_count + ch.length > buf_size)
      {
        uint32 new_buf_size = buf_size;
        while (buf_count + ch.length > new_buf_size)
          new_buf_size *= 2;
        uint8* temp = new uint8[new_buf_size];
        memcpy(temp, buf, buf_count);
        delete[] buf;
        buf = temp;
        buf_size = new_buf_size;
      }
      memcpy(buf + buf_count, ch.data, ch.length);
      buf_count += ch.length;
    }
  }
  if (gzinflate(buf, buf_count, src, &total_src_size))
  {
    delete[] src;
    delete[] data;
    delete[] buf;
    return false;
  }
  delete[] buf;

  for (int i = 0; i < numPasses; i++)
    read_sub_image(passes[i].width, passes[i].height, passes[i].data, passes[i].src, hdr, pal);

  if (hdr.interlaceMethod == 0)
  {
    _bits = passes[0].data;
    delete[] src;
  }
  else
  {
    _bits = new uint32[_width * _height];
    uint32 sx[7] = {0, 4, 0, 2, 0, 1, 0};
    uint32 dx[7] = {8, 8, 4, 4, 2, 2, 1};
    uint32 sy[7] = {0, 0, 4, 0, 2, 0, 1};
    uint32 dy[7] = {8, 8, 8, 4, 4, 2, 2};
    for (int i = 0; i < 7; i++)
    {
      if (passes[i].ref >= 0)
      {
        int j = passes[i].ref;
        write_image(passes[j].data, passes[j].width, passes[j].height,
                    _bits, _width, _height,
                    sx[i], sy[i], dx[i], dy[i]);
      }
    }
    delete[] src;
    delete[] data;
  }

  return true;
}
