#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct extDecodedpic_t {
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
  struct extDecodedpic_t *pNext;
  } extDecodedPicList;

  extern void openDecode (char* filename);
  extern void* decodeFrame();
  extern void* closeDecode();

#ifdef __cplusplus
}
#endif /* __cplusplus */
