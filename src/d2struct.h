#ifndef __D2STRUCT__
#define __D2STRUCT__

#pragma pack(push, 1)
struct TBL_HEADER
{
  uint16 crc;
  uint16 numElements;
  uint32 hashSize;
  uint8 version;
  uint32 indexStart;
  uint32 maxTries;
  uint32 indexEnd;
};
struct TBL_HASHTBL
{
  uint8 used;
  uint16 index;
  uint32 hash;
  uint32 keyOffset;
  uint32 strOffset;
  uint16 strLength;
};
struct DC6_HEADER
{
  uint32 version;
  uint32 unknown1;
  uint32 unknown2;
  uint32 termination;
  uint32 directions;
  uint32 frames_per_dir;
};
struct DC6_FRAME
{
  uint32 flip;
  sint32 width;
  sint32 height;
  sint32 offset_x;
  sint32 offset_y;
  uint32 unknown;
  uint32 next_block;
  uint32 length;
};
#pragma pack(pop)

#endif // __D2STRUCT__
