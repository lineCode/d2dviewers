// jpegApp.cpp
//{{{  includes
#include "pch.h"

#include "../inc/jpeglib.h"

//#pragma comment(lib,"jpeg-static")
#pragma comment(lib,"turbojpeg-static")
//}}}
int files = 0;
int totalBytes = 0;

//{{{  tag const
//{{{  recognised tags
#define TAG_MAKE               0x010F
#define TAG_MODEL              0x0110
#define TAG_ORIENTATION        0x0112
#define TAG_DATETIME           0x0132
#define TAG_THUMBNAIL_OFFSET   0x0201
#define TAG_THUMBNAIL_LENGTH   0x0202
#define TAG_EXPOSURETIME       0x829A
#define TAG_FNUMBER            0x829D
#define TAG_EXIF_OFFSET        0x8769
#define TAG_EXPOSURE_PROGRAM   0x8822
#define TAG_GPSINFO            0x8825
#define TAG_ISO_EQUIVALENT     0x8827
#define TAG_DATETIME_ORIGINAL  0x9003
#define TAG_DATETIME_DIGITIZED 0x9004
#define TAG_SHUTTERSPEED       0x9201
#define TAG_APERTURE           0x9202
#define TAG_EXPOSURE_BIAS      0x9204
#define TAG_MAXAPERTURE        0x9205
#define TAG_SUBJECT_DISTANCE   0x9206
#define TAG_METERING_MODE      0x9207
#define TAG_LIGHT_SOURCE       0x9208
#define TAG_FLASH              0x9209
#define TAG_FOCALLENGTH        0x920A
#define TAG_MAKER_NOTE         0x927C
#define TAG_USERCOMMENT        0x9286
#define TAG_EXIF_IMAGEWIDTH    0xa002
#define TAG_EXIF_IMAGELENGTH   0xa003
#define TAG_INTEROP_OFFSET     0xa005
#define TAG_FOCALPLANEXRES     0xa20E
#define TAG_FOCALPLANEUNITS    0xa210
#define TAG_EXPOSURE_INDEX     0xa215
#define TAG_EXPOSURE_MODE      0xa402
#define TAG_WHITEBALANCE       0xa403
#define TAG_DIGITALZOOMRATIO   0xa404
#define TAG_FOCALLENGTH_35MM   0xa405
//}}}
//{{{
struct tagTable_t {
  unsigned short mTag;
  char* mDesc;
  };
