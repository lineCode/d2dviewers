// cMp3File.h
#pragma once
//{{{  includes
#include "../common/cAudFrame.h"
#include "../common/cMp3decoder.h"

#include "../common/cJpegImage.h"

#include "concurrent_vector.h"

//#pragma comment (lib,"avutil.lib")
//#pragma comment (lib,"avcodec.lib")
//#pragma comment (lib,"avformat.lib")
//}}}

class cMp3File {
public:
  cMp3File (wchar_t* fileName) : mFileName(fileName), mFullFileName (fileName) {}
  cMp3File (wstring& parentName, wchar_t* fileName) : mFileName(fileName), mFullFileName (parentName + L"\\" + fileName) {}

  //{{{
  int isLoaded() {
    return mLoaded;
    }
  //}}}
  //{{{
  wstring getFileName() {
    return mFileName;
    }
  //}}}
  //{{{
  wstring getFullFileName() {
    return mFullFileName;
    }
  //}}}
  //{{{
  int getSampleRate() {
    return mSampleRate;
    }
  //}}}
  //{{{
  int getChannels() {
    return mChannels;
    }
  //}}}
  //{{{
  int getBitRate() {
    return mBitRate;
    }
  //}}}
  //{{{
  int getMode() {
    return mMode;
    }
  //}}}
  //{{{
  int getTagSize() {
    return mTagSize;
    }
  //}}}
  //{{{
  double getMaxSecs() {
    return mFrames.size() * getSecsPerFrame();
    }
  //}}}
  //{{{
  double getSecsPerFrame() {
    return 1152.0 / mSampleRate;
    }
  //}}}

  //{{{
  cAudFrame* getFrame (int frame) {
    return (frame >= 0) && (frame < mFrames.size()) ? mFrames[frame] : nullptr;
    }
  //}}}
  //{{{
  ID2D1Bitmap* getBitmap() {
    return mBitmap;
    }
  //}}}

  //{{{
  D2D1_RECT_F& getLayout() {
    return mLayout;
    }
  //}}}
  //{{{
  void setLayout (D2D1_RECT_F& layout) {
    mLayout = layout;
    }
  //}}}
  //{{{
  bool pick (D2D1_POINT_2F& point) {

    return (point.x > mLayout.left) && (point.x < mLayout.right) &&
           (point.y > mLayout.top) && (point.y < mLayout.bottom);
    }
  //}}}

