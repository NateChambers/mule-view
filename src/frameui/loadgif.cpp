#include <stdlib.h>

#include "base/file.h"
#include "image.h"
#include "base/utils.h"

#define MAX_CODES 4095

#pragma pack(push, 1)
struct GifHeader
{
  char sign[6];
  sint16 screenWidth;
  sint16 screenHeight;
  sint8 misc;
  sint8 back;
  sint8 null;
};
struct GifImageHeader
{
  sint8 sign;
  sint16 x, y;
  sint16 width, height;
  sint8 misc;
};

struct Rgb
{
  uint8 red;
  uint8 green;
  uint8 blue;
};

struct Rgba
{
  uint8 red;
  uint8 green;
  uint8 blue;
  uint8 alpha;
};
#pragma pack(pop)

struct GifDecoder
{
  sint32 codeSize;
  sint32 clearCode;
  sint32 endingCode;
  sint32 freeCode;
  sint32 topSlot;
  sint32 slot;
  uint8 b1;
  uint8 buf[257];
  uint8* pbytes;
  sint32 bytesInBlock;
  sint32 bitsLeft;
  sint32 codeMask[13];
  uint8 stack[MAX_CODES + 1];
  uint8 suffix[MAX_CODES + 1];
  sint32 prefix[MAX_CODES + 1];
  GifDecoder()
  {
    bytesInBlock = 0;
    bitsLeft = 0;
    for (int i = 0; i < 13; i++)
      codeMask[i] = (1 << i) - 1;
  }

  int loadGifBlock(File* f)
  {
    pbytes = buf;

    if (f->eof())
      return -1;
    bytesInBlock = f->getc();
    if (bytesInBlock > 0 && f->read(buf, bytesInBlock) != bytesInBlock)
      return -1;
    return bytesInBlock;
  }
  int getGifCode(File* f)
  {
    if (bitsLeft == 0)
    {
      if (bytesInBlock <= 0 && loadGifBlock(f) == -1)
        return -1;
      b1 = *pbytes++;
      bitsLeft = 8;
      bytesInBlock--;
    }
    int ret = b1 >> (8 - bitsLeft);
    while (codeSize > bitsLeft)
    {
      if (bytesInBlock <= 0 && loadGifBlock(f) == -1)
        return -1;
      b1 = *pbytes++;
      ret |= b1 << bitsLeft;
      bitsLeft += 8;
      bytesInBlock--;
    }
    bitsLeft -= codeSize;
    ret &= codeMask[codeSize];

    return ret;
  }
};

bool Image::loadGIF(File* f)
{
  GifHeader hdr;
  GifDecoder gif;

  if (f->read(&hdr, sizeof hdr) != sizeof hdr)
    return false;
  if (strncmp(hdr.sign, "GIF87a", 6) && strncmp(hdr.sign, "GIF89a", 6))
    return false;
  int bitsPerPixel = 1 + (7 & hdr.misc);
  Rgb* pal = NULL;
  if (hdr.misc & 0x80)
  {
    pal = new Rgb[1 << bitsPerPixel];
    if (f->read(pal, sizeof(Rgb) * (1 << bitsPerPixel)) != sizeof(Rgb) * (1 << bitsPerPixel))
    {
      delete[] pal;
      return false;
    }
  }
  GifImageHeader imageHdr;
  if (f->read(&imageHdr, sizeof imageHdr) != sizeof imageHdr)
  {
    delete[] pal;
    return false;
  }
  while (imageHdr.sign == '!')
  {
    int extType = *(1 + (char*) &imageHdr);
    int size = *(2 + (char*) &imageHdr);
    f->seek(size + 4 - sizeof imageHdr, SEEK_CUR);
    if (f->read(&imageHdr, sizeof imageHdr) != sizeof imageHdr)
    {
      delete[] pal;
      return false;
    }
  }
  if (imageHdr.sign != ',')
  {
    delete[] pal;
    return false;
  }
  if (imageHdr.misc & 0x80)
  {
    bitsPerPixel = 1 + (imageHdr.misc & 7);
    delete[] pal;
    pal = new Rgb[1 << bitsPerPixel];
    if (f->read(pal, sizeof(Rgb) * (1 << bitsPerPixel)) != sizeof(Rgb) * (1 << bitsPerPixel))
    {
      delete[] pal;
      return false;
    }
  }
  if (pal == NULL)
    return false;
  _width = imageHdr.width;
  _height = imageHdr.height;
  int size = f->getc();
  if (size < 2 || size > 9)
  {
    delete[] pal;
    return false;
  }
  uint8* ptr = new uint8[_width * _height];
  uint8* tmpBuf = ptr;

  gif.codeSize = size + 1;
  gif.topSlot = 1 << gif.codeSize;
  gif.clearCode = 1 << size;
  gif.endingCode = gif.clearCode + 1;
  gif.slot = gif.freeCode = gif.endingCode + 1;
  gif.bytesInBlock = gif.bitsLeft = 0;

  uint8* sp;
  sint32 code;
  sint32 c;
  sint32 oc = 0;
  sint32 fc = 0;
  sp = gif.stack;
  while ((c = gif.getGifCode(f)) != gif.endingCode)
  {
    if (c < 0)
    {
      delete[] pal;
      delete[] tmpBuf;
      return false;
    }
    if (c == gif.clearCode)
    {
      gif.codeSize = size + 1;
      gif.slot = gif.freeCode;
      gif.topSlot = 1 << gif.codeSize;
      while ((c = gif.getGifCode(f)) == gif.clearCode)
        ;
      if (c == gif.endingCode)
        break;
      if (c >= gif.slot)
        c = 0;
      oc = fc = c;
      *ptr++ = c;
    }
    else
    {
      code = c;
      if (code >= gif.slot)
      {
        if (code > gif.slot)
        {
          delete[] pal;
          delete[] tmpBuf;
          return false;
        }
        code = oc;
        *sp++ = fc;
      }
      while (code >= gif.freeCode)
      {
        *sp++ = gif.suffix[code];
        code = gif.prefix[code];
      }
      *sp++ = code;
      if (gif.slot < gif.topSlot)
      {
        gif.suffix[gif.slot] = fc = code;
        gif.prefix[gif.slot++] = oc;
        oc = c;
      }
      if (gif.slot >= gif.topSlot)
      {
        if (gif.codeSize < 12)
        {
          gif.topSlot <<= 1;
          gif.codeSize++;
        }
      }
      while (sp > gif.stack)
        *ptr++ = *--sp;
    }
  }

  _bits = new uint32[_width * _height];
  for (int i = 0; i < _width * _height; i++)
  {
    int p = tmpBuf[i];
    _bits[i] = clr(pal[p].red, pal[p].green, pal[p].blue);
  }
  delete[] tmpBuf;
  delete[] pal;

  return true;
}
