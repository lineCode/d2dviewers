// jpegView.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2DWindow.h"

#include "../inc/jpeglib/jpeglib.h"
#pragma comment(lib,"turbojpeg-static")

using namespace std;
//}}}
//{{{  const
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
static const int kBytesPerFormat[] = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8 };

static const wchar_t* kWeekDay[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"};
//}}}
//{{{  typedef
class cAppWindow;
typedef void (cAppWindow::*cAppWindowFunc)();

class cImage;
typedef void (cAppWindow::*cAppWindowImageFunc)(cImage* image);
//}}}

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
class cImage {
public:
  //{{{
  cImage (wstring& parentName, wchar_t* fileName) :
      mFileName(fileName), mFullFileName(parentName + L"\\" + fileName),
      mOrientation(0), mFocalLength(0), mAperture(0), mExposure(0), mGPSinfo(nullptr),
      mFullSize(SizeU(0,0)), mImageSize(SizeU(0,0)), mD2D1BitmapFull(nullptr),
      mFileBytes(0), mThumbOffset(0), mThumbBytes(0), mD2D1BitmapThumb(nullptr), mNoThumb(true),
      mInfoLoadTime(0), mThumbLoadTime(0), mFullLoadTime(0), mThumbSize(SizeU(0,0)), mFullLoadScale(0) {}
  //}}}

  //{{{  gets
  wstring& getFileName() { return mFileName; }
  wstring& getFullFileName() { return mFullFileName; }

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
  bool pick (D2D1_POINT_2F& point) {

    return (point.x > mLayout.left) && (point.x < mLayout.right) &&
           (point.y > mLayout.top) && (point.y < mLayout.bottom);
    }
  //}}}

  //{{{
  bool loadInfo() {

    auto time = getTimer();

    auto fileHandle = CreateFile (mFullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      //{{{  error
      wcout << L"loadThumbBitmap1 buffer error - " << mFullFileName<< endl;
      return false;
      }
      //}}}

    mFileBytes = GetFileSize (fileHandle, NULL);
    GetFileTime (fileHandle, &mCreationTime, &mLastAccessTime, &mLastWriteTime);

    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    bool hasThumb = parseJpegHeader (fileBuffer, mFileBytes, false);

    // close the file mapping object
    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);

    mInfoLoadTime = float(getTimer() - time);

    return hasThumb;
    }
  //}}}
  //{{{
  bool loadThumbBitmap (ID2D1DeviceContext* deviceContext, D2D1_SIZE_U& thumbSize) {

    mNoThumb = false;

    auto time = getTimer();

    auto fileHandle = CreateFile (mFullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      //{{{  error
      wcout << L"loadThumbBitmap1 buffer error - " << mFullFileName<< endl;
      return false;
      }
      //}}}

    mFileBytes = GetFileSize (fileHandle, NULL);
    if (mFileBytes == 0) {
      //{{{  no bytes, return false
      wcout << L"loadThumbBitmap no fileBytes - " << mFullFileName << endl;
      CloseHandle (fileHandle);
      return false;
      }
      //}}}

    GetFileTime (fileHandle, &mCreationTime, &mLastAccessTime, &mLastWriteTime);

    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    auto hasThumb = parseJpegHeader (fileBuffer, mFileBytes, true);
    auto thumbBuffer = hasThumb ? fileBuffer+mThumbOffset : fileBuffer;
    auto thumbBytes = hasThumb ? mThumbBytes : mFileBytes;

    if ((thumbBuffer[0] != 0xFF) || (thumbBuffer[1] != 0xD8)) {
      //{{{  no SOI marker, return
      wcout << L"loadThumbBitmap no SOI marker - " << mFullFileName << endl;
      UnmapViewOfFile (fileBuffer);
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
    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);

    auto time1 = getTimer();

    D2D1_BITMAP_PROPERTIES d2d1_bitmapProperties;
    d2d1_bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d2d1_bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    d2d1_bitmapProperties.dpiX = 96.0f;
    d2d1_bitmapProperties.dpiY = 96.0f;
    deviceContext->CreateBitmap (D2D1::SizeU (thumbWidth, thumbHeight),
                                 thumbArray, pitch, &d2d1_bitmapProperties, &mD2D1BitmapThumb);
    free (thumbArray);

    auto time2 = getTimer();
    mThumbLoadTime = float(time2 - time);

    return true;
    }
  //}}}
  //{{{
  bool loadFullBitmap (ID2D1DeviceContext* deviceContext, int scale) {

    if (mD2D1BitmapFull)
      return false;

    auto time = getTimer();
    mFullLoadScale = scale;
    auto fileHandle = CreateFile (mFullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      //{{{  error
      wcout << L"loadFullBitmap buffer error - " << mFullFileName << endl;
      return false;
      }
      //}}}
    size_t loadSize = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    if ((fileBuffer[0] != 0xFF) || (fileBuffer[1] != 0xD8)) {
      //{{{  no SOI marker, return
      wcout << L"loadFullBitmap no SOI marker - " << mFullFileName << endl;
      return false;
      }
      //}}}

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);

    jpeg_mem_src (&cinfo, fileBuffer, (unsigned long)loadSize);
    jpeg_read_header (&cinfo, true);

    cinfo.scale_denom = scale;
    cinfo.out_color_space = JCS_EXT_BGRA;
    jpeg_start_decompress (&cinfo);

    mImageSize.width = cinfo.image_width;
    mImageSize.height = cinfo.image_height;
    mFullSize.width = cinfo.output_width;
    mFullSize.height = cinfo.output_height;
    auto pitch = cinfo.output_components * mFullSize.width;

    D2D1_BITMAP_PROPERTIES d2d1_bitmapProperties;
    d2d1_bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d2d1_bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    d2d1_bitmapProperties.dpiX = 96.0f;
    d2d1_bitmapProperties.dpiY = 96.0f;
    deviceContext->CreateBitmap (mFullSize, d2d1_bitmapProperties, &mD2D1BitmapFull);

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

    // close the file mapping object
    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);

    mFullLoadTime = float(getTimer() - time);
    wcout << L"loadFull " << mFullFileName << L" took:" << mFullLoadTime << endl;
    return true;
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

    wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
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
      wcout << L"parseHeader not enough bytes - " << mFullFileName << endl;
      return false;
      }
      //}}}

    auto soiPtr = ptr;
    if (getExifWord (ptr, false) != 0xFFD8) {
      //{{{  return false if no SOI marker
      wcout << L"parseHeader no soi marker - " << mFullFileName << endl;
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
        wcout << L"parseHeader EXIF00 ident error - " << mFullFileName << endl;
        return false;
        }
        //}}}
      exifIdPtr += 6;
      auto offsetBasePtr = exifIdPtr;

      bool intelEndian = getExifWord (exifIdPtr, false) == 0x4949;
      exifIdPtr += 2;

      if (getExifWord (exifIdPtr, intelEndian) != 0x002a) {
        //{{{  return false if no 0x002a word
        wcout << L"parseHeader EXIF 2a error - " << mFullFileName << endl;
        return false;
        }
        //}}}
      exifIdPtr += 2;

      auto firstOffset = getExifOffset (exifIdPtr, intelEndian);
      if (firstOffset != 8) {
        //{{{  return false if unusual firstOffset,
        wcout << L"parseHeader firstOffset warning - " << mFullFileName << L" " << firstOffset << endl;
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
  wstring mFileName;
  wstring mFullFileName;

  D2D1_RECT_F mLayout;

  DWORD mFileBytes;
  DWORD mThumbOffset;
  DWORD mThumbBytes;

  bool mNoThumb;
  ID2D1Bitmap* mD2D1BitmapThumb;

  D2D1_SIZE_U mFullSize;
  D2D1_SIZE_U mImageSize;
  ID2D1Bitmap* mD2D1BitmapFull;

  //{{{  info
  wstring mMake;
  wstring mModel;

  DWORD mOrientation;
  float mExposure;
  float mFocalLength;
  float mAperture;

  tm mExifTm;
  wstring mExifTimeString;
  FILETIME mCreationTime;
  FILETIME mLastAccessTime;
  FILETIME mLastWriteTime;

  cGPSinfo* mGPSinfo;
  //}}}
  //{{{  debug
  float mInfoLoadTime;
  float mThumbLoadTime;
  float mFullLoadTime;

  int mFullLoadScale;
  D2D1_SIZE_U mThumbSize;

  wstring mDebugInfo1;
  //}}}
  };
