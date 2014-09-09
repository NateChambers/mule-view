#include <stdlib.h>

#include "base/file.h"
#include "image.h"
#include "base/utils.h"

#pragma pack (push, 1)
struct TGAHeader
{
  uint8 idLength;
  uint8 colormapType;
  uint8 imageType;
  uint16 colormapIndex;
  uint16 colormapLength;
  uint8 colormapEntrySize;
  uint16 xOrigin;
  uint16 yOrigin;
  uint16 width;
  uint16 height;
  uint8 pixelSize;
  uint8 imageDesc;
};
#pragma pack (pop)

bool Image::loadTGA(File* f)
{
  TGAHeader hdr;
  if (f->read(&hdr, sizeof hdr) != sizeof hdr)
    return NULL;
  if (hdr.imageType != 0 && hdr.imageType != 1 && hdr.imageType != 2 &&
      hdr.imageType != 3 && hdr.imageType != 9 && hdr.imageType != 10 &&
      hdr.imageType != 11)
    return false;
  if (hdr.pixelSize != 32 && hdr.pixelSize != 24 && hdr.pixelSize != 16 &&
      hdr.pixelSize != 15 && hdr.pixelSize != 8)
    return false;
  _width = hdr.width;
  _height = hdr.height;
  f->seek((sizeof hdr) + hdr.idLength, SEEK_SET);
  _bits = new uint32[_width * _height];
  uint32* pal = NULL;
  if (hdr.imageType == 1 || hdr.imageType == 9)
  {
    pal = new uint32[hdr.colormapLength];
    if (hdr.colormapEntrySize == 15 || hdr.colormapEntrySize == 16)
    {
      for (int i = 0; i < hdr.colormapLength; i++)
      {
        int a = f->getc();
        if (f->eof())
        {
          delete[] pal;
          delete[] _bits;
          return false;
        }
        int b = f->getc();
        pal[i] = clr(((b & 0x7C) >> 2) << 16,
                     (((b & 0x03) << 3) | ((a & 0xE0) >> 5)) << 8,
                     a & 0x1F);
      }
    }
    else if (hdr.colormapEntrySize == 24)
    {
      for (int i = 0; i < hdr.colormapLength; i++)
      {
        int r = f->getc();
        int g = f->getc();
        if (f->eof())
        {
          delete[] pal;
          delete[] _bits;
          return false;
        }
        int b = f->getc();
        pal[i] = clr(r, g, b);
      }
    }
    else if (hdr.colormapEntrySize == 32)
    {
      if (f->read(pal, hdr.colormapLength * sizeof(uint32)) != hdr.colormapLength * sizeof(uint32))
      {
        delete[] pal;
        delete[] _bits;
        return false;
      }
      for (uint16 i = 0; i < hdr.colormapLength; i++)
        pal[i] = clr_rgba_noflip(pal[i]);
    }
  }
  if (hdr.imageType >= 1 && hdr.imageType <= 3)
  {
    for (int y = 0; y < _height; y++)
    {
      uint32* out;
      if (hdr.imageDesc & 0x20)
        out = _bits + y * _width;
      else
        out = _bits + (_height - y - 1) * _width;
      for (int x = 0; x < _width; x++)
      {
        if (hdr.pixelSize == 8)
        {
          if (f->eof())
          {
            delete[] pal;
            delete[] _bits;
            return false;
          }
          int c = f->getc();
          if (pal)
            *out++ = pal[c];
          else
            *out++ = clr(c, c, c);
        }
        else if (hdr.pixelSize == 24)
        {
          int b = f->getc();
          int g = f->getc();
          if (f->eof())
          {
            delete[] pal;
            delete[] _bits;
            return false;
          }
          int r = f->getc();
          *out++ = clr(r, g, b);
        }
        else
        {
          int b = f->getc();
          int g = f->getc();
          int r = f->getc();
          if (f->eof())
          {
            delete[] pal;
            delete[] _bits;
            return false;
          }
          int a = f->getc();
          *out++ = clr(r, g, b, a);
        }
      }
    }
  }
  else
  {
    int x = 0;
    int y = 0;
    uint32* out;
    if (hdr.imageDesc & 0x20)
      out = _bits + y * _width;
    else
      out = _bits + (_height - y - 1) * _width;
    while (y < _height)
    {
      if (f->eof())
      {
        delete[] pal;
        delete[] _bits;
        return false;
      }
      int pkHdr = f->getc();
      int pkSize = (pkHdr & 0x7F) + 1;
      uint32 color;
      for (int i = 0; i < pkSize; i++)
      {
        if (i == 0 || (pkHdr & 0x80) == 0)
        {
          if (hdr.pixelSize == 8)
          {
            if (f->eof())
            {
              delete[] pal;
              delete[] _bits;
              return false;
            }
            int c = f->getc();
            if (pal)
              color = pal[c];
            else
              color = clr(c, c, c);
          }
          else if (hdr.pixelSize == 24)
          {
            int b = f->getc();
            int g = f->getc();
            if (f->eof())
            {
              delete[] pal;
              delete[] _bits;
              return false;
            }
            int r = f->getc();
            color = clr(r, g, b);
          }
          else
          {
            int b = f->getc();
            int g = f->getc();
            int r = f->getc();
            if (f->eof())
            {
              delete[] pal;
              delete[] _bits;
              return false;
            }
            int a = f->getc();
            color = clr(r, g, b, a);
          }
        }
        *out++ = color;
        if (++x >= _width)
        {
          x = 0;
          if (++y >= _height)
            break;
          if (hdr.imageDesc & 0x20)
            out = _bits + y * _width;
          else
            out = _bits + (_height - y - 1) * _width;
        }
      }
    }
  }
  delete[] pal;
  return true;
}
