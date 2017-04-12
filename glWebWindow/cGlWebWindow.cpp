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

#include "../../shared/utils.h"
#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/net/cWinEsp8266Http.h"
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

class cGlWebWindow : public cGlWindow  {
public:
  //{{{
  void run (std::string title, int width, int height, std::string fileName) {

    cGlWindow::initialise (title, width, height, (unsigned char*)freeSansBold);

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

    cGlWindow::run();
    };
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
        case GLFW_KEY_V : toggleVsync(); break;
        case GLFW_KEY_P : togglePerf(); break;
        case GLFW_KEY_S : toggleStats(); break;
        case GLFW_KEY_T : toggleTests(); break;

        case GLFW_KEY_I : toggleSolid(); break;
        case GLFW_KEY_A : toggleEdges(); break;
        case GLFW_KEY_Q : fringeWidth (getFringeWidth() - 0.25f); break;
        case GLFW_KEY_W : fringeWidth (getFringeWidth() + 0.25f); break;
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
  std::wstring_convert <std::codecvt_utf8_utf16 <wchar_t> > converter;

  cPicWidget* mPicWidget;
  int mFileIndex = 0;
  bool mFileIndexChanged = false;

  std::vector <std::string> mFileList;
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
