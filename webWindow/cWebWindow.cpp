// cMp3Window.cpp
//{{{  includes
#include "pch.h"

#include <vector>

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../../shared/utils.h"
#include "../../shared/net/cHttp.h"

#include "../../shared/widgets/cRootContainer.h"
#include "../../shared/widgets/cPicWidget.h"

#include "../../shared/rapidjson/document.h"
#include "../../shared/decoders/cPng.h"
//}}}
//{{{  heap debug
#define MAX_HEAP_DEBUG 1000
//{{{
class cHeapAlloc {
public:
  void* mPtr = nullptr;
  size_t mSize = 0;
  uint8_t mHeap = 0;
  };
//}}}
cHeapAlloc mHeapDebugAllocs [MAX_HEAP_DEBUG];
const char* kDebugHeapLabels[3] = { "big", "sml", "new" };
size_t mHeapDebugAllocated[3] = { 0,0,0 };
size_t mHeapDebugHighwater[3] = { 0, 0, 0 };
uint32_t mHeapDebugOutAllocs[3] = { 0, 0, 0 };
//{{{
void* debugMalloc (size_t size, const char* tag, uint8_t heap) {

  auto ptr = malloc (size);

  mHeapDebugOutAllocs[heap]++;
  mHeapDebugAllocated[heap] += size;
  for (auto i = 0; i < MAX_HEAP_DEBUG; i++) {
    if (!mHeapDebugAllocs[i].mPtr) {
      mHeapDebugAllocs[i].mPtr = ptr;
      mHeapDebugAllocs[i].mSize = size;
      mHeapDebugAllocs[i].mHeap = heap;
      break;
      }
    if (i >= MAX_HEAP_DEBUG-1)
      printf ("new cockup\n");
    }

  if (mHeapDebugAllocated[heap] > mHeapDebugHighwater[heap]) {
    mHeapDebugHighwater[heap] = mHeapDebugAllocated[heap];
    printf ("%s %3d %7zd %7zd %s\n", kDebugHeapLabels[heap],
            mHeapDebugOutAllocs[heap], mHeapDebugAllocated[heap], size, tag ? tag : "");
    }

  return ptr;
  }
//}}}
//{{{
void debugFree (void* ptr) {

  if (ptr) {
    for (auto i = 0; i < MAX_HEAP_DEBUG; i++) {
      if (mHeapDebugAllocs[i].mPtr && mHeapDebugAllocs[i].mPtr == ptr) {
        mHeapDebugAllocs[i].mPtr = nullptr;
        mHeapDebugOutAllocs[mHeapDebugAllocs[i].mHeap]--;
        mHeapDebugAllocated[mHeapDebugAllocs[i].mHeap] -= mHeapDebugAllocs[i].mSize;
        break;
        }
      if (i >= MAX_HEAP_DEBUG-1)
        printf ("delete cockup\n");
      }
    }

  free (ptr);
  }
//}}}
void* operator new (size_t size) { return debugMalloc (size, "", 2); }
void operator delete (void* ptr) { debugFree (ptr); }
void* operator new[](size_t size) { printf("new[] %d\n", int(size)); return debugMalloc (size, "", '['); }
void operator delete[](void *ptr) { printf ("delete[]\n"); debugFree (ptr); }
//}}}

class cMp3Window : public iDraw, public cD2dWindow {
public:
  //{{{  iDraw
  uint16_t getLcdWidthPix() { return mRoot->getWidthPix(); }
  uint16_t getLcdHeightPix() { return mRoot->getHeightPix(); }

  //{{{
  void pixel (uint32_t colour, int16_t x, int16_t y) {
    rectClipped (colour, x, y, 1, 1);
    }
  //}}}
  //{{{
  void rect (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    mBrush->SetColor (ColorF (colour, ((colour & 0xFF000000) >> 24) / 255.0f));
    getDeviceContext()->FillRectangle (RectF (float(x), float(y), float(x + width), float(y + height)), mBrush);
    }
  //}}}
  //{{{
  void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    rect (0xC0000000 | (colour & 0xFFFFFF), x,y, width, height);
    }
  //}}}
  //{{{
  int text (uint32_t colour, uint16_t fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    // create layout
    std::wstring wstr = converter.from_bytes (str.data());
    IDWriteTextLayout* textLayout;
    getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(),
                                          fontHeight <= cWidget::getFontHeight() ? getTextFormat() : getTextFormatSize (fontHeight),
                                          width, height, &textLayout);

    // draw it
    mBrush->SetColor (ColorF (colour, ((colour & 0xFF000000) >> 24) / 255.0f));
    getDeviceContext()->DrawTextLayout (Point2F(float(x), float(y)), textLayout, mBrush);

    // return measure
    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics (&metrics);
    return (int)metrics.width;
    }
  //}}}

  //{{{
  void copy (uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    if (src && width && height) {
      ID2D1Bitmap* bitmap;
      getDeviceContext()->CreateBitmap (SizeU (width, height), getBitmapProperties(), &bitmap);
      bitmap->CopyFromMemory (&RectU (0, 0, width, height), src, width*4);
      getDeviceContext()->DrawBitmap (bitmap, RectF ((float)x, (float)y, (float)x+width,(float)y+height));
      bitmap->Release();
      }
    }
  //}}}
  void copy (uint8_t* src, int16_t srcx, int16_t srcy, uint16_t srcWidth, int16_t srcHeight,
             int16_t dstx, int16_t dsty, uint16_t dstWidth, uint16_t dstHeight) {};

  //{{{
  ID2D1Bitmap* allocBitmap (uint16_t width, uint16_t height) {

    ID2D1Bitmap* bitmap;
    getDeviceContext()->CreateBitmap (SizeU (width, height), getBitmapProperties(), &bitmap);
    return bitmap;
    }
  //}}}
  //{{{
  void copy (ID2D1Bitmap* bitMap, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    getDeviceContext()->DrawBitmap (bitMap, RectF ((float)x, (float)y, (float)x+width,(float)y+height));
    }
  //}}}
  //}}}
  //{{{
  void run (wchar_t* title, int width, int height, std::string fileName) {

    initialise (title, width+10, height+6);

    getDeviceContext()->CreateSolidColorBrush (ColorF(ColorF::CornflowerBlue), &mBrush);
    setChangeRate (1);

    mRoot = new cRootContainer (width, height);

    if (fileName.empty())
      metApp();
    else
      show (fileName);

    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00:
      case 0x10: // shift
      case 0x11: // control
        return false;
      case 0x1B: // escape
        return true;

      default: debug ("key " + hex(key));
      }
    return false;
    }
  //}}}
  void onMouseDown (bool right, int x, int y) { mRoot->press (0, x, y, 0,  0, 0); }
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) { mRoot->press (1, x, y, 0, xInc, yInc); }
  void onMouseUp (bool right, bool mouseMoved, int x, int y) { mRoot->release(); }
  void onDraw (ID2D1DeviceContext* dc) { mRoot->render (this); }

