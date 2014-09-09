#include "d2data.h"
#include "d2struct.h"
#include "d2mule.h"

static struct D2Storage
{
  int x0, y0;
  int width, height;
} storage[2] = {
  {154, 143,  6,  8},
  {419, 316, 10,  4},
};

Image* D2Data::renderItem(D2Item* item)
{
  File* invfile = loadImageByName(item->image);
  if (!invfile) return NULL;
  DC6_FRAME frm;
  invfile->read(&frm, sizeof frm);
  int itemWidth = (frm.width + 28) / 29;
  int itemHeight = (frm.height + 28) / 29;
  if (item->base)
  {
    itemWidth = item->base->invwidth;
    itemHeight = item->base->invheight;
  }

  LocalPtr<D2Image> image = new D2Image(*invBackground);
  D2Font* font = getFont();

  String desc = item->description;
  int gidSep = desc.lastIndexOf('$');
  if (gidSep >= 0) desc.resize(gidSep);
  Array<String> lines;
  desc.split(lines, "\\n");
  if (!lines.length())
    return NULL;
  int ilvlSep = lines[0].lastIndexOf('(');
  if (ilvlSep >= 0)
  {
    lines[0].resize(ilvlSep);
    lines[0].trim();
  }
  int boxWidth = 0;
  for (int i = 0; i < lines.length(); i++)
  {
    int width = font->width(lines[i], lines[i].length());
    boxWidth = max(boxWidth, width);
  }
  boxWidth += 8;
  int boxHeight = 16 * lines.length();

  int storType, storX, storY;
  int storRoom[2][16][16];
  memset(storRoom, 0, sizeof storRoom);

  int validT[128];
  int validX[128];
  int validY[128];
  int validNum = 0;
  for (int t = 0; t < 2; t++)
  {
    for (int x = 0; x + itemWidth <= storage[t].width; x++)
    {
      for (int y = 0; y + itemHeight <= storage[t].height; y++)
      {
        if (storage[t].y0 + y * 29 - boxHeight + 1 >= 0 ||
            storage[t].y0 + (y + itemHeight) * 29 + boxHeight <= invBackground->height())
        {
          validT[validNum] = t;
          validX[validNum] = x;
          validY[validNum] = y;
          validNum++;
        }
      }
    }
  }

  if (validNum)
  {
    validNum = (rand() % validNum);
    storType = validT[validNum];
    storX = validX[validNum];
    storY = validY[validNum];
  }
  else
  {
    storType = 0;
    storY = 0;
    storX = (rand() % (storage[0].width - itemWidth + 1));
  }
  int itemX = storage[storType].x0 + storX * 29;
  int itemY = storage[storType].y0 + storY * 29;

  image->fill(itemX, itemY - 1, itemX + itemWidth * 29, itemY - 1 + itemHeight * 29, 118, blendTable[2]);
  uint8* tint = NULL;
  if (item->base && item->invColor >= 0)
    tint = tints[item->base->invtrans][item->invColor];
  drawDC6(image, itemX, itemY, &frm, invfile, tint, item->flags & D2Item::fEthereal ? blendTable[1] : NULL);
  for (int x = 0; x < itemWidth; x++) for (int y = 0; y < itemHeight; y++)
    storRoom[storType][storX + x][storY + y] = 1;

  if (item->parent)
  {
    for (int curi = 0; curi < item->parent->items.length(); curi++)
    {
      D2Item* curitem = item->parent->items[curi];
      if (curitem == item) continue;
      File* curfile = loadImageByName(curitem->image);
      if (!curfile) continue;
      DC6_FRAME curfrm;
      curfile->read(&curfrm, sizeof curfrm);
      int curWidth = (curfrm.width + 28) / 29;
      int curHeight = (curfrm.height + 28) / 29;
      int curT = -1, curX, curY;
      for (int t = 0; t < 2 && curT < 0; t++)
      {
        for (int x = 0; x + curWidth <= storage[t].width && curT < 0; x++)
        {
          for (int y = 0; y + curHeight <= storage[t].height; y++)
          {
            int valid = 1;
            for (int cx = 0; cx < curWidth && valid; cx++) for (int cy = 0; cy < curHeight; cy++)
            {
              if (storRoom[t][x + cx][y + cy])
              {
                valid = 0;
                break;
              }
            }
            if (valid)
            {
              curT = t;
              curX = x;
              curY = y;
              break;
            }
          }
        }
      }
      if (curT >= 0)
      {
        for (int x = 0; x < curWidth; x++) for (int y = 0; y < curHeight; y++)
          storRoom[curT][curX + x][curY + y] = 1;
        int scrX = storage[curT].x0 + curX * 29;
        int scrY = storage[curT].y0 + curY * 29;
        if (curitem->description.find("\\n\\xffc1") >= 0 || !strncmp(curitem->description, "\\xffc1", 6))
          image->fill(scrX, scrY - 1, scrX + curWidth * 29, scrY - 1 + curHeight * 29, 8, blendTable[2]);
        else
          image->fill(scrX, scrY - 1, scrX + curWidth * 29, scrY - 1 + curHeight * 29, 234, blendTable[2]);
        uint8* tint = NULL;
        if (curitem->base && curitem->invColor >= 0)
          tint = tints[curitem->base->invtrans][curitem->invColor];
        drawDC6(image, scrX, scrY, &curfrm, curfile, tint, curitem->flags & D2Item::fEthereal ? blendTable[1] : NULL);
      }
    }
  }

  if (item->sockets.length())
  {
    File* soxImage = loadImage("data\\global\\ui\\PANEL\\gemsocket.dc6");
    int sox = item->sockets.length();
    if (sox > 6) sox = 6;
    D2Point const* pos = getSocketPos(itemWidth, itemHeight, sox);
    for (int i = 0; i < sox; i++)
    {
      soxImage->seek(0, SEEK_SET);
      drawDC6(image, itemX + pos[i].x - 1, itemY + pos[i].y + 1, NULL, soxImage, NULL, blendTable[2]);
      if (item->sockets[i] != "gemsocket")
      {
        File* gemfile = loadImageByName(item->sockets[i]);
        if (gemfile) drawDC6(image, itemX + pos[i].x, itemY + pos[i].y, NULL, gemfile);
      }
    }
  }

  int boxX = itemX + (itemWidth * 29 - boxWidth) / 2;
  if (boxX < 0) boxX = 0;
  if (boxX + boxWidth > image->width()) boxX = image->width() - boxWidth;
  int boxY = itemY - boxHeight + 1;
  if (boxY < 0)
  {
    boxY = itemY + itemHeight * 29;
    if (boxY + boxHeight > image->height())
      boxY = image->height() - boxHeight;
  }
  image->fill(boxX, boxY, boxX + boxWidth, boxY + boxHeight, 0, blendTable[0]);
  for (int i = 0; i < lines.length(); i++)
  {
    int width = font->width(lines[i], lines[i].length());
    int lineX = boxX + (boxWidth - width) / 2;
    int lineY = boxY + i * 16 + 8;
    drawText(image, lineX, lineY, lines[i], lines[i].length(), font);
  }

  int cursorX = itemX + itemWidth * 29 - 2 - (rand() % 10);
  int cursorY = itemY + 4 + (rand() % (itemHeight * 29 - 8));
  drawFrames(image, "data\\global\\ui\\CURSOR\\protate.dc6", rand() % 8, 1, &cursorX, &cursorY);

  int resLeft = min(itemX, boxX) - 10;
  int resTop = min(itemY, boxY) - 10;
  int resRight = max(itemX + itemWidth * 29, boxX + boxWidth) + 10;
  int resBottom = max(itemY + itemHeight * 29, boxY + boxHeight) + 10;

  return image->image(resLeft, resTop, resRight, resBottom, palette);
}
