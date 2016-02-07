//{{{
static DWORD WINAPI hlsLoaderThread (LPVOID arg) {

  curl_global_init (CURL_GLOBAL_ALL);
  CURL* curl = curl_easy_init();
  //{{{  get playlist sequenceNum and tsFile filename root, start at last but one
  char filename [200];
  int sequenceNum = 0;
  int numSeqFiles = 0;
  getPlaylist (curl, filename, &sequenceNum, &numSeqFiles);
  //}}}
  sequenceNum += numSeqFiles - 4;

  //{{{  init aacDecoder
  NeAACDecHandle decoder = NeAACDecOpen();
  NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (decoder);
  config->outputFormat = FAAD_FMT_16BIT;

  NeAACDecSetConfiguration (decoder, config);
  //}}}
  //{{{  init vidCodecContext, vidCodec, vidParser, vidFrame
  av_register_all();

  AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_H264);;
  AVCodecContext* vidCodecContext = avcodec_alloc_context3 (vidCodec);;
  avcodec_open2 (vidCodecContext, vidCodec, NULL);

  AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_H264);;

  AVFrame* vidFrame = av_frame_alloc();;
  struct SwsContext* swsContext = NULL;
  //}}}
  unsigned char* pesVid = (unsigned char*)malloc (2000000);
  while (true) {
    printf ("loading tsFile:%d", sequenceNum);
    //{{{  set ts file url
    char tsUrl [200];
    sprintf (tsUrl, "%s%s%d.ts", root, filename, sequenceNum);
    curl_easy_setopt (curl, CURLOPT_URL, tsUrl);
    //}}}
    //{{{  load ts file
    tsBufEndIndex = 0;
    int nn = curl_easy_perform (curl);
    //}}}
    if (tsBufEndIndex < 100) {
      printf (" not found\n");
      Sleep (2000);
      }
    else {
      int pesAudIndex = 0;
      int lastAudPidContinuity = -1;
      unsigned char* pesAud = tsBuf;

      int pesVidIndex = 0;
      int lastVidPidContinuity = -1;

      int tsIndex = 0;
      while (tsIndex < tsBufEndIndex) {
        if (tsBuf[tsIndex] == 0x47) {
          //{{{  tsFrame syncCode found, extract pes from tsFrames
          bool payStart  =  (tsBuf[tsIndex+1] & 0x40) != 0;
          int pid        = ((tsBuf[tsIndex+1] & 0x1F) << 8) | tsBuf[tsIndex+2];
          int continuity =   tsBuf[tsIndex+3] & 0x0F;
          bool payload   =  (tsBuf[tsIndex+3] & 0x10) != 0;
          bool adapt     =  (tsBuf[tsIndex+3] & 0x20) != 0;
          int tsFrameIndex = adapt ? (5 + tsBuf[tsIndex+4]) : 4;

          if (pid == 34) {
            //{{{  handle audio
            if (payload && ((lastAudPidContinuity == -1) || (continuity == ((lastAudPidContinuity+1) & 0x0F)))) {
              while (tsFrameIndex < 188) {
                // look for PES startCode
                if (payStart &&
                    (tsBuf[tsIndex+tsFrameIndex] == 0) &&
                    (tsBuf[tsIndex+tsFrameIndex+1] == 0) &&
                    (tsBuf[tsIndex+tsFrameIndex+2] == 1) &&
                    (tsBuf[tsIndex+tsFrameIndex+3] == 0xC0)) {
                  // PES start code found
                  //pesAudLen = (tsBuf[tsIndex+tsFrameIndex+4] << 8) | tsBuf[tsIndex+tsFrameIndex+5];
                  int ph1 = tsBuf[tsIndex+tsFrameIndex+6];
                  int ph2 = tsBuf[tsIndex+tsFrameIndex+7];

                  int pesAudHeadLen = tsBuf[tsIndex+tsFrameIndex+8];
                  tsFrameIndex += 9 + pesAudHeadLen;
                  }
                else
                  pesAud[pesAudIndex++] = tsBuf[tsIndex+tsFrameIndex++];
                }
              }
            else
              printf ("--- ts continuity break last:%d this:%d\n", lastAudPidContinuity, continuity);

            lastAudPidContinuity = continuity;
            }
            //}}}
          else if (pid == 33) {
            //{{{  handle vid
            if (payload && ((lastVidPidContinuity == -1) || (continuity == ((lastVidPidContinuity+1) & 0x0F)))) {
              // look for PES startCode
              if (payStart &&
                  (tsBuf[tsIndex+tsFrameIndex] == 0) &&
                  (tsBuf[tsIndex+tsFrameIndex+1] == 0) &&
                  (tsBuf[tsIndex+tsFrameIndex+2] == 1) &&
                  (tsBuf[tsIndex+tsFrameIndex+3] == 0xE0)) {
                // PES start code found
                int pesHeadLen = tsBuf[tsIndex+tsFrameIndex+8];
                tsFrameIndex += 9 + pesHeadLen;
                }
              while (tsFrameIndex < 188)
                pesVid[pesVidIndex++] = tsBuf[tsIndex+tsFrameIndex++];
              }
            else
              printf ("--- ts vid continuity break last:%d this:%d\n", lastVidPidContinuity, continuity);

            lastVidPidContinuity = continuity;
            }
            //}}}
          //else
          //  printf ("skipping pid:%d\n", pid);

          tsIndex += 188;
          }
          //}}}
        else
          tsIndex++;
        }
      printf (" ts:%d audPes:%d vidPes:%d\n", tsBufEndIndex, pesAudIndex, pesVidIndex);

      pesAud = tsBuf;
      if (audFramesLoaded == 0) {
        //{{{  warm up aud decoder with firstframe
        NeAACDecInit (decoder, tsBuf, 2048, &sampleRate, &channels);

        // dummy decode of first frame
        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode (decoder, &frameInfo, tsBuf, 2048);
        }
        //}}}
      for (int index = 0; index < pesAudIndex;) {
        //{{{  decode audPes into audFrames
        // alloc a frame and copy from pesAud
        int len = ((pesAud[index+3]&0x03)<<11) | (pesAud[index+4]<<3) | ((pesAud[index+5]&0xE0)>>5);
        audFrames[audFramesLoaded] = malloc (len);
        memcpy (audFrames[audFramesLoaded], pesAud+index, len);

        // decode, accum frameLR power
        NeAACDecFrameInfo frameInfo;
        short* ptr = (short*)NeAACDecDecode (decoder, &frameInfo, (unsigned char*)audFrames[audFramesLoaded], 2048);
        samples = frameInfo.samples / 2;
        audFramesPerSec = (float)sampleRate / samples;

        double valueL = 0;
        double valueR = 0;
        for (int i = 0; i < samples; i++) {
          valueL += pow(*ptr++, 2);
          valueR += pow(*ptr++, 2);
          }
        audFramesPowerL[audFramesLoaded] = (float)sqrt(valueL) / frameInfo.samples;
        audFramesPowerR[audFramesLoaded] = (float)sqrt(valueR) / frameInfo.samples;

        audFramesLoaded++;

        index += len;
        }
        //}}}

      if (pesVidIndex > 0)
        if (vidFramesLoaded < maxVidFrames-2) {
          //{{{  decode vidPes into vidFrames
          AVPacket vidPacket;
          av_init_packet (&vidPacket);
          vidPacket.data = pesVid;
          vidPacket.size = 0;

          while (pesVidIndex > 0) {
            uint8_t* data = NULL;
            av_parser_parse2 (vidParser, vidCodecContext,
                              &data, &vidPacket.size, vidPacket.data, pesVidIndex, 0, 0, AV_NOPTS_VALUE);

            int gotPicture = 0;
            int len = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
            vidPacket.data += len;
            pesVidIndex -= len;

            if (gotPicture != 0) {
              if (!swsContext)
                swsContext = sws_getContext (vidCodecContext->width, vidCodecContext->height, vidCodecContext->pix_fmt,
                                             vidCodecContext->width, vidCodecContext->height, AV_PIX_FMT_BGR24,
                                             SWS_BICUBIC, NULL, NULL, NULL);

              // create wicBitmap
              appWindow->getWicImagingFactory()->CreateBitmap (vidCodecContext->width, vidCodecContext->height,
                                               GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand,
                                               &vidFrames[vidFramesLoaded]);
              //{{{  lock wicBitmap
              WICRect wicRect = { 0, 0, vidCodecContext->width, vidCodecContext->height };
              IWICBitmapLock* wicBitmapLock = NULL;
              vidFrames[vidFramesLoaded]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);
              //}}}
              //{{{  get wicBitmap buffer
              UINT bufferLen = 0;
              BYTE* buffer = NULL;
              wicBitmapLock->GetDataPointer (&bufferLen, &buffer);
              //}}}
              int linesize = vidCodecContext->width * 3;
              sws_scale (swsContext, vidFrame->data, vidFrame->linesize, 0, vidCodecContext->height, &buffer, &linesize);
              wicBitmapLock->Release();

              vidFramesLoaded++;
              }
            }
          }
          //}}}

      appWindow->changed();
      sequenceNum++;
      }
    }

  av_free (vidFrame);
  avcodec_close (vidCodecContext);
  NeAACDecClose (decoder);
  return 0;
  }
