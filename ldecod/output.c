/*{{{*/
#include "global.h"
#include "mbuffer.h"
#include "image.h"
#include "memalloc.h"
#include "sei.h"
#include "fast_memory.h"
/*}}}*/

/*{{{*/
static void allocate_p_dec_pic(VideoParameters *p_Vid, DecodedPicList *pDecPic, StorablePicture *p, int iLumaSize, int iFrameSize, int iLumaSizeX, int iLumaSizeY, int iChromaSizeX, int iChromaSizeY)
{
  int symbol_size_in_bytes = ((p_Vid->pic_unit_bitsize_on_disk+7) >> 3);

  if(pDecPic->pY)
    mem_free(pDecPic->pY);
  pDecPic->iBufSize = iFrameSize;
  pDecPic->pY = mem_malloc(pDecPic->iBufSize);
  pDecPic->pU = pDecPic->pY+iLumaSize;
  pDecPic->pV = pDecPic->pU + ((iFrameSize-iLumaSize)>>1);
  //init;
  pDecPic->iYUVFormat = p->chroma_format_idc;
  pDecPic->iYUVStorageFormat = 0;
  pDecPic->iBitDepth = p_Vid->pic_unit_bitsize_on_disk;
  pDecPic->iWidth = iLumaSizeX; //p->size_x;
  pDecPic->iHeight = iLumaSizeY; //p->size_y;
  pDecPic->iYBufStride = iLumaSizeX*symbol_size_in_bytes; //p->size_x *symbol_size_in_bytes;
  pDecPic->iUVBufStride = iChromaSizeX*symbol_size_in_bytes; //p->size_x_cr*symbol_size_in_bytes;
}
/*}}}*/

