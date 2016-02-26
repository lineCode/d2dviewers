//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
//}}}
//{{{  const
#define SLICE_START_CODE_MIN    0x101
#define SLICE_START_CODE_MAX    0x1AF
#define USER_DATA_START_CODE    0x1B2
#define SEQUENCE_HEADER_CODE    0x1B3
#define SEQUENCE_ERROR_CODE     0x1B4
#define EXTENSION_START_CODE    0x1B5
#define SEQUENCE_END_CODE       0x1B7
#define GROUP_START_CODE        0x1B8
#define ISO_END_CODE            0x1B9

//{{{  scalable_mode
#define SC_NONE 0
#define SC_DP   1
#define SC_SPAT 2
#define SC_SNR  3
#define SC_TEMP 4
//}}}
//{{{  picture coding type
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4
//}}}
//{{{  picture structure
#define TOP_FIELD     1
#define BOTTOM_FIELD  2
#define FRAME_PICTURE 3
//}}}
//{{{  macroblock type
#define MACROBLOCK_INTRA                        1
#define MACROBLOCK_PATTERN                      2
#define MACROBLOCK_MOTION_BACKWARD              4
#define MACROBLOCK_MOTION_FORWARD               8
#define MACROBLOCK_QUANT                        16
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG       32
#define PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS 64
//}}}
//{{{  motion_type
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3
//}}}
//{{{  mv_format
#define MV_FIELD 0
#define MV_FRAME 1
//}}}
//{{{  chroma_format
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3
//}}}
//{{{  extension start code IDs /
#define SEQUENCE_EXTENSION_ID                    1
#define SEQUENCE_DISPLAY_EXTENSION_ID            2
#define QUANT_MATRIX_EXTENSION_ID                3
#define COPYRIGHT_EXTENSION_ID                   4
#define SEQUENCE_SCALABLE_EXTENSION_ID           5
#define PICTURE_DISPLAY_EXTENSION_ID             7
#define PICTURE_CODING_EXTENSION_ID              8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID    9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID  10
//}}}

#define ZIG_ZAG      0

#define PROFILE_422  (128+5)
#define MAIN_LEVEL   8

#define MB_WEIGHT    32
#define MB_CLASS4    64
//}}}
//{{{  tables
//{{{
static unsigned char scan[2][64] = {
  { /* Zig-Zag scan pattern  */
    0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
    12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
  },
  { /* Alternate scan pattern */
    0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
    41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
    51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
    53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
  }} ;
//}}}
//{{{
/* default intra quantization matrix */
static unsigned char default_intra_quantizer_matrix[64] = {
  8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83 } ;
//}}}
//{{{
/* non-linear quantization coefficient table */
static unsigned char Non_Linear_quantizer_scale[32] = {
   0, 1, 2, 3, 4, 5, 6, 7,
   8,10,12,14,16,18,20,22,
  24,28,32,36,40,44,48,52,
  56,64,72,80,88,96,104,112 } ;
//}}}

static int Table_6_20[3] = {6,8,12};
static unsigned char stwc_table[3][4] = { {6,3,7,4}, {2,1,5,4}, {2,5,7,4} };
static unsigned char stwclass_table[9] = {0, 1, 2, 1, 1, 2, 3, 3, 4};
//{{{
static double frame_rate_Table[16] = {
  0.0, ((23.0*1000.0)/1001.0), 24.0, 25.0,
  ((30.0*1000.0)/1001.0), 30.0, 50.0, ((60.0*1000.0)/1001.0), 60.0, -1, -1, -1, -1, -1, -1, -1 };
//}}}

//{{{  struct VLCtab
typedef struct {
  char val, len;
  } VLCtab;
//}}}
//{{{
/* Table B-3, macroblock_type in P-pictures, codes 001..1xx */
static VLCtab PMBtab0[8] = {
  {-1,0},
  {MACROBLOCK_MOTION_FORWARD,3},
  {MACROBLOCK_PATTERN,2}, {MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1}
};
//}}}
//{{{
/* Table B-3, macroblock_type in P-pictures, codes 000001..00011x */
static VLCtab PMBtab1[8] = {
  {-1,0},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,6},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,5}, {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,5}, {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_INTRA,5}, {MACROBLOCK_INTRA,5}
};
//}}}
//{{{
/* Table B-4, macroblock_type in B-pictures, codes 0010..11xx */
static VLCtab BMBtab0[16] = {
  {-1,0},
  {-1,0},
  {MACROBLOCK_MOTION_FORWARD,4},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,4},
  {MACROBLOCK_MOTION_BACKWARD,3},
  {MACROBLOCK_MOTION_BACKWARD,3},
  {MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,3},
  {MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,3},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2}
};
//}}}
//{{{
/* Table B-4, macroblock_type in B-pictures, codes 000001..00011x */
static VLCtab BMBtab1[8] = {
  {-1,0},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,6},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,6},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,6},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_INTRA,5},
  {MACROBLOCK_INTRA,5}
};
//}}}

//{{{
/* Table B-5, macroblock_type in spat. scal. I-pictures, codes 0001..1xxx */
static VLCtab spIMBtab[16] = {
  {-1,0},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS,4},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,4},
  {MACROBLOCK_INTRA,4},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2}, {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2}, {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1}, {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1}, {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1}, {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1}, {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,1}
};
//}}}
//{{{
/* Table B-6, macroblock_type in spat. scal. P-pictures, codes 0010..11xx */
static VLCtab spPMBtab0[16] =
{
  {-1,0},
  {-1,0},
  {MACROBLOCK_MOTION_FORWARD,4},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD,4},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,3}, {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,3},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,3}, {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,3},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,2}
};
//}}}
//{{{
/* Table B-6, macroblock_type in spat. scal. P-pictures, codes 0000010..000111x */
static VLCtab spPMBtab1[16] = {
  {-1,0},
  {-1,0},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,7},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS,7},
  {MACROBLOCK_PATTERN,7},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,7},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,7},
  {MACROBLOCK_INTRA,7},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,6},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_PATTERN,6}
};
//}}}
//{{{
/* Table B-7, macroblock_type in spat. scal. B-pictures, codes 0010..11xx */
static VLCtab spBMBtab0[14] = {
  {MACROBLOCK_MOTION_FORWARD,4},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,4},
  {MACROBLOCK_MOTION_BACKWARD,3},
  {MACROBLOCK_MOTION_BACKWARD,3},
  {MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,3},
  {MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,3},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,2}
};
//}}}
//{{{
/* Table B-7, macroblock_type in spat. scal. B-pictures, codes 0000100..000111x */
static VLCtab spBMBtab1[12] = {
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,7},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,7},
  {MACROBLOCK_INTRA,7},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,7},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_BACKWARD,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_BACKWARD,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,6},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,6}
};
//}}}
//{{{
/* Table B-7, macroblock_type in spat. scal. B-pictures, codes 00000100x..000001111 */
static VLCtab spBMBtab2[8] = {
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,8},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,8},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,8},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,8},
  {SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG|MACROBLOCK_QUANT|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,9},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_QUANT|MACROBLOCK_PATTERN,9},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS,9},
  {PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS|MACROBLOCK_PATTERN,9}
};
//}}}

//{{{
/* Table B-8, macroblock_type in spat. scal. B-pictures, codes 001..1xx */
static VLCtab SNRMBtab[8] = {
  {-1,0},
  {0,3},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_PATTERN,1},
  {MACROBLOCK_PATTERN,1},
  {MACROBLOCK_PATTERN,1},
  {MACROBLOCK_PATTERN,1}
};
//}}}

//{{{
/* Table B-10, motion_code, codes 0001 ... 01xx */
static VLCtab MVtab0[8] =
{ {-1,0}, {3,3}, {2,2}, {2,2}, {1,1}, {1,1}, {1,1}, {1,1}
};
//}}}
//{{{
/* Table B-10, motion_code, codes 0000011 ... 000011x */
static VLCtab MVtab1[8] =
{ {-1,0}, {-1,0}, {-1,0}, {7,6}, {6,6}, {5,6}, {4,5}, {4,5}
};
//}}}
//{{{
/* Table B-10, motion_code, codes 0000001100 ... 000001011x */
static VLCtab MVtab2[12] =
{ {16,9}, {15,9}, {14,9}, {13,9},
  {12,9}, {11,9}, {10,8}, {10,8},
  {9,8},  {9,8},  {8,8},  {8,8}
};
//}}}

//{{{
/* Table B-9, coded_block_pattern, codes 01000 ... 111xx */
static VLCtab CBPtab0[32] =
{ {-1,0}, {-1,0}, {-1,0}, {-1,0},
  {-1,0}, {-1,0}, {-1,0}, {-1,0},
  {62,5}, {2,5},  {61,5}, {1,5},  {56,5}, {52,5}, {44,5}, {28,5},
  {40,5}, {20,5}, {48,5}, {12,5}, {32,4}, {32,4}, {16,4}, {16,4},
  {8,4},  {8,4},  {4,4},  {4,4},  {60,3}, {60,3}, {60,3}, {60,3}
};
//}}}
//{{{
/* Table B-9, coded_block_pattern, codes 00000100 ... 001111xx */
static VLCtab CBPtab1[64] =
{ {-1,0}, {-1,0}, {-1,0}, {-1,0},
  {58,8}, {54,8}, {46,8}, {30,8},
  {57,8}, {53,8}, {45,8}, {29,8}, {38,8}, {26,8}, {37,8}, {25,8},
  {43,8}, {23,8}, {51,8}, {15,8}, {42,8}, {22,8}, {50,8}, {14,8},
  {41,8}, {21,8}, {49,8}, {13,8}, {35,8}, {19,8}, {11,8}, {7,8},
  {34,7}, {34,7}, {18,7}, {18,7}, {10,7}, {10,7}, {6,7},  {6,7},
  {33,7}, {33,7}, {17,7}, {17,7}, {9,7},  {9,7},  {5,7},  {5,7},
  {63,6}, {63,6}, {63,6}, {63,6}, {3,6},  {3,6},  {3,6},  {3,6},
  {36,6}, {36,6}, {36,6}, {36,6}, {24,6}, {24,6}, {24,6}, {24,6}
};
//}}}
//{{{
/* Table B-9, coded_block_pattern, codes 000000001 ... 000000111 */
static VLCtab CBPtab2[8] =
{ {-1,0}, {0,9}, {39,9}, {27,9}, {59,9}, {55,9}, {47,9}, {31,9}
};
//}}}

//{{{
/* Table B-1, macroblock_address_increment, codes 00010 ... 011xx */
static VLCtab MBAtab1[16] =
{ {-1,0}, {-1,0}, {7,5}, {6,5}, {5,4}, {5,4}, {4,4}, {4,4},
  {3,3}, {3,3}, {3,3}, {3,3}, {2,3}, {2,3}, {2,3}, {2,3}
};
//}}}
//{{{
/* Table B-1, macroblock_address_increment, codes 00000011000 ... 0000111xxxx */
static VLCtab MBAtab2[104] =
{
  {33,11}, {32,11}, {31,11}, {30,11}, {29,11}, {28,11}, {27,11}, {26,11},
  {25,11}, {24,11}, {23,11}, {22,11}, {21,10}, {21,10}, {20,10}, {20,10},
  {19,10}, {19,10}, {18,10}, {18,10}, {17,10}, {17,10}, {16,10}, {16,10},
  {15,8},  {15,8},  {15,8},  {15,8},  {15,8},  {15,8},  {15,8},  {15,8},
  {14,8},  {14,8},  {14,8},  {14,8},  {14,8},  {14,8},  {14,8},  {14,8},
  {13,8},  {13,8},  {13,8},  {13,8},  {13,8},  {13,8},  {13,8},  {13,8},
  {12,8},  {12,8},  {12,8},  {12,8},  {12,8},  {12,8},  {12,8},  {12,8},
  {11,8},  {11,8},  {11,8},  {11,8},  {11,8},  {11,8},  {11,8},  {11,8},
  {10,8},  {10,8},  {10,8},  {10,8},  {10,8},  {10,8},  {10,8},  {10,8},
  {9,7},   {9,7},   {9,7},   {9,7},   {9,7},   {9,7},   {9,7},   {9,7},
  {9,7},   {9,7},   {9,7},   {9,7},   {9,7},   {9,7},   {9,7},   {9,7},
  {8,7},   {8,7},   {8,7},   {8,7},   {8,7},   {8,7},   {8,7},   {8,7},
  {8,7},   {8,7},   {8,7},   {8,7},   {8,7},   {8,7},   {8,7},   {8,7}
};
//}}}

