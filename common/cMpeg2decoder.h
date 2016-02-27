// cMpeg2decoder.h
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>

#include "cYuvFrame.h"
//}}}
//{{{  const
#define SLICE_START_CODE_MIN     0x101
#define SLICE_START_CODE_MAX     0x1AF
#define USER_DATA_START_CODE     0x1B2
#define SEQUENCE_HEADER_CODE     0x1B3
#define EXTENSION_START_CODE     0x1B5
#define SEQUENCE_END_CODE        0x1B7
#define GROUP_START_CODE         0x1B8
#define ISO_END_CODE             0x1B9

#define SEQUENCE_EXTENSION_ID        1
#define PICTURE_CODING_EXTENSION_ID  8

// picture coding type
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

// macroblock type
#define MACROBLOCK_INTRA                        1
#define MACROBLOCK_PATTERN                      2
#define MACROBLOCK_MOTION_BACKWARD              4
#define MACROBLOCK_MOTION_FORWARD               8
#define MACROBLOCK_QUANT                        16
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG       32
#define PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS 64

// motion_type
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2

// mv_format
#define MV_FIELD 0
#define MV_FRAME 1
//}}}
//{{{  tables
//{{{
static uint8_t scan[2][64] = {
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
static uint8_t default_intra_quantizer_matrix[64] = {
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
static uint8_t Non_Linear_quantizer_scale[32] = {
   0, 1, 2, 3, 4, 5, 6, 7,
   8,10,12,14,16,18,20,22,
  24,28,32,36,40,44,48,52,
  56,64,72,80,88,96,104,112 } ;
//}}}

static uint8_t stwc_table[3][4] = { {6,3,7,4}, {2,1,5,4}, {2,5,7,4} };
static uint8_t stwclass_table[9] = {0, 1, 2, 1, 1, 2, 3, 3, 4};

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
//{{{  sse2 const
__declspec(align(64)) short sse2_tab_i_04[] = {
  16384, 21407, 16384,  8867, 16384, -8867, 16384,-21407,  // w05 w04 w01 w00 w13 w12 w09 w08
  16384,  8867,-16384,-21407,-16384, 21407, 16384, -8867,  // w07 w06 w03 w02 w15 w14 w11 w10
  22725, 19266, 19266, -4520, 12873,-22725,  4520,-12873,
  12873,  4520,-22725,-12873,  4520, 19266, 19266,-22725 };

__declspec(align(64)) short sse2_tab_i_17[] = {
  22725, 29692, 22725, 12299, 22725,-12299, 22725,-29692,
  22725, 12299,-22725,-29692,-22725, 29692, 22725,-12299,
  31521, 26722, 26722, -6270, 17855,-31521,  6270,-17855,
  17855,  6270,-31521,-17855,  6270, 26722, 26722,-31521 };

__declspec(align(64)) short sse2_tab_i_26[] = {
  21407, 27969, 21407, 11585, 21407,-11585, 21407,-27969,
  21407, 11585,-21407,-27969,-21407, 27969, 21407,-11585,
  29692, 25172, 25172, -5906, 16819,-29692,  5906,-16819,
  16819,  5906,-29692,-16819,  5906, 25172, 25172,-29692 };

__declspec(align(64)) short sse2_tab_i_35[] = {
  19266, 25172, 19266, 10426, 19266,-10426, 19266,-25172,
  19266, 10426,-19266,-25172,-19266, 25172, 19266,-10426,
  26722, 22654, 22654, -5315, 15137,-26722,  5315,-15137,
  15137,  5315,-26722,-15137,  5315, 22654, 22654,-26722 };

#define DCT_8_INV_ROWX2(tab1, tab2)  \
{  \
  r1 = _mm_shufflelo_epi16(r1, _MM_SHUFFLE(3, 1, 2, 0));  \
  r1 = _mm_shufflehi_epi16(r1, _MM_SHUFFLE(3, 1, 2, 0));  \
  a0 = _mm_madd_epi16(_mm_shuffle_epi32(r1, _MM_SHUFFLE(0, 0, 0, 0)), *(__m128i*)(tab1+8*0));  \
  a1 = _mm_madd_epi16(_mm_shuffle_epi32(r1, _MM_SHUFFLE(1, 1, 1, 1)), *(__m128i*)(tab1+8*2));  \
  a2 = _mm_madd_epi16(_mm_shuffle_epi32(r1, _MM_SHUFFLE(2, 2, 2, 2)), *(__m128i*)(tab1+8*1));  \
  a3 = _mm_madd_epi16(_mm_shuffle_epi32(r1, _MM_SHUFFLE(3, 3, 3, 3)), *(__m128i*)(tab1+8*3));  \
  s0 = _mm_add_epi32(_mm_add_epi32(a0, round_row), a2);  \
  s1 = _mm_add_epi32(a1, a3);  \
  p0 = _mm_srai_epi32(_mm_add_epi32(s0, s1), 11);  \
  p1 = _mm_shuffle_epi32(_mm_srai_epi32(_mm_sub_epi32(s0, s1), 11), _MM_SHUFFLE(0, 1, 2, 3));  \
  r2 = _mm_shufflelo_epi16(r2, _MM_SHUFFLE(3, 1, 2, 0));  \
  r2 = _mm_shufflehi_epi16(r2, _MM_SHUFFLE(3, 1, 2, 0));  \
  b0 = _mm_madd_epi16(_mm_shuffle_epi32(r2, _MM_SHUFFLE(0, 0, 0, 0)), *(__m128i*)(tab2+8*0));  \
  b1 = _mm_madd_epi16(_mm_shuffle_epi32(r2, _MM_SHUFFLE(1, 1, 1, 1)), *(__m128i*)(tab2+8*2));  \
  b2 = _mm_madd_epi16(_mm_shuffle_epi32(r2, _MM_SHUFFLE(2, 2, 2, 2)), *(__m128i*)(tab2+8*1));  \
  b3 = _mm_madd_epi16(_mm_shuffle_epi32(r2, _MM_SHUFFLE(3, 3, 3, 3)), *(__m128i*)(tab2+8*3));  \
  s2 = _mm_add_epi32(_mm_add_epi32(b0, round_row), b2);  \
  s3 = _mm_add_epi32(b3, b1);  \
  p2 = _mm_srai_epi32(_mm_add_epi32(s2, s3), 11);  \
  p3 = _mm_shuffle_epi32(_mm_srai_epi32(_mm_sub_epi32(s2, s3), 11), _MM_SHUFFLE(0, 1, 2, 3));  \
  r1 = _mm_packs_epi32(p0, p1);  \
  r2 = _mm_packs_epi32(p2, p3);  \
}
//}}}

class cMpeg2decoder {
public:
  //{{{
  cMpeg2decoder() {

    Clip = (uint8_t*)malloc(1024);
    Clip += 384;
    for (int i = -384; i < 640; i++)
      Clip[i] = (i < 0) ? 0 : ((i > 255) ? 255 : i);

    initFastIDCT();

    for (int i = 0; i < 64; i++)
      intra_quantizer_matrix[i] = default_intra_quantizer_matrix[i];

    for (int i = 0; i < 64; i++)
      non_intra_quantizer_matrix[i] = 16;

    for (int i = 0; i < 64; i++) {
      chroma_intra_quantizer_matrix[i] = intra_quantizer_matrix[i];
      chroma_non_intra_quantizer_matrix[i] = non_intra_quantizer_matrix[i];
      }
    }
  //}}}
  //{{{
  ~cMpeg2decoder() {

    free (Clip);

    // deallocate buffers
    for (int i = 0; i < 3; i++) {
      free (backward_reference_frame[i]);
      free (forward_reference_frame[i]);
      free (auxframe[i]);
      }
    }
  //}}}

  //{{{
  bool decodePes (uint8_t* pesBuffer, int pesLen, cYuvFrame* yuvFrame) {

    bool frameWritten = false;

    m32bits = 0;
    mBitCount = 0;
    mBufferPtr = pesBuffer;
    mBufferEnd = pesBuffer + pesLen;
    consumeBits (0);

    if (!mGotSequenceHeader) {
      // get sequenceHeaderCode
      while (mBufferPtr < mBufferEnd) {
        uint32_t code = getHeader (false);
        if (code == 0x1B3) { // sequenceHeaderCode
          //{{{  allocate buffers from sequenceHeader width, height
          for (int cc = 0; cc < 3; cc++) {
            int size = (cc == 0) ? mWidth * mHeight : mChromaWidth * mChromaHeight;
            auxframe[cc] = (uint8_t*)malloc(size);
            forward_reference_frame[cc] = (uint8_t*)malloc(size);
            backward_reference_frame[cc] = (uint8_t*)malloc(size);
            }
          mGotSequenceHeader = true;
          break;
          }
          //}}}
        else if (code == 0x1B9) // isoEndCode
          break;
        }
      }

    // get pictureStartCode
    while ((mBufferPtr < mBufferEnd) && (getHeader (true) != 0x100)) {}

    // decodePicture
    if (mBufferPtr < mBufferEnd) {
      //{{{  updatePictureBuffers;
      for (int cc = 0; cc < 3; cc++) {
        // B pics do not need to be save for future reference
        if (picture_coding_type == B_TYPE)
          current_frame[cc] = auxframe[cc];

        else {
          // the previously decoded reference frame is stored coincident with the location where the backward
          // reference frame is stored (backwards prediction is not needed in P pictures)
          // update pointer for potential future B pictures
          uint8_t* tmp = forward_reference_frame[cc];
          forward_reference_frame[cc] = backward_reference_frame[cc];
          backward_reference_frame[cc] = tmp;

          // can erase over old backward reference frame since it is not used in a P picture
          // - since any subsequent B pictures will use the previously decoded I or P frame as the backward_reference_frame
          current_frame[cc] = backward_reference_frame[cc];
          }
        }
      //}}}
      while (slice (mBwidth * mBheight) >= 0);
      //{{{  reorderFrames write or display current or previously decoded reference frame
      int32_t linesize[2];
      linesize[0] = mWidth;
      linesize[1] = mChromaWidth;
      yuvFrame->set (0, (picture_coding_type == B_TYPE) ? auxframe : forward_reference_frame, linesize,
                     mWidth, mHeight, pesLen, picture_coding_type);
      frameWritten = true;
      //}}}
      }

    return frameWritten;
    }
  //}}}

private:
  //{{{
  void consumeBits (int numBits) {

    m32bits <<= numBits;
    mBitCount -= numBits;
    while (mBitCount <= 24) {
      m32bits |= *mBufferPtr++ << (24 - mBitCount);
      mBitCount += 8;
      }
    }
  //}}}
  //{{{
  uint32_t peekBits (int numBits) {
    return m32bits >> (32 - numBits);
    }
  //}}}
  //{{{
  uint32_t getBits (int numBits) {

    uint32_t val = m32bits >> (32 - numBits);
    consumeBits (numBits);
    return val;
    }
  //}}}
  //{{{
  uint32_t getStartCode() {

    if (mBufferPtr < mBufferEnd) {
      consumeBits (mBitCount & 7);
      while ((mBufferPtr < mBufferEnd) && (m32bits >> 8) != 0x001)
        consumeBits (8);
      return (mBufferPtr < mBufferEnd) ? m32bits : 0x1B9; // fake isoEndCode
      }
    else
      return 0x1B9; // fake isoEndCode
    }
  //}}}
  //{{{  getHeader
  //{{{
  void picture_coding_extension() {

    f_code[0][0] = getBits(4);
    f_code[0][1] = getBits(4);
    f_code[1][0] = getBits(4);
    f_code[1][1] = getBits(4);

    intra_dc_precision         = getBits(2);
    picture_structure          = getBits(2);
    top_field_first            = getBits(1);
    frame_pred_frame_dct       = getBits(1);
    concealment_motion_vectors = getBits(1);
    q_scale_type               = getBits(1);
    intra_vlc_format           = getBits(1);
    alternate_scan             = getBits(1);
    int repeat_first_field         = getBits(1);
    int chroma_420_type            = getBits(1);
    progressive          = getBits(1);

    int composite_display_flag     = getBits(1);
    if (composite_display_flag) {
      int v_axis            = getBits(1);
      int field_sequence    = getBits(3);
      int sub_carrier       = getBits(1);
      int burst_amplitude   = getBits(7);
      int sub_carrier_phase = getBits(8);
      }
    }
  //}}}
  //{{{
  void extension_and_user_data() {
  /* decode extension and user data  ISO/IEC 13818-2 section 6.2.2.2 */

    uint32_t code = getStartCode();
    while (code == EXTENSION_START_CODE || code == USER_DATA_START_CODE) {
      if (code == EXTENSION_START_CODE) {
        consumeBits (32);
        uint32_t ext_ID = getBits (4);
        switch (ext_ID) {
          case PICTURE_CODING_EXTENSION_ID:
            picture_coding_extension();
            break;
          default:
            //printf ("reserved extension start code ID %d\n", ext_ID);
            break;
          }
        }
      else
        /* userData ISO/IEC 13818-2  sections 6.3.4.1 and 6.2.2.2.2 skip ahead to the next start code */
        consumeBits (32);

      code = getStartCode();
      }
    }
  //}}}
  //{{{
  void sequenceHeader() {

    int horizontal_size = getBits (12);
    int vertical_size = getBits (12);
    int aspect_ratio_information = getBits (4);
    int frame_rate_code = getBits (4);
    int bit_rate_value = getBits (18);
    consumeBits (1);
    int vbv_buffer_size = getBits (10);
    int constrained_parameters_flag = getBits (1);

    mBwidth = (horizontal_size + 15) / 16;
    mBheight = 2 * ((vertical_size + 31) / 32);
    mWidth = 16 * mBwidth;
    mHeight = 16 * mBheight;
    mChromaWidth = mWidth >> 1;
    mChromaHeight = mHeight >> 1;
    block_count = 6;

    //if ((load_intra_quantizer_matrix = getBits (1)))
    //  for (int32_t i = 0; i < 64; i++)
    //    intra_quantizer_matrix[scan[0][i]] = getBits (8);

    //if ((load_non_intra_quantizer_matrix = getBits(1)))
    //  for (int32_t i = 0; i < 64; i++)
    //    non_intra_quantizer_matrix[scan[0][i]] = getBits (8);

    // copy luminance to chrominance matrices
    //for (int32_t i = 0; i < 64; i++) {
    //  chroma_intra_quantizer_matrix[i] = intra_quantizer_matrix[i];
    //  chroma_non_intra_quantizer_matrix[i] = non_intra_quantizer_matrix[i];
    //  }

    extension_and_user_data();
    }
  //}}}
  //{{{
  /* decode picture header ISO/IEC 13818-2 section 6.2.3 */
  void pictureHeader() {

    /* unless later overwritten by picture_spatial_scalable_extension() */
    temporal_reference = getBits (10);
    picture_coding_type = getBits (3);
    int32_t vbv_delay = getBits (16);

    if (picture_coding_type == P_TYPE || picture_coding_type == B_TYPE) {
      full_pel_forward_vector = getBits(1);
      forward_f_code = getBits(3);
      }

    if (picture_coding_type == B_TYPE) {
      full_pel_backward_vector = getBits(1);
      backward_f_code = getBits(3);
      }

    // consume extraBytes
    int Extra_Information_Byte_Count = 0;
    while (getBits (1)) {
      consumeBits (8);
      Extra_Information_Byte_Count++;
      }

    extension_and_user_data();
    }
  //}}}

  //{{{
  uint32_t getHeader (bool reportUnexpected) {

    getStartCode();
    uint32_t code = getBits (32);

    switch (code) {
      case 0x100 : // PICTURE_START_CODE:
        // printf ("getHeader %08x pictureHeader\n", code);
        pictureHeader();
        break;

      case 0x1B3 : // SEQUENCE_HEADER_CODE:
        //printf ("getHeader %x sequenceHeader\n", code);
        sequenceHeader();
        break;

      case 0x1B7: // SEQUENCE_END_CODE:
        printf ("getHeader %08x SEQUENCE_END_CODE\n", code);
        break;

      case 0x1B8 : // GROUP_START_CODE:
        //printf ("getHeader %08x groupOfPicturesHeader\n", code);
        break;

      default:
        if (reportUnexpected)
          printf ("getHeader unexpected %08x\n", code);
        break;
      }

    return code;
    }
  //}}}
  //}}}

  //{{{  sse2
  //{{{
  void sse2_ap945_idct (short* block) {

    __m128i r1, r2, a0, a1, a2, a3, b0, b1, b2, b3, s0, s1, s2, s3, p0, p1, p2, p3;
    __m128i round_row = _mm_set_epi16(0, 1024, 0, 1024, 0, 1024, 0, 1024);

    r1 = *(__m128i*)(block+8*0);
    r2 = *(__m128i*)(block+8*1);
    DCT_8_INV_ROWX2(sse2_tab_i_04, sse2_tab_i_17);
    *(__m128i*)(block+8*0) = r1;
    *(__m128i*)(block+8*1) = r2;

    r1 = *(__m128i*)(block+8*4);
    r2 = *(__m128i*)(block+8*7);
    DCT_8_INV_ROWX2(sse2_tab_i_04, sse2_tab_i_17);
    *(__m128i*)(block+8*4) = r1;
    *(__m128i*)(block+8*7) = r2;

    r1 = *(__m128i*)(block+8*2);
    r2 = *(__m128i*)(block+8*3);
    DCT_8_INV_ROWX2(sse2_tab_i_26, sse2_tab_i_35);
    *(__m128i*)(block+8*2) = r1;
    *(__m128i*)(block+8*3) = r2;

    r1 = *(__m128i*)(block+8*6);
    r2 = *(__m128i*)(block+8*5);
    DCT_8_INV_ROWX2(sse2_tab_i_26, sse2_tab_i_35);
    *(__m128i*)(block+8*6) = r1;
    *(__m128i*)(block+8*5) = r2;

    __m128i x0 = *(__m128i*)(block+8*0);
    __m128i x1 = *(__m128i*)(block+8*1);
    __m128i x2 = *(__m128i*)(block+8*2);
    __m128i x3 = *(__m128i*)(block+8*3);
    __m128i x4 = *(__m128i*)(block+8*4);
    __m128i x5 = *(__m128i*)(block+8*5);
    __m128i x6 = *(__m128i*)(block+8*6);
    __m128i x7 = *(__m128i*)(block+8*7);

    __m128i tan1 = _mm_set1_epi16(13036);
    __m128i tan2 = _mm_set1_epi16(27146);
    __m128i tan3 = _mm_set1_epi16(-21746);
    __m128i cos4 = _mm_set1_epi16(-19195);
    __m128i round_err = _mm_set1_epi16(1);
    __m128i round_col = _mm_set1_epi16(32);
    __m128i round_corr = _mm_set1_epi16(31);

    __m128i tp765 = _mm_adds_epi16(_mm_mulhi_epi16(x7, tan1), x1);
    __m128i tp465 = _mm_subs_epi16(_mm_mulhi_epi16(x1, tan1), x7);
    __m128i tm765 = _mm_adds_epi16(_mm_mulhi_epi16(x5, tan3), _mm_adds_epi16(x5, x3));
    __m128i tm465 = _mm_subs_epi16(x5, _mm_adds_epi16(_mm_mulhi_epi16(x3, tan3), x3));

    __m128i t7 = _mm_adds_epi16(_mm_adds_epi16(tp765, tm765), round_err);
    __m128i tp65 = _mm_subs_epi16(tp765, tm765);
    __m128i t4 = _mm_adds_epi16(tp465, tm465);
    __m128i tm65 = _mm_adds_epi16(_mm_subs_epi16(tp465, tm465), round_err);

    __m128i tmp1 = _mm_adds_epi16(tp65, tm65);
    __m128i t6 = _mm_or_si128(_mm_adds_epi16(_mm_mulhi_epi16(tmp1, cos4), tmp1), round_err);
    __m128i tmp2 = _mm_subs_epi16(tp65, tm65);
    __m128i t5 = _mm_or_si128(_mm_adds_epi16(_mm_mulhi_epi16(tmp2, cos4), tmp2), round_err);

    __m128i tp03 = _mm_adds_epi16(x0, x4);
    __m128i tp12 = _mm_subs_epi16(x0, x4);
    __m128i tm03 = _mm_adds_epi16(_mm_mulhi_epi16(x6, tan2), x2);
    __m128i tm12 = _mm_subs_epi16(_mm_mulhi_epi16(x2, tan2), x6);

    __m128i t0 = _mm_adds_epi16(_mm_adds_epi16(tp03, tm03), round_col);
    __m128i t3 = _mm_adds_epi16(_mm_subs_epi16(tp03, tm03), round_corr);
    __m128i t1 = _mm_adds_epi16(_mm_adds_epi16(tp12, tm12), round_col);
    __m128i t2 = _mm_adds_epi16(_mm_subs_epi16(tp12, tm12), round_corr);

    *(__m128i*)(block+8*0) = _mm_srai_epi16(_mm_adds_epi16(t0, t7), 6);
    *(__m128i*)(block+8*7) = _mm_srai_epi16(_mm_subs_epi16(t0, t7), 6);
    *(__m128i*)(block+8*1) = _mm_srai_epi16(_mm_adds_epi16(t1, t6), 6);
    *(__m128i*)(block+8*6) = _mm_srai_epi16(_mm_subs_epi16(t1, t6), 6);
    *(__m128i*)(block+8*2) = _mm_srai_epi16(_mm_adds_epi16(t2, t5), 6);
    *(__m128i*)(block+8*5) = _mm_srai_epi16(_mm_subs_epi16(t2, t5), 6);
    *(__m128i*)(block+8*3) = _mm_srai_epi16(_mm_adds_epi16(t3, t4), 6);
    *(__m128i*)(block+8*4) = _mm_srai_epi16(_mm_subs_epi16(t3, t4), 6);
    }
  //}}}

  //{{{
  //void asm_Add_Block (int comp, int bx, int by, int dct_type, int addflag)
  //{
    //unsigned char *rfp;
    //int iincr, cc = cc_table[comp];

    //if (cc==0)  // luminance
    //{
      //if (picture_structure==FRAME_PICTURE)
        //if (dct_type)  // field DCT coding
        //{
          //rfp = current_frame[0] + Picture_Width*(by+((comp&2)>>1)) + bx + ((comp&1)<<3);
          //iincr = Picture_Width<<1;
        //}
        //else  // frame DCT coding
        //{
          //rfp = current_frame[0] + Picture_Width*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
          //iincr = Picture_Width;
        //}
      //else  // field picture
      //{
        //rfp = current_frame[0] + (Picture_Width<<1)*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
        //iincr = Picture_Width<<1;
      //}
    //}
    //else  // chrominance
    //{
      //// scale coordinates
      //bx >>= 1;
      //by >>= 1;

      //if (picture_structure==FRAME_PICTURE)
      //{
          //rfp = current_frame[cc] + mChromaWidth*(by+((comp&2)<<2)) + bx + (comp&8);
          //iincr = mChromaWidth;
      //}
      //else  // field picture
      //{
        //rfp = current_frame[cc] + (mChromaWidth<<1)*(by+((comp&2)<<2)) + bx + (comp&8);
        //iincr = mChromaWidth<<1;
      //}
    //}

    //// 128bits is powerless in this case
    //__m64 *src = (__m64*)block[comp];

    //if (addflag)
    //{
       //__m64 zero = _mm_setzero_si64();

  ////#pragma unroll(8)
      //for (int loop=0; loop<8; loop++)
      //{
        //__m64 unpacklo = _m_punpcklbw(*(__m64*)rfp, zero);
        //__m64 sum1 = _m_paddw(*src++, unpacklo);
        //__m64 unpackhi = _m_punpckhbw(*(__m64*)rfp, zero);
        //__m64 sum2 = _m_paddw(*src++, unpackhi);
        //*(__m64*)rfp = _m_packuswb(sum1, sum2);
        //rfp += iincr;
      //}
    //}
    //else
    //{
      //__m64 offset = _mm_set1_pi16(128);

  ////#pragma unroll(8)
      //for (int loop=0; loop<8; loop++)
      //{
        //__m64 sum1 = _m_paddw(*src++, offset);
        //__m64 sum2 = _m_paddw(*src++, offset);
        //*(__m64*)rfp = _m_packuswb(sum1, sum2);
        //rfp += iincr;
      //}
    //}
  //}
  //}}}
  //{{{
  void asm_Clear_Block (int comp) {

    float* ptr = (float*)block[comp];
    __m128 zero = _mm_setzero_ps();

    //#pragma unroll(8)
    for (int loop = 0; loop < 8; loop++) {
      _mm_store_ps(ptr, zero);
      ptr += 4;
      }
    }
  //}}}
  //{{{
  //bool asm_start_of_slice (int *MBA, int *MBAinc, int dc_dct_pred[3], int PMV[2][2][2])
  //{
    //unsigned int code;
    //int slice_vert_pos_ext;

    //next_start_code();
    //code = Get_Bits(32);

    //if (code<SLICE_START_CODE_MIN || code>SLICE_START_CODE_MAX)
    //{
      //// only slice headers are allowed in picture_data
      //Flaw_Flag = true;
      //return false;
    //}

    //// decode slice header (may change quantizer_scale)
    //slice_vert_pos_ext = slice_header();

    //// decode macroblock address increment
    //*MBAinc = Get_macroblock_address_increment();
    //if (Flaw_Flag)
      //return false;

    //// set current location
    //// ISO/IEC 13818-2 section 6.3.17: Macroblock
    //*MBA = ((slice_vert_pos_ext<<7) + (code&255) - 1)*mBwidth + *MBAinc - 1;
    //*MBAinc = 1;  // first macroblock in slice: not skipped

    //// reset all DC coefficient and motion vector predictors
    //// ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    //dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    //// ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
    //PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
    //PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;

    //// successful: trigger decode macroblocks in slice
    //return true;
  //}

  //// ISO/IEC 13818-2 sections 7.2 through 7.5
  //static int decode_macroblock(int *macroblock_type, int *motion_type, int *dct_type,
    //int PMV[2][2][2], int dc_dct_pred[3], int motion_vertical_field_select[2][2], int dmvector[2])
  //{
    //int quantizer_scale_code, comp, motion_vector_count, mv_format;
    //int dmv, mvscale, coded_block_pattern;

    //// ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes
    //switch (picture_coding_type)
    //{
      //case I_TYPE:
        //*macroblock_type = Get_I_macroblock_type();
        //break;

      //case P_TYPE:
        //*macroblock_type = Get_P_macroblock_type();
        //break;

      //case B_TYPE:
        //*macroblock_type = Get_B_macroblock_type();
        //break;
    //}

    //if (Flaw_Flag)
      //return 0;  // go to next slice

    //// get frame/field motion type
    //if (*macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD))
    //{
      //if (picture_structure==FRAME_PICTURE)
        //*motion_type = frame_pred_frame_dct ? MC_FRAME : Get_Bits(2);
      //else
        //*motion_type = Get_Bits(2);
      //}
    //else if ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
      //*motion_type = (picture_structure==FRAME_PICTURE) ? MC_FRAME : MC_FIELD;

    //// derive motion_vector_count, mv_format and dmv (table 6-17, 6-18)
    //if (picture_structure==FRAME_PICTURE)
    //{
      //motion_vector_count = (*motion_type==MC_FIELD) ? 2 : 1;
      //mv_format = (*motion_type==MC_FRAME) ? MV_FRAME : MV_FIELD;
    //}
    //else
    //{
      //motion_vector_count = (*motion_type==MC_16X8) ? 2 : 1;
      //mv_format = MV_FIELD;
    //}

    //dmv = (*motion_type==MC_DMV);  // dual prime

    //// field mv predictions in frame pictures have to be scaled
    //// ISO/IEC 13818-2 section 7.6.3.1 Decoding the motion vectors
    //mvscale = (mv_format==MV_FIELD && picture_structure==FRAME_PICTURE);

    //// get dct_type (frame DCT / field DCT)
    //*dct_type = picture_structure==FRAME_PICTURE && !frame_pred_frame_dct
      //&& (*macroblock_type & (MACROBLOCK_PATTERN|MACROBLOCK_INTRA)) ? Get_Bits(1) : 0;

    //if (Flaw_Flag)
      //return 0;  // go to next slice

    //if (*macroblock_type & MACROBLOCK_QUANT)
    //{
      //quantizer_scale_code = Get_Bits(5);

      //// ISO/IEC 13818-2 section 7.4.2.2: Quantizer scale factor
      //quantizer_scale = q_scale_type ?
        //Non_Linear_quantizer_scale[quantizer_scale_code] : (quantizer_scale_code << 1);
    //}

    //// ISO/IEC 13818-2 section 6.3.17.2: Motion vectors
    //// decode forward motion vectors
    //if ((*macroblock_type & MACROBLOCK_MOTION_FORWARD)
      //|| ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors))
      //motion_vectors(PMV, dmvector, motion_vertical_field_select, 0, motion_vector_count,
                     //mv_format, f_code[0][0]-1, f_code[0][1]-1, dmv, mvscale);

    //if (Flaw_Flag)
      //return 0;  // go to next slice

    //// decode backward motion vectors
    //if (*macroblock_type & MACROBLOCK_MOTION_BACKWARD)
      //motion_vectors(PMV, dmvector, motion_vertical_field_select, 1, motion_vector_count,
                     //mv_format, f_code[1][0]-1, f_code[1][1]-1, 0, mvscale);

    //if (Flaw_Flag)
      //return 0;  // go to next slice

    //if ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
      //Flush_Buffer(1);  // marker bit

    //// ISO/IEC 13818-2 section 6.3.17.4: Coded block pattern
    //if (*macroblock_type & MACROBLOCK_PATTERN)
    //{
      //coded_block_pattern = Get_coded_block_pattern();

      //if (chroma_format==CHROMA422)
        //coded_block_pattern = (coded_block_pattern<<2) | Get_Bits(2);
    //}
    //else
        //coded_block_pattern = (*macroblock_type & MACROBLOCK_INTRA) ? (1<<block_count)-1 : 0;

    //if (Flaw_Flag)
      //return 0;  // go to next slice

    //// decode blocks
    //for (comp=0; comp<block_count; comp++)
    //{
      //Clear_Block(comp);

      //if (coded_block_pattern & (1<<(block_count-1-comp)))
      //{
        //if (*macroblock_type & MACROBLOCK_INTRA)
          //Decode_MPEG2_Intra_Block(comp, dc_dct_pred);
        //else
          //Decode_MPEG2_Non_Intra_Block(comp);

        //if (Flaw_Flag)
          //return 0;  // go to next slice
      //}
      //else
        //block[comp][63] = 1;  // ISO/IEC 13818-2 section 7.4.4: Mismatch control
    //}

    //// reset intra_dc predictors
    //// ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    //if (!(*macroblock_type & MACROBLOCK_INTRA))
      //dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    //// reset motion vector predictors
    //if ((*macroblock_type & MACROBLOCK_INTRA) && !concealment_motion_vectors)
    //{
      //// intra mb without concealment motion vectors
      //// ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
      //PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
      //PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
    //}

    //// special "No_MC" macroblock_type case
    //// ISO/IEC 13818-2 section 7.6.3.5: Prediction in P pictures
    //if (picture_coding_type==P_TYPE && !(*macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_INTRA)))
    //{
      //// non-intra mb without forward mv in a P picture
      //// ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
      //PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

      //// derive motion_type
      //// ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes, frame_motion_type
      //if (picture_structure==FRAME_PICTURE)
        //*motion_type = MC_FRAME;
      //else
      //{
        //*motion_type = MC_FIELD;
        //motion_vertical_field_select[0][0] = (picture_structure==BOTTOM_FIELD);
      //}
    //}

    //// successfully decoded macroblock
    //return 1;
  //}
  //}}}

  //{{{
  __forceinline void full_0_sse2 (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m128i*)d = _mm_loadu_si128((__m128i*)s);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void full_1_sse2 (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m128i*)d = _mm_avg_epu8(_mm_loadu_si128((__m128i*)s), _mm_loadu_si128((__m128i*)(s+lx)));

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void full_2_sse2 (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m128i*)d = _mm_avg_epu8(_mm_loadu_si128((__m128i*)s), _mm_loadu_si128((__m128i*)(s+1)));

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  // (a+b+c+d+2)>>2 = avg(avg(a,b)+avg(c,d)) - (a^b)|(c^d) & avg(a,b)^avg(c,d) & 0x01
  __forceinline void full_3_sse2 (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m128i shade = _mm_set1_epi8(1);

    for (int loop=0; loop<h; loop++)
    {
      __m128i pixel1 = _mm_loadu_si128((__m128i*)s);
      __m128i pixel2 = _mm_loadu_si128((__m128i*)(s+1));
      __m128i pixel3 = _mm_loadu_si128((__m128i*)(s+lx));
      __m128i pixel4 = _mm_loadu_si128((__m128i*)(s+lx+1));

      __m128i avg12 = _mm_avg_epu8(pixel1, pixel2);
      __m128i avg34 = _mm_avg_epu8(pixel3, pixel4);
      __m128i avg = _mm_avg_epu8(avg12, avg34);

      __m128i xor12 = _mm_xor_si128(pixel1, pixel2);
      __m128i xor34 = _mm_xor_si128(pixel3, pixel4);
      __m128i or1234 = _mm_or_si128(xor12, xor34);
      __m128i xoravg = _mm_xor_si128(avg12, avg34);
      __m128i offset = _mm_and_si128(_mm_and_si128(or1234, xoravg), shade);

      *(__m128i*)d = _mm_sub_epi8(avg, offset);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void full_4_sse2 (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m128i*)d = _mm_avg_epu8(_mm_loadu_si128((__m128i*)s), *(__m128i*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void full_5_sse2 (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      __m128i avg = _mm_avg_epu8(_mm_loadu_si128((__m128i*)s), _mm_loadu_si128((__m128i*)(s+lx)));
      *(__m128i*)d = _mm_avg_epu8(avg , *(__m128i*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void full_6_sse2 (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      __m128i avg = _mm_avg_epu8(_mm_loadu_si128((__m128i*)s), _mm_loadu_si128((__m128i*)(s+1)));
      *(__m128i*)d = _mm_avg_epu8(avg, *(__m128i*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void full_7_sse2 (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m128i shade = _mm_set1_epi8(1);

    for (int loop=0; loop<h; loop++)
    {
      __m128i pixel1 = _mm_loadu_si128((__m128i*)s);
      __m128i pixel2 = _mm_loadu_si128((__m128i*)(s+1));
      __m128i pixel3 = _mm_loadu_si128((__m128i*)(s+lx));
      __m128i pixel4 = _mm_loadu_si128((__m128i*)(s+lx+1));

      __m128i avg12 = _mm_avg_epu8(pixel1, pixel2);
      __m128i avg34 = _mm_avg_epu8(pixel3, pixel4);
      __m128i avg = _mm_avg_epu8(avg12, avg34);

      __m128i xor12 = _mm_xor_si128(pixel1, pixel2);
      __m128i xor34 = _mm_xor_si128(pixel3, pixel4);
      __m128i or1234 = _mm_or_si128(xor12, xor34);
      __m128i xoravg = _mm_xor_si128(avg12, avg34);
      __m128i offset = _mm_and_si128(_mm_and_si128(or1234, xoravg), shade);

      *(__m128i*)d = _mm_avg_epu8(_mm_sub_epi8(avg, offset), *(__m128i*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_1_sse (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m64*)d = _m_pavgb(*(__m64*)s, *(__m64*)(s+lx));

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_2_sse (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m64*)d = _m_pavgb(*(__m64*)s, *(__m64*)(s+1));

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_3_sse (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m64 shade = _mm_set1_pi8(1);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+1);
      __m64 pixel3 = *(__m64*)(s+lx);
      __m64 pixel4 = *(__m64*)(s+lx+1);

      __m64 avg12 = _m_pavgb(pixel1, pixel2);
      __m64 avg34 = _m_pavgb(pixel3, pixel4);
      __m64 avg = _m_pavgb(avg12, avg34);

      __m64 xor12 = _m_pxor(pixel1, pixel2);
      __m64 xor34 = _m_pxor(pixel3, pixel4);
      __m64 or1234 = _m_por(xor12, xor34);
      __m64 xoravg = _m_pxor(avg12, avg34);
      __m64 offset = _m_pand(_m_pand(or1234, xoravg), shade);

      *(__m64*)d = _m_psubb(avg, offset);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_4_sse (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m64*)d = _m_pavgb(*(__m64*)s, *(__m64*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_5_sse (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      __m64 avg = _m_pavgb(*(__m64*)s, *(__m64*)(s+lx));
      *(__m64*)d = _m_pavgb(avg, *(__m64*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_6_sse (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      __m64 avg = _m_pavgb(*(__m64*)s, *(__m64*)(s+1));
      *(__m64*)d = _m_pavgb(avg, *(__m64*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_7_sse (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m64 shade = _mm_set1_pi8(1);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+1);
      __m64 pixel3 = *(__m64*)(s+lx);
      __m64 pixel4 = *(__m64*)(s+lx+1);

      __m64 avg12 = _m_pavgb(pixel1, pixel2);
      __m64 avg34 = _m_pavgb(pixel3, pixel4);
      __m64 avg = _m_pavgb(avg12, avg34);

      __m64 xor12 = _m_pxor(pixel1, pixel2);
      __m64 xor34 = _m_pxor(pixel3, pixel4);
      __m64 or1234 = _m_por(xor12, xor34);
      __m64 xoravg = _m_pxor(avg12, avg34);
      __m64 offset = _m_pand(_m_pand(or1234, xoravg), shade);

      *(__m64*)d = _m_pavgb(_m_psubb(avg, offset), *(__m64*)d);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_0_mmx (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    for (int loop=0; loop<h; loop++)
    {
      *(__m64*)d = *(__m64*)s;

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  // (a+b+1)>>1 = (a|b) - (a^b)>>1
  __forceinline void half_1_mmx (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m64 shade = _mm_set1_pi8((char)0xfe);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+lx);

      __m64 srlq = _m_psrlqi(_m_pand(_m_pxor(pixel1, pixel2), shade), 1);
      *(__m64*)d = _m_psubb(_m_por(pixel1, pixel2), srlq);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_2_mmx (unsigned char* s, unsigned char *d, int h, int lx2)
  {
    __m64 shade = _mm_set1_pi8((char)0xfe);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+1);

      __m64 srlq = _m_psrlqi(_m_pand(_m_pxor(pixel1, pixel2), shade), 1);
      *(__m64*)d = _m_psubb(_m_por(pixel1, pixel2), srlq);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_3_mmx (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m64 zero = _mm_setzero_si64();
    __m64 offset = _mm_set1_pi16(2);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+1);
      __m64 pixel3 = *(__m64*)(s+lx);
      __m64 pixel4 = *(__m64*)(s+lx+1);

      __m64 unpacklo1 = _m_punpcklbw(pixel1, zero);
      __m64 unpacklo2 = _m_punpcklbw(pixel2, zero);
      __m64 unpacklo3 = _m_punpcklbw(pixel3, zero);
      __m64 unpacklo4 = _m_punpcklbw(pixel4, zero);
      __m64 sumlo1 = _m_paddw(unpacklo1, unpacklo2);
      __m64 sumlo2 = _m_paddw(unpacklo3, unpacklo4);
      __m64 sumlo = _m_paddw(_m_paddw(sumlo1, sumlo2), offset);
      __m64 avglo = _m_psrlwi(sumlo, 2);

      __m64 unpackhi1 = _m_punpckhbw(pixel1, zero);
      __m64 unpackhi2 = _m_punpckhbw(pixel2, zero);
      __m64 unpackhi3 = _m_punpckhbw(pixel3, zero);
      __m64 unpackhi4 = _m_punpckhbw(pixel4, zero);
      __m64 sumhi1 = _m_paddw(unpackhi1, unpackhi2);
      __m64 sumhi2 = _m_paddw(unpackhi3, unpackhi4);
      __m64 sumhi = _m_paddw(_m_paddw(sumhi1, sumhi2), offset);
      __m64 avghi = _m_psrlwi(sumhi, 2);

      *(__m64*)d = _m_packuswb(avglo, avghi);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_4_mmx (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    __m64 shade = _mm_set1_pi8((char)0xfe);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)d;

      __m64 srlq = _m_psrlqi(_m_pand(_m_pxor(pixel1, pixel2), shade), 1);
      *(__m64*)d = _m_psubb(_m_por(pixel1, pixel2), srlq);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_5_mmx (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m64 shade = _mm_set1_pi8((char)0xfe);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+lx);
      __m64 pixel3 = *(__m64*)d;

      __m64 srlq1 = _m_psrlqi(_m_pand(_m_pxor(pixel1, pixel2), shade), 1);
      __m64 avg = _m_psubb(_m_por(pixel1, pixel2), srlq1);

      __m64 srlq2 = _m_psrlqi(_m_pand(_m_pxor(avg, pixel3), shade), 1);
      *(__m64*)d = _m_psubb(_m_por(avg, pixel3), srlq2);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  __forceinline void half_6_mmx (unsigned char* s, unsigned char* d, int h, int lx2)
  {
    __m64 shade = _mm_set1_pi8((char)0xfe);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+1);
      __m64 pixel3 = *(__m64*)d;

      __m64 srlq1 = _m_psrlqi(_m_pand(_m_pxor(pixel1, pixel2), shade), 1);
      __m64 avg = _m_psubb(_m_por(pixel1, pixel2), srlq1);

      __m64 srlq2 = _m_psrlqi(_m_pand(_m_pxor(avg, pixel3), shade), 1);
      *(__m64*)d = _m_psubb(_m_por(avg, pixel3), srlq2);

      s += lx2;
      d += lx2;
    }
  }
  //}}}
  //{{{
  // ((a+b+c+d+2)>>2 + e + 1)>>1 = (a+b+c+d + e<<2 + 6)>>3
  __forceinline void half_7_mmx (unsigned char* s, unsigned char* d, int h, int lx, int lx2)
  {
    __m64 zero = _mm_setzero_si64();
    __m64 offset = _mm_set1_pi16(6);

    for (int loop=0; loop<h; loop++)
    {
      __m64 pixel1 = *(__m64*)s;
      __m64 pixel2 = *(__m64*)(s+1);
      __m64 pixel3 = *(__m64*)(s+lx);
      __m64 pixel4 = *(__m64*)(s+lx+1);
      __m64 pixel5 = *(__m64*)d;

      __m64 unpacklo1 = _m_punpcklbw(pixel1, zero);
      __m64 unpacklo2 = _m_punpcklbw(pixel2, zero);
      __m64 unpacklo3 = _m_punpcklbw(pixel3, zero);
      __m64 unpacklo4 = _m_punpcklbw(pixel4, zero);
      __m64 unpacklo5 = _m_punpcklbw(pixel5, zero);
      __m64 sumlo1 = _m_paddw(unpacklo1, unpacklo2);
      __m64 sumlo2 = _m_paddw(unpacklo3, unpacklo4);
      __m64 sumlo3 = _m_psllwi(unpacklo5, 2);
      __m64 sumlo = _m_paddw(_m_paddw(sumlo1, sumlo2), _m_paddw(sumlo3, offset));
      __m64 avglo = _m_psrlwi(sumlo, 3);

      __m64 unpackhi1 = _m_punpckhbw(pixel1, zero);
      __m64 unpackhi2 = _m_punpckhbw(pixel2, zero);
      __m64 unpackhi3 = _m_punpckhbw(pixel3, zero);
      __m64 unpackhi4 = _m_punpckhbw(pixel4, zero);
      __m64 unpackhi5 = _m_punpckhbw(pixel5, zero);
      __m64 sumhi1 = _m_paddw(unpackhi1, unpackhi2);
      __m64 sumhi2 = _m_paddw(unpackhi3, unpackhi4);
      __m64 sumhi3 = _m_psllwi(unpackhi5, 2);
      __m64 sumhi = _m_paddw(_m_paddw(sumhi1, sumhi2), _m_paddw(sumhi3, offset));
      __m64 avghi = _m_psrlwi(sumhi, 3);

      *(__m64*)d = _m_packuswb(avglo, avghi);

      s += lx2;
      d += lx2;
    }
  }
  //}}}

  //{{{
  void asm_form_prediction (unsigned char* src[], int sfield, int dfield, int lx, int lx2,
                            int h, int x, int y, int dx, int dy, int average_flag) {
  // ISO/IEC 13818-2 section 7.6.4: Forming predictions

    unsigned char *sY = src[0]+(sfield?lx2>>1:0) + lx * (y + (dy>>1)) + x + (dx>>1);
    unsigned char *dY = current_frame[0]+(dfield?lx2>>1:0) + lx * y + x;
    unsigned char *sYo = sY + 8;
    unsigned char *dYo = dY + 8;

    int typeY = (average_flag<<2) + ((dx & 1)<<1) + (dy & 1);
    switch (typeY) {
      //{{{  luma
      case 0:  // d[i] = s[i];
        full_0_sse2 (sY, dY, h, lx2);
        break;

      case 1:  // d[i] = (s[i]+s[i+lx]+1)>>1;
        full_1_sse2 (sY, dY, h, lx, lx2);
        break;

      case 2:  // d[i] = (s[i]+s[i+1]+1)>>1;
        full_2_sse2 (sY, dY, h, lx2);
        break;

      case 3:  // d[i] = (s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2;
        full_3_sse2 (sY, dY, h, lx, lx2);
        break;

      case 4:  // d[i] = (s[i]+d[i]+1)>>1;
        full_4_sse2 (sY, dY, h, lx2);
        break;

      case 5:  // d[i] = ((s[i]+s[i+lx]+1)>>1) + d[i] + 1)>>1;
        full_5_sse2 (sY, dY, h, lx, lx2);
        break;

      case 6:  // d[i] = (((s[i]+s[i+1]+1)>>1) + d[1] + 1)>>1;
        full_6_sse2 (sY, dY, h, lx2);
        break;

      case 7:  // d[i] = (((s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2) + d[i] + 1)>>1;
        full_7_sse2 (sY, dY, h, lx, lx2);
        break;
      }
      //}}}

    // ISO/IEC 13818-2 section 7.6.3: Motion vectors for chrominance components
    lx >>= 1;
    lx2 >>= 1;
    x >>= 1;
    dx /= 2;
    h >>= 1;
    y >>= 1;
    dy /= 2;

    int sOffset = (sfield ? lx2 >> 1: 0) + lx * (y + (dy>>1)) + x + (dx>>1);
    int dOffset = (dfield ? lx2 >> 1: 0) + lx * y + x;
    unsigned char *sCr = src[1] + sOffset;
    unsigned char *dCr = current_frame[1] + dOffset;
    unsigned char *sCb = src[2] + sOffset;
    unsigned char *dCb = current_frame[2] + dOffset;

    int typeC = (average_flag << 2) + ((dx & 1) << 1) + (dy & 1);
    switch (typeC) {
      //{{{  chroma
      case 0:
        half_0_mmx (sCr, dCr, h, lx2);
        half_0_mmx (sCb, dCb, h, lx2);
        break;

      case 1:
        half_1_sse (sCr, dCr, h, lx, lx2);
        half_1_sse(sCb, dCb, h, lx, lx2);
        break;

      case 2:
        half_2_sse (sCr, dCr, h, lx2);
        half_2_sse (sCb, dCb, h, lx2);
        break;

      case 3:
        half_3_sse (sCr, dCr, h, lx, lx2);
        half_3_sse (sCb, dCb, h, lx, lx2);
        break;

      case 4:
        half_4_sse (sCr, dCr, h, lx2);
        half_4_sse (sCb, dCb, h, lx2);
        break;

      case 5:
        half_5_sse (sCr, dCr, h, lx, lx2);
        half_5_sse (sCb, dCb, h, lx, lx2);
        break;

      case 6:
        half_6_sse (sCr, dCr, h, lx2);
        half_6_sse (sCb, dCb, h, lx2);
        break;

      case 7:
        half_7_sse (sCr, dCr, h, lx, lx2);
        half_7_sse (sCb, dCb, h, lx, lx2);
        break;
      }
      //}}}
    }
  //}}}
  //}}}
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

    for (int i = 0; i < 8; i++)
      idctRow (block+8*i);

    for (int i = 0; i < 8; i++)
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
  //{{{  macroBlocks
  //{{{
  int getImacroblockType() {

    if (getBits (1))
      return 1;

    if (!getBits (1)) {
      printf ("Invalid macroblock_type code\n");
      Flaw_Flag = 1;
      }

    return 17;
    }
  //}}}
  //{{{
  int getPmacroblockType() {

    int code;
    if ((code = peekBits (6)) >= 8) {
      code >>= 3;
      consumeBits (PMBtab0[code].len);
      return PMBtab0[code].val;
      }

    if (code == 0) {
      printf ("Invalid macroblock_type code\n");
      Flaw_Flag = 1;
      return 0;
      }

    consumeBits (PMBtab1[code].len);
    return PMBtab1[code].val;
    }
  //}}}
  //{{{
  int getBmacroblockType() {

    int code;
    if ((code = peekBits(6)) >= 8) {
      code >>= 2;
      consumeBits (BMBtab0[code].len);
      return BMBtab0[code].val;
      }

    if (code == 0) {
      printf("Invalid macroblock_type code\n");
      Flaw_Flag = 1;
      return 0;
      }

    consumeBits(BMBtab1[code].len);
    return BMBtab1[code].val;
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
      default:
        printf("getMacroblockType(): unrecognized picture coding type\n");
        break;
      }

    return macroblock_type;
    }
  //}}}

  //{{{
  int getCodedBlockPattern() {

    int code;
    if ((code = peekBits(9))>=128) {
      code >>= 4;
      consumeBits (CBPtab0[code].len);
      return CBPtab0[code].val;
      }

    if (code >= 8) {
      code >>= 1;
      consumeBits (CBPtab1[code].len);
      return CBPtab1[code].val;
      }

    if (code<1) {
      printf("Invalid coded_block_pattern code\n");
      Flaw_Flag = 1;
      return 0;
      }

    consumeBits (CBPtab2[code].len);
    return CBPtab2[code].val;
    }
  //}}}
  //{{{
  int getMacroBlockAddressIncrement() {

    int code;
    int val = 0;

    while ((code = peekBits(11))<24) {
      if (code != 15) /* if not macroblock_stuffing */ {
        if (code == 8) /* if macroblock_escape */ {
          val+= 33;
          }
        else {
          printf("Invalid macroblock_address_increment code\n");
          Flaw_Flag = 1;
          return 1;
          }
        }
      consumeBits (11);
      }

    /* macroblock_address_increment == 1 ('1' is in the MSB position of the lookahead) */
    if (code >= 1024) {
      consumeBits (1);
      return val + 1;
      }

    /* codes 00010 ... 011xx */
    if (code >= 128) {
      /* remove leading zeros */
      code >>= 6;
      consumeBits (MBAtab1[code].len);
      return val + MBAtab1[code].val;
      }

    /* codes 00000011000 ... 0000111xxxx */
    code-= 24; /* remove common base */
    consumeBits (MBAtab2[code].len);
    return val + MBAtab2[code].val;
    }
  //}}}

  //{{{
  /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
  void macroBlockModes (int* pmacroblock_type, int* pstwtype, int* pstwclass,
                        int* pmotion_type, int* pmotion_vector_count, int* pmv_format,
                        int* pmvscale, int* pdct_type) {

    int motion_type = 0;
    int motion_vector_count, mv_format, mvscale;
    int dct_type;

    /* get macroblock_type */
    int macroblock_type = getMacroBlockType();

    if (Flaw_Flag)
      return;

    /* get spatial_temporal_weight_code */
    int stwtype, stwclass;
    if (macroblock_type & 32)
      stwtype = 4;
    else
      stwtype = (macroblock_type & 64) ? 8 : 0;

    /* SCALABILITY: derive spatial_temporal_weight_class (Table 7-18) */
    stwclass = stwclass_table [stwtype];

    /* get frame/field motion type */
    if (macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD))
      motion_type = frame_pred_frame_dct ? MC_FRAME : getBits(2);
    else if ((macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
      /* concealment motion vectors */
      motion_type = MC_FRAME;

    /* derive motion_vector_count, mv_format and dmv, (table 6-17, 6-18) */
    motion_vector_count = (motion_type == MC_FIELD && stwclass < 2) ? 2 : 1;
    mv_format = (motion_type == MC_FRAME) ? MV_FRAME : MV_FIELD;

    /* field mv predictions in frame pictures have to be scaled
     * ISO/IEC 13818-2 section 7.6.3.1 Decoding the motion vectors
     * IMPLEMENTATION: mvscale is derived for later use in motion_vectors()
     * it displaces the stage:
     *    if((mv_format=="field")&&(t==1)&&(picture_structure=="Frame picture"))
     *      prediction = PMV[r][s][t] DIV 2; */
    mvscale = mv_format == MV_FIELD;

    /* get dct_type (frame DCT / field DCT) */
    dct_type = (!frame_pred_frame_dct) && (macroblock_type & (MACROBLOCK_PATTERN | MACROBLOCK_INTRA)) ? getBits(1) : 0;

    /* return values */
    *pmacroblock_type = macroblock_type;
    *pstwtype = stwtype;
    *pstwclass = stwclass;
    *pmotion_type = motion_type;
    *pmotion_vector_count = motion_vector_count;
    *pmv_format = mv_format;
    *pmvscale = mvscale;
    *pdct_type = dct_type;
    }
  //}}}
  //}}}

  //{{{
  int getLumaDCdctDiff() {
  // combined MPEG-1 and MPEG-2 stage. parse VLC and perform dct_diff arithmetic.

    // decode length
    int code = peekBits(5);

    int size;
    if (code < 31) {
      size = DClumtab0[code].val;
      consumeBits (DClumtab0[code].len);
      }
    else {
      code = peekBits(9) - 0x1f0;
      size = DClumtab1[code].val;
      consumeBits (DClumtab1[code].len);
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

    // decode length
    int size;
    int code = peekBits (5);
    if (code<31) {
      size = DCchromtab0[code].val;
      consumeBits (DCchromtab0[code].len);
      }
    else {
      code = peekBits(10) - 0x3e0;
      size = DCchromtab1[code].val;
      consumeBits (DCchromtab1[code].len);
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
  void decodeIntraBlock (int comp, int dc_dct_pred[]) {

    int val, sign, run;

    // decode DC coefficients
    int cc = (comp < 4) ? 0 : (comp & 1) + 1;
    if (cc == 0)
      val = (dc_dct_pred[0]+= getLumaDCdctDiff());
    else if (cc==1)
      val = (dc_dct_pred[1]+= getChromaDCdctDiff());
    else
      val = (dc_dct_pred[2]+= getChromaDCdctDiff());

    if (Flaw_Flag)
      return;

    block[comp][0] = val << (3 - intra_dc_precision);

    // decode AC coefficients
    for (int i = 1; ; i++) {
      DCTtab* tab;
      uint32_t code = peekBits (16);
      if (code >= 16384 && !intra_vlc_format)
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
        //{{{  flaw
        printf ("invalid Huffman code in Decode_MPEG2_Intra_Block()\n");
        Flaw_Flag = 1;
        return;
        }
        //}}}
      consumeBits (tab->len);

      if (tab->run == 64) // end_of_block
        return;

      if (tab->run == 65) {
        //{{{  escape
        i += run = getBits (6);
        val = getBits (12);
        if ((val & 2047) == 0) {
          printf ("invalid escape in Decode_MPEG2_Intra_Block()\n");
          Flaw_Flag = 1;
          return;
          }
        if ((sign = (val >= 2048)))
          val = 4096 - val;
        }
        //}}}
      else {
        i+= run = tab->run;
        val = tab->level;
        sign = getBits (1);
      }

      if (i >= 64) {
        //{{{  flaw
        printf ("DCT coeff index (i) out of bounds (intra2)\n");
        Flaw_Flag = 1;
        return;
        }
        //}}}

      uint8_t j = scan [alternate_scan][i];
      val = (val * quantizer_scale * intra_quantizer_matrix[j]) >> 4;
      block [comp][j] = sign ? -val : val;
      }
    }
  //}}}
  //{{{
  void decodeNonIntraBlock (int comp) {

    int val, sign, run;

    // decode AC coefficients
    for (int i = 0; ; i++) {
      DCTtab* tab;
      uint32_t code = peekBits (16);
      if (code >= 16384) {
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
        //{{{  flaw
        printf ("invalid Huffman code in Decode_MPEG2_Non_Intra_Block()\n");
        Flaw_Flag = 1;
        return;
        }
        //}}}
      consumeBits (tab->len);

      if (tab->run == 64) // end_of_block
        return;

      if (tab->run == 65) {
        //{{{  escape
        i += run = getBits (6);
        val = getBits (12);
        if ((val & 2047) == 0) {
          printf ("invalid escape in Decode_MPEG2_Intra_Block()\n");
          Flaw_Flag = 1;
          return;
          }
        if ((sign = (val >= 2048)))
          val = 4096 - val;
        }
        //}}}
      else {
        i += run = tab->run;
        val = tab->level;
        sign = getBits (1);
        }

      if (i >= 64) {
        //{{{  flaw
        printf ("DCT coeff index (i) out of bounds (inter2)\n");
        Flaw_Flag = 1;
        return;
        }
        //}}}

      uint8_t j = scan [alternate_scan][i];
      val = (((val << 1) + 1) * quantizer_scale * non_intra_quantizer_matrix [j]) >> 5;
      block [comp][j] = sign ? -val : val;
      }
    }
  //}}}

  //{{{
  int getMotionCode() {

    int code;
    if (getBits (1))
      return 0;

    if ((code = peekBits(9)) >= 64) {
      code >>= 6;
      consumeBits (MVtab0 [code].len);
      return getBits (1) ? -MVtab0 [code].val : MVtab0 [code].val;
      }

    if (code >= 24) {
      code >>= 3;
      consumeBits (MVtab1 [code].len);
      return getBits (1) ? -MVtab1 [code].val : MVtab1 [code].val;
      }

    if ((code -= 12) < 0) {
      Flaw_Flag = 1;
      return 0;
      }

    consumeBits (MVtab2 [code].len);
    return getBits (1) ? -MVtab2 [code].val : MVtab2 [code].val;
    }
  //}}}
  //{{{
  void decodeVector (int* pred, int r_size, int motion_code, int motion_residual, int full_pel_vector) {
  // calculate motion vector component

    int lim = 16 << r_size;
    int vec = full_pel_vector ? (*pred >> 1) : (*pred);

    if (motion_code > 0) {
      vec += ((motion_code-1) << r_size) + motion_residual + 1;
      if (vec >= lim)
        vec -= lim + lim;
      }
    else if (motion_code < 0) {
      vec -= ((-motion_code - 1) << r_size) + motion_residual + 1;
      if (vec < -lim)
        vec += lim + lim;
      }

    *pred = full_pel_vector ? (vec << 1) : vec;
    }
  //}}}
  //{{{
  void motionVector (int* PMV, int h_r_size, int v_r_size, int mvscale, int full_pel_vector) {
  // get and decode motion vector and differential motion vector for one prediction */

    // horizontal component
    int motion_code = getMotionCode();
    int motion_residual = (h_r_size!=0 && motion_code!=0) ? getBits (h_r_size) : 0;
    decodeVector (&PMV[0], h_r_size, motion_code, motion_residual, full_pel_vector);

    // vertical component
    motion_code = getMotionCode();
    motion_residual = (v_r_size != 0 && motion_code != 0) ? getBits (v_r_size) : 0;

    if (mvscale)
      PMV[1] >>= 1;

    decodeVector (&PMV[1], v_r_size, motion_code, motion_residual, full_pel_vector);

    if (mvscale)
      PMV[1] <<= 1;
    }
  //}}}
  //{{{
  void motionVectors (int PMV[2][2][2], int motion_vertical_field_select[2][2],
                      int s, int motion_vector_count, int mv_format, int h_r_size, int v_r_size, int mvscale) {

    if (motion_vector_count == 1) {
      if (mv_format == MV_FIELD)
        motion_vertical_field_select[1][s] = motion_vertical_field_select[0][s] = getBits(1);
      motionVector (PMV[0][s], h_r_size, v_r_size, mvscale, 0);

      // update other motion vector predictors
      PMV[1][s][0] = PMV[0][s][0];
      PMV[1][s][1] = PMV[0][s][1];
      }

    else {
      motion_vertical_field_select[0][s] = getBits (1);
      motionVector (PMV[0][s], h_r_size, v_r_size, mvscale, 0);
      motion_vertical_field_select [1][s] = getBits (1);
      motionVector (PMV[1][s], h_r_size, v_r_size, mvscale, 0);
      }
    }
  //}}}

  //{{{
  void formComponentPrediction (uint8_t* src, uint8_t* dst, int lx, int lx2,
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
    int xint = dx >> 1;
    int yint = dy >> 1;

    // derive half pel flags
    int xh = dx & 1;
    int yh = dy & 1;

    // compute the linear address of pel_ref[][] and pel_pred[][] based on cartesian/raster cordinates provided
    uint8_t* s = src + lx * (y + yint) + x + xint;
    uint8_t* d = dst + lx * y + x;

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
            int v = d[i] + ((uint32_t)(s[i] + s[i+lx] +1) >>1);
            d[i]= (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = (uint32_t)(s[i] + s[i+lx] + 1) >> 1;
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
            int v = d[i] + ((uint32_t)(s[i] + s[i+1] + 1) >> 1);
            d[i] = (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = (uint32_t)(s[i] + s[i+1] + 1) >> 1;
          s += lx2;
          d += lx2;
          }
        }
      }
      //}}}
    else {
      //{{{  horizontal and vertical half-pel
      if (average_flag) {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++) {
            int v = d[i] + ((uint32_t)(s[i] + s[i+1] + s[i+lx] + s[i+lx+1] + 2) >> 2);
            d[i] = (v + (v >= 0 ? 1 : 0)) >> 1;
            }
          s += lx2;
          d += lx2;
          }
        }
      else {
        for (int j = 0; j < h; j++) {
          for (int i = 0; i < w; i++)
            d[i] = (uint32_t)(s[i] + s[i+1] + s[i+lx] + s[i+lx+1] + 2) >> 2;
          s += lx2;
          d += lx2;
          }
        }
      }
      //}}}
    }
  //}}}
  //{{{
  void formPrediction (uint8_t* src[], int sfield, uint8_t* dst[], int dfield,
                       int lx, int lx2, int w, int h, int x, int y, int dx, int dy, int average_flag) {

    formComponentPrediction (src[0] + (sfield ? lx2 >> 1 : 0), 
                             dst[0] + (dfield ? lx2 >> 1 : 0), lx, lx2, w, h, x, y, dx, dy, average_flag);

    lx >>= 1;
    lx2 >>= 1;
    w >>= 1;
    x >>= 1;
    dx /= 2;
    h >>= 1;
    y >>= 1;
    dy /= 2;
    formComponentPrediction (src[1] + (sfield ? lx2 >> 1 : 0), 
                             dst[1] + (dfield ? lx2 >> 1 : 0), lx, lx2, w, h, x, y, dx, dy, average_flag);
    formComponentPrediction (src[2] + (sfield ? lx2 >> 1: 0), 
                             dst[2] + (dfield ? lx2 >> 1 : 0), lx, lx2, w, h, x, y, dx, dy, average_flag);
  }
  //}}}
  //{{{
  void formPredictions (int bx, int by, int macroblock_type, int motion_type, int PMV[2][2][2],
                        int motion_vertical_field_select[2][2], int stwtype) {

    int stwtop = stwtype % 3;  // 0:temporal, 1:(spat+temp)/2, 2:spatial
    int stwbot = stwtype / 3;

    if ((macroblock_type & MACROBLOCK_MOTION_FORWARD) || (picture_coding_type == P_TYPE)) {
      if ((motion_type == MC_FRAME) || !(macroblock_type & MACROBLOCK_MOTION_FORWARD)) {
        // frame-based prediction (broken into top and bottom halves for spatial scalability prediction purposes)
        if (stwtop < 2)
          formPrediction (forward_reference_frame, 0, current_frame, 0,
                          mWidth, mWidth << 1, 16, 8, bx, by, PMV[0][0][0], PMV[0][0][1], stwtop);
        if (stwbot < 2)
          formPrediction (forward_reference_frame, 1, current_frame, 1,
                          mWidth, mWidth << 1, 16, 8, bx, by, PMV[0][0][0], PMV[0][0][1],stwbot);
        }
      else if (motion_type == MC_FIELD) {
         // top field prediction
        if (stwtop < 2)
          formPrediction (forward_reference_frame,motion_vertical_field_select[0][0], current_frame, 0, 
                          mWidth << 1, mWidth << 1, 16, 8, bx, by >> 1, PMV[0][0][0], PMV[0][0][1]>>1, stwtop);
        // bottom field prediction
        if (stwbot < 2)
          formPrediction (forward_reference_frame, motion_vertical_field_select[1][0], current_frame, 1, 
                          mWidth << 1, mWidth << 1, 16, 8, bx, by >> 1, PMV[1][0][0], PMV[1][0][1] >> 1, stwbot);
        }
      else
        printf ("invalid motion_type\n");
      stwtop = stwbot = 1;
      }

    if (macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
      if (motion_type == MC_FRAME) {
        // frame-based prediction
        if (stwtop < 2)
          formPrediction (backward_reference_frame, 0, current_frame, 0,
                          mWidth, mWidth << 1, 16, 8, bx, by, PMV[0][1][0],PMV[0][1][1], stwtop);
        if (stwbot < 2)
          formPrediction (backward_reference_frame, 1, current_frame, 1,
                          mWidth, mWidth << 1, 16, 8, bx, by, PMV[0][1][0],PMV[0][1][1], stwbot);
        }
      else {
        // top field prediction
        if (stwtop < 2)
          formPrediction (backward_reference_frame, motion_vertical_field_select[0][1], current_frame, 0, 
                          mWidth << 1, mWidth << 1, 16, 8, bx, by >> 1, PMV[0][1][0], PMV[0][1][1] >> 1, stwtop);
        // bottom field prediction
        if (stwbot < 2)
          formPrediction (backward_reference_frame, motion_vertical_field_select[1][1], current_frame, 1, 
                          mWidth << 1, mWidth << 1, 16, 8, bx, by >> 1, PMV[1][1][0], PMV[1][1][1] >> 1, stwbot);
        }
      }
    }
  //}}}
  //{{{
  void addBlock (int comp, int bx, int by, int dct_type, int addflag) {
  // move/add 8x8-Block from block[comp] to backward_reference_frame */
  // copy reconstructed 8x8 block from block[comp] to current_frame[]
  // ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data

    int iincr;
    uint8_t* rfp;

    // derive color component index
    int cc = (comp < 4) ? 0 : (comp & 1) + 1;
    if (cc == 0) {
      if (dct_type) {
        // luminance  field DCT coding
        rfp = current_frame[0] + mWidth * (by + ((comp & 2) >> 1)) + bx + ((comp & 1) << 3);
        iincr = (mWidth << 1) - 8;
        }
      else {
        // luminance frame DCT coding
        rfp = current_frame[0] + mWidth * (by + ((comp & 2) << 2)) + bx + ((comp & 1) << 3);
        iincr = mWidth - 8;
        }
      }
    else {
      // chrominance scale coordinates, frame DCT coding
      rfp = current_frame[cc] + mChromaWidth * ((by >> 1) + ((comp & 2) << 2)) + (bx >> 1) + (comp & 8);
      iincr = mChromaWidth - 8;
      }

    short* bp = block [comp];
    if (addflag) {
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          *rfp = Clip [*bp++ + *rfp];
          rfp++;
          }
        rfp += iincr;
        }
      }
    else {
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++)
          *rfp++ = Clip [*bp++ + 128];
        rfp += iincr;
        }
      }
    }
  //}}}
  //{{{
  void clearBlock (int comp) {

    short* Block_Ptr = block[comp];
    for (int i = 0; i < 64; i++)
      *Block_Ptr++ = 0;
    }
  //}}}
  //{{{
  void motionCompensation (int MBA, int macroblock_type, int motion_type, int PMV[2][2][2],
                           int motion_vertical_field_select[2][2], int stwtype, int dct_type) {

    // derive current macroblock position within picture ISO/IEC 13818-2 section 6.3.1.6 and 6.3.1.7
    int bx = 16 * (MBA % mBwidth);
    int by = 16 * (MBA / mBwidth);

    // motion compensation
    if (!(macroblock_type & MACROBLOCK_INTRA))
      formPredictions (bx, by, macroblock_type, motion_type, PMV, motion_vertical_field_select, stwtype);

    // copy or add block data int32_to picture */
    for (int comp = 0; comp < block_count; comp++) {
      // ISO/IEC 13818-2 section Annex A: inverse DCT
      fastIDCT (block[comp]);
      //sse2_ap945_idct (block[comp]);

      // ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data
      addBlock (comp, bx, by, dct_type, (macroblock_type & MACROBLOCK_INTRA) == 0);
      }
    }
  //}}}

  //{{{
  int sliceHeader() {

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
      extra_information_slice = 0;
      while (getBits (1)) {
        consumeBits (8);
        extra_information_slice++;
        }
      }
    else
      intra_slice = 0;

    return 0;
    }
  //}}}
  //{{{
  int startOfSlice (int MBAmax, int* MBA, int* MBAinc, int dc_dct_pred[3], int PMV[2][2][2]) {
  // return -1 - means go to next picture

    Flaw_Flag = 0;

    uint32_t code = getStartCode();
    if (code < SLICE_START_CODE_MIN || code > SLICE_START_CODE_MAX) {
      // only slice headers are allowed in picture_data
      printf ("startOfSlice Premature end of picture\n");
      return -1;
      }
    consumeBits (32);

    // decode slice header - may change quantizer_scale
    int slice_vert_pos_ext = sliceHeader();

    // decode macroblock address increment
    *MBAinc = getMacroBlockAddressIncrement();
    if (Flaw_Flag) {
      printf ("startOfSlice MBAinc unsuccessful\n");
      return 0;
      }

    // set current location
    *MBA = ((slice_vert_pos_ext<<7) + (code&255) - 1) * mBwidth + *MBAinc - 1;

    // first macroblock in slice: not skipped
    *MBAinc = 1;

    // reset all DC coefficient and motion vector predictors
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    // Resetting motion vector predictors */
    PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
    PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;

    // successful decode macroblocks in slice
    return 1;
    }
  //}}}
  //{{{
  void skippedMacroblock (int dc_dct_pred[3], int PMV[2][2][2],
                          int* motion_type, int motion_vertical_field_select[2][2], int* stwtype, int* macroblock_type) {

    for (int comp = 0; comp < block_count; comp++)
      clearBlock (comp);

    // reset intra_dc predictors  ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    // reset motion vector predictors  ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
    if (picture_coding_type == P_TYPE)
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

    // derive motion_type */
    *motion_type = MC_FRAME;

    // skipped I are spatial-only predicted, skipped P and B are temporal-only predicted ISO/IEC 13818-2 section 7.7.6:
    *stwtype = (picture_coding_type == I_TYPE) ? 8 : 0;

    // IMPLEMENTATION: clear MACROBLOCK_INTRA */
    *macroblock_type &= ~MACROBLOCK_INTRA;
    }
  //}}}
  //{{{
  int decodeMacroblock (int* macroblock_type, int* stwtype, int* stwclass, int* motion_type, int* dct_type,
                        int PMV[2][2][2], int dc_dct_pred[3], int motion_vertical_field_select[2][2]) {

    int quantizer_scale_code;
    int comp;
    int motion_vector_count;
    int mv_format;
    int mvscale;
    int coded_block_pattern;

    // ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes
    macroBlockModes (macroblock_type, stwtype, stwclass, motion_type, &motion_vector_count, &mv_format,  &mvscale, dct_type);

    if (Flaw_Flag)
      return 0;  // trigger: go to next slice

    if (*macroblock_type & MACROBLOCK_QUANT) {
      quantizer_scale_code = getBits(5);
      // ISO/IEC 13818-2 section 7.4.2.2: Quantizer scale factor
      quantizer_scale = q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : (quantizer_scale_code << 1);
      }

    // motion vectors ISO/IEC 13818-2 section 6.3.17.2: Motion vectors decode forward motion vectors
    if ((*macroblock_type & MACROBLOCK_MOTION_FORWARD) || ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors))
      motionVectors (PMV, motion_vertical_field_select, 0, motion_vector_count, mv_format, f_code[0][0]-1, f_code[0][1]-1, mvscale);

    if (Flaw_Flag)
      return 0;  // trigger: go to next slice

    // decode backward motion vectors
    if (*macroblock_type & MACROBLOCK_MOTION_BACKWARD)
        motionVectors (PMV, motion_vertical_field_select, 1, motion_vector_count, mv_format, f_code[1][0]-1, f_code[1][1]-1, mvscale);

    if (Flaw_Flag)
      return 0;  // trigger: go to next slice

    if ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
      consumeBits (1); // remove marker_bit

    // macroblock_pattern ISO/IEC 13818-2 section 6.3.17.4: Coded block pattern
    if (*macroblock_type & MACROBLOCK_PATTERN)
      coded_block_pattern = getCodedBlockPattern();
    else
      coded_block_pattern = (*macroblock_type & MACROBLOCK_INTRA) ? (1 << block_count) - 1 : 0;

    if (Flaw_Flag)
      return 0;  // trigger: go to next slice

    // decode blocks
    for (comp = 0; comp < block_count; comp++) {
      clearBlock (comp);
      if (coded_block_pattern & (1<<(block_count-1-comp))) {
        if (*macroblock_type & MACROBLOCK_INTRA)
          decodeIntraBlock (comp, dc_dct_pred);
        else
          decodeNonIntraBlock (comp);
        if (Flaw_Flag)
          return 0;  // trigger: go to next slice
        }
      }

    if (picture_coding_type == D_TYPE)
      // remove end_of_macroblock (always 1, prevents startcode emulation) ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6
      consumeBits (1);

    // reset intra_dc predictors ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    if (!(*macroblock_type & MACROBLOCK_INTRA))
      dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    // reset motion vector predictors
    if ((*macroblock_type & MACROBLOCK_INTRA) && !concealment_motion_vectors) {
      // intra mb without concealment motion vectors ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
      PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
      }

    // special "No_MC" macroblock_type case ISO/IEC 13818-2 section 7.6.3.5: Prediction in P pictures
    if ((picture_coding_type == P_TYPE) && !(*macroblock_type & (MACROBLOCK_MOTION_FORWARD | MACROBLOCK_INTRA))) {
      // non-intra mb without forward mv in a P picture ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

      // derive motion_type ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes, frame_motion_type
      *motion_type = MC_FRAME;
      }

    if (*stwclass == 4) {
      // purely spatially predicted macroblock ISO/IEC 13818-2 section 7.7.5.1: Resetting motion vector predictions
      PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
      PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
      }

    // successfully decoded macroblock
    return 1;
    }
  //}}}
  //{{{
  int slice (int MBAmax) {
  // decode all macroblocks of the current picture ISO/IEC 13818-2 section 6.3.16 */

    int MBA = 0;
    int MBAinc = 0;

    int dc_dct_pred[3];
    int PMV[2][2][2];
    int ret = startOfSlice (MBAmax, &MBA, &MBAinc, dc_dct_pred, PMV);
    if (ret != 1)
      return (ret);

    Flaw_Flag = 0;
    for (;;) {
      // this is how we properly exit out of picture
      if (MBA >= MBAmax)
        return -1; /* all macroblocks decoded */
      if (MBAinc == 0) {
        if (!peekBits(23) || Flaw_Flag) {
          // next_start_code or fault
      resync: // if Flaw_Flag: resynchronize to next next_start_code
          Flaw_Flag = 0;
          return 0;     // trigger: go to next slice
          }
        else {
          // neither next_start_code nor Flaw_Flag  decode macroblock address increment
          MBAinc = getMacroBlockAddressIncrement();
          if (Flaw_Flag)
            goto resync;
          }
        }

      if (MBA >= MBAmax) { /* MBAinc points beyond picture dimensions */
        printf ("Too many macroblocks in picture\n");
        return -1;
        }

      int macroblock_type, motion_type, dct_type;
      int motion_vertical_field_select[2][2];
      int stwtype, stwclass;
      if (MBAinc == 1) {
        // not skipped
        ret = decodeMacroblock (&macroblock_type, &stwtype, &stwclass, &motion_type, &dct_type, PMV, dc_dct_pred, motion_vertical_field_select);
        if (ret == -1)
          return -1;
        if (ret == 0)
          goto resync;
        }
      else // MBAinc!=1: skipped macroblock ISO/IEC 13818-2 section 7.6.6
        skippedMacroblock (dc_dct_pred, PMV, &motion_type, motion_vertical_field_select, &stwtype, &macroblock_type);

      // ISO/IEC 13818-2 section 7.6
      motionCompensation (MBA, macroblock_type, motion_type, PMV, motion_vertical_field_select, stwtype, dct_type);

      // next macroblock
      MBA++;
      MBAinc--;
      if (MBA >= MBAmax)
        return -1; /* all macroblocks decoded */
      }
    }
  //}}}

  //{{{  vars
  int mBitCount = 0;
  uint32_t m32bits = 0;
  uint8_t* mBufferPtr = NULL;
  uint8_t* mBufferEnd = NULL;
  bool mGotSequenceHeader = false;

  int mWidth;
  int mHeight;
  int mChromaWidth = 0;
  int mChromaHeight = 0;
  int mBwidth = 0;
  int mBheight = 0;
  int block_count = 0;

  uint8_t* auxframe[3];
  uint8_t* current_frame[3];
  uint8_t* forward_reference_frame[3];
  uint8_t* backward_reference_frame[3];

  short* iclp = NULL;
  short iclip[1024];
  uint8_t* Clip = NULL;

  int intra_quantizer_matrix[64];
  int non_intra_quantizer_matrix[64];
  int chroma_intra_quantizer_matrix[64];
  int chroma_non_intra_quantizer_matrix[64];

  bool Flaw_Flag = 0;
  bool Second_Field = 0;
  int q_scale_type = 0;
  int pict_scal = 0;
  int alternate_scan = 0;
  int quantizer_scale = 0;
  int intra_slice = 0;
  short block[12][64];

  int temporal_reference = 0;
  int picture_coding_type = 0;

  int full_pel_forward_vector = 0;
  int forward_f_code = 0;
  int full_pel_backward_vector = 0;
  int backward_f_code = 0;

  int f_code[2][2];
  int intra_dc_precision = 0;
  int picture_structure = 0;
  int top_field_first = 0;
  int frame_pred_frame_dct = 0;
  int concealment_motion_vectors = 0;
  int intra_vlc_format = 0;
  int progressive = 0;
  //}}}
  };