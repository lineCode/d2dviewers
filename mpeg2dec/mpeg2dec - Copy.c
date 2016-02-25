/*{{{  includes*/
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
/*}}}*/
/*{{{  const*/
#define ERROR (-1)

#define PICTURE_START_CODE      0x100
#define SLICE_START_CODE_MIN    0x101
#define SLICE_START_CODE_MAX    0x1AF
#define USER_DATA_START_CODE    0x1B2
#define SEQUENCE_HEADER_CODE    0x1B3
#define SEQUENCE_ERROR_CODE     0x1B4
#define EXTENSION_START_CODE    0x1B5
#define SEQUENCE_END_CODE       0x1B7
#define GROUP_START_CODE        0x1B8
#define SYSTEM_START_CODE_MIN   0x1B9
#define SYSTEM_START_CODE_MAX   0x1FF

#define ISO_END_CODE            0x1B9
#define PACK_START_CODE         0x1BA
#define SYSTEM_START_CODE       0x1BB

#define VIDEO_ELEMENTARY_STREAM 0x1e0

/* scalable_mode */
#define SC_NONE 0
#define SC_DP   1
#define SC_SPAT 2
#define SC_SNR  3
#define SC_TEMP 4

/* picture coding type */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

/* picture structure */
#define TOP_FIELD     1
#define BOTTOM_FIELD  2
#define FRAME_PICTURE 3

/* macroblock type */
#define MACROBLOCK_INTRA                        1
#define MACROBLOCK_PATTERN                      2
#define MACROBLOCK_MOTION_BACKWARD              4
#define MACROBLOCK_MOTION_FORWARD               8
#define MACROBLOCK_QUANT                        16
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG       32
#define PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS 64

/* motion_type */
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3

/* mv_format */
#define MV_FIELD 0
#define MV_FRAME 1

/* chroma_format */
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

/* extension start code IDs */
#define SEQUENCE_EXTENSION_ID                    1
#define SEQUENCE_DISPLAY_EXTENSION_ID            2
#define QUANT_MATRIX_EXTENSION_ID                3
#define COPYRIGHT_EXTENSION_ID                   4
#define SEQUENCE_SCALABLE_EXTENSION_ID           5
#define PICTURE_DISPLAY_EXTENSION_ID             7
#define PICTURE_CODING_EXTENSION_ID              8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID    9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID  10

#define ZIG_ZAG                                  0

#define PROFILE_422                             (128+5)
#define MAIN_LEVEL                              8

/* Layers: used by Verbose_Flag, Verifier_Flag, Stats_Flag, and Trace_Flag */
#define NO_LAYER                                0
#define SEQUENCE_LAYER                          1
#define PICTURE_LAYER                           2
#define SLICE_LAYER                             3
#define MACROBLOCK_LAYER                        4
#define BLOCK_LAYER                             5
#define EVENT_LAYER                             6
#define ALL_LAYERS                              7

#define FILENAME_LENGTH                       256

#define MB_WEIGHT                  32
#define MB_CLASS4                  64
/*}}}*/
/*{{{  tables*/
/* zig-zag and alternate scan patterns */
/*{{{*/
unsigned char scan[2][64] = {
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
/*}}}*/
/*{{{*/
/* default intra quantization matrix */
unsigned char default_intra_quantizer_matrix[64] = {
  8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83 } ;
/*}}}*/
/*{{{*/
/* non-linear quantization coefficient table */
unsigned char Non_Linear_quantizer_scale[32] = {
   0, 1, 2, 3, 4, 5, 6, 7,
   8,10,12,14,16,18,20,22,
  24,28,32,36,40,44,48,52,
  56,64,72,80,88,96,104,112 } ;
/*}}}*/

/*{{{*/
static double frame_rate_Table[16] = {
  0.0, ((23.0*1000.0)/1001.0), 24.0, 25.0,
  ((30.0*1000.0)/1001.0), 30.0, 50.0, ((60.0*1000.0)/1001.0), 60.0, -1, -1, -1, -1, -1, -1, -1 };
/*}}}*/

/*{{{  struct VLCtab*/
typedef struct {
  char val, len;
  } VLCtab;
/*}}}*/
/*{{{*/
/* Table B-3, macroblock_type in P-pictures, codes 001..1xx */
static VLCtab PMBtab0[8] = {
  {ERROR,0},
  {MACROBLOCK_MOTION_FORWARD,3},
  {MACROBLOCK_PATTERN,2}, {MACROBLOCK_PATTERN,2},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1},
  {MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,1}
};
/*}}}*/
/*{{{*/
/* Table B-3, macroblock_type in P-pictures, codes 000001..00011x */
static VLCtab PMBtab1[8] = {
  {ERROR,0},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,6},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,5}, {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,5}, {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_INTRA,5}, {MACROBLOCK_INTRA,5}
};
/*}}}*/
/*{{{*/
/* Table B-4, macroblock_type in B-pictures, codes 0010..11xx */
static VLCtab BMBtab0[16] = {
  {ERROR,0},
  {ERROR,0},
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
/*}}}*/
/*{{{*/
/* Table B-4, macroblock_type in B-pictures, codes 000001..00011x */
static VLCtab BMBtab1[8] = {
  {ERROR,0},
  {MACROBLOCK_QUANT|MACROBLOCK_INTRA,6},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,6},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_PATTERN,6},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_QUANT|MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD|MACROBLOCK_PATTERN,5},
  {MACROBLOCK_INTRA,5},
  {MACROBLOCK_INTRA,5}
};
/*}}}*/

