#ifndef WELS_D3D9_UTILS_H__
#define WELS_D3D9_UTILS_H__

#include <stdio.h>
#include "codec_def.h"

#include <d3d9.h>

class CD3D9ExUtils {
 public:
  CD3D9ExUtils();
  ~CD3D9ExUtils();

 public:
  HRESULT Init (BOOL bWindowed);
  HRESULT Uninit (void);
  HRESULT Process (void* dst[3], SBufferInfo* Info, FILE* fp = NULL);

 private:
  HRESULT InitResource (void* pSharedHandle, SBufferInfo* Info);
  HRESULT Render (void* pDst[3], SBufferInfo* Info);

 private:
  HMODULE               m_hDll;
  HWND                  m_hWnd;
  unsigned char*        m_pDumpYUV;
  BOOL                  m_bInitDone;
  int                   m_nWidth;
  int                   m_nHeight;

  LPDIRECT3D9EX         m_lpD3D9;
  LPDIRECT3DDEVICE9EX   m_lpD3D9Device;

  D3DPRESENT_PARAMETERS m_d3dpp;
  LPDIRECT3DSURFACE9    m_lpD3D9RawSurfaceShare;
};

class CUtils {
 public:
  CUtils();
  ~CUtils();

  int Process (void* dst[3], SBufferInfo* Info, FILE* fp);

 private:
  void* hHandle;
};

#endif//WELS_D3D9_UTILS_H__

