#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include "../common/date.h"  // doesn't play well with windows min()

#include <windows.h>
#include <wrl.h>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>

#include "Shlwapi.h" // for shell path functions
#pragma comment(lib,"shlwapi.lib")

#include "concurrent_vector.h"

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
using namespace std;
