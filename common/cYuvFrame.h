// cYuvFrame.h
#pragma once

//{{{  DCT_8_INV_ROWX2
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

//{{{
static __declspec(align(64)) short sse2_tab_i_04[] = {
  16384, 21407, 16384,  8867, 16384, -8867, 16384,-21407,  // w05 w04 w01 w00 w13 w12 w09 w08
  16384,  8867,-16384,-21407,-16384, 21407, 16384, -8867,  // w07 w06 w03 w02 w15 w14 w11 w10
  22725, 19266, 19266, -4520, 12873,-22725,  4520,-12873,
  12873,  4520,-22725,-12873,  4520, 19266, 19266,-22725 };
//}}}
//{{{
static __declspec(align(64)) short sse2_tab_i_17[] = {
  22725, 29692, 22725, 12299, 22725,-12299, 22725,-29692,
  22725, 12299,-22725,-29692,-22725, 29692, 22725,-12299,
  31521, 26722, 26722, -6270, 17855,-31521,  6270,-17855,
  17855,  6270,-31521,-17855,  6270, 26722, 26722,-31521 };
//}}}
//{{{
static __declspec(align(64)) short sse2_tab_i_26[] = {
  21407, 27969, 21407, 11585, 21407,-11585, 21407,-27969,
  21407, 11585,-21407,-27969,-21407, 27969, 21407,-11585,
  29692, 25172, 25172, -5906, 16819,-29692,  5906,-16819,
  16819,  5906,-29692,-16819,  5906, 25172, 25172,-29692 };
//}}}
//{{{
static __declspec(align(64)) short sse2_tab_i_35[] = {
  19266, 25172, 19266, 10426, 19266,-10426, 19266,-25172,
  19266, 10426,-19266,-25172,-19266, 25172, 19266,-10426,
  26722, 22654, 22654, -5315, 15137,-26722,  5315,-15137,
  15137,  5315,-26722,-15137,  5315, 22654, 22654,-26722 };
//}}}