//}}}
//{{{
static DWORD WINAPI fileLoaderThread (LPVOID arg) {

  AVFormatContext* avFormatContext = (AVFormatContext*)arg;
  //{{{  aud init
  AVCodecContext* audCodecContext = NULL;
  AVCodec* audCodec = NULL;
  AVFrame* audFrame = NULL;

  if (audStream >= 0) {
    audCodecContext = avFormatContext->streams[audStream]->codec;
    audCodec = avcodec_find_decoder (audCodecContext->codec_id);
    avcodec_open2 (audCodecContext, audCodec, NULL);
    audFrame = av_frame_alloc();
    }
  //}}}
  //{{{  vid init
  AVCodecContext* vidCodecContext = NULL;
  AVCodec* vidCodec = NULL;
  AVFrame* vidFrame = NULL;
  struct SwsContext* swsContext = NULL;

  if (vidStream >= 0) {
    vidCodecContext = avFormatContext->streams[vidStream]->codec;
    vidCodec = avcodec_find_decoder (vidCodecContext->codec_id);
    avcodec_open2 (vidCodecContext, vidCodec, NULL);
    vidFrame = av_frame_alloc();
    swsContext = sws_getContext (vidCodecContext->width, vidCodecContext->height, vidCodecContext->pix_fmt,
                                 vidCodecContext->width, vidCodecContext->height, AV_PIX_FMT_BGR24,
                                 SWS_BICUBIC, NULL, NULL, NULL);
    }
  //}}}
  //{{{  sub init
  AVCodecContext* subCodecContext = NULL;
  AVCodec* subCodec = NULL;

  if (subStream >= 0) {
    subCodecContext = avFormatContext->streams[subStream]->codec;
    subCodec = avcodec_find_decoder (subCodecContext->codec_id);
    avcodec_open2 (subCodecContext, subCodec, NULL);
    }
  //}}}
  AVSubtitle sub;
  memset (&sub, 0, sizeof(AVSubtitle));

  audFramesLoaded = 0;
  vidFramesLoaded = 0;
  AVPacket avPacket;
  while (true) {
    while (av_read_frame (avFormatContext, &avPacket) >= 0) {
      if (avPacket.stream_index == audStream) {
        // aud packet
        int gotPicture = 0;
        avcodec_decode_audio4 (audCodecContext, audFrame, &gotPicture, &avPacket);
        if (gotPicture) {
          samples = audFrame->nb_samples;
          audFramesPerSec = (float)sampleRate / samples;
          audFrames[audFramesLoaded] = malloc (channels * samples * 2);

          if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
            //{{{  16bit signed planar
            short* lptr = (short*)audFrame->data[0];
            short* rptr = (short*)audFrame->data[1];
            short* ptr = (short*)audFrames[audFramesLoaded];

            double valueL = 0;
            double valueR = 0;
            for (int i = 0; i < audFrame->nb_samples; i++) {
              *ptr = *lptr++;
              valueL += pow(*ptr++, 2);
              *ptr = *rptr++;
              valueR += pow(*ptr++, 2);
              }

            audFramesPowerL[audFramesLoaded] = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
            audFramesPowerR[audFramesLoaded] = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
            audFramesLoaded++;
            }
            //}}}
          else if (audCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
            //{{{  32bit float planar
            float* lptr = (float*)audFrame->data[0];
            float* rptr = (float*)audFrame->data[1];
            short* ptr = (short*)audFrames[audFramesLoaded];

            double valueL = 0;
            double valueR = 0;
            for (int i = 0; i < audFrame->nb_samples; i++) {
              *ptr = (short)(*lptr++ * 0x8000);
              valueL += pow(*ptr++, 2);
              *ptr = (short)(*rptr++ * 0x8000);
              valueR += pow(*ptr++, 2);
              }

            audFramesPowerL[audFramesLoaded] = (float)sqrt (valueL) / (audFrame->nb_samples * 2.0f);
            audFramesPowerR[audFramesLoaded] = (float)sqrt (valueR) / (audFrame->nb_samples * 2.0f);
            audFramesLoaded++;
            }
            //}}}
          else
            printf ("new sample_fmt:%d\n",audCodecContext->sample_fmt);
          }
        }

      else if (avPacket.stream_index == vidStream) {
        // vid packet
        if (vidFramesLoaded < 2000) {
          int gotPicture = 0;
          avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &avPacket);
          if (gotPicture) {
            // create wicBitmap
            appWindow->getWicImagingFactory()->CreateBitmap (vidCodecContext->width, vidCodecContext->height,
                                             GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand,
                                             &vidFrames[vidFramesLoaded]);
            //{{{  lock wicBitmap
            WICRect wicRect = { 0, 0, vidCodecContext->width, vidCodecContext->height };
            IWICBitmapLock* wicBitmapLock = NULL;
            vidFrames[vidFramesLoaded]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);
            //}}}
            //{{{  get wicBitmap buffer
            UINT bufferLen = 0;
            BYTE* buffer = NULL;
            wicBitmapLock->GetDataPointer (&bufferLen, &buffer);
            //}}}
            int linesize = vidCodecContext->width * 3;
            sws_scale (swsContext, vidFrame->data, vidFrame->linesize, 0, vidCodecContext->height, &buffer, &linesize);

            // add subtitle
            for (unsigned int r = 0; r < sub.num_rects; r++) {
              uint8_t* src = sub.rects[r]->pict.data[0];
              uint32_t* palette = (uint32_t*)sub.rects[r]->pict.data[1];

              for (int j = 0; j < sub.rects[r]->h; j++) {
                uint8_t* dst = buffer + ((sub.rects[r]->y + j)* linesize) + (sub.rects[r]->x * 3);
                for (int i = 0; i < sub.rects[r]->w; i++) {
                  uint32_t abgr = *(palette + *src++);
                  uint8_t a = abgr >> 24;
                  if (a) {
                    *dst++ = abgr & 0xFF;
                    *dst++ = (abgr >> 8) & 0xFF;
                    *dst++ = (abgr >> 16) & 0xFF;
                    }
                  else
                    dst += 3;
                  }
                }
              }

            wicBitmapLock->Release();

            vidFramesLoaded++;
            }
          }
        }
      else if (avPacket.stream_index == subStream) {
        int gotPicture = 0;
        avsubtitle_free (&sub);
        avcodec_decode_subtitle2 (subCodecContext, &sub, &gotPicture, &avPacket);
        }
      av_free_packet (&avPacket);
      }
    }

  av_free (vidFrame);
  av_free (audFrame);
  avcodec_close (vidCodecContext);
  avcodec_close (audCodecContext);
  avformat_close_input (&avFormatContext);

  return 0;
  }
//}}}

av_register_all();
avformat_network_init();
AVFormatContext* avFormatContext = NULL;
avformat_open_input (&avFormatContext, filename, NULL, NULL);
avformat_find_stream_info (avFormatContext, NULL);

for (unsigned int i = 0; i < avFormatContext->nb_streams; i++)
  if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (vidStream == -1))
    vidStream = i;
  else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) && (audStream == -1))
    audStream = i;
  else if ((avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) && (subStream == -1))
    subStream = i;

if (audStream >= 0) {
  channels = avFormatContext->streams[audStream]->codec->channels;
  sampleRate = avFormatContext->streams[audStream]->codec->sample_rate;
  }

printf ("filename:%s sampleRate:%d channels:%d streams:%d aud:%d vid:%d sub:%d\n",
        filename, sampleRate, channels,
        avFormatContext->nb_streams, audStream, vidStream, subStream);
av_dump_format (avFormatContext, 0, filename, 0);

if (channels > 2)
  channels = 2;

appWindow = cAppWindow::create (argv[1],
  (vidStream >= 0) ? avFormatContext->streams[vidStream]->codec->width : 1024,

CreateThread (NULL, 0, fileLoaderThread, avFormatContext, 0, NULL);
}