//}}}
//{{{
class cDirectory {
public:
  cDirectory() {}
  //{{{
  cImage* pick (D2D1_POINT_2F& point) {

    for (auto directory : mDirectories) {
      auto pickedImage = directory->pick (point);
      if (pickedImage)
        return pickedImage;
      }

    for (auto image : mImages)
      if (image->pick (point))
        return image;

    return NULL;
    }
  //}}}

  //{{{
  int bestThumb (D2D1_POINT_2F& point, cImage** bestImage, float& bestMetric) {
  // get bestThumbImage for thumbLoader, nearest unloaded to point

    int numLoaded = 0;
    for (auto directory : mDirectories)
      numLoaded += directory->bestThumb (point, bestImage, bestMetric);

    for (auto image : mImages)
      if (image->getNoThumb()) {
        float xdiff = image->getLayout().left - point.x;
        float ydiff = image->getLayout().top - point.y;
        if ((xdiff >= 0) && (ydiff >= 0)) {
          // only bottom right quadrant
          float metric = (xdiff*xdiff) + (ydiff*ydiff);
          if (metric < bestMetric) {
            bestMetric = metric;
            *bestImage = image;
            }
          }
        }
      else
        numLoaded++;

    return numLoaded;
    }
  //}}}
  //{{{
  void layoutThumbs (D2D1_RECT_F& rect, D2D1_SIZE_U& thumbSize, int& column, int columns, int rows) {

    for (auto directory : mDirectories)
      directory->layoutThumbs (rect, thumbSize, column, columns, rows);

    for (auto image : mImages) {
      rect.right = rect.left + thumbSize.width;
      rect.bottom = rect.top + thumbSize.height;
      image->setLayout (rect);

      rect.left = rect.right;
      if ((column++ % columns) == (columns-1)) {
        rect.left = 0;
        rect.top = rect.bottom;
        column = 0;
        }
      }
    }
  //}}}

