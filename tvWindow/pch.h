#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <tchar.h>
#include <io.h>

#include <string>
#include <sstream>
#include <iomanip>

#include <map>
#include <thread>

// direct2d
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <DXGI1_2.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <Wincodec.h>

#ifdef __cplusplus
extern "C" {
#endif
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

using namespace Microsoft::WRL;
using namespace D2D1;
using namespace std;
