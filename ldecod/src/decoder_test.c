#define _CRT_SECURE_NO_WARNINGS
/*{{{  includes*/
#include <sys/stat.h>
#include "win32.h"
#include "h264decoder.h"
#include "configfile.h"

#include "decoder_test.h"
/*}}}*/
#define DECOUTPUT_TEST    0
#define PRINT_OUTPUT_POC  0
/*{{{*/
static int WriteOneFrame (DecodedPicList* pDecPic, int hFileOutput0, int hFileOutput1, int bOutputAllFrames) {

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

  int argc = 0;
  char argv = "";
  
  int iRet;
  DecodedPicList* pDecPicList;
  int hFileDecOutput0=-1, hFileDecOutput1=-1;
  int iFramesOutput=0, iFramesDecoded=0;
  InputParameters InputParams;

  init_time();

  memset (&InputParams, 0, sizeof(InputParameters));
  strcpy (InputParams.outfile, ""); //! set default output file name
  strcpy (InputParams.reffile, ""); //! set default reference file name
  ParseCommand (&InputParams, argc, &argv);

  strcpy (InputParams.infile, "C:/Users/colin/Desktop/d2dviewers/test.h264"); //! set default bitstream name
  fprintf(stdout,"JM %s %s\n", VERSION, EXT_VERSION);

  //open decoder;
  iRet = OpenDecoder (&InputParams);
  if (iRet != DEC_OPEN_NOERR) {
    fprintf (stderr, "Open encoder failed: 0x%x!\n", iRet);
    return; //failed;
    }

  //decoding;
  for (int i = 0; i < 100; i++) {
    iRet = DecodeOneFrame (&pDecPicList);
    if (iRet == DEC_EOS || iRet == DEC_SUCCEED) {
      // process the decoded picture, output or display;
      iFramesOutput += WriteOneFrame (pDecPicList, hFileDecOutput0, hFileDecOutput1, 0);
      iFramesDecoded++;
      }
    };
  iRet = FinitDecoder(&pDecPicList);

  iFramesOutput += WriteOneFrame (pDecPicList, hFileDecOutput0, hFileDecOutput1 , 1);
  iRet = CloseDecoder();

  }
