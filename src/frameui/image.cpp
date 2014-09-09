#include <stdlib.h>

#include "base/file.h"

#include "image.h"

//#include "icon.h"

inline int _min(int a, int b)
{
  return a < b ? a : b;
}
inline int _max(int a, int b)
{
  return a > b ? a : b;
}
inline uint8 _add(uint8 x, uint8 y)
{
  if (x > 255 - y)
    return 255;
  else
    return x + y;
}
static inline uint8 _clip(uint32 x)
{
  if (x > 255)
    return 255;
  else
    return uint8(x);
}
static inline uint8 _clip(uint64 x)
{
  if (x > 255)
    return 255;
  else
    return uint8(x);
}
inline void _qmemset(void* mem, uint32 fill, uint32 count)
{
  _asm
  {
    cld
    mov edi, mem
    mov ecx, count
    mov eax, fill
    rep stosd
  }
}
static inline uint8 _unalpha(uint32 c, uint32 a)
{
  uint32 res = (c * 255) / a;
  if (res > 255) res = 255;
  return (uint8) res;
}

unsigned char Image::mtable[65536];
bool Image::gtable = false;
void Image::initTable()
{
  if (!gtable)
  {
    for (int c = 0; c <= 255; c++)
      for (int a = 0; a <= 255; a++)
        mtable[a * 256 + c] = (uint8) (c * a / 255);
    gtable = true;
  }
}

Image::Image()
{
  initTable ();

  _width = 0;
  _height = 0;
  _bits = NULL;
  _flags = 0xFFFFFF;
}
Image::Image(int width, int height)
{
  initTable ();

  _width = width;
  _height = height;
  _bits = new uint32[width * height];
  _flags = 0xFFFFFF;
  _qmemset (_bits, 0xFF000000, width * height);
}
Image::Image(int width, int height, uint32* bits)
{
  initTable ();

  _width = width;
  _height = height;
  _bits = bits;
  _flags = _unowned | 0xFFFFFF;
  updateAlpha();
}
Image::Image(int width, int height, uint32* bits, uint32 color)
{
  initTable ();

  _width = width;
  _height = height;
  _bits = bits;
  _flags = _unowned | 0xFFFFFF;
  _qmemset (_bits, color, width * height);
  updateAlpha();
  if ((color & 0xFF000000) != 0xFF000000)
    _flags |= _alpha;
}
void Image::setSize(int width, int height)
{
  if (!(_flags & _unowned))
    delete[] _bits;
  _width = width;
  _height = height;
  _bits = new uint32[width * height];
  _flags = 0xFFFFFF;
  _qmemset (_bits, 0xFF000000, width * height);
}
void Image::setBits(int width, int height, uint32* bits)
{
  if (!(_flags & _unowned))
    delete[] _bits;
  _width = width;
  _height = height;
  _bits = bits;
  _flags = _unowned | 0xFFFFFF;
  updateAlpha();
  //_qmemset (_bits, 0xFF000000, width * height);
}
Image::Image(char const* filename)
{
  load(TempFile(File::open(filename, File::READ)));
}
Image::Image(File* file)
{
  load(file);
}
Image::~Image()
{
  if (!(_flags & _unowned))
    delete[] _bits;
}
void Image::updateAlpha()
{
  _flags &= ~_alpha;
  for (int i = 0; i < _width * _height; i++)
  {
    if ((_bits[i] & 0xFF000000) != 0xFF000000)
    {
      _flags |= _alpha;
      return;
    }
  }
}