//}}}
//{{{
const tagTable_t kTagTable[] = {
  { 0x001,   "InteropIndex"},
  { 0x002,   "InteropVersion"},
  { 0x100,   "ImageWidth"},
  { 0x101,   "ImageLength"},
  { 0x102,   "BitsPerSample"},
  { 0x103,   "Compression"},
  { 0x106,   "PhotometricInterpretation"},
  { 0x10A,   "FillOrder"},
  { 0x10D,   "DocumentName"},
  { 0x10E,   "ImageDescription"},
  { 0x10F,   "Make"},
  { 0x110,   "Model"},
  { 0x111,   "StripOffsets"},
  { 0x112,   "Orientation"},
  { 0x115,   "SamplesPerPixel"},
  { 0x116,   "RowsPerStrip"},
  { 0x117,   "StripByteCounts"},
  { 0x11A,   "XResolution"},
  { 0x11B,   "YResolution"},
  { 0x11C,   "PlanarConfiguration"},
  { 0x128,   "ResolutionUnit"},
  { 0x12D,   "TransferFunction"},
  { 0x131,   "Software"},
  { 0x132,   "DateTime"},
  { 0x13B,   "Artist"},
  { 0x13E,   "WhitePoint"},
  { 0x13F,   "PrimaryChromaticities"},
  { 0x156,   "TransferRange"},
  { 0x200,   "JPEGProc"},
  { 0x201,   "ThumbnailOffset"},
  { 0x202,   "ThumbnailLength"},
  { 0x211,   "YCbCrCoefficients"},
  { 0x212,   "YCbCrSubSampling"},
  { 0x213,   "YCbCrPositioning"},
  { 0x214,   "ReferenceBlackWhite"},
  { 0x1001,  "RelatedImageWidth"},
  { 0x1002,  "RelatedImageLength"},
  { 0x828D,  "CFARepeatPatternDim"},
  { 0x828E,  "CFAPattern"},
  { 0x828F,  "BatteryLevel"},
  { 0x8298,  "Copyright"},
  { 0x829A,  "ExposureTime"},
  { 0x829D,  "FNumber"},
  { 0x83BB,  "IPTC/NAA"},
  { 0x8769,  "ExifOffset"},
  { 0x8773,  "InterColorProfile"},
  { 0x8822,  "ExposureProgram"},
  { 0x8824,  "SpectralSensitivity"},
  { 0x8825,  "GPS Dir offset"},
  { 0x8827,  "ISOSpeedRatings"},
  { 0x8828,  "OECF"},
  { 0x9000,  "ExifVersion"},
  { 0x9003,  "DateTimeOriginal"},
  { 0x9004,  "DateTimeDigitized"},
  { 0x9101,  "ComponentsConfiguration"},
  { 0x9102,  "CompressedBitsPerPixel"},
  { 0x9201,  "ShutterSpeedValue"},
  { 0x9202,  "ApertureValue"},
  { 0x9203,  "BrightnessValue"},
  { 0x9204,  "ExposureBiasValue"},
  { 0x9205,  "MaxApertureValue"},
  { 0x9206,  "SubjectDistance"},
  { 0x9207,  "MeteringMode"},
  { 0x9208,  "LightSource"},
  { 0x9209,  "Flash"},
  { 0x920A,  "FocalLength"},
  { 0x927C,  "MakerNote"},
  { 0x9286,  "UserComment"},
  { 0x9290,  "SubSecTime"},
  { 0x9291,  "SubSecTimeOriginal"},
  { 0x9292,  "SubSecTimeDigitized"},
  { 0xA000,  "FlashPixVersion"},
  { 0xA001,  "ColorSpace"},
  { 0xA002,  "ExifImageWidth"},
  { 0xA003,  "ExifImageLength"},
  { 0xA004,  "RelatedAudioFile"},
  { 0xA005,  "InteroperabilityOffset"},
  { 0xA20B,  "FlashEnergy"},
  { 0xA20C,  "SpatialFrequencyResponse"},
  { 0xA20E,  "FocalPlaneXResolution"},
  { 0xA20F,  "FocalPlaneYResolution"},
  { 0xA210,  "FocalPlaneResolutionUnit"},
  { 0xA214,  "SubjectLocation"},
  { 0xA215,  "ExposureIndex"},
  { 0xA217,  "SensingMethod"},
  { 0xA300,  "FileSource"},
  { 0xA301,  "SceneType"},
  { 0xA301,  "CFA Pattern"},
  { 0xA401,  "CustomRendered"},
  { 0xA402,  "ExposureMode"},
  { 0xA403,  "WhiteBalance"},
  { 0xA404,  "DigitalZoomRatio"},
  { 0xA405,  "FocalLengthIn35mmFilm"},
  { 0xA406,  "SceneCaptureType"},
  { 0xA407,  "GainControl"},
  { 0xA408,  "Contrast"},
  { 0xA409,  "Saturation"},
  { 0xA40a,  "Sharpness"},
  { 0xA40c,  "SubjectDistanceRange"},
} ;
//}}}
const int kBytesPerFormat[] = {0,1,1,2,4,8,1,1,2,4,8,4,8};
//}}}
//{{{
unsigned int getWord (BYTE** ptr, bool intelEndian, int& bytesLeft) {

  bytesLeft -= 2;
  if (intelEndian) {
    unsigned int lowerByte = *(*ptr)++;
    unsigned int upperByte = *(*ptr)++;
    return (upperByte << 8) | lowerByte;
    }
  else {
    unsigned int upperByte = *(*ptr)++;
    unsigned int lowerByte = *(*ptr)++;
    return (upperByte << 8) | lowerByte;
    }
  }
//}}}
//{{{
unsigned int getOffset (BYTE** ptr, bool intelEndian, int& bytesLeft) {

  if (intelEndian)
    return getWord (ptr, intelEndian, bytesLeft) | (getWord (ptr, intelEndian, bytesLeft) << 16);
  else
    return (getWord (ptr, intelEndian, bytesLeft) << 16) | getWord (ptr, intelEndian, bytesLeft);
  }
