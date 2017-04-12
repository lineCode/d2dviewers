// cGlWebWindow.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wrl.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#define _USE_MATH_DEFINES
#include <locale>
#include <codecvt>

#include <vector>
#include <thread>

#include "Shlwapi.h" // for shell path functions
#pragma comment(lib,"shlwapi.lib")

#include "../../shared/nanoVg/cGlWindow.h"
#include "../../shared/fonts/FreeSansBold.h"
#include "../../shared/nanoVg/cPerfGraph.h"
#include "../../shared/nanoVg/cGpuGraph.h"

#define USE_NANOVG
#include "../../shared/utils.h"
#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/net/cWinEsp8266Http.h"

#include "../../shared/widgets/cRootContainer.h"
#include "../../shared/widgets/cDecodePicWidget.h"
#include "../../shared/widgets/cListWidget.h"

#include "../../shared/rapidjson/document.h"
//}}}
//{{{  debugHeap
#define MAX_HEAP_DEBUG 2000

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
      printf ("debugMAlloc::not enough heapDebugAllocs\n");
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
        printf ("debugFree::ptr not found %llx\n", (int64_t)ptr);
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
//#define ESP8266

class cGlWebWindow : public cGlWindow, public iDraw  {
public:
  //{{{
  void run (std::string title, int width, int height, std::string fileName) {

    create (title, width, height);
    createFontMem ("sans", (unsigned char*)freeSansBold, sizeof(freeSansBold), 0);
    fontFace ("sans");

    mRoot = new cRootContainer (width, height);
    mRoot->addTopLeft (mPicWidget = new cDecodePicWidget());

    if (fileName.empty()) {
      mRoot->addTopLeft (new cListWidget (mFileList, mFileIndex, mFileIndexChanged, 0, 0));
      std::thread ([=]() { sharesThread(); } ).detach();
      }
      //metObs();
    else if (GetFileAttributesA (fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
      listDirectory (std::string(), fileName, "*.png;*.gif;*.jpg;*.bmp");
      mPicWidget->setFileName (mFileList[0]);
      }
    else
      mPicWidget->setFileName (fileName);

    // init timers
    glfwSetTime (0);
    float cpuTime = 0.0f;
    float frameTime = 0.0f;
    double prevt = glfwGetTime();

    mFpsGraph = new cPerfGraph (cPerfGraph::GRAPH_RENDER_FPS, "frame");
    mCpuGraph = new cPerfGraph (cPerfGraph::GRAPH_RENDER_MS, "cpu");
    mGpuGraph = new cGpuGraph (cPerfGraph::GRAPH_RENDER_MS, "gpu");

    glClearColor (0, 0, 0, 1.0f);
    while (!glfwWindowShouldClose (mWindow)) {
      mGpuGraph->start();
      mCpuGraph->start ((float)glfwGetTime());

      // Update and render
      int winWidth, winHeight;
      glfwGetWindowSize (mWindow, &winWidth, &winHeight);
      int frameBufferWidth, frameBufferHeight;
      glfwGetFramebufferSize (mWindow, &frameBufferWidth, &frameBufferHeight);

      glViewport (0, 0, frameBufferWidth, frameBufferHeight);
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

      beginFrame (winWidth, winHeight, (float)frameBufferWidth / (float)winWidth);
      if (mRoot)
        mRoot->render (this);
      if (mDrawTests) {
        //{{{  draw
        drawEyes (winWidth*3.0f/4.0f, winHeight/2.0f, winWidth/4.0f, winHeight/2.0f,
                  0, 0, (float)glfwGetTime());
        drawLines (0.0f, 50.0f, (float)winWidth, (float)winHeight, (float)glfwGetTime());
        drawSpinner (winWidth/2.0f, winHeight/2.0f, 20.0f, (float)glfwGetTime());
        }
        //}}}
      if (mDrawStats)
        drawStats ((float)winWidth, (float)winHeight, getFrameStats() + (mVsync ? " vsync" : " free"));
      if (mDrawPerf) {
        //{{{  render perf stats
        mFpsGraph->render (this, 0.0f, winHeight-35.0f, winWidth/3.0f -2.0f, 35.0f);
        mCpuGraph->render (this, winWidth/3.0f, winHeight-35.0f, winWidth/3.0f - 2.0f, 35.0f);
        mGpuGraph->render (this, winWidth*2.0f/3.0f, winHeight-35.0f, winWidth/3.0f, 35.0f);
        }
        //}}}
      endFrame();
      glfwSwapBuffers (mWindow);

      mCpuGraph->updateTime ((float)glfwGetTime());
      mFpsGraph->updateTime ((float)glfwGetTime());

      float gpuTimes[3];
      int n = mGpuGraph->stop (gpuTimes, 3);
      for (int i = 0; i < n; i++)
        mGpuGraph->updateValue (gpuTimes[i]);

      glfwPollEvents();
      }

    };
  //}}}
  //{{{  iDraw
  uint16_t getLcdWidthPix() { return mRoot->getPixWidth(); }
  uint16_t getLcdHeightPix() { return mRoot->getPixHeight(); }

  cVg* getContext() { return this; }

  //{{{
  virtual void pixel (uint32_t colour, int16_t x, int16_t y) {
    rectClipped (colour, x, y, 1, 1);
    }
  //}}}
  //{{{
  virtual void drawRect (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    beginPath();
    rect (x, y, width, height);
    fillColor (nvgRGBA ((colour & 0xFF0000) >> 16, (colour & 0xFF00) >> 8, colour & 0xFF,255));
    triangleFill();
    }
  //}}}
  //{{{
  virtual void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    //rect (0xC0000000 | (colour & 0xFFFFFF), x,y, width, height);
    }
  //}}}
  //{{{
  virtual int drawText (uint32_t colour, uint16_t fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

    fontSize ((float)fontHeight);
    textAlign (cVg::ALIGN_LEFT | cVg::ALIGN_TOP);
    fillColor (nvgRGBA ((colour & 0xFF0000) >> 16, (colour & 0xFF00) >> 8, colour & 0xFF,255));
    text ((float)x+3, (float)y+1, str);
    //return (int)metrics.width;
    return 0;
    }
  //}}}
  //{{{
  virtual void ellipseSolid (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {
    beginPath();
    ellipse (x, y, xradius, yradius);
    fillColor (nvgRGBA ((colour & 0xFF0000) >> 16, (colour & 0xFF00) >> 8, colour & 0xFF,255));
    fill();
    }
  //}}}
  //}}}

  const std::vector<std::string> kShares =
    { "VOD", "SSE", "BARC", "CPG", "AV.", "NG.", "SBRY","MRW","HSBA","CNA","GSK","RBS","RMG" };
  const std::vector<std::string> kItems = { "t", "e", "l_fix", "ltt" };

protected:
  //{{{
  void onKey (int key, int scancode, int action, int mods) {

    //mods == GLFW_MOD_SHIFT
    //mods == GLFW_MOD_CONTROL

    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {
      switch (key) {
        case GLFW_KEY_ESCAPE : glfwSetWindowShouldClose (mWindow, GL_TRUE); break;
        default: debug ("key " + hex(key));
        }

      //case 0x00:
      //case 0x10: // shift
      //case 0x11: // control
        //return false;
      //case 0x1B: // escape
        //return true;

      //case 0x25:
        //if (mFileIndex > 0) mFileIndex--;
        //mPicWidget->setFileName (mFileList[mFileIndex]);
        //break; // left arrow

      //case 0x27:
        //if (mFileIndex < mFileList.size()-1) mFileIndex++;
        //mPicWidget->setFileName (mFileList[mFileIndex]);
        //break; // right arrow
      }
    }
  //}}}
  //{{{
  void onChar (char ch, int mods) {
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y, bool controlled) {
    mRoot->press (0, x, y, 0,  0, 0, controlled);
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y, bool controlled) {
    mRoot->release();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y, bool controlled) {
    mMouseX = (float)x;
    mMouseY = (float)y;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc, bool controlled) {
    mMouseX = (float)x;
    mMouseY = (float)y;
    mRoot->press (1, x, y, 0, xInc, yInc, controlled);
    }
  //}}}
  //{{{
  void onMouseWheel (int delta, bool controlled) {
    }
  //}}}

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
        else if (!PathMatchSpecExA (findFileData.cFileName, pathMatchName, PMSF_MULTIPLE))
          mFileList.push_back (mFullDirName + "/" + findFileData.cFileName);
        } while (FindNextFileA (file, &findFileData));