  //{{{
  void selected (ID2D1DeviceContext* dc, D2D1_BITMAP_PROPERTIES bitmapProperties) {
    if (mLoaded < 2)
      load (dc, bitmapProperties, true);
    }
  //}}}
  //{{{
  void load (ID2D1DeviceContext* dc, D2D1_BITMAP_PROPERTIES bitmapProperties, bool loadSamples) {

    auto fileHandle = CreateFile (mFullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    auto mFileSize = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (uint8_t*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    if (!mLoaded) {
      mTagSize = id3tag (dc, bitmapProperties, fileBuffer, mFileSize);
      mLoaded = 1;
      }

    auto time = getTimer();
    cMp3Decoder mMp3Decoder;

    auto fileBufferPtr = fileBuffer + mTagSize;
    int bufferBytes = mFileSize - mTagSize;
    while (bufferBytes > 0) {
      cAudFrame* audFrame = new cAudFrame();
      audFrame->set (0, 2, mSampleRate, loadSamples ? 1152 : 0);
      int bytesUsed = mMp3Decoder.decodeNextFrame (fileBufferPtr, bufferBytes,
                                                   &audFrame->mPower[0], loadSamples ? audFrame->mSamples : nullptr);
      if (bytesUsed > 0) {
        fileBufferPtr += bytesUsed;
        bufferBytes -= bytesUsed;
        mSampleRate = mMp3Decoder.getSampleRate();
        mBitRate = mMp3Decoder.getBitRate();
        mChannels = mMp3Decoder.getNumChannels();
        mMode = mMp3Decoder.getMode();
        mFrames.push_back (audFrame);
        }
      else
        break;
      }

    if (loadSamples)
      mLoaded = 2;
    printf ("finished load %f\n", getTimer()- time);

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}
  //{{{
  //void loaderffmpeg (const wchar_t* wFileName) {

    //auto fileHandle = CreateFile (wFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    //int mFileBytes = GetFileSize (fileHandle, NULL);
    //auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    //auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    //av_register_all();
    //AVCodecParserContext* audParser = av_parser_init (AV_CODEC_ID_MP3);
    //AVCodec* audCodec = avcodec_find_decoder (AV_CODEC_ID_MP3);
    //AVCodecContext*audContext = avcodec_alloc_context3 (audCodec);
    //avcodec_open2 (audContext, audCodec, NULL);

    //AVPacket avPacket;
    //av_init_packet (&avPacket);
    //avPacket.data = fileBuffer;
    //avPacket.size = 0;

    //auto time = startTimer();

    //int pesLen = mFileBytes;
    //uint8_t* pesPtr = fileBuffer;
    //int mLoadAudFrame = 0;
    //while (pesLen) {
      //auto lenUsed = av_parser_parse2 (audParser, audContext, &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      //pesPtr += lenUsed;
      //pesLen -= lenUsed;

      //if (avPacket.data) {
        //auto got = 0;
        //auto avFrame = av_frame_alloc();
        //auto bytesUsed = avcodec_decode_audio4 (audContext, avFrame, &got, &avPacket);
        //if (bytesUsed >= 0) {
          //avPacket.data += bytesUsed;
          //avPacket.size -= bytesUsed;
          //if (got) {
            //{{{  got frame
            //mSampleRate = avFrame->sample_rate;
            //mAudFrames[mLoadAudFrame] = new cAudFrame();
            //mAudFrames[mLoadAudFrame]->set (0, avFrame->channels, 48000, avFrame->nb_samples);
            //auto samplePtr = (short*)mAudFrames[mLoadAudFrame]->mSamples;
            //double valueL = 0;
            //double valueR = 0;
            //short* leftPtr = (short*)avFrame->data[0];
            //short* rightPtr = (short*)avFrame->data[1];
            //for (int i = 0; i < avFrame->nb_samples; i++) {
              //*samplePtr = *leftPtr++;
              //valueL += pow (*samplePtr++, 2);
              //*samplePtr = *rightPtr++;
              //valueR += pow (*samplePtr++, 2);
              //}
            //mAudFrames[mLoadAudFrame]->mPower[0] = (float)sqrt (valueL) / (avFrame->nb_samples * 2.0f);
            //mAudFrames[mLoadAudFrame]->mPower[1] = (float)sqrt (valueR) / (avFrame->nb_samples * 2.0f);
            //mLoadAudFrame++;
            //changed();
            //}
            //}}}
          //av_frame_free (&avFrame);
          //}
        //}
      //}

    //UnmapViewOfFile (fileBuffer);
    //CloseHandle (fileHandle);
    //}
  //}}}

private:
  //{{{
  int id3tag (ID2D1DeviceContext* dc, D2D1_BITMAP_PROPERTIES bitmapProperties, uint8_t* buffer, int bufferLen) {
  // check for ID3 tag

    auto ptr = buffer;
    auto tag = ((*ptr)<<24) | (*(ptr+1)<<16) | (*(ptr+2)<<8) | *(ptr+3);

    if (tag == 0x49443303)  {
     // got ID3 tag
      auto tagSize = (*(ptr+6)<<21) | (*(ptr+7)<<14) | (*(ptr+8)<<7) | *(ptr+9);
      printf ("%c%c%c ver:%d %02x flags:%02x tagSize:%d\n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), tagSize);
      ptr += 10;

      while (ptr < buffer + tagSize) {
        auto tag = ((*ptr)<<24) | (*(ptr+1)<<16) | (*(ptr+2)<<8) | *(ptr+3);
        auto frameSize = (*(ptr+4)<<24) | (*(ptr+5)<<16) | (*(ptr+6)<<8) | (*(ptr+7));
        if (!frameSize)
          break;
        auto frameFlags1 = *(ptr+8);
        auto frameFlags2 = *(ptr+9);

        printf ("%c%c%c%c - %02x %02x - frameSize:%d - ", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), frameFlags1, frameFlags2, frameSize);
        for (auto i = 0; i < (tag == 0x41504943 ? 11 : frameSize); i++)
          printf ("%c", *(ptr+10+i));
        printf ("\n");

        for (auto i = 0; i < (frameSize < 32 ? frameSize : 32); i++)
          printf ("%02x ", *(ptr+10+i));
        printf ("\n");

        if (tag == 0x41504943) {
          auto jpegImage = new cJpegImage();
          if (jpegImage->loadBuffer (dc, bitmapProperties, 1, ptr + 10 + 14, frameSize - 14))
            mBitmap = jpegImage->getFullBitmap();
          }
        ptr += frameSize + 10;
        }
      return tagSize + 10;
      }
    else {
      // print start of file
      for (auto i = 0; i < 32; i++)
        printf ("%02x ", *(ptr+i));
      printf ("\n");
      return 0;
      }
    }
  //}}}

  wstring mFileName;
  wstring mFullFileName;

  int mLoaded = 0;
  int mTagSize = 0;
  double mDurationSecs = 0;

  int mSampleRate = 44100;
  int mChannels = 2;
  int mBitRate = 0;
  int mMode = 0;

  D2D1_RECT_F mLayout = {0,0,0,0};

  ID2D1Bitmap* mBitmap = nullptr;
  concurrency::concurrent_vector<cAudFrame*> mFrames;
  };