/*{{{*/
static void write_out_picture(VideoParameters *p_Vid, StorablePicture *p, int p_out)
{
  InputParameters *p_Inp = p_Vid->p_Inp;
  DecodedPicList *pDecPic;

  static const int SubWidthC  [4]= { 1, 2, 2, 1};
  static const int SubHeightC [4]= { 1, 2, 1, 1};

  int crop_left, crop_right, crop_top, crop_bottom;
  int symbol_size_in_bytes = ((p_Vid->pic_unit_bitsize_on_disk+7) >> 3);
  int rgb_output =  p_Vid->p_EncodePar[p->layer_id]->rgb_output; //(p_Vid->active_sps->vui_seq_parameters.matrix_coefficients==0);
  unsigned char *buf;
  //int iPicSizeTab[4] = {2, 3, 4, 6};
  int iLumaSize, iFrameSize;
  int iLumaSizeX, iLumaSizeY;
  int iChromaSizeX, iChromaSizeY;

  if (p->non_existing)
    return;

  // should this be done only once?
  if (p->frame_cropping_flag) {
    /*{{{*/
    crop_left   = SubWidthC [p->chroma_format_idc] * p->frame_crop_left_offset;
    crop_right  = SubWidthC [p->chroma_format_idc] * p->frame_crop_right_offset;
    crop_top    = SubHeightC[p->chroma_format_idc] * ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_top_offset;
    crop_bottom = SubHeightC[p->chroma_format_idc] * ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_bottom_offset;
    }
    /*}}}*/
  else
    crop_left = crop_right = crop_top = crop_bottom = 0;

  iChromaSizeX = p->size_x_cr- p->frame_crop_left_offset -p->frame_crop_right_offset;
  iChromaSizeY = p->size_y_cr - ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_top_offset -( 2 - p->frame_mbs_only_flag ) * p->frame_crop_bottom_offset;
  iLumaSizeX = p->size_x - crop_left-crop_right;
  iLumaSizeY = p->size_y - crop_top - crop_bottom;
  iLumaSize  = iLumaSizeX * iLumaSizeY * symbol_size_in_bytes;
  iFrameSize = (iLumaSizeX * iLumaSizeY + 2 * (iChromaSizeX * iChromaSizeY)) * symbol_size_in_bytes; //iLumaSize*iPicSizeTab[p->chroma_format_idc]/2;
  //printf ("write frame size: %dx%d\n", p->size_x-crop_left-crop_right,p->size_y-crop_top-crop_bottom );

  // KS: this buffer should actually be allocated only once, but this is still much faster than the previous version
  pDecPic = get_one_avail_dec_pic_from_list (p_Vid->pDecOuputPic, 0, 0);
  if( (pDecPic->pY == NULL) || (pDecPic->iBufSize < iFrameSize) )
    allocate_p_dec_pic(p_Vid, pDecPic, p, iLumaSize, iFrameSize, iLumaSizeX, iLumaSizeY, iChromaSizeX, iChromaSizeY);

  pDecPic->bValid = 1;

  pDecPic->iPOC = p->frame_poc;

  if (NULL == pDecPic->pY)
    no_mem_exit("write_out_picture: buf");

  if (rgb_output) {
    /*{{{*/
    buf = malloc (p->size_x * p->size_y * symbol_size_in_bytes);
    crop_left   = p->frame_crop_left_offset;
    crop_right  = p->frame_crop_right_offset;
    crop_top    = ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_top_offset;
    crop_bottom = ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_bottom_offset;

    p_Vid->img2buf (p->imgUV[1], buf, p->size_x_cr, p->size_y_cr, symbol_size_in_bytes, crop_left, crop_right, crop_top, crop_bottom, pDecPic->iYBufStride);

    if (p->frame_cropping_flag) {
      crop_left   = SubWidthC[p->chroma_format_idc] * p->frame_crop_left_offset;
      crop_right  = SubWidthC[p->chroma_format_idc] * p->frame_crop_right_offset;
      crop_top    = SubHeightC[p->chroma_format_idc]*( 2 - p->frame_mbs_only_flag ) * p->frame_crop_top_offset;
      crop_bottom = SubHeightC[p->chroma_format_idc]*( 2 - p->frame_mbs_only_flag ) * p->frame_crop_bottom_offset;
      }
    else
      crop_left = crop_right = crop_top = crop_bottom = 0;

    if(buf)
      free(buf);
    }
    /*}}}*/

  buf = (pDecPic->bValid==1)? pDecPic->pY: pDecPic->pY+iLumaSizeX*symbol_size_in_bytes;
  p_Vid->img2buf (p->imgY, buf, p->size_x, p->size_y, symbol_size_in_bytes, crop_left, crop_right, crop_top, crop_bottom, pDecPic->iYBufStride);

  if (p->chroma_format_idc!=YUV400) {
    /*{{{*/
    crop_left   = p->frame_crop_left_offset;
    crop_right  = p->frame_crop_right_offset;
    crop_top    = ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_top_offset;
    crop_bottom = ( 2 - p->frame_mbs_only_flag ) * p->frame_crop_bottom_offset;
    buf = (pDecPic->bValid==1)? pDecPic->pU : pDecPic->pU + iChromaSizeX*symbol_size_in_bytes;
    p_Vid->img2buf (p->imgUV[0], buf, p->size_x_cr, p->size_y_cr, symbol_size_in_bytes, crop_left, crop_right, crop_top, crop_bottom, pDecPic->iUVBufStride);
    /*}}}*/
    if (!rgb_output) {
      /*{{{*/
      buf = (pDecPic->bValid==1)? pDecPic->pV : pDecPic->pV + iChromaSizeX*symbol_size_in_bytes;
      p_Vid->img2buf (p->imgUV[1], buf, p->size_x_cr, p->size_y_cr, symbol_size_in_bytes, crop_left, crop_right, crop_top, crop_bottom, pDecPic->iUVBufStride);
      }
      /*}}}*/
    }
  else {
    if (p_Inp->write_uv) {
      /*{{{*/
      int i,j;
      imgpel cr_val = (imgpel) (1<<(p_Vid->bitdepth_luma - 1));

      get_mem3Dpel (&(p->imgUV), 1, p->size_y/2, p->size_x/2);

      for (j = 0; j < p->size_y/2; j++)
        for (i = 0; i < p->size_x/2; i++)
          p->imgUV[0][j][i] = cr_val;

      // fake out U=V=128 to make a YUV 4:2:0 stream
      buf = malloc (p->size_x * p->size_y * symbol_size_in_bytes);
      p_Vid->img2buf (p->imgUV[0], buf, p->size_x/2, p->size_y/2, symbol_size_in_bytes,
                      crop_left/2, crop_right/2, crop_top/2, crop_bottom/2, pDecPic->iYBufStride/2);

      free (buf);
      free_mem3Dpel (p->imgUV);

      p->imgUV = NULL;
      }
      /*}}}*/
    }

  //free(buf);
  pDecPic->bValid = 0;
  }