//{{{
/* Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110 */
static VLCtab DClumtab0[32] =
{ {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
  {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
  {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
  {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {-1, 0}
};
//}}}
//{{{
/* Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111 */
static VLCtab DClumtab1[16] =
{ {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
  {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9}
};
//}}}
//{{{
/* Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110 */
static VLCtab DCchromtab0[32] =
{ {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
  {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
  {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
  {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {-1, 0}
};
//}}}
//{{{
/* Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111 */
static VLCtab DCchromtab1[32] =
{ {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
  {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
  {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
  {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10}
};
//}}}

//{{{  struct DCTtab
typedef struct {
  char run, level, len;
  } DCTtab;
//}}}
//{{{
/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for first (DC) coefficient)
 */
DCTtab DCTtabfirst[12] =
{
  {0,2,4}, {2,1,4}, {1,1,3}, {1,1,3},
  {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1},
  {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1}
};
//}}}
//{{{
/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for all other coefficients)
 */
DCTtab DCTtabnext[12] =
{
  {0,2,4},  {2,1,4},  {1,1,3},  {1,1,3},
  {64,0,2}, {64,0,2}, {64,0,2}, {64,0,2}, /* EOB */
  {0,1,2},  {0,1,2},  {0,1,2},  {0,1,2}
};
//}}}
//{{{
/* Table B-14, DCT coefficients table zero,
 * codes 000001xx ... 00111xxx
 */
DCTtab DCTtab0[60] =
{
  {65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, /* Escape */
  {2,2,7}, {2,2,7}, {9,1,7}, {9,1,7},
  {0,4,7}, {0,4,7}, {8,1,7}, {8,1,7},
  {7,1,6}, {7,1,6}, {7,1,6}, {7,1,6},
  {6,1,6}, {6,1,6}, {6,1,6}, {6,1,6},
  {1,2,6}, {1,2,6}, {1,2,6}, {1,2,6},
  {5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
  {13,1,8}, {0,6,8}, {12,1,8}, {11,1,8},
  {3,2,8}, {1,3,8}, {0,5,8}, {10,1,8},
  {0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
  {0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
  {4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
  {4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5}
};
//}}}
//{{{
/* Table B-15, DCT coefficients table one,
 * codes 000001xx ... 11111111
*/
DCTtab DCTtab0a[252] =
{
  {65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, /* Escape */
  {7,1,7}, {7,1,7}, {8,1,7}, {8,1,7},
  {6,1,7}, {6,1,7}, {2,2,7}, {2,2,7},
  {0,7,6}, {0,7,6}, {0,7,6}, {0,7,6},
  {0,6,6}, {0,6,6}, {0,6,6}, {0,6,6},
  {4,1,6}, {4,1,6}, {4,1,6}, {4,1,6},
  {5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
  {1,5,8}, {11,1,8}, {0,11,8}, {0,10,8},
  {13,1,8}, {12,1,8}, {3,2,8}, {1,4,8},
  {2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
  {2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
  {1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
  {1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4}, /* EOB */
  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
  {0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
  {0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
  {0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
  {0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
  {9,1,7}, {9,1,7}, {1,3,7}, {1,3,7},
  {10,1,7}, {10,1,7}, {0,8,7}, {0,8,7},
  {0,9,7}, {0,9,7}, {0,12,8}, {0,13,8},
  {2,3,8}, {4,2,8}, {0,14,8}, {0,15,8}
};
//}}}
//{{{
/* Table B-14, DCT coefficients table zero,
 * codes 0000001000 ... 0000001111
 */
DCTtab DCTtab1[8] =
{
  {16,1,10}, {5,2,10}, {0,7,10}, {2,3,10},
  {1,4,10}, {15,1,10}, {14,1,10}, {4,2,10}
};
//}}}
//{{{
/* Table B-15, DCT coefficients table one,
 * codes 000000100x ... 000000111x
 */
DCTtab DCTtab1a[8] =
{
  {5,2,9}, {5,2,9}, {14,1,9}, {14,1,9},
  {2,4,10}, {16,1,10}, {15,1,9}, {15,1,9}
};
//}}}
//{{{
/* Table B-14/15, DCT coefficients table zero / one,
 * codes 000000010000 ... 000000011111
 */
DCTtab DCTtab2[16] =
{
  {0,11,12}, {8,2,12}, {4,3,12}, {0,10,12},
  {2,4,12}, {7,2,12}, {21,1,12}, {20,1,12},
  {0,9,12}, {19,1,12}, {18,1,12}, {1,5,12},
  {3,3,12}, {0,8,12}, {6,2,12}, {17,1,12}
};
//}}}
//{{{
/* Table B-14/15, DCT coefficients table zero / one,
 * codes 0000000010000 ... 0000000011111
 */
DCTtab DCTtab3[16] =
{
  {10,2,13}, {9,2,13}, {5,3,13}, {3,4,13},
  {2,5,13}, {1,7,13}, {1,6,13}, {0,15,13},
  {0,14,13}, {0,13,13}, {0,12,13}, {26,1,13},
  {25,1,13}, {24,1,13}, {23,1,13}, {22,1,13}
};
//}}}
//{{{
/* Table B-14/15, DCT coefficients table zero / one,
 * codes 00000000010000 ... 00000000011111
 */
DCTtab DCTtab4[16] =
{
  {0,31,14}, {0,30,14}, {0,29,14}, {0,28,14},
  {0,27,14}, {0,26,14}, {0,25,14}, {0,24,14},
  {0,23,14}, {0,22,14}, {0,21,14}, {0,20,14},
  {0,19,14}, {0,18,14}, {0,17,14}, {0,16,14}
};
//}}}
//{{{
/* Table B-14/15, DCT coefficients table zero / one,
 * codes 000000000010000 ... 000000000011111
 */
DCTtab DCTtab5[16] =
{
  {0,40,15}, {0,39,15}, {0,38,15}, {0,37,15},
  {0,36,15}, {0,35,15}, {0,34,15}, {0,33,15},
  {0,32,15}, {1,14,15}, {1,13,15}, {1,12,15},
  {1,11,15}, {1,10,15}, {1,9,15}, {1,8,15}
};
//}}}
//{{{
/* Table B-14/15, DCT coefficients table zero / one,
 * codes 0000000000010000 ... 0000000000011111
 */
DCTtab DCTtab6[16] =
{
  {1,18,16}, {1,17,16}, {1,16,16}, {1,15,16},
  {6,3,16}, {16,2,16}, {15,2,16}, {14,2,16},
  {13,2,16}, {12,2,16}, {11,2,16}, {31,1,16},
  {30,1,16}, {29,1,16}, {28,1,16}, {27,1,16}
};
//}}}
//}}}

static int Oldref_progressive_frame;
static int Newref_progressive_frame;

class cMpeg2dec {
public:
  //{{{
  cMpeg2dec() {

    Clip = (unsigned char*)malloc(1024);
    Clip += 384;
    for (int i = -384; i < 640; i++)
      Clip[i] = (i < 0) ? 0 : ((i > 255) ? 255 : i);

    initFastIDCT();
    }
  //}}}
  //{{{
  ~cMpeg2dec() {

    free (Clip);
    _close (Infile);
     }
  //}}}

  //{{{
  void initBitstream (char* filename) {

    Infile = _open (filename, O_RDONLY | O_BINARY);
    Incnt = 0;

    Rdptr = Rdbfr + 2048;
    Rdmax = Rdptr;

    Bfr = 0;
    flushBuffer (0);
    }
  //}}}
  //{{{
  void decodeBitstream() {

    int bitFrame = 0;

    printf ("waiting for SEQUENCE_HEADER_CODE\n");
    while (getNextHeader (false) != SEQUENCE_HEADER_CODE) {}
    initSequence();

    while (getNextHeader (true) != 0x100) {} // wait for pictureStartCode
    int seqFrame = 0;
    decodePicture (bitFrame, seqFrame);
    if (!Second_Field) {
      bitFrame++;
      seqFrame++;
      }

    while (true) {
      while (getNextHeader (true) != 0x100) {} // wait for pictureStartCode

      //SEQUENCE_END_CODE
      decodePicture (bitFrame, seqFrame);
      if (!Second_Field) {
        bitFrame++;
        seqFrame++;
        }
      }

    /* put last frame */
    if (seqFrame != 0) {
      if (Second_Field)
        printf ("last frame incomplete, not stored\n");
      else
        writeFrame (backward_reference_frame, bitFrame-1,
                    progressive_sequence || progressive_frame, Coded_Picture_Width, Coded_Picture_Height, Chroma_Width);
      }

    for (int i = 0; i < 3; i++) {
      free (backward_reference_frame[i]);
      free (forward_reference_frame[i]);
      free (auxframe[i]);
      }
    }
  //}}}

  //{{{
  virtual void writeFrame (unsigned char* src[], int frame, bool progressive, int width, int height, int chromaWidth) {
    printf ("writeFrame %d %d %d %d %d\n", frame, progressive, width, height, chromaWidth);
    }
  //}}}

private:
  //{{{  idct
  #define W1 2841 /* 2048*sqrt(2)*cos(1*pi/16) */
  #define W2 2676 /* 2048*sqrt(2)*cos(2*pi/16) */
  #define W3 2408 /* 2048*sqrt(2)*cos(3*pi/16) */
  #define W5 1609 /* 2048*sqrt(2)*cos(5*pi/16) */
  #define W6 1108 /* 2048*sqrt(2)*cos(6*pi/16) */
  #define W7 565  /* 2048*sqrt(2)*cos(7*pi/16) */
  //{{{
  /* row (horizontal) IDCT
   *           7                       pi         1
   * dst[k] = sum c[l] * src[l] * cos( -- * ( k + - ) * l )
   *          l=0                      8          2
   * where: c[0]    = 128
   *        c[1..7] = 128*sqrt(2) */
  void idctRow (short* blk) {

    int x0, x1, x2, x3, x4, x5, x6, x7, x8;

    /* shortcut */
    if (!((x1 = blk[4]<<11) | (x2 = blk[6]) | (x3 = blk[2]) |
          (x4 = blk[1]) | (x5 = blk[7]) | (x6 = blk[5]) | (x7 = blk[3]))) {
      blk[0]=blk[1]=blk[2]=blk[3]=blk[4]=blk[5]=blk[6]=blk[7]=blk[0]<<3;
      return;
      }

    x0 = (blk[0]<<11) + 128; /* for proper rounding in the fourth stage */

    /* first stage */
    x8 = W7*(x4+x5);
    x4 = x8 + (W1-W7)*x4;
    x5 = x8 - (W1+W7)*x5;
    x8 = W3*(x6+x7);
    x6 = x8 - (W3-W5)*x6;
    x7 = x8 - (W3+W5)*x7;

    /* second stage */
    x8 = x0 + x1;
    x0 -= x1;
    x1 = W6*(x3+x2);
    x2 = x1 - (W2+W6)*x2;
    x3 = x1 + (W2-W6)*x3;
    x1 = x4 + x6;
    x4 -= x6;
    x6 = x5 + x7;
    x5 -= x7;

    /* third stage */
    x7 = x8 + x3;
    x8 -= x3;
    x3 = x0 + x2;
    x0 -= x2;
    x2 = (181*(x4+x5)+128)>>8;
    x4 = (181*(x4-x5)+128)>>8;

    /* fourth stage */
    blk[0] = (x7+x1)>>8;
    blk[1] = (x3+x2)>>8;
    blk[2] = (x0+x4)>>8;
    blk[3] = (x8+x6)>>8;
    blk[4] = (x8-x6)>>8;
    blk[5] = (x0-x4)>>8;
    blk[6] = (x3-x2)>>8;
    blk[7] = (x7-x1)>>8;
    }
  //}}}
  //{{{
  /* column (vertical) IDCT
   *             7                         pi         1
   * dst[8*k] = sum c[l] * src[8*l] * cos( -- * ( k + - ) * l )
   *            l=0                        8          2
   * where: c[0]    = 1/1024
   *        c[1..7] = (1/1024)*sqrt(2) */
  void idctCol (short* blk) {

    int x0, x1, x2, x3, x4, x5, x6, x7, x8;

    /* shortcut */
    if (!((x1 = (blk[8*4]<<8)) | (x2 = blk[8*6]) | (x3 = blk[8*2]) |
          (x4 = blk[8*1]) | (x5 = blk[8*7]) | (x6 = blk[8*5]) | (x7 = blk[8*3]))) {
      blk[8*0]=blk[8*1]=blk[8*2]=blk[8*3]=blk[8*4]=blk[8*5]=blk[8*6]=blk[8*7] = iclp[(blk[8*0]+32)>>6];
      return;
      }

    x0 = (blk[8*0]<<8) + 8192;

    /* first stage */
    x8 = W7*(x4+x5) + 4;
    x4 = (x8+(W1-W7)*x4)>>3;
    x5 = (x8-(W1+W7)*x5)>>3;
    x8 = W3*(x6+x7) + 4;
    x6 = (x8-(W3-W5)*x6)>>3;
    x7 = (x8-(W3+W5)*x7)>>3;

    /* second stage */
    x8 = x0 + x1;
    x0 -= x1;
    x1 = W6*(x3+x2) + 4;
    x2 = (x1-(W2+W6)*x2)>>3;
    x3 = (x1+(W2-W6)*x3)>>3;
    x1 = x4 + x6;
    x4 -= x6;
    x6 = x5 + x7;
    x5 -= x7;

    /* third stage */
    x7 = x8 + x3;
    x8 -= x3;
    x3 = x0 + x2;
    x0 -= x2;
    x2 = (181*(x4+x5)+128)>>8;
    x4 = (181*(x4-x5)+128)>>8;

    /* fourth stage */
    blk[8*0] = iclp[(x7+x1)>>14];
    blk[8*1] = iclp[(x3+x2)>>14];
    blk[8*2] = iclp[(x0+x4)>>14];
    blk[8*3] = iclp[(x8+x6)>>14];
    blk[8*4] = iclp[(x8-x6)>>14];
    blk[8*5] = iclp[(x0-x4)>>14];
    blk[8*6] = iclp[(x3-x2)>>14];
    blk[8*7] = iclp[(x7-x1)>>14];
    }
  //}}}
  //{{{
  /* two dimensional inverse discrete cosine transform */
  void fastIDCT (short* block) {

    int i;
    for (i = 0; i < 8; i++)
      idctRow (block+8*i);

    for (i = 0; i < 8; i++)
      idctCol (block+i);
    }
  //}}}
  //{{{
  void initFastIDCT() {

    iclp = iclip + 512;
    for (int i = -512; i < 512; i++)
      iclp[i] = (i < -256) ? -256 : ((i > 255) ? 255 : i);
    }
  //}}}
  //}}}
  //{{{  get bits, code
  // getBits
  //{{{
  void fillBuffer() {

    int Buffer_Level = _read (Infile, Rdbfr, 2048);
    Rdptr = Rdbfr;

    /* end of the bitstream file */
    if (Buffer_Level < 2048) {
      /* just to be safe */
      if (Buffer_Level < 0)
        Buffer_Level = 0;

      /* pad until the next to the next 32-bit word boundary */
      while (Buffer_Level & 3)
        Rdbfr[Buffer_Level++] = 0;

      /* pad the buffer with sequence end codes */
      while (Buffer_Level < 2048) {
        Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>24;
        Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>16;
        Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>8;
        Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE&0xff;
        }
      }
    }
  //}}}

  //{{{
  unsigned int showBits (int n) {
  /* return next n bits (right adjusted) without advancing */
    return Bfr >> (32 - n);
    }
  //}}}

  //{{{
  void flushBuffer (int n) {
  /* advance by n bits */

    Bfr <<= n;
    Incnt -= n;

    if (Incnt <= 24) {
      if (Rdptr < Rdbfr+2044) {
        do {
          Bfr |= *Rdptr++ << (24 - Incnt);
          Incnt += 8;
          }
        while (Incnt <= 24);
        }
      else {
        do {
          if (Rdptr >= Rdbfr + 2048)
            fillBuffer();
          Bfr |= *Rdptr++ << (24 - Incnt);
          Incnt += 8;
          }
        while (Incnt <= 24);
        }
      }
    }
  //}}}
  //{{{
  void flushBuffer32() {

    Bfr = 0;
    Incnt -= 32;
    while (Incnt <= 24) {
      if (Rdptr >= Rdbfr+2048)
        fillBuffer();
      Bfr |= *Rdptr++ << (24 - Incnt);
      Incnt += 8;
      }
    }
  //}}}

  //{{{
  /* return next n bits (right adjusted) */
  unsigned int getBits (int n) {

    unsigned int val = showBits (n);
    flushBuffer (n);
    return val;
    }
  //}}}
  //{{{
  /* return next bit (could be made faster than getBits(1)) */
  unsigned int getBits1() {
    return getBits (1);
    }
  //}}}
  //{{{
  unsigned int getBits32() {

    unsigned int l = showBits (32);
    flushBuffer32();
    return l;
    }
  //}}}

  //{{{
  int extraBitInformation() {
  /* decode extra bit information ISO/IEC 13818-2 section 6.2.3.4. */

    int Byte_Count = 0;
    while (getBits1()) {
      flushBuffer (8);
      Byte_Count++;
      }

    return Byte_Count;
    }
  //}}}
  //{{{
  unsigned int showNextStartCode() {
  // align to start of nextStartCode, return it, don't consume it

    /* byte align */
    flushBuffer (Incnt & 7);

    while (showBits (24) != 0x01L)
      flushBuffer (8);

    return showBits (32);
    }
  //}}}
  //}}}
  //{{{  macroBlocks
  //{{{
  int getImacroblockType() {

    if (getBits1())
      return 1;

    if (!getBits1()) {
      printf ("Invalid macroblock_type code\n");
      Fault_Flag = 1;
      }

    return 17;
    }
  //}}}
  //{{{
  int getPmacroblockType() {

    int code;
    if ((code = showBits (6)) >= 8) {
      code >>= 3;
      flushBuffer (PMBtab0[code].len);
      return PMBtab0[code].val;
      }

    if (code == 0) {
      printf ("Invalid macroblock_type code\n");
      Fault_Flag = 1;
      return 0;
      }

    flushBuffer (PMBtab1[code].len);
    return PMBtab1[code].val;
    }
  //}}}
  //{{{
  int getBmacroblockType() {

    int code;
    if ((code = showBits(6)) >= 8) {
      code >>= 2;
      flushBuffer (BMBtab0[code].len);
      return BMBtab0[code].val;
      }

    if (code == 0) {
      printf("Invalid macroblock_type code\n");
      Fault_Flag = 1;
      return 0;
      }

    flushBuffer(BMBtab1[code].len);
    return BMBtab1[code].val;
    }
  //}}}
  //{{{
  int getDmacroblockType() {

    if (!getBits1()) {
      printf ("Invalid macroblock_type code\n");
      Fault_Flag = 1;
      }

    return 1;
    }
  //}}}
  //{{{
  int getMacroBlockType() {

    int macroblock_type = 0;
    switch (picture_coding_type) {
      case I_TYPE:
        macroblock_type = getImacroblockType();
        break;
      case P_TYPE:
        macroblock_type = getPmacroblockType();
        break;
      case B_TYPE:
        macroblock_type = getBmacroblockType();
        break;
      case D_TYPE:
        macroblock_type = getDmacroblockType();
        break;
      default:
        printf("getMacroblockType(): unrecognized picture coding type\n");
        break;
      }

    return macroblock_type;
    }
  //}}}
  //{{{
  int getMotionCode() {

    int code;
    if (getBits1())
      return 0;

    if ((code = showBits(9))>=64) {
      code >>= 6;
      flushBuffer (MVtab0[code].len);
      return getBits1() ? -MVtab0[code].val : MVtab0[code].val;
      }

    if (code >= 24) {
      code >>= 3;
      flushBuffer (MVtab1[code].len);
      return getBits1() ? -MVtab1[code].val : MVtab1[code].val;
      }

    if ((code -= 12) < 0) {
      Fault_Flag = 1;
      return 0;
      }

    flushBuffer(MVtab2[code].len);
    return getBits1() ? -MVtab2[code].val : MVtab2[code].val;
    }
  //}}}
  //{{{
  /* get differential motion vector (for dual prime prediction) */
  int getDmvector() {

    if (getBits(1))
      return getBits(1) ? -1 : 1;
    else
      return 0;
    }
  //}}}
  //{{{
  int getCodedBlockPattern() {

    int code;
    if ((code = showBits(9))>=128) {
      code >>= 4;
      flushBuffer(CBPtab0[code].len);
      return CBPtab0[code].val;
      }

    if (code>=8) {
      code >>= 1;
      flushBuffer(CBPtab1[code].len);
      return CBPtab1[code].val;
      }

    if (code<1) {
      printf("Invalid coded_block_pattern code\n");
      Fault_Flag = 1;
      return 0;
      }

    flushBuffer(CBPtab2[code].len);
    return CBPtab2[code].val;
    }
  //}}}
  //{{{
  int getMacroBlockAddressIncrement() {

    int code;
    int val = 0;

    while ((code = showBits(11))<24) {
      if (code != 15) /* if not macroblock_stuffing */ {
        if (code == 8) /* if macroblock_escape */ {
          val+= 33;
          }
        else {
          printf("Invalid macroblock_address_increment code\n");
          Fault_Flag = 1;
          return 1;
          }
        }
      flushBuffer (11);
      }

    /* macroblock_address_increment == 1 ('1' is in the MSB position of the lookahead) */
    if (code >= 1024) {
      flushBuffer (1);
      return val + 1;
      }

    /* codes 00010 ... 011xx */
    if (code >= 128) {
      /* remove leading zeros */
      code >>= 6;
      flushBuffer (MBAtab1[code].len);
      return val + MBAtab1[code].val;
      }

    /* codes 00000011000 ... 0000111xxxx */
    code-= 24; /* remove common base */
    flushBuffer (MBAtab2[code].len);
    return val + MBAtab2[code].val;
    }
  //}}}
  //{{{
  int getLumaDCdctDiff() {
  // combined MPEG-1 and MPEG-2 stage. parse VLC and perform dct_diff arithmetic.
  //  MPEG-1:  ISO/IEC 11172-2 section MPEG-2:  ISO/IEC 13818-2 section 7.2.1
  //  Note: the arithmetic here is presented more elegantly than the spec, yet the results, dct_diff, are the same. */

    /* decode length */
    int code = showBits(5);

    int size;
    if (code < 31) {
      size = DClumtab0[code].val;
      flushBuffer (DClumtab0[code].len);
      }
    else {
      code = showBits(9) - 0x1f0;
      size = DClumtab1[code].val;
      flushBuffer (DClumtab1[code].len);
      }

    int dct_diff;
    if (size == 0)
      dct_diff = 0;
    else {
      dct_diff = getBits (size);
      if ((dct_diff & (1<<(size-1)))==0)
        dct_diff-= (1<<size) - 1;
      }

    return dct_diff;
    }
  //}}}
  //{{{
  int getChromaDCdctDiff() {


    /* decode length */
    int size;
    int code = showBits(5);
    if (code<31) {
      size = DCchromtab0[code].val;
      flushBuffer(DCchromtab0[code].len);
      }
    else {
      code = showBits(10) - 0x3e0;
      size = DCchromtab1[code].val;
      flushBuffer(DCchromtab1[code].len);
      }

    int dct_diff;
    if (size == 0)
      dct_diff = 0;
    else {
      dct_diff = getBits(size);
      if ((dct_diff & (1<<(size-1)))==0)
        dct_diff-= (1<<size) - 1;
      }

    return dct_diff;
    }
  //}}}
  //{{{
  /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
  void macroBlockModes (int* pmacroblock_type, int* pstwtype, int* pstwclass,
                        int* pmotion_type, int* pmotion_vector_count, int* pmv_format,
                        int* pdmv, int* pmvscale, int* pdct_type) {

    int motion_type = 0;
    int motion_vector_count, mv_format, dmv, mvscale;
    int dct_type;

    /* get macroblock_type */
    int macroblock_type = getMacroBlockType();

    if (Fault_Flag)
      return;

    /* get spatial_temporal_weight_code */
    int stwtype, stwcode, stwclass;
    if (macroblock_type & MB_WEIGHT) {
      if (spatial_temporal_weight_code_table_index==0)
        stwtype = 4;
      else {
        stwcode = getBits(2);
        stwtype = stwc_table[spatial_temporal_weight_code_table_index-1][stwcode];
        }
      }
    else
      stwtype = (macroblock_type & MB_CLASS4) ? 8 : 0;

    /* SCALABILITY: derive spatial_temporal_weight_class (Table 7-18) */
    stwclass = stwclass_table [stwtype];

    /* get frame/field motion type */
    if (macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD)) {
      if (picture_structure == FRAME_PICTURE) /* frame_motion_type */
        motion_type = frame_pred_frame_dct ? MC_FRAME : getBits(2);
      else /* field_motion_type */
        motion_type = getBits(2);
      }
    else if ((macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
      /* concealment motion vectors */
      motion_type = (picture_structure == FRAME_PICTURE) ? MC_FRAME : MC_FIELD;

    /* derive motion_vector_count, mv_format and dmv, (table 6-17, 6-18) */
    if (picture_structure == FRAME_PICTURE) {
      motion_vector_count = (motion_type == MC_FIELD && stwclass < 2) ? 2 : 1;
      mv_format = (motion_type == MC_FRAME) ? MV_FRAME : MV_FIELD;
      }
    else {
      motion_vector_count = (motion_type == MC_16X8) ? 2 : 1;
      mv_format = MV_FIELD;
      }

    dmv = (motion_type==MC_DMV); /* dual prime */
    /* field mv predictions in frame pictures have to be scaled
     * ISO/IEC 13818-2 section 7.6.3.1 Decoding the motion vectors
     * IMPLEMENTATION: mvscale is derived for later use in motion_vectors()
     * it displaces the stage:
     *    if((mv_format=="field")&&(t==1)&&(picture_structure=="Frame picture"))
     *      prediction = PMV[r][s][t] DIV 2; */
    mvscale = ((mv_format == MV_FIELD) && (picture_structure == FRAME_PICTURE));

    /* get dct_type (frame DCT / field DCT) */
    dct_type = (picture_structure == FRAME_PICTURE)
                && (!frame_pred_frame_dct) && (macroblock_type & (MACROBLOCK_PATTERN | MACROBLOCK_INTRA))
                 ? getBits(1) : 0;

    /* return values */
    *pmacroblock_type = macroblock_type;
    *pstwtype = stwtype;
    *pstwclass = stwclass;
    *pmotion_type = motion_type;
    *pmotion_vector_count = motion_vector_count;
    *pmv_format = mv_format;
    *pdmv = dmv;
    *pmvscale = mvscale;
    *pdct_type = dct_type;
    }
  //}}}
  //}}}
  //{{{  extension and user data
  //{{{
  void sequence_extension() {
  /* decode sequence extension  ISO/IEC 13818-2 section 6.2.2.3 */

    int horizontal_size_extension;
    int vertical_size_extension;
    int bit_rate_extension;
    int vbv_buffer_size_extension;

    scalable_mode = SC_NONE; /* unless overwritten by sequence_scalable_extension() */
    layer_id = 0;                /* unless overwritten by sequence_scalable_extension() */

    profile_and_level_indication = getBits(8);
    progressive_sequence         = getBits(1);
    chroma_format                = getBits(2);
    horizontal_size_extension    = getBits(2);
    vertical_size_extension      = getBits(2);
    bit_rate_extension           = getBits(12);
    flushBuffer (1);
    vbv_buffer_size_extension    = getBits(8);
    low_delay                    = getBits(1);
    frame_rate_extension_n       = getBits(2);
    frame_rate_extension_d       = getBits(5);

    frame_rate = frame_rate_Table[frame_rate_code] * ((frame_rate_extension_n+1)/(frame_rate_extension_d+1));

    /* special case for 422 profile & level must be made */
    if ((profile_and_level_indication >> 7) & 1) {
      /* escape bit of profile_and_level_indication set */
      /* 4:2:2 Profile @ Main Level */
      if ((profile_and_level_indication & 15) == 5) {
        profile = PROFILE_422;
        level   = MAIN_LEVEL;
        }
      }
    else {
      profile = profile_and_level_indication >> 4;  /* Profile is upper nibble */
      level   = profile_and_level_indication & 0xF;  /* Level is lower nibble */
      }

    horizontal_size = (horizontal_size_extension<<12) | (horizontal_size&0x0fff);
    vertical_size = (vertical_size_extension<<12) | (vertical_size&0x0fff);

    /* ISO/IEC 13818-2 does not define bit_rate_value to be composed of
     * both the original bit_rate_value parsed in sequence_header() and
     * the optional bit_rate_extension in sequence_extension_header().
     * However, we use it for bitstream verification purposes.*/
    bit_rate_value += (bit_rate_extension << 18);
    bit_rate = ((double) bit_rate_value) * 400.0;
    vbv_buffer_size += (vbv_buffer_size_extension << 10);
    }
  //}}}
  //{{{
  /* decode sequence display extension */
  void sequence_display_extension() {

    int pos = Bitcnt;
    video_format = getBits(3);
    color_description = getBits(1);

    if (color_description) {
      color_primaries = getBits(8);
      transfer_characteristics = getBits(8);
      matrix_coefficients = getBits(8);
      }

    display_horizontal_size = getBits(14);
    flushBuffer (1);
    display_vertical_size = getBits(14);
    }
  //}}}
  //{{{
  void quant_matrix_extension() {
  /* decode quant matrix entension  ISO/IEC 13818-2 section 6.2.3.2 */

    int i;
    int pos = Bitcnt;
    if ((load_intra_quantizer_matrix = getBits(1))) {
      for (i = 0; i < 64; i++) {
        chroma_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = intra_quantizer_matrix[scan[ZIG_ZAG][i]] = getBits(8);
        }
      }

    if ((load_non_intra_quantizer_matrix = getBits(1))) {
      for (i = 0; i < 64; i++) {
        chroma_non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = getBits(8);
        }
      }

    if ((load_chroma_intra_quantizer_matrix = getBits(1))) {
      for (i = 0; i < 64; i++)
        chroma_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = getBits(8);
      }

    if ((load_chroma_non_intra_quantizer_matrix = getBits(1))) {
      for (i = 0; i < 64; i++)
        chroma_non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = getBits(8);
      }
    }
  //}}}
  //{{{
  void sequence_scalable_extension() {
  /* decode sequence scalable extension ISO/IEC 13818-2   section 6.2.2.5 */

    int pos = Bitcnt;

    /* values (without the +1 offset) of scalable_mode are defined in Table 6-10 of ISO/IEC 13818-2 */
    scalable_mode = getBits(2) + 1; /* add 1 to make SC_DP != SC_NONE */

    layer_id = getBits(4);
    if (scalable_mode == SC_SPAT) {
      lower_layer_prediction_horizontal_size = getBits(14);
      flushBuffer (1);
      lower_layer_prediction_vertical_size   = getBits(14);
      horizontal_subsampling_factor_m        = getBits(5);
      horizontal_subsampling_factor_n        = getBits(5);
      vertical_subsampling_factor_m          = getBits(5);
      vertical_subsampling_factor_n          = getBits(5);
      }

    if (scalable_mode == SC_TEMP)
      printf ("temporal scalability not implemented\n");
    }
  //}}}
  //{{{
  void picture_display_extension() {
  /* decode picture display extension ISO/IEC 13818-2 section 6.2.3.3. */

    int i;
    int number_of_frame_center_offsets;
    int pos;

    pos = Bitcnt;
    /* based on ISO/IEC 13818-2 section 6.3.12 (November 1994) Picture display extensions */
    /* derive number_of_frame_center_offsets */
    if (progressive_sequence) {
      if (repeat_first_field) {
        if (top_field_first)
          number_of_frame_center_offsets = 3;
        else
          number_of_frame_center_offsets = 2;
        }
      else
        number_of_frame_center_offsets = 1;
      }
    else {
      if (picture_structure!=FRAME_PICTURE)
        number_of_frame_center_offsets = 1;
      else {
        if(repeat_first_field)
          number_of_frame_center_offsets = 3;
        else
          number_of_frame_center_offsets = 2;
        }
      }

    /* now parse */
    for (i = 0; i < number_of_frame_center_offsets; i++) {
      frame_center_horizontal_offset[i] = getBits(16);
      flushBuffer (1);
      frame_center_vertical_offset[i] = getBits(16);
      flushBuffer (1);
      }
    }

  //}}}
  //{{{
  void picture_coding_extension() {
  /* decode picture coding extension */

    int pos = Bitcnt;

    f_code[0][0] = getBits(4);
    f_code[0][1] = getBits(4);
    f_code[1][0] = getBits(4);
    f_code[1][1] = getBits(4);

    intra_dc_precision         = getBits(2);
    picture_structure          = getBits(2);
    top_field_first            = getBits(1);
    frame_pred_frame_dct       = getBits(1);
    concealment_motion_vectors = getBits(1);
    q_scale_type           = getBits(1);
    intra_vlc_format           = getBits(1);
    alternate_scan         = getBits(1);
    repeat_first_field         = getBits(1);
    chroma_420_type            = getBits(1);
    progressive_frame          = getBits(1);
    composite_display_flag     = getBits(1);

    if (composite_display_flag) {
      v_axis            = getBits(1);
      field_sequence    = getBits(3);
      sub_carrier       = getBits(1);
      burst_amplitude   = getBits(7);
      sub_carrier_phase = getBits(8);
      }
    }
  //}}}
  //{{{
  void picture_spatial_scalable_extension() {
  /* decode picture spatial scalable extension ISO/IEC 13818-2 section 6.2.3.5. */

    int pos = Bitcnt;
    pict_scal = 1; /* use spatial scalability in this picture */

    lower_layer_temporal_reference = getBits (10);
    flushBuffer (1);

    lower_layer_horizontal_offset = getBits (15);
    if (lower_layer_horizontal_offset >= 16384)
      lower_layer_horizontal_offset -= 32768;

    flushBuffer (1);
    lower_layer_vertical_offset = getBits(15);
    if (lower_layer_vertical_offset >= 16384)
      lower_layer_vertical_offset -= 32768;

    spatial_temporal_weight_code_table_index = getBits(2);
    lower_layer_progressive_frame = getBits(1);
    lower_layer_deinterlaced_field_select = getBits(1);
    }
  //}}}
  //{{{
  void picture_temporal_scalable_extension() {
  /* decode picture temporal scalable extension not implemented ISO/IEC 13818-2 section 6.2.3.4. */

    printf ("temporal scalability not supported\n");
    }
  //}}}
  //{{{
  void copyright_extension() {
  /* Copyright extension  ISO/IEC 13818-2 section 6.2.3.6. (header added in November, 1994 to the IS document) */

    int pos = Bitcnt;

    copyright_flag = getBits(1);
    copyright_identifier = getBits(8);
    original_or_copy = getBits(1);

    int reserved_data = getBits(7);

    flushBuffer (1);
    copyright_number_1 =   getBits(20);

    flushBuffer (1);
    copyright_number_2 =   getBits(22);

    flushBuffer (1);
    copyright_number_3 =   getBits(22);
  }
  //}}}
  //{{{
  /* decode extension and user data  ISO/IEC 13818-2 section 6.2.2.2 */
  void extension_and_user_data() {

    unsigned int code = showNextStartCode();
    while (code == EXTENSION_START_CODE || code == USER_DATA_START_CODE) {
      if (code == EXTENSION_START_CODE) {
        flushBuffer32();
        int ext_ID = getBits (4);
        switch (ext_ID) {
        case SEQUENCE_EXTENSION_ID:
          sequence_extension();
          break;
        case SEQUENCE_DISPLAY_EXTENSION_ID:
          sequence_display_extension();
          break;
        case QUANT_MATRIX_EXTENSION_ID:
          quant_matrix_extension();
          break;
        case SEQUENCE_SCALABLE_EXTENSION_ID:
          sequence_scalable_extension();
          break;
        case PICTURE_DISPLAY_EXTENSION_ID:
          picture_display_extension();
          break;
        case PICTURE_CODING_EXTENSION_ID:
          picture_coding_extension();
          break;
        case PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
          picture_spatial_scalable_extension();
          break;
        case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
          picture_temporal_scalable_extension();
          break;
        case COPYRIGHT_EXTENSION_ID:
          copyright_extension();
          break;
       default:
          printf ("reserved extension start code ID %d\n", ext_ID);
          break;
          }
        }
      else /* userData ISO/IEC 13818-2  sections 6.3.4.1 and 6.2.2.2.2 skip ahead to the next start code */
        flushBuffer32();

      code = showNextStartCode();
      }
    }
  //}}}
  //}}}
  //{{{  getHeader
  //{{{
  void sequenceHeader() {

    int i;
    int pos = Bitcnt;

    horizontal_size = getBits (12);
    vertical_size = getBits (12);
    aspect_ratio_information = getBits (4);
    frame_rate_code = getBits (4);
    bit_rate_value = getBits (18);
    flushBuffer (1);
    vbv_buffer_size = getBits (10);
    constrained_parameters_flag = getBits (1);

    if ((load_intra_quantizer_matrix = getBits (1))) {
      for (i = 0; i < 64; i++)
        intra_quantizer_matrix[scan[ZIG_ZAG][i]] = getBits (8);
      }
    else {
      for (i = 0; i < 64; i++)
        intra_quantizer_matrix[i] = default_intra_quantizer_matrix[i];
      }

    if ((load_non_intra_quantizer_matrix = getBits(1))) {
      for (i = 0; i < 64; i++)
        non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = getBits (8);
      }
    else {
      for (i = 0; i < 64; i++)
        non_intra_quantizer_matrix[i] = 16;
      }

    /* copy luminance to chrominance matrices */
    for (i = 0; i < 64; i++) {
      chroma_intra_quantizer_matrix[i] = intra_quantizer_matrix[i];
      chroma_non_intra_quantizer_matrix[i] = non_intra_quantizer_matrix[i];
      }

    extension_and_user_data();
    }
  //}}}
  //{{{
  /* decode group of pictures header  ISO/IEC 13818-2 section 6.2.2.6 */
  void groupOfPicturesHeader() {

    int pos = Bitcnt;
    drop_flag = getBits (1);
    hour = getBits (5);
    minute = getBits (6);
    flushBuffer (1);
    sec = getBits (6);
    frame = getBits (6);
    closed_gop = getBits (1);
    broken_link = getBits (1);

    extension_and_user_data();
    }
  //}}}
  //{{{
  /* decode picture header ISO/IEC 13818-2 section 6.2.3 */
  void pictureHeader() {

    /* unless later overwritten by picture_spatial_scalable_extension() */
    pict_scal = 0;

    int pos = Bitcnt;
    temporal_reference = getBits (10);
    picture_coding_type = getBits (3);
    vbv_delay = getBits (16);

    if (picture_coding_type == P_TYPE || picture_coding_type == B_TYPE) {
      full_pel_forward_vector = getBits(1);
      forward_f_code = getBits(3);
      }

    if (picture_coding_type == B_TYPE) {
      full_pel_backward_vector = getBits(1);
      backward_f_code = getBits(3);
      }

    int Extra_Information_Byte_Count = extraBitInformation();
    extension_and_user_data();
    }
  //}}}
  //{{{
  unsigned int getNextHeader (bool reportUnexpected) {

    showNextStartCode();
    unsigned int code = getBits32();

    switch (code) {
      case 0x1B3 : // SEQUENCE_HEADER_CODE:
        //printf ("getNextHeader %x sequenceHeader\n", code);
        sequenceHeader();
        break;

      case 0x1B8 : // GROUP_START_CODE:
        //printf ("getNextHeader %08x groupOfPicturesHeader\n", code);
        groupOfPicturesHeader();
        break;

      case 0x100 : // PICTURE_START_CODE:
        // printf ("getNextHeader %08x pictureHeader\n", code);
        pictureHeader();
        break;

      case 0x1B7: // SEQUENCE_END_CODE:
        printf ("getNextHeader %08x SEQUENCE_END_CODE\n", code);
        break;

      default:
        if (reportUnexpected)
          printf ("getNextHeader unexpected %08x\n", code);
        break;
      }

    return code;
    }
  //}}}
  //}}}

  //{{{
  /* decode one intra coded MPEG-2 block */
  void decodeIntraBlock (int comp, int dc_dct_pred[]) {

    int val, sign, run;
    unsigned int code;
    DCTtab* tab;

    /* ISO/IEC 13818-2 section 7.2.1: decode DC coefficients */
    int cc = (comp < 4) ? 0 : (comp & 1) + 1;
    if (cc == 0)
      val = (dc_dct_pred[0]+= getLumaDCdctDiff());
    else if (cc==1)
      val = (dc_dct_pred[1]+= getChromaDCdctDiff());
    else
      val = (dc_dct_pred[2]+= getChromaDCdctDiff());

    if (Fault_Flag)
      return;

    block[comp][0] = val << (3-intra_dc_precision);
    int nc = 0;

    /* decode AC coefficients */
    for (int i = 1; ; i++) {
      code = showBits (16);
      if (code>=16384 && !intra_vlc_format)
        tab = &DCTtabnext[(code>>12)-4];
      else if (code >= 1024) {
        if (intra_vlc_format)
          tab = &DCTtab0a[(code>>8)-4];
        else
          tab = &DCTtab0[(code>>8)-4];
        }
      else if (code >= 512) {
        if (intra_vlc_format)
          tab = &DCTtab1a[(code>>6)-8];
        else
          tab = &DCTtab1[(code>>6)-8];
        }
      else if (code >= 256)
        tab = &DCTtab2[(code>>4)-16];
      else if (code >= 128)
        tab = &DCTtab3[(code>>3)-16];
      else if (code >= 64)
        tab = &DCTtab4[(code>>2)-16];
      else if (code >= 32)
        tab = &DCTtab5[(code>>1)-16];
      else if (code >= 16)
        tab = &DCTtab6[code-16];
      else {
        printf("invalid Huffman code in Decode_MPEG2_Intra_Block()\n");
        Fault_Flag = 1;
        return;
        }

      flushBuffer (tab->len);

      if (tab->run == 64) /* end_of_block */
        return;

      if (tab->run == 65) /* escape */ {
        i += run = getBits (6);
        val = getBits (12);
        if ((val&2047) == 0) {
          printf("invalid escape in Decode_MPEG2_Intra_Block()\n");
          Fault_Flag = 1;
          return;
          }
        if ((sign = (val >= 2048)))
          val = 4096 - val;
        }
      else {
        i+= run = tab->run;
        val = tab->level;
        sign = getBits (1);
      }

      if (i >= 64) {
        printf ("DCT coeff index (i) out of bounds (intra2)\n");
        Fault_Flag = 1;
        return;
        }

      int j = scan[alternate_scan][i];
      val = (val * quantizer_scale * intra_quantizer_matrix[j]) >> 4;
      block[comp][j] = sign ? -val : val;
      nc++;
      }
    }
  //}}}
  //{{{
  /* decode one non-intra coded MPEG-2 block */
  void decodeNonIntraBlock (int comp) {

    int val, sign, nc, run;
    nc = 0;
    /* decode AC coefficients */
    for (int i = 0; ; i++) {
      DCTtab* tab;
      unsigned int code = showBits (16);
      if (code>=16384) {
        if (i == 0)
          tab = &DCTtabfirst[(code>>12)-4];
        else
          tab = &DCTtabnext[(code>>12)-4];
        }
      else if (code >= 1024)
        tab = &DCTtab0[(code>>8)-4];
      else if (code >= 512)
        tab = &DCTtab1[(code>>6)-8];
      else if (code >= 256)
        tab = &DCTtab2[(code>>4)-16];
      else if (code >= 128)
        tab = &DCTtab3[(code>>3)-16];
      else if (code >= 64)
        tab = &DCTtab4[(code>>2)-16];
      else if (code >= 32)
        tab = &DCTtab5[(code>>1)-16];
      else if (code >= 16)
        tab = &DCTtab6[code-16];
      else {
        printf("invalid Huffman code in Decode_MPEG2_Non_Intra_Block()\n");
        Fault_Flag = 1;
        return;
        }

      flushBuffer (tab->len);

      if (tab->run == 64) /* end_of_block */
        return;

      if (tab->run == 65) /* escape */ {
        i += run = getBits (6);
        val = getBits (12);
        if ((val & 2047) == 0) {
          printf ("invalid escape in Decode_MPEG2_Intra_Block()\n");
          Fault_Flag = 1;
          return;
          }
        if ((sign = (val >= 2048)))
          val = 4096 - val;
        }
      else {
        i += run = tab->run;
        val = tab->level;
        sign = getBits (1);
        }

      if (i >= 64) {
        printf ("DCT coeff index (i) out of bounds (inter2)\n");
        Fault_Flag = 1;
        return;
        }

      int j = scan[alternate_scan][i];
      val = (((val << 1) + 1) * quantizer_scale * non_intra_quantizer_matrix[j]) >> 5;
      block[comp][j] = sign ? -val : val;
      nc++;
      }
    }
  //}}}

  //{{{
  void decodeMotionVector (int* pred, int r_size, int motion_code, int motion_residual, int full_pel_vector) {
  // calculate motion vector component  ISO/IEC 13818-2 section 7.6.3.1: Decoding the motion vectors

    int lim = 16 << r_size;
    int vec = full_pel_vector ? (*pred >> 1) : (*pred);

    if (motion_code>0) {
      vec += ((motion_code-1) << r_size) + motion_residual + 1;
      if (vec >= lim)
        vec -= lim + lim;
      }
    else if (motion_code<0) {
      vec -= ((-motion_code-1) << r_size) + motion_residual + 1;
      if (vec < -lim)
        vec += lim + lim;
      }

    *pred = full_pel_vector ? (vec << 1) : vec;
    }
  //}}}
  //{{{
  /* get and decode motion vector and differential motion vector for one prediction */
  void motionVector (int* PMV, int* dmvector, int h_r_size, int v_r_size, int dmv, int mvscale, int full_pel_vector) {

    /* horizontal component ISO/IEC 13818-2 Table B-10 */
    int motion_code = getMotionCode();
    int motion_residual = (h_r_size!=0 && motion_code!=0) ? getBits(h_r_size) : 0;
    decodeMotionVector (&PMV[0], h_r_size, motion_code, motion_residual, full_pel_vector);

    if (dmv)
      dmvector[0] = getDmvector();

    /* vertical component */
    motion_code = getMotionCode();
    motion_residual = (v_r_size!=0 && motion_code!=0) ? getBits(v_r_size) : 0;

    if (mvscale)
      PMV[1] >>= 1; /* DIV 2 */

    decodeMotionVector (&PMV[1], v_r_size, motion_code, motion_residual, full_pel_vector);

    if (mvscale)
      PMV[1] <<= 1;

    if (dmv)
      dmvector[1] = getDmvector();
    }
  //}}}
  //{{{
  /* ISO/IEC 13818-2 sections 6.2.5.2, 6.3.17.2, and 7.6.3: Motion vectors */
  void motionVectors (int PMV[2][2][2], int dmvector[2], int motion_vertical_field_select[2][2],
                      int s, int motion_vector_count, int mv_format, int h_r_size, int v_r_size, int dmv, int mvscale) {

    if (motion_vector_count == 1) {
      if (mv_format == MV_FIELD && !dmv)
        motion_vertical_field_select[1][s] = motion_vertical_field_select[0][s] = getBits(1);
      motionVector (PMV[0][s], dmvector, h_r_size, v_r_size, dmv, mvscale, 0);

      /* update other motion vector predictors */
      PMV[1][s][0] = PMV[0][s][0];
      PMV[1][s][1] = PMV[0][s][1];
      }

    else {
      motion_vertical_field_select[0][s] = getBits(1);
      motionVector (PMV[0][s], dmvector, h_r_size, v_r_size, dmv, mvscale, 0);
      motion_vertical_field_select[1][s] = getBits(1);
      motionVector (PMV[1][s], dmvector, h_r_size, v_r_size, dmv, mvscale, 0);
      }
    }
  //}}}
  //{{{
  /* ISO/IEC 13818-2 section 7.6.3.6: Dual prime additional arithmetic */
  void dualPrimeArithmetic (int DMV[][2], int* dmvector, int mvx, int mvy) {

    if (picture_structure == FRAME_PICTURE) {
      if (top_field_first) {
        /* vector for prediction of top field from bottom field */
        DMV[0][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
        DMV[0][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] - 1;

        /* vector for prediction of bottom field from top field */
        DMV[1][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
        DMV[1][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] + 1;
        }
      else {
        /* vector for prediction of top field from bottom field */
        DMV[0][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
        DMV[0][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] - 1;

        /* vector for prediction of bottom field from top field */
        DMV[1][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
        DMV[1][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] + 1;
        }
      }
    else {
      /* vector for prediction from field of opposite 'parity' */
      DMV[0][0] = ((mvx+(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((mvy+(mvy>0))>>1) + dmvector[1];

      /* correct for vertical field shift */
      if (picture_structure==TOP_FIELD)
        DMV[0][1]--;
      else
        DMV[0][1]++;
      }
    }
  //}}}

  //{{{
  void formComponentPrediction (unsigned char* src, unsigned char* dst, int lx, int lx2,
                                  int w, int h, int x, int y, int dx, int dy, int average_flag) {
  //{{{  description
  /* ISO/IEC 13818-2 section 7.6.4: Forming predictions */
  /* NOTE: the arithmetic below produces numerically equivalent results
   *  to 7.6.4, yet is more elegant. It differs in the following ways:
   *
   *   1. the vectors (dx, dy) are based on cartesian frame
   *      coordiantes along a half-pel grid (always positive numbers)
   *      In contrast, vector[r][s][t] are differential (with positive and
   *      negative values). As a result, deriving the integer vectors
   *      (int_vec[t]) from dx, dy is accomplished by a simple right shift.
   *
   *   2. Half pel flags (xh, yh) are equivalent to the LSB (Least
   *      Significant Bit) of the half-pel coordinates (dx,dy).
   *  NOTE: the work of combining predictions (ISO/IEC 13818-2 section 7.6.7)
   *  is distributed among several other stages.  This is accomplished by
   *  folding line offsets into the source and destination (src,dst)
   *  addresses (note the call arguments to form_prediction() in Predict()),
   *  line stride variables lx and lx2, the block dimension variables (w,h),
   *  average_flag, and by the very order in which Predict() is called.
   *  This implementation design (implicitly different than the spec) was chosen for its elegance. */
  //}}}

    // half pel scaling for integer vectors
    int xint = dx>>1;
    int yint = dy>>1;

    // derive half pel flags
    int xh = dx & 1;
    int yh = dy & 1;

    // compute the linear address of pel_ref[][] and pel_pred[][] based on cartesian/raster cordinates provided
    unsigned char* s = src + lx * (y + yint) + x + xint;
    unsigned char* d = dst + lx * y + x;

    if (!xh && !yh) {
      //{{{  no horizontal nor vertical half-pel
      if (average_flag) {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++) {
            int v = d[i] + s[i];
            d[i] = (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = s[i];
          s += lx2;
          d += lx2;
          }
        }
      }
      //}}}
    else if (!xh && yh) {
      //{{{  no horizontal but vertical half-pel
      if (average_flag) {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++) {
            int v = d[i] + ((unsigned int)(s[i] + s[i+lx] +1) >>1);
            d[i]= (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = (unsigned int)(s[i] + s[i+lx] + 1) >> 1;
          s += lx2;
          d += lx2;
          }
        }
      }
      //}}}
    else if (xh && !yh) {
      //{{{  horizontal but no vertical half-pel
      if (average_flag) {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++) {
            int v = d[i] + ((unsigned int)(s[i] + s[i+1] + 1) >> 1);
            d[i] = (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = (unsigned int)(s[i] + s[i+1] + 1) >> 1;
          s += lx2;
          d += lx2;
          }
        }
      }
      //}}}
    else {
      //{{{  if (xh && yh) horizontal and vertical half-pel
      if (average_flag) {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++) {
            int v = d[i] + ((unsigned int)(s[i] + s[i+1] + s[i+lx] + s[i+lx+1] + 2) >> 2);
            d[i] = (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = (unsigned int)(s[i] + s[i+1] + s[i+lx] + s[i+lx+1] + 2) >> 2;
          s += lx2;
          d += lx2;
          }
        }
      }
      //}}}
    }
  //}}}
  //{{{
  void formPrediction (unsigned char* src[], int sfield, unsigned char* dst[], int dfield,
                       int lx, int lx2, int w, int h, int x, int y, int dx, int dy, int average_flag) {

    /* Y */
    formComponentPrediction (src[0] + (sfield?lx2>>1:0), dst[0] + (dfield?lx2>>1:0), lx, lx2, w, h, x, y, dx, dy, average_flag);

    lx >>= 1;
    lx2 >>= 1;
    w >>= 1;
    x >>= 1;
    dx /= 2;
    h >>= 1;
    y >>= 1;
    dy /= 2;

    /* Cb */
    formComponentPrediction (src[1] + (sfield?lx2>>1:0), dst[1] + (dfield?lx2>>1:0), lx, lx2, w, h, x, y, dx, dy, average_flag);
    /* Cr */
    formComponentPrediction (src[2] + (sfield?lx2>>1:0), dst[2] + (dfield?lx2>>1:0), lx, lx2, w, h, x, y, dx, dy, average_flag);
  }
  //}}}
  //{{{
  void formPredictions (int bx, int by, int macroblock_type, int motion_type, int PMV[2][2][2],
                        int motion_vertical_field_select[2][2], int dmvector[2], int stwtype) {

    int currentfield;
    unsigned char** predframe;
    int DMV[2][2];

    // 0:temporal, 1:(spat+temp)/2, 2:spatial
    int stwtop = stwtype % 3;
    int stwbot = stwtype / 3;

    if ((macroblock_type & MACROBLOCK_MOTION_FORWARD) || (picture_coding_type == P_TYPE)) {
      if (picture_structure == FRAME_PICTURE) {
        if ((motion_type == MC_FRAME) || !(macroblock_type & MACROBLOCK_MOTION_FORWARD)) {
          //{{{  frame-based prediction (broken into top and bottom halves for spatial scalability prediction purposes)
          if (stwtop < 2)
            formPrediction(forward_reference_frame,0,current_frame,0,
              Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by, PMV[0][0][0],PMV[0][0][1],stwtop);

          if (stwbot < 2)
            formPrediction(forward_reference_frame,1,current_frame,1,
              Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by, PMV[0][0][0],PMV[0][0][1],stwbot);
          }
          //}}}
        else if (motion_type == MC_FIELD) {
          //{{{  top field prediction
          if (stwtop < 2)
            formPrediction(forward_reference_frame,motion_vertical_field_select[0][0],
              current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8, bx,by>>1,PMV[0][0][0],PMV[0][0][1]>>1,stwtop);

          /* bottom field prediction */
          if (stwbot < 2)
            formPrediction(forward_reference_frame,motion_vertical_field_select[1][0],
              current_frame,1,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8, bx,by>>1,PMV[1][0][0],PMV[1][0][1]>>1,stwbot);
          }
          //}}}
        else if (motion_type == MC_DMV) {
          //{{{  calculate derived motion vectors
          dualPrimeArithmetic (DMV,dmvector,PMV[0][0][0],PMV[0][0][1]>>1);

          if (stwtop < 2) {
            /* predict top field from top field */
            formPrediction(forward_reference_frame,0,current_frame,0,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1, PMV[0][0][0],PMV[0][0][1]>>1,0);

            /* predict and add to top field from bottom field */
            formPrediction(forward_reference_frame,1,current_frame,0,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1, DMV[0][0],DMV[0][1],1);
            }

          if (stwbot<2) {
            /* predict bottom field from bottom field */
            formPrediction(forward_reference_frame,1,current_frame,1,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1, PMV[0][0][0],PMV[0][0][1]>>1,0);

            /* predict and add to bottom field from top field */
            formPrediction(forward_reference_frame,0,current_frame,1,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1, DMV[1][0],DMV[1][1],1);
            }
          }
          //}}}
        else
          printf ("invalid motion_type\n");
        }
      else {
        //{{{  field picture
        currentfield = (picture_structure==BOTTOM_FIELD);

        /* determine which frame to use for prediction */
        if ((picture_coding_type==P_TYPE) && Second_Field && (currentfield!=motion_vertical_field_select[0][0]))
          predframe = backward_reference_frame; /* same frame */
        else
          predframe = forward_reference_frame; /* previous frame */

        if ((motion_type==MC_FIELD) || !(macroblock_type & MACROBLOCK_MOTION_FORWARD)) {
          //{{{  field-based prediction */
          if (stwtop<2)
            formPrediction(predframe,motion_vertical_field_select[0][0],current_frame,0,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,bx,by,
              PMV[0][0][0],PMV[0][0][1],stwtop);
          }
          //}}}
        else if (motion_type==MC_16X8) {
          if (stwtop<2) {
            formPrediction(predframe,motion_vertical_field_select[0][0],current_frame,0,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by,
              PMV[0][0][0],PMV[0][0][1],stwtop);

            /* determine which frame to use for lower half prediction */
            if ((picture_coding_type==P_TYPE) && Second_Field && (currentfield!=motion_vertical_field_select[1][0]))
              predframe = backward_reference_frame; /* same frame */
            else
              predframe = forward_reference_frame; /* previous frame */

            formPrediction(predframe,motion_vertical_field_select[1][0],current_frame,0,
              Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by+8, PMV[1][0][0],PMV[1][0][1],stwtop);
            }
          }
        else if (motion_type==MC_DMV) {
          //{{{  dual prime prediction */
          if (Second_Field)
            predframe = backward_reference_frame; /* same frame */
          else
            predframe = forward_reference_frame; /* previous frame */

          /* calculate derived motion vectors */
          dualPrimeArithmetic (DMV,dmvector,PMV[0][0][0],PMV[0][0][1]);

          /* predict from field of same parity */
          formPrediction(forward_reference_frame,currentfield,current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,bx,by, PMV[0][0][0],PMV[0][0][1],0);

          /* predict from field of opposite parity */
          formPrediction(predframe,!currentfield,current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,bx,by, DMV[0][0],DMV[0][1],1);
          }
          //}}}
        else
          /* invalid motion_type */
          printf("invalid motion_type\n");
        }
        //}}}
      stwtop = stwbot = 1;
      }

    if (macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
      if (picture_structure == FRAME_PICTURE) {
        if (motion_type == MC_FRAME) {
          //{{{  frame-based prediction
          if (stwtop < 2)
            formPrediction(backward_reference_frame,0,current_frame,0,
              Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by, PMV[0][1][0],PMV[0][1][1],stwtop);

          if (stwbot < 2)
            formPrediction(backward_reference_frame,1,current_frame,1,
              Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by, PMV[0][1][0],PMV[0][1][1],stwbot);
          }
          //}}}
        else {
          //{{{  top field prediction
          if (stwtop<2)
            formPrediction(backward_reference_frame,motion_vertical_field_select[0][1],
              current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8, bx,by>>1,PMV[0][1][0],PMV[0][1][1]>>1,stwtop);
          //}}}
          //{{{  bottom field prediction
          if (stwbot<2)
            formPrediction(backward_reference_frame,motion_vertical_field_select[1][1],
              current_frame,1,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8, bx,by>>1,PMV[1][1][0],PMV[1][1][1]>>1,stwbot);
          //}}}
          }
        }
      else {
        //{{{  field picture
        if (motion_type==MC_FIELD) {
          /* field-based prediction */
          formPrediction(backward_reference_frame,motion_vertical_field_select[0][1],
            current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16, bx,by,PMV[0][1][0],PMV[0][1][1],stwtop);
          }
        else if (motion_type==MC_16X8) {
          formPrediction(backward_reference_frame,motion_vertical_field_select[0][1],
            current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8, bx,by,PMV[0][1][0],PMV[0][1][1],stwtop);

          formPrediction(backward_reference_frame,motion_vertical_field_select[1][1],
            current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8, bx,by+8,PMV[1][1][0],PMV[1][1][1],stwtop);
          }
        else
          /* invalid motion_type */
          printf("invalid motion_type\n");
        }
        //}}}
      }
    }
  //}}}
  //{{{
  void addBlock (int comp, int bx, int by, int dct_type, int addflag) {
  /* move/add 8x8-Block from block[comp] to backward_reference_frame */
  /* copy reconstructed 8x8 block from block[comp] to current_frame[]
   * ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data
   * This stage also embodies some of the operations implied by:
   *   - ISO/IEC 13818-2 section 7.6.7: Combining predictions
   *   - ISO/IEC 13818-2 section 6.1.3: Macroblock */

    int iincr;
    unsigned char* rfp;

    /* derive color component index equivalent to ISO/IEC 13818-2 Table 7-1 */
    int cc = (comp<4) ? 0 : (comp&1)+1; /* color component index */
    if (cc == 0) {
      //{{{  luminance
      if (picture_structure == FRAME_PICTURE)
        if (dct_type) {
          //  field DCT coding
          rfp = current_frame[0] + Coded_Picture_Width*(by+((comp&2)>>1)) + bx + ((comp&1)<<3);
          iincr = (Coded_Picture_Width<<1) - 8;
          }
        else {
          // frame DCT coding
          rfp = current_frame[0] + Coded_Picture_Width*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
          iincr = Coded_Picture_Width - 8;
          }
      else {
        // field picture
        rfp = current_frame[0] + (Coded_Picture_Width<<1)*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
        iincr = (Coded_Picture_Width<<1) - 8;
        }
      }
      //}}}
    else {
      //{{{  chrominance scale coordinates
      bx >>= 1;
      by >>= 1;
      if (picture_structure == FRAME_PICTURE) {
        // frame DCT coding
        rfp = current_frame[cc] + Chroma_Width*(by+((comp&2)<<2)) + bx + (comp&8);
        iincr = Chroma_Width - 8;
        }
      else {
        // field picture
        rfp = current_frame[cc] + (Chroma_Width<<1)*(by+((comp&2)<<2)) + bx + (comp&8);
        iincr = (Chroma_Width<<1) - 8;
        }
      }
      //}}}

    short* bp = block[comp];
    if (addflag) {
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          *rfp = Clip[*bp++ + *rfp];
          rfp++;
          }
        rfp += iincr;
        }
      }
    else {
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++)
          *rfp++ = Clip[*bp++ + 128];
        rfp += iincr;
        }
      }
    }
  //}}}
  //{{{
  void clearBlock (int comp) {
  /* IMPLEMENTATION: set scratch pad macroblock to zero */

    short* Block_Ptr = block[comp];
    for (int i = 0; i < 64; i++)
      *Block_Ptr++ = 0;
    }
  //}}}
  //{{{
  void motionCompensation (int MBA, int macroblock_type, int motion_type, int PMV[2][2][2],
                           int motion_vertical_field_select[2][2], int dmvector[2], int stwtype, int dct_type) {
  /* ISO/IEC 13818-2 section 7.6 */

    /* derive current macroblock position within picture ISO/IEC 13818-2 section 6.3.1.6 and 6.3.1.7 */
    int bx = 16 * (MBA % mb_width);
    int by = 16 * (MBA / mb_width);

    /* motion compensation */
    if (!(macroblock_type & MACROBLOCK_INTRA))
      formPredictions (bx, by, macroblock_type, motion_type, PMV, motion_vertical_field_select, dmvector, stwtype);

    /* copy or add block data into picture */
    for (int comp = 0; comp < block_count; comp++) {
      /* ISO/IEC 13818-2 section Annex A: inverse DCT */
      fastIDCT (block[comp]);

      /* ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data */
      addBlock (comp, bx, by, dct_type, (macroblock_type & MACROBLOCK_INTRA) == 0);
      }
    }
  //}}}

  //{{{
  /* ISO/IEC 13818-2 section 6.2.4 */
  int sliceHeader() {

    int pos = Bitcnt;
    int slice_vertical_position_extension = (vertical_size>2800) ? getBits (3) : 0;
    if (scalable_mode == SC_DP)
      priority_breakpoint = getBits (7);

    int quantizer_scale_code = getBits (5);
    quantizer_scale = q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : quantizer_scale_code<<1;

    /* slice_id introduced in March 1995 as part of the video corridendum (after the IS was drafted in November 1994) */
    int slice_picture_id_enable = 0;
    int slice_picture_id = 0;
    int extra_information_slice = 0;
    if (getBits(1)) {
      intra_slice = getBits (1);
      slice_picture_id_enable = getBits (1);
      slice_picture_id = getBits (6);
      extra_information_slice = extraBitInformation();
      }
    else
      intra_slice = 0;

    return slice_vertical_position_extension;
    }
  //}}}
  //{{{
  int startOfSlice (int MBAmax, int* MBA, int* MBAinc, int dc_dct_pred[3], int PMV[2][2][2]) {
  // return -1 means go to next picture

    Fault_Flag = 0;

    unsigned int code = showNextStartCode();
    if (code < SLICE_START_CODE_MIN || code > SLICE_START_CODE_MAX) {
      /* only slice headers are allowed in picture_data */
      printf ("start_of_slice(): Premature end of picture\n");
      return -1;  /* trigger: go to next picture */
      }
    flushBuffer32();

    /* decode slice header (may change quantizer_scale) */
    int slice_vert_pos_ext = sliceHeader();

    /* decode macroblock address increment */
    *MBAinc = getMacroBlockAddressIncrement();

    if (Fault_Flag) {
      printf ("start_of_slice(): MBAinc unsuccessful\n");
      return 0;  /* trigger: go to next slice */
      }

    /* set current location */
    // NOTE: the arithmetic used to derive macroblock_address is equivalent to ISO/IEC 13818-2 section 6.3.17: Macroblock
    *MBA = ((slice_vert_pos_ext<<7) + (code&255) - 1) * mb_width + *MBAinc - 1;
    *MBAinc = 1; /* first macroblock in slice: not skipped */

    /* reset all DC coefficient and motion vector predictors ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
    PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
    PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;

    /* successfull: trigger decode macroblocks in slice */
    return 1;
    }
  //}}}

  //{{{
  void skippedMacroblock (int dc_dct_pred[3], int PMV[2][2][2],
                          int* motion_type, int motion_vertical_field_select[2][2], int* stwtype, int* macroblock_type) {
  // ISO/IEC 13818-2 section 7.6.6

    for (int comp = 0; comp < block_count; comp++)
      clearBlock (comp);

    // reset intra_dc predictors  ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    // reset motion vector predictors  ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
    if (picture_coding_type == P_TYPE)
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

    // derive motion_type */
    if (picture_structure == FRAME_PICTURE)
      *motion_type = MC_FRAME;
    else {
      *motion_type = MC_FIELD;
      // predict from field of same parity ISO/IEC 13818-2 section 7.6.6.1 and 7.6.6.3: P field picture and B field picture
      motion_vertical_field_select[0][0] = motion_vertical_field_select[0][1] = (picture_structure == BOTTOM_FIELD);
      }

    // skipped I are spatial-only predicted, skipped P and B are temporal-only predicted ISO/IEC 13818-2 section 7.7.6:
    *stwtype = (picture_coding_type == I_TYPE) ? 8 : 0;

    // IMPLEMENTATION: clear MACROBLOCK_INTRA */
    *macroblock_type &= ~MACROBLOCK_INTRA;
    }
  //}}}
  //{{{
  /* ISO/IEC 13818-2 sections 7.2 through 7.5 */
  int decodeMacroblock (int* macroblock_type, int* stwtype, int* stwclass, int* motion_type, int* dct_type,
                        int PMV[2][2][2], int dc_dct_pred[3], int motion_vertical_field_select[2][2], int dmvector[2]) {

    int quantizer_scale_code;
    int comp;
    int motion_vector_count;
    int mv_format;
    int dmv;
    int mvscale;
    int coded_block_pattern;

    /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
    macroBlockModes (macroblock_type, stwtype, stwclass, motion_type, &motion_vector_count, &mv_format, &dmv, &mvscale, dct_type);

    if (Fault_Flag)
      return(0);  /* trigger: go to next slice */

    if (*macroblock_type & MACROBLOCK_QUANT) {
      quantizer_scale_code = getBits(5);
      /* ISO/IEC 13818-2 section 7.4.2.2: Quantizer scale factor */
      quantizer_scale = q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : (quantizer_scale_code << 1);
      }

    /* motion vectors ISO/IEC 13818-2 section 6.3.17.2: Motion vectors decode forward motion vectors */
    if ((*macroblock_type & MACROBLOCK_MOTION_FORWARD) || ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors))
      motionVectors (PMV,dmvector,motion_vertical_field_select,
                     0, motion_vector_count, mv_format, f_code[0][0]-1, f_code[0][1]-1, dmv,mvscale);

    if (Fault_Flag)
      return(0);  /* trigger: go to next slice */

    /* decode backward motion vectors */
    if (*macroblock_type & MACROBLOCK_MOTION_BACKWARD)
        motionVectors (PMV,dmvector, motion_vertical_field_select,
                       1, motion_vector_count, mv_format, f_code[1][0]-1, f_code[1][1]-1, 0, mvscale);

    if (Fault_Flag)
      return 0;  /* trigger: go to next slice */

    if ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
      flushBuffer (1); /* remove marker_bit */

    /* macroblock_pattern ISO/IEC 13818-2 section 6.3.17.4: Coded block pattern */
    if (*macroblock_type & MACROBLOCK_PATTERN)
      coded_block_pattern = getCodedBlockPattern();
    else
      coded_block_pattern = (*macroblock_type & MACROBLOCK_INTRA) ? (1 << block_count) - 1 : 0;

    if (Fault_Flag)
      return 0;  /* trigger: go to next slice */

    /* decode blocks */
    for (comp = 0; comp < block_count; comp++) {
      clearBlock (comp);
      if (coded_block_pattern & (1<<(block_count-1-comp))) {
        if (*macroblock_type & MACROBLOCK_INTRA)
          decodeIntraBlock (comp, dc_dct_pred);
        else
          decodeNonIntraBlock (comp);
        if (Fault_Flag)
          return 0;  /* trigger: go to next slice */
        }
      }

    if (picture_coding_type == D_TYPE)
      /* remove end_of_macroblock (always 1, prevents startcode emulation) ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
      flushBuffer (1);

    /* reset intra_dc predictors ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
    if (!(*macroblock_type & MACROBLOCK_INTRA))
      dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    /* reset motion vector predictors */
    if ((*macroblock_type & MACROBLOCK_INTRA) && !concealment_motion_vectors) {
      /* intra mb without concealment motion vectors ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
      PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
      }

    /* special "No_MC" macroblock_type case ISO/IEC 13818-2 section 7.6.3.5: Prediction in P pictures */
    if ((picture_coding_type == P_TYPE) && !(*macroblock_type & (MACROBLOCK_MOTION_FORWARD | MACROBLOCK_INTRA))) {
      /* non-intra mb without forward mv in a P picture ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

      /* derive motion_type ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes, frame_motion_type */
      if (picture_structure == FRAME_PICTURE)
        *motion_type = MC_FRAME;
      else {
        *motion_type = MC_FIELD;
        /* predict from field of same parity */
        motion_vertical_field_select[0][0] = (picture_structure == BOTTOM_FIELD);
        }
      }

    if (*stwclass == 4) {
      /* purely spatially predicted macroblock ISO/IEC 13818-2 section 7.7.5.1: Resetting motion vector predictions */
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
      PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
      }

    /* successfully decoded macroblock */
    return 1;
    }
  //}}}
  //{{{
  /* decode all macroblocks of the current picture ISO/IEC 13818-2 section 6.3.16 */
  int slice (int framenum, int MBAmax) {

    int MBA = 0;
    int MBAinc = 0;

    int dc_dct_pred[3];
    int PMV[2][2][2];
    int ret = startOfSlice (MBAmax, &MBA, &MBAinc, dc_dct_pred, PMV);
    if (ret != 1)
      return (ret);

    Fault_Flag = 0;
    for (;;) {
      /* this is how we properly exit out of picture */
      if (MBA >= MBAmax)
        return -1; /* all macroblocks decoded */
      if (MBAinc == 0) {
        if (!showBits(23) || Fault_Flag) {
          //* next_start_code or fault 
      resync: /* if Fault_Flag: resynchronize to next next_start_code */
          Fault_Flag = 0;
          return 0;     /* trigger: go to next slice */
          }
        else { 
          /* neither next_start_code nor Fault_Flag  decode macroblock address increment */
          MBAinc = getMacroBlockAddressIncrement();
          if (Fault_Flag)
            goto resync;
          }
        }

      if (MBA >= MBAmax) { /* MBAinc points beyond picture dimensions */
        printf ("Too many macroblocks in picture\n");
        return -1;
        }

      int macroblock_type, motion_type, dct_type;
      int motion_vertical_field_select[2][2];
      int dmvector[2];
      int stwtype, stwclass;
      if (MBAinc == 1) /* not skipped */ {
        ret = decodeMacroblock (&macroblock_type, &stwtype, &stwclass,
                                &motion_type, &dct_type, PMV, dc_dct_pred, motion_vertical_field_select, dmvector);
        if (ret == -1)
          return -1;
        if (ret == 0)
          goto resync;
        }
      else /* MBAinc!=1: skipped macroblock ISO/IEC 13818-2 section 7.6.6 */
        skippedMacroblock (dc_dct_pred, PMV, &motion_type, motion_vertical_field_select, &stwtype, &macroblock_type);

      /* ISO/IEC 13818-2 section 7.6 */
      motionCompensation (MBA, macroblock_type, motion_type, PMV, motion_vertical_field_select, dmvector, stwtype, dct_type);

      // next macroblock
      MBA++;
      MBAinc--;
      if (MBA >= MBAmax)
        return -1; /* all macroblocks decoded */
      }
    }
  //}}}

  //{{{
  void initSequence() {

    mb_width = (horizontal_size + 15) / 16;
    mb_height = (!progressive_sequence) ? 2 * ((vertical_size + 31) / 32) : (vertical_size + 15) / 16;
    Coded_Picture_Width = 16 * mb_width;
    Coded_Picture_Height = 16 * mb_height;
    Chroma_Width = Coded_Picture_Width >> 1;
    Chroma_Height = Coded_Picture_Height >> 1;
    block_count = Table_6_20 [chroma_format - 1];

    for (int cc = 0; cc < 3; cc++) {
      int size = (cc == 0) ? Coded_Picture_Width * Coded_Picture_Height : Chroma_Width * Chroma_Height;
      backward_reference_frame[cc] = (unsigned char*)malloc(size);
      forward_reference_frame[cc] = (unsigned char*)malloc(size);
      auxframe[cc] = (unsigned char*)malloc(size);
      }

    printf ("initSequence %d %d %d\n",   horizontal_size, vertical_size, block_count);
    }
  //}}}
  //{{{
  void decodePicture (int bitFrame, int seqFrame) {
  /* decode one frame or field picture */

    if (picture_structure == FRAME_PICTURE && Second_Field) {
      printf ("odd number of field pictures\n");
      Second_Field = false;
      }

    //{{{  updatePictureBuffers;
    for (int cc = 0; cc < 3; cc++) {
      /* B pictures do not need to be save for future reference */
      if (picture_coding_type == B_TYPE)
        current_frame[cc] = auxframe[cc];
      else {
        /* only update at the beginning of the coded frame */
        if (!Second_Field) {
          unsigned char* tmp = forward_reference_frame[cc];

          /* the previously decoded reference frame is stored coincident with the location where the backward
             reference frame is stored (backwards prediction is not needed in P pictures) */
          forward_reference_frame[cc] = backward_reference_frame[cc];

          /* update pointer for potential future B pictures */
          backward_reference_frame[cc] = tmp;
          }

        // can erase over old backward reference frame since it is not used
        // in a P picture, and since any subsequent B pictures will use the
        // previously decoded I or P frame as the backward_reference_frame */
        current_frame[cc] = backward_reference_frame[cc];
        }

      // one-time folding of a line offset into the pointer which stores the
      // memory address of the current frame saves offsets and conditional
      // branches throughout the remainder of the picture processing loop */
      if (picture_structure == BOTTOM_FIELD)
        current_frame[cc] += (cc == 0) ? Coded_Picture_Width : Chroma_Width;
      }
    //}}}

    // decode picture data ISO/IEC 13818-2 section 6.2.3.7
    while (slice (bitFrame, (picture_structure == FRAME_PICTURE) ? (mb_width * mb_height) : ((mb_width * mb_height)>>1)) >= 0);

    //{{{  reorderFrames - write or display current or previously decoded reference frame ISO/IEC 13818-2 section 6.1.1.11
    if (seqFrame != 0) {
      if (picture_structure == FRAME_PICTURE || Second_Field) {
        if (picture_coding_type == B_TYPE)
          writeFrame (auxframe, bitFrame-1,
                      progressive_sequence || progressive_frame, Coded_Picture_Width, Coded_Picture_Height, Chroma_Width);
        else {
          Newref_progressive_frame = progressive_frame;
          progressive_frame = Oldref_progressive_frame;
          writeFrame (forward_reference_frame, bitFrame-1,
                      progressive_sequence || progressive_frame, Coded_Picture_Width, Coded_Picture_Height, Chroma_Width);
          Oldref_progressive_frame = progressive_frame = Newref_progressive_frame;
          }
        }
      }
    else
      Oldref_progressive_frame = progressive_frame;
    //}}}

    if (picture_structure != FRAME_PICTURE)
      Second_Field = !Second_Field;
    }
  //}}}

  //{{{  vars
  int Coded_Picture_Width;
  int Coded_Picture_Height;
  int Chroma_Width;
  int Chroma_Height;
  int block_count;

  unsigned char* auxframe[3];
  unsigned char* current_frame[3];
  unsigned char* backward_reference_frame[3];
  unsigned char* forward_reference_frame[3];

  short* iclp = NULL;
  short iclip[1024];
  unsigned char* Clip = NULL;

  int True_Framenum = 0;
  int True_Framenum_max = -1;
  int Temporal_Reference_Base = 0;
  int Temporal_Reference_GOP_Reset = 0;

  int Infile = 0;
  unsigned char Rdbfr[2048];
  unsigned char* Rdptr = NULL;
  unsigned int Bfr = 0;
  unsigned char* Rdmax = NULL;
  int Incnt = 0;
  int Bitcnt = 0;

  int intra_quantizer_matrix[64];
  int non_intra_quantizer_matrix[64];
  int chroma_intra_quantizer_matrix[64];
  int chroma_non_intra_quantizer_matrix[64];

  int load_intra_quantizer_matrix = 0;
  int load_non_intra_quantizer_matrix = 0;
  int load_chroma_intra_quantizer_matrix= 0;
  int load_chroma_non_intra_quantizer_matrix= 0;

  int Fault_Flag = 0;
  int Second_Field = 0;
  int scalable_mode = 0;
  int q_scale_type = 0;
  int alternate_scan = 0;
  int pict_scal = 0;
  int priority_breakpoint = 0;
  int quantizer_scale = 0;
  int intra_slice = 0;
  short block[12][64];

  int profile = 0;
  int level = 0;
  //{{{  normative derived variables (as per ISO/IEC 13818-2)
  int horizontal_size = 0;
  int vertical_size = 0;

  int mb_width = 0;
  int mb_height = 0;

  double bit_rate = 0;
  double frame_rate = 0;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.2.1:  sequence_header()
  int aspect_ratio_information;
  int frame_rate_code;
  int bit_rate_value;
  int vbv_buffer_size;
  int constrained_parameters_flag;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.2.3:  sequence_extension()
  int profile_and_level_indication;
  int progressive_sequence;
  int chroma_format;
  int low_delay;
  int frame_rate_extension_n;
  int frame_rate_extension_d;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.2.4:  sequence_display_extension()
  int video_format;
  int color_description;
  int color_primaries;
  int transfer_characteristics;
  int matrix_coefficients;
  int display_horizontal_size;
  int display_vertical_size;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.3: picture_header()
  int temporal_reference;
  int picture_coding_type;
  int vbv_delay;
  int full_pel_forward_vector;
  int forward_f_code;
  int full_pel_backward_vector;
  int backward_f_code;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.3.1: picture_coding_extension() header
  int f_code[2][2];
  int intra_dc_precision;
  int picture_structure;
  int top_field_first;
  int frame_pred_frame_dct;
  int concealment_motion_vectors;
  int intra_vlc_format;
  int repeat_first_field;
  int chroma_420_type;
  int progressive_frame;
  int composite_display_flag;
  int v_axis;
  int field_sequence;
  int sub_carrier;
  int burst_amplitude;
  int sub_carrier_phase;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.3.3: picture_display_extension() header
  int frame_center_horizontal_offset[3];
  int frame_center_vertical_offset[3];
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.2.5: sequence_scalable_extension() header
  int layer_id;
  int lower_layer_prediction_horizontal_size;
  int lower_layer_prediction_vertical_size;
  int horizontal_subsampling_factor_m;
  int horizontal_subsampling_factor_n;
  int vertical_subsampling_factor_m;
  int vertical_subsampling_factor_n;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.3.5: picture_spatial_scalable_extension() header
  int lower_layer_temporal_reference;
  int lower_layer_horizontal_offset;
  int lower_layer_vertical_offset;
  int spatial_temporal_weight_code_table_index;
  int lower_layer_progressive_frame;
  int lower_layer_deinterlaced_field_select;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.3.6: copyright_extension() header
  int copyright_flag;
  int copyright_identifier;
  int original_or_copy;
  int copyright_number_1;
  int copyright_number_2;
  int copyright_number_3;
  //}}}
  //{{{  ISO/IEC 13818-2 section 6.2.2.6: group_of_pictures_header()
  int drop_flag;
  int hour;
  int minute;
  int sec;
  int frame;
  int closed_gop;
  int broken_link;
  //}}}
  //}}}
  };