/*{{{*/
/* Table B-5, macroblock_type in spat. scal. I-pictures, codes 0001..1xxx */
static VLCtab spIMBtab[16] = {
  {ERROR,0},
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
/*}}}*/
/*{{{*/
/* Table B-6, macroblock_type in spat. scal. P-pictures, codes 0010..11xx */
static VLCtab spPMBtab0[16] =
{
  {ERROR,0},
  {ERROR,0},
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
/*}}}*/
/*{{{*/
/* Table B-6, macroblock_type in spat. scal. P-pictures, codes 0000010..000111x */
static VLCtab spPMBtab1[16] = {
  {ERROR,0},
  {ERROR,0},
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/

/*{{{*/
/* Table B-8, macroblock_type in spat. scal. B-pictures, codes 001..1xx */
static VLCtab SNRMBtab[8] = {
  {ERROR,0},
  {0,3},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_QUANT|MACROBLOCK_PATTERN,2},
  {MACROBLOCK_PATTERN,1},
  {MACROBLOCK_PATTERN,1},
  {MACROBLOCK_PATTERN,1},
  {MACROBLOCK_PATTERN,1}
};
/*}}}*/

/*{{{*/
/* Table B-10, motion_code, codes 0001 ... 01xx */
static VLCtab MVtab0[8] =
{ {ERROR,0}, {3,3}, {2,2}, {2,2}, {1,1}, {1,1}, {1,1}, {1,1}
};
/*}}}*/
/*{{{*/
/* Table B-10, motion_code, codes 0000011 ... 000011x */
static VLCtab MVtab1[8] =
{ {ERROR,0}, {ERROR,0}, {ERROR,0}, {7,6}, {6,6}, {5,6}, {4,5}, {4,5}
};
/*}}}*/
/*{{{*/
/* Table B-10, motion_code, codes 0000001100 ... 000001011x */
static VLCtab MVtab2[12] =
{ {16,9}, {15,9}, {14,9}, {13,9},
  {12,9}, {11,9}, {10,8}, {10,8},
  {9,8},  {9,8},  {8,8},  {8,8}
};
/*}}}*/

/*{{{*/
/* Table B-9, coded_block_pattern, codes 01000 ... 111xx */
static VLCtab CBPtab0[32] =
{ {ERROR,0}, {ERROR,0}, {ERROR,0}, {ERROR,0},
  {ERROR,0}, {ERROR,0}, {ERROR,0}, {ERROR,0},
  {62,5}, {2,5},  {61,5}, {1,5},  {56,5}, {52,5}, {44,5}, {28,5},
  {40,5}, {20,5}, {48,5}, {12,5}, {32,4}, {32,4}, {16,4}, {16,4},
  {8,4},  {8,4},  {4,4},  {4,4},  {60,3}, {60,3}, {60,3}, {60,3}
};
/*}}}*/
/*{{{*/
/* Table B-9, coded_block_pattern, codes 00000100 ... 001111xx */
static VLCtab CBPtab1[64] =
{ {ERROR,0}, {ERROR,0}, {ERROR,0}, {ERROR,0},
  {58,8}, {54,8}, {46,8}, {30,8},
  {57,8}, {53,8}, {45,8}, {29,8}, {38,8}, {26,8}, {37,8}, {25,8},
  {43,8}, {23,8}, {51,8}, {15,8}, {42,8}, {22,8}, {50,8}, {14,8},
  {41,8}, {21,8}, {49,8}, {13,8}, {35,8}, {19,8}, {11,8}, {7,8},
  {34,7}, {34,7}, {18,7}, {18,7}, {10,7}, {10,7}, {6,7},  {6,7},
  {33,7}, {33,7}, {17,7}, {17,7}, {9,7},  {9,7},  {5,7},  {5,7},
  {63,6}, {63,6}, {63,6}, {63,6}, {3,6},  {3,6},  {3,6},  {3,6},
  {36,6}, {36,6}, {36,6}, {36,6}, {24,6}, {24,6}, {24,6}, {24,6}
};
/*}}}*/
/*{{{*/
/* Table B-9, coded_block_pattern, codes 000000001 ... 000000111 */
static VLCtab CBPtab2[8] =
{ {ERROR,0}, {0,9}, {39,9}, {27,9}, {59,9}, {55,9}, {47,9}, {31,9}
};
/*}}}*/

/*{{{*/
/* Table B-1, macroblock_address_increment, codes 00010 ... 011xx */
static VLCtab MBAtab1[16] =
{ {ERROR,0}, {ERROR,0}, {7,5}, {6,5}, {5,4}, {5,4}, {4,4}, {4,4},
  {3,3}, {3,3}, {3,3}, {3,3}, {2,3}, {2,3}, {2,3}, {2,3}
};
/*}}}*/
/*{{{*/
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
/*}}}*/

/*{{{*/
/* Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110 */
static VLCtab DClumtab0[32] =
{ {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
  {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
  {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
  {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {ERROR, 0}
};
/*}}}*/
/*{{{*/
/* Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111 */
static VLCtab DClumtab1[16] =
{ {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
  {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9}
};
/*}}}*/
/*{{{*/
/* Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110 */
static VLCtab DCchromtab0[32] =
{ {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
  {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
  {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
  {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {ERROR, 0}
};
/*}}}*/
/*{{{*/
/* Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111 */
static VLCtab DCchromtab1[32] =
{ {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
  {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
  {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
  {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10}
};
/*}}}*/

/*{{{  struct DCTtab*/
typedef struct {
  char run, level, len;
  } DCTtab;
/*}}}*/
/*{{{*/
/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for first (DC) coefficient)
 */
DCTtab DCTtabfirst[12] =
{
  {0,2,4}, {2,1,4}, {1,1,3}, {1,1,3},
  {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1},
  {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1}
};
/*}}}*/
/*{{{*/
/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for all other coefficients)
 */
DCTtab DCTtabnext[12] =
{
  {0,2,4},  {2,1,4},  {1,1,3},  {1,1,3},
  {64,0,2}, {64,0,2}, {64,0,2}, {64,0,2}, /* EOB */
  {0,1,2},  {0,1,2},  {0,1,2},  {0,1,2}
};
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
/* Table B-14, DCT coefficients table zero,
 * codes 0000001000 ... 0000001111
 */
DCTtab DCTtab1[8] =
{
  {16,1,10}, {5,2,10}, {0,7,10}, {2,3,10},
  {1,4,10}, {15,1,10}, {14,1,10}, {4,2,10}
};
/*}}}*/
/*{{{*/
/* Table B-15, DCT coefficients table one,
 * codes 000000100x ... 000000111x
 */
DCTtab DCTtab1a[8] =
{
  {5,2,9}, {5,2,9}, {14,1,9}, {14,1,9},
  {2,4,10}, {16,1,10}, {15,1,9}, {15,1,9}
};
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*{{{*/
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
/*}}}*/
/*}}}*/
/*{{{  vars*/
int True_Framenum;
int True_Framenum_max  = -1;
int Temporal_Reference_Base = 0;
int Temporal_Reference_GOP_Reset = 0;

/* decoder operation control flags */
int Fault_Flag = 0;
int System_Stream_Flag = 0;
int Display_Progressive_Flag = 0;
int Spatial_Flag = 0;
int Trace_Flag = 0;
int Ersatz_Flag = 0;
int User_Data_Flag = 0;

/* filenames */
char* Substitute_Picture_Filename;
char* Enhancement_Layer_Bitstream_Filename;

/* buffers for multiuse purposes */
char Error_Text[256];
unsigned char* Clip;

/* pointers to generic picture buffers */
unsigned char* backward_reference_frame[3];
unsigned char* forward_reference_frame[3];
unsigned char* auxframe[3];
unsigned char* current_frame[3];
unsigned char* substitute_frame[3];

/* pointers to scalability picture buffers */
unsigned char* llframe0[3];
unsigned char* llframe1[3];

short* lltmp;
char* Lower_Layer_Picture_Filename;
/*{{{  non-normative variables derived from normative elements*/
int Coded_Picture_Width;
int Coded_Picture_Height;

int Chroma_Width;
int Chroma_Height;

int block_count;
int Second_Field;

int profile, level;
/*}}}*/
/*{{{  normative derived variables (as per ISO/IEC 13818-2)*/
int horizontal_size;
int vertical_size;
int mb_width;
int mb_height;
double bit_rate;
double frame_rate;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.2.1:  sequence_header()*/
int aspect_ratio_information;
int frame_rate_code;
int bit_rate_value;
int vbv_buffer_size;
int constrained_parameters_flag;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.2.3:  sequence_extension()*/
int profile_and_level_indication;
int progressive_sequence;
int chroma_format;
int low_delay;
int frame_rate_extension_n;
int frame_rate_extension_d;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.2.4:  sequence_display_extension()*/
int video_format;
int color_description;
int color_primaries;
int transfer_characteristics;
int matrix_coefficients;
int display_horizontal_size;
int display_vertical_size;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.3: picture_header()*/
int temporal_reference;
int picture_coding_type;
int vbv_delay;
int full_pel_forward_vector;
int forward_f_code;
int full_pel_backward_vector;
int backward_f_code;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.3.1: picture_coding_extension() header*/
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
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.3.3: picture_display_extension() header*/
int frame_center_horizontal_offset[3];
int frame_center_vertical_offset[3];
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.2.5: sequence_scalable_extension() header*/
int layer_id;
int lower_layer_prediction_horizontal_size;
int lower_layer_prediction_vertical_size;
int horizontal_subsampling_factor_m;
int horizontal_subsampling_factor_n;
int vertical_subsampling_factor_m;
int vertical_subsampling_factor_n;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.3.5: picture_spatial_scalable_extension() header*/
int lower_layer_temporal_reference;
int lower_layer_horizontal_offset;
int lower_layer_vertical_offset;
int spatial_temporal_weight_code_table_index;
int lower_layer_progressive_frame;
int lower_layer_deinterlaced_field_select;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.3.6: copyright_extension() header*/
int copyright_flag;
int copyright_identifier;
int original_or_copy;
int copyright_number_1;
int copyright_number_2;
int copyright_number_3;
/*}}}*/
/*{{{  ISO/IEC 13818-2 section 6.2.2.6: group_of_pictures_header()*/
int drop_flag;
int hour;
int minute;
int sec;
int frame;
int closed_gop;
int broken_link;
/*}}}*/

/*{{{  struct layer_data*/
/* layer specific variables (needed for SNR and DP scalability) */
struct layer_data {
  /* bit input */
  int Infile;
  unsigned char Rdbfr[2048];
  unsigned char *Rdptr;
  unsigned char Inbfr[16];

  /* from mpeg2play */
  unsigned int Bfr;
  unsigned char *Rdmax;
  int Incnt;
  int Bitcnt;

  /* sequence header and quant_matrix_extension() */
  int intra_quantizer_matrix[64];
  int non_intra_quantizer_matrix[64];
  int chroma_intra_quantizer_matrix[64];
  int chroma_non_intra_quantizer_matrix[64];

  int load_intra_quantizer_matrix;
  int load_non_intra_quantizer_matrix;
  int load_chroma_intra_quantizer_matrix;
  int load_chroma_non_intra_quantizer_matrix;

  int MPEG2_Flag;

  /* sequence scalable extension */
  int scalable_mode;

  /* picture coding extension */
  int q_scale_type;
  int alternate_scan;

  /* picture spatial scalable extension */
  int pict_scal;

  /* slice/macroblock */
  int priority_breakpoint;
  int quantizer_scale;
  int intra_slice;
  short block[12][64];
  } base, enhan, *ld;
/*}}}*/
int Decode_Layer;
/*}}}*/

/*{{{*/
void Error (char *text) {

  printf (text);
  exit (1);
  }
/*}}}*/

/*{{{  fast idct*/
#ifndef PI
  #ifdef M_PI
    #define PI M_PI
  #else
    #define PI 3.14159265358979323846
  #endif
#endif

#define W1 2841 /* 2048*sqrt(2)*cos(1*pi/16) */
#define W2 2676 /* 2048*sqrt(2)*cos(2*pi/16) */
#define W3 2408 /* 2048*sqrt(2)*cos(3*pi/16) */
#define W5 1609 /* 2048*sqrt(2)*cos(5*pi/16) */
#define W6 1108 /* 2048*sqrt(2)*cos(6*pi/16) */
#define W7 565  /* 2048*sqrt(2)*cos(7*pi/16) */

static short iclip[1024]; /* clipping table */
static short *iclp;

/*{{{*/
/* row (horizontal) IDCT
 *
 *           7                       pi         1
 * dst[k] = sum c[l] * src[l] * cos( -- * ( k + - ) * l )
 *          l=0                      8          2
 *
 * where: c[0]    = 128
 *        c[1..7] = 128*sqrt(2)
 */

static void idctrow (blk)
short *blk;
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  if (!((x1 = blk[4]<<11) | (x2 = blk[6]) | (x3 = blk[2]) |
        (x4 = blk[1]) | (x5 = blk[7]) | (x6 = blk[5]) | (x7 = blk[3])))
  {
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
/*}}}*/
/*{{{*/
/* column (vertical) IDCT
 *
 *             7                         pi         1
 * dst[8*k] = sum c[l] * src[8*l] * cos( -- * ( k + - ) * l )
 *            l=0                        8          2
 *
 * where: c[0]    = 1/1024
 *        c[1..7] = (1/1024)*sqrt(2)
 */
static void idctcol (blk)
short *blk;
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  if (!((x1 = (blk[8*4]<<8)) | (x2 = blk[8*6]) | (x3 = blk[8*2]) |
        (x4 = blk[8*1]) | (x5 = blk[8*7]) | (x6 = blk[8*5]) | (x7 = blk[8*3])))
  {
    blk[8*0]=blk[8*1]=blk[8*2]=blk[8*3]=blk[8*4]=blk[8*5]=blk[8*6]=blk[8*7]=
      iclp[(blk[8*0]+32)>>6];
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
/*}}}*/
/*{{{*/
/* two dimensional inverse discrete cosine transform */
void Fast_IDCT (block)
short *block;
{
  int i;

  for (i=0; i<8; i++)
    idctrow(block+8*i);

  for (i=0; i<8; i++)
    idctcol(block+i);
}
/*}}}*/
/*{{{*/
void Initialize_Fast_IDCT()
{
  int i;

  iclp = iclip+512;
  for (i= -512; i<512; i++)
    iclp[i] = (i<-256) ? -256 : ((i>255) ? 255 : i);
}
/*}}}*/
/*}}}*/
/*{{{*/
void Write_Frame (unsigned char*src[], int frame) {

  if (progressive_sequence || progressive_frame)
    printf ("writeFrame prog %d, %d %d\n", frame,Coded_Picture_Width, Coded_Picture_Height);
  else
    printf ("writeFrame interlaced %d, %d %d\n", frame,Coded_Picture_Width, Coded_Picture_Height);
}
/*}}}*/

// getBits
/*{{{*/
/* Trace_Flag output */
void Print_Bits (int code, int bits, int len) {

  int i;
  for (i = 0; i < len; i++)
    printf ("%d", (code>>(bits-1-i))&1);
  }
/*}}}*/
/*{{{*/
/* MPEG-1 system layer demultiplexer */
int Get_Byte() {

  while(ld->Rdptr >= ld->Rdbfr+2048) {
    read(ld->Infile,ld->Rdbfr,2048);
    ld->Rdptr -= 2048;
    ld->Rdmax -= 2048;
    }

  return *ld->Rdptr++;
  }
/*}}}*/
/*{{{*/
/* extract a 16-bit word from the bitstream buffer */
int Get_Word() {

  int Val = Get_Byte();
  return (Val<<8) | Get_Byte();
  }
/*}}}*/
/*{{{*/
int Get_Long() {

  int i = Get_Word();
  return (i<<16) | Get_Word();
  }
/*}}}*/
/*{{{*/
/* parse system layer, ignore everything we don't need */
void Next_Packet() {

  unsigned int code;
  int l;
  for(;;) {
    code = Get_Long();

    /* remove system layer byte stuffing */
    while ((code & 0xffffff00) != 0x100)
      code = (code<<8) | Get_Byte();

    switch(code) {
      /*{{{*/
      case PACK_START_CODE: /* pack header */
        /* skip pack header (system_clock_reference and mux_rate) */
        ld->Rdptr += 8;
        break;
      /*}}}*/
      /*{{{*/
      case VIDEO_ELEMENTARY_STREAM:
        code = Get_Word();             /* packet_length */
        ld->Rdmax = ld->Rdptr + code;

        code = Get_Byte();
        if((code>>6)==0x02) {
          ld->Rdptr++;
          code = Get_Byte();  /* parse PES_header_data_length */
          ld->Rdptr += code;    /* advance pointer by PES_header_data_length */
          printf ("MPEG-2 PES packet\n");
          return;
          }
        else if(code==0xff) {
          /* parse MPEG-1 packet header */
          while ((code = Get_Byte()) == 0xFF);
          }

        /* stuffing bytes */
        if (code >= 0x40) {
          if (code >= 0x80) {
            fprintf (stderr,"Error in packet header\n");
            exit(1);
            }

          /* skip STD_buffer_scale */
          ld->Rdptr++;
          code = Get_Byte();
        }

        if (code >= 0x30) {
          if (code >= 0x40) {
            fprintf (stderr,"Error in packet header\n");
            exit (1);
            }
          /* skip presentation and decoding time stamps */
          ld->Rdptr += 9;
          }

        else if (code>=0x20) {
          /* skip presentation time stamps */
          ld->Rdptr += 4;
          }

        else if (code!=0x0f) {
          fprintf(stderr,"Error in packet header\n");
          exit(1);
          }

        return;
      /*}}}*/
      /*{{{*/
      case ISO_END_CODE: /* end */
        /* simulate a buffer full of sequence end codes */
        l = 0;
        while (l<2048) {
          ld->Rdbfr[l++] = SEQUENCE_END_CODE>>24;
          ld->Rdbfr[l++] = SEQUENCE_END_CODE>>16;
          ld->Rdbfr[l++] = SEQUENCE_END_CODE>>8;
          ld->Rdbfr[l++] = SEQUENCE_END_CODE&0xff;
          }

        ld->Rdptr = ld->Rdbfr;
        ld->Rdmax = ld->Rdbfr + 2048;
        return;
      /*}}}*/
      default:
        if (code>=SYSTEM_START_CODE) {
          /* skip system headers and non-video packets*/
          code = Get_Word();
          ld->Rdptr += code;
          }
        else {
          fprintf(stderr,"Unexpected startcode %08x in system layer\n",code);
          exit(1);
          }
        break;
      }
    }
  }
/*}}}*/
/*{{{*/
void Fill_Buffer() {

  int Buffer_Level = read (ld->Infile,ld->Rdbfr,2048);
  ld->Rdptr = ld->Rdbfr;

  if (System_Stream_Flag)
    ld->Rdmax -= 2048;

  /* end of the bitstream file */
  if (Buffer_Level < 2048) {
    /* just to be safe */
    if (Buffer_Level < 0)
      Buffer_Level = 0;

    /* pad until the next to the next 32-bit word boundary */
    while (Buffer_Level & 3)
      ld->Rdbfr[Buffer_Level++] = 0;

    /* pad the buffer with sequence end codes */
    while (Buffer_Level < 2048) {
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>24;
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>16;
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>8;
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE&0xff;
      }
    }
  }
/*}}}*/
/*{{{*/
/* advance by n bits */
void Flush_Buffer (int N) {

  int Incnt;
  ld->Bfr <<= N;
  Incnt = ld->Incnt -= N;

  if (Incnt <= 24) {
    if (System_Stream_Flag && (ld->Rdptr >= ld->Rdmax-4)) {
      do {
        if (ld->Rdptr >= ld->Rdmax)
          Next_Packet();
        ld->Bfr |= Get_Byte() << (24 - Incnt);
        Incnt += 8;
        }
      while (Incnt <= 24);
      }
    else if (ld->Rdptr < ld->Rdbfr+2044) {
      do {
        ld->Bfr |= *ld->Rdptr++ << (24 - Incnt);
        Incnt += 8;
        }
      while (Incnt <= 24);
      }
    else {
      do {
        if (ld->Rdptr >= ld->Rdbfr+2048)
          Fill_Buffer();
        ld->Bfr |= *ld->Rdptr++ << (24 - Incnt);
        Incnt += 8;
        }
      while (Incnt <= 24);
      }
    ld->Incnt = Incnt;
    }
  }
/*}}}*/
/*{{{*/
void Flush_Buffer32() {

  int Incnt;
  ld->Bfr = 0;
  Incnt = ld->Incnt;
  Incnt -= 32;

  if (System_Stream_Flag && (ld->Rdptr >= ld->Rdmax-4)) {
    while (Incnt <= 24) {
      if (ld->Rdptr >= ld->Rdmax)
        Next_Packet();
      ld->Bfr |= Get_Byte() << (24 - Incnt);
      Incnt += 8;
      }
    }
  else {
    while (Incnt <= 24) {
      if (ld->Rdptr >= ld->Rdbfr+2048) Fill_Buffer();
      ld->Bfr |= *ld->Rdptr++ << (24 - Incnt);
      Incnt += 8;
      }
    }

  ld->Incnt = Incnt;
  }
/*}}}*/
/*{{{*/
/* return next n bits (right adjusted) without advancing */
unsigned int Show_Bits (int N) {
  return ld->Bfr >> (32-N);
  }
/*}}}*/
/*{{{*/
/* return next n bits (right adjusted) */
unsigned int Get_Bits (int N) {

  unsigned int Val = Show_Bits(N);
  Flush_Buffer(N);
  return Val;
  }
/*}}}*/
/*{{{*/
/* return next bit (could be made faster than Get_Bits(1)) */
unsigned int Get_Bits1() {
  return Get_Bits (1);
  }
/*}}}*/
/*{{{*/
unsigned int Get_Bits32() {

  unsigned int l = Show_Bits (32);
  Flush_Buffer32();
  return l;
  }
/*}}}*/
/*{{{*/
/* initialize buffer, call once before first getbits or showbits */
void Initialize_Buffer() {

  ld->Incnt = 0;
  ld->Rdptr = ld->Rdbfr + 2048;
  ld->Rdmax = ld->Rdptr;

  ld->Bfr = 0;
  Flush_Buffer(0); /* fills valid data into bfr */
  }
/*}}}*/

/*{{{*/
/* align to start of next next_start_code */
void next_start_code() {

  /* byte align */
  Flush_Buffer (ld->Incnt&7);
  while (Show_Bits(24) != 0x01L)
    Flush_Buffer(8);
  }
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 section 5.3 */
/* Purpose: this function is mainly designed to aid in bitstream conformance
   testing.  A simple Flush_Buffer(1) would do */
void marker_bit (char* text) {
  int marker = Get_Bits(1);
  }
/*}}}*/
/*{{{*/
/* decode extra bit information */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static int extra_bit_information() {

  int Byte_Count = 0;
  while (Get_Bits1()) {
    Flush_Buffer (8);
    Byte_Count++;
    }

  return Byte_Count;
  }
/*}}}*/

/*{{{*/
static char* MBdescr[]={
  "",                  "Intra",        "No MC, Coded",         "",
  "Bwd, Not Coded",    "",             "Bwd, Coded",           "",
  "Fwd, Not Coded",    "",             "Fwd, Coded",           "",
  "Interp, Not Coded", "",             "Interp, Coded",        "",
  "",                  "Intra, Quant", "No MC, Coded, Quant",  "",
  "",                  "",             "Bwd, Coded, Quant",    "",
  "",                  "",             "Fwd, Coded, Quant",    "",
  "",                  "",             "Interp, Coded, Quant", ""
};
/*}}}*/
/*{{{*/
static int Get_I_macroblock_type()
{
#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_type(I) ");
#endif /* TRACE */

  if (Get_Bits1())
  {
#ifdef TRACE
    if (Trace_Flag)
      printf("(1): Intra (1)\n");
#endif /* TRACE */
    return 1;
  }

  if (!Get_Bits1())
  {
    printf ("Invalid macroblock_type code\n");
    Fault_Flag = 1;
  }

#ifdef TRACE
  if (Trace_Flag)
    printf("(01): Intra, Quant (17)\n");
#endif /* TRACE */

  return 17;
}
/*}}}*/
/*{{{*/
static int Get_P_macroblock_type()
{
  int code;

#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_type(P) (");
#endif /* TRACE */

  if ((code = Show_Bits(6))>=8)
  {
    code >>= 3;
    Flush_Buffer(PMBtab0[code].len);
#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,3,PMBtab0[code].len);
      printf("): %s (%d)\n",MBdescr[(int)PMBtab0[code].val],PMBtab0[code].val);
    }
#endif /* TRACE */
    return PMBtab0[code].val;
  }

  if (code==0)
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag = 1;
    return 0;
  }

  Flush_Buffer(PMBtab1[code].len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code,6,PMBtab1[code].len);
    printf("): %s (%d)\n",MBdescr[(int)PMBtab1[code].val],PMBtab1[code].val);
  }
#endif /* TRACE */

  return PMBtab1[code].val;
}
/*}}}*/
/*{{{*/
static int Get_B_macroblock_type()
{
  int code;

#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_type(B) (");
#endif /* TRACE */

  if ((code = Show_Bits(6))>=8)
  {
    code >>= 2;
    Flush_Buffer(BMBtab0[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,4,BMBtab0[code].len);
      printf("): %s (%d)\n",MBdescr[(int)BMBtab0[code].val],BMBtab0[code].val);
    }
#endif /* TRACE */

    return BMBtab0[code].val;
  }

  if (code==0)
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag = 1;
    return 0;
  }

  Flush_Buffer(BMBtab1[code].len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code,6,BMBtab1[code].len);
    printf("): %s (%d)\n",MBdescr[(int)BMBtab1[code].val],BMBtab1[code].val);
  }
#endif /* TRACE */

  return BMBtab1[code].val;
}
/*}}}*/
/*{{{*/
static int Get_D_macroblock_type()
{
  if (!Get_Bits1())
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag=1;
  }

  return 1;
}
/*}}}*/
/*{{{*/
/* macroblock_type for pictures with spatial scalability */
static int Get_I_Spatial_macroblock_type()
{
  int code;

#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_type(I,spat) (");
#endif /* TRACE */

  code = Show_Bits(4);

  if (code==0)
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag = 1;
    return 0;
  }

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code,4,spIMBtab[code].len);
    printf("): %02x\n",spIMBtab[code].val);
  }
#endif /* TRACE */

  Flush_Buffer(spIMBtab[code].len);
  return spIMBtab[code].val;
}
/*}}}*/
/*{{{*/
static int Get_P_Spatial_macroblock_type()
{
  int code;

#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_type(P,spat) (");
#endif /* TRACE */

  code = Show_Bits(7);

  if (code<2)
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag = 1;
    return 0;
  }

  if (code>=16)
  {
    code >>= 3;
    Flush_Buffer(spPMBtab0[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,4,spPMBtab0[code].len);
      printf("): %02x\n",spPMBtab0[code].val);
    }
#endif /* TRACE */

    return spPMBtab0[code].val;
  }

  Flush_Buffer(spPMBtab1[code].len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code,7,spPMBtab1[code].len);
    printf("): %02x\n",spPMBtab1[code].val);
  }
#endif /* TRACE */

  return spPMBtab1[code].val;
}
/*}}}*/
/*{{{*/
static int Get_B_Spatial_macroblock_type()
{
  int code;
  VLCtab *p;

#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_type(B,spat) (");
#endif /* TRACE */

  code = Show_Bits(9);

  if (code>=64)
    p = &spBMBtab0[(code>>5)-2];
  else if (code>=16)
    p = &spBMBtab1[(code>>2)-4];
  else if (code>=8)
    p = &spBMBtab2[code-8];
  else
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag = 1;
    return 0;
  }

  Flush_Buffer(p->len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code,9,p->len);
    printf("): %02x\n",p->val);
  }
#endif /* TRACE */

  return p->val;
}
/*}}}*/
/*{{{*/
static int Get_SNR_macroblock_type()
{
  int code;

#ifdef TRACE      /* *CH* */
  if (Trace_Flag)
    printf("macroblock_type(SNR) (");
#endif TRACE

  code = Show_Bits(3);

  if (code==0)
  {
    printf("Invalid macroblock_type code\n");
    Fault_Flag = 1;
    return 0;
  }

  Flush_Buffer(SNRMBtab[code].len);

#ifdef TRACE      /* *CH* */
  if (Trace_Flag)
  {
    Print_Bits(code,3,SNRMBtab[code].len);
    printf("): %s (%d)\n",MBdescr[(int)SNRMBtab[code].val],SNRMBtab[code].val);
  }
#endif TRACE


  return SNRMBtab[code].val;
}
/*}}}*/

/*{{{*/
int Get_macroblock_type()
{
  int macroblock_type = 0;

  if (ld->scalable_mode==SC_SNR)
    macroblock_type = Get_SNR_macroblock_type();
  else
  {
    switch (picture_coding_type)
    {
    case I_TYPE:
      macroblock_type = ld->pict_scal ? Get_I_Spatial_macroblock_type() : Get_I_macroblock_type();
      break;
    case P_TYPE:
      macroblock_type = ld->pict_scal ? Get_P_Spatial_macroblock_type() : Get_P_macroblock_type();
      break;
    case B_TYPE:
      macroblock_type = ld->pict_scal ? Get_B_Spatial_macroblock_type() : Get_B_macroblock_type();
      break;
    case D_TYPE:
      macroblock_type = Get_D_macroblock_type();
      break;
    default:
      printf("Get_macroblock_type(): unrecognized picture coding type\n");
      break;
    }
  }

  return macroblock_type;
}
/*}}}*/
/*{{{*/
int Get_motion_code()
{
  int code;

#ifdef TRACE
  if (Trace_Flag)
    printf("motion_code (");
#endif /* TRACE */

  if (Get_Bits1())
  {
#ifdef TRACE
    if (Trace_Flag)
      printf("0): 0\n");
#endif /* TRACE */
    return 0;
  }

  if ((code = Show_Bits(9))>=64)
  {
    code >>= 6;
    Flush_Buffer(MVtab0[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,3,MVtab0[code].len);
      printf("%d): %d\n",
        Show_Bits(1),Show_Bits(1)?-MVtab0[code].val:MVtab0[code].val);
    }
#endif /* TRACE */

    return Get_Bits1()?-MVtab0[code].val:MVtab0[code].val;
  }

  if (code>=24)
  {
    code >>= 3;
    Flush_Buffer(MVtab1[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,6,MVtab1[code].len);
      printf("%d): %d\n",
        Show_Bits(1),Show_Bits(1)?-MVtab1[code].val:MVtab1[code].val);
    }
#endif /* TRACE */

    return Get_Bits1()?-MVtab1[code].val:MVtab1[code].val;
  }

  if ((code-=12)<0)
  {
    Fault_Flag=1;
    return 0;
  }

  Flush_Buffer(MVtab2[code].len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code+12,9,MVtab2[code].len);
    printf("%d): %d\n",
      Show_Bits(1),Show_Bits(1)?-MVtab2[code].val:MVtab2[code].val);
  }
#endif /* TRACE */

  return Get_Bits1() ? -MVtab2[code].val : MVtab2[code].val;
}
/*}}}*/
/*{{{*/
/* get differential motion vector (for dual prime prediction) */
int Get_dmvector()
{
#ifdef TRACE
  if (Trace_Flag)
    printf("dmvector (");
#endif /* TRACE */

  if (Get_Bits(1))
  {
#ifdef TRACE
    if (Trace_Flag)
      printf(Show_Bits(1) ? "11): -1\n" : "10): 1\n");
#endif /* TRACE */
    return Get_Bits(1) ? -1 : 1;
  }
  else
  {
#ifdef TRACE
    if (Trace_Flag)
      printf("0): 0\n");
#endif /* TRACE */
    return 0;
  }
}
/*}}}*/
/*{{{*/
int Get_coded_block_pattern()
{
  int code;

#ifdef TRACE
  if (Trace_Flag)
    printf("coded_block_pattern_420 (");
#endif /* TRACE */

  if ((code = Show_Bits(9))>=128)
  {
    code >>= 4;
    Flush_Buffer(CBPtab0[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,5,CBPtab0[code].len);
      printf("): ");
      Print_Bits(CBPtab0[code].val,6,6);
      printf(" (%d)\n",CBPtab0[code].val);
    }
#endif /* TRACE */

    return CBPtab0[code].val;
  }

  if (code>=8)
  {
    code >>= 1;
    Flush_Buffer(CBPtab1[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,8,CBPtab1[code].len);
      printf("): ");
      Print_Bits(CBPtab1[code].val,6,6);
      printf(" (%d)\n",CBPtab1[code].val);
    }
#endif /* TRACE */

    return CBPtab1[code].val;
  }

  if (code<1)
  {
    printf("Invalid coded_block_pattern code\n");
    Fault_Flag = 1;
    return 0;
  }

  Flush_Buffer(CBPtab2[code].len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code,9,CBPtab2[code].len);
    printf("): ");
    Print_Bits(CBPtab2[code].val,6,6);
    printf(" (%d)\n",CBPtab2[code].val);
  }
#endif /* TRACE */

  return CBPtab2[code].val;
}
/*}}}*/
/*{{{*/
int Get_macroblock_address_increment()
{
  int code, val;

#ifdef TRACE
  if (Trace_Flag)
    printf("macroblock_address_increment (");
#endif /* TRACE */

  val = 0;

  while ((code = Show_Bits(11))<24)
  {
    if (code!=15) /* if not macroblock_stuffing */
    {
      if (code==8) /* if macroblock_escape */
      {
#ifdef TRACE
        if (Trace_Flag)
          printf("00000001000 ");
#endif /* TRACE */

        val+= 33;
      }
      else
      {
        printf("Invalid macroblock_address_increment code\n");
        Fault_Flag = 1;
        return 1;
      }
    }
    else /* macroblock suffing */
    {
#ifdef TRACE
      if (Trace_Flag)
        printf("00000001111 ");
#endif /* TRACE */
    }

    Flush_Buffer(11);
  }

  /* macroblock_address_increment == 1 */
  /* ('1' is in the MSB position of the lookahead) */
  if (code>=1024)
  {
    Flush_Buffer(1);
#ifdef TRACE
    if (Trace_Flag)
      printf("1): %d\n",val+1);
#endif /* TRACE */
    return val + 1;
  }

  /* codes 00010 ... 011xx */
  if (code>=128)
  {
    /* remove leading zeros */
    code >>= 6;
    Flush_Buffer(MBAtab1[code].len);

#ifdef TRACE
    if (Trace_Flag)
    {
      Print_Bits(code,5,MBAtab1[code].len);
      printf("): %d\n",val+MBAtab1[code].val);
    }
#endif /* TRACE */


    return val + MBAtab1[code].val;
  }

  /* codes 00000011000 ... 0000111xxxx */
  code-= 24; /* remove common base */
  Flush_Buffer(MBAtab2[code].len);

#ifdef TRACE
  if (Trace_Flag)
  {
    Print_Bits(code+24,11,MBAtab2[code].len);
    printf("): %d\n",val+MBAtab2[code].val);
  }
#endif /* TRACE */

  return val + MBAtab2[code].val;
}
/*}}}*/
/*{{{*/
/* combined MPEG-1 and MPEG-2 stage. parse VLC and
   perform dct_diff arithmetic.

   MPEG-1:  ISO/IEC 11172-2 section
   MPEG-2:  ISO/IEC 13818-2 section 7.2.1

   Note: the arithmetic here is presented more elegantly than
   the spec, yet the results, dct_diff, are the same.
*/

int Get_Luma_DC_dct_diff()
{
  int code, size, dct_diff;

#ifdef TRACE
/*
  if (Trace_Flag)
    printf("dct_dc_size_luminance: (");
*/
#endif /* TRACE */

  /* decode length */
  code = Show_Bits(5);

  if (code<31)
  {
    size = DClumtab0[code].val;
    Flush_Buffer(DClumtab0[code].len);
#ifdef TRACE
/*
    if (Trace_Flag)
    {
      Print_Bits(code,5,DClumtab0[code].len);
      printf("): %d",size);
    }
*/
#endif /* TRACE */
  }
  else
  {
    code = Show_Bits(9) - 0x1f0;
    size = DClumtab1[code].val;
    Flush_Buffer(DClumtab1[code].len);

#ifdef TRACE
/*
    if (Trace_Flag)
    {
      Print_Bits(code+0x1f0,9,DClumtab1[code].len);
      printf("): %d",size);
    }
*/
#endif /* TRACE */
  }

#ifdef TRACE
/*
  if (Trace_Flag)
    printf(", dct_dc_differential (");
*/
#endif /* TRACE */

  if (size==0)
    dct_diff = 0;
  else
  {
    dct_diff = Get_Bits(size);
#ifdef TRACE
/*
    if (Trace_Flag)
      Print_Bits(dct_diff,size,size);
*/
#endif /* TRACE */
    if ((dct_diff & (1<<(size-1)))==0)
      dct_diff-= (1<<size) - 1;
  }

#ifdef TRACE
/*
  if (Trace_Flag)
    printf("): %d\n",dct_diff);
*/
#endif /* TRACE */

  return dct_diff;
}
/*}}}*/
/*{{{*/
int Get_Chroma_DC_dct_diff()
{
  int code, size, dct_diff;

#ifdef TRACE
/*
  if (Trace_Flag)
    printf("dct_dc_size_chrominance: (");
*/
#endif /* TRACE */

  /* decode length */
  code = Show_Bits(5);

  if (code<31)
  {
    size = DCchromtab0[code].val;
    Flush_Buffer(DCchromtab0[code].len);

#ifdef TRACE
/*
    if (Trace_Flag)
    {
      Print_Bits(code,5,DCchromtab0[code].len);
      printf("): %d",size);
    }
*/
#endif /* TRACE */
  }
  else
  {
    code = Show_Bits(10) - 0x3e0;
    size = DCchromtab1[code].val;
    Flush_Buffer(DCchromtab1[code].len);

#ifdef TRACE
/*
    if (Trace_Flag)
    {
      Print_Bits(code+0x3e0,10,DCchromtab1[code].len);
      printf("): %d",size);
    }
*/
#endif /* TRACE */
  }

#ifdef TRACE
/*
  if (Trace_Flag)
    printf(", dct_dc_differential (");
*/
#endif /* TRACE */

  if (size==0)
    dct_diff = 0;
  else
  {
    dct_diff = Get_Bits(size);
#ifdef TRACE
/*
    if (Trace_Flag)
      Print_Bits(dct_diff,size,size);
*/
#endif /* TRACE */
    if ((dct_diff & (1<<(size-1)))==0)
      dct_diff-= (1<<size) - 1;
  }

#ifdef TRACE
/*
  if (Trace_Flag)
    printf("): %d\n",dct_diff);
*/
#endif /* TRACE */

  return dct_diff;
}
/*}}}*/

/*{{{*/
/* decode sequence display extension */

static void sequence_display_extension()
{
  int pos;

  pos = ld->Bitcnt;
  video_format      = Get_Bits(3);
  color_description = Get_Bits(1);

  if (color_description) {
    color_primaries          = Get_Bits(8);
    transfer_characteristics = Get_Bits(8);
    matrix_coefficients      = Get_Bits(8);
    }

  display_horizontal_size = Get_Bits(14);
  marker_bit ("sequence_display_extension");
  display_vertical_size   = Get_Bits(14);
  }
/*}}}*/
/*{{{*/
/* decode quant matrix entension */
/* ISO/IEC 13818-2 section 6.2.3.2 */
static void quant_matrix_extension()
{
  int i;
  int pos;

  pos = ld->Bitcnt;

  if((ld->load_intra_quantizer_matrix = Get_Bits(1)))
  {
    for (i=0; i<64; i++)
    {
      ld->chroma_intra_quantizer_matrix[scan[ZIG_ZAG][i]]
      = ld->intra_quantizer_matrix[scan[ZIG_ZAG][i]]
      = Get_Bits(8);
    }
  }

  if((ld->load_non_intra_quantizer_matrix = Get_Bits(1)))
  {
    for (i=0; i<64; i++)
    {
      ld->chroma_non_intra_quantizer_matrix[scan[ZIG_ZAG][i]]
      = ld->non_intra_quantizer_matrix[scan[ZIG_ZAG][i]]
      = Get_Bits(8);
    }
  }

  if((ld->load_chroma_intra_quantizer_matrix = Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->chroma_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = Get_Bits(8);
  }

  if((ld->load_chroma_non_intra_quantizer_matrix = Get_Bits(1))) {
    for (i=0; i<64; i++)
      ld->chroma_non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = Get_Bits(8);
    }
  }
/*}}}*/
/*{{{*/
/* decode sequence scalable extension */
/* ISO/IEC 13818-2   section 6.2.2.5 */
static void sequence_scalable_extension()
{
  int pos;

  pos = ld->Bitcnt;

  /* values (without the +1 offset) of scalable_mode are defined in
     Table 6-10 of ISO/IEC 13818-2 */
  ld->scalable_mode = Get_Bits(2) + 1; /* add 1 to make SC_DP != SC_NONE */

  layer_id = Get_Bits(4);

  if (ld->scalable_mode==SC_SPAT) {
    lower_layer_prediction_horizontal_size = Get_Bits(14);
    marker_bit ("sequence_scalable_extension()");
    lower_layer_prediction_vertical_size   = Get_Bits(14);
    horizontal_subsampling_factor_m        = Get_Bits(5);
    horizontal_subsampling_factor_n        = Get_Bits(5);
    vertical_subsampling_factor_m          = Get_Bits(5);
    vertical_subsampling_factor_n          = Get_Bits(5);
    }

  if (ld->scalable_mode==SC_TEMP)
    Error ("temporal scalability not implemented\n");
  }
/*}}}*/
/*{{{*/
/* decode picture display extension */
/* ISO/IEC 13818-2 section 6.2.3.3. */
static void picture_display_extension()
{
  int i;
  int number_of_frame_center_offsets;
  int pos;

  pos = ld->Bitcnt;
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
    frame_center_horizontal_offset[i] = Get_Bits(16);
    marker_bit("picture_display_extension, first marker bit");
    frame_center_vertical_offset[i]   = Get_Bits(16);
    marker_bit("picture_display_extension, second marker bit");
    }
  }

/*}}}*/
/*{{{*/
/* decode picture coding extension */
static void picture_coding_extension()
{
  int pos;

  pos = ld->Bitcnt;

  f_code[0][0] = Get_Bits(4);
  f_code[0][1] = Get_Bits(4);
  f_code[1][0] = Get_Bits(4);
  f_code[1][1] = Get_Bits(4);

  intra_dc_precision         = Get_Bits(2);
  picture_structure          = Get_Bits(2);
  top_field_first            = Get_Bits(1);
  frame_pred_frame_dct       = Get_Bits(1);
  concealment_motion_vectors = Get_Bits(1);
  ld->q_scale_type           = Get_Bits(1);
  intra_vlc_format           = Get_Bits(1);
  ld->alternate_scan         = Get_Bits(1);
  repeat_first_field         = Get_Bits(1);
  chroma_420_type            = Get_Bits(1);
  progressive_frame          = Get_Bits(1);
  composite_display_flag     = Get_Bits(1);

  if (composite_display_flag)
  {
    v_axis            = Get_Bits(1);
    field_sequence    = Get_Bits(3);
    sub_carrier       = Get_Bits(1);
    burst_amplitude   = Get_Bits(7);
    sub_carrier_phase = Get_Bits(8);
  }
}
/*}}}*/
/*{{{*/
/* decode picture spatial scalable extension */
/* ISO/IEC 13818-2 section 6.2.3.5. */
static void picture_spatial_scalable_extension()
{
  int pos;

  pos = ld->Bitcnt;

  ld->pict_scal = 1; /* use spatial scalability in this picture */

  lower_layer_temporal_reference = Get_Bits(10);
  marker_bit("picture_spatial_scalable_extension(), first marker bit");
  lower_layer_horizontal_offset = Get_Bits(15);
  if (lower_layer_horizontal_offset>=16384)
    lower_layer_horizontal_offset-= 32768;
  marker_bit("picture_spatial_scalable_extension(), second marker bit");
  lower_layer_vertical_offset = Get_Bits(15);
  if (lower_layer_vertical_offset>=16384)
    lower_layer_vertical_offset-= 32768;
  spatial_temporal_weight_code_table_index = Get_Bits(2);
  lower_layer_progressive_frame = Get_Bits(1);
  lower_layer_deinterlaced_field_select = Get_Bits(1);
}
/*}}}*/
/*{{{*/
/* decode picture temporal scalable extension
 *
 * not implemented
 */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static void picture_temporal_scalable_extension()
{
  Error ("temporal scalability not supported\n");

}

/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2  sections 6.3.4.1 and 6.2.2.2.2 */
static void user_data()
{
  /* skip ahead to the next start code */
  next_start_code();
}
/*}}}*/
/*{{{*/
/* Copyright extension */
/* ISO/IEC 13818-2 section 6.2.3.6. */
/* (header added in November, 1994 to the IS document) */
static void copyright_extension()
{
  int pos;
  int reserved_data;

  pos = ld->Bitcnt;


  copyright_flag =       Get_Bits(1);
  copyright_identifier = Get_Bits(8);
  original_or_copy =     Get_Bits(1);

  /* reserved */
  reserved_data = Get_Bits(7);

  marker_bit("copyright_extension(), first marker bit");
  copyright_number_1 =   Get_Bits(20);
  marker_bit("copyright_extension(), second marker bit");
  copyright_number_2 =   Get_Bits(22);
  marker_bit("copyright_extension(), third marker bit");
  copyright_number_3 =   Get_Bits(22);
}
/*}}}*/
/*{{{*/
/* introduced in September 1995 to assist Spatial Scalability */
static void Update_Temporal_Reference_Tacking_Data()
{
  static int temporal_reference_wrap  = 0;
  static int temporal_reference_old   = 0;

  if (ld == &base)      /* *CH* */
  {
    if (picture_coding_type!=B_TYPE && temporal_reference!=temporal_reference_old)
    /* check first field of */
    {
       /* non-B-frame */
      if (temporal_reference_wrap)
      {/* wrap occured at previous I- or P-frame */
       /* now all intervening B-frames which could still have high temporal_reference values are done  */
        Temporal_Reference_Base += 1024;
      temporal_reference_wrap = 0;
      }

      /* distinguish from a reset */
      if (temporal_reference<temporal_reference_old && !Temporal_Reference_GOP_Reset)
      temporal_reference_wrap = 1;  /* we must have just passed a GOP-Header! */

      temporal_reference_old = temporal_reference;
      Temporal_Reference_GOP_Reset = 0;
    }

    True_Framenum = Temporal_Reference_Base + temporal_reference;

    /* temporary wrap of TR at 1024 for M frames */
    if (temporal_reference_wrap && temporal_reference <= temporal_reference_old)
      True_Framenum += 1024;

    True_Framenum_max = (True_Framenum > True_Framenum_max) ? True_Framenum : True_Framenum_max;
  }
}
/*}}}*/
/*{{{*/
/* decode sequence extension */
/* ISO/IEC 13818-2 section 6.2.2.3 */
static void sequence_extension()
{
  int horizontal_size_extension;
  int vertical_size_extension;
  int bit_rate_extension;
  int vbv_buffer_size_extension;

  /* derive bit position for trace */
  ld->MPEG2_Flag = 1;

  ld->scalable_mode = SC_NONE; /* unless overwritten by sequence_scalable_extension() */
  layer_id = 0;                /* unless overwritten by sequence_scalable_extension() */

  profile_and_level_indication = Get_Bits(8);
  progressive_sequence         = Get_Bits(1);
  chroma_format                = Get_Bits(2);
  horizontal_size_extension    = Get_Bits(2);
  vertical_size_extension      = Get_Bits(2);
  bit_rate_extension           = Get_Bits(12);
  marker_bit("sequence_extension");
  vbv_buffer_size_extension    = Get_Bits(8);
  low_delay                    = Get_Bits(1);
  frame_rate_extension_n       = Get_Bits(2);
  frame_rate_extension_d       = Get_Bits(5);

  frame_rate = frame_rate_Table[frame_rate_code] *
    ((frame_rate_extension_n+1)/(frame_rate_extension_d+1));

  /* special case for 422 profile & level must be made */
  if((profile_and_level_indication>>7) & 1)
  {  /* escape bit of profile_and_level_indication set */

    /* 4:2:2 Profile @ Main Level */
    if((profile_and_level_indication&15)==5)
    {
      profile = PROFILE_422;
      level   = MAIN_LEVEL;
    }
  }
  else
  {
    profile = profile_and_level_indication >> 4;  /* Profile is upper nibble */
    level   = profile_and_level_indication & 0xF;  /* Level is lower nibble */
  }


  horizontal_size = (horizontal_size_extension<<12) | (horizontal_size&0x0fff);
  vertical_size = (vertical_size_extension<<12) | (vertical_size&0x0fff);


  /* ISO/IEC 13818-2 does not define bit_rate_value to be composed of
   * both the original bit_rate_value parsed in sequence_header() and
   * the optional bit_rate_extension in sequence_extension_header().
   * However, we use it for bitstream verification purposes.
   */

  bit_rate_value += (bit_rate_extension << 18);
  bit_rate = ((double) bit_rate_value) * 400.0;
  vbv_buffer_size += (vbv_buffer_size_extension << 10);
}
/*}}}*/

/*{{{*/
/* decode extension and user data */
/* ISO/IEC 13818-2 section 6.2.2.2 */
static void extension_and_user_data()
{
  int code,ext_ID;
  next_start_code();
  while ((code = Show_Bits(32))==EXTENSION_START_CODE || code==USER_DATA_START_CODE) {
    if (code==EXTENSION_START_CODE) {
      Flush_Buffer32();
      ext_ID = Get_Bits(4);
      switch (ext_ID)
      {
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
        fprintf(stderr,"reserved extension start code ID %d\n",ext_ID);
        break;
      }
      next_start_code();
    }
    else
    {
      Flush_Buffer32();
      user_data();
    }
  }
}
/*}}}*/
/*{{{*/
/* decode group of pictures header */
/* ISO/IEC 13818-2 section 6.2.2.6 */
static void group_of_pictures_header()
{
  int pos;

  if (ld == &base) {
    Temporal_Reference_Base = True_Framenum_max + 1;  /* *CH* */
    Temporal_Reference_GOP_Reset = 1;
    }

  pos = ld->Bitcnt;
  drop_flag   = Get_Bits(1);
  hour        = Get_Bits(5);
  minute      = Get_Bits(6);
  marker_bit("group_of_pictures_header()");
  sec         = Get_Bits(6);
  frame       = Get_Bits(6);
  closed_gop  = Get_Bits(1);
  broken_link = Get_Bits(1);
  extension_and_user_data();
}
/*}}}*/
/*{{{*/
/* decode picture header */
/* ISO/IEC 13818-2 section 6.2.3 */
static void picture_header()
{
  int pos;
  int Extra_Information_Byte_Count;

  /* unless later overwritten by picture_spatial_scalable_extension() */
  ld->pict_scal = 0;

  pos = ld->Bitcnt;
  temporal_reference  = Get_Bits(10);
  picture_coding_type = Get_Bits(3);
  vbv_delay           = Get_Bits(16);

  if (picture_coding_type==P_TYPE || picture_coding_type==B_TYPE)
  {
    full_pel_forward_vector = Get_Bits(1);
    forward_f_code = Get_Bits(3);
  }
  if (picture_coding_type==B_TYPE)
  {
    full_pel_backward_vector = Get_Bits(1);
    backward_f_code = Get_Bits(3);
  }

  Extra_Information_Byte_Count = extra_bit_information();
  extension_and_user_data();

  /* update tracking information used to assist spatial scalability */
  Update_Temporal_Reference_Tacking_Data();
}
/*}}}*/
/*{{{*/
/* decode sequence header */
static void sequence_header()
{
  int i;
  int pos;

  pos = ld->Bitcnt;
  horizontal_size             = Get_Bits(12);
  vertical_size               = Get_Bits(12);
  aspect_ratio_information    = Get_Bits(4);
  frame_rate_code             = Get_Bits(4);
  bit_rate_value              = Get_Bits(18);
  marker_bit("sequence_header()");
  vbv_buffer_size             = Get_Bits(10);
  constrained_parameters_flag = Get_Bits(1);

  if((ld->load_intra_quantizer_matrix = Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->intra_quantizer_matrix[scan[ZIG_ZAG][i]] = Get_Bits(8);
  }
  else
  {
    for (i=0; i<64; i++)
      ld->intra_quantizer_matrix[i] = default_intra_quantizer_matrix[i];
  }

  if((ld->load_non_intra_quantizer_matrix = Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->non_intra_quantizer_matrix[scan[ZIG_ZAG][i]] = Get_Bits(8);
  }
  else
  {
    for (i=0; i<64; i++)
      ld->non_intra_quantizer_matrix[i] = 16;
  }

  /* copy luminance to chrominance matrices */
  for (i=0; i<64; i++)
  {
    ld->chroma_intra_quantizer_matrix[i] =
      ld->intra_quantizer_matrix[i];

    ld->chroma_non_intra_quantizer_matrix[i] =
      ld->non_intra_quantizer_matrix[i];
    }
  extension_and_user_data();
  }
/*}}}*/
/*{{{*/
/* decode headers from one input stream until an End of Sequence or picture start code is found */
int Get_Hdr() {

  unsigned int code;
  for (;;) {
    /* look for next_start_code */
    next_start_code();
    code = Get_Bits32();

    switch (code) {
      case SEQUENCE_HEADER_CODE:
        sequence_header();
        break;

      case GROUP_START_CODE:
        group_of_pictures_header();
        break;

      case PICTURE_START_CODE:
        picture_header();
        return 1;
        break;

      case SEQUENCE_END_CODE:
        return 0;
        break;

      default:
        fprintf(stderr,"Unexpected next_start_code %08x (ignored)\n",code);
        break;
      }

    }
  }
/*}}}*/

/*{{{*/
/* decode one intra coded MPEG-1 block */
void Decode_MPEG1_Intra_Block(comp,dc_dct_pred)
int comp;
int dc_dct_pred[];
{
  int val, i, j, sign;
  unsigned int code;
  DCTtab *tab;
  short *bp;

  bp = ld->block[comp];

  /* ISO/IEC 11172-2 section 2.4.3.7: Block layer. */
  /* decode DC coefficients */
  if (comp<4)
    bp[0] = (dc_dct_pred[0]+=Get_Luma_DC_dct_diff()) << 3;
  else if (comp==4)
    bp[0] = (dc_dct_pred[1]+=Get_Chroma_DC_dct_diff()) << 3;
  else
    bp[0] = (dc_dct_pred[2]+=Get_Chroma_DC_dct_diff()) << 3;

  if (Fault_Flag) return;

  /* D-pictures do not contain AC coefficients */
  if(picture_coding_type == D_TYPE)
    return;

  /* decode AC coefficients */
  for (i=1; ; i++)
  {
    code = Show_Bits(16);
    if (code>=16384)
      tab = &DCTtabnext[(code>>12)-4];
    else if (code>=1024)
      tab = &DCTtab0[(code>>8)-4];
    else if (code>=512)
      tab = &DCTtab1[(code>>6)-8];
    else if (code>=256)
      tab = &DCTtab2[(code>>4)-16];
    else if (code>=128)
      tab = &DCTtab3[(code>>3)-16];
    else if (code>=64)
      tab = &DCTtab4[(code>>2)-16];
    else if (code>=32)
      tab = &DCTtab5[(code>>1)-16];
    else if (code>=16)
      tab = &DCTtab6[code-16];
    else
    {
      printf("invalid Huffman code in Decode_MPEG1_Intra_Block()\n");
      Fault_Flag = 1;
      return;
    }

    Flush_Buffer(tab->len);

    if (tab->run==64) /* end_of_block */
      return;

    if (tab->run==65) /* escape */
    {
      i+= Get_Bits(6);

      val = Get_Bits(8);
      if (val==0)
        val = Get_Bits(8);
      else if (val==128)
        val = Get_Bits(8) - 256;
      else if (val>128)
        val -= 256;

      if((sign = (val<0)))
        val = -val;
    }
    else
    {
      i+= tab->run;
      val = tab->level;
      sign = Get_Bits(1);
    }

    if (i>=64)
    {
      fprintf(stderr,"DCT coeff index (i) out of bounds (intra)\n");
      Fault_Flag = 1;
      return;
    }

    j = scan[ZIG_ZAG][i];
    val = (val*ld->quantizer_scale*ld->intra_quantizer_matrix[j]) >> 3;

    /* mismatch control ('oddification') */
    if (val!=0) /* should always be true, but it's not guaranteed */
      val = (val-1) | 1; /* equivalent to: if ((val&1)==0) val = val - 1; */

    /* saturation */
    if (!sign)
      bp[j] = (val>2047) ?  2047 :  val; /* positive */
    else
      bp[j] = (val>2048) ? -2048 : -val; /* negative */
  }
}
/*}}}*/
/*{{{*/
/* decode one non-intra coded MPEG-1 block */
void Decode_MPEG1_Non_Intra_Block(comp)
int comp;
{
  int val, i, j, sign;
  unsigned int code;
  DCTtab *tab;
  short *bp;

  bp = ld->block[comp];

  /* decode AC coefficients */
  for (i=0; ; i++)
  {
    code = Show_Bits(16);
    if (code>=16384)
    {
      if (i==0)
        tab = &DCTtabfirst[(code>>12)-4];
      else
        tab = &DCTtabnext[(code>>12)-4];
    }
    else if (code>=1024)
      tab = &DCTtab0[(code>>8)-4];
    else if (code>=512)
      tab = &DCTtab1[(code>>6)-8];
    else if (code>=256)
      tab = &DCTtab2[(code>>4)-16];
    else if (code>=128)
      tab = &DCTtab3[(code>>3)-16];
    else if (code>=64)
      tab = &DCTtab4[(code>>2)-16];
    else if (code>=32)
      tab = &DCTtab5[(code>>1)-16];
    else if (code>=16)
      tab = &DCTtab6[code-16];
    else
    {
      printf("invalid Huffman code in Decode_MPEG1_Non_Intra_Block()\n");
      Fault_Flag = 1;
      return;
    }

    Flush_Buffer(tab->len);

    if (tab->run==64) /* end_of_block */
      return;

    if (tab->run==65) /* escape */
    {
      i+= Get_Bits(6);

      val = Get_Bits(8);
      if (val==0)
        val = Get_Bits(8);
      else if (val==128)
        val = Get_Bits(8) - 256;
      else if (val>128)
        val -= 256;

      if((sign = (val<0)))
        val = -val;
    }
    else
    {
      i+= tab->run;
      val = tab->level;
      sign = Get_Bits(1);
    }

    if (i>=64)
    {
      fprintf(stderr,"DCT coeff index (i) out of bounds (inter)\n");
      Fault_Flag = 1;
      return;
    }

    j = scan[ZIG_ZAG][i];
    val = (((val<<1)+1)*ld->quantizer_scale*ld->non_intra_quantizer_matrix[j]) >> 4;

    /* mismatch control ('oddification') */
    if (val!=0) /* should always be true, but it's not guaranteed */
      val = (val-1) | 1; /* equivalent to: if ((val&1)==0) val = val - 1; */

    /* saturation */
    if (!sign)
      bp[j] = (val>2047) ?  2047 :  val; /* positive */
    else
      bp[j] = (val>2048) ? -2048 : -val; /* negative */
  }
}
/*}}}*/
/*{{{*/
/* decode one intra coded MPEG-2 block */
void Decode_MPEG2_Intra_Block(comp,dc_dct_pred)
int comp;
int dc_dct_pred[];
{
  int val, i, j, sign, nc, cc, run;
  unsigned int code;
  DCTtab *tab;
  short *bp;
  int *qmat;
  struct layer_data *ld1;

  /* with data partitioning, data always goes to base layer */
  ld1 = (ld->scalable_mode==SC_DP) ? &base : ld;
  bp = ld1->block[comp];

  if (base.scalable_mode==SC_DP)
    if (base.priority_breakpoint<64)
      ld = &enhan;
    else
      ld = &base;

  cc = (comp<4) ? 0 : (comp&1)+1;

  qmat = (comp<4 || chroma_format==CHROMA420)
         ? ld1->intra_quantizer_matrix
         : ld1->chroma_intra_quantizer_matrix;

  /* ISO/IEC 13818-2 section 7.2.1: decode DC coefficients */
  if (cc==0)
    val = (dc_dct_pred[0]+= Get_Luma_DC_dct_diff());
  else if (cc==1)
    val = (dc_dct_pred[1]+= Get_Chroma_DC_dct_diff());
  else
    val = (dc_dct_pred[2]+= Get_Chroma_DC_dct_diff());

  if (Fault_Flag) return;

  bp[0] = val << (3-intra_dc_precision);

  nc=0;

#ifdef TRACE
  if (Trace_Flag)
    printf("DCT(%d)i:",comp);
#endif /* TRACE */

  /* decode AC coefficients */
  for (i=1; ; i++)
  {
    code = Show_Bits(16);
    if (code>=16384 && !intra_vlc_format)
      tab = &DCTtabnext[(code>>12)-4];
    else if (code>=1024)
    {
      if (intra_vlc_format)
        tab = &DCTtab0a[(code>>8)-4];
      else
        tab = &DCTtab0[(code>>8)-4];
    }
    else if (code>=512)
    {
      if (intra_vlc_format)
        tab = &DCTtab1a[(code>>6)-8];
      else
        tab = &DCTtab1[(code>>6)-8];
    }
    else if (code>=256)
      tab = &DCTtab2[(code>>4)-16];
    else if (code>=128)
      tab = &DCTtab3[(code>>3)-16];
    else if (code>=64)
      tab = &DCTtab4[(code>>2)-16];
    else if (code>=32)
      tab = &DCTtab5[(code>>1)-16];
    else if (code>=16)
      tab = &DCTtab6[code-16];
    else
    {
      printf("invalid Huffman code in Decode_MPEG2_Intra_Block()\n");
      Fault_Flag = 1;
      return;
    }

    Flush_Buffer(tab->len);

#ifdef TRACE
    if (Trace_Flag)
    {
      printf(" (");
      Print_Bits(code,16,tab->len);
    }
#endif /* TRACE */

    if (tab->run==64) /* end_of_block */
    {
#ifdef TRACE
      if (Trace_Flag)
        printf("): EOB\n");
#endif /* TRACE */
      return;
    }

    if (tab->run==65) /* escape */
    {
#ifdef TRACE
      if (Trace_Flag)
      {
        putchar(' ');
        Print_Bits(Show_Bits(6),6,6);
      }
#endif /* TRACE */

      i+= run = Get_Bits(6);

#ifdef TRACE
      if (Trace_Flag)
      {
        putchar(' ');
        Print_Bits(Show_Bits(12),12,12);
      }
#endif /* TRACE */

      val = Get_Bits(12);
      if ((val&2047)==0)
      {
        printf("invalid escape in Decode_MPEG2_Intra_Block()\n");
        Fault_Flag = 1;
        return;
      }
      if((sign = (val>=2048)))
        val = 4096 - val;
    }
    else
    {
      i+= run = tab->run;
      val = tab->level;
      sign = Get_Bits(1);

#ifdef TRACE
      if (Trace_Flag)
        printf("%d",sign);
#endif /* TRACE */
    }

    if (i>=64)
    {
      fprintf(stderr,"DCT coeff index (i) out of bounds (intra2)\n");
      Fault_Flag = 1;
      return;
    }

#ifdef TRACE
    if (Trace_Flag)
      printf("): %d/%d",run,sign ? -val : val);
#endif /* TRACE */

    j = scan[ld1->alternate_scan][i];
    val = (val * ld1->quantizer_scale * qmat[j]) >> 4;
    bp[j] = sign ? -val : val;
    nc++;

    if (base.scalable_mode==SC_DP && nc==base.priority_breakpoint-63)
      ld = &enhan;
  }
}
/*}}}*/
/*{{{*/
/* decode one non-intra coded MPEG-2 block */
void Decode_MPEG2_Non_Intra_Block(comp)
int comp;
{
  int val, i, j, sign, nc, run;
  unsigned int code;
  DCTtab *tab;
  short *bp;
  int *qmat;
  struct layer_data *ld1;

  /* with data partitioning, data always goes to base layer */
  ld1 = (ld->scalable_mode==SC_DP) ? &base : ld;
  bp = ld1->block[comp];

  if (base.scalable_mode==SC_DP)
    if (base.priority_breakpoint<64)
      ld = &enhan;
    else
      ld = &base;

  qmat = (comp<4 || chroma_format==CHROMA420)
         ? ld1->non_intra_quantizer_matrix
         : ld1->chroma_non_intra_quantizer_matrix;

  nc = 0;

#ifdef TRACE
  if (Trace_Flag)
    printf("DCT(%d)n:",comp);
#endif /* TRACE */

  /* decode AC coefficients */
  for (i=0; ; i++)
  {
    code = Show_Bits(16);
    if (code>=16384)
    {
      if (i==0)
        tab = &DCTtabfirst[(code>>12)-4];
      else
        tab = &DCTtabnext[(code>>12)-4];
    }
    else if (code>=1024)
      tab = &DCTtab0[(code>>8)-4];
    else if (code>=512)
      tab = &DCTtab1[(code>>6)-8];
    else if (code>=256)
      tab = &DCTtab2[(code>>4)-16];
    else if (code>=128)
      tab = &DCTtab3[(code>>3)-16];
    else if (code>=64)
      tab = &DCTtab4[(code>>2)-16];
    else if (code>=32)
      tab = &DCTtab5[(code>>1)-16];
    else if (code>=16)
      tab = &DCTtab6[code-16];
    else
    {
      printf("invalid Huffman code in Decode_MPEG2_Non_Intra_Block()\n");
      Fault_Flag = 1;
      return;
    }

    Flush_Buffer(tab->len);

#ifdef TRACE
    if (Trace_Flag)
    {
      printf(" (");
      Print_Bits(code,16,tab->len);
    }
#endif /* TRACE */

    if (tab->run==64) /* end_of_block */
    {
#ifdef TRACE
      if (Trace_Flag)
        printf("): EOB\n");
#endif /* TRACE */
      return;
    }

    if (tab->run==65) /* escape */
    {
#ifdef TRACE
      if (Trace_Flag)
      {
        putchar(' ');
        Print_Bits(Show_Bits(6),6,6);
      }
#endif /* TRACE */

      i+= run = Get_Bits(6);

#ifdef TRACE
      if (Trace_Flag)
      {
        putchar(' ');
        Print_Bits(Show_Bits(12),12,12);
      }
#endif /* TRACE */

      val = Get_Bits(12);
      if ((val&2047)==0)
      {
        printf("invalid escape in Decode_MPEG2_Intra_Block()\n");
        Fault_Flag = 1;
        return;
      }
      if((sign = (val>=2048)))
        val = 4096 - val;
    }
    else
    {
      i+= run = tab->run;
      val = tab->level;
      sign = Get_Bits(1);

#ifdef TRACE
      if (Trace_Flag)
        printf("%d",sign);
#endif /* TRACE */
    }

    if (i>=64)
    {
      fprintf(stderr,"DCT coeff index (i) out of bounds (inter2)\n");
      Fault_Flag = 1;
      return;
    }

#ifdef TRACE
    if (Trace_Flag)
      printf("): %d/%d",run,sign?-val:val);
#endif /* TRACE */

    j = scan[ld1->alternate_scan][i];
    val = (((val<<1)+1) * ld1->quantizer_scale * qmat[j]) >> 5;
    bp[j] = sign ? -val : val;
    nc++;

    if (base.scalable_mode==SC_DP && nc==base.priority_breakpoint-63)
      ld = &enhan;
  }
}
/*}}}*/

/*{{{*/
/* calculate motion vector component */
/* ISO/IEC 13818-2 section 7.6.3.1: Decoding the motion vectors */
/* Note: the arithmetic here is more elegant than that which is shown
   in 7.6.3.1.  The end results (PMV[][][]) should, however, be the same.  */
static void decode_motion_vector (pred,r_size,motion_code,motion_residual,full_pel_vector)
int *pred;
int r_size, motion_code, motion_residual;
int full_pel_vector; /* MPEG-1 (ISO/IEC 11172-1) support */
{
  int lim, vec;

  lim = 16<<r_size;
  vec = full_pel_vector ? (*pred >> 1) : (*pred);

  if (motion_code>0)
  {
    vec+= ((motion_code-1)<<r_size) + motion_residual + 1;
    if (vec>=lim)
      vec-= lim + lim;
  }
  else if (motion_code<0)
  {
    vec-= ((-motion_code-1)<<r_size) + motion_residual + 1;
    if (vec<-lim)
      vec+= lim + lim;
  }
  *pred = full_pel_vector ? (vec<<1) : vec;
}
/*}}}*/
/*{{{*/
/* get and decode motion vector and differential motion vector for one prediction */
void motion_vector (PMV, dmvector, h_r_size,v_r_size,dmv,mvscale,full_pel_vector)
int *PMV;
int *dmvector;
int h_r_size;
int v_r_size;
int dmv; /* MPEG-2 only: get differential motion vectors */
int mvscale; /* MPEG-2 only: field vector in frame pic */
int full_pel_vector; /* MPEG-1 only */
{
  int motion_code, motion_residual;

  /* horizontal component */
  /* ISO/IEC 13818-2 Table B-10 */
  motion_code = Get_motion_code();

  motion_residual = (h_r_size!=0 && motion_code!=0) ? Get_Bits(h_r_size) : 0;

#ifdef TRACE
  if (Trace_Flag)
  {
    if (h_r_size!=0 && motion_code!=0)
    {
      printf("motion_residual (");
      Print_Bits(motion_residual,h_r_size,h_r_size);
      printf("): %d\n",motion_residual);
    }
  }
#endif /* TRACE */


  decode_motion_vector(&PMV[0],h_r_size,motion_code,motion_residual,full_pel_vector);

  if (dmv)
    dmvector[0] = Get_dmvector();


  /* vertical component */
  motion_code     = Get_motion_code();
  motion_residual = (v_r_size!=0 && motion_code!=0) ? Get_Bits(v_r_size) : 0;

#ifdef TRACE
  if (Trace_Flag)
  {
    if (v_r_size!=0 && motion_code!=0)
    {
      printf("motion_residual (");
      Print_Bits(motion_residual,v_r_size,v_r_size);
      printf("): %d\n",motion_residual);
    }
  }
#endif /* TRACE */

  if (mvscale)
    PMV[1] >>= 1; /* DIV 2 */

  decode_motion_vector(&PMV[1],v_r_size,motion_code,motion_residual,full_pel_vector);

  if (mvscale)
    PMV[1] <<= 1;

  if (dmv)
    dmvector[1] = Get_dmvector();

#ifdef TRACE
  if (Trace_Flag)
    printf("PMV = %d,%d\n",PMV[0],PMV[1]);
#endif /* TRACE */
}
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 sections 6.2.5.2, 6.3.17.2, and 7.6.3: Motion vectors */
void motion_vectors (PMV, dmvector, motion_vertical_field_select,s,motion_vector_count,mv_format,h_r_size,v_r_size,dmv,mvscale)

int PMV[2][2][2];
int dmvector[2];
int motion_vertical_field_select[2][2];

int s, motion_vector_count, mv_format, h_r_size, v_r_size, dmv, mvscale;
{
  if (motion_vector_count==1)
  {
    if (mv_format==MV_FIELD && !dmv)
    {
      motion_vertical_field_select[1][s] = motion_vertical_field_select[0][s] = Get_Bits(1);
#ifdef TRACE
      if (Trace_Flag)
      {
        printf("motion_vertical_field_select[][%d] (%d): %d\n",s,
          motion_vertical_field_select[0][s],motion_vertical_field_select[0][s]);
      }
#endif /* TRACE */
    }

    motion_vector(PMV[0][s],dmvector,h_r_size,v_r_size,dmv,mvscale,0);

    /* update other motion vector predictors */
    PMV[1][s][0] = PMV[0][s][0];
    PMV[1][s][1] = PMV[0][s][1];
  }
  else
  {
    motion_vertical_field_select[0][s] = Get_Bits(1);
#ifdef TRACE
    if (Trace_Flag)
    {
      printf("motion_vertical_field_select[0][%d] (%d): %d\n",s,
        motion_vertical_field_select[0][s],motion_vertical_field_select[0][s]);
    }
#endif /* TRACE */
    motion_vector(PMV[0][s],dmvector,h_r_size,v_r_size,dmv,mvscale,0);

    motion_vertical_field_select[1][s] = Get_Bits(1);
#ifdef TRACE
    if (Trace_Flag)
    {
      printf("motion_vertical_field_select[1][%d] (%d): %d\n",s,
        motion_vertical_field_select[1][s],motion_vertical_field_select[1][s]);
    }
#endif /* TRACE */
    motion_vector(PMV[1][s],dmvector,h_r_size,v_r_size,dmv,mvscale,0);
  }
}
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 section 7.6.3.6: Dual prime additional arithmetic */
void Dual_Prime_Arithmetic (DMV, dmvector, mvx, mvy)
int DMV[][2];
int *dmvector; /* differential motion vector */
int mvx, mvy;  /* decoded mv components (always in field format) */
{
  if (picture_structure==FRAME_PICTURE)
  {
    if (top_field_first)
    {
      /* vector for prediction of top field from bottom field */
      DMV[0][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] - 1;

      /* vector for prediction of bottom field from top field */
      DMV[1][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
      DMV[1][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] + 1;
    }
    else
    {
      /* vector for prediction of top field from bottom field */
      DMV[0][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] - 1;

      /* vector for prediction of bottom field from top field */
      DMV[1][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
      DMV[1][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] + 1;
    }
  }
  else
  {
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

/*}}}*/

/*{{{*/
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
 *
 *
 *  NOTE: the work of combining predictions (ISO/IEC 13818-2 section 7.6.7)
 *  is distributed among several other stages.  This is accomplished by
 *  folding line offsets into the source and destination (src,dst)
 *  addresses (note the call arguments to form_prediction() in Predict()),
 *  line stride variables lx and lx2, the block dimension variables (w,h),
 *  average_flag, and by the very order in which Predict() is called.
 *  This implementation design (implicitly different than the spec)
 *  was chosen for its elegance.
*/

static void form_component_prediction (src, dst, lx, lx2, w, h, x, y, dx, dy, average_flag)
unsigned char *src;
unsigned char *dst;
int lx;          /* raster line increment */
int lx2;
int w,h;
int x,y;
int dx,dy;
int average_flag;      /* flag that signals bi-directional or Dual-Prime
                          averaging (7.6.7.1 and 7.6.7.4). if average_flag==1,
                          a previously formed prediction has been stored in
                          pel_pred[] */
{
  int xint;      /* horizontal integer sample vector: analogous to int_vec[0] */
  int yint;      /* vertical integer sample vectors: analogous to int_vec[1] */
  int xh;        /* horizontal half sample flag: analogous to half_flag[0]  */
  int yh;        /* vertical half sample flag: analogous to half_flag[1]  */
  int i, j, v;
  unsigned char *s;    /* source pointer: analogous to pel_ref[][]   */
  unsigned char *d;    /* destination pointer:  analogous to pel_pred[][]  */

  /* half pel scaling for integer vectors */
  xint = dx>>1;
  yint = dy>>1;

  /* derive half pel flags */
  xh = dx & 1;
  yh = dy & 1;

  /* compute the linear address of pel_ref[][] and pel_pred[][]
     based on cartesian/raster cordinates provided */
  s = src + lx*(y+yint) + x + xint;
  d = dst + lx*y + x;

  if (!xh && !yh) /* no horizontal nor vertical half-pel */
  {
    if (average_flag)
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          v = d[i]+s[i];
          d[i] = (v+(v>=0?1:0))>>1;
        }

        s+= lx2;
        d+= lx2;
      }
    }
    else
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          d[i] = s[i];
        }

        s+= lx2;
        d+= lx2;
      }
    }
  }
  else if (!xh && yh) /* no horizontal but vertical half-pel */
  {
    if (average_flag)
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          v = d[i] + ((unsigned int)(s[i]+s[i+lx]+1)>>1);
          d[i]=(v+(v>=0?1:0))>>1;
        }

        s+= lx2;
        d+= lx2;
      }
    }
    else
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          d[i] = (unsigned int)(s[i]+s[i+lx]+1)>>1;
        }

        s+= lx2;
        d+= lx2;
      }
    }
  }
  else if (xh && !yh) /* horizontal but no vertical half-pel */
  {
    if (average_flag)
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          v = d[i] + ((unsigned int)(s[i]+s[i+1]+1)>>1);
          d[i] = (v+(v>=0?1:0))>>1;
        }

        s+= lx2;
        d+= lx2;
      }
    }
    else
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          d[i] = (unsigned int)(s[i]+s[i+1]+1)>>1;
        }

        s+= lx2;
        d+= lx2;
      }
    }
  }
  else /* if (xh && yh) horizontal and vertical half-pel */
  {
    if (average_flag)
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          v = d[i] + ((unsigned int)(s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2);
          d[i] = (v+(v>=0?1:0))>>1;
        }

        s+= lx2;
        d+= lx2;
      }
    }
    else
    {
      for (j=0; j<h; j++)
      {
        for (i=0; i<w; i++)
        {
          d[i] = (unsigned int)(s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2;
        }

        s+= lx2;
        d+= lx2;
      }
    }
  }
}
/*}}}*/
/*{{{*/
static void form_prediction (src, sfield, dst, dfield, lx, lx2, w, h, x, y, dx, dy, average_flag)
unsigned char *src[]; /* prediction source buffer */
int sfield;           /* prediction source field number (0 or 1) */
unsigned char *dst[]; /* prediction destination buffer */
int dfield;           /* prediction destination field number (0 or 1)*/
int lx,lx2;           /* line strides */
int w,h;              /* prediction block/sub-block width, height */
int x,y;              /* pixel co-ordinates of top-left sample in current MB */
int dx,dy;            /* horizontal, vertical prediction address */
int average_flag;     /* add prediction error to prediction ? */
{
  /* Y */
  form_component_prediction(src[0]+(sfield?lx2>>1:0),dst[0]+(dfield?lx2>>1:0),
    lx,lx2,w,h,x,y,dx,dy,average_flag);

  if (chroma_format!=CHROMA444)
  {
    lx>>=1; lx2>>=1; w>>=1; x>>=1; dx/=2;
  }

  if (chroma_format==CHROMA420)
  {
    h>>=1; y>>=1; dy/=2;
  }

  /* Cb */
  form_component_prediction(src[1]+(sfield?lx2>>1:0),dst[1]+(dfield?lx2>>1:0),
    lx,lx2,w,h,x,y,dx,dy,average_flag);

  /* Cr */
  form_component_prediction(src[2]+(sfield?lx2>>1:0),dst[2]+(dfield?lx2>>1:0),
    lx,lx2,w,h,x,y,dx,dy,average_flag);
}
/*}}}*/
/*{{{*/
void form_predictions (bx, by, macroblock_type, motion_type, PMV, motion_vertical_field_select, dmvector,stwtype)
int bx, by;
int macroblock_type;
int motion_type;
int PMV[2][2][2], motion_vertical_field_select[2][2], dmvector[2];
int stwtype;
{
  int currentfield;
  unsigned char **predframe;
  int DMV[2][2];
  int stwtop, stwbot;

  stwtop = stwtype%3; /* 0:temporal, 1:(spat+temp)/2, 2:spatial */
  stwbot = stwtype/3;

  if ((macroblock_type & MACROBLOCK_MOTION_FORWARD)
   || (picture_coding_type==P_TYPE))
  {
    if (picture_structure==FRAME_PICTURE)
    {
      if ((motion_type==MC_FRAME)
        || !(macroblock_type & MACROBLOCK_MOTION_FORWARD))
      {
        /* frame-based prediction (broken into top and bottom halves
             for spatial scalability prediction purposes) */
        if (stwtop<2)
          form_prediction(forward_reference_frame,0,current_frame,0,
            Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by,
            PMV[0][0][0],PMV[0][0][1],stwtop);

        if (stwbot<2)
          form_prediction(forward_reference_frame,1,current_frame,1,
            Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by,
            PMV[0][0][0],PMV[0][0][1],stwbot);
      }
      else if (motion_type==MC_FIELD) /* field-based prediction */
      {
        /* top field prediction */
        if (stwtop<2)
          form_prediction(forward_reference_frame,motion_vertical_field_select[0][0],
            current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,
            bx,by>>1,PMV[0][0][0],PMV[0][0][1]>>1,stwtop);

        /* bottom field prediction */
        if (stwbot<2)
          form_prediction(forward_reference_frame,motion_vertical_field_select[1][0],
            current_frame,1,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,
            bx,by>>1,PMV[1][0][0],PMV[1][0][1]>>1,stwbot);
      }
      else if (motion_type==MC_DMV) /* dual prime prediction */
      {
        /* calculate derived motion vectors */
        Dual_Prime_Arithmetic(DMV,dmvector,PMV[0][0][0],PMV[0][0][1]>>1);

        if (stwtop<2)
        {
          /* predict top field from top field */
          form_prediction(forward_reference_frame,0,current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1,
            PMV[0][0][0],PMV[0][0][1]>>1,0);

          /* predict and add to top field from bottom field */
          form_prediction(forward_reference_frame,1,current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1,
            DMV[0][0],DMV[0][1],1);
        }

        if (stwbot<2)
        {
          /* predict bottom field from bottom field */
          form_prediction(forward_reference_frame,1,current_frame,1,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1,
            PMV[0][0][0],PMV[0][0][1]>>1,0);

          /* predict and add to bottom field from top field */
          form_prediction(forward_reference_frame,0,current_frame,1,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by>>1,
            DMV[1][0],DMV[1][1],1);
        }
      }
      else
        /* invalid motion_type */
        printf("invalid motion_type\n");
    }
    else /* TOP_FIELD or BOTTOM_FIELD */
    {
      /* field picture */
      currentfield = (picture_structure==BOTTOM_FIELD);

      /* determine which frame to use for prediction */
      if ((picture_coding_type==P_TYPE) && Second_Field
         && (currentfield!=motion_vertical_field_select[0][0]))
        predframe = backward_reference_frame; /* same frame */
      else
        predframe = forward_reference_frame; /* previous frame */

      if ((motion_type==MC_FIELD)
        || !(macroblock_type & MACROBLOCK_MOTION_FORWARD))
      {
        /* field-based prediction */
        if (stwtop<2)
          form_prediction(predframe,motion_vertical_field_select[0][0],current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,bx,by,
            PMV[0][0][0],PMV[0][0][1],stwtop);
      }
      else if (motion_type==MC_16X8)
      {
        if (stwtop<2)
        {
          form_prediction(predframe,motion_vertical_field_select[0][0],current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by,
            PMV[0][0][0],PMV[0][0][1],stwtop);

          /* determine which frame to use for lower half prediction */
          if ((picture_coding_type==P_TYPE) && Second_Field
             && (currentfield!=motion_vertical_field_select[1][0]))
            predframe = backward_reference_frame; /* same frame */
          else
            predframe = forward_reference_frame; /* previous frame */

          form_prediction(predframe,motion_vertical_field_select[1][0],current_frame,0,
            Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,bx,by+8,
            PMV[1][0][0],PMV[1][0][1],stwtop);
        }
      }
      else if (motion_type==MC_DMV) /* dual prime prediction */
      {
        if (Second_Field)
          predframe = backward_reference_frame; /* same frame */
        else
          predframe = forward_reference_frame; /* previous frame */

        /* calculate derived motion vectors */
        Dual_Prime_Arithmetic(DMV,dmvector,PMV[0][0][0],PMV[0][0][1]);

        /* predict from field of same parity */
        form_prediction(forward_reference_frame,currentfield,current_frame,0,
          Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,bx,by,
          PMV[0][0][0],PMV[0][0][1],0);

        /* predict from field of opposite parity */
        form_prediction(predframe,!currentfield,current_frame,0,
          Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,bx,by,
          DMV[0][0],DMV[0][1],1);
      }
      else
        /* invalid motion_type */
        printf("invalid motion_type\n");
    }
    stwtop = stwbot = 1;
  }

  if (macroblock_type & MACROBLOCK_MOTION_BACKWARD)
  {
    if (picture_structure==FRAME_PICTURE)
    {
      if (motion_type==MC_FRAME)
      {
        /* frame-based prediction */
        if (stwtop<2)
          form_prediction(backward_reference_frame,0,current_frame,0,
            Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by,
            PMV[0][1][0],PMV[0][1][1],stwtop);

        if (stwbot<2)
          form_prediction(backward_reference_frame,1,current_frame,1,
            Coded_Picture_Width,Coded_Picture_Width<<1,16,8,bx,by,
            PMV[0][1][0],PMV[0][1][1],stwbot);
      }
      else /* field-based prediction */
      {
        /* top field prediction */
        if (stwtop<2)
          form_prediction(backward_reference_frame,motion_vertical_field_select[0][1],
            current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,
            bx,by>>1,PMV[0][1][0],PMV[0][1][1]>>1,stwtop);

        /* bottom field prediction */
        if (stwbot<2)
          form_prediction(backward_reference_frame,motion_vertical_field_select[1][1],
            current_frame,1,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,
            bx,by>>1,PMV[1][1][0],PMV[1][1][1]>>1,stwbot);
      }
    }
    else /* TOP_FIELD or BOTTOM_FIELD */
    {
      /* field picture */
      if (motion_type==MC_FIELD)
      {
        /* field-based prediction */
        form_prediction(backward_reference_frame,motion_vertical_field_select[0][1],
          current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,16,
          bx,by,PMV[0][1][0],PMV[0][1][1],stwtop);
      }
      else if (motion_type==MC_16X8)
      {
        form_prediction(backward_reference_frame,motion_vertical_field_select[0][1],
          current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,
          bx,by,PMV[0][1][0],PMV[0][1][1],stwtop);

        form_prediction(backward_reference_frame,motion_vertical_field_select[1][1],
          current_frame,0,Coded_Picture_Width<<1,Coded_Picture_Width<<1,16,8,
          bx,by+8,PMV[1][1][0],PMV[1][1][1],stwtop);
      }
      else
        /* invalid motion_type */
        printf("invalid motion_type\n");
    }
  }
}
/*}}}*/

