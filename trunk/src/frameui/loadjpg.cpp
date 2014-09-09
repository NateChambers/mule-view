#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>

#include "base/file.h"
#include "image.h"

extern "C"
{
  #include "jpeg/jpeglib.h"
  #include "jpeg/jerror.h"
}

struct my_error_mgr
{
  jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};
static void initSource(j_decompress_ptr)
{
}
static boolean fillInputBuffer(j_decompress_ptr)
{
  return TRUE;
}
static void skipInputData(j_decompress_ptr cinfo, long count)
{
  jpeg_source_mgr* src = cinfo->src;
  if (count > 0)
  {
    src->bytes_in_buffer -= count;
    src->next_input_byte += count;
  }
}
static void termSource(j_decompress_ptr)
{
}
static void myErrorExit(j_common_ptr cinfo)
{
  my_error_mgr* myerr = (my_error_mgr*) cinfo->err;
  longjmp(myerr->setjmp_buffer, 1);
}

bool Image::loadJPG(File* f)
{
  f->seek(0, SEEK_END);
  uint32 size = f->tell();
  f->seek(0, SEEK_SET);
  uint8* data = new uint8[size];
  if (f->read(data, size) != size)
  {
    delete[] data;
    return false;
  }

  jpeg_decompress_struct cinfo;
  my_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = myErrorExit;
  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(&cinfo);
    delete[] _bits;
    delete[] data;
    return false;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_source_mgr jsrc;

  jsrc.bytes_in_buffer = size;
  jsrc.next_input_byte = (JOCTET*) data;
  jsrc.init_source = initSource;
  jsrc.fill_input_buffer = fillInputBuffer;
  jsrc.skip_input_data = skipInputData;
  jsrc.resync_to_restart = jpeg_resync_to_restart;
  jsrc.term_source = termSource;
  cinfo.src = &jsrc;

  jpeg_read_header(&cinfo, TRUE);
  jpeg_calc_output_dimensions(&cinfo);
  jpeg_start_decompress(&cinfo);

  _width = cinfo.output_width;
  _height = cinfo.output_height;
  _bits = new uint32[_width * _height];
  int rowSpan = cinfo.image_width * cinfo.num_components;
  bool isGreyScale = (cinfo.jpeg_color_space == JCS_GRAYSCALE);
  uint32 palette[256];

  if (cinfo.quantize_colors)
  {
    int shift = 8 - cinfo.data_precision;
    if (cinfo.jpeg_color_space != JCS_GRAYSCALE)
    {
      for (int i = 0; i < cinfo.actual_number_of_colors; i++)
        palette[i] = clr(cinfo.colormap[0][i] << shift,
                          cinfo.colormap[1][i] << shift,
                          cinfo.colormap[2][i] << shift);
    }
    else
    {
      for (int i = 0; i < cinfo.actual_number_of_colors; i++)
        palette[i] = clr(cinfo.colormap[0][i] << shift,
                          cinfo.colormap[0][i] << shift,
                          cinfo.colormap[0][i] << shift);
    }
  }

  JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, rowSpan, 1);

  int y = 0;
  while (cinfo.output_scanline < cinfo.output_height)
  {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    uint32* lineBuf = &_bits[_width * y];
    if (cinfo.output_components == 1)
    {
      if (cinfo.quantize_colors)
      {
        for (int i = 0; i < _width; i++)
          lineBuf[i] = palette[buffer[0][i]];
      }
      else
      {
        for (int i = 0; i < _width; i++)
          lineBuf[i] = clr(buffer[0][i], buffer[0][i], buffer[0][i]);
      }
    }
    else
    {
      uint8* ptr = buffer[0];
      for (int i = 0; i < _width; i++, ptr += cinfo.num_components)
      {
        if (cinfo.num_components > 3)
          lineBuf[i] = clr(ptr[0], ptr[1], ptr[2], ptr[3]);
        else
          lineBuf[i] = clr(ptr[0], ptr[1], ptr[2]);
      }
    }
    y++;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  delete[] data;
  return true;
}