//}}}
//{{{
BYTE* readEXIF (BYTE* jpegHeader, int& jpegBytes) {
// read APP1 EXIF marker

  BYTE* ptr = jpegHeader;
  if ((*ptr++ != 0xFF) || (*ptr++ != 0xD8)) {
    //{{{  error
    printf ("no soi marker\n");
    return NULL;
    }
    //}}}
  if ((*ptr++ != 0xFF) || (*ptr++ != 0xE1)) {
    //{{{  error
    printf ("no APP1 marker\n");
    return NULL;
    }
    //}}}

  int bytesLeft = (*ptr++) << 8;
  bytesLeft |=  *ptr++;
  bytesLeft -= 2;
  if (bytesLeft < 0) {
    //{{{  error
    printf ("JPGD_BAD_VARIABLE_MARKER\n");
    return NULL;
    }
    //}}}

  if (bytesLeft >= 6) {
    //{{{  get exif ident
    unsigned int ident = (*ptr++) << 8;
    ident |=  *ptr++;

    unsigned int ident1 = (*ptr++) << 8;
    ident1 |=  *ptr++;

    unsigned int nnn = (*ptr++) << 8;
    nnn |=  *ptr++;

    bytesLeft -= 6;
    //}}}
    if ((ident != 0x4578) || (ident1 != 0x6966) || (nnn != 0)) {
      //{{{  error - not EXIF00 ident
      printf ("APP1 exif ident not found - marker length:%x ident %04x %04x %04x\n", bytesLeft, ident, ident1, nnn);
      return NULL;
      }
      //}}}
    else {
      bool intelEndian = true;
      if (bytesLeft >= 2) {
        //{{{  get endian
        unsigned int endian = (*ptr++) << 8;
        endian |= *ptr++;

        intelEndian = endian == 0x4949;
        bytesLeft -= 2;
        }
        //}}}
      //{{{  get 0x2a word
      unsigned int nnn = getWord (&ptr, intelEndian, bytesLeft);
      if (nnn != 0x2a)
        printf ("- not 2a %04x\n", nnn);
      //}}}
      //{{{  get firstDirectoryOffset
      unsigned int firstDirectoryOffset = getOffset (&ptr, intelEndian, bytesLeft);

      if (firstDirectoryOffset != 8)
        printf ("firstDirectoryOffset %08x\n", firstDirectoryOffset);
      //}}}

      unsigned int exifOffset = 0;
      unsigned int thumbnailOffset = 0;
      unsigned int thumbnailLength = 0;
      //{{{  get 0th IFD entries
      int numDirectoryEntries = getWord (&ptr, intelEndian, bytesLeft);

      for (int entry = 0; entry < numDirectoryEntries; entry++) {
        // directory entry, 12 bytes - tag16, format16, components32, offset32
        unsigned int tag = getWord (&ptr, intelEndian, bytesLeft);
        unsigned int format = getWord (&ptr, intelEndian, bytesLeft);
        unsigned int components = getOffset (&ptr, intelEndian, bytesLeft);
        unsigned int offset = getOffset (&ptr, intelEndian, bytesLeft);
        unsigned int bytes = components * kBytesPerFormat[format];

        if (tag == TAG_EXIF_OFFSET)
          exifOffset = offset;

        //for (int a = 0; a < sizeof(kTagTable) / sizeof(tagTable_t); a++)
        //  if (kTagTable[a].mTag == tag) {
        //    printf ("%24s ", kTagTable[a].mDesc);
        //    break;
        //    }
        //printf ("0th IFD tag %04x format %04x comp %08x offset %08x bytes %d\n",
        //        tag, format, components, offset, bytes);
        }
      //}}}

      unsigned int secondDirectoryOffset = getOffset (&ptr, intelEndian, bytesLeft);
      if (secondDirectoryOffset == 0)
        printf ("secondDirectoryOffset %x\n", secondDirectoryOffset);
      //{{{  get 0th IFD fields
      int firstFieldOffset = 8 + 2 + (numDirectoryEntries*12) + 4;
      int fieldBytes = exifOffset - firstFieldOffset;
      ptr += fieldBytes;
      //}}}

      if (exifOffset != 0) {
        //{{{  get exif IFD entries
        int numDirectoryEntries = getWord (&ptr, intelEndian, bytesLeft);

        for (int entry = 0; entry < numDirectoryEntries; entry++) {
          // directory entry, 12 bytes - tag16, format16, components32, offset32
          unsigned int tag = getWord (&ptr, intelEndian, bytesLeft);
          unsigned int format = getWord (&ptr, intelEndian, bytesLeft);
          unsigned int components = getOffset (&ptr, intelEndian, bytesLeft);
          unsigned int offset = getOffset (&ptr, intelEndian, bytesLeft);
          unsigned int bytes = components * kBytesPerFormat[format];

          //for (int a = 0; a < sizeof(kTagTable) / sizeof(tagTable_t); a++)
          //  if (kTagTable[a].mTag == tag) {
          //    printf ("%24s ", kTagTable[a].mDesc);
          //    break;
          //    }
          //printf ("EXIF IFD tag %04x format %04x comp %08x offset %08x bytes %d\n",
          //        tag, format, components, offset, bytes);
          }
        //}}}
        unsigned int zeroDirectoryOffset = getOffset (&ptr, intelEndian, bytesLeft);
        if (zeroDirectoryOffset != 0)
          printf ("directoryOffset %x\n", zeroDirectoryOffset);
        //{{{  get exif IFD fields
        firstFieldOffset += fieldBytes + 2 + (numDirectoryEntries*12) + 4;
        fieldBytes = secondDirectoryOffset - firstFieldOffset;
        ptr += fieldBytes;
        //}}}
        }

      numDirectoryEntries = getWord (&ptr, intelEndian, bytesLeft);
      //{{{  get 1st IFD entries
      for (int entry = 0; entry < numDirectoryEntries; entry++) {
        // directory entry, 12 bytes - tag16,format16,components32,offset32
        unsigned int tag = getWord (&ptr, intelEndian, bytesLeft);
        unsigned int format = getWord (&ptr, intelEndian, bytesLeft);
        unsigned int components = getOffset (&ptr, intelEndian, bytesLeft);
        unsigned int offset = getOffset (&ptr, intelEndian, bytesLeft);
        unsigned int bytes = components * kBytesPerFormat[format];

        if (tag == TAG_THUMBNAIL_OFFSET)
          thumbnailOffset = offset;
        if (tag == TAG_THUMBNAIL_LENGTH)
          thumbnailLength = offset;

        //for (int a = 0; a < sizeof(kTagTable) / sizeof(tagTable_t); a++)
        //  if (kTagTable[a].mTag == tag) {
        //    printf ("%24s ", kTagTable[a].mDesc);
        //    break;
        //    }
        //printf ("1st IFD tag %04x format %04x comp %08x offset %08x bytes %d\n",
        //        tag, format, components, offset, bytes);
        }
      //}}}

      unsigned int zeroDirectoryOffset1 = getOffset (&ptr, intelEndian, bytesLeft);
      if (zeroDirectoryOffset1 != 0)
        printf ("directoryOffset %x\n", zeroDirectoryOffset1);
      //{{{  get 1st IFD fields
      firstFieldOffset += fieldBytes + 2 + (numDirectoryEntries*12) + 4;
      fieldBytes = 16;
      ptr += fieldBytes;
      //}}}

      if (thumbnailOffset != 0) {
        // skip to thumbnail offset
        jpegBytes = thumbnailLength;
        return ptr + thumbnailOffset - firstFieldOffset - fieldBytes;
        }
      }
    }
  return NULL;
  }