      FindClose (file);
      }
    }
  //}}}
  //{{{
  void metObs() {

  #ifdef ESP8266
    cWinEsp8266Http http;
  #else
    cWinSockHttp http;
  #endif
    http.initialise();

    // get json capabilities
    std::string key = "key=bb26678b-81e2-497b-be31-f8d136a300c6";
    http.get ("datapoint.metoffice.gov.uk", "public/data/layer/wxobs/all/json/capabilities?" + key);

    // save them
    FILE* file = fopen ("c:/metOffice/wxobs/capabilities.json", "wb");
    fwrite (http.getContent(), 1, (unsigned long)http.getContentSize(), file);
    fclose (file);

    if (http.getContent()) {
      //{{{  parse json capabilities
      rapidjson::Document layers;
      if (layers.Parse ((const char*)http.getContent(), http.getContentSize()).HasParseError()) {
        debug ("layers load error " + dec(layers.GetParseError()));
        return;
        }
      http.freeContent();
      //}}}

      for (auto& item : layers["Layers"]["Layer"].GetArray()) {
        auto layer = item.GetObject();
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

            struct stat st;
            if (stat (fileName.c_str(), &st)) {
              //{{{  no file, load from net
              http.get ("datapoint.metoffice.gov.uk", netPath);

              FILE* file = fopen (fileName.c_str(), "wb");
              fwrite (http.getContent(), 1, (unsigned long)http.getContentSize(), file);
              fclose (file);

              http.freeContent();
              }
              //}}}

            mFileList.push_back (fileName);
            mPicWidget->setFileName (fileName);
            }
          }
        }
      }

    http.freeContent();
    }
  //}}}

  //{{{
  std::string share (cHttp& http, std::string symbol, bool full = false) {

    http.get("finance.google.com", "finance/info?client=ig&q="+symbol);

    rapidjson::Document prices;
    auto parseError = prices.Parse ((const char*)(http.getContent()+6), http.getContentSize()-8).HasParseError();
    http.freeContent();

    if (parseError) {
      debug ("prices load error " + dec(prices.GetParseError()));
      return "";
      }

    std::string str;
    for (auto item : kItems) {
      str += prices[item.c_str()].GetString();
      str += " ";
      }

    //printf ("id      :%s\n", prices["id"].GetString());
    //printf ("t       :%s\n", prices["t"].GetString());
    //printf ("e       :%s\n", prices["e"].GetString());
    //printf ("l       :%s\n", prices["l"].GetString());
    //printf ("lfix    :%s\n", prices["l_fix"].GetString());
    //printf ("lcur    :%s\n", prices["l_cur"].GetString());
    //printf ("ltt     :%s\n", prices["ltt"].GetString());
    //printf ("lt      :%s\n", prices["lt"].GetString());
    //printf ("lt_dts  :%s\n", prices["lt_dts"].GetString());
    //printf ("c       :%s\n", prices["c"].GetString());
    //printf ("c_fix   :%s\n", prices["c_fix"].GetString());
    //printf ("cp      :%s\n", prices["cp"].GetString());
    //printf ("cp_fix  :%s\n", prices["cp_fix"].GetString());
    //printf ("ccol    :%s\n", prices["ccol"].GetString());
    //printf ("pcls_fix:%s\n", prices["pcls_fix"].GetString());

    return str;
    }
  //}}}
  //{{{
  void sharesThread() {

  #ifdef ESP8266
    cWinEsp8266Http http;
  #else
    cWinSockHttp http;
  #endif
    http.initialise();

    for (auto ticker : kShares)
      mFileList.push_back (ticker);

    while (true) {
      mFileIndex = 0;
      for (auto ticker : kShares)
        mFileList[mFileIndex++] = share (http, ticker);
      Sleep (5000);
      }
    }
  //}}}

  //{{{  vars
  bool mDrawPerf = false;
  bool mDrawStats = true;
  bool mDrawTests = true;

  cPerfGraph* mFpsGraph = nullptr;
  cPerfGraph* mCpuGraph = nullptr;
  cGpuGraph* mGpuGraph = nullptr;

  cRootContainer* mRoot = nullptr;

  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  cPicWidget* mPicWidget;
  int mFileIndex = 0;
  bool mFileIndexChanged = false;

  std::vector <std::string> mFileList;

  float mMouseX = 0;
  float mMouseY = 0;
  //}}}
  };

int main (int argc, char* argv[]) {

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    debug ("WSAStartup failed");
    exit (0);
    }
    //}}}

  cGlWebWindow webWindow;
  webWindow.run ("webWindow", 800, 800, argv[1] ? std::string(argv[1]) : std::string());
  }