bool Image::load(File* file)
{
  initTable();

  _width = 0;
  _height = 0;
  _bits = NULL;
  _flags = 0xFFFFFF;

  if (file)
  {
    bool loaded = false;
    file->seek(0, SEEK_SET); if (!loaded) loaded = loadGIF(file);
    file->seek(0, SEEK_SET); if (!loaded) loaded = loadPNG(file);
    file->seek(0, SEEK_SET); if (!loaded) loaded = loadTGA(file);
    file->seek(0, SEEK_SET); if (!loaded) loaded = loadJPG(file);
    if (!loaded)
    {
      delete[] _bits;
      _bits = NULL;
      _width = 0;
      _height = 0;
      _flags = 0;
    }
    else
      updateAlpha();
    return loaded;
  }
  return false;
}

void Image::blt(int x, int y, Image const* src)
{
  if (src == NULL || src->_bits == NULL || src == this) return;
  blt(x, y, src, 0, 0, src->_width, src->_height);
}
void Image::blt(int x, int y, Image const* src, int srcX, int srcY, int srcW, int srcH)
{
  if (src == NULL || src->_bits == NULL || src == this) return;
  int startY = _max(0, -y);
  int endY = _min(_height - y, srcH);
  int startX = _max(0, -x);
  int endX = _min(_width - x, srcW);
  startY += srcY;
  endY += srcY;
  startX += srcX;
  endX += srcX;
  x -= srcX;
  y -= srcY;
  for (int cy = startY; cy < endY; cy++)
  {
    uint8* srcBits = (uint8*) (src->_bits + src->_width * cy + startX);
    uint8* dstBits = (uint8*) (_bits + _width * (cy + y) + (startX + x));
    if ((src->_flags & _alpha) == 0)
      memcpy(dstBits, srcBits, (endX - startX) * 4);
    else
    {
      for (int cx = startX; cx < endX; cx++)
      {
        if (dstBits[3] == 255)
        {
          uint8* tSrc = mtable + uint32(srcBits[3]) * 256;
          uint8* tDst = mtable + (255 - uint32(srcBits[3])) * 256;
          dstBits[0] = _add(tDst[dstBits[0]], tSrc[srcBits[0]]);
          dstBits[1] = _add(tDst[dstBits[1]], tSrc[srcBits[1]]);
          dstBits[2] = _add(tDst[dstBits[2]], tSrc[srcBits[2]]);
        }
        else if (dstBits[3] == 0)
        {
          dstBits[0] = srcBits[0];
          dstBits[1] = srcBits[1];
          dstBits[2] = srcBits[2];
          dstBits[3] = srcBits[3];
        }
        else
        {
          uint32 ad = uint32(dstBits[3]) * uint32(255 - srcBits[3]);
          uint32 as = uint32(srcBits[3]) * 255;
          dstBits[0] = (uint8) _clip((uint32(dstBits[0]) * ad + uint32(srcBits[0]) * as) / (ad + as));
          dstBits[1] = (uint8) _clip((uint32(dstBits[1]) * ad + uint32(srcBits[1]) * as) / (ad + as));
          dstBits[2] = (uint8) _clip((uint32(dstBits[2]) * ad + uint32(srcBits[2]) * as) / (ad + as));
          dstBits[3] = (uint8) _clip((ad + as) / 255);
        }
        dstBits += 4;
        srcBits += 4;
      }
    }
  }
  if ((src->_flags & _alpha) == 0 && startX == 0 && endX == _width &&
      startY == 0 && endY == _height)
    _flags &= ~_alpha;
}
void Image::fill(uint32 color)
{
  _qmemset(_bits, color, _width * _height);
  if ((color & 0xFF000000) == 0xFF000000)
    _flags &= ~_alpha;
  else
    _flags |= _alpha;
}
void Image::fill(uint32 color, int x, int y, int width, int height)
{
  if (y < 0) {height += y; y = 0;}
  if (x < 0) {width += x; x = 0;}
  if (y + height > _height) height = _height - y;
  if (x + width > _width) width = _width - x;
  if (width <= 0 || height <= 0)
    return;
  for (int cy = y; cy < y + height; cy++)
    _qmemset(_bits + _width * cy + x, color, width);
  if ((color & 0xFF000000) != 0xFF000000)
    _flags |= _alpha;
  else if (x == 0 && y == 0 && width == _width && height == _height)
    _flags &= ~_alpha;
  else if (_flags & _alpha)
    updateAlpha();
}
void Image::toAlpha()
{
  _flags &= ~_alpha;
  for (int i = 0; i < _width * _height; i++)
  {
    uint32 ta = brightness((_bits[i] >> 16) & 0xFF, (_bits[i] >> 8) & 0xFF, _bits[i] & 0xFF);
    _bits[i] = clr(0xFFFFFF, ta);
    if (ta != 0xFF) _flags |= _alpha;
  }
}
void Image::setAlpha(Image* image)
{
  if (image == NULL || image->_width != _width || image->_height != _height)
    return;
  _flags &= ~_alpha;
  for (int i = 0; i < _width * _height; i++)
  {
    uint32 ta = brightness((image->_bits[i] >> 16) & 0xFF,
                           (image->_bits[i] >> 8) & 0xFF,
                           image->_bits[i] & 0xFF);
    _bits[i] = clr_noflip(_bits[i], ta);
    if (ta != 0xFF)
      _flags |= _alpha;
  }
}

