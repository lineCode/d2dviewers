#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <tchar.h>
#include <io.h>

#include <thread>
#include "Shlwapi.h" // for shell path functions
#pragma comment(lib,"shlwapi.lib")

// direct2d
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <DXGI1_2.h>
#include <d2d1helper.h>
#include <dwrite.h>

using namespace Microsoft::WRL;
using namespace D2D1;
