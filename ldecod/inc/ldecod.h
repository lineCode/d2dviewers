// ldecode.h - minimal interface to ldecod
#pragma once

typedef enum
{
  DEC_GEN_NOERR = 0,
  DEC_OPEN_NOERR = 0,
  DEC_CLOSE_NOERR = 0,
  DEC_SUCCEED = 0,
  DEC_EOS =1,
  DEC_NEED_DATA = 2,
  DEC_INVALID_PARAM = 3,
  DEC_ERRMASK = 0x8000
  //  DEC_ERRMASK = 0x80000000
  } DecErrCode;

typedef struct Decodedpic_t {
  int bValid;                 //0: invalid, 1: valid, 3: valid for 3D output;
  int iViewId;                //-1: single view, >=0 multiview[VIEW1|VIEW0];
  int iPOC;
  int iYUVFormat;             //0: 4:0:0, 1: 4:2:0, 2: 4:2:2, 3: 4:4:4
  int iYUVStorageFormat;      //0: YUV seperate; 1: YUV interleaved; 2: 3D output;
  int iBitDepth;
  byte *pY;                   //if iPictureFormat is 1, [0]: top; [1] bottom;
  byte *pU;
  byte *pV;
  int iWidth;                 //frame width;
  int iHeight;                //frame height;
  int iYBufStride;            //stride of pY[0/1] buffer in bytes;
  int iUVBufStride;           //stride of pU[0/1] and pV[0/1] buffer in bytes;
  int iSkipPicNum;
  int iBufSize;
  struct Decodedpic_t *pNext;
  } DecodedPicList;

extern "C" {
  int OpenDecoder (char* filename);
  int DecodeOneFrame (DecodedPicList** ppDecPic);
  int FinitDecoder (DecodedPicList** ppDecPicList);
  int CloseDecoder();
  }