  //{{{
  void scanFileSysytem (wstring& parentName, wchar_t* directoryName,
                        int& numImages, int& numDirectories, cAppWindow* appWindow, cAppWindowFunc func) {
  // directoryName is findFileData.cFileName wchar_t*

    mDirectoryName = directoryName;
    mFullDirectoryName = parentName.empty() ? directoryName : parentName + L"\\" + directoryName;
    auto searchStr (mFullDirectoryName +  L"\\*");

    WIN32_FIND_DATA findFileData;
    auto file = FindFirstFileEx (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                 FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (file != INVALID_HANDLE_VALUE) {
      do {
        if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            (findFileData.cFileName[0] != L'.'))  {
          cDirectory* directory = new cDirectory();
          directory->scanFileSysytem (mFullDirectoryName, findFileData.cFileName,
                                      numImages, numDirectories, appWindow, func);
          mDirectories.push_back (directory);
          }
        else if (PathMatchSpec (findFileData.cFileName, L"*.jpg"))
          mImages.push_back (new cImage (mFullDirectoryName, findFileData.cFileName));
        } while (FindNextFile (file, &findFileData));
      FindClose (file);
      }

    numImages += (int)mImages.size();
    numDirectories += (int)mDirectories.size();

    //printf ("%d %d %ls %ls\n", (int)mImages.size(), (int)mDirectories.size(), mName, mFullName);
    (appWindow->*func)();
    }
  //}}}
  //{{{
  void traverseImages (cAppWindow* appWindow, cAppWindowImageFunc imageFunc) {

    for (auto directory : mDirectories)
      directory->traverseImages (appWindow, imageFunc);

    for (auto image : mImages)
      (appWindow->*imageFunc)(image);
    }
  //}}}

private:
  wstring mDirectoryName;
  wstring mFullDirectoryName;

  concurrency::concurrent_vector<cImage*> mImages;
  concurrency::concurrent_vector<cDirectory*> mDirectories;
  };
//}}}
//{{{
class cView2d {
public:
  //{{{
  cView2d() : mMatrix(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), mMinScale(1.0f),
              mSrcSize(SizeU(1, 1)), mDstSize(SizeF(1.0f, 1.0f)) {}
  //}}}

  // gets
  float getScale() { return mMatrix._11; }
  D2D1_POINT_2F getPoint() { return Point2F (mMatrix._31, mMatrix._32); }
  Matrix3x2F& getMatrix() { return mMatrix; }