HBITMAP Image::createBitmap(HDC hDC)
{
  bool nullDC = (hDC == NULL);
  if (nullDC)
    hDC = GetDC(NULL);
  HBITMAP hBitmap = CreateCompatibleBitmap(hDC, _width, _height);
  fillBitmap(hBitmap, hDC);
  if (nullDC)
    ReleaseDC(NULL, hDC);
  return hBitmap;
}
HBITMAP Image::createAlphaBitmap(HDC hDC)
{
  if ((_flags & _alpha) == 0)
    return NULL;

  bool nullDC = (hDC == NULL);
  if (nullDC)
    hDC = GetDC(NULL);

  BITMAPV5HEADER bi;
  memset(&bi, 0, sizeof bi);
  bi.bV5Size = sizeof bi;
  bi.bV5Width = _width;
  bi.bV5Height = -_height;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;
  uint8* dst;
  HBITMAP hBitmap = CreateDIBSection(hDC, (BITMAPINFO*) &bi,
    DIB_RGB_COLORS, (void**) &dst, NULL, 0);
  if (dst)
    memcpy(dst, _bits, sizeof(uint32) * _width * _height);

  if (nullDC)
    ReleaseDC(NULL, hDC);
  return hBitmap;
}
void Image::fillBitmap(HBITMAP hBitmap, HDC hDC)
{
  uint32* temp = NULL;
  if ((_flags & _alpha) && (_flags & _bgcolor))
  {
    temp = new uint32[_width * _height];
    uint32 bgr = (_flags >> 16) & 0xFF;
    uint32 bgg = (_flags >> 8) & 0xFF;
    uint32 bgb = _flags & 0xFF;
    uint8* ptr = (uint8*) temp;
    for (int i = 0; i < _width * _height; i++)
    {
      uint8* alpha = mtable + uint32(_bits[i] >> 24) * 256;
      uint8* invalpha = mtable + (255 - uint32(_bits[i] >> 24)) * 256;
      *ptr++ = _add(alpha[_bits[i] & 0xFF], invalpha[bgb]);
      *ptr++ = _add(alpha[(_bits[i] >> 8) & 0xFF], invalpha[bgg]);
      *ptr++ = _add(alpha[(_bits[i] >> 16) & 0xFF], invalpha[bgr]);
      *ptr++ = 255;
    }
  }
  BITMAPINFOHEADER bi;
  bi.biSize = sizeof bi;
  bi.biWidth = _width;
  bi.biHeight = -_height;
  bi.biPlanes = 1;
  bi.biBitCount = 32;
  bi.biCompression = BI_RGB;
  bi.biSizeImage = 0;
  bi.biXPelsPerMeter = 1;
  bi.biYPelsPerMeter = 1;
  bi.biClrUsed = 0;
  bi.biClrImportant = 0;
  SetDIBits(hDC, hBitmap, 0, _height, temp ? temp : _bits, (BITMAPINFO*) &bi, DIB_RGB_COLORS);
  delete[] temp;
}
Image* Image::fromBitmap(HBITMAP hBitmap)
{
  BITMAP bm;
  GetObject(hBitmap, sizeof bm, &bm);
  Image* image = new Image(bm.bmWidth, bm.bmHeight);

  BITMAPINFOHEADER bi;
  bi.biSize = sizeof bi;
  bi.biWidth = bm.bmWidth;
  bi.biHeight = -bm.bmHeight;
  bi.biPlanes = 1;
  bi.biBitCount = 32;
  bi.biCompression = BI_RGB;
  bi.biSizeImage = 0;
  bi.biXPelsPerMeter = 1;
  bi.biYPelsPerMeter = 1;
  bi.biClrUsed = 0;
  bi.biClrImportant = 0;
  HDC hDC = GetDC(NULL);
  GetDIBits(hDC, hBitmap, 0, bm.bmHeight, image->_bits, (BITMAPINFO*) &bi, DIB_RGB_COLORS);
  image->_flags &= ~_alpha;
  for (int i = 0; i < image->_width * image->_height; i++)
  {
    if ((image->_bits[i] & 0xFF000000) != 0xFF000000)
    {
      image->_bits[i] = clr_rgba_noflip(image->_bits[i]);
      image->_flags |= _alpha;
    }
  }

  return image;
}