/*}}}*/
/*{{{*/
/*!
 ************************************************************************
 * \brief
 *    Convert image plane to temporary buffer for file writing
 * \param imgX
 *    Pointer to image plane
 * \param buf
 *    Buffer for file output
 * \param size_x
 *    horizontal size
 * \param size_y
 *    vertical size
 * \param symbol_size_in_bytes
 *    number of bytes used per pel
 * \param crop_left
 *    pixels to crop from left
 * \param crop_right
 *    pixels to crop from right
 * \param crop_top
 *    pixels to crop from top
 * \param crop_bottom
 *    pixels to crop from bottom
 ************************************************************************
 */
static void img2buf_byte (imgpel** imgX, unsigned char* buf, int size_x, int size_y, int symbol_size_in_bytes, int crop_left, int crop_right, int crop_top, int crop_bottom, int iOutStride)
{
  int twidth  = size_x - crop_left - crop_right;
  int theight = size_y - crop_top - crop_bottom;
  imgpel **img = &imgX[crop_top];
  int i;
  for(i = 0; i < theight; i++)
  {
    memcpy(buf, *img++ + crop_left, twidth);
    buf += iOutStride;
  }
}
/*}}}*/
/*{{{*/
/*!
 ************************************************************************
 * \brief
 *    Convert image plane to temporary buffer for file writing
 * \param imgX
 *    Pointer to image plane
 * \param buf
 *    Buffer for file output
 * \param size_x
 *    horizontal size
 * \param size_y
 *    vertical size
 * \param symbol_size_in_bytes
 *    number of bytes used per pel
 * \param crop_left
 *    pixels to crop from left
 * \param crop_right
 *    pixels to crop from right
 * \param crop_top
 *    pixels to crop from top
 * \param crop_bottom
 *    pixels to crop from bottom
 ************************************************************************
 */
static void img2buf_normal (imgpel** imgX, unsigned char* buf, int size_x, int size_y, int symbol_size_in_bytes, int crop_left, int crop_right, int crop_top, int crop_bottom, int iOutStride)
{
  int i,j;

  int twidth  = size_x - crop_left - crop_right;
  int theight = size_y - crop_top - crop_bottom;

  int size = 0;

  // sizeof (imgpel) > sizeof(char)
  // little endian
  if (sizeof (imgpel) < symbol_size_in_bytes)
  {
    // this should not happen. we should not have smaller imgpel than our source material.
    size = sizeof (imgpel);
    // clear buffer
    for(j=0; j<theight; j++)
      memset (buf+j*iOutStride, 0, (twidth * symbol_size_in_bytes));
  }
  else
  {
    size = symbol_size_in_bytes;
  }

  if ((crop_top || crop_bottom || crop_left || crop_right) || (size != 1))
  {
    for(i=crop_top; i<size_y-crop_bottom; i++)
    {
      int ipos = (i - crop_top) * iOutStride;
      for(j=crop_left; j<size_x-crop_right; j++)
      {
        memcpy(buf+(ipos+(j-crop_left)*symbol_size_in_bytes),&(imgX[i][j]), size);
      }
    }
  }
  else
  {
#if (IMGTYPE == 0)
    //if (sizeof(imgpel) == sizeof(char))
    {
      //memcpy(buf, &(imgX[0][0]), size_y * size_x * sizeof(imgpel));
      for(j=0; j<size_y; j++)
        memcpy(buf+j*iOutStride, imgX[j], size_x*sizeof(imgpel));
    }
    //else
#else
    {
      imgpel *cur_pixel;
      unsigned char *pDst;
      for(j = 0; j < size_y; j++)
      {
        cur_pixel = imgX[j];
        pDst = buf +j*iOutStride;
        for(i=0; i < size_x; i++)
          *(pDst++)=(unsigned char)*(cur_pixel++);
      }
    }
#endif
  }
}
/*}}}*/
/*{{{*/
void init_output (CodingParameters *p_cps, int symbol_size_in_bytes)
{
  if (( sizeof(char) == sizeof (imgpel))) {
    if ( sizeof(char) == symbol_size_in_bytes)
      p_cps->img2buf = img2buf_byte;
    else
      p_cps->img2buf = img2buf_normal;
    }
  else
    p_cps->img2buf = img2buf_normal;
  }