  // sets
  //{{{
  bool setPoint (float x, float y) {

    bool changed = false;

    float xMin = mDstSize.width - (mSrcSize.width * mMatrix._11);
    if (x > 0.0f) {
      mMatrix._31 = 0.0f;
      changed = true;
      }
    else if (x < xMin) {
      mMatrix._31 = xMin;
      changed = true;
      }
    else if (x != mMatrix._31) {
      mMatrix._31 = x;
      changed = true;
      }

    float yMin = mDstSize.height - (mSrcSize.height * mMatrix._22);
    if (yMin > 0.0f)
      yMin = 0;

    if (y > 0.0f) {
      mMatrix._32 = 0.0f;
      changed = true;
      }
    else if (y < yMin) {
      mMatrix._32 = yMin;
      changed = true;
      }
    else if (y != mMatrix._32) {
      mMatrix._32 = y;
      changed = true;
      }

    return changed;
    }
  //}}}
  //{{{
  void setSrcDstSize (D2D1_SIZE_U srcSize, D2D1_SIZE_F dstSize) {

    auto wasShowingAll = getShowAll();

    mSrcSize = srcSize;
    mDstSize = dstSize;

    float xScale = dstSize.height / srcSize.height;
    float yScale = dstSize.width / srcSize.width;
    mMinScale = (xScale < yScale) ? xScale : yScale;

    if (wasShowingAll)
      showAll();
    }
  //}}}

  bool changeScale (float ratio) { return setScale (getScale() * ratio); }

  // reverse transform
  //{{{
  D2D1_POINT_2F dstToSrc (int x, int y) {

    return Point2F ((x - mMatrix._31) / mMatrix._11, (y - mMatrix._32) / mMatrix._22);
    }
  //}}}
  //{{{
  D2D1_POINT_2F srcToDst (float x, float y) {

    return Point2F ((x * mMatrix._11) + mMatrix._31,  (y * mMatrix._22) + mMatrix._32);
    }
  //}}}

private:
  float getMinScale() { return mMinScale; }
  bool getShowAll() { return getScale() == getMinScale(); }

  //{{{
  bool setScale (float scale) {

    bool changed = false;

    if (scale < mMinScale) {
      mMatrix._11 = mMinScale;
      mMatrix._22 = mMinScale;
      setPoint (mMatrix._31, mMatrix._32);
      changed = true;
      }

    else if ((scale != mMatrix._11) || (scale != mMatrix._22)) {
      mMatrix._11 = scale;
      mMatrix._22 = scale;
      setPoint (mMatrix._31, mMatrix._32);
      changed = true;
      }

    return changed;
    }
  //}}}

  void showAll() { setScale (getMinScale()); }

  // vars
  Matrix3x2F mMatrix;