private:
  //{{{
  void metApp() {

    int picvalue;
    bool picchanged;
    auto picWidget = new cPicWidget (mRoot->getWidth(),mRoot->getHeight(), 1, picvalue, picchanged);
    mRoot->addTopLeft (picWidget);

    std::string key = "key=bb26678b-81e2-497b-be31-f8d136a300c6";
    mHttp.get ("datapoint.metoffice.gov.uk", "public/data/layer/wxobs/all/json/capabilities?" + key);
    if (mHttp.getContent()) {
      rapidjson::Document layers;
      if (layers.Parse ((const char*)mHttp.getContent(), mHttp.getContentSize()).HasParseError()) {
        debug ("layers load error " + dec(layers.GetParseError()));
        return;
        }
      mHttp.freeContent();

      for (auto& element : layers["Layers"]["Layer"].GetArray()) {
        auto layer = element.GetObject();
        std::string displayName = layer["@displayName"].GetString();
        //if (displayName == "Lightning") {
        //if (displayName == "SatelliteIR") {
        //if (displayName == "SatelliteVis") {
        //if (displayName == "Rainfall") {
          std::string layerName = layer["Service"]["LayerName"].GetString();
          std::string imageFormat = layer["Service"]["ImageFormat"].GetString();
          for (auto& timeItem : layer["Service"]["Times"]["Time"].GetArray()) {
            std::string time = timeItem.GetString();
            mHttp.get ("datapoint.metoffice.gov.uk",
                       "public/data/layer/wxobs/" + layerName + "/" + imageFormat + "?TIME=" + time + "Z&" + key);
            printf ("%ds\n", mHttp.getContentSize());
            cPng png (mHttp.getContent(), mHttp.getContentSize());
            if (png.readHeader()) {
              auto picBuf = png.decodeBody();
              if (picBuf) {
                auto rgbaBuf = (uint32_t*)picBuf;
                for (int i = 0; i < png.getWidth() * png.getHeight(); i++)
                  rgbaBuf[i] = (rgbaBuf[i] & 0xFF00FF00) | ((rgbaBuf[i] & 0x000000FF) << 16) | ((rgbaBuf[i] & 0x00FF0000) >> 16);
                picWidget->setPic (picBuf, png.getWidth(), png.getHeight(), 4);
                }
              }
            mHttp.freeContent();
            }
          //}
        }
      }
    }
  //}}}
  //{{{
  void show (std::string fileName) {

    int picvalue;
    bool picchanged;
    auto picWidget = new cPicWidget (mRoot->getWidth(),mRoot->getHeight(), 1, picvalue, picchanged);
    mRoot->addTopLeft (picWidget);

    HANDLE mFileHandle = CreateFileA (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    int mFileSize = (int)GetFileSize (mFileHandle, NULL);

    uint8_t* mBuffer = (uint8_t*)MapViewOfFile (CreateFileMapping (mFileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);

    cPng png (mBuffer, mFileSize);
    if (png.readHeader()) {
      auto picBuf = png.decodeBody();
      printf ("w:%d h:%d dep:%d format:%d comp:%d pixSiz:%d bpp:%d erro:%d\n",
              png.getWidth(), png.getHeight(), png.getBitDepth(),png. getFormat(),
              png.getComponents(), png.getPixelSize(), png.getBpp(), png.getError());

      if (picBuf) {
        auto rgbaBuf = (uint32_t*)picBuf;
        for (int i = 0; i < png.getWidth() * png.getHeight(); i++)
          rgbaBuf[i] = (rgbaBuf[i] & 0xFF00FF00) | ((rgbaBuf[i] & 0x000000FF) << 16) | ((rgbaBuf[i] & 0x00FF0000) >> 16);
        picWidget->setPic (picBuf, png.getWidth(), png.getHeight(), 4);
        }
      }

    UnmapViewOfFile (mBuffer);
    CloseHandle (mFileHandle);
    }
  //}}}
  //{{{  vars
  cRootContainer* mRoot = nullptr;

  // d2dwindow
  ID2D1SolidColorBrush* mBrush;
  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  cHttp mHttp;
  //}}}
  };

//{{{
int main (int argc, char* argv[]) {

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    debug ("WSAStartup failed");
    exit (0);
    }
    //}}}

  startTimer();
  cMp3Window mp3Window;
  //mp3Window.run (L"mp3window", 800, 480, argv[1] ? std::string(argv[1]) : std::string());
  mp3Window.run (L"mp3window", 500+4, 500+34, argv[1] ? std::string(argv[1]) : std::string());
  }
//}}}