/*}}}*/

#if (PAIR_FIELDS_IN_OUTPUT)
  void clear_picture (VideoParameters *p_Vid, StorablePicture *p);
  /*{{{*/
  void flush_pending_output (VideoParameters *p_Vid, int p_out) {

    if (p_Vid->pending_output_state != FRAME)
      write_out_picture(p_Vid, p_Vid->pending_output, p_out);

    if (p_Vid->pending_output->imgY) {
      free_mem2Dpel (p_Vid->pending_output->imgY);
      p_Vid->pending_output->imgY=NULL;
      }

    if (p_Vid->pending_output->imgUV) {
      free_mem3Dpel (p_Vid->pending_output->imgUV);
      p_Vid->pending_output->imgUV=NULL;
      }

    p_Vid->pending_output_state = FRAME;
    }
  /*}}}*/
  /*{{{*/
  /*!
   ************************************************************************
   * \brief
   *    Writes out a storable picture
   *    If the picture is a field, the output buffers the picture and tries
   *    to pair it with the next field.
   * \param p
   *    Picture to be written
   * \param p_out
   *    Output file
   ************************************************************************
   */
  void write_picture (VideoParameters *p_Vid, StorablePicture *p, int p_out, int real_structure)
  {
     int i, add;

    if (real_structure==FRAME)
    {

      flush_pending_output(p_Vid, p_out);
      write_out_picture(p_Vid, p, p_out);
      return;
    }
    if (real_structure == p_Vid->pending_output_state)
    {
      flush_pending_output(p_Vid, p_out);
      write_picture(p_Vid, p, p_out, real_structure);
      return;
    }

    if (p_Vid->pending_output_state == FRAME)
    {
      p_Vid->pending_output->size_x = p->size_x;
      p_Vid->pending_output->size_y = p->size_y;
      p_Vid->pending_output->size_x_cr = p->size_x_cr;
      p_Vid->pending_output->size_y_cr = p->size_y_cr;
      p_Vid->pending_output->chroma_format_idc = p->chroma_format_idc;

      p_Vid->pending_output->frame_mbs_only_flag = p->frame_mbs_only_flag;
      p_Vid->pending_output->frame_cropping_flag = p->frame_cropping_flag;
      if (p_Vid->pending_output->frame_cropping_flag)
      {
        p_Vid->pending_output->frame_crop_left_offset = p->frame_crop_left_offset;
        p_Vid->pending_output->frame_crop_right_offset = p->frame_crop_right_offset;
        p_Vid->pending_output->frame_crop_top_offset = p->frame_crop_top_offset;
        p_Vid->pending_output->frame_crop_bottom_offset = p->frame_crop_bottom_offset;
      }

      get_mem2Dpel (&(p_Vid->pending_output->imgY), p_Vid->pending_output->size_y, p_Vid->pending_output->size_x);
      get_mem3Dpel (&(p_Vid->pending_output->imgUV), 2, p_Vid->pending_output->size_y_cr, p_Vid->pending_output->size_x_cr);

      clear_picture(p_Vid, p_Vid->pending_output);

      // copy first field
      if (real_structure == TOP_FIELD)
      {
        add = 0;
      }
      else
      {
        add = 1;
      }

      for (i=0; i<p_Vid->pending_output->size_y; i+=2)
      {
        memcpy(p_Vid->pending_output->imgY[(i+add)], p->imgY[(i+add)], p->size_x * sizeof(imgpel));
      }
      for (i=0; i<p_Vid->pending_output->size_y_cr; i+=2)
      {
        memcpy(p_Vid->pending_output->imgUV[0][(i+add)], p->imgUV[0][(i+add)], p->size_x_cr * sizeof(imgpel));
        memcpy(p_Vid->pending_output->imgUV[1][(i+add)], p->imgUV[1][(i+add)], p->size_x_cr * sizeof(imgpel));
      }
      p_Vid->pending_output_state = real_structure;
    }
    else
    {
      if (  (p_Vid->pending_output->size_x!=p->size_x) || (p_Vid->pending_output->size_y!= p->size_y)
         || (p_Vid->pending_output->frame_mbs_only_flag != p->frame_mbs_only_flag)
         || (p_Vid->pending_output->frame_cropping_flag != p->frame_cropping_flag)
         || ( p_Vid->pending_output->frame_cropping_flag &&
              (  (p_Vid->pending_output->frame_crop_left_offset   != p->frame_crop_left_offset)
               ||(p_Vid->pending_output->frame_crop_right_offset  != p->frame_crop_right_offset)
               ||(p_Vid->pending_output->frame_crop_top_offset    != p->frame_crop_top_offset)
               ||(p_Vid->pending_output->frame_crop_bottom_offset != p->frame_crop_bottom_offset)
              )
            )
         )
      {
        flush_pending_output(p_Vid, p_out);
        write_picture (p_Vid, p, p_out, real_structure);
        return;
      }
      // copy second field
      if (real_structure == TOP_FIELD)
      {
        add = 0;
      }
      else
      {
        add = 1;
      }

      for (i=0; i<p_Vid->pending_output->size_y; i+=2)
      {
        memcpy(p_Vid->pending_output->imgY[(i+add)], p->imgY[(i+add)], p->size_x * sizeof(imgpel));
      }
      for (i=0; i<p_Vid->pending_output->size_y_cr; i+=2)
      {
        memcpy(p_Vid->pending_output->imgUV[0][(i+add)], p->imgUV[0][(i+add)], p->size_x_cr * sizeof(imgpel));
        memcpy(p_Vid->pending_output->imgUV[1][(i+add)], p->imgUV[1][(i+add)], p->size_x_cr * sizeof(imgpel));
      }

      flush_pending_output(p_Vid, p_out);
    }
  }
  /*}}}*/
