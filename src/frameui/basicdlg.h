#ifndef __BASICDLG__
#define __BASICDLG__

#include "framewnd.h"

#include "frameui/controlframes.h"
#include "registry.h"
#include "base/array.h"

class BasicDialog : public RootWindow
{
  Array<Pair<Window*, WideString*> > strings;
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  BasicDialog(Window* parent, wchar_t const* title, int width, int height);

  StaticFrame* addTip(Frame* frame, wchar_t const* text);
  void bindString(Window* frm, WideString* str)
  {
    strings.push(MakePair(frm, str));
  }
};


#endif // __BASICDLG__