  float mMinScale;
  D2D1_SIZE_U mSrcSize;
  D2D1_SIZE_F mDstSize;
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  cAppWindow() : mThumbSize(SizeU(160,120)), mCurView(&mThumbView),
                 mNumNestedDirectories(0), mNumNestedImages(0),
                 mNumThumbsLoaded(0), mFileSystemScanned(false),
                 mProxImage(NULL), mPickImage(nullptr), mFullImage(nullptr) {}
  //}}}
  //{{{
  void run (int width, int height, wchar_t* windowTitle, wchar_t* rootDirectory, int numLoaderThreads) {
  // windowTitle, rootDirectory wchar_t* rather than wstring

    initialise (windowTitle, width, height);
    getDeviceContext()->CreateSolidColorBrush (ColorF (0x101010, 0.8f), &mPanelBrush);

    // start threads
    thread ([=]() { scanDirectoryFunc (rootDirectory); } ).detach();
    for (auto i = 0; i < numLoaderThreads; i++)
      thread ([=]() { thumbLoaderFunc (i); } ).detach();

    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : return false;

      case 0x10: shiftKeyDown = true; return false; // shift key
      case 0x11: controlKeyDown = true; return false; // control key

      case 0x1B: return true; // escape abort

      case 0x20: break; // space bar

      case 0x21: break; // page up
      case 0x22: break; // page down

      case 0x23: break; // end
      case 0x24: break; // home

      case 0x25: break; // left arrow
      case 0x27: break; // right arrow

      case 0x26: if (mCurView->changeScale (1.0f / 1.1f)) changed(); break; // up arrow
      case 0x28: if (mCurView->changeScale (1.1f)) changed(); break; // down arrow

      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    float ratio = controlKeyDown ? 1.5f : shiftKeyDown ? 1.25f : 1.1f;
    if (delta < 0)
      ratio = 1.0f / ratio;

    if (mCurView->changeScale (ratio))
      changed();
    }
  //}}}
  //{{{
  void onMouseProx (int x, int y) {

    if (!mFullImage) {
      // displaying thumbs
      mProxImage = mDirectory.pick (mThumbView.dstToSrc (x, y));
      if (mPickImage != mProxImage) {
        mPickImage = mProxImage;
        if (mProxImage)
          if (!mProxImage->getThumbBitmap())
            mProxImage->loadInfo();
        changed();
        }
      }
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    float ratio = controlKeyDown ? 2.0f : shiftKeyDown ? 1.5f : 1.0f;

    if (mCurView->setPoint (mCurView->getPoint().x + xInc*ratio,  mCurView->getPoint().y + yInc*ratio))
      changed();
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!mouseMoved) {
      if (mFullImage) {
        // full
        mFullImage->releaseFullBitmap();
        mFullImage = nullptr;
        mCurView = &mThumbView;
        changed();
        }
      else if (mPickImage) {
        // thumbs
        setChangeRate (4);
        mFullImage = mPickImage;
        int scale = 1 + int((mFullImage->getImageSize().width / getClientF().width));
        mFullView.setSrcDstSize (SizeU(mFullImage->getImageSize().width/scale, mFullImage->getImageSize().height/scale), getClientF());
        mPickImage->loadFullBitmap (getDeviceContext(), scale);
        setChangeRate (0);

        changed();
        mCurView = &mFullView;
        }
      }
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* deviceContext) {

    deviceContext->Clear (ColorF(ColorF::Black));

    if (!mFullImage) {
      // thumbs
      deviceContext->SetTransform (mThumbView.getMatrix());
      mDirectory.traverseImages (this, &cAppWindow::drawThumb);

      if (mPickImage) {
        //{{{  highlight pickImage and draw infoPanel
        deviceContext->DrawRoundedRectangle (
          RoundedRect (mPickImage->getLayout(), 3.0f,3.0f), getYellowBrush(), 3.0f/mThumbView.getScale());

        // draw info panel
        deviceContext->SetTransform (Matrix3x2F::Scale (Size(1.0f,1.0f), Point2F (0,0)));

        auto rPanel (RectF (0.0f, getClientF().height-180.0f, getClientF().width, getClientF().height));
        deviceContext->FillRectangle (rPanel, mPanelBrush);

        rPanel.left += 10.0f;
        rPanel.top += 10.0f;
        if (mPickImage->getThumbBitmap()) {
          //{{{  draw thumb bitmap
          rPanel.right = rPanel.left + (160.0f*4.0f/3.0f);
          rPanel.bottom = rPanel.top + (120.0f*4.0f/3.0f);

          deviceContext->DrawBitmap (mPickImage->getThumbBitmap(), rPanel);
          rPanel.left = rPanel.right + 10.0f;
          rPanel.right = getClientF().width;
          rPanel.bottom = getClientF().height;
          }
          //}}}

        //{{{  draw fullFileName text
        deviceContext->DrawText (
          mPickImage->getFullFileName().c_str(), (UINT32)mPickImage->getFullFileName().size(), getTextFormat(),
          rPanel, getWhiteBrush());
        //}}}
        if (!mPickImage->getExifTimeString().empty()) {
          //{{{  draw exifTime text
          rPanel.top += 20.0f;

          deviceContext->DrawText (
            mPickImage->getExifTimeString().c_str(), (UINT32)mPickImage->getExifTimeString().size(), getTextFormat(),
            rPanel, getWhiteBrush());
          }
          //}}}
        //{{{  draw imageSize text
        rPanel.top += 20.0f;
        wstring str (to_wstring (mPickImage->getImageSize().width) + L"x" +
                          to_wstring (mPickImage->getImageSize().height));
        deviceContext->DrawText (str.c_str(), (UINT32)str.size(), getTextFormat(), rPanel, getWhiteBrush());
        //}}}
        //{{{  draw make text
        rPanel.top += 20.0f;

        deviceContext->DrawText (
          mPickImage->getMake().c_str(), (UINT32)mPickImage->getMake().size(), getTextFormat(),
          rPanel, getWhiteBrush());
        //}}}
        //{{{  draw model text
        rPanel.top += 20.0f;

        deviceContext->DrawText (
          mPickImage->getModel().c_str(), (UINT32)mPickImage->getModel().size(), getTextFormat(),
          rPanel, getWhiteBrush());
        //}}}
        if (false) {
          //{{{  draw orientation text
          rPanel.top += 20.0f;

          wstringstream stringstream;
          stringstream << L"orientation " << mPickImage->getOrientation();
          deviceContext->DrawText (stringstream.str().c_str(), (UINT32)stringstream.str().size(), getTextFormat(),
                                   rPanel, getWhiteBrush());
          }
          //}}}
        if (mPickImage->getAperture() > 0) {
          //{{{  draw aperture text
          rPanel.top += 20.0f;

          wstring str (L"aperture " + to_wstring(mPickImage->getAperture()));
          deviceContext->DrawText (str.c_str(), (UINT32)str.size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}
        if (mPickImage->getFocalLength() > 0) {
          //{{{  draw focalLength text
          rPanel.top += 20.0f;

          wstring str (L"focal length " + to_wstring(mPickImage->getFocalLength()));
          deviceContext->DrawText (str.c_str(), (UINT32)str.size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}
        if (mPickImage->getExposure() > 0) {
          //{{{  draw exposure text
          rPanel.top += 20.0f;

          wstring str (L"exposure " + to_wstring(mPickImage->getExposure()));
          deviceContext->DrawText (str.c_str(), (UINT32)str.size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}

        rPanel.left = getClientF().width/2.0f;
        rPanel.top = getClientF().height-180.0f + 10.0f;
        if (true)
          //{{{  draw debugStr
          deviceContext->DrawText (
            mPickImage->getDebugInfo().c_str(), (UINT32)mPickImage->getDebugInfo().size(), getTextFormat(),
            rPanel, getYellowBrush());

          rPanel.top += 20.0f;
          deviceContext->DrawText (
            mPickImage->getDebugInfo1().c_str(), (UINT32)mPickImage->getDebugInfo1().size(), getTextFormat(),
            rPanel, getYellowBrush());
          //}}}

        //{{{  draw creationTime
        rPanel.top += 20.0f;

        deviceContext->DrawText (
          mPickImage->getCreationTime().c_str(), (UINT32)mPickImage->getCreationTime().size(), getTextFormat(),
          rPanel, getYellowBrush());
        //}}}
        //{{{  draw lastAccessTime
        rPanel.top += 20.0f;

        deviceContext->DrawText (
          mPickImage->getLastAccessTime().c_str(), (UINT32)mPickImage->getLastAccessTime().size(), getTextFormat(),
          rPanel, getYellowBrush());
        //}}}
        //{{{  draw lastWriteTime
        rPanel.top += 20.0f;

        deviceContext->DrawText (
          mPickImage->getLastWriteTime().c_str(), (UINT32)mPickImage->getLastWriteTime().size(), getTextFormat(),
          rPanel, getYellowBrush());
        //}}}

        if (!mPickImage->getGPSinfo().empty()) {
          //{{{  draw GPSinfo
          rPanel.top += 20.0f;

          deviceContext->DrawText (
            mPickImage->getGPSinfo().c_str(), (UINT32)mPickImage->getGPSinfo().size(), getTextFormat(),
            rPanel, getYellowBrush());
          }
          //}}}
        }
        //}}}
      }

    else if (mFullImage->getFullBitmap()) {
      // fullImage
      deviceContext->SetTransform (mFullView.getMatrix());
      deviceContext->DrawBitmap (
        mFullImage->getFullBitmap(),
        RectF (0,0, (float)mFullImage->getFullSize().width,(float)mFullImage->getFullSize().height));

      // full image text
      deviceContext->SetTransform (Matrix3x2F::Scale (Size(1.0f,1.0f), Point2F (0,0)));
      deviceContext->DrawText (
        mFullImage->getFullFileName().c_str(), (UINT32)mFullImage->getFullFileName().size(), getTextFormat(),
        RectF(getClientF().width/2.0f, 0, getClientF().width,getClientF().height),
        getWhiteBrush());
      }

    // debug text
    wstringstream stringstream;
    stringstream << L"images" << mNumNestedImages << L" sub:" << mNumNestedDirectories
                 << L" loaded:" << mNumThumbsLoaded
                 << L" scale:" << mCurView->getScale()
                 << L" point:" << mCurView->getPoint().x << L"," << mCurView->getPoint().y;
    deviceContext->SetTransform (Matrix3x2F::Scale(Size(1.0f,1.0f), Point2F(0, 0)));
    deviceContext->DrawText (stringstream.str().c_str(), (UINT32)stringstream.str().size(), getTextFormat(),
                             RectF(0,0, getClientF().width,getClientF().height),
                             getWhiteBrush());
    }
  //}}}

private:
  //{{{
  void layoutThumbs() {

    auto windowAspect = getClientF().width / getClientF().height;
    auto thumbAspect = (float)mThumbSize.width / (float)mThumbSize.height;

    auto rows = 1;
    auto columns  = (int)sqrt (mNumNestedImages);
    while (columns < mNumNestedImages) {
      rows = ((mNumNestedImages-1) / columns) + 1;
      if (((thumbAspect * columns) / rows) > windowAspect)
        break;
      columns++;
      }

    int layoutColumn = 0;
    mDirectory.layoutThumbs (RectF (0,0,0,0), mThumbSize, layoutColumn, columns, rows);

    mThumbView.setSrcDstSize (SizeU(columns * mThumbSize.width, rows * mThumbSize.height), getClientF());

    changed();
    }
  //}}}
  //{{{
  void drawThumb (cImage* image) {

    if (image->getThumbBitmap())
      getDeviceContext()->DrawBitmap (image->getThumbBitmap(), image->getLayout());
    }
  //}}}
  //{{{
  void scanDirectoryFunc (wchar_t* rootDirectory) {
  // rootdirectory wchar_t* rather than wstring

    auto time1 = getTimer();
    mDirectory.scanFileSysytem (wstring(), rootDirectory,
                                mNumNestedImages, mNumNestedDirectories,
                                this, &cAppWindow::layoutThumbs);
    mFileSystemScanned = true;
    auto time2 = getTimer();

    wcout << L"scanDirectoryFunc exit images:" << mNumNestedImages
               << L" directories:" << mNumNestedDirectories
               << L" took:" << time2-time1
               << endl;
    }
  //}}}
  //{{{
  void thumbLoaderFunc (int index) {

    auto slept = 0;

    // give scanner a chance to get going
    while (!mFileSystemScanned && (slept < 10))
      Sleep (slept++);

    auto wasted = 0;
    while (slept < 20) {
      cImage* bestThumb = nullptr;
      auto bestMetric = FLT_MAX;
      mNumThumbsLoaded = mDirectory.bestThumb (mThumbView.dstToSrc(0,0), &bestThumb, bestMetric);
      if (bestThumb) {
        if (bestThumb->getNoThumb()) {
          if (bestThumb->loadThumbBitmap (getDeviceContext(), mThumbSize))
            changed();
          }
        else
          wasted++;
        }
      else
        Sleep (++slept);
      }

    wcout << L"thumbLoaderFunc:" << index
          << L" loaded:" << mNumThumbsLoaded
          << L" images:" << mNumNestedImages
          << L" wasted:" << wasted
          << L" slept:" << slept
          << endl;
    }
  //}}}
  //{{{  vars
  cDirectory mDirectory;
  int mNumNestedDirectories;
  int mNumNestedImages;

  cImage* mProxImage;
  cImage* mPickImage;
  cImage* mFullImage;

  cView2d* mCurView;
  cView2d mFullView;
  cView2d mThumbView;

  D2D1_SIZE_U mThumbSize;
  int mNumThumbsLoaded;
  bool mFileSystemScanned;

  ID2D1SolidColorBrush* mPanelBrush;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  startTimer();

  cAppWindow appWindow;
  appWindow.run (1860, 1000, L"jpegView", argv[1] ? argv[1] : L"C:\\Users\\nnn\\Pictures", 2);
  }
//}}}
