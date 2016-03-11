// cJpegImage.h
#pragma once
//{{{  includes
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>

#include "../inc/jpeglib/jpeglib.h"
#pragma comment (lib,"turbojpeg-static")

using namespace std;
//}}}
//{{{  defines
//{{{  tags x
// tags
#define TAG_INTEROP_INDEX          0x0001
#define TAG_INTEROP_VERSION        0x0002
//}}}
//{{{  tags 1xx
#define TAG_IMAGE_WIDTH            0x0100
#define TAG_IMAGE_LENGTH           0x0101
#define TAG_BITS_PER_SAMPLE        0x0102
#define TAG_COMPRESSION            0x0103
#define TAG_PHOTOMETRIC_INTERP     0x0106
#define TAG_FILL_ORDER             0x010A
#define TAG_DOCUMENT_NAME          0x010D
#define TAG_IMAGE_DESCRIPTION      0x010E
#define TAG_MAKE                   0x010F
#define TAG_MODEL                  0x0110
#define TAG_SRIP_OFFSET            0x0111
#define TAG_ORIENTATION            0x0112
#define TAG_SAMPLES_PER_PIXEL      0x0115
#define TAG_ROWS_PER_STRIP         0x0116
#define TAG_STRIP_BYTE_COUNTS      0x0117
#define TAG_X_RESOLUTION           0x011A
#define TAG_Y_RESOLUTION           0x011B
#define TAG_PLANAR_CONFIGURATION   0x011C
#define TAG_RESOLUTION_UNIT        0x0128
#define TAG_TRANSFER_FUNCTION      0x012D
#define TAG_SOFTWARE               0x0131
#define TAG_DATETIME               0x0132
#define TAG_ARTIST                 0x013B
#define TAG_WHITE_POINT            0x013E
#define TAG_PRIMARY_CHROMATICITIES 0x013F
#define TAG_TRANSFER_RANGE         0x0156
//}}}
//{{{  tags 2xx
#define TAG_JPEG_PROC              0x0200
#define TAG_THUMBNAIL_OFFSET       0x0201
#define TAG_THUMBNAIL_LENGTH       0x0202
#define TAG_Y_CB_CR_COEFFICIENTS   0x0211
#define TAG_Y_CB_CR_SUB_SAMPLING   0x0212
#define TAG_Y_CB_CR_POSITIONING    0x0213
#define TAG_REFERENCE_BLACK_WHITE  0x0214
//}}}
//{{{  tags 1xxx
#define TAG_RELATED_IMAGE_WIDTH    0x1001
#define TAG_RELATED_IMAGE_LENGTH   0x1002
//}}}
//{{{  tags 8xxx
#define TAG_CFA_REPEAT_PATTERN_DIM 0x828D
#define TAG_CFA_PATTERN1           0x828E
#define TAG_BATTERY_LEVEL          0x828F
#define TAG_COPYRIGHT              0x8298
#define TAG_EXPOSURETIME           0x829A
#define TAG_FNUMBER                0x829D
#define TAG_IPTC_NAA               0x83BB
#define TAG_EXIF_OFFSET            0x8769
#define TAG_INTER_COLOR_PROFILE    0x8773
#define TAG_EXPOSURE_PROGRAM       0x8822
#define TAG_SPECTRAL_SENSITIVITY   0x8824
#define TAG_GPSINFO                0x8825
#define TAG_ISO_EQUIVALENT         0x8827
#define TAG_OECF                   0x8828
//}}}
//{{{  tags 9xxx
#define TAG_EXIF_VERSION           0x9000
#define TAG_DATETIME_ORIGINAL      0x9003
#define TAG_DATETIME_DIGITIZED     0x9004
#define TAG_COMPONENTS_CONFIG      0x9101
#define TAG_CPRS_BITS_PER_PIXEL    0x9102
#define TAG_SHUTTERSPEED           0x9201
#define TAG_APERTURE               0x9202
#define TAG_BRIGHTNESS_VALUE       0x9203
#define TAG_EXPOSURE_BIAS          0x9204
#define TAG_MAXAPERTURE            0x9205
#define TAG_SUBJECT_DISTANCE       0x9206
#define TAG_METERING_MODE          0x9207
#define TAG_LIGHT_SOURCE           0x9208
#define TAG_FLASH                  0x9209
#define TAG_FOCALLENGTH            0x920A
#define TAG_SUBJECTAREA            0x9214
#define TAG_MAKER_NOTE             0x927C
#define TAG_USERCOMMENT            0x9286
#define TAG_SUBSEC_TIME            0x9290
#define TAG_SUBSEC_TIME_ORIG       0x9291
#define TAG_SUBSEC_TIME_DIG        0x9292

