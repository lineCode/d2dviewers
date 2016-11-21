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
#include "../../shared/widgets/cPngWidget.h"
#include "../../shared/widgets/cTextBox.h"

#include "../../shared/rapidjson/document.h"
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

int mIndex = 0;
cPngWidget* mPngWidget;
std::vector<std::string> mFileList;

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
    mPngWidget = new cPngWidget (mRoot->getWidth(), mRoot->getHeight());
    mRoot->addTopLeft (mPngWidget);

    if (fileName.empty())
      metApp();
    else if (GetFileAttributesA (fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
      listDirectory (std::string(), fileName, "*.png");
      mPngWidget->setFileName (mFileList[0]);
      }
    else
      mPngWidget->setFileName(fileName);

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

      case 0x25: if (mIndex > 0) mIndex--;                  mPngWidget->setFileName(mFileList[mIndex]); break; // left arrow
      case 0x27: if (mIndex < mFileList.size()-1) mIndex++; mPngWidget->setFileName(mFileList[mIndex]); break; // right arrow

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
  void listDirectory (std::string parentName, std::string directoryName, char* pathMatchName) {

    std::string mFullDirName = parentName.empty() ? directoryName : parentName + "/" + directoryName;

    std::string searchStr (mFullDirName +  "/*");
    WIN32_FIND_DATAA findFileData;
    auto file = FindFirstFileExA (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                  FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (file != INVALID_HANDLE_VALUE) {
      do {
        if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (findFileData.cFileName[0] != '.'))
          listDirectory (mFullDirName, findFileData.cFileName, pathMatchName);
        else if (PathMatchSpecA (findFileData.cFileName, pathMatchName))
          mFileList.push_back (mFullDirName + "/" + findFileData.cFileName);
        } while (FindNextFileA (file, &findFileData));

      FindClose (file);
      }
    }
  //}}}
  //{{{
  void metApp() {

    // get json capabilities
    std::string key = "key=bb26678b-81e2-497b-be31-f8d136a300c6";
    mHttp.get ("datapoint.metoffice.gov.uk", "public/data/layer/wxobs/all/json/capabilities?" + key);

    // save them
    FILE* file = fopen ("c:/metOffice/wxobs/capabilities.json", "wb");
    fwrite (mHttp.getContent(), 1, (unsigned long)mHttp.getContentSize(), file);
    fclose (file);

    if (mHttp.getContent()) {
      //{{{  parse json capabilities
      rapidjson::Document layers;
      if (layers.Parse ((const char*)mHttp.getContent(), mHttp.getContentSize()).HasParseError()) {
        debug ("layers load error " + dec(layers.GetParseError()));
        return;
        }
      mHttp.freeContent();
      //}}}

      for (auto& element : layers["Layers"]["Layer"].GetArray()) {
        auto layer = element.GetObject();
        std::string displayName = layer["@displayName"].GetString();
        if (displayName == "Rainfall") { //"SatelliteIR" "SatelliteVis" "Rainfall"  "Lightning"
          std::string layerName = layer["Service"]["LayerName"].GetString();
          std::string imageFormat = layer["Service"]["ImageFormat"].GetString();
          for (auto& item : layer["Service"]["Times"]["Time"].GetArray()) {
            std::string time = item.GetString();
            std::string netPath = "public/data/layer/wxobs/" + layerName + "/" + imageFormat + "?TIME=" + time + "Z&" + key;
            for (int i = 0; i < time.size(); i++)
              if (time[i] == 'T')
                time[i] = ' ';
              else if (time[i] == ':')
                time[i] = '-';
            std::string fileName = "c:/metOffice/wxobs/" + layerName + "/" + time + ".png";

            FILE* file = fopen (fileName.c_str(), "rb");
            if (file)
              fclose (file);
            else {
              //{{{  load from net
              mHttp.get ("datapoint.metoffice.gov.uk", netPath);

              printf ("saving %s %d\n", fileName.c_str(), mHttp.getContentSize());

              FILE* file = fopen (fileName.c_str(), "wb");
              fwrite (mHttp.getContent(), 1, (unsigned long)mHttp.getContentSize(), file);
              fclose (file);

              mHttp.freeContent();
              }
              //}}}
            mFileList.push_back (fileName);
            mPngWidget->setFileName (fileName);
            }
          }
        }
      }

    mHttp.freeContent();
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
  mp3Window.run (L"mp3window", 800, 800, argv[1] ? std::string(argv[1]) : std::string());
  }
//}}}
