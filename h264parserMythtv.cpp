    1 // MythTV headers
    2 #include "H264Parser.h"
    3 #include <iostream>
    4 #include "mythlogging.h"
    5 #include "recorders/dtvrecorder.h" // for FrameRate
    6 
    7 extern "C" {
    8 #include "libavcodec/avcodec.h"
    9 #include "libavcodec/mpegvideo.h"
   10 #include "libavcodec/golomb.h"
   11 }
   12 
   13 #include <cmath>
   14 #include <strings.h>
   15 
   16 static const float eps = 1E-5;
   17 
   18 /*
   19   Most of the comments below were cut&paste from ITU-T Rec. H.264
   20   as found here:  http://www.itu.int/rec/T-REC-H.264/e
   21  */
   22 
   23 /*
   24   Useful definitions:
   25 
   26   * access unit: A set of NAL units always containing exactly one
   27   primary coded picture. In addition to the primary coded picture, an
   28   access unit may also contain one or more redundant coded pictures
   29   or other NAL units not containing slices or slice data partitions
   30   of a coded picture. The decoding of an access unit always results
   31   in a decoded picture.
   32 
   33   * instantaneous decoding refresh (IDR) access unit: An access unit in
   34   which the primary coded picture is an IDR picture.
   35 
   36   * instantaneous decoding refresh (IDR) picture: A coded picture
   37   containing only slices with I or SI slice types that causes the
   38   decoding process to mark all reference pictures as "unused for
   39   reference" immediately after decoding the IDR picture. After the
   40   decoding of an IDR picture all following coded pictures in decoding
   41   order can be decoded without inter prediction from any picture
   42   decoded prior to the IDR picture. The first picture of each coded
   43   video sequence is an IDR picture.
   44 
   45   * NAL unit: A syntax structure containing an indication of the type
   46   of data to follow and bytes containing that data in the form of an
   47   RBSP interspersed as necessary with emulation prevention bytes.
   48 
   49   * raw byte sequence payload (RBSP): A syntax structure containing an
   50   integer number of bytes that is encapsulated in a NAL unit. An RBSP
   51   is either empty or has the form of a string of data bits containing
   52   syntax elements followed by an RBSP stop bit and followed by zero
   53   or more subsequent bits equal to 0.
   54 
   55   * raw byte sequence payload (RBSP) stop bit: A bit equal to 1 present
   56   within a raw byte sequence payload (RBSP) after a string of data
   57   bits. The location of the end of the string of data bits within an
   58   RBSP can be identified by searching from the end of the RBSP for
   59   the RBSP stop bit, which is the last non-zero bit in the RBSP.
   60 
   61   * parity: The parity of a field can be top or bottom.
   62 
   63   * picture: A collective term for a field or a frame.
   64 
   65   * picture parameter set: A syntax structure containing syntax
   66   elements that apply to zero or more entire coded pictures as
   67   determined by the pic_parameter_set_id syntax element found in each
   68   slice header.
   69 
   70   * primary coded picture: The coded representation of a picture to be
   71   used by the decoding process for a bitstream conforming to this
   72   Recommendation | International Standard. The primary coded picture
   73   contains all macroblocks of the picture. The only pictures that
   74   have a normative effect on the decoding process are primary coded
   75   pictures. See also redundant coded picture.
   76 
   77   * VCL: Video Coding Layer
   78 
   79   - The VCL is specified to efficiently represent the content of the
   80   video data. The NAL is specified to format that data and provide
   81   header information in a manner appropriate for conveyance on a
   82   variety of communication channels or storage media. All data are
   83   contained in NAL units, each of which contains an integer number of
   84   bytes. A NAL unit specifies a generic format for use in both
   85   packet-oriented and bitstream systems. The format of NAL units for
   86   both packet-oriented transport and byte stream is identical except
   87   that each NAL unit can be preceded by a start code prefix and extra
   88   padding bytes in the byte stream format.
   89 
   90 */
   91 
   92 H264Parser::H264Parser(void)
   93 {
   94     rbsp_buffer_size = 188 * 2;
   95     rbsp_buffer = new uint8_t[rbsp_buffer_size];
   96     if (rbsp_buffer == 0)
   97         rbsp_buffer_size = 0;
   98 
   99     Reset();
  100     I_is_keyframe = true;
  101     au_contains_keyframe_message = false;
  102 }
  103 
  104 void H264Parser::Reset(void)
  105 {
  106     state_changed = false;
  107     seen_sps = false;
  108     is_keyframe = false;
  109     SPS_offset = 0;
  110 
  111     sync_accumulator = 0xffffffff;
  112     AU_pending = false;
  113 
  114     frame_num = prev_frame_num = -1;
  115     slice_type = SLICE_UNDEF;
  116     prev_pic_parameter_set_id = pic_parameter_set_id = -1;
  117     prev_field_pic_flag = field_pic_flag = -1;
  118     prev_bottom_field_flag = bottom_field_flag = -1;
  119     prev_nal_ref_idc = nal_ref_idc = 111;  //  != [0|1|2|3]
  120     prev_pic_order_cnt_type = pic_order_cnt_type =
  121     prev_pic_order_cnt_lsb = pic_order_cnt_lsb = 0;
  122     prev_delta_pic_order_cnt_bottom = delta_pic_order_cnt_bottom = 0;
  123     prev_delta_pic_order_cnt[0] = delta_pic_order_cnt[0] = 0;
  124     prev_delta_pic_order_cnt[1] = delta_pic_order_cnt[1] = 0;
  125     prev_nal_unit_type = nal_unit_type = UNKNOWN;
  126 
  127     // The value of idr_pic_id shall be in the range of 0 to 65535, inclusive.
  128     prev_idr_pic_id = idr_pic_id = 65536;
  129 
  130     log2_max_frame_num = log2_max_pic_order_cnt_lsb = 0;
  131     seq_parameter_set_id = 0;
  132 
  133     delta_pic_order_always_zero_flag = 0;
  134     separate_colour_plane_flag = 0;
  135     chroma_format_idc = 1;
  136     frame_mbs_only_flag = -1;
  137     pic_order_present_flag = -1;
  138     redundant_pic_cnt_present_flag = 0;
  139 
  140     num_ref_frames = 0;
  141     redundant_pic_cnt = 0;
  142 
  143 //    pic_width_in_mbs = pic_height_in_map_units = 0;
  144     pic_width = pic_height = 0;
  145     frame_crop_left_offset = frame_crop_right_offset = 0;
  146     frame_crop_top_offset = frame_crop_bottom_offset = 0;
  147     aspect_ratio_idc = 0;
  148     sar_width = sar_height = 0;
  149     unitsInTick = 0;
  150     timeScale = 0;
  151     fixedRate = 0;
  152 
  153     pkt_offset = AU_offset = frame_start_offset = keyframe_start_offset = 0;
  154     on_frame = on_key_frame = false;
  155 
  156     resetRBSP();
  157 }
  158 
  159 
  160 QString H264Parser::NAL_type_str(uint8_t type)
  161 {
  162     switch (type)
  163     {
  164       case UNKNOWN:
  165         return "UNKNOWN";
  166       case SLICE:
  167         return "SLICE";
  168       case SLICE_DPA:
  169         return "SLICE_DPA";
  170       case SLICE_DPB:
  171         return "SLICE_DPB";
  172       case SLICE_DPC:
  173         return "SLICE_DPC";
  174       case SLICE_IDR:
  175         return "SLICE_IDR";
  176       case SEI:
  177         return "SEI";
  178       case SPS:
  179         return "SPS";
  180       case PPS:
  181         return "PPS";
  182       case AU_DELIMITER:
  183         return "AU_DELIMITER";
  184       case END_SEQUENCE:
  185         return "END_SEQUENCE";
  186       case END_STREAM:
  187         return "END_STREAM";
  188       case FILLER_DATA:
  189         return "FILLER_DATA";
  190       case SPS_EXT:
  191         return "SPS_EXT";
  192     }
  193     return "OTHER";
  194 }
  195 
  196 bool H264Parser::new_AU(void)
  197 {
  198     /*
  199       An access unit consists of one primary coded picture, zero or more
  200       corresponding redundant coded pictures, and zero or more non-VCL NAL
  201       units. The association of VCL NAL units to primary or redundant coded
  202       pictures is described in subclause 7.4.1.2.5.
  203 
  204       The first access unit in the bitstream starts with the first NAL unit
  205       of the bitstream.
  206 
  207       The first of any of the following NAL units after the last VCL NAL
  208       unit of a primary coded picture specifies the start of a new access
  209       unit.
  210 
  211       –    access unit delimiter NAL unit (when present)
  212       –    sequence parameter set NAL unit (when present)
  213       –    picture parameter set NAL unit (when present)
  214       –    SEI NAL unit (when present)
  215       –    NAL units with nal_unit_type in the range of 14 to 18, inclusive
  216       –    first VCL NAL unit of a primary coded picture (always present)
  217     */
  218 
  219     /*
  220       7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded
  221       picture This subclause specifies constraints on VCL NAL unit syntax
  222       that are sufficient to enable the detection of the first VCL NAL unit
  223       of each primary coded picture.
  224 
  225       Any coded slice NAL unit or coded slice data partition A NAL unit of
  226       the primary coded picture of the current access unit shall be
  227       different from any coded slice NAL unit or coded slice data partition
  228       A NAL unit of the primary coded picture of the previous access unit in
  229       one or more of the following ways.
  230 
  231       - frame_num differs in value. The value of frame_num used to
  232         test this condition is the value of frame_num that appears in
  233         the syntax of the slice header, regardless of whether that value
  234         is inferred to have been equal to 0 for subsequent use in the
  235         decoding process due to the presence of
  236         memory_management_control_operation equal to 5.
  237           Note: If the current picture is an IDR picture FrameNum and
  238           PrevRefFrameNum are set equal to 0.
  239       - pic_parameter_set_id differs in value.
  240       - field_pic_flag differs in value.
  241       - bottom_field_flag is present in both and differs in value.
  242       - nal_ref_idc differs in value with one of the nal_ref_idc
  243         values being equal to 0.
  244       - pic_order_cnt_type is equal to 0 for both and either
  245         pic_order_cnt_lsb differs in value, or delta_pic_order_cnt_bottom
  246         differs in value.
  247       - pic_order_cnt_type is equal to 1 for both and either
  248         delta_pic_order_cnt[0] differs in value, or
  249         delta_pic_order_cnt[1] differs in value.
  250       - nal_unit_type differs in value with one of the nal_unit_type values
  251         being equal to 5.
  252       - nal_unit_type is equal to 5 for both and idr_pic_id differs in
  253         value.
  254 
  255       NOTE – Some of the VCL NAL units in redundant coded pictures or some
  256       non-VCL NAL units (e.g. an access unit delimiter NAL unit) may also
  257       be used for the detection of the boundary between access units, and
  258       may therefore aid in the detection of the start of a new primary
  259       coded picture.
  260 
  261     */
  262 
  263     bool       result = false;
  264 
  265     if (prev_frame_num != -1)
  266     {
  267         // Need previous slice information for comparison
  268 
  269         if (nal_unit_type != SLICE_IDR && frame_num != prev_frame_num)
  270             result = true;
  271         else if (prev_pic_parameter_set_id != -1 &&
  272                  pic_parameter_set_id != prev_pic_parameter_set_id)
  273             result = true;
  274         else if (field_pic_flag != prev_field_pic_flag)
  275             result = true;
  276         else if ((bottom_field_flag != -1 && prev_bottom_field_flag != -1) &&
  277                  bottom_field_flag != prev_bottom_field_flag)
  278             result = true;
  279         else if ((nal_ref_idc == 0 || prev_nal_ref_idc == 0) &&
  280                  nal_ref_idc != prev_nal_ref_idc)
  281             result = true;
  282         else if ((pic_order_cnt_type == 0 && prev_pic_order_cnt_type == 0) &&
  283                  (pic_order_cnt_lsb != prev_pic_order_cnt_lsb ||
  284                   delta_pic_order_cnt_bottom !=
  285                   prev_delta_pic_order_cnt_bottom))
  286             result = true;
  287         else if ((pic_order_cnt_type == 1 && prev_pic_order_cnt_type == 1) &&
  288                  (delta_pic_order_cnt[0] != prev_delta_pic_order_cnt[0] ||
  289                   delta_pic_order_cnt[1] != prev_delta_pic_order_cnt[1]))
  290             result = true;
  291         else if ((nal_unit_type == SLICE_IDR ||
  292                   prev_nal_unit_type == SLICE_IDR) &&
  293                  nal_unit_type != prev_nal_unit_type)
  294             result = true;
  295         else if ((nal_unit_type == SLICE_IDR &&
  296                   prev_nal_unit_type == SLICE_IDR) &&
  297                  idr_pic_id != prev_idr_pic_id)
  298             result = true;
  299     }
  300 
  301     prev_frame_num = frame_num;
  302     prev_pic_parameter_set_id = pic_parameter_set_id;
  303     prev_field_pic_flag = field_pic_flag;
  304     prev_bottom_field_flag = bottom_field_flag;
  305     prev_nal_ref_idc = nal_ref_idc;
  306     prev_pic_order_cnt_lsb = pic_order_cnt_lsb;
  307     prev_delta_pic_order_cnt_bottom = delta_pic_order_cnt_bottom;
  308     prev_delta_pic_order_cnt[0] = delta_pic_order_cnt[0];
  309     prev_delta_pic_order_cnt[1] = delta_pic_order_cnt[1];
  310     prev_nal_unit_type = nal_unit_type;
  311     prev_idr_pic_id = idr_pic_id;
  312 
  313     return result;
  314 }
  315 
  316 void H264Parser::resetRBSP(void)
  317 {
  318     rbsp_index = 0;
  319     consecutive_zeros = 0;
  320     have_unfinished_NAL = false;
  321 }
  322 
  323 bool H264Parser::fillRBSP(const uint8_t *byteP, uint32_t byte_count,
  324                           bool found_start_code)
  325 {
  326     /*
  327       bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE
  328       bytes larger then the actual data
  329     */
  330     uint32_t required_size = rbsp_index + byte_count +
  331                              FF_INPUT_BUFFER_PADDING_SIZE;
  332     if (rbsp_buffer_size < required_size)
  333     {
  334         // Round up to packet size
  335         required_size = ((required_size / 188) + 1) * 188;
  336 
  337         /* Need a bigger buffer */
  338         uint8_t *new_buffer = new uint8_t[required_size];
  339 
  340         if (new_buffer == NULL)
  341         {
  342             /* Allocation failed. Discard the new bytes */
  343             LOG(VB_GENERAL, LOG_ERR,
  344                 "H264Parser::fillRBSP: FAILED to allocate RBSP buffer!");
  345             return false;
  346         }
  347 
  348         /* Copy across bytes from old buffer */
  349         memcpy(new_buffer, rbsp_buffer, rbsp_index);
  350         delete [] rbsp_buffer;
  351         rbsp_buffer = new_buffer;
  352         rbsp_buffer_size = required_size;
  353     }
  354 
  355     /* Fill rbsp while we have data */
  356     while (byte_count)
  357     {
  358         /* Copy the byte into the rbsp, unless it
  359          * is the 0x03 in a 0x000003 */
  360         if (consecutive_zeros < 2 || *byteP != 0x03)
  361             rbsp_buffer[rbsp_index++] = *byteP;
  362 
  363         if (*byteP == 0)
  364             ++consecutive_zeros;
  365         else
  366             consecutive_zeros = 0;
  367 
  368         ++byteP;
  369         --byte_count;
  370     }
  371 
  372     /* If we've found the next start code then that, plus the first byte of
  373      * the next NAL, plus the preceding zero bytes will all be in the rbsp
  374      * buffer. Move rbsp_index++ back to the end of the actual rbsp data. We
  375      * need to know the correct size of the rbsp to decode some NALs.
  376      */
  377     if (found_start_code)
  378     {
  379         if (rbsp_index >= 4)
  380         {
  381             rbsp_index -= 4;
  382             while (rbsp_index > 0 && rbsp_buffer[rbsp_index-1] == 0)
  383                 --rbsp_index;
  384         }
  385         else
  386         {
  387             /* This should never happen. */
  388             LOG(VB_GENERAL, LOG_ERR,
  389                 QString("H264Parser::fillRBSP: Found start code, rbsp_index "
  390                         "is %1 but it should be >4")
  391                     .arg(rbsp_index));
  392         }
  393     }
  394 
  395     /* Stick some 0xff on the end for get_bits to run into */
  396     memset(&rbsp_buffer[rbsp_index], 0xff, FF_INPUT_BUFFER_PADDING_SIZE);
  397     return true;
  398 }
  399 
  400 uint32_t H264Parser::addBytes(const uint8_t  *bytes,
  401                               const uint32_t  byte_count,
  402                               const uint64_t  stream_offset)
  403 {
  404     const uint8_t *startP = bytes;
  405     const uint8_t *endP;
  406     bool           found_start_code;
  407     bool           good_nal_unit;
  408 
  409     state_changed = false;
  410     on_frame      = false;
  411     on_key_frame  = false;
  412 
  413     while (startP < bytes + byte_count && !on_frame)
  414     {
  415         endP = avpriv_find_start_code(startP,
  416                                   bytes + byte_count, &sync_accumulator);
  417 
  418         found_start_code = ((sync_accumulator & 0xffffff00) == 0x00000100);
  419 
  420         /* Between startP and endP we potentially have some more
  421          * bytes of a NAL that we've been parsing (plus some bytes of
  422          * start code)
  423          */
  424         if (have_unfinished_NAL)
  425         {
  426             if (!fillRBSP(startP, endP - startP, found_start_code))
  427             {
  428                 resetRBSP();
  429                 return endP - bytes;
  430             }
  431             processRBSP(found_start_code); /* Call may set have_uinfinished_NAL
  432                                             * to false */
  433         }
  434 
  435         /* Dealt with everything up to endP */
  436         startP = endP;
  437 
  438         if (found_start_code)
  439         {
  440             if (have_unfinished_NAL)
  441             {
  442                 /* We've found a new start code, without completely
  443                  * parsing the previous NAL. Either there's a
  444                  * problem with the stream or with this parser.
  445                  */
  446                 LOG(VB_GENERAL, LOG_ERR,
  447                     "H264Parser::addBytes: Found new start "
  448                     "code, but previous NAL is incomplete!");
  449             }
  450 
  451             /* Prepare for accepting the new NAL */
  452             resetRBSP();
  453 
  454             /* If we find the start of an AU somewhere from here
  455              * to the next start code, the offset to associate with
  456              * it is the one passed in to this call, not any of the
  457              * subsequent calls.
  458              */
  459             pkt_offset = stream_offset; // + (startP - bytes);
  460 
  461 /*
  462   nal_unit_type specifies the type of RBSP data structure contained in
  463   the NAL unit as specified in Table 7-1. VCL NAL units
  464   are specified as those NAL units having nal_unit_type
  465   equal to 1 to 5, inclusive. All remaining NAL units
  466   are called non-VCL NAL units:
  467 
  468   0  Unspecified
  469   1  Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
  470   2  Coded slice data partition A slice_data_partition_a_layer_rbsp( )
  471   3  Coded slice data partition B slice_data_partition_b_layer_rbsp( )
  472   4  Coded slice data partition C slice_data_partition_c_layer_rbsp( )
  473   5  Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
  474   6  Supplemental enhancement information (SEI) 5 sei_rbsp( )
  475   7  Sequence parameter set (SPS) seq_parameter_set_rbsp( )
  476   8  Picture parameter set pic_parameter_set_rbsp( )
  477   9  Access unit delimiter access_unit_delimiter_rbsp( )
  478   10 End of sequence end_of_seq_rbsp( )
  479   11 End of stream end_of_stream_rbsp( )
  480 */
  481             nal_unit_type = sync_accumulator & 0x1f;
  482             nal_ref_idc = (sync_accumulator >> 5) & 0x3;
  483 
  484             good_nal_unit = true;
  485             if (nal_ref_idc)
  486             {
  487                 /* nal_ref_idc shall be equal to 0 for all NAL units having
  488                  * nal_unit_type equal to 6, 9, 10, 11, or 12.
  489                  */
  490                 if (nal_unit_type == SEI ||
  491                     (nal_unit_type >= AU_DELIMITER &&
  492                      nal_unit_type <= FILLER_DATA))
  493                     good_nal_unit = false;
  494             }
  495             else
  496             {
  497                 /* nal_ref_idc shall not be equal to 0 for NAL units with
  498                  * nal_unit_type equal to 5
  499                  */
  500                 if (nal_unit_type == SLICE_IDR)
  501                     good_nal_unit = false;
  502             }
  503 
  504             if (good_nal_unit)
  505             {
  506                 if (nal_unit_type == SPS || nal_unit_type == PPS ||
  507                     nal_unit_type == SEI || NALisSlice(nal_unit_type))
  508                 {
  509                     /* This is a NAL we need to parse. We may have the body
  510                      * of it in the part of the stream past to us this call,
  511                      * or we may get the rest in subsequent calls to addBytes.
  512                      * Either way, we set have_unfinished_NAL, so that we
  513                      * start filling the rbsp buffer
  514                      */
  515                       have_unfinished_NAL = true;
  516                 }
  517                 else if (nal_unit_type == AU_DELIMITER ||
  518                         (nal_unit_type > SPS_EXT &&
  519                          nal_unit_type < AUXILIARY_SLICE))
  520                 {
  521                     set_AU_pending();
  522                 }
  523             }
  524             else
  525                 LOG(VB_GENERAL, LOG_ERR,
  526                     "H264Parser::addbytes: malformed NAL units");
  527         } //found start code
  528     }
  529 
  530     return startP - bytes;
  531 }
  532 
  533 
  534 void H264Parser::processRBSP(bool rbsp_complete)
  535 {
  536     GetBitContext gb;
  537 
  538     init_get_bits(&gb, rbsp_buffer, 8 * rbsp_index);
  539 
  540     if (nal_unit_type == SEI)
  541     {
  542         /* SEI cannot be parsed without knowing its size. If
  543          * we haven't got the whole rbsp, return and wait for
  544          * the rest
  545          */
  546         if (!rbsp_complete)
  547             return;
  548 
  549         set_AU_pending();
  550 
  551         decode_SEI(&gb);
  552     }
  553     else if (nal_unit_type == SPS)
  554     {
  555         /* Best wait until we have the whole thing */
  556         if (!rbsp_complete)
  557             return;
  558 
  559         set_AU_pending();
  560 
  561         if (!seen_sps)
  562             SPS_offset = pkt_offset;
  563 
  564         decode_SPS(&gb);
  565     }
  566     else if (nal_unit_type == PPS)
  567     {
  568         /* Best wait until we have the whole thing */
  569         if (!rbsp_complete)
  570             return;
  571 
  572         set_AU_pending();
  573 
  574         decode_PPS(&gb);
  575     }
  576     else
  577     {
  578         /* Need only parse the header. So return only
  579          * if we have insufficient bytes */
  580         if (!rbsp_complete && rbsp_index < MAX_SLICE_HEADER_SIZE)
  581             return;
  582 
  583         decode_Header(&gb);
  584 
  585         if (new_AU())
  586             set_AU_pending();
  587     }
  588 
  589     /* If we got this far, we managed to parse a sufficient
  590      * prefix of the current NAL. We can go onto the next. */
  591     have_unfinished_NAL = false;
  592 
  593     if (AU_pending && NALisSlice(nal_unit_type))
  594     {
  595         /* Once we know the slice type of a new AU, we can
  596          * determine if it is a keyframe or just a frame
  597          */
  598         AU_pending = false;
  599         state_changed = seen_sps;
  600 
  601         on_frame = true;
  602         frame_start_offset = AU_offset;
  603 
  604         if (is_keyframe || au_contains_keyframe_message)
  605         {
  606             on_key_frame = true;
  607             keyframe_start_offset = AU_offset;
  608         }
  609     }
  610 }
  611 
  612 /*
  613   7.4.3 Slice header semantics
  614 */
  615 bool H264Parser::decode_Header(GetBitContext *gb)
  616 {
  617     is_keyframe = false;
  618 
  619     if (log2_max_frame_num == 0)
  620     {
  621         /* SPS has not been parsed yet */
  622         return false;
  623     }
  624 
  625     /*
  626       first_mb_in_slice specifies the address of the first macroblock
  627       in the slice. When arbitrary slice order is not allowed as
  628       specified in Annex A, the value of first_mb_in_slice is
  629       constrained as follows.
  630 
  631       – If separate_colour_plane_flag is equal to 0, the value of
  632       first_mb_in_slice shall not be less than the value of
  633       first_mb_in_slice for any other slice of the current picture
  634       that precedes the current slice in decoding order.
  635 
  636       – Otherwise (separate_colour_plane_flag is equal to 1), the value of
  637       first_mb_in_slice shall not be less than the value of
  638       first_mb_in_slice for any other slice of the current picture
  639       that precedes the current slice in decoding order and has the
  640       same value of colour_plane_id.
  641      */
  642     /* uint first_mb_in_slice = */ get_ue_golomb(gb);
  643 
  644     /*
  645       slice_type specifies the coding type of the slice according to
  646       Table 7-6.   e.g. P, B, I, SP, SI
  647 
  648       When nal_unit_type is equal to 5 (IDR picture), slice_type shall
  649       be equal to 2, 4, 7, or 9 (I or SI)
  650      */
  651     slice_type = get_ue_golomb_31(gb);
  652 
  653     /* s->pict_type = golomb_to_pict_type[slice_type % 5];
  654      */
  655 
  656     /*
  657       pic_parameter_set_id specifies the picture parameter set in
  658       use. The value of pic_parameter_set_id shall be in the range of
  659       0 to 255, inclusive.
  660      */
  661     pic_parameter_set_id = get_ue_golomb(gb);
  662 
  663     /*
  664       separate_colour_plane_flag equal to 1 specifies that the three
  665       colour components of the 4:4:4 chroma format are coded
  666       separately. separate_colour_plane_flag equal to 0 specifies that
  667       the colour components are not coded separately.  When
  668       separate_colour_plane_flag is not present, it shall be inferred
  669       to be equal to 0. When separate_colour_plane_flag is equal to 1,
  670       the primary coded picture consists of three separate components,
  671       each of which consists of coded samples of one colour plane (Y,
  672       Cb or Cr) that each use the monochrome coding syntax. In this
  673       case, each colour plane is associated with a specific
  674       colour_plane_id value.
  675      */
  676     if (separate_colour_plane_flag)
  677         get_bits(gb, 2);  // colour_plane_id
  678 
  679     /*
  680       frame_num is used as an identifier for pictures and shall be
  681       represented by log2_max_frame_num_minus4 + 4 bits in the
  682       bitstream....
  683 
  684       If the current picture is an IDR picture, frame_num shall be equal to 0.
  685 
  686       When max_num_ref_frames is equal to 0, slice_type shall be equal
  687           to 2, 4, 7, or 9.
  688     */
  689     frame_num = get_bits(gb, log2_max_frame_num);
  690 
  691     /*
  692       field_pic_flag equal to 1 specifies that the slice is a slice of a
  693       coded field. field_pic_flag equal to 0 specifies that the slice is a
  694       slice of a coded frame. When field_pic_flag is not present it shall be
  695       inferred to be equal to 0.
  696 
  697       bottom_field_flag equal to 1 specifies that the slice is part of a
  698       coded bottom field. bottom_field_flag equal to 0 specifies that the
  699       picture is a coded top field. When this syntax element is not present
  700       for the current slice, it shall be inferred to be equal to 0.
  701     */
  702     if (!frame_mbs_only_flag)
  703     {
  704         field_pic_flag = get_bits1(gb);
  705         bottom_field_flag = field_pic_flag ? get_bits1(gb) : 0;
  706     }
  707     else
  708     {
  709         field_pic_flag = 0;
  710         bottom_field_flag = -1;
  711     }
  712 
  713     /*
  714       idr_pic_id identifies an IDR picture. The values of idr_pic_id
  715       in all the slices of an IDR picture shall remain unchanged. When
  716       two consecutive access units in decoding order are both IDR
  717       access units, the value of idr_pic_id in the slices of the first
  718       such IDR access unit shall differ from the idr_pic_id in the
  719       second such IDR access unit. The value of idr_pic_id shall be in
  720       the range of 0 to 65535, inclusive.
  721      */
  722     if (nal_unit_type == SLICE_IDR)
  723     {
  724         idr_pic_id = get_ue_golomb(gb);
  725         is_keyframe = true;
  726     }
  727     else
  728         is_keyframe = (I_is_keyframe && isKeySlice(slice_type));
  729 
  730     /*
  731       pic_order_cnt_lsb specifies the picture order count modulo
  732       MaxPicOrderCntLsb for the top field of a coded frame or for a coded
  733       field. The size of the pic_order_cnt_lsb syntax element is
  734       log2_max_pic_order_cnt_lsb_minus4 + 4 bits. The value of the
  735       pic_order_cnt_lsb shall be in the range of 0 to MaxPicOrderCntLsb – 1,
  736       inclusive.
  737 
  738       delta_pic_order_cnt_bottom specifies the picture order count
  739       difference between the bottom field and the top field of a coded
  740       frame.
  741     */
  742     if (pic_order_cnt_type == 0)
  743     {
  744         pic_order_cnt_lsb = get_bits(gb, log2_max_pic_order_cnt_lsb);
  745 
  746         if ((pic_order_present_flag == 1) && !field_pic_flag)
  747             delta_pic_order_cnt_bottom = get_se_golomb(gb);
  748         else
  749             delta_pic_order_cnt_bottom = 0;
  750     }
  751     else
  752         delta_pic_order_cnt_bottom = 0;
  753 
  754     /*
  755       delta_pic_order_always_zero_flag equal to 1 specifies that
  756       delta_pic_order_cnt[ 0 ] and delta_pic_order_cnt[ 1 ] are not
  757       present in the slice headers of the sequence and shall be
  758       inferred to be equal to 0. delta_pic_order_always_zero_flag
  759       equal to 0 specifies that delta_pic_order_cnt[ 0 ] is present in
  760       the slice headers of the sequence and delta_pic_order_cnt[ 1 ]
  761       may be present in the slice headers of the sequence.
  762     */
  763     if (delta_pic_order_always_zero_flag)
  764     {
  765         delta_pic_order_cnt[1] = delta_pic_order_cnt[0] = 0;
  766     }
  767     else if (pic_order_cnt_type == 1)
  768     {
  769         /*
  770           delta_pic_order_cnt[ 0 ] specifies the picture order count
  771           difference from the expected picture order count for the top
  772           field of a coded frame or for a coded field as specified in
  773           subclause 8.2.1. The value of delta_pic_order_cnt[ 0 ] shall
  774           be in the range of -2^31 to 2^31 - 1, inclusive. When this
  775           syntax element is not present in the bitstream for the
  776           current slice, it shall be inferred to be equal to 0.
  777 
  778           delta_pic_order_cnt[ 1 ] specifies the picture order count
  779           difference from the expected picture order count for the
  780           bottom field of a coded frame specified in subclause
  781           8.2.1. The value of delta_pic_order_cnt[ 1 ] shall be in the
  782           range of -2^31 to 2^31 - 1, inclusive. When this syntax
  783           element is not present in the bitstream for the current
  784           slice, it shall be inferred to be equal to 0.
  785         */
  786         delta_pic_order_cnt[0] = get_se_golomb(gb);
  787 
  788         if ((pic_order_present_flag == 1) && !field_pic_flag)
  789             delta_pic_order_cnt[1] = get_se_golomb(gb);
  790         else
  791             delta_pic_order_cnt[1] = 0;
  792      }
  793 
  794     /*
  795       redundant_pic_cnt shall be equal to 0 for slices and slice data
  796       partitions belonging to the primary coded picture. The
  797       redundant_pic_cnt shall be greater than 0 for coded slices and
  798       coded slice data partitions in redundant coded pictures.  When
  799       redundant_pic_cnt is not present, its value shall be inferred to
  800       be equal to 0. The value of redundant_pic_cnt shall be in the
  801       range of 0 to 127, inclusive.
  802     */
  803     redundant_pic_cnt = redundant_pic_cnt_present_flag ? get_ue_golomb(gb) : 0;
  804 
  805     return true;
  806 }
  807 
  808 /*
  809  * libavcodec used for example
  810  */
  811 void H264Parser::decode_SPS(GetBitContext * gb)
  812 {
  813     int profile_idc;
  814     int lastScale;
  815     int nextScale;
  816     int deltaScale;
  817 
  818     seen_sps = true;
  819 
  820     profile_idc = get_bits(gb, 8);
  821     get_bits1(gb);      // constraint_set0_flag
  822     get_bits1(gb);      // constraint_set1_flag
  823     get_bits1(gb);      // constraint_set2_flag
  824     get_bits1(gb);      // constraint_set3_flag
  825     get_bits(gb, 4);    // reserved
  826     get_bits(gb, 8);    // level_idc
  827     get_ue_golomb(gb);  // sps_id
  828 
  829     if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
  830         profile_idc == 244 || profile_idc == 44  || profile_idc == 83  ||
  831         profile_idc == 86  || profile_idc == 118 || profile_idc == 128 )
  832     { // high profile
  833         if ((chroma_format_idc = get_ue_golomb(gb)) == 3)
  834             separate_colour_plane_flag = (get_bits1(gb) == 1);
  835 
  836         get_ue_golomb(gb);     // bit_depth_luma_minus8
  837         get_ue_golomb(gb);     // bit_depth_chroma_minus8
  838         get_bits1(gb);         // qpprime_y_zero_transform_bypass_flag
  839 
  840         if (get_bits1(gb))     // seq_scaling_matrix_present_flag
  841         {
  842             for (int idx = 0; idx < ((chroma_format_idc != 3) ? 8 : 12); ++idx)
  843             {
  844                 if (get_bits1(gb)) // Scaling list present
  845                 {
  846                     lastScale = nextScale = 8;
  847                     int sl_n = ((idx < 6) ? 16 : 64);
  848                     for(int sl_i = 0; sl_i < sl_n; ++sl_i)
  849                     {
  850                         if (nextScale != 0)
  851                         {
  852                             deltaScale = get_se_golomb(gb);
  853                             nextScale = (lastScale + deltaScale + 256) % 256;
  854                         }
  855                         lastScale = (nextScale == 0) ? lastScale : nextScale;
  856                     }
  857                 }
  858             }
  859         }
  860     }
  861 
  862     /*
  863       log2_max_frame_num_minus4 specifies the value of the variable
  864       MaxFrameNum that is used in frame_num related derivations as
  865       follows:
  866 
  867        MaxFrameNum = 2( log2_max_frame_num_minus4 + 4 )
  868      */
  869     log2_max_frame_num = get_ue_golomb(gb) + 4;
  870 
  871     int  offset_for_non_ref_pic;
  872     int  offset_for_top_to_bottom_field;
  873     uint tmp;
  874 
  875     /*
  876       pic_order_cnt_type specifies the method to decode picture order
  877       count (as specified in subclause 8.2.1). The value of
  878       pic_order_cnt_type shall be in the range of 0 to 2, inclusive.
  879      */
  880     pic_order_cnt_type = get_ue_golomb(gb);
  881     if (pic_order_cnt_type == 0)
  882     {
  883         /*
  884           log2_max_pic_order_cnt_lsb_minus4 specifies the value of the
  885           variable MaxPicOrderCntLsb that is used in the decoding
  886           process for picture order count as specified in subclause
  887           8.2.1 as follows:
  888 
  889           MaxPicOrderCntLsb = 2( log2_max_pic_order_cnt_lsb_minus4 + 4 )
  890 
  891           The value of log2_max_pic_order_cnt_lsb_minus4 shall be in
  892           the range of 0 to 12, inclusive.
  893          */
  894         log2_max_pic_order_cnt_lsb = get_ue_golomb(gb) + 4;
  895     }
  896     else if (pic_order_cnt_type == 1)
  897     {
  898         /*
  899           delta_pic_order_always_zero_flag equal to 1 specifies that
  900           delta_pic_order_cnt[ 0 ] and delta_pic_order_cnt[ 1 ] are
  901           not present in the slice headers of the sequence and shall
  902           be inferred to be equal to
  903           0. delta_pic_order_always_zero_flag
  904          */
  905         delta_pic_order_always_zero_flag = get_bits1(gb);
  906 
  907         /*
  908           offset_for_non_ref_pic is used to calculate the picture
  909           order count of a non-reference picture as specified in
  910           8.2.1. The value of offset_for_non_ref_pic shall be in the
  911           range of -231 to 231 - 1, inclusive.
  912          */
  913         offset_for_non_ref_pic = get_se_golomb(gb);
  914 
  915         /*
  916           offset_for_top_to_bottom_field is used to calculate the
  917           picture order count of a bottom field as specified in
  918           subclause 8.2.1. The value of offset_for_top_to_bottom_field
  919           shall be in the range of -231 to 231 - 1, inclusive.
  920          */
  921         offset_for_top_to_bottom_field = get_se_golomb(gb);
  922 
  923         /*
  924           offset_for_ref_frame[ i ] is an element of a list of
  925           num_ref_frames_in_pic_order_cnt_cycle values used in the
  926           decoding process for picture order count as specified in
  927           subclause 8.2.1. The value of offset_for_ref_frame[ i ]
  928           shall be in the range of -231 to 231 - 1, inclusive.
  929          */
  930         tmp = get_ue_golomb(gb);
  931         for (uint idx = 0; idx < tmp; ++idx)
  932             get_se_golomb(gb);  // offset_for_ref_frame[i]
  933     }
  934     (void) offset_for_non_ref_pic; // suppress unused var warning
  935     (void) offset_for_top_to_bottom_field; // suppress unused var warning
  936 
  937     /*
  938       num_ref_frames specifies the maximum number of short-term and
  939       long-term reference frames, complementary reference field pairs,
  940       and non-paired reference fields that may be used by the decoding
  941       process for inter prediction of any picture in the
  942       sequence. num_ref_frames also determines the size of the sliding
  943       window operation as specified in subclause 8.2.5.3. The value of
  944       num_ref_frames shall be in the range of 0 to MaxDpbSize (as
  945       specified in subclause A.3.1 or A.3.2), inclusive.
  946      */
  947     num_ref_frames = get_ue_golomb(gb);
  948     /*
  949       gaps_in_frame_num_value_allowed_flag specifies the allowed
  950       values of frame_num as specified in subclause 7.4.3 and the
  951       decoding process in case of an inferred gap between values of
  952       frame_num as specified in subclause 8.2.5.2.
  953      */
  954     /* bool gaps_in_frame_num_allowed_flag = */ get_bits1(gb);
  955 
  956     /*
  957       pic_width_in_mbs_minus1 plus 1 specifies the width of each
  958       decoded picture in units of macroblocks.  16 macroblocks in a row
  959      */
  960     pic_width = (get_ue_golomb(gb) + 1) * 16;
  961     /*
  962       pic_height_in_map_units_minus1 plus 1 specifies the height in
  963       slice group map units of a decoded frame or field.  16
  964       macroblocks in each column.
  965      */
  966     pic_height = (get_ue_golomb(gb) + 1) * 16;
  967 
  968     /*
  969       frame_mbs_only_flag equal to 0 specifies that coded pictures of
  970       the coded video sequence may either be coded fields or coded
  971       frames. frame_mbs_only_flag equal to 1 specifies that every
  972       coded picture of the coded video sequence is a coded frame
  973       containing only frame macroblocks.
  974      */
  975     frame_mbs_only_flag = get_bits1(gb);
  976     if (!frame_mbs_only_flag)
  977     {
  978         pic_height *= 2;
  979 
  980         /*
  981           mb_adaptive_frame_field_flag equal to 0 specifies no
  982           switching between frame and field macroblocks within a
  983           picture. mb_adaptive_frame_field_flag equal to 1 specifies
  984           the possible use of switching between frame and field
  985           macroblocks within frames. When mb_adaptive_frame_field_flag
  986           is not present, it shall be inferred to be equal to 0.
  987          */
  988         get_bits1(gb); // mb_adaptive_frame_field_flag
  989     }
  990 
  991     get_bits1(gb);     // direct_8x8_inference_flag
  992 
  993     /*
  994       frame_cropping_flag equal to 1 specifies that the frame cropping
  995       offset parameters follow next in the sequence parameter
  996       set. frame_cropping_flag equal to 0 specifies that the frame
  997       cropping offset parameters are not present.
  998      */
  999     if (get_bits1(gb)) // frame_cropping_flag
 1000     {
 1001         frame_crop_left_offset = get_ue_golomb(gb);
 1002         frame_crop_right_offset = get_ue_golomb(gb);
 1003         frame_crop_top_offset = get_ue_golomb(gb);
 1004         frame_crop_bottom_offset = get_ue_golomb(gb);
 1005     }
 1006 
 1007     /*
 1008       vui_parameters_present_flag equal to 1 specifies that the
 1009       vui_parameters( ) syntax structure as specified in Annex E is
 1010       present. vui_parameters_present_flag equal to 0 specifies that
 1011       the vui_parameters( ) syntax structure as specified in Annex E
 1012       is not present.
 1013      */
 1014     if (get_bits1(gb)) // vui_parameters_present_flag
 1015         vui_parameters(gb);
 1016 }
 1017 
 1018 void H264Parser::parse_SPS(uint8_t *sps, uint32_t sps_size,
 1019                            bool& interlaced, int32_t& max_ref_frames)
 1020 {
 1021     GetBitContext gb;
 1022     init_get_bits(&gb, sps, sps_size << 3);
 1023     decode_SPS(&gb);
 1024     interlaced = !frame_mbs_only_flag;
 1025     max_ref_frames = num_ref_frames;
 1026 }
 1027 
 1028 void H264Parser::decode_PPS(GetBitContext * gb)
 1029 {
 1030     /*
 1031       pic_parameter_set_id identifies the picture parameter set that
 1032       is referred to in the slice header. The value of
 1033       pic_parameter_set_id shall be in the range of 0 to 255,
 1034       inclusive.
 1035      */
 1036     pic_parameter_set_id = get_ue_golomb(gb);
 1037 
 1038     /*
 1039       seq_parameter_set_id refers to the active sequence parameter
 1040       set. The value of seq_parameter_set_id shall be in the range of
 1041       0 to 31, inclusive.
 1042      */
 1043     seq_parameter_set_id = get_ue_golomb(gb);
 1044     get_bits1(gb); // entropy_coding_mode_flag;
 1045 
 1046     /*
 1047       pic_order_present_flag equal to 1 specifies that the picture
 1048       order count related syntax elements are present in the slice
 1049       headers as specified in subclause 7.3.3. pic_order_present_flag
 1050       equal to 0 specifies that the picture order count related syntax
 1051       elements are not present in the slice headers.
 1052      */
 1053     pic_order_present_flag = get_bits1(gb);
 1054 
 1055 #if 0 // Rest not currently needed, and requires <math.h>
 1056     uint num_slice_groups = get_ue_golomb(gb) + 1;
 1057     if (num_slice_groups > 1) // num_slice_groups (minus 1)
 1058     {
 1059         uint idx;
 1060 
 1061         switch (get_ue_golomb(gb)) // slice_group_map_type
 1062         {
 1063           case 0:
 1064             for (idx = 0; idx < num_slice_groups; ++idx)
 1065                 get_ue_golomb(gb); // run_length_minus1[idx]
 1066             break;
 1067           case 1:
 1068             for (idx = 0; idx < num_slice_groups; ++idx)
 1069             {
 1070                 get_ue_golomb(gb); // top_left[idx]
 1071                 get_ue_golomb(gb); // bottom_right[idx]
 1072             }
 1073             break;
 1074           case 3:
 1075           case 4:
 1076           case 5:
 1077             get_bits1(gb);     // slice_group_change_direction_flag
 1078             get_ue_golomb(gb); // slice_group_change_rate_minus1
 1079             break;
 1080           case 6:
 1081             uint pic_size_in_map_units = get_ue_golomb(gb) + 1;
 1082             uint num_bits = (int)ceil(log2(num_slice_groups));
 1083             for (idx = 0; idx < pic_size_in_map_units; ++idx)
 1084             {
 1085                 get_bits(gb, num_bits); //slice_group_id[idx]
 1086             }
 1087         }
 1088     }
 1089 
 1090     get_ue_golomb(gb); // num_ref_idx_10_active_minus1
 1091     get_ue_golomb(gb); // num_ref_idx_11_active_minus1
 1092     get_bits1(gb);     // weighted_pred_flag;
 1093     get_bits(gb, 2);   // weighted_bipred_idc
 1094     get_se_golomb(gb); // pic_init_qp_minus26
 1095     get_se_golomb(gb); // pic_init_qs_minus26
 1096     get_se_golomb(gb); // chroma_qp_index_offset
 1097     get_bits1(gb);     // deblocking_filter_control_present_flag
 1098     get_bits1(gb);     // constrained_intra_pref_flag
 1099     redundant_pic_cnt_present_flag = get_bits1(gb);
 1100 #endif
 1101 }
 1102 
 1103 void H264Parser::decode_SEI(GetBitContext *gb)
 1104 {
 1105     int   recovery_frame_cnt = -1;
 1106     bool  exact_match_flag = false;
 1107     bool  broken_link_flag = false;
 1108     int   changing_group_slice_idc = -1;
 1109 
 1110     int type = 0, size = 0;
 1111 
 1112     /* A message requires at least 2 bytes, and then
 1113      * there's the stop bit plus alignment, so there
 1114      * can be no message in less than 24 bits */
 1115     while (get_bits_left(gb) >= 24)
 1116     {
 1117         do {
 1118             type += show_bits(gb, 8);
 1119         } while (get_bits(gb, 8) == 0xFF);
 1120 
 1121         do {
 1122             size += show_bits(gb, 8);
 1123         } while (get_bits(gb, 8) == 0xFF);
 1124 
 1125         switch (type)
 1126         {
 1127           case SEI_TYPE_RECOVERY_POINT:
 1128             recovery_frame_cnt = get_ue_golomb(gb);
 1129             exact_match_flag = get_bits1(gb);
 1130             broken_link_flag = get_bits1(gb);
 1131             changing_group_slice_idc = get_bits(gb, 2);
 1132             au_contains_keyframe_message = (recovery_frame_cnt == 0);
 1133             if ((size - 12) > 0)
 1134                 skip_bits(gb, (size - 12) * 8);
 1135             return;
 1136 
 1137           default:
 1138             skip_bits(gb, size * 8);
 1139             break;
 1140         }
 1141     }
 1142 
 1143     (void) exact_match_flag; // suppress unused var warning
 1144     (void) broken_link_flag; // suppress unused var warning
 1145     (void) changing_group_slice_idc; // suppress unused var warning
 1146 }
 1147 
 1148 void H264Parser::vui_parameters(GetBitContext * gb)
 1149 {
 1150     /*
 1151       aspect_ratio_info_present_flag equal to 1 specifies that
 1152       aspect_ratio_idc is present. aspect_ratio_info_present_flag
 1153       equal to 0 specifies that aspect_ratio_idc is not present.
 1154      */
 1155     if (get_bits1(gb)) //aspect_ratio_info_present_flag
 1156     {
 1157         /*
 1158           aspect_ratio_idc specifies the value of the sample aspect
 1159           ratio of the luma samples. Table E-1 shows the meaning of
 1160           the code. When aspect_ratio_idc indicates Extended_SAR, the
 1161           sample aspect ratio is represented by sar_width and
 1162           sar_height. When the aspect_ratio_idc syntax element is not
 1163           present, aspect_ratio_idc value shall be inferred to be
 1164           equal to 0.
 1165          */
 1166         aspect_ratio_idc = get_bits(gb, 8);
 1167 
 1168         switch (aspect_ratio_idc)
 1169         {
 1170           case 0:
 1171             // Unspecified
 1172             break;
 1173           case 1:
 1174             // 1:1
 1175             /*
 1176               1280x720 16:9 frame without overscan
 1177               1920x1080 16:9 frame without overscan (cropped from 1920x1088)
 1178               640x480 4:3 frame without overscan
 1179              */
 1180             break;
 1181           case 2:
 1182             // 12:11
 1183             /*
 1184               720x576 4:3 frame with horizontal overscan
 1185               352x288 4:3 frame without overscan
 1186              */
 1187             break;
 1188           case 3:
 1189             // 10:11
 1190             /*
 1191               720x480 4:3 frame with horizontal overscan
 1192               352x240 4:3 frame without overscan
 1193              */
 1194             break;
 1195           case 4:
 1196             // 16:11
 1197             /*
 1198               720x576 16:9 frame with horizontal overscan
 1199               540x576 4:3 frame with horizontal overscan
 1200              */
 1201             break;
 1202           case 5:
 1203             // 40:33
 1204             /*
 1205               720x480 16:9 frame with horizontal overscan
 1206               540x480 4:3 frame with horizontal overscan
 1207              */
 1208             break;
 1209           case 6:
 1210             // 24:11
 1211             /*
 1212               352x576 4:3 frame without overscan
 1213               540x576 16:9 frame with horizontal overscan
 1214              */
 1215             break;
 1216           case 7:
 1217             // 20:11
 1218             /*
 1219               352x480 4:3 frame without overscan
 1220               480x480 16:9 frame with horizontal overscan
 1221              */
 1222             break;
 1223           case 8:
 1224             // 32:11
 1225             /*
 1226               352x576 16:9 frame without overscan
 1227              */
 1228             break;
 1229           case 9:
 1230             // 80:33
 1231             /*
 1232               352x480 16:9 frame without overscan
 1233              */
 1234             break;
 1235           case 10:
 1236             // 18:11
 1237             /*
 1238               480x576 4:3 frame with horizontal overscan
 1239              */
 1240             break;
 1241           case 11:
 1242             // 15:11
 1243             /*
 1244               480x480 4:3 frame with horizontal overscan
 1245              */
 1246             break;
 1247           case 12:
 1248             // 64:33
 1249             /*
 1250               540x576 16:9 frame with horizontal overscan
 1251              */
 1252             break;
 1253           case 13:
 1254             // 160:99
 1255             /*
 1256               540x576 16:9 frame with horizontal overscan
 1257              */
 1258             break;
 1259           case EXTENDED_SAR:
 1260             sar_width  = get_bits(gb, 16);
 1261             sar_height = get_bits(gb, 16);
 1262             break;
 1263         }
 1264     }
 1265     else
 1266         sar_width = sar_height = 0;
 1267 
 1268     if (get_bits1(gb)) //overscan_info_present_flag
 1269         get_bits1(gb); //overscan_appropriate_flag
 1270 
 1271     if (get_bits1(gb)) //video_signal_type_present_flag
 1272     {
 1273         get_bits(gb, 3); //video_format
 1274         get_bits1(gb);   //video_full_range_flag
 1275         if (get_bits1(gb)) // colour_description_present_flag
 1276         {
 1277             get_bits(gb, 8); // colour_primaries
 1278             get_bits(gb, 8); // transfer_characteristics
 1279             get_bits(gb, 8); // matrix_coefficients
 1280         }
 1281     }
 1282 
 1283     if (get_bits1(gb)) //chroma_loc_info_present_flag
 1284     {
 1285         get_ue_golomb(gb); //chroma_sample_loc_type_top_field ue(v)
 1286         get_ue_golomb(gb); //chroma_sample_loc_type_bottom_field ue(v)
 1287     }
 1288 
 1289     if (get_bits1(gb)) //timing_info_present_flag
 1290     {
 1291         unitsInTick = get_bits_long(gb, 32); //num_units_in_tick
 1292         timeScale = get_bits_long(gb, 32);   //time_scale
 1293         fixedRate = get_bits1(gb);
 1294     }
 1295 }
 1296 
 1297 double H264Parser::frameRate(void) const
 1298 {
 1299     uint64_t    num;
 1300     double      fps;
 1301 
 1302     num   = 500 * (uint64_t)timeScale; /* 1000 * 0.5 */
 1303     fps   = ( unitsInTick != 0 ? num / (double)unitsInTick : 0 ) / 1000;
 1304 
 1305     return fps;
 1306 }
 1307 
 1308 void H264Parser::getFrameRate(FrameRate &result) const
 1309 {
 1310     if (unitsInTick == 0)
 1311         result = FrameRate(0);
 1312     else if (timeScale & 0x1)
 1313         result = FrameRate(timeScale, unitsInTick * 2);
 1314     else
 1315         result = FrameRate(timeScale / 2, unitsInTick);
 1316 }
 1317 
 1318 uint H264Parser::aspectRatio(void) const
 1319 {
 1320 
 1321     double aspect = 0.0;
 1322 
 1323     if (pic_height)
 1324         aspect = pictureWidthCropped() / (double)pictureHeightCropped();
 1325 
 1326     switch (aspect_ratio_idc)
 1327     {
 1328         case 0:
 1329             // Unspecified
 1330             break;
 1331         case 1:
 1332             // 1:1
 1333             break;
 1334         case 2:
 1335             // 12:11
 1336             aspect *= 1.0909090909090908;
 1337             break;
 1338         case 3:
 1339             // 10:11
 1340             aspect *= 0.90909090909090906;
 1341             break;
 1342         case 4:
 1343             // 16:11
 1344             aspect *= 1.4545454545454546;
 1345             break;
 1346         case 5:
 1347             // 40:33
 1348             aspect *= 1.2121212121212122;
 1349             break;
 1350         case 6:
 1351             // 24:11
 1352             aspect *= 2.1818181818181817;
 1353             break;
 1354         case 7:
 1355             // 20:11
 1356             aspect *= 1.8181818181818181;
 1357             break;
 1358         case 8:
 1359             // 32:11
 1360             aspect *= 2.9090909090909092;
 1361             break;
 1362         case 9:
 1363             // 80:33
 1364             aspect *= 2.4242424242424243;
 1365             break;
 1366         case 10:
 1367             // 18:11
 1368             aspect *= 1.6363636363636365;
 1369             break;
 1370         case 11:
 1371             // 15:11
 1372             aspect *= 1.3636363636363635;
 1373             break;
 1374         case 12:
 1375             // 64:33
 1376             aspect *= 1.9393939393939394;
 1377             break;
 1378         case 13:
 1379             // 160:99
 1380             aspect *= 1.6161616161616161;
 1381             break;
 1382         case 14:
 1383             // 4:3
 1384             aspect *= 1.3333333333333333;
 1385             break;
 1386         case 15:
 1387             // 3:2
 1388             aspect *= 1.5;
 1389             break;
 1390         case 16:
 1391             // 2:1
 1392             aspect *= 2.0;
 1393             break;
 1394         case EXTENDED_SAR:
 1395             if (sar_height)
 1396                 aspect *= sar_width / (double)sar_height;
 1397             else
 1398                 aspect = 0.0;
 1399             break;
 1400     }
 1401 
 1402     if (aspect == 0.0)
 1403         return 0;
 1404     if (fabs(aspect - 1.3333333333333333) < eps)
 1405         return 2;
 1406     if (fabs(aspect - 1.7777777777777777) < eps)
 1407         return 3;
 1408     if (fabs(aspect - 2.21) < eps)
 1409         return 4;
 1410 
 1411     return aspect * 1000000;
 1412 }
 1413 
 1414 // Following the lead of libavcodec, ignore the left cropping.
 1415 uint H264Parser::pictureWidthCropped(void) const
 1416 {
 1417     uint ChromaArrayType = separate_colour_plane_flag ? 0 : chroma_format_idc;
 1418     uint CropUnitX = 1;
 1419     uint SubWidthC = chroma_format_idc == 3 ? 1 : 2;
 1420     if (ChromaArrayType != 0)
 1421         CropUnitX = SubWidthC;
 1422     uint crop = CropUnitX * frame_crop_right_offset;
 1423     return pic_width - crop;
 1424 }
 1425 
 1426 // Following the lead of libavcodec, ignore the top cropping.
 1427 uint H264Parser::pictureHeightCropped(void) const
 1428 {
 1429     uint ChromaArrayType = separate_colour_plane_flag ? 0 : chroma_format_idc;
 1430     uint CropUnitY = 2 - frame_mbs_only_flag;
 1431     uint SubHeightC = chroma_format_idc <= 1 ? 2 : 1;
 1432     if (ChromaArrayType != 0)
 1433         CropUnitY *= SubHeightC;
 1434     uint crop = CropUnitY * frame_crop_bottom_offset;
 1435     return pic_height - crop;
 1436 }
Generated on Mon Feb 22 2016 12:01:19 for MythTV by   doxygen 1.8.9.1
