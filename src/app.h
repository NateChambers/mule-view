#ifndef __MULEVIEW_APP__
#define __MULEVIEW_APP__

#include <windows.h>
#include "base/wstring.h"
#include "frameui/framewnd.h"

class MainWnd;
class D2Data;
class MuleData;

class Application
{
  static Application* instance;
  HINSTANCE hInstance;
  friend Application* getApp();
  WideString root;

  MainWnd* mainWindow;
  D2Data* d2data;
public:
  Application(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
  ~Application();

  HINSTANCE getInstanceHandle() const
  {
    return hInstance;
  }
  WideString getRootPath() const
  {
    return root;
  }
  HWND getMainWindow() const;

  D2Data* getD2Data() const
  {
    return d2data;
  }

  int run();
};
inline Application* getApp()
{
  return Application::instance;
}
inline HINSTANCE getInstance()
{
  return getApp()->getInstanceHandle();
}

#endif // __MULEVIEW_APP__