#define TAG_WINXP_TITLE            0x9c9b // Windows XP - not part of exif standard.
#define TAG_WINXP_COMMENT          0x9c9c // Windows XP - not part of exif standard.
#define TAG_WINXP_AUTHOR           0x9c9d // Windows XP - not part of exif standard.
#define TAG_WINXP_KEYWORDS         0x9c9e // Windows XP - not part of exif standard.
#define TAG_WINXP_SUBJECT          0x9c9f // Windows XP - not part of exif standard.
//}}}
//{{{  tags Axxx
#define TAG_FLASH_PIX_VERSION      0xA000
#define TAG_COLOR_SPACE            0xA001
#define TAG_PIXEL_X_DIMENSION      0xA002
#define TAG_PIXEL_Y_DIMENSION      0xA003
#define TAG_RELATED_AUDIO_FILE     0xA004
#define TAG_INTEROP_OFFSET         0xA005
#define TAG_FLASH_ENERGY           0xA20B
#define TAG_SPATIAL_FREQ_RESP      0xA20C
#define TAG_FOCAL_PLANE_XRES       0xA20E
#define TAG_FOCAL_PLANE_YRES       0xA20F
#define TAG_FOCAL_PLANE_UNITS      0xA210
#define TAG_SUBJECT_LOCATION       0xA214
#define TAG_EXPOSURE_INDEX         0xA215
#define TAG_SENSING_METHOD         0xA217
#define TAG_FILE_SOURCE            0xA300
#define TAG_SCENE_TYPE             0xA301
#define TAG_CFA_PATTERN            0xA302
#define TAG_CUSTOM_RENDERED        0xA401
#define TAG_EXPOSURE_MODE          0xA402
#define TAG_WHITEBALANCE           0xA403
#define TAG_DIGITALZOOMRATIO       0xA404
#define TAG_FOCALLENGTH_35MM       0xA405
#define TAG_SCENE_CAPTURE_TYPE     0xA406
#define TAG_GAIN_CONTROL           0xA407
#define TAG_CONTRAST               0xA408
#define TAG_SATURATION             0xA409
#define TAG_SHARPNESS              0xA40A
#define TAG_DISTANCE_RANGE         0xA40C
#define TAG_IMAGE_UNIQUE_ID        0xA420
//}}}

//{{{  format
#define FMT_BYTE       1
#define FMT_STRING     2
#define FMT_USHORT     3
#define FMT_ULONG      4
#define FMT_URATIONAL  5
#define FMT_SBYTE      6
#define FMT_UNDEFINED  7
#define FMT_SSHORT     8
#define FMT_SLONG      9
#define FMT_SRATIONAL 10
#define FMT_SINGLE    11
#define FMT_DOUBLE    12
//}}}
//}}}
//{{{  static const
static const int kBytesPerFormat[] = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8 };

static const wchar_t* kWeekDay[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"};

static const D2D1_BITMAP_PROPERTIES kBitmapProperties = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 96.0f, 96.0f };
//}}}

class cJpegImage {
public:
  cJpegImage() {}
  cJpegImage (wstring& parentName, wchar_t* filename) : mFilename(filename), mFullFilename(parentName + L"\\" + filename) {}
  //{{{  gets
  wstring& getFileName() { return mFilename; }
  wstring& getFullFileName() { return mFullFilename; }

  bool getNoThumb() { return mNoThumb; }
  ID2D1Bitmap* getThumbBitmap() { return mD2D1BitmapThumb; }

  D2D1_SIZE_U& getFullSize() { return mFullSize; }
  D2D1_SIZE_U& getImageSize() { return mImageSize; }
  ID2D1Bitmap* getFullBitmap() { return mD2D1BitmapFull; }

  D2D1_RECT_F& getLayout() { return mLayout; }

  // info
  wstring& getMake() { return mMake; }
  wstring& getModel() { return mModel; }

  wstring& getExifTimeString() { return mExifTimeString; }
  wstring getCreationTime() { return getFileTimeString (&mLastWriteTime); }
  wstring getLastAccessTime()  { return getFileTimeString (&mLastAccessTime); }
  wstring getLastWriteTime()  { return getFileTimeString (&mLastWriteTime); }

  DWORD getOrientation() { return mOrientation; }
  float getAperture() { return mAperture; }
  float getFocalLength() { return mFocalLength; }
  float getExposure() { return mExposure; }