#else
/*{{{*/
void write_picture (VideoParameters *p_Vid, StorablePicture *p, int p_out, int real_structure) {
  write_out_picture(p_Vid, p, p_out);
  }
/*}}}*/
#endif

/*{{{*/
void init_out_buffer (VideoParameters *p_Vid)
{
  p_Vid->out_buffer = alloc_frame_store();

#if (PAIR_FIELDS_IN_OUTPUT)
  p_Vid->pending_output = calloc (sizeof(StorablePicture), 1);
  if (NULL==p_Vid->pending_output) no_mem_exit("init_out_buffer");
  p_Vid->pending_output->imgUV = NULL;
  p_Vid->pending_output->imgY  = NULL;
#endif
}
/*}}}*/
/*{{{*/
void uninit_out_buffer (VideoParameters *p_Vid)
{
  free_frame_store(p_Vid->out_buffer);
  p_Vid->out_buffer=NULL;

#if (PAIR_FIELDS_IN_OUTPUT)
  flush_pending_output(p_Vid, p_Vid->p_out);
  free (p_Vid->pending_output);
#endif
}
/*}}}*/

/*{{{*/
void clear_picture (VideoParameters *p_Vid, StorablePicture *p)
{
  int i,j;

  for(i=0;i<p->size_y;i++)
    for (j=0; j<p->size_x; j++)
      p->imgY[i][j] = (imgpel) p_Vid->dc_pred_value_comp[0];

  for(i=0;i<p->size_y_cr;i++)
    for (j=0; j<p->size_x_cr; j++)
      p->imgUV[0][i][j] = (imgpel) p_Vid->dc_pred_value_comp[1];

  for(i=0;i<p->size_y_cr;i++)
    for (j=0; j<p->size_x_cr; j++)
      p->imgUV[1][i][j] = (imgpel) p_Vid->dc_pred_value_comp[2];
  }