/*{{{*/
/* ISO/IEC 13818-2 section 6.2.4 */
int slice_header() {

  int slice_vertical_position_extension;
  int quantizer_scale_code;
  int pos;
  int slice_picture_id_enable = 0;
  int slice_picture_id = 0;
  int extra_information_slice = 0;

  pos = ld->Bitcnt;
  slice_vertical_position_extension = (ld->MPEG2_Flag && vertical_size>2800) ? Get_Bits(3) : 0;
  if (ld->scalable_mode == SC_DP)
    ld->priority_breakpoint = Get_Bits(7);

  quantizer_scale_code = Get_Bits(5);
  ld->quantizer_scale = ld->MPEG2_Flag ? (ld->q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : quantizer_scale_code<<1) : quantizer_scale_code;

  /* slice_id introduced in March 1995 as part of the video corridendum (after the IS was drafted in November 1994) */
  if (Get_Bits(1)) {
    ld->intra_slice = Get_Bits(1);
    slice_picture_id_enable = Get_Bits(1);
    slice_picture_id = Get_Bits(6);
    extra_information_slice = extra_bit_information();
    }
  else
    ld->intra_slice = 0;

  return slice_vertical_position_extension;
}
/*}}}*/

/*{{{*/
/* move/add 8x8-Block from block[comp] to backward_reference_frame */
/* copy reconstructed 8x8 block from block[comp] to current_frame[]
 * ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data
 * This stage also embodies some of the operations implied by:
 *   - ISO/IEC 13818-2 section 7.6.7: Combining predictions
 *   - ISO/IEC 13818-2 section 6.1.3: Macroblock
*/
static void Add_Block (comp,bx,by,dct_type,addflag)
int comp,bx,by,dct_type,addflag;
{
  int cc,i, j, iincr;
  unsigned char *rfp;
  short *bp;


  /* derive color component index */
  /* equivalent to ISO/IEC 13818-2 Table 7-1 */
  cc = (comp<4) ? 0 : (comp&1)+1; /* color component index */

  if (cc==0)
  {
    /* luminance */

    if (picture_structure==FRAME_PICTURE)
      if (dct_type)
      {
        /* field DCT coding */
        rfp = current_frame[0]
              + Coded_Picture_Width*(by+((comp&2)>>1)) + bx + ((comp&1)<<3);
        iincr = (Coded_Picture_Width<<1) - 8;
      }
      else
      {
        /* frame DCT coding */
        rfp = current_frame[0]
              + Coded_Picture_Width*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
        iincr = Coded_Picture_Width - 8;
      }
    else
    {
      /* field picture */
      rfp = current_frame[0]
            + (Coded_Picture_Width<<1)*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
      iincr = (Coded_Picture_Width<<1) - 8;
    }
  }
  else
  {
    /* chrominance */

    /* scale coordinates */
    if (chroma_format!=CHROMA444)
      bx >>= 1;
    if (chroma_format==CHROMA420)
      by >>= 1;
    if (picture_structure==FRAME_PICTURE)
    {
      if (dct_type && (chroma_format!=CHROMA420))
      {
        /* field DCT coding */
        rfp = current_frame[cc]
              + Chroma_Width*(by+((comp&2)>>1)) + bx + (comp&8);
        iincr = (Chroma_Width<<1) - 8;
      }
      else
      {
        /* frame DCT coding */
        rfp = current_frame[cc]
              + Chroma_Width*(by+((comp&2)<<2)) + bx + (comp&8);
        iincr = Chroma_Width - 8;
      }
    }
    else
    {
      /* field picture */
      rfp = current_frame[cc]
            + (Chroma_Width<<1)*(by+((comp&2)<<2)) + bx + (comp&8);
      iincr = (Chroma_Width<<1) - 8;
    }
  }

  bp = ld->block[comp];

  if (addflag)
  {
    for (i=0; i<8; i++)
    {
      for (j=0; j<8; j++)
      {
        *rfp = Clip[*bp++ + *rfp];
        rfp++;
      }

      rfp+= iincr;
    }
  }
  else
  {
    for (i=0; i<8; i++)
    {
      for (j=0; j<8; j++)
        *rfp++ = Clip[*bp++ + 128];

      rfp+= iincr;
    }
  }
}
/*}}}*/
/*{{{*/
/* IMPLEMENTATION: set scratch pad macroblock to zero */
static void Clear_Block (comp)
int comp;
{
  short *Block_Ptr;
  int i;

  Block_Ptr = ld->block[comp];

  for (i=0; i<64; i++)
    *Block_Ptr++ = 0;
}
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
static void macroblock_modes (pmacroblock_type,pstwtype,pstwclass,
  pmotion_type,pmotion_vector_count,pmv_format,pdmv,pmvscale,pdct_type)
  int *pmacroblock_type, *pstwtype, *pstwclass;
  int *pmotion_type, *pmotion_vector_count, *pmv_format, *pdmv, *pmvscale;
  int *pdct_type;
{
  int macroblock_type;
  int stwtype, stwcode, stwclass;
  int motion_type = 0;
  int motion_vector_count, mv_format, dmv, mvscale;
  int dct_type;
  static unsigned char stwc_table[3][4]
    = { {6,3,7,4}, {2,1,5,4}, {2,5,7,4} };
  static unsigned char stwclass_table[9]
    = {0, 1, 2, 1, 1, 2, 3, 3, 4};

  /* get macroblock_type */
  macroblock_type = Get_macroblock_type();

  if (Fault_Flag) return;

  /* get spatial_temporal_weight_code */
  if (macroblock_type & MB_WEIGHT)
  {
    if (spatial_temporal_weight_code_table_index==0)
      stwtype = 4;
    else
    {
      stwcode = Get_Bits(2);
#ifdef TRACE
      if (Trace_Flag)
      {
        printf("spatial_temporal_weight_code (");
        Print_Bits(stwcode,2,2);
        printf("): %d\n",stwcode);
      }
#endif /* TRACE */
      stwtype = stwc_table[spatial_temporal_weight_code_table_index-1][stwcode];
    }
  }
  else
    stwtype = (macroblock_type & MB_CLASS4) ? 8 : 0;

  /* SCALABILITY: derive spatial_temporal_weight_class (Table 7-18) */
  stwclass = stwclass_table[stwtype];

  /* get frame/field motion type */
  if (macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD))
  {
    if (picture_structure==FRAME_PICTURE) /* frame_motion_type */
    {
      motion_type = frame_pred_frame_dct ? MC_FRAME : Get_Bits(2);
#ifdef TRACE
      if (!frame_pred_frame_dct && Trace_Flag)
      {
        printf("frame_motion_type (");
        Print_Bits(motion_type,2,2);
        printf("): %s\n",motion_type==MC_FIELD?"Field":
                         motion_type==MC_FRAME?"Frame":
                         motion_type==MC_DMV?"Dual_Prime":"Invalid");
      }
#endif /* TRACE */
    }
    else /* field_motion_type */
    {
      motion_type = Get_Bits(2);
#ifdef TRACE
      if (Trace_Flag)
      {
        printf("field_motion_type (");
        Print_Bits(motion_type,2,2);
        printf("): %s\n",motion_type==MC_FIELD?"Field":
                         motion_type==MC_16X8?"16x8 MC":
                         motion_type==MC_DMV?"Dual_Prime":"Invalid");
      }
#endif /* TRACE */
    }
  }
  else if ((macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
  {
    /* concealment motion vectors */
    motion_type = (picture_structure==FRAME_PICTURE) ? MC_FRAME : MC_FIELD;
  }
#if 0
  else
  {
    printf("maroblock_modes(): unknown macroblock type\n");
    motion_type = -1;
  }
#endif

  /* derive motion_vector_count, mv_format and dmv, (table 6-17, 6-18) */
  if (picture_structure==FRAME_PICTURE)
  {
    motion_vector_count = (motion_type==MC_FIELD && stwclass<2) ? 2 : 1;
    mv_format = (motion_type==MC_FRAME) ? MV_FRAME : MV_FIELD;
  }
  else
  {
    motion_vector_count = (motion_type==MC_16X8) ? 2 : 1;
    mv_format = MV_FIELD;
  }

  dmv = (motion_type==MC_DMV); /* dual prime */

  /* field mv predictions in frame pictures have to be scaled
   * ISO/IEC 13818-2 section 7.6.3.1 Decoding the motion vectors
   * IMPLEMENTATION: mvscale is derived for later use in motion_vectors()
   * it displaces the stage:
   *
   *    if((mv_format=="field")&&(t==1)&&(picture_structure=="Frame picture"))
   *      prediction = PMV[r][s][t] DIV 2;
   */

  mvscale = ((mv_format==MV_FIELD) && (picture_structure==FRAME_PICTURE));

  /* get dct_type (frame DCT / field DCT) */
  dct_type = (picture_structure==FRAME_PICTURE)
             && (!frame_pred_frame_dct)
             && (macroblock_type & (MACROBLOCK_PATTERN|MACROBLOCK_INTRA))
             ? Get_Bits(1)
             : 0;

#ifdef TRACE
  if (Trace_Flag  && (picture_structure==FRAME_PICTURE)
             && (!frame_pred_frame_dct)
             && (macroblock_type & (MACROBLOCK_PATTERN|MACROBLOCK_INTRA)))
    printf("dct_type (%d): %s\n",dct_type,dct_type?"Field":"Frame");
#endif /* TRACE */

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
/*}}}*/
/*{{{*/
/* return==-1 means go to next picture */
/* the expression "start of slice" is used throughout the normative
   body of the MPEG specification */
static int start_of_slice (MBAmax, MBA, MBAinc,
  dc_dct_pred, PMV)
int MBAmax;
int *MBA;
int *MBAinc;
int dc_dct_pred[3];
int PMV[2][2][2];
{
  unsigned int code;
  int slice_vert_pos_ext;

  ld = &base;

  Fault_Flag = 0;

  next_start_code();
  code = Show_Bits(32);

  if (code<SLICE_START_CODE_MIN || code>SLICE_START_CODE_MAX)
  {
    /* only slice headers are allowed in picture_data */
    printf("start_of_slice(): Premature end of picture\n");
    return(-1);  /* trigger: go to next picture */
  }

  Flush_Buffer32();

  /* decode slice header (may change quantizer_scale) */
  slice_vert_pos_ext = slice_header();


  /* SCALABILITY: Data Partitioning */
  if (base.scalable_mode==SC_DP)
  {
    ld = &enhan;
    next_start_code();
    code = Show_Bits(32);

    if (code<SLICE_START_CODE_MIN || code>SLICE_START_CODE_MAX)
    {
      /* only slice headers are allowed in picture_data */
      printf("DP: Premature end of picture\n");
      return(-1);    /* trigger: go to next picture */
    }

    Flush_Buffer32();

    /* decode slice header (may change quantizer_scale) */
    slice_vert_pos_ext = slice_header();

    if (base.priority_breakpoint!=1)
      ld = &base;
  }

  /* decode macroblock address increment */
  *MBAinc = Get_macroblock_address_increment();

  if (Fault_Flag)
  {
    printf("start_of_slice(): MBAinc unsuccessful\n");
    return(0);   /* trigger: go to next slice */
  }

  /* set current location */
  /* NOTE: the arithmetic used to derive macroblock_address below is
   *       equivalent to ISO/IEC 13818-2 section 6.3.17: Macroblock
   */
  *MBA = ((slice_vert_pos_ext<<7) + (code&255) - 1)*mb_width + *MBAinc - 1;
  *MBAinc = 1; /* first macroblock in slice: not skipped */

  /* reset all DC coefficient and motion vector predictors */
  /* reset all DC coefficient and motion vector predictors */
  /* ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
  dc_dct_pred[0]=dc_dct_pred[1]=dc_dct_pred[2]=0;

  /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
  PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
  PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;

  /* successfull: trigger decode macroblocks in slice */
  return(1);
}
/*}}}*/
/*{{{*/
static void frame_reorder (int Bitstream_Framenum, int Sequence_Framenum) {

  /* tracking variables to insure proper output in spatial scalability */
  static int Oldref_progressive_frame, Newref_progressive_frame;

  if (Sequence_Framenum!=0) {
    if (picture_structure==FRAME_PICTURE || Second_Field) {
      if (picture_coding_type == B_TYPE)
        Write_Frame (auxframe, Bitstream_Framenum-1);
      else {
        Newref_progressive_frame = progressive_frame;
        progressive_frame = Oldref_progressive_frame;
        Write_Frame (forward_reference_frame, Bitstream_Framenum-1);
        Oldref_progressive_frame = progressive_frame = Newref_progressive_frame;
        }
      }
    }
  else
    Oldref_progressive_frame = progressive_frame;
  }
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 section 7.6 */
static void motion_compensation (MBA, macroblock_type, motion_type, PMV,
  motion_vertical_field_select, dmvector, stwtype, dct_type)
int MBA;
int macroblock_type;
int motion_type;
int PMV[2][2][2];
int motion_vertical_field_select[2][2];
int dmvector[2];
int stwtype;
int dct_type;
{
  int bx, by;
  int comp;

  /* derive current macroblock position within picture */
  /* ISO/IEC 13818-2 section 6.3.1.6 and 6.3.1.7 */
  bx = 16*(MBA%mb_width);
  by = 16*(MBA/mb_width);

  /* motion compensation */
  if (!(macroblock_type & MACROBLOCK_INTRA))
    form_predictions(bx,by,macroblock_type,motion_type,PMV,
      motion_vertical_field_select,dmvector,stwtype);

  /* SCALABILITY: Data Partitioning */
  if (base.scalable_mode==SC_DP)
    ld = &base;

  /* copy or add block data into picture */
  for (comp=0; comp<block_count; comp++)
  {
    /* ISO/IEC 13818-2 section Annex A: inverse DCT */
    Fast_IDCT(ld->block[comp]);

    /* ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data */
    Add_Block(comp,bx,by,dct_type,(macroblock_type & MACROBLOCK_INTRA)==0);
  }

}
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 section 7.6.6 */
static void skipped_macroblock (dc_dct_pred, PMV, motion_type,
  motion_vertical_field_select, stwtype, macroblock_type)
int dc_dct_pred[3];
int PMV[2][2][2];
int *motion_type;
int motion_vertical_field_select[2][2];
int *stwtype;
int *macroblock_type;
{
  int comp;

  /* SCALABILITY: Data Paritioning */
  if (base.scalable_mode==SC_DP)
    ld = &base;

  for (comp=0; comp<block_count; comp++)
    Clear_Block(comp);

  /* reset intra_dc predictors */
  /* ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
  dc_dct_pred[0]=dc_dct_pred[1]=dc_dct_pred[2]=0;

  /* reset motion vector predictors */
  /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
  if (picture_coding_type==P_TYPE)
    PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;

  /* derive motion_type */
  if (picture_structure==FRAME_PICTURE)
    *motion_type = MC_FRAME;
  else
  {
    *motion_type = MC_FIELD;

    /* predict from field of same parity */
    /* ISO/IEC 13818-2 section 7.6.6.1 and 7.6.6.3: P field picture and B field
       picture */
    motion_vertical_field_select[0][0]=motion_vertical_field_select[0][1] =
      (picture_structure==BOTTOM_FIELD);
  }

  /* skipped I are spatial-only predicted, */
  /* skipped P and B are temporal-only predicted */
  /* ISO/IEC 13818-2 section 7.7.6: Skipped macroblocks */
  *stwtype = (picture_coding_type==I_TYPE) ? 8 : 0;

 /* IMPLEMENTATION: clear MACROBLOCK_INTRA */
  *macroblock_type&= ~MACROBLOCK_INTRA;

}
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 sections 7.2 through 7.5 */
static int decode_macroblock (macroblock_type, stwtype, stwclass,
  motion_type, dct_type, PMV, dc_dct_pred,
  motion_vertical_field_select, dmvector)
int *macroblock_type;
int *stwtype;
int *stwclass;
int *motion_type;
int *dct_type;
int PMV[2][2][2];
int dc_dct_pred[3];
int motion_vertical_field_select[2][2];
int dmvector[2];
{
  /* locals */
  int quantizer_scale_code;
  int comp;

  int motion_vector_count;
  int mv_format;
  int dmv;
  int mvscale;
  int coded_block_pattern;

  /* SCALABILITY: Data Patitioning */
  if (base.scalable_mode==SC_DP) {
    if (base.priority_breakpoint<=2)
      ld = &enhan;
    else
      ld = &base;
  }

  /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
  macroblock_modes(macroblock_type, stwtype, stwclass,
    motion_type, &motion_vector_count, &mv_format, &dmv, &mvscale,
    dct_type);

  if (Fault_Flag) return(0);  /* trigger: go to next slice */

  if (*macroblock_type & MACROBLOCK_QUANT) {
    quantizer_scale_code = Get_Bits(5);

#ifdef TRACE
    if (Trace_Flag) {
      printf("quantiser_scale_code (");
      Print_Bits(quantizer_scale_code,5,5);
      printf("): %d\n",quantizer_scale_code);
    }
#endif /* TRACE */

    /* ISO/IEC 13818-2 section 7.4.2.2: Quantizer scale factor */
    if (ld->MPEG2_Flag)
      ld->quantizer_scale = ld->q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : (quantizer_scale_code << 1);
    else
      ld->quantizer_scale = quantizer_scale_code;

    /* SCALABILITY: Data Partitioning */
    if (base.scalable_mode==SC_DP)
      /* make sure base.quantizer_scale is valid */
      base.quantizer_scale = ld->quantizer_scale;
  }

  /* motion vectors */
  /* ISO/IEC 13818-2 section 6.3.17.2: Motion vectors */
  /* decode forward motion vectors */
  if ((*macroblock_type & MACROBLOCK_MOTION_FORWARD) || ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)) {
    if (ld->MPEG2_Flag)
      motion_vectors(PMV,dmvector,motion_vertical_field_select,
        0,motion_vector_count,mv_format,f_code[0][0]-1,f_code[0][1]-1,
        dmv,mvscale);
    else
      motion_vector(PMV[0][0],dmvector,
      forward_f_code-1,forward_f_code-1,0,0,full_pel_forward_vector);
  }

  if (Fault_Flag) return(0);  /* trigger: go to next slice */

  /* decode backward motion vectors */
  if (*macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
    if (ld->MPEG2_Flag)
      motion_vectors(PMV,dmvector,motion_vertical_field_select,
        1,motion_vector_count,mv_format,f_code[1][0]-1,f_code[1][1]-1,0,
        mvscale);
    else
      motion_vector(PMV[0][1],dmvector,
        backward_f_code-1,backward_f_code-1,0,0,full_pel_backward_vector);
  }

  if (Fault_Flag) return(0);  /* trigger: go to next slice */

  if ((*macroblock_type & MACROBLOCK_INTRA) && concealment_motion_vectors)
    Flush_Buffer(1); /* remove marker_bit */

  if (base.scalable_mode==SC_DP && base.priority_breakpoint==3)
    ld = &enhan;

  /* macroblock_pattern */
  /* ISO/IEC 13818-2 section 6.3.17.4: Coded block pattern */
  if (*macroblock_type & MACROBLOCK_PATTERN) {
    coded_block_pattern = Get_coded_block_pattern();

    if (chroma_format==CHROMA422) {
      /* coded_block_pattern_1 */
      coded_block_pattern = (coded_block_pattern<<2) | Get_Bits(2);

#ifdef TRACE
       if (Trace_Flag) {
         printf("coded_block_pattern_1: ");
         Print_Bits(coded_block_pattern,2,2);
         printf(" (%d)\n",coded_block_pattern&3);
       }
#endif /* TRACE */
     }
     else if (chroma_format==CHROMA444) {
      /* coded_block_pattern_2 */
      coded_block_pattern = (coded_block_pattern<<6) | Get_Bits(6);

#ifdef TRACE
      if (Trace_Flag) {
        printf("coded_block_pattern_2: ");
        Print_Bits(coded_block_pattern,6,6);
        printf(" (%d)\n",coded_block_pattern&63);
      }
#endif /* TRACE */
    }
  }
  else
    coded_block_pattern = (*macroblock_type & MACROBLOCK_INTRA) ?
      (1<<block_count)-1 : 0;

  if (Fault_Flag) return(0);  /* trigger: go to next slice */

  /* decode blocks */
  for (comp=0; comp<block_count; comp++) {
    /* SCALABILITY: Data Partitioning */
    if (base.scalable_mode==SC_DP)
    ld = &base;

    Clear_Block(comp);

    if (coded_block_pattern & (1<<(block_count-1-comp)))
    {
      if (*macroblock_type & MACROBLOCK_INTRA) {
        if (ld->MPEG2_Flag)
          Decode_MPEG2_Intra_Block(comp,dc_dct_pred);
        else
          Decode_MPEG1_Intra_Block(comp,dc_dct_pred);
      }
      else {
        if (ld->MPEG2_Flag)
          Decode_MPEG2_Non_Intra_Block(comp);
        else
          Decode_MPEG1_Non_Intra_Block(comp);
      }

      if (Fault_Flag) return(0);  /* trigger: go to next slice */
    }
  }

  if(picture_coding_type==D_TYPE) {
    /* remove end_of_macroblock (always 1, prevents startcode emulation) */
    /* ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
    marker_bit("D picture end_of_macroblock bit");
  }

  /* reset intra_dc predictors */
  /* ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
  if (!(*macroblock_type & MACROBLOCK_INTRA))
    dc_dct_pred[0]=dc_dct_pred[1]=dc_dct_pred[2]=0;

  /* reset motion vector predictors */
  if ((*macroblock_type & MACROBLOCK_INTRA) && !concealment_motion_vectors) {
    /* intra mb without concealment motion vectors */
    /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
    PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
    PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
  }

  /* special "No_MC" macroblock_type case */
  /* ISO/IEC 13818-2 section 7.6.3.5: Prediction in P pictures */
  if ((picture_coding_type==P_TYPE)
    && !(*macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_INTRA))) {
    /* non-intra mb without forward mv in a P picture */
    /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
    PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;

    /* derive motion_type */
    /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes, frame_motion_type */
    if (picture_structure==FRAME_PICTURE)
      *motion_type = MC_FRAME;
    else {
      *motion_type = MC_FIELD;
      /* predict from field of same parity */
      motion_vertical_field_select[0][0] = (picture_structure==BOTTOM_FIELD);
    }
  }

  if (*stwclass==4) {
    /* purely spatially predicted macroblock */
    /* ISO/IEC 13818-2 section 7.7.5.1: Resetting motion vector predictions */
    PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
    PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
  }

  /* successfully decoded macroblock */
  return(1);

} /* decode_macroblock */
/*}}}*/
/*{{{*/
/* decode all macroblocks of the current picture */
/* ISO/IEC 13818-2 section 6.3.16 */
static int slice (framenum, MBAmax)
int framenum, MBAmax;
{
  int MBA;
  int MBAinc, macroblock_type, motion_type, dct_type;
  int dc_dct_pred[3];
  int PMV[2][2][2], motion_vertical_field_select[2][2];
  int dmvector[2];
  int stwtype, stwclass;
  int ret;

  MBA = 0; /* macroblock address */
  MBAinc = 0;

  if((ret=start_of_slice(MBAmax, &MBA, &MBAinc, dc_dct_pred, PMV))!=1)
    return(ret);

  Fault_Flag=0;

  for (;;)
  {

    /* this is how we properly exit out of picture */
    if (MBA>=MBAmax)
      return(-1); /* all macroblocks decoded */

#ifdef TRACE
    if (Trace_Flag)
      printf("frame %d, MB %d\n",framenum,MBA);
#endif /* TRACE */

    ld = &base;

    if (MBAinc==0)
    {
      if (base.scalable_mode==SC_DP && base.priority_breakpoint==1)
          ld = &enhan;

      if (!Show_Bits(23) || Fault_Flag) /* next_start_code or fault */
      {
resync: /* if Fault_Flag: resynchronize to next next_start_code */
        Fault_Flag = 0;
        return(0);     /* trigger: go to next slice */
      }
      else /* neither next_start_code nor Fault_Flag */
      {
        if (base.scalable_mode==SC_DP && base.priority_breakpoint==1)
          ld = &enhan;

        /* decode macroblock address increment */
        MBAinc = Get_macroblock_address_increment();

        if (Fault_Flag) goto resync;
      }
    }

    if (MBA>=MBAmax)
    {
      /* MBAinc points beyond picture dimensions */
      printf("Too many macroblocks in picture\n");
      return(-1);
    }

    if (MBAinc==1) /* not skipped */
    {
      ret = decode_macroblock(&macroblock_type, &stwtype, &stwclass,
              &motion_type, &dct_type, PMV, dc_dct_pred,
              motion_vertical_field_select, dmvector);

      if(ret==-1)
        return(-1);

      if(ret==0)
        goto resync;

    }
    else /* MBAinc!=1: skipped macroblock */
    {
      /* ISO/IEC 13818-2 section 7.6.6 */
      skipped_macroblock(dc_dct_pred, PMV, &motion_type,
        motion_vertical_field_select, &stwtype, &macroblock_type);
    }

    /* ISO/IEC 13818-2 section 7.6 */
    motion_compensation(MBA, macroblock_type, motion_type, PMV,
      motion_vertical_field_select, dmvector, stwtype, dct_type);


    /* advance to next macroblock */
    MBA++;
    MBAinc--;

    if (MBA>=MBAmax)
      return(-1); /* all macroblocks decoded */
  }
}
/*}}}*/
/*{{{*/
/* decode all macroblocks of the current picture */
/* stages described in ISO/IEC 13818-2 section 7 */
static void picture_data (framenum)
int framenum;
{
  int MBAmax;
  int ret;

  /* number of macroblocks per picture */
  MBAmax = mb_width*mb_height;

  if (picture_structure!=FRAME_PICTURE)
    MBAmax>>=1; /* field picture has half as mnay macroblocks as frame */

  for(;;)
  {
    if((ret=slice(framenum, MBAmax))<0)
      return;
  }

}
/*}}}*/
/*{{{*/
/* ISO/IEC 13818-2 section 7.8 */
static void Decode_SNR_Macroblock (SNRMBA, SNRMBAinc, MBA, MBAmax, dct_type)
  int *SNRMBA, *SNRMBAinc;
  int MBA, MBAmax;
  int *dct_type;
{
  int SNRmacroblock_type, SNRcoded_block_pattern, SNRdct_type, dummy;
  int slice_vert_pos_ext, quantizer_scale_code, comp, code;

  ld = &enhan;

  if (*SNRMBAinc==0)
  {
    if (!Show_Bits(23)) /* next_start_code */ {
      next_start_code();
      code = Show_Bits(32);

      if (code<SLICE_START_CODE_MIN || code>SLICE_START_CODE_MAX) {
        /* only slice headers are allowed in picture_data */
        printf("SNR: Premature end of picture\n");
        return;
      }

      Flush_Buffer32();

      /* decode slice header (may change quantizer_scale) */
      slice_vert_pos_ext = slice_header();

      /* decode macroblock address increment */
      *SNRMBAinc = Get_macroblock_address_increment();

      /* set current location */
      *SNRMBA = ((slice_vert_pos_ext<<7) + (code&255) - 1)*mb_width + *SNRMBAinc - 1;

      *SNRMBAinc = 1; /* first macroblock in slice: not skipped */
    }
    else /* not next_start_code */
    {
      if (*SNRMBA>=MBAmax) {
        printf("Too many macroblocks in picture\n");
        return;
      }

      /* decode macroblock address increment */
      *SNRMBAinc = Get_macroblock_address_increment();
    }
  }

  if (*SNRMBA!=MBA) {
    /* streams out of sync */
    printf("Cant't synchronize streams\n");
    return;
  }

  if (*SNRMBAinc==1) /* not skipped */ {
    macroblock_modes(&SNRmacroblock_type, &dummy, &dummy,
      &dummy, &dummy, &dummy, &dummy, &dummy,
      &SNRdct_type);

    if (SNRmacroblock_type & MACROBLOCK_PATTERN)
      *dct_type = SNRdct_type;

    if (SNRmacroblock_type & MACROBLOCK_QUANT) {
      quantizer_scale_code = Get_Bits(5);
      ld->quantizer_scale =
        ld->q_scale_type ? Non_Linear_quantizer_scale[quantizer_scale_code] : quantizer_scale_code<<1;
    }

    /* macroblock_pattern */
    if (SNRmacroblock_type & MACROBLOCK_PATTERN) {
      SNRcoded_block_pattern = Get_coded_block_pattern();

      if (chroma_format==CHROMA422)
        SNRcoded_block_pattern = (SNRcoded_block_pattern<<2) | Get_Bits(2); /* coded_block_pattern_1 */
      else if (chroma_format==CHROMA444)
        SNRcoded_block_pattern = (SNRcoded_block_pattern<<6) | Get_Bits(6); /* coded_block_pattern_2 */
    }
    else
      SNRcoded_block_pattern = 0;

    /* decode blocks */
    for (comp=0; comp<block_count; comp++) {
      Clear_Block(comp);

      if (SNRcoded_block_pattern & (1<<(block_count-1-comp)))
        Decode_MPEG2_Non_Intra_Block(comp);
    }
  }
  else /* SNRMBAinc!=1: skipped macroblock */ {
    for (comp=0; comp<block_count; comp++)
      Clear_Block(comp);
  }

  ld = &base;
}
/*}}}*/
/*{{{*/
/* SCALABILITY: add SNR enhancement layer block data to base layer */
/* ISO/IEC 13818-2 section 7.8.3.4: Addition of coefficients from the two layes */
static void Sum_Block (comp)
int comp;
{
  short *Block_Ptr1, *Block_Ptr2;
  int i;

  Block_Ptr1 = base.block[comp];
  Block_Ptr2 = enhan.block[comp];

  for (i=0; i<64; i++)
    *Block_Ptr1++ += *Block_Ptr2++;
}

/*}}}*/
/*{{{*/
/* limit coefficients to -2048..2047 */
/* ISO/IEC 13818-2 section 7.4.3 and 7.4.4: Saturation and Mismatch control */
static void Saturate (Block_Ptr)
short *Block_Ptr;
{
  int i, sum, val;

  sum = 0;

  /* ISO/IEC 13818-2 section 7.4.3: Saturation */
  for (i=0; i<64; i++)
  {
    val = Block_Ptr[i];

    if (val>2047)
      val = 2047;
    else if (val<-2048)
      val = -2048;

    Block_Ptr[i] = val;
    sum+= val;
  }

  /* ISO/IEC 13818-2 section 7.4.4: Mismatch control */
  if ((sum&1)==0)
    Block_Ptr[63]^= 1;

}
/*}}}*/
/*{{{*/
/* reuse old picture buffers as soon as they are no longer needed based on life-time axioms of MPEG */
static void Update_Picture_Buffers() {

  int cc;              /* color component index */
  unsigned char *tmp;  /* temporary swap pointer */
  for (cc = 0; cc < 3; cc++) {
    /* B pictures do not need to be save for future reference */
    if (picture_coding_type == B_TYPE)
      current_frame[cc] = auxframe[cc];
    else {
      /* only update at the beginning of the coded frame */
      if (!Second_Field) {
        tmp = forward_reference_frame[cc];

        /* the previously decoded reference frame is stored
           coincident with the location where the backward
           reference frame is stored (backwards prediction is not
           needed in P pictures) */
        forward_reference_frame[cc] = backward_reference_frame[cc];

        /* update pointer for potential future B pictures */
        backward_reference_frame[cc] = tmp;
       }

      /* can erase over old backward reference frame since it is not used
         in a P picture, and since any subsequent B pictures will use the
         previously decoded I or P frame as the backward_reference_frame */
      current_frame[cc] = backward_reference_frame[cc];
      }

    /* IMPLEMENTATION:
       one-time folding of a line offset into the pointer which stores the
       memory address of the current frame saves offsets and conditional
       branches throughout the remainder of the picture processing loop */
    if (picture_structure == BOTTOM_FIELD)
      current_frame[cc]+= (cc==0) ? Coded_Picture_Width : Chroma_Width;
    }
  }
/*}}}*/

/*{{{*/
static void Copy_Frame (src, dst, width, height, parity, field_mode)
unsigned char *src;
unsigned char *dst;
int width;
int height;
int parity;        /* field parity (top or bottom) to overwrite */
int field_mode;    /* 0 = frame, 1 = field                      */
{
  int row, col;
  int s, d;
  int incr;

  s = d = 0;

  printf("COPYING (w=%d, h=%d, parity=%d, field_mode=%d)\n",
    width,height,parity,field_mode);

  if(field_mode)
  {
    incr = 2;

    if(parity==0)
      s += width;
  }
  else
    incr = 1;

  for(row=0; row<height; row+=incr)
  {
    for(col=0; col<width; col++)
      dst[d+col] = src[s+col];

    d += (width*incr);
    s += (width*incr);
  }
}
/*}}}*/
/*{{{*/
static int Read_Component (Filename, Frame, Width, Height)
char *Filename;
unsigned char *Frame;
int Width;
int Height;
{
  int Size;
  int Bytes_Read;
  int Infile;

  Size = Width*Height;

  printf("SUBS: reading %s\n", Filename);

  if (!(Infile=open(Filename,O_RDONLY|O_BINARY))<0) {
    printf("ERROR: unable to open reference filename (%s)\n", Filename);
    return(-1);
    }

  Bytes_Read = read(Infile, Frame, Size);

  if(Bytes_Read!=Size)
    printf("was able to read only %d bytes of %d of file %s\n", Bytes_Read, Size, Filename);

  close(Infile);
  return(0);
}
/*}}}*/
/*{{{*/
static int Read_Components (filename, frame, framenum)
char *filename;
unsigned char *frame[3];
int framenum;
{
  int err = 0;
  char outname[FILENAME_LENGTH];
  char name[FILENAME_LENGTH];

  sprintf(outname,filename,framenum);


  sprintf(name,"%s.Y",outname);
  err += Read_Component(name, frame[0], Coded_Picture_Width,
    Coded_Picture_Height);

  sprintf(name,"%s.U",outname);
  err += Read_Component(name, frame[1], Chroma_Width, Chroma_Height);

  sprintf(name,"%s.V",outname);
  err += Read_Component(name, frame[2], Chroma_Width, Chroma_Height);

  return(err);
}
/*}}}*/
/*{{{*/
/* Note: fields are only read to serve as the same-frame reference for a second field */
static void Read_Frame (fname, frame, framenum)
char *fname;
unsigned char *frame[];
int framenum;
{
  int parity;
  int rerr = 0;
  int field_mode;

  if (framenum < 0)
    printf ("ERROR: framenum (%d) is less than zero\n", framenum);

  rerr = Read_Components (fname, substitute_frame, framenum);
  if (rerr != 0)
    printf ("was unable to substitute frame\n");

  /* now copy to the appropriate buffer */
  /* first field (which we are attempting to substitute) must be
     of opposite field parity to the current one */
  if ((Second_Field) && (picture_coding_type == P_TYPE)) {
    parity      = (picture_structure==TOP_FIELD ? 1:0);
    field_mode  = (picture_structure==FRAME_PICTURE ? 0:1);
    }
  else {
    /* Like frame structued pictures, B pictures only substitute an entire frame
       since both fields always predict from the same frame (with respect to forward/backwards directions) */
    parity = 0;
    field_mode = 0;
    }

  Copy_Frame (substitute_frame[0], frame[0], Coded_Picture_Width, Coded_Picture_Height, parity, field_mode);
  Copy_Frame (substitute_frame[1], frame[1], Chroma_Width, Chroma_Height, parity, field_mode);
  Copy_Frame (substitute_frame[2], frame[2], Chroma_Width, Chroma_Height, parity, field_mode);
  }
/*}}}*/
/*{{{*/
void Substitute_Frame_Buffer (int bitstream_framenum, int sequence_framenum) {

  /* static tracking variables */
  static int previous_temporal_reference;
  static int previous_bitstream_framenum;
  static int previous_anchor_temporal_reference;
  static int previous_anchor_bitstream_framenum;
  static int previous_picture_coding_type;
  static int bgate;

  /* local temporary variables */
  int substitute_display_framenum;
  printf ("SUB: seq fn(%d) bitfn(%d) tempref(%d) picstr(%d) type(%d)\n",
    sequence_framenum, bitstream_framenum, temporal_reference, picture_structure, picture_coding_type);

  /* we don't substitute at the first picture of a sequence */
  if ((sequence_framenum != 0) || (Second_Field)) {
    /* only at the start of the frame */
    if ((picture_structure==FRAME_PICTURE)||(!Second_Field)) {
      if (picture_coding_type==P_TYPE) {
        /* the most recently decoded reference frame needs substituting */
        substitute_display_framenum = bitstream_framenum - 1;
        Read_Frame(Substitute_Picture_Filename, forward_reference_frame, substitute_display_framenum);
      }
      // only the first B frame in a consequitve set of B pictures loads a substitute backward_reference_frame
      // since all subsequent B frames predict from the same reference pictures */
      else if((picture_coding_type==B_TYPE)&&(bgate!=1)) {
        substitute_display_framenum = (previous_temporal_reference - temporal_reference) + bitstream_framenum - 1;
        Read_Frame(Substitute_Picture_Filename, backward_reference_frame, substitute_display_framenum);
        }
      } /* P fields can predict from the two most recently decoded fields, even from the first field of the same frame being decoded */
    else if (Second_Field && (picture_coding_type==P_TYPE)) {
      /* our favourite case: the IP field picture pair */
      if ((previous_picture_coding_type == I_TYPE) && (picture_coding_type == P_TYPE))
        substitute_display_framenum = bitstream_framenum;
      else /* our more generic P field picture pair */
        substitute_display_framenum = (temporal_reference - previous_anchor_temporal_reference) + bitstream_framenum - 1;

      Read_Frame(Substitute_Picture_Filename, current_frame, substitute_display_framenum);
      }
    else if ((picture_coding_type!=B_TYPE)||(picture_coding_type!=D_TYPE))
      printf("NO SUBS FOR THIS PICTURE\n");
    }

  /* set b gate so we don't redundantly load next time around */
  if (picture_coding_type == B_TYPE)
    bgate = 1;
  else
    bgate = 0;

  /* update general tracking variables */
  if ((picture_structure==FRAME_PICTURE)||(!Second_Field)) {
    previous_temporal_reference  = temporal_reference;
    previous_bitstream_framenum  = bitstream_framenum;
    }

  /* update reference frame tracking variables */
  if ((picture_coding_type!=B_TYPE) && ((picture_structure==FRAME_PICTURE)||Second_Field)) {
    previous_anchor_temporal_reference  = temporal_reference;
    previous_anchor_bitstream_framenum  = bitstream_framenum;
    }

  previous_picture_coding_type = picture_coding_type;
  }
/*}}}*/

/*{{{*/
static void Read_Lower_Layer_Component_Framewise (comp,lw,lh)
     int comp;
     int lw, lh;
{
  FILE *fd;
  char fname[256];
  char ext[3][3] = {".Y",".U",".V"};
/*  char *ext = {".Y",".U",".V"}; */
  int i,j;

  sprintf (fname, Lower_Layer_Picture_Filename, True_Framenum);
  strcat (fname,ext[comp]);

  fd=fopen(fname,"rb");
  if (fd==NULL) exit(-1);
  for (j=0; j<lh; j++) {
     for (i=0; i<lw; i++)
       llframe0[comp][lw*j+i]=getc(fd);
     if (! lower_layer_progressive_frame) {
  j++;
  for (i=0; i<lw; i++)
    llframe1[comp][lw*j+i]=getc(fd);
     }
  }
  fclose(fd);
}
/*}}}*/
/*{{{*/
static void Read_Lower_Layer_Component_Fieldwise (comp,lw,lh)
     int comp;
     int lw, lh;
{
  FILE *fd;
  char fname[256];
  char ext[3][3] = {".Y",".U",".V"};
/*  char *ext = {".Y",".U",".V"}; */
  int i,j;

  sprintf (fname, Lower_Layer_Picture_Filename, True_Framenum, lower_layer_progressive_frame ? 'f':'a');
  strcat (fname, ext[comp]);

  fd=fopen(fname,"rb");
  if (fd==NULL) exit(-1);
  for (j=0; j<lh; j+=lower_layer_progressive_frame?1:2)
    for (i=0; i<lw; i++)
      llframe0[comp][lw*j+i]=getc(fd);
  fclose(fd);

  if (! lower_layer_progressive_frame) {
    sprintf (fname, Lower_Layer_Picture_Filename, True_Framenum,'b');
    strcat (fname, ext[comp]);

    fd = fopen (fname, "rb");
    if (fd == NULL)
      exit(-1);
    for (j = 1; j < lh; j += 2)
      for (i = 0; i < lw; i++)
        llframe1[comp][lw*j+i] = getc(fd);
    fclose(fd);
  }
}

/*}}}*/
/*{{{*/
/* vertical resampling */
static void Subsample_Vertical (s,d,lx,lys,lyd,m,n,j0,dj)
unsigned char *s;
short *d;
int lx, lys, lyd, m, n, j0, dj;
{
  int i, j, c1, c2, jd;
  unsigned char *s1, *s2;
  short *d1;

  for (j=j0; j<lyd; j+=dj)
  {
    d1 = d + lx*j;
    jd = (j*m)/n;
    s1 = s + lx*jd;
    s2 = (jd<lys-1)? s1+lx : s1;
    c2 = (16*((j*m)%n) + (n>>1))/n;
    c1 = 16 - c2;
    for (i=0; i<lx; i++)
      d1[i] = c1*s1[i] + c2*s2[i];
  }
}
/*}}}*/
/*{{{*/
/* horizontal resampling */
static void Subsample_Horizontal (s,d,x0,lx,lxs,lxd,ly,m,n)
short *s;
unsigned char *d;
int x0, lx, lxs, lxd, ly, m, n;
{
  int i, i1, j, id, c1, c2, v;
  short *s1, *s2;
  unsigned char *d1;

  for (i1=0; i1<lx; i1++)
  {
    d1 = d + i1;
    i = x0 + i1;
    id = (i*m)/n;
    s1 = s+id;
    s2 = (id<lxs-1) ? s1+1 : s1;
    c2 = (16*((i*m)%n) + (n>>1))/n;
    c1 = 16 - c2;
    for (j=0; j<ly; j++)
    {
      v = c1*(*s1) + c2*(*s2);
      *d1 = (v + ((v>=0) ? 128 : 127))>>8;
      d1+= lxd;
      s1+= lxs;
      s2+= lxs;
    }
  }
}
/*}}}*/
/*{{{*/
/* deinterlace one field (interpolate opposite parity samples)
 *
 * deinterlacing is done in-place: if j0=1, fld0 contains the input field in
 * its even lines and the odd lines are interpolated by this routine
 * if j0=0, the input field is in the odd lines and the even lines are
 * interpolated
 *
 * fld0: field to be deinterlaced
 * fld1: other field (referenced by the two field aperture filter)
 * j0:   0: interpolate even (top) lines, 1: interpolate odd (bottom) lines
 * lx:   width of fld0 and fld1
 * ly:   height of the deinterlaced field (has to be even)
 * aperture: 1: use one field aperture filter (two field otherwise)
 */
static void Deinterlace (fld0,fld1,j0,lx,ly,aperture)
unsigned char *fld0,*fld1;
int j0,lx,ly; /* ly has to be even */
int aperture;
{
  int i,j,v;
  unsigned char *p0, *p0m1, *p0p1, *p1, *p1m2, *p1p2;

  /* deinterlace one field */
  for (j=j0; j<ly; j+=2)
  {
    p0 = fld0+lx*j;
    p0m1 = (j==0)    ? p0+lx : p0-lx;
    p0p1 = (j==ly-1) ? p0-lx : p0+lx;

    if (aperture)
      for (i=0; i<lx; i++)
        p0[i] = (unsigned int)(p0m1[i] + p0p1[i] + 1)>>1;
    else
    {
      p1 = fld1 + lx*j;
      p1m2 = (j<2)     ? p1 : p1-2*lx;
      p1p2 = (j>=ly-2) ? p1 : p1+2*lx;
      for (i=0; i<lx; i++)
      {
        v = 8*(p0m1[i]+p0p1[i]) + 2*p1[i] - p1m2[i] - p1p2[i];
        p0[i] = Clip[(v + ((v>=0) ? 8 : 7))>>4];
      }
    }
  }
}
/*}}}*/
/*{{{*/
/* form spatial prediction */
static void Make_Spatial_Prediction_Frame (progressive_frame,
  llprogressive_frame,fld0,fld1,tmp,dst,llx0,lly0,llw,llh,horizontal_size,
  vertical_size,vm,vn,hm,hn,aperture)
int progressive_frame,llprogressive_frame;
unsigned char *fld0,*fld1;
short *tmp;
unsigned char *dst;
int llx0,lly0,llw,llh,horizontal_size,vertical_size,vm,vn,hm,hn,aperture;
{
  int w, h, x0, llw2, llh2;

  llw2 = (llw*hn)/hm;
  llh2 = (llh*vn)/vm;

  if (llprogressive_frame)
  {
    /* progressive -> progressive / interlaced */
    Subsample_Vertical(fld0,tmp,llw,llh,llh2,vm,vn,0,1);
  }
  else if (progressive_frame)
  {
    /* interlaced -> progressive */
    if (lower_layer_deinterlaced_field_select)
    {
      Deinterlace(fld1,fld0,0,llw,llh,aperture);
      Subsample_Vertical(fld1,tmp,llw,llh,llh2,vm,vn,0,1);
    }
    else
    {
      Deinterlace(fld0,fld1,1,llw,llh,aperture);
      Subsample_Vertical(fld0,tmp,llw,llh,llh2,vm,vn,0,1);
    }
  }
  else
  {
    /* interlaced -> interlaced */
    Deinterlace(fld0,fld1,1,llw,llh,aperture);
    Deinterlace(fld1,fld0,0,llw,llh,aperture);
    Subsample_Vertical(fld0,tmp,llw,llh,llh2,vm,vn,0,2);
    Subsample_Vertical(fld1,tmp,llw,llh,llh2,vm,vn,1,2);
  }

    /* vertical limits */
    if (lly0<0)
    {
      tmp-= llw*lly0;
      llh2+= lly0;
      if (llh2<0)
        llh2 = 0;
      h = (vertical_size<llh2) ? vertical_size : llh2;
    }
    else
    {
      dst+= horizontal_size*lly0;
      h= vertical_size - lly0;
      if (h>llh2)
        h = llh2;
    }

    /* horizontal limits */
    if (llx0<0)
    {
      x0 = -llx0;
      llw2+= llx0;
      if (llw2<0)
        llw2 = 0;
      w = (horizontal_size<llw2) ? horizontal_size : llw2;
    }
    else
    {
      dst+= llx0;
      x0 = 0;
      w = horizontal_size - llx0;
      if (w>llw2)
        w = llw2;
    }

  Subsample_Horizontal(tmp,dst,x0,w,llw,horizontal_size,h,hm,hn);
}
/*}}}*/
/*{{{*/
void Spatial_Prediction() {

  {
    Read_Lower_Layer_Component_Fieldwise(0,lower_layer_prediction_horizontal_size,
      lower_layer_prediction_vertical_size);      /* Y */
    Read_Lower_Layer_Component_Fieldwise(1,lower_layer_prediction_horizontal_size>>1,
      lower_layer_prediction_vertical_size>>1);   /* Cb ("U") */
    Read_Lower_Layer_Component_Fieldwise(2,lower_layer_prediction_horizontal_size>>1,
      lower_layer_prediction_vertical_size>>1);   /* Cr ("V") */
  }


  Make_Spatial_Prediction_Frame  /* Y */
    (progressive_frame,lower_layer_progressive_frame,llframe0[0],llframe1[0],
     lltmp,current_frame[0],lower_layer_horizontal_offset,
     lower_layer_vertical_offset,
     lower_layer_prediction_horizontal_size,
     lower_layer_prediction_vertical_size,
     horizontal_size,vertical_size,vertical_subsampling_factor_m,
     vertical_subsampling_factor_n,horizontal_subsampling_factor_m,
     horizontal_subsampling_factor_n,
     picture_structure!=FRAME_PICTURE); /* this changed from CD to DIS */

  Make_Spatial_Prediction_Frame  /* Cb */
    (progressive_frame,lower_layer_progressive_frame,llframe0[1],llframe1[1],
     lltmp,current_frame[1],lower_layer_horizontal_offset/2,
     lower_layer_vertical_offset/2,
     lower_layer_prediction_horizontal_size>>1,
     lower_layer_prediction_vertical_size>>1,
     horizontal_size>>1,vertical_size>>1,vertical_subsampling_factor_m,
     vertical_subsampling_factor_n,horizontal_subsampling_factor_m,
     horizontal_subsampling_factor_n,1);

  Make_Spatial_Prediction_Frame  /* Cr */
    (progressive_frame,lower_layer_progressive_frame,llframe0[2],llframe1[2],
     lltmp,current_frame[2],lower_layer_horizontal_offset/2,
     lower_layer_vertical_offset/2,
     lower_layer_prediction_horizontal_size>>1,
     lower_layer_prediction_vertical_size>>1,
     horizontal_size>>1,vertical_size>>1,vertical_subsampling_factor_m,
     vertical_subsampling_factor_n,horizontal_subsampling_factor_m,
     horizontal_subsampling_factor_n,1);
}
/*}}}*/

/*{{{*/
/* decode one frame or field picture */
void Decode_Picture (int bitstream_framenum, int sequence_framenum) {

  if (picture_structure==FRAME_PICTURE && Second_Field) {
    /* recover from illegal number of field pictures */
    printf ("odd number of field pictures\n");
    Second_Field = 0;
    }

  /* IMPLEMENTATION: update picture buffer pointers */
  Update_Picture_Buffers();

  /* ISO/IEC 13818-4 section 2.4.5.4 "frame buffer intercept method" */
  /* (section number based on November 1995 (Dallas) draft of the conformance document) */
  if (Ersatz_Flag)
    Substitute_Frame_Buffer (bitstream_framenum, sequence_framenum);

  /* form spatial scalable picture */
  /* ISO/IEC 13818-2 section 7.7: Spatial scalability */
  if (base.pict_scal && !Second_Field)
    Spatial_Prediction();

  /* decode picture data ISO/IEC 13818-2 section 6.2.3.7 */
  picture_data (bitstream_framenum);

  /* write or display current or previously decoded reference frame */
  /* ISO/IEC 13818-2 section 6.1.1.11: Frame reordering */
  frame_reorder (bitstream_framenum, sequence_framenum);

  if (picture_structure!=FRAME_PICTURE)
    Second_Field = !Second_Field;
  }
/*}}}*/
/*{{{*/
void Output_Last_Frame_of_Sequence (int Framenum) {

  if (Second_Field)
    printf ("last frame incomplete, not stored\n");
  else
    Write_Frame (backward_reference_frame,Framenum-1);
  }
/*}}}*/

/*{{{*/
static int Headers() {

  int ret;
  ld = &base;

  /* return when end of sequence (0) or picture header has been parsed (1) */
  ret = Get_Hdr();
  return ret;
  }
/*}}}*/
/*{{{*/
/* mostly IMPLEMENTAION specific rouintes */
static void Initialize_Sequence() {

  int cc, size;
  static int Table_6_20[3] = {6,8,12};

  /* force MPEG-1 parameters for proper decoder behavior see ISO/IEC 13818-2 section D.9.14 */
  if (!base.MPEG2_Flag) {
    progressive_sequence = 1;
    progressive_frame = 1;
    picture_structure = FRAME_PICTURE;
    frame_pred_frame_dct = 1;
    chroma_format = CHROMA420;
    matrix_coefficients = 5;
    }

  /* round to nearest multiple of coded macroblocks  ISO/IEC 13818-2 section 6.3.3 sequence_header() */
  mb_width = (horizontal_size+15)/16;
  mb_height = (base.MPEG2_Flag && !progressive_sequence) ? 2*((vertical_size+31)/32) : (vertical_size+15)/16;

  Coded_Picture_Width = 16*mb_width;
  Coded_Picture_Height = 16*mb_height;

  /* ISO/IEC 13818-2 sections 6.1.1.8, 6.1.1.9, and 6.1.1.10 */
  Chroma_Width = (chroma_format==CHROMA444) ? Coded_Picture_Width : Coded_Picture_Width>>1;
  Chroma_Height = (chroma_format!=CHROMA420) ? Coded_Picture_Height : Coded_Picture_Height>>1;

  /* derived based on Table 6-20 in ISO/IEC 13818-2 section 6.3.17 */
  block_count = Table_6_20[chroma_format-1];

  for (cc=0; cc<3; cc++) {
    if (cc==0)
      size = Coded_Picture_Width*Coded_Picture_Height;
    else
      size = Chroma_Width*Chroma_Height;

    if (!(backward_reference_frame[cc] = (unsigned char *)malloc(size)))
      Error ("backward_reference_frame[] malloc failed\n");

    if (!(forward_reference_frame[cc] = (unsigned char *)malloc(size)))
      Error ("forward_reference_frame[] malloc failed\n");

    if (!(auxframe[cc] = (unsigned char *)malloc(size)))
      Error ("auxframe[] malloc failed\n");

    if(Ersatz_Flag)
      if (!(substitute_frame[cc] = (unsigned char *)malloc(size)))
        Error ("substitute_frame[] malloc failed\n");

    if (base.scalable_mode==SC_SPAT) {
      /* this assumes lower layer is 4:2:0 */
      if (!(llframe0[cc] = (unsigned char *)malloc((lower_layer_prediction_horizontal_size*lower_layer_prediction_vertical_size)/(cc?4:1))))
        Error ("llframe0 malloc failed\n");
      if (!(llframe1[cc] = (unsigned char *)malloc((lower_layer_prediction_horizontal_size*lower_layer_prediction_vertical_size)/(cc?4:1))))
        Error ("llframe1 malloc failed\n");
      }
    }

  /* SCALABILITY: Spatial */
  if (base.scalable_mode==SC_SPAT) {
    if (!(lltmp = (short *)malloc(lower_layer_prediction_horizontal_size*((lower_layer_prediction_vertical_size*vertical_subsampling_factor_n)/vertical_subsampling_factor_m)*sizeof(short))))
      Error ("lltmp malloc failed\n");
    }
  }
/*}}}*/
/*{{{*/
static void Deinitialize_Sequence() {

  int i;

  /* clear flags */
  base.MPEG2_Flag=0;

  for (i=0;i<3;i++) {
    free(backward_reference_frame[i]);
    free(forward_reference_frame[i]);
    free(auxframe[i]);

    if (base.scalable_mode==SC_SPAT) {
      free(llframe0[i]);
      free(llframe1[i]);
      }
    }

  if (base.scalable_mode==SC_SPAT)
    free (lltmp);
  }
/*}}}*/
/*{{{*/
static int video_sequence (int* Bitstream_Framenumber) {

  int Bitstream_Framenum = *Bitstream_Framenumber;
  int Sequence_Framenum = 0;

  Initialize_Sequence();

  /* decode picture whose header has already been parsed in Decode_Bitstream() */
  Decode_Picture (Bitstream_Framenum, Sequence_Framenum);

  /* update picture numbers */
  if (!Second_Field) {
    Bitstream_Framenum++;
    Sequence_Framenum++;
    }

  /* loop through the rest of the pictures in the sequence */
  int Return_Value;
  while ((Return_Value = Headers())) {
    Decode_Picture (Bitstream_Framenum, Sequence_Framenum);
    if (!Second_Field) {
      Bitstream_Framenum++;
      Sequence_Framenum++;
      }
    }

  /* put last frame */
  if (Sequence_Framenum!=0)
    Output_Last_Frame_of_Sequence(Bitstream_Framenum);

  Deinitialize_Sequence();

  *Bitstream_Framenumber = Bitstream_Framenum;
  return (Return_Value);
  }
/*}}}*/
/*{{{*/
static void Initialize_Decoder() {

  /* Clip table */
  if (!(Clip = (unsigned char *)malloc(1024)))
    Error ("Clip[] malloc failed\n");

  Clip += 384;
  for (int i=-384; i<640; i++)
    Clip[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

  Initialize_Fast_IDCT();
  }
/*}}}*/
/*{{{*/
static int Decode_Bitstream() {

  int Bitstream_Framenum = 0;
  for (;;) {
    int ret = Headers();
    if (ret == 1)
      ret = video_sequence (&Bitstream_Framenum);
    else
      return (ret);
    }
  }
/*}}}*/

/*{{{*/
int main (int argc, char* argv[]) {

  Lower_Layer_Picture_Filename = " ";
  Substitute_Picture_Filename  = " ";
  Enhancement_Layer_Bitstream_Filename = " ";

  ld = &base;
  base.Infile = open (argv[1], O_RDONLY | O_BINARY);

  System_Stream_Flag = 1;
  Initialize_Buffer();
  Initialize_Decoder();
  int ret = Decode_Bitstream();
  close (base.Infile);

  return 0;
  }
/*}}}*/