  //{{{
  wstring getDebugInfo() {

    wstring str (L"bytes:" + to_wstring (mFileBytes));

    if (mThumbBytes)
      str += L" tBytes:" + to_wstring (mThumbBytes);
    if (mThumbSize.width)
      str += L" tSize:" + to_wstring (mThumbSize.width) + L"x" + to_wstring (mThumbSize.height);
    if (mThumbLoadTime)
      str += L" tTime:" + to_wstring (mThumbLoadTime);
    if (mInfoLoadTime)
      str += L" iTime:" + to_wstring (mInfoLoadTime);
    if (mFullLoadScale)
      str += L" fScale:" + to_wstring (mFullLoadScale);
    if (mFullLoadTime)
      str += L" fTime:" + to_wstring (mFullLoadTime);

    return str;
    }
  //}}}
  wstring getDebugInfo1() { return mDebugInfo1; }
  wstring getGPSinfo() { return mGPSinfo ? mGPSinfo->getString() : L""; }
  //}}}

  void setLayout (D2D1_RECT_F& layout) { mLayout = layout; }
  //{{{
  bool pickItem (D2D1_POINT_2F& point) {

    return (point.x > mLayout.left) && (point.x < mLayout.right) &&
           (point.y > mLayout.top) && (point.y < mLayout.bottom);
    }
  //}}}

  //{{{
  bool loadInfo() {

    auto time = getTimer();

    auto fileHandle = CreateFile (mFullFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      //{{{  error
      wcout << L"loadThumbBitmap1 buffer error - " << mFullFilename << endl;
      return false;
      }
      //}}}

    mFileBytes = GetFileSize (fileHandle, NULL);
    GetFileTime (fileHandle, &mCreationTime, &mLastAccessTime, &mLastWriteTime);

    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto buffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    bool hasThumb = parseJpegHeader (buffer, mFileBytes, false);

    // close the file mapping object
    UnmapViewOfFile (buffer);
    CloseHandle (fileHandle);

    mInfoLoadTime = float(getTimer() - time);

    return hasThumb;
    }
  //}}}
  //{{{
  bool loadThumbBitmap (ID2D1DeviceContext* dc, D2D1_SIZE_U& thumbSize) {

    mNoThumb = false;

    auto time = getTimer();

    auto fileHandle = CreateFile (mFullFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      //{{{  error
      wcout << L"loadThumbBitmap1 buffer error - " << mFullFilename << endl;
      return false;
      }
      //}}}

    mFileBytes = GetFileSize (fileHandle, NULL);
    if (mFileBytes == 0) {
      //{{{  no bytes, return false
      wcout << L"loadThumbBitmap no fileBytes - " << mFullFilename << endl;
      CloseHandle (fileHandle);
      return false;
      }
      //}}}

    GetFileTime (fileHandle, &mCreationTime, &mLastAccessTime, &mLastWriteTime);

    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto buffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    auto hasThumb = parseJpegHeader (buffer, mFileBytes, true);
    auto thumbBuffer = hasThumb ? buffer+mThumbOffset : buffer;
    auto thumbBytes = hasThumb ? mThumbBytes : mFileBytes;

    if ((thumbBuffer[0] != 0xFF) || (thumbBuffer[1] != 0xD8)) {
      //{{{  no SOI marker, return
      wcout << L"loadThumbBitmap no SOI marker - " << mFullFilename << endl;
      UnmapViewOfFile (buffer);
      CloseHandle (fileHandle);
      return false;
      }
      //}}}

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);

    jpeg_mem_src (&cinfo, thumbBuffer, (unsigned long)thumbBytes);
    jpeg_read_header (&cinfo, true);

    // adjust scale to match thumbSize
    while ((cinfo.scale_denom < 8) &&
           ((cinfo.image_width/cinfo.scale_denom > ((unsigned int)thumbSize.width*3/2)) ||
             cinfo.image_height/cinfo.scale_denom > ((unsigned int)thumbSize.height*3/2)))
      cinfo.scale_denom *= 2;

    cinfo.out_color_space = JCS_EXT_BGRA;
    jpeg_start_decompress (&cinfo);

    auto thumbWidth = cinfo.output_width;
    auto thumbHeight = cinfo.output_height;
    auto pitch = cinfo.output_components * thumbWidth;
    mThumbSize.width = thumbWidth;
    mThumbSize.height = thumbHeight;

    auto thumbArray  = (BYTE*)malloc (pitch * thumbHeight);
    while (cinfo.output_scanline < thumbHeight) {
      BYTE* lineArray[1];
      lineArray[0] = thumbArray + (cinfo.output_scanline * pitch);
      jpeg_read_scanlines (&cinfo, lineArray, 1);
      }
    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    // close the file mapping object
    UnmapViewOfFile (buffer);
    CloseHandle (fileHandle);

