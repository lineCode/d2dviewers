// h264dec.cpp
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#include "read_config.h"
#include "typedefs.h"
#include "measure_time.h"
#include "d3d9_utils.h"

using namespace std;

//{{{
void H264DecodeInstance (ISVCDecoder* pDecoder, const char* kpH264FileName, int32_t& iWidth, int32_t& iHeight) {

  // open file and find size
  FILE* pH264File = fopen (kpH264FileName, "rb");
  fseek (pH264File, 0L, SEEK_END);
  int32_t iFileSize = (int32_t) ftell (pH264File);
  fseek (pH264File, 0L, SEEK_SET);

  // read file and append slice startCode
  uint8_t* pBuf = new uint8_t[iFileSize + 4];
  fread (pBuf, 1, iFileSize, pH264File);
  fclose (pH264File);
  uint8_t uiStartCode[4] = {0, 0, 0, 1};
  memcpy (pBuf + iFileSize, &uiStartCode[0], 4);

  // set conceal option
  int32_t iErrorConMethod = (int32_t) ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
  pDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);

  CUtils utils;
  unsigned long long uiTimeStamp = 0;
  int32_t iBufPos = 0;
  while (true) {
    //{{{  read frame
    int32_t iEndOfStreamFlag = 0;
    if (iBufPos >= iFileSize) {
      //{{{  end of stream
      iEndOfStreamFlag = true;
      pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
      break;
      }
      //}}}

    // find sliceSize by looking for next slice startCode
    int32_t iSliceSize;
    for (iSliceSize = 1; iSliceSize < iFileSize; iSliceSize++)
      if ((!pBuf[iBufPos+iSliceSize] && !pBuf[iBufPos+iSliceSize+1])  &&
          ((!pBuf[iBufPos+iSliceSize+2] && pBuf[iBufPos+iSliceSize+3] == 1) || (pBuf[iBufPos+iSliceSize+2] == 1)))
        break;

    SBufferInfo sDstBufInfo;
    memset (&sDstBufInfo, 0, sizeof (SBufferInfo));
    sDstBufInfo.uiInBsTimeStamp = uiTimeStamp++;

    uint8_t* pData[3]; // yuv ptrs
    //pDecoder->DecodeFrame2 (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);
    pDecoder->DecodeFrameNoDelay (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);

    if (sDstBufInfo.iBufferStatus == 1) {
      utils.Process ((void**)pData, &sDstBufInfo, NULL);
      iWidth = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
      iHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
      }

    iBufPos += iSliceSize;
    }
    //}}}

  if (pBuf) {
    delete[] pBuf;
    pBuf = NULL;
    }
  }
//}}}

//{{{
int32_t main (int32_t iArgC, char* pArgV[]) {

  ISVCDecoder* pDecoder = NULL;
  WelsCreateDecoder (&pDecoder);

  int iLevelSetting = (int)WELS_LOG_WARNING;
  pDecoder->SetOption (DECODER_OPTION_TRACE_LEVEL, &iLevelSetting);

  SDecodingParam sDecParam = {0};
  sDecParam.sVideoProperty.size = sizeof (sDecParam.sVideoProperty);
  sDecParam.uiTargetDqLayer = (uint8_t) - 1;
  sDecParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
  sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
  pDecoder->Initialize (&sDecParam);

  int32_t iWidth = 0;
  int32_t iHeight = 0;
  H264DecodeInstance (pDecoder, pArgV[1], iWidth, iHeight);

  if (sDecParam.pFileNameRestructed != NULL) {
    delete []sDecParam.pFileNameRestructed;
    sDecParam.pFileNameRestructed = NULL;
    }

  if (pDecoder) {
    pDecoder->Uninitialize();
    WelsDestroyDecoder (pDecoder);
    }

  return 0;
  }
//}}}