void Image::blt(BLTInfo& info)
{
  if (info.src == NULL || info.src->_bits == NULL || info.src == this)
    return;
  if (info.dstW == 0 || info.dstH == 0)
    return;
  if (info.srcW == 0 || info.srcH == 0)
    return;
  if (info.dstW < 0)
  {
    info.x += info.dstW;
    info.dstW = -info.dstW;
    info.flipX = !info.flipX;
  }
  if (info.srcW < 0)
  {
    info.srcX += info.srcW;
    info.srcW = -info.srcW;
    info.flipX = !info.flipX;
  }
  if (info.dstH < 0)
  {
    info.y += info.dstH;
    info.dstH = -info.dstH;
    info.flipX = !info.flipX;
  }
  if (info.srcH < 0)
  {
    info.srcY += info.srcH;
    info.srcH = -info.srcH;
    info.flipY = !info.flipY;
  }
  bool mod = ((info.modulate & 0x00FFFFFF) != 0x00FFFFFF);
  uint8* modr = mtable + ((info.modulate >> 16) & 0xFF) * 256;
  uint8* modg = mtable + ((info.modulate >> 8) & 0xFF) * 256;
  uint8* modb = mtable + (info.modulate & 0xFF) * 256;
  uint8* moda = mtable + ((info.modulate >> 24) & 0xFF) * 256;

  int cxStart = 0;
  int cxEnd = info.dstW;
  if (info.x + cxStart < 0) cxStart = -info.x;
  if (info.x + cxEnd > _width) cxEnd = _width - info.x;
  int cyStart = 0;
  int cyEnd = info.dstH;
  if (info.y + cyStart < 0) cyStart = -info.y;
  if (info.y + cyEnd > _height) cyEnd = _height - info.y;
  if (info.srcW == info.dstW && info.srcH == info.dstH)
  {
    if (info.flipX)
    {
      if (info.srcX + info.srcW - cxStart > info.src->_width)
        cxStart = info.srcX + info.srcW - info.src->_width;
      if (info.srcX + info.srcW - cxEnd < 0)
        cxEnd = info.srcX + info.srcW;
    }
    else
    {
      if (info.srcX + cxStart < 0) cxStart = -info.srcX;
      if (info.srcX + cxEnd > info.src->_width) cxEnd = info.src->_width - info.srcX;
    }
    if (info.flipY)
    {
      if (info.srcY + info.srcH - cyStart > info.src->_height)
        cyStart = info.srcY + info.srcH - info.src->_height;
      if (info.srcY + info.srcH - cyEnd < 0)
        cyEnd = info.srcY + info.srcH;
    }
    else
    {
      if (info.srcY + cyStart < 0) cyStart = -info.srcY;
      if (info.srcY + cyEnd > info.src->_height) cyEnd = info.src->_height - info.srcY;
    }
    int dstOY = info.y;
    int dstOX = info.x;
    int srcOY = info.flipY ? info.srcY + info.srcH - 1 : info.srcY;
    int srcOX = info.flipX ? info.srcX + info.srcW - 1 : info.srcX;
    int srcDY = info.flipY ? -1 : 1;
    int srcDX = info.flipX ? -1 : 1;
    for (int cy = cyStart; cy < cyEnd && cxEnd > cxStart; cy++)
    {
      int dstY = dstOY + cy;
      int srcY = srcOY + cy * srcDY;
      uint8* srcBits = (uint8*) (info.src->_bits + info.src->_width * srcY + srcOX + cxStart);
      uint8* dstBits = (uint8*) (_bits + _width * dstY + dstOX + cxStart);
      if ((info.src->_flags & _alpha) == 0 && !info.flipX &&
          !info.desaturate && info.alphaMode == BLTInfo::Blend)
        memcpy(dstBits, srcBits, (cxEnd - cxStart) * 4);
      else
      {
        for (int cx = cxStart; cx < cxEnd; cx++)
        {
          uint8 srca = moda[srcBits[3]];
          uint8* tSrc = mtable + uint32(srca) * 256;
          uint8* tDst = mtable + (255 - uint32(srca)) * 256;
          uint8 srcb = srcBits[0];
          uint8 srcg = srcBits[1];
          uint8 srcr = srcBits[2];
          if (mod)
          {
            srcb = modb[srcb];
            srcg = modg[srcg];
            srcr = modr[srcr];
          }
          if (info.desaturate)
          {
            uint8 sat = brightness(srcr, srcg, srcb);
            srcb = sat;
            srcg = sat;
            srcr = sat;
          }
          if (info.alphaMode == BLTInfo::Blend)
          {
            if (dstBits[3] == 0xFF)
            {
              dstBits[0] = _add(tDst[dstBits[0]], tSrc[srcb]);
              dstBits[1] = _add(tDst[dstBits[1]], tSrc[srcg]);
              dstBits[2] = _add(tDst[dstBits[2]], tSrc[srcr]);
            }
            else if (dstBits[3] == 0)
            {
              dstBits[0] = srcb;
              dstBits[1] = srcg;
              dstBits[2] = srcr;
              dstBits[3] = srca;
            }
            else
            {
              uint32 ad = uint32(dstBits[3]) * uint32(255 - srca);
              uint32 as = uint32(srca) * 255;
              dstBits[0] = (uint8) _clip((uint32(dstBits[0]) * ad + uint32(srcb) * as) / (ad + as));
              dstBits[1] = (uint8) _clip((uint32(dstBits[1]) * ad + uint32(srcg) * as) / (ad + as));
              dstBits[2] = (uint8) _clip((uint32(dstBits[2]) * ad + uint32(srcr) * as) / (ad + as));
              dstBits[3] = (uint8) _clip((ad + as) / 255);
            }
          }
          else
          {
            dstBits[0] = _add(dstBits[0], tSrc[srcb]);
            dstBits[1] = _add(dstBits[1], tSrc[srcg]);
            dstBits[2] = _add(dstBits[2], tSrc[srcr]);
          }
          dstBits += 4;
          srcBits += 4 * srcDX;
        }
      }
    }
  }
  else
  {
    for (int cx = cxStart; cx < cxEnd; cx++)
    {
      for (int cy = cyStart; cy < cyEnd; cy++)
      {
        int dstX = info.x + cx;
        int dstY = info.y + cy;
        int srcX, srcY;
        if (info.flipX)
          srcX = (info.srcX * info.srcW) * info.dstW - (cx + 1) * info.srcW;
        else
          srcX = info.srcX * info.dstW + cx * info.srcW;
        if (info.flipY)
          srcY = (info.srcY * info.srcH) * info.dstH - (cy + 1) * info.srcH;
        else
          srcY = info.srcY * info.dstH + cy * info.srcH;
        uint64 cb = 0;
        uint64 cg = 0;
        uint64 cr = 0;
        uint64 ca = 0;
        uint64 area = 0;
        int curX = srcX;
        int curY = srcY;
        while (curX < srcX + info.srcW)
        {
          int px = curX / info.dstW;
          int endX = (px + 1) * info.dstW;
          if (endX > srcX + info.srcW)
            endX = srcX + info.srcW;
          if (px >= 0 && px < info.src->_width)
          {
            while (curY < srcY + info.srcH)
            {
              int py = curY / info.dstH;
              int endY = (py + 1) * info.dstH;
              if (endY > srcY + info.srcH)
                endY = srcY + info.srcH;
              if (py >= 0 && py < info.src->_height)
              {
                uint8* src = (uint8*) (info.src->_bits + info.src->_width * py + px);
                uint64 da = (endX - curX) * (endY - curY);
                uint64 alpha = moda[src[3]];
                if (info.desaturate)
                {
                  uint64 sat = uint64(brightness(modr[src[2]], modg[src[1]], modb[src[0]])) * alpha * da;
                  cb += sat;
                  cg += sat;
                  cr += sat;
                }
                else
                {
                  cb += uint64(modb[src[0]]) * alpha * da;
                  cg += uint64(modg[src[1]]) * alpha * da;
                  cr += uint64(modr[src[2]]) * alpha * da;
                }
                ca += alpha * da;
                area += da;
              }
              curY = endY;
            }
          }
          curX = endX;
        }
        if (area)
        {
          uint8* dst = (uint8*) (_bits + _width * dstY + dstX);
          if (info.alphaMode == BLTInfo::Blend)
          {
            if (dst[3] == 0)
            {
              uint64 as = (ca ? ca : 1);
              dst[0] = _clip(cb / as);
              dst[1] = _clip(cg / as);
              dst[2] = _clip(cr / as);
              dst[3] = _clip(ca / area);
            }
            else
            {
              uint64 ad = dst[3] * (255 - ca / area);
              uint64 as = (ca / area) * 255;
              dst[0] = _clip((uint64(dst[0]) * ad + cb * 255 / area) / (as + ad));
              dst[1] = _clip((uint64(dst[1]) * ad + cg * 255 / area) / (as + ad));
              dst[2] = _clip((uint64(dst[2]) * ad + cr * 255 / area) / (as + ad));
              dst[3] = _clip((ad + as) / 255);
            }
          }
          else
          {
            dst[0] = _clip(uint64(dst[0]) + cb / area);
            dst[1] = _clip(uint64(dst[1]) + cg / area);
            dst[2] = _clip(uint64(dst[2]) + cr / area);
          }
        }
      }
    }
  }
}

