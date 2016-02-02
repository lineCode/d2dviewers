#define _CRT_SECURE_NO_WARNINGS
/*{{{  includes*/
#include "win32.h"
#include "h264decoder.h"

#include "decoder_test.h"
/*}}}*/
/*{{{*/
static int WriteOneFrame (DecodedPicList* pDecPic) {

  DecodedPicList* pPic = pDecPic;
  int i, iWidth, iHeight, iStride, iWidthUV, iHeightUV, iStrideUV;
  byte* pbBuf;

  iWidth = pPic->iWidth*((pPic->iBitDepth+7)>>3);
  iHeight = pPic->iHeight;
  iStride = pPic->iYBufStride;

  if(pPic->iYUVFormat != YUV444)
    iWidthUV = pPic->iWidth>>1;
  else
    iWidthUV = pPic->iWidth;

  if(pPic->iYUVFormat == YUV420)
    iHeightUV = pPic->iHeight>>1;
  else
    iHeightUV = pPic->iHeight;
  iWidthUV *= ((pPic->iBitDepth+7)>>3);

  iStrideUV = pPic->iUVBufStride;

  printf ("WriteOneFrame %d %d %d\n",  iWidth, iHeight, iStride);
  pbBuf = pPic->pY;
  for (int j = 0; j < iHeight; j += 4) {
    for (i = 0; i < iWidth; i += 4) {
      printf("%1x", (*(pbBuf + (j*iStride) + i))>>4);
      }
    printf ("\n");
    }

  return 1;
  }
/*}}}*/

void decodeMain() {

  init_time();

  InputParameters InputParams;
  memset (&InputParams, 0, sizeof(InputParameters));
  strcpy (InputParams.infile, "C:/Users/colin/Desktop/h264avc/test.h264"); // set default bitstream name
  strcpy (InputParams.outfile, "nnn.yuv");  // set default output file name
  InputParams.write_uv = 1;                 // Write 4:2:0 chroma components for monochrome streams
  InputParams.FileFormat = 0;               // NAL mode (0=Annex B, 1: RTP packets)
  InputParams.ref_offset = 0;               // SNR computation offset
  InputParams.poc_scale = 2;                // Poc Scale (1 or 2)
  InputParams.bDisplayDecParams = 0;        // 1: Display parameters;
  InputParams.conceal_mode = 0;             // Err Concealment(0:Off,1:Frame Copy,2:Motion Copy)
  InputParams.ref_poc_gap = 2;              // Reference POC gap (2: IPP (Default), 4: IbP / IpP)
  InputParams.poc_gap = 2;                  // POC gap (2: IPP /IbP/IpP (Default), 4: IPP with frame skip = 1 etc.)
  InputParams.silent = 0;                   // Silent decode
  InputParams.intra_profile_deblocking = 1; // Enable Deblocking filter in intra only profiles (0=disable, 1=filter according to SPS parameters)
  InputParams.iDecFrmNum = 100;             // Number of frames to be decoded (-n)
  InputParams.DecodeAllLayers = 0;          // Decode all views (-mpr)

  printf ("JM %s %s\n", VERSION, EXT_VERSION);

  if (OpenDecoder (&InputParams) != DEC_OPEN_NOERR) {
    printf ("Open encoder failed\n");
    return;
    }

  // decoding;
  DecodedPicList* pDecPicList;
  for (int i = 0; i < 100; i++) {
    int iRet = DecodeOneFrame (&pDecPicList);
    if (iRet == DEC_EOS || iRet == DEC_SUCCEED)
      WriteOneFrame (pDecPicList);
    };
  FinitDecoder (&pDecPicList);

  WriteOneFrame (pDecPicList);
  CloseDecoder();
  }
