// cD2dWindow.h
#pragma once

class cD2dWindow {
public:
  //{{{
  cD2dWindow() : mHWND(0), mChanged(false), mChangeRate(0),
                 keyDown(false), shiftKeyDown(false), controlKeyDown(false),
                 mouseDown(false), rightDown(false), mouseMoved(false),
                 downMousex(0), downMousey(0), lastMousex(0), lastMousey(0), proxMousex(0), proxMousey(0) {}
  //}}}
  virtual ~cD2dWindow() {}

  void initialise (wchar_t* title, int width, int height);

  bool getMouseDown() { return mouseDown; }

  // gets
  D2D1_SIZE_F getClientF() { return clientF; }

  ComPtr<IWICImagingFactory> getWicImagingFactory() { return wicImagingFactory; }
  ID2D1DeviceContext* getDeviceContext() { return mDeviceContext.Get(); }
  IDWriteTextFormat* getTextFormat() { return textFormat; }
  ID2D1SolidColorBrush* getBlueBrush() { return blueBrush; }
  ID2D1SolidColorBrush* getBlackBrush() { return blackBrush; }
  ID2D1SolidColorBrush* getDimGreyBrush() { return dimGreyBrush; }
  ID2D1SolidColorBrush* getGreyBrush() { return greyBrush; }
  ID2D1SolidColorBrush* getWhiteBrush() { return whiteBrush; }
  ID2D1SolidColorBrush* getYellowBrush() { return yellowBrush; }

  void changed() { mChanged = true; }
  void setChangeRate (int changeRate) { mChangeRate = changeRate; }

  LRESULT wndProc (HWND hWnd, unsigned int msg, WPARAM wparam, LPARAM lparam);
  void messagePump();

  // public static var
  static cD2dWindow* mD2dWindow;

protected:
  virtual bool onKey(int key) { return false; }
  virtual void onMouseWheel (int delta) {}
  virtual void onMouseProx (int x, int y) {}
  virtual void onMouseDown (bool right, int x, int y) {};
  virtual void onMouseMove (bool right, int x, int y, int xInc, int yInc) {}
  virtual void onMouseUp (bool right, bool mouseMoved, int x, int y) {}
  virtual void onDraw (ID2D1DeviceContext* dc) = 0;
  //{{{  protected vars
  bool keyDown;
  bool shiftKeyDown;
  bool controlKeyDown;

  bool mouseDown;
  bool rightDown;

  bool mouseMoved;

  int downMousex;
  int downMousey;

  int lastMousex;
  int lastMousey;

  int proxMousex;
  int proxMousey;
  //}}}

private:
  void createDeviceResources();
  void createSizedResources();
  void createDirect2d();

  void onResize();
  void onRender();

  // private vars
  HWND mHWND;
  bool mChanged;
  int mChangeRate;
  //{{{  deviceIndependentResources
  ComPtr<ID2D1Factory1> mD2D1Factory;
  IDWriteFactory* DWriteFactory;
  ComPtr<IWICImagingFactory> wicImagingFactory;
  //}}}
  //{{{  deviceResources
  ComPtr<ID3D11Device1> mD3dDevice1;
  ComPtr<ID3D11DeviceContext1> mD3dContext1;
  ComPtr<IDXGIDevice> mDxgiDevice;
  //}}}
  //{{{  sizedResources
  D2D1_SIZE_U client;
  D2D1_SIZE_F clientF;

  ComPtr<IDXGISwapChain1> mSwapChain;
  ComPtr<ID2D1Bitmap1> mD2dTargetBitmap;

  ComPtr<ID2D1DeviceContext> mDeviceContext;

  IDWriteTextFormat* textFormat;
  ID2D1SolidColorBrush* blueBrush;
  ID2D1SolidColorBrush* blackBrush;
  ID2D1SolidColorBrush* dimGreyBrush;
  ID2D1SolidColorBrush* greyBrush;
  ID2D1SolidColorBrush* whiteBrush;
  ID2D1SolidColorBrush* yellowBrush;
  //}}}
  std::thread mRenderThread;
  };