void Image::desaturate()
{
  uint8* data = (uint8*) _bits;
  for (int i = _width * _height; i > 0; i--)
  {
    uint8 sat = brightness(data[2], data[1], data[0]);
    data[0] = sat;
    data[1] = sat;
    data[2] = sat;
    data += 4;
  }
}

bool Image::getRect(int& left, int& top, int& right, int& bottom) const
{
  left = _width;
  top = _height;
  right = 0;
  bottom = 0;
  uint32* data = _bits;
  for (int y = 0; y < _height; y++)
  {
    for (int x = 0; x < _width; x++, data++)
    {
      if (*data & 0xFF000000)
      {
        if (x < left) left = x;
        if (x >= right) right = x + 1;
        if (y < top) top = y;
        if (y >= bottom) bottom = y + 1;
      }
    }
  }
  if (left >= right)
    return false;
  return true;
}

static inline int swork(int org, int acc, int count, float coeff)
{
  int res = int(float(org) * (1 + count * coeff) - float(acc) * coeff + 0.5f);
  if (res < 0) res = 0;
  if (res > 255) res = 255;
  return res;
}
static inline int sadd(uint32 pixel, int* acc)
{
  acc[0] += int(pixel & 0xFF);
  acc[1] += int((pixel >> 8) & 0xFF);
  acc[2] += int((pixel >> 16) & 0xFF);
  acc[3] += int((pixel >> 24) & 0xFF);
  return 1;
}
void Image::sharpen(float coeff)
{
  static int ox[4] = {-1, 0, 0, 1};
  static int oy[4] = {0, -1, 1, 0};
  uint32* extra = new uint32[_width];
  for (int y = 0; y < _height; y++)
  {
    uint32 prev;
    for (int x = 0; x < _width; x++)
    {
      int pos = x + y * _width;
      int acc[4] = {0, 0, 0, 0};
      int count = 0;
      if (x > 0)
        count += sadd(prev, acc);
      if (y > 0)
        count += sadd(extra[x], acc);
      if (x < _width - 1)
        count += sadd(_bits[pos + 1], acc);
      if (y < _height - 1)
        count += sadd(_bits[pos + _width], acc);
      prev = _bits[pos];
      extra[x] = _bits[pos];
      _bits[pos] = swork(prev & 0xFF, acc[0], count, coeff) |
                  (swork((prev >> 8) & 0xFF, acc[0], count, coeff) << 8) |
                  (swork((prev >> 16) & 0xFF, acc[0], count, coeff) << 16) |
                  (swork((prev >> 24) & 0xFF, acc[0], count, coeff) << 24);
    }
  }
  delete[] extra;
}
static inline int bwork(int org, float coeff)
{
  int res = int(float(org) * coeff + 0.5f);
  if (res < 0) res = 0;
  if (res > 255) res = 255;
  return res;
}
void Image::modBrightness(float coeff)
{
  for (int i = _width * _height - 1; i >= 0; i--)
    _bits[i] = bwork(_bits[i] & 0xFF, coeff) |
              (bwork((_bits[i] >> 8) & 0xFF, coeff) << 8) |
              (bwork((_bits[i] >> 16) & 0xFF, coeff) << 16) |
              (_bits[i] & 0xFF000000);
}
void Image::replaceColor(uint32 color, uint32 with)
{
  _flags &= ~_alpha;
  for (int i = 0; i < _width * _height; i++)
  {
    if (_bits[i] == color)
      _bits[i] = with;
    if ((_bits[i] & 0xFF000000) != 0xFF000000)
      _flags |= _alpha;
  }
}

uint32 Image::interpolate(uint32 color, uint32 from, uint32 to)
{
  uint32 clr_r = ((color >> 16) & 0xFF);
  uint32 clr_g = ((color >> 8) & 0xFF);
  uint32 clr_b = (color & 0xFF);
  uint32 from_r = ((from >> 16) & 0xFF);
  uint32 from_g = ((from >> 8) & 0xFF);
  uint32 from_b = (from & 0xFF);
  uint32 to_r = ((to >> 16) & 0xFF);
  uint32 to_g = ((to >> 8) & 0xFF);
  uint32 to_b = (to & 0xFF);
  return clr(_clip((from_r * (255 - clr_r) + to_r * clr_r) / 255),
             _clip((from_g * (255 - clr_g) + to_g * clr_g) / 255),
             _clip((from_b * (255 - clr_b) + to_b * clr_b) / 255));
}
