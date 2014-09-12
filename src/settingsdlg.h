#ifndef __SETTINGSDLG__
#define __SETTINGSDLG__

#include "frameui/framewnd.h"
#include "frameui/controlframes.h"
#include "registry.h"

class SettingsWnd : public RootWindow
{
  StaticFrame* addTip(Frame* frame, wchar_t const* text);
  EditFrame* iD2Path;
  EditFrame* iMulePath;
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  SettingsWnd(Window* parent);
  ~SettingsWnd();
};

class AboutDlg : public RootWindow
{
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  AboutDlg(Window* parent);
};

#endif // __SETTINGSDLG__
