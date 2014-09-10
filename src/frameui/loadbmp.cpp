#include "image.h"
#include "base/file.h"

void Image::writeBMP(File* file)
{
  int perline = (_width * 3 + 3) & (~3);
  BITMAPFILEHEADER hdr;
  BITMAPINFOHEADER info;
  memset(&info, 0, sizeof info);
  hdr.bfType = 'MB';
  hdr.bfSize = sizeof hdr + sizeof info + perline * _height;
  hdr.bfReserved1 = 0;
  hdr.bfReserved2 = 0;
  hdr.bfOffBits = sizeof hdr + sizeof info;
  info.biSize = sizeof info;
  info.biWidth = _width;
  info.biHeight = _height;
  info.biPlanes = 1;
  info.biBitCount = 24;
  info.biCompression = BI_RGB;
  file->write(&hdr, sizeof hdr);
  file->write(&info, sizeof info);
  for (int i = _height - 1; i >= 0; i--)
  {
    uint32* src = _bits + i * _width;
    for (int j = 0; j < _width; j++)
    {
      file->putc(*src >> 16);
      file->putc(*src >> 8);
      file->putc(*src);
    }
    for (int j = _width * 3; j < perline; j++)
      file->putc(0);
  }
}