//}}}

//{{{
void readJpeg (char* filename) {

  files++;

  struct stat file_info;
  int rc = stat (filename, &file_info);

  unsigned long jpegSize = file_info.st_size;
  unsigned char* jpegBuffer = (unsigned char*)malloc(jpegSize);

  totalBytes += jpegSize;

  FILE* fd = fopen (filename, "rb");
  size_t i = 0;
  while (i < jpegSize)
    i += fread (jpegBuffer + i, 1, jpegSize - i, fd);
  fclose (fd);

  struct jpeg_error_mgr jerr;
  struct jpeg_decompress_struct cinfo;
  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_decompress (&cinfo);

  int thumbnailBytes = jpegSize;
  BYTE* thumbnailPtr = readEXIF (jpegBuffer, thumbnailBytes);
  if (thumbnailPtr)
    jpeg_mem_src (&cinfo, thumbnailPtr, thumbnailBytes);
  else
    jpeg_mem_src (&cinfo, jpegBuffer, jpegSize);

  rc = jpeg_read_header (&cinfo, TRUE);
  if (rc != 1)
    exit (EXIT_FAILURE);

  jpeg_start_decompress (&cinfo);

  int bmpSize = cinfo.output_width * cinfo.output_height * cinfo.output_components;
  unsigned char* bmpBuffer = (unsigned char*)malloc(bmpSize);
  int row_stride = cinfo.output_width * cinfo.output_components;

  while (cinfo.output_scanline < cinfo.output_height) {
    unsigned char* buffer_array[1];
    buffer_array[0] = bmpBuffer + (cinfo.output_scanline) * row_stride;
    jpeg_read_scanlines (&cinfo, buffer_array, 1);
    }
  free (bmpBuffer);

  jpeg_finish_decompress (&cinfo);
  jpeg_destroy_decompress (&cinfo);

  free (jpegBuffer);

  printf ("\r%d %8d %4dx%4d %s        ", files, jpegSize, cinfo.output_width, cinfo.output_height, filename);
  }
//}}}
//{{{
void readDirectory (char* directory) {

  SetCurrentDirectory (directory);

  WIN32_FIND_DATA findFileData;
  HANDLE file = FindFirstFile ("*", &findFileData);
  if (file != INVALID_HANDLE_VALUE) {
    do {
      if (findFileData.cFileName[0] == '.')
        continue;
      if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        readDirectory (findFileData.cFileName);
      else if (strstr (findFileData.cFileName, ".jpg") || strstr (findFileData.cFileName, ".JPG"))
        readJpeg (findFileData.cFileName);
      } while (FindNextFile (file, &findFileData));

    FindClose (file);
    }

  printf ("files %d, bytes %d %s\n", files, totalBytes, directory);

  SetCurrentDirectory ("..");
  }
//}}}

int main (int argc, char *argv[]) {

  startCounter();
  readDirectory (argv[1]);

  printf ("files %d, bytes %d\n", files, totalBytes);

  Sleep (5000);
  return 0;
  }