    auto time1 = getTimer();
    dc->CreateBitmap (D2D1::SizeU (thumbWidth, thumbHeight), thumbArray, pitch, kBitmapProperties, &mD2D1BitmapThumb);
    free (thumbArray);

    auto time2 = getTimer();
    mThumbLoadTime = float(time2 - time);

    return true;
    }
  //}}}
  //{{{
  bool loadBuffer (ID2D1DeviceContext* dc, int scale, uint8_t* buffer, size_t bufferSize) {
  // load jpegImage in buffer, line by line into created bitmap

    if (mD2D1BitmapFull)
      return false;

    auto time = getTimer();
    mFullLoadScale = scale;

    if ((buffer[0] != 0xFF) || (buffer[1] != 0xD8)) {
      //{{{  no SOI marker, return false
      wcout << L"loadFullBitmap no SOI marker - " << mFullFilename << endl;
      return false;
      }
      //}}}

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);

    jpeg_mem_src (&cinfo, buffer, (unsigned long)bufferSize);
    jpeg_read_header (&cinfo, true);

    cinfo.scale_denom = scale;
    cinfo.out_color_space = JCS_EXT_BGRA;
    jpeg_start_decompress (&cinfo);

    mImageSize.width = cinfo.image_width;
    mImageSize.height = cinfo.image_height;
    mFullSize.width = cinfo.output_width;
    mFullSize.height = cinfo.output_height;
    auto pitch = cinfo.output_components * mFullSize.width;

    dc->CreateBitmap (mFullSize, kBitmapProperties, &mD2D1BitmapFull);

    BYTE* lineArray[1];
    lineArray[0] = (BYTE*)malloc (pitch);;
    D2D1_RECT_U r(RectU (0, 0, mFullSize.width, 0));
    while (cinfo.output_scanline < mFullSize.height) {
      r.top = cinfo.output_scanline;
      r.bottom = r.top+1;
      jpeg_read_scanlines (&cinfo, lineArray, 1);
      mD2D1BitmapFull->CopyFromMemory (&r, lineArray[0], pitch);
      }
    free (lineArray[0]);

    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    mFullLoadTime = float(getTimer() - time);
    wcout << L"load took:" << mFullLoadTime << endl;
    return true;
    }
  //}}}
  //{{{
  bool loadFullBitmap (ID2D1DeviceContext* dc, int scale) {

    if (mD2D1BitmapFull)
      return false;

    mFullLoadScale = scale;
    auto fileHandle = CreateFile (mFullFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      //{{{  error
      wcout << L"loadFullBitmap buffer error - " << mFullFilename << endl;
      return false;
      }
      //}}}
    size_t bufferSize = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto buffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    bool ret = loadBuffer (dc, scale, buffer, bufferSize);

    // close the file mapping object
    UnmapViewOfFile (buffer);
    CloseHandle (fileHandle);
    return ret;
    }
  //}}}
  //{{{
  void releaseFullBitmap() {

    if (mD2D1BitmapFull) {
      mD2D1BitmapFull->Release();
      mD2D1BitmapFull = nullptr;
      }
    }
  //}}}