class cYuvFrame  {
public:
  //{{{
  void set (int64_t pts, uint8_t** yuv, int* strides, int width, int height, int len, int pictType) {

    bool sizeChanged = (mWidth != width) || (mHeight != height) || (mYStride != strides[0]) || (mUVStride != strides[1]);
    mWidth = width;
    mHeight = height;
    mLen = len;
    mPictType = pictType;

    mYStride = strides[0];
    mUVStride = strides[1];

    if (sizeChanged && mYbuf)
      free (mYbuf);
    mYbuf = (uint8_t*)_mm_malloc (height * mYStride, 128);
    memcpy (mYbuf, yuv[0], height * mYStride);

    if (sizeChanged && mUbuf)
      free (mUbuf);
    mUbuf = (uint8_t*)_mm_malloc ((height/2) * mUVStride, 128);
    memcpy (mUbuf, yuv[1], (height/2) * mUVStride);

    if (sizeChanged && mVbuf)
      free (mVbuf);
    mVbuf = (uint8_t*)_mm_malloc ((height/2) * mUVStride, 128);
    memcpy (mVbuf, yuv[2], (height/2) * mUVStride);

    mPts = pts;
    }
  //}}}
  //{{{
  uint32_t* bgra() {

    auto argb = (uint32_t*)_mm_malloc (mWidth * 4 * mHeight, 128);
    auto argbStride = mWidth;

    __m128i y0r0, y0r1, u0, v0;
    __m128i y00r0, y01r0, y00r1, y01r1;
    __m128i u00, u01, v00, v01;
    __m128i rv00, rv01, gu00, gu01, gv00, gv01, bu00, bu01;
    __m128i r00, r01, g00, g01, b00, b01;
    __m128i rgb0123, rgb4567, rgb89ab, rgbcdef;
    __m128i gbgb;
    __m128i ysub, uvsub;
    __m128i zero, facy, facrv, facgu, facgv, facbu;
    __m128i *srcy128r0, *srcy128r1;
    __m128i *dstrgb128r0, *dstrgb128r1;
    __m64   *srcu64, *srcv64;

    int x, y;

    ysub  = _mm_set1_epi32 (0x00100010);
    uvsub = _mm_set1_epi32 (0x00800080);

    facy  = _mm_set1_epi32 (0x004a004a);
    facrv = _mm_set1_epi32 (0x00660066);
    facgu = _mm_set1_epi32 (0x00190019);
    facgv = _mm_set1_epi32 (0x00340034);
    facbu = _mm_set1_epi32 (0x00810081);

    zero  = _mm_set1_epi32( 0x00000000 );

    for (y = 0; y < mHeight; y += 2) {
      srcy128r0 = (__m128i *)(mYbuf + mYStride*y);
      srcy128r1 = (__m128i *)(mYbuf + mYStride*y + mYStride);
      srcu64 = (__m64 *)(mUbuf + mUVStride*(y/2));
      srcv64 = (__m64 *)(mVbuf + mUVStride*(y/2));

      dstrgb128r0 = (__m128i *)(argb + argbStride*y);
      dstrgb128r1 = (__m128i *)(argb + argbStride*y + argbStride);

      for (x = 0; x < mWidth; x += 16) {
        u0 = _mm_loadl_epi64( (__m128i *)srcu64 ); srcu64++;
        v0 = _mm_loadl_epi64( (__m128i *)srcv64 ); srcv64++;

        y0r0 = _mm_load_si128( srcy128r0++ );
        y0r1 = _mm_load_si128( srcy128r1++ );

        // constant y factors
        y00r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r0, zero), ysub), facy);
        y01r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r0, zero), ysub), facy);
        y00r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r1, zero), ysub), facy);
        y01r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r1, zero), ysub), facy);

        // expand u and v so they're aligned with y values
        u0  = _mm_unpacklo_epi8 (u0, zero);
        u00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (u0, u0), uvsub);
        u01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (u0, u0), uvsub);

        v0  = _mm_unpacklo_epi8( v0,  zero );
        v00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (v0, v0), uvsub);
        v01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (v0, v0), uvsub);

        // common factors on both rows.
        rv00 = _mm_mullo_epi16 (facrv, v00);
        rv01 = _mm_mullo_epi16 (facrv, v01);
        gu00 = _mm_mullo_epi16 (facgu, u00);
        gu01 = _mm_mullo_epi16 (facgu, u01);
        gv00 = _mm_mullo_epi16 (facgv, v00);
        gv01 = _mm_mullo_epi16 (facgv, v01);
        bu00 = _mm_mullo_epi16 (facbu, u00);
        bu01 = _mm_mullo_epi16 (facbu, u01);

        // row 0
        r00 = _mm_srai_epi16 (_mm_add_epi16 (y00r0, rv00), 6);
        r01 = _mm_srai_epi16 (_mm_add_epi16 (y01r0, rv01), 6);
        g00 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y00r0, gu00), gv00), 6);
        g01 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y01r0, gu01), gv01), 6);
        b00 = _mm_srai_epi16 (_mm_add_epi16 (y00r0, bu00), 6);
        b01 = _mm_srai_epi16 (_mm_add_epi16 (y01r0, bu01), 6);

        r00 = _mm_packus_epi16 (r00, r01);         // rrrr.. saturated
        g00 = _mm_packus_epi16 (g00, g01);         // gggg.. saturated
        b00 = _mm_packus_epi16 (b00, b01);         // bbbb.. saturated

        r01     = _mm_unpacklo_epi8 (r00, zero); // 0r0r..
        gbgb    = _mm_unpacklo_epi8 (b00, g00);  // gbgb..
        rgb0123 = _mm_unpacklo_epi16 (gbgb, r01);  // 0rgb0rgb..
        rgb4567 = _mm_unpackhi_epi16 (gbgb, r01);  // 0rgb0rgb..

        r01     = _mm_unpackhi_epi8 (r00, zero);
        gbgb    = _mm_unpackhi_epi8 (b00, g00 );
        rgb89ab = _mm_unpacklo_epi16 (gbgb, r01);
        rgbcdef = _mm_unpackhi_epi16 (gbgb, r01);

        _mm_store_si128 (dstrgb128r0++, rgb0123);
        _mm_store_si128 (dstrgb128r0++, rgb4567);
        _mm_store_si128 (dstrgb128r0++, rgb89ab);
        _mm_store_si128 (dstrgb128r0++, rgbcdef);

        // row 1
        r00 = _mm_srai_epi16 (_mm_add_epi16 (y00r1, rv00), 6);
        r01 = _mm_srai_epi16 (_mm_add_epi16 (y01r1, rv01), 6);
        g00 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y00r1, gu00), gv00), 6);
        g01 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y01r1, gu01), gv01), 6);
        b00 = _mm_srai_epi16 (_mm_add_epi16 (y00r1, bu00), 6);
        b01 = _mm_srai_epi16 (_mm_add_epi16 (y01r1, bu01), 6);

        r00 = _mm_packus_epi16 (r00, r01);         // rrrr.. saturated
        g00 = _mm_packus_epi16 (g00, g01);         // gggg.. saturated
        b00 = _mm_packus_epi16 (b00, b01);         // bbbb.. saturated

        r01     = _mm_unpacklo_epi8 (r00,  zero); // 0r0r..
        gbgb    = _mm_unpacklo_epi8 (b00,  g00);  // gbgb..
        rgb0123 = _mm_unpacklo_epi16 (gbgb, r01);  // 0rgb0rgb..
        rgb4567 = _mm_unpackhi_epi16 (gbgb, r01);  // 0rgb0rgb..

        r01     = _mm_unpackhi_epi8 (r00, zero);
        gbgb    = _mm_unpackhi_epi8 (b00, g00);
        rgb89ab = _mm_unpacklo_epi16 (gbgb, r01);
        rgbcdef = _mm_unpackhi_epi16 (gbgb, r01);

        _mm_store_si128 (dstrgb128r1++, rgb0123);
        _mm_store_si128 (dstrgb128r1++, rgb4567);
        _mm_store_si128 (dstrgb128r1++, rgb89ab);
        _mm_store_si128 (dstrgb128r1++, rgbcdef);
        }
      }

    return argb;
    }
  //}}}
  //{{{
  void freeResources() {

    mPts = 0;
    mWidth = 0;
    mHeight = 0;
    mLen = 0;

    mYStride = 0;
    mUVStride = 0;

    if (mYbuf)
      _mm_free (mYbuf);
    mYbuf = nullptr;

    if (mUbuf)
      _mm_free (mUbuf);
    mUbuf = nullptr;

    if (mVbuf)
      _mm_free (mVbuf);
    mVbuf = nullptr;
    }
  //}}}
  //{{{
  void invalidate() {

    mPts = 0;
    mLen = 0;
    }
  //}}}

  int64_t mPts = 0;
  int mWidth = 0;
  int mHeight = 0;
  int mLen = 0;
  int mPictType = 0;

  int mYStride = 0;
  int mUVStride= 0;

  uint8_t* mYbuf = nullptr;
  uint8_t* mUbuf = nullptr;
  uint8_t* mVbuf = nullptr;
  };