/*}}}*/
/*{{{*/
void write_unpaired_field (VideoParameters *p_Vid, FrameStore* fs, int p_out)
{
  StorablePicture *p;
  assert (fs->is_used<3);

  if(fs->is_used & 0x01)
  {
    // we have a top field
    // construct an empty bottom field
    p = fs->top_field;
    fs->bottom_field = alloc_storable_picture(p_Vid, BOTTOM_FIELD, p->size_x, 2*p->size_y, p->size_x_cr, 2*p->size_y_cr, 1);
    fs->bottom_field->chroma_format_idc = p->chroma_format_idc;
    clear_picture(p_Vid, fs->bottom_field);
    dpb_combine_field_yuv(p_Vid, fs);
    write_picture (p_Vid, fs->frame, p_out, TOP_FIELD);
  }

  if(fs->is_used & 0x02)
  {
    // we have a bottom field
    // construct an empty top field
    p = fs->bottom_field;
    fs->top_field = alloc_storable_picture(p_Vid, TOP_FIELD, p->size_x, 2*p->size_y, p->size_x_cr, 2*p->size_y_cr, 1);
    fs->top_field->chroma_format_idc = p->chroma_format_idc;
    clear_picture(p_Vid, fs->top_field);
    fs ->top_field->frame_cropping_flag = fs->bottom_field->frame_cropping_flag;
    if(fs ->top_field->frame_cropping_flag)
    {
      fs ->top_field->frame_crop_top_offset = fs->bottom_field->frame_crop_top_offset;
      fs ->top_field->frame_crop_bottom_offset = fs->bottom_field->frame_crop_bottom_offset;
      fs ->top_field->frame_crop_left_offset = fs->bottom_field->frame_crop_left_offset;
      fs ->top_field->frame_crop_right_offset = fs->bottom_field->frame_crop_right_offset;
    }
    dpb_combine_field_yuv(p_Vid, fs);
    write_picture (p_Vid, fs->frame, p_out, BOTTOM_FIELD);
  }

  fs->is_used = 3;
}
/*}}}*/
/*{{{*/
void flush_direct_output (VideoParameters *p_Vid, int p_out)
{
  write_unpaired_field(p_Vid, p_Vid->out_buffer, p_out);

  free_storable_picture(p_Vid->out_buffer->frame);
  p_Vid->out_buffer->frame = NULL;
  free_storable_picture(p_Vid->out_buffer->top_field);
  p_Vid->out_buffer->top_field = NULL;
  free_storable_picture(p_Vid->out_buffer->bottom_field);
  p_Vid->out_buffer->bottom_field = NULL;
  p_Vid->out_buffer->is_used = 0;
}
/*}}}*/
/*{{{*/
void write_stored_frame (VideoParameters *p_Vid, FrameStore *fs, int p_out)
{
  // make sure no direct output field is pending
  flush_direct_output(p_Vid, p_out);

  if (fs->is_used<3)
    write_unpaired_field(p_Vid, fs, p_out);
  else
  {
    if (fs->recovery_frame)
      p_Vid->recovery_flag = 1;
    if ((!p_Vid->non_conforming_stream) || p_Vid->recovery_flag)
      write_picture(p_Vid, fs->frame, p_out, FRAME);
  }

  fs->is_output = 1;
}
/*}}}*/

/*{{{*/
void direct_output (VideoParameters *p_Vid, StorablePicture *p, int p_out)
{
  InputParameters *p_Inp = p_Vid->p_Inp;
  if (p->structure==FRAME) {
    /*{{{*/
    // we have a frame (or complementary field pair)
    // so output it directly
    flush_direct_output(p_Vid, p_out);
    write_picture (p_Vid, p, p_out, FRAME);
    calculate_frame_no(p_Vid, p);
    free_storable_picture(p);
    return;
    }
    /*}}}*/

  if (p->structure == TOP_FIELD) {
    /*{{{*/
    if (p_Vid->out_buffer->is_used &1)
      flush_direct_output(p_Vid, p_out);
    p_Vid->out_buffer->top_field = p;
    p_Vid->out_buffer->is_used |= 1;
    }
    /*}}}*/

  if (p->structure == BOTTOM_FIELD) {
    /*{{{*/
    if (p_Vid->out_buffer->is_used &2)
      flush_direct_output(p_Vid, p_out);
    p_Vid->out_buffer->bottom_field = p;
    p_Vid->out_buffer->is_used |= 2;
    }
    /*}}}*/

  if (p_Vid->out_buffer->is_used == 3) {
    // we have both fields, so output them
    dpb_combine_field_yuv(p_Vid, p_Vid->out_buffer);
    write_picture (p_Vid, p_Vid->out_buffer->frame, p_out, FRAME);

    calculate_frame_no(p_Vid, p);
    free_storable_picture(p_Vid->out_buffer->frame);
    p_Vid->out_buffer->frame = NULL;
    free_storable_picture(p_Vid->out_buffer->top_field);
    p_Vid->out_buffer->top_field = NULL;
    free_storable_picture(p_Vid->out_buffer->bottom_field);
    p_Vid->out_buffer->bottom_field = NULL;
    p_Vid->out_buffer->is_used = 0;
  }
}
/*}}}*/