private:
  //{{{
  class cGPSinfo {
  public:
    cGPSinfo() : mVersion(0),
                 mLatitudeRef(0),  mLatitudeDeg(0), mLatitudeMin(0), mLatitudeSec(0),
                 mLongitudeRef(0), mLongitudeDeg(0), mLongitudeMin(0), mLongitudeSec(0),
                 mAltitudeRef(0),  mAltitude(0), mImageDirectionRef(0), mImageDirection(0),
                 mHour(0), mMinute(0), mSecond(0) {}

    //{{{
    wstring getString() {

      wstringstream stringstream;

      if (!mDatum.empty())
        stringstream << mDatum << L' ';
      if (mLatitudeDeg || mLatitudeMin || mLatitudeSec)
        stringstream << mLatitudeDeg << mLatitudeRef << mLatitudeMin << L"'" << mLatitudeSec << L' ';
      if (mLongitudeDeg || mLongitudeMin || mLongitudeSec)
        stringstream << mLongitudeDeg << mLongitudeRef << mLongitudeMin << L"'" << mLongitudeSec << L' ';

      if (mAltitude)
        stringstream << L"alt:" << mAltitudeRef << mAltitude << L' ';
      if (mImageDirection)
        stringstream << L"dir:" << mImageDirectionRef << mImageDirection << L' ';

      if (!mDatum.empty())
        stringstream << mDate << L' ';

      if (mHour || mMinute || mSecond)
        stringstream << mHour << L':' << mMinute << L':' << mSecond;

      return stringstream.str();
      }
    //}}}

    int mVersion;

    wchar_t mLatitudeRef;
    float mLatitudeDeg;
    float mLatitudeMin;
    float mLatitudeSec;

    char mLongitudeRef;
    float mLongitudeDeg;
    float mLongitudeMin;
    float mLongitudeSec;

    wchar_t mAltitudeRef;
    float mAltitude;

    wchar_t mImageDirectionRef;
    float mImageDirection;

    float mHour;
    float mMinute;
    float mSecond;

    wstring mDate;
    wstring mDatum;
    };
  //}}}
  //{{{
  wstring getFileTimeString (FILETIME* fileTime) {

    SYSTEMTIME stUTC;
    FileTimeToSystemTime (fileTime, &stUTC);

    SYSTEMTIME stLocal;
    SystemTimeToTzSpecificLocalTime (NULL, &stUTC, &stLocal);

    wstringstream stringstream;
    stringstream << kWeekDay[stLocal.wDayOfWeek] << L" "
                 << setw(4) << stLocal.wYear << L":"
                 << setfill(L'0') << setw(2) << stLocal.wMonth << L":"
                 << setfill(L'0') << setw(2) << stLocal.wDay  << L" "
                 << setfill(L'0') << setw(2) << stLocal.wHour << L":"
                 << setfill(L'0') << setw(2) << stLocal.wMinute << L":"
                 << setfill(L'0') << setw(2) << stLocal.wSecond;
    return stringstream.str();
    }
  //}}}
  //{{{
  wstring getJpegMarkerStr (WORD marker, WORD markerLength) {

    wstring str;

    switch (marker) {
      case 0xFFC0: str = L"sof0"; break;
      case 0xFFC2: str = L"sof2"; break;
      case 0xFFC4: str = L"huff"; break;
      case 0xFFDA: str = L"sos"; break;
      case 0xFFDB: str = L"quan"; break;
      case 0xFFDD: str = L"dri"; break;
      case 0xFFE0: str = L"app0"; break;
      case 0xFFE1: str = L"app1"; break;
      case 0xFFE2: str = L"app2"; break;
      case 0xFFE3: str = L"app3"; break;
      case 0xFFE4: str = L"app4"; break;
      case 0xFFE5: str = L"app5"; break;
      case 0xFFE6: str = L"app6"; break;
      case 0xFFE7: str = L"app7"; break;
      case 0xFFE8: str = L"app8"; break;
      case 0xFFE9: str = L"app9"; break;
      case 0xFFEA: str = L"app10"; break;
      case 0xFFEB: str = L"app11"; break;
      case 0xFFEC: str = L"app12"; break;
      case 0xFFED: str = L"app13"; break;
      case 0xFFEE: str = L"app14"; break;
      case 0xFFFE: str = L"com"; break;
      default: str = to_wstring (marker);
      }

    return str + L":" + to_wstring (markerLength) + L" ";
    }
  //}}}

  //{{{
  WORD getExifWord (BYTE* ptr, bool intelEndian) {

    return *(ptr + (intelEndian ? 1 : 0)) << 8 |  *(ptr + (intelEndian ? 0 : 1));
    }
  //}}}
  //{{{
  DWORD getExifOffset (BYTE* ptr, bool intelEndian) {

    return getExifWord (ptr + (intelEndian ? 2 : 0), intelEndian) << 16 |
           getExifWord (ptr + (intelEndian ? 0 : 2), intelEndian);
    }
  //}}}
  //{{{
  float getExifSignedRational (BYTE* ptr, bool intelEndian, DWORD& numerator, DWORD& denominator) {

    numerator = getExifOffset (ptr, intelEndian);
    denominator = getExifOffset (ptr+4, intelEndian);

    if (denominator == 0)
      return 0;
    else
      return (float)numerator / denominator;
    }
  //}}}
  //{{{
  wstring getExifString (BYTE* ptr) {
  // Convert exif time to Unix time structure

    std::wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
    return (converter.from_bytes ((char*)ptr));
    }
  //}}}
  //{{{
  wstring getExifTime (BYTE* ptr, struct tm* tmPtr) {
  // Convert exif time to Unix time structure

    // Check for format: YYYY:MM:DD HH:MM:SS format.
    // Date and time normally separated by a space, but also seen a ':' there, so
    // skip the middle space with '%*c' so it can be any character.
    tmPtr->tm_wday = -1;
    tmPtr->tm_sec = 0;
    int a = sscanf ((char*)ptr, "%d%*c%d%*c%d%*c%d:%d:%d",
                    &tmPtr->tm_year, &tmPtr->tm_mon, &tmPtr->tm_mday,
                    &tmPtr->tm_hour, &tmPtr->tm_min, &tmPtr->tm_sec);

    tmPtr->tm_isdst = -1;
    tmPtr->tm_mon -= 1;     // Adjust for unix zero-based months
    tmPtr->tm_year -= 1900; // Adjust for year starting at 1900

    // find day of week
    mktime (tmPtr);

    // form wstring
    wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
    wstring str = converter.from_bytes ((char*)ptr);
    if ((tmPtr->tm_wday >= 0) && (tmPtr->tm_wday <= 6)) {
      str += L" ";
      str += kWeekDay[tmPtr->tm_wday];
      }

    return str;
    }
  //}}}
  //{{{
  cGPSinfo* getExifGpsInfo (BYTE* ptr, BYTE* offsetBasePtr, bool intelEndian) {

    auto gpsInfo = new cGPSinfo();

    auto numDirectoryEntries = getExifWord (ptr, intelEndian);
    ptr += 2;

    for (auto entry = 0; entry < numDirectoryEntries; entry++) {
      auto tag = getExifWord (ptr, intelEndian);
      auto format = getExifWord (ptr+2, intelEndian);
      auto components = getExifOffset (ptr+4, intelEndian);
      auto offset = getExifOffset (ptr+8, intelEndian);
      auto bytes = components * kBytesPerFormat[format];
      auto valuePtr = (bytes <= 4) ? ptr+8 : offsetBasePtr + offset;
      ptr += 12;

      DWORD numerator;
      DWORD denominator;
      switch (tag) {
        //{{{
        case 0x00:  // version
          gpsInfo->mVersion = getExifOffset (valuePtr, intelEndian);
          break;
        //}}}
        //{{{
        case 0x01:  // latitude ref
          gpsInfo->mLatitudeRef = valuePtr[0];
          break;
        //}}}
        //{{{
        case 0x02:  // latitude
          gpsInfo->mLatitudeDeg = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
          gpsInfo->mLatitudeMin = getExifSignedRational (valuePtr + 8, intelEndian, numerator, denominator);
          gpsInfo->mLatitudeSec = getExifSignedRational (valuePtr + 16, intelEndian, numerator, denominator);
          break;
        //}}}
        //{{{
        case 0x03:  // longitude ref
          gpsInfo->mLongitudeRef = valuePtr[0];
          break;
        //}}}
        //{{{
        case 0x04:  // longitude
          gpsInfo->mLongitudeDeg = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
          gpsInfo->mLongitudeMin = getExifSignedRational (valuePtr + 8, intelEndian, numerator, denominator);
          gpsInfo->mLongitudeSec = getExifSignedRational (valuePtr + 16, intelEndian, numerator, denominator);
          break;
        //}}}
        //{{{
        case 0x05:  // altitude ref
          gpsInfo->mAltitudeRef = valuePtr[0];
          break;
        //}}}
        //{{{
        case 0x06:  // altitude
          gpsInfo->mAltitude = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
          break;
        //}}}
        //{{{
        case 0x07:  // timeStamp
          gpsInfo->mHour = getExifSignedRational (valuePtr, intelEndian, numerator, denominator),
          gpsInfo->mMinute = getExifSignedRational (valuePtr + 8, intelEndian, numerator, denominator),
          gpsInfo->mSecond = getExifSignedRational (valuePtr + 16, intelEndian, numerator, denominator);
          break;
        //}}}
        //{{{
        case 0x08:  // satellites
          printf ("TAG_gps_Satellites %ls \n", getExifString (valuePtr).c_str());
          break;
        //}}}
        //{{{
        case 0x0B:  // dop
          printf ("TAG_gps_DOP %f\n", getExifSignedRational (valuePtr, intelEndian, numerator, denominator));
          break;
        //}}}
        //{{{
        case 0x10:  // direction ref
          gpsInfo->mImageDirectionRef = valuePtr[0];
          break;
        //}}}
        //{{{
        case 0x11:  // direction
          gpsInfo->mImageDirection = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
          break;
        //}}}
        //{{{
        case 0x12:  // datum
          gpsInfo->mDatum = getExifString (valuePtr);
          break;
        //}}}
        //{{{
        case 0x1D:  // date
          gpsInfo->mDate = getExifString (valuePtr);
          break;
        //}}}
        //{{{
        case 0x1B:
          //printf ("TAG_gps_ProcessingMethod\n");
          break;
        //}}}
        //{{{
        case 0x1C:
          //printf ("TAG_gps_AreaInformation\n");
          break;
        //}}}
        case 0x09: printf ("TAG_gps_status\n"); break;
        case 0x0A: printf ("TAG_gps_MeasureMode\n"); break;
        case 0x0C: printf ("TAG_gps_SpeedRef\n"); break;
        case 0x0D: printf ("TAG_gps_Speed\n"); break;
        case 0x0E: printf ("TAG_gps_TrackRef\n"); break;
        case 0x0F: printf ("TAG_gps_Track\n"); break;
        case 0x13: printf ("TAG_gps_DestLatitudeRef\n"); break;
        case 0x14: printf ("TAG_gps_DestLatitude\n"); break;
        case 0x15: printf ("TAG_gps_DestLongitudeRef\n"); break;
        case 0x16: printf ("TAG_gps_DestLongitude\n"); break;
        case 0x17: printf ("TAG_gps_DestBearingRef\n"); break;
        case 0x18: printf ("TAG_gps_DestBearing\n"); break;
        case 0x19: printf ("TAG_gps_DestDistanceRef\n"); break;
        case 0x1A: printf ("TAG_gps_DestDistance\n"); break;
        case 0x1E: printf ("TAG_gps_Differential\n"); break;
        default: printf ("unknown gps tag %x\n", tag); break;
        }
      }

    return gpsInfo;
    }
  //}}}
  //{{{
  void parseExifDirectory (BYTE* offsetBasePtr, BYTE* ptr, bool intelEndian) {

    auto numDirectoryEntries = getExifWord (ptr, intelEndian);
    ptr += 2;

    for (auto entry = 0; entry < numDirectoryEntries; entry++) {
      auto tag = getExifWord (ptr, intelEndian);
      auto format = getExifWord (ptr+2, intelEndian);
      auto components = getExifOffset (ptr+4, intelEndian);
      auto offset = getExifOffset (ptr+8, intelEndian);
      auto bytes = components * kBytesPerFormat[format];
      auto valuePtr = (bytes <= 4) ? ptr+8 : offsetBasePtr + offset;
      ptr += 12;

      DWORD numerator;
      DWORD denominator;
      switch (tag) {
        case TAG_EXIF_OFFSET:
          parseExifDirectory (offsetBasePtr, offsetBasePtr + offset, intelEndian); break;
        case TAG_ORIENTATION:
          mOrientation = offset; break;
        case TAG_APERTURE:
          mAperture = (float)exp(getExifSignedRational (valuePtr, intelEndian, numerator, denominator)*log(2)*0.5); break;
        case TAG_FOCALLENGTH:
          mFocalLength = getExifSignedRational (valuePtr, intelEndian, numerator, denominator); break;
        case TAG_EXPOSURETIME:
          mExposure = getExifSignedRational (valuePtr, intelEndian, numerator, denominator); break;
        case TAG_MAKE:
          if (mMake.empty()) mMake = getExifString (valuePtr); break;
        case TAG_MODEL:
          if (mModel.empty()) mModel = getExifString (valuePtr); break;
        case TAG_DATETIME:
        case TAG_DATETIME_ORIGINAL:
        case TAG_DATETIME_DIGITIZED:
          if (mExifTimeString.empty()) mExifTimeString = getExifTime (valuePtr, &mExifTm); break;
        case TAG_THUMBNAIL_OFFSET:
          mThumbOffset = offset; break;
        case TAG_THUMBNAIL_LENGTH:
          mThumbBytes = offset; break;
        case TAG_GPSINFO:
          mGPSinfo = getExifGpsInfo (offsetBasePtr + offset, offsetBasePtr, intelEndian); break;
        //case TAG_MAXAPERTURE:
        //  printf ("TAG_MAXAPERTURE\n"); break;
        //case TAG_SHUTTERSPEED:
        //  printf ("TAG_SHUTTERSPEED\n"); break;
        default:;
        //  printf ("TAG %x\n", tag);
        }
      }

    auto extraDirectoryOffset = getExifOffset (ptr, intelEndian);
    if (extraDirectoryOffset > 4)
      parseExifDirectory (offsetBasePtr, offsetBasePtr + extraDirectoryOffset, intelEndian);
    }
  //}}}
  //{{{
  bool parseJpegHeader (BYTE* ptr, size_t bufferBytes, bool allowDebug) {
  // find and read APP1 EXIF marker, return true if thumb, valid mThumbBuffer, mThumbLength
  // - make more resilient to bytes running out etc

    mThumbOffset = 0;
    mThumbBytes = 0;

    if (bufferBytes < 4) {
      //{{{  return false if not enough bytes for first marker
      wcout << L"parseHeader not enough bytes - " << mFullFilename << endl;
      return false;
      }
      //}}}

    auto soiPtr = ptr;
    if (getExifWord (ptr, false) != 0xFFD8) {
      //{{{  return false if no SOI marker
      wcout << L"parseHeader no soi marker - " << mFullFilename << endl;
      return false;
      }
      //}}}
    ptr += 2;

    auto marker = getExifWord (ptr, false);
    ptr += 2;
    auto markerLength = getExifWord (ptr, false);
    ptr += 2;
    auto exifIdPtr = ((marker == 0xFFE1) && (markerLength < bufferBytes-6)) ? ptr : nullptr;
    //{{{  parse markers, SOF marker for imageSize, until SOS, should abort if no more bytes
    wstring str (allowDebug ? L"soi " : L"");

    while ((marker >= 0xFF00) && (marker != 0xFFDA)) {
      // until invalid marker or SOS
      if (allowDebug)
        str += getJpegMarkerStr (marker, markerLength);
      if ((marker == 0xFFC0) || (marker == 0xFFC2)) {
        mImageSize.height = getExifWord (ptr+1, false);
        mImageSize.width = getExifWord (ptr+3, false);
        }
      ptr += markerLength-2;

      marker = getExifWord (ptr, false);
      ptr += 2;
      markerLength = getExifWord (ptr, false);
      ptr += 2;
      };

    if (allowDebug)
      mDebugInfo1 = str + getJpegMarkerStr (marker, markerLength);
    //}}}

    if (exifIdPtr) {
      //   check exifId, ignore trailing two nulls
      if (getExifOffset (exifIdPtr, false) != 0x45786966) {
        //{{{  return false if not EXIF
        wcout << L"parseHeader EXIF00 ident error - " << mFullFilename << endl;
        return false;
        }
        //}}}
      exifIdPtr += 6;
      auto offsetBasePtr = exifIdPtr;

      bool intelEndian = getExifWord (exifIdPtr, false) == 0x4949;
      exifIdPtr += 2;

      if (getExifWord (exifIdPtr, intelEndian) != 0x002a) {
        //{{{  return false if no 0x002a word
        wcout << L"parseHeader EXIF 2a error - " << mFullFilename << endl;
        return false;
        }
        //}}}
      exifIdPtr += 2;

      auto firstOffset = getExifOffset (exifIdPtr, intelEndian);
      if (firstOffset != 8) {
        //{{{  return false if unusual firstOffset,
        wcout << L"parseHeader firstOffset warning - " << mFullFilename << L" " << firstOffset << endl;
        return false;
        }
        //}}}
      exifIdPtr += 4;

      parseExifDirectory (offsetBasePtr, exifIdPtr, intelEndian);

      // adjust mThumbOffset to be from soiMarker
      mThumbOffset += DWORD(offsetBasePtr - soiPtr);
      return mThumbBytes > 0;
      }
    else // no thumb
      return false;
    }
  //}}}

  // vars
  wstring mFilename;
  wstring mFullFilename;

  D2D1_RECT_F mLayout = {0,0,0,0};

  DWORD mFileBytes = 0;
  DWORD mThumbOffset = 0;
  DWORD mThumbBytes = 0;

  bool mNoThumb = true;
  ID2D1Bitmap* mD2D1BitmapThumb = nullptr;

  D2D1_SIZE_U mFullSize = {0,0};
  D2D1_SIZE_U mImageSize = {0,0};
  ID2D1Bitmap* mD2D1BitmapFull = nullptr;

  //{{{  info
  wstring mMake;
  wstring mModel;

  DWORD mOrientation = 0;
  float mExposure = 0;
  float mFocalLength = 0;
  float mAperture = 0;

  tm mExifTm;
  wstring mExifTimeString;
  FILETIME mCreationTime;
  FILETIME mLastAccessTime;
  FILETIME mLastWriteTime;

  cGPSinfo* mGPSinfo = nullptr;
  //}}}
  //{{{  debug
  float mInfoLoadTime = 0;
  float mThumbLoadTime = 0;
  float mFullLoadTime = 0;

  int mFullLoadScale = 0;
  D2D1_SIZE_U mThumbSize = {0,0};

  wstring mDebugInfo1;
  //}}}
  };
