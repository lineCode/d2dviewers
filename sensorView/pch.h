#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wrl.h>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>

// direct2d
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <DXGI1_2.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <Wincodec.h>

#include "../common/jpegHeader.h"
#include "../common/usbUtils.h"
#include "../inc/CyAPI.h"

using namespace Microsoft::WRL;
using namespace D2D1;
