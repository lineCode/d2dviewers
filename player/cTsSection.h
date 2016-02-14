// cTs.h
#pragma once
//#define TSERROR
//#define EIT_EXTENDED_EVENT_DEBUG
//{{{  includes
#include "pch.h"

#include "huffDecoder.h"
#include "dvbSubtitle.h"

#include <locale>
#include <time.h>
//}}}
//{{{  ts, pid, tid const
// ts
#define TS_SIZE          188
#define TS_HEADER        0x47
#define PAY_START        0x40
#define ADAPT_FIELD      0x20

// pid
#define PID_PAT          0x00   /* Program Association Table */
#define PID_CAT          0x01   /* Conditional Access Table */
#define PID_NIT          0x10   /* Network Information Table */
#define PID_BAT          0x11   /* Bouquet Association Table */
#define PID_SDT          0x11   /* Service Description Table */
#define PID_EIT          0x12   /* Event Information Table */
#define PID_RST          0x13   /* Running Status Table */
#define PID_TDT          0x14   /* Time Date Table */
#define PID_TOT          0x14   /* Time Offset Table */
#define PID_STF          0x14   /* Stuffing Table */
#define PID_SYN          0x15   /* Network sync */

// tid
#define TID_PAT          0x00   /* Program Association Section */
#define TID_CAT          0x01   /* Conditional Access Section */
#define TID_PMT          0x02   /* Conditional Access Section */
#define TID_EIT          0x12   /* Event Information Section */
#define TID_NIT_ACT      0x40   /* Network Information Section - actual */
#define TID_NIT_OTH      0x41   /* Network Information Section - other */
#define TID_SDT_ACT      0x42   /* Service Description Section - actual */
#define TID_SDT_OTH      0x46   /* Service Description Section - other */
#define TID_BAT          0x4A   /* Bouquet Association Section */
#define TID_EIT_ACT      0x4E   /* Event Information Section - actual */
#define TID_EIT_OTH      0x4F   /* Event Information Section - other */
#define TID_EIT_ACT_SCH  0x50   /* Event Information Section - actual, schedule  */
#define TID_EIT_OTH_SCH  0x60   /* Event Information Section - other, schedule */
#define TID_TDT          0x70   /* Time Date Section */
#define TID_RST          0x71   /* Running Status Section */
#define TID_ST           0x72   /* Stuffing Section */
#define TID_TOT          0x73   /* Time Offset Section */

// serviceType
#define kServiceTypeTV            0x01
#define kServiceTypeRadio         0x02
#define kServiceTypeAdvancedSDTV  0x16
#define kServiceTypeAdvancedHDTV  0x19
//}}}
//{{{  descr const
#define DESCR_VIDEO_STREAM          0x02
#define DESCR_AUDIO_STREAM          0x03
#define DESCR_HIERARCHY             0x04
#define DESCR_REGISTRATION          0x05
#define DESCR_DATA_STREAM_ALIGN     0x06
#define DESCR_TARGET_BACKGRID       0x07
#define DESCR_VIDEO_WINDOW          0x08
#define DESCR_CA                    0x09
#define DESCR_ISO_639_LANGUAGE      0x0A
#define DESCR_SYSTEM_CLOCK          0x0B
#define DESCR_MULTIPLEX_BUFFER_UTIL 0x0C
#define DESCR_COPYRIGHT             0x0D
#define DESCR_MAXIMUM_BITRATE       0x0E
#define DESCR_PRIVATE_DATA_IND      0x0F

#define DESCR_SMOOTHING_BUFFER      0x10
#define DESCR_STD                   0x11
#define DESCR_IBP                   0x12

#define DESCR_NW_NAME               0x40
#define DESCR_SERVICE_LIST          0x41
#define DESCR_STUFFING              0x42
#define DESCR_SAT_DEL_SYS           0x43
#define DESCR_CABLE_DEL_SYS         0x44
#define DESCR_VBI_DATA              0x45
#define DESCR_VBI_TELETEXT          0x46
#define DESCR_BOUQUET_NAME          0x47
#define DESCR_SERVICE               0x48
#define DESCR_COUNTRY_AVAIL         0x49
#define DESCR_LINKAGE               0x4A
#define DESCR_NVOD_REF              0x4B
#define DESCR_TIME_SHIFTED_SERVICE  0x4C
#define DESCR_SHORT_EVENT           0x4D
#define DESCR_EXTENDED_EVENT        0x4E
#define DESCR_TIME_SHIFTED_EVENT    0x4F

#define DESCR_COMPONENT             0x50
#define DESCR_MOSAIC                0x51
#define DESCR_STREAM_ID             0x52
#define DESCR_CA_IDENT              0x53
#define DESCR_CONTENT               0x54
#define DESCR_PARENTAL_RATING       0x55
#define DESCR_TELETEXT              0x56
#define DESCR_TELEPHONE             0x57
#define DESCR_LOCAL_TIME_OFF        0x58
#define DESCR_SUBTITLING            0x59
#define DESCR_TERR_DEL_SYS          0x5A
#define DESCR_ML_NW_NAME            0x5B
#define DESCR_ML_BQ_NAME            0x5C
#define DESCR_ML_SERVICE_NAME       0x5D
#define DESCR_ML_COMPONENT          0x5E
#define DESCR_PRIV_DATA_SPEC        0x5F

#define DESCR_SERVICE_MOVE          0x60
#define DESCR_SHORT_SMOOTH_BUF      0x61
#define DESCR_FREQUENCY_LIST        0x62
#define DESCR_PARTIAL_TP_STREAM     0x63
#define DESCR_DATA_BROADCAST        0x64
#define DESCR_CA_SYSTEM             0x65
#define DESCR_DATA_BROADCAST_ID     0x66
#define DESCR_TRANSPORT_STREAM      0x67
#define DESCR_DSNG                  0x68
#define DESCR_PDC                   0x69
#define DESCR_AC3                   0x6A
#define DESCR_ANCILLARY_DATA        0x6B
#define DESCR_CELL_LIST             0x6C
#define DESCR_CELL_FREQ_LINK        0x6D
#define DESCR_ANNOUNCEMENT_SUPPORT  0x6E
//}}}
//{{{  tables
//{{{  pat layout
#define PAT_LEN 8
typedef struct {
  unsigned char table_id                  :8;

  unsigned char section_length_hi         :4;
  unsigned char dummy1                    :2;
  unsigned char dummy                     :1;
  unsigned char section_syntax_indicator  :1;

  unsigned char section_length_lo         :8;
  unsigned char transport_stream_id_hi    :8;
  unsigned char transport_stream_id_lo    :8;

  unsigned char current_next_indicator    :1;
  unsigned char version_number            :5;
  unsigned char dummy2                    :2;

  unsigned char section_number            :8;
  unsigned char last_section_number       :8;
  } pat_t;

#define PAT_PROG_LEN 4
typedef struct {
  unsigned char program_number_hi         :8;
  unsigned char program_number_lo         :8;

  unsigned char network_pid_hi            :5;
  unsigned char                           :3;
  unsigned char network_pid_lo            :8;
  /* or program_map_pid (if prog_num=0)*/
  } pat_prog_t;
//}}}
//{{{  pmt layout
#define PMT_LEN 12
typedef struct {
   unsigned char table_id                  :8;

   unsigned char section_length_hi         :4;
   unsigned char                           :2;
   unsigned char dummy                     :1; // has to be 0
   unsigned char section_syntax_indicator  :1;
   unsigned char section_length_lo         :8;

   unsigned char program_number_hi         :8;
   unsigned char program_number_lo         :8;
   unsigned char current_next_indicator    :1;
   unsigned char version_number            :5;
   unsigned char                           :2;
   unsigned char section_number            :8;
   unsigned char last_section_number       :8;
   unsigned char PCR_PID_hi                :5;
   unsigned char                           :3;
   unsigned char PCR_PID_lo                :8;
   unsigned char program_info_length_hi    :4;
   unsigned char                           :4;
   unsigned char program_info_length_lo    :8;
   //descrs
} pmt_t;

#define PMT_INFO_LEN 5
typedef struct {
   unsigned char stream_type        :8;
   unsigned char elementary_PID_hi  :5;
   unsigned char                    :3;
   unsigned char elementary_PID_lo  :8;
   unsigned char ES_info_length_hi  :4;
   unsigned char                    :4;
   unsigned char ES_info_length_lo  :8;
   // descrs
} pmt_info_t;
//}}}
//{{{  nit layout
#define NIT_LEN 10
typedef struct {
   unsigned char table_id                     :8;

   unsigned char section_length_hi         :4;
   unsigned char                           :3;
   unsigned char section_syntax_indicator  :1;
   unsigned char section_length_lo         :8;

   unsigned char network_id_hi             :8;
   unsigned char network_id_lo             :8;
   unsigned char current_next_indicator    :1;
   unsigned char version_number            :5;
   unsigned char                           :2;
   unsigned char section_number            :8;
   unsigned char last_section_number       :8;
   unsigned char network_descr_length_hi   :4;
   unsigned char                           :4;
   unsigned char network_descr_length_lo   :8;
  /* descrs */
} nit_t;

#define SIZE_NIT_MID 2
typedef struct {                                 // after descrs
   unsigned char transport_stream_loop_length_hi  :4;
   unsigned char                                  :4;
   unsigned char transport_stream_loop_length_lo  :8;
} nit_mid_t;

#define SIZE_NIT_END 4
struct nit_end_struct {
   long CRC;
};

#define NIT_TS_LEN 6
typedef struct {
   unsigned char transport_stream_id_hi      :8;
   unsigned char transport_stream_id_lo      :8;
   unsigned char original_network_id_hi      :8;
   unsigned char original_network_id_lo      :8;
   unsigned char transport_descrs_length_hi  :4;
   unsigned char                             :4;
   unsigned char transport_descrs_length_lo  :8;
   /* descrs  */
} nit_ts_t;
//}}}
//{{{  eit layout
#define EIT_LEN 14
typedef struct {
   unsigned char table_id                    :8;

   unsigned char section_length_hi           :4;
   unsigned char                             :3;
   unsigned char section_syntax_indicator    :1;
   unsigned char section_length_lo           :8;

   unsigned char service_id_hi               :8;
   unsigned char service_id_lo               :8;
   unsigned char current_next_indicator      :1;
   unsigned char version_number              :5;
   unsigned char                             :2;
   unsigned char section_number              :8;
   unsigned char last_section_number         :8;
   unsigned char transport_stream_id_hi      :8;
   unsigned char transport_stream_id_lo      :8;
   unsigned char original_network_id_hi      :8;
   unsigned char original_network_id_lo      :8;
   unsigned char segment_last_section_number :8;
   unsigned char segment_last_table_id       :8;
} eit_t;

#define EIT_EVENT_LEN 12
typedef struct {
   unsigned char event_id_hi                 :8;
   unsigned char event_id_lo                 :8;
   unsigned char mjd_hi                      :8;
   unsigned char mjd_lo                      :8;
   unsigned char start_time_h                :8;
   unsigned char start_time_m                :8;
   unsigned char start_time_s                :8;
   unsigned char duration_h                  :8;
   unsigned char duration_m                  :8;
   unsigned char duration_s                  :8;
   unsigned char descrs_loop_length_hi       :4;
   unsigned char free_ca_mode                :1;
   unsigned char running_status              :3;
   unsigned char descrs_loop_length_lo       :8;
} eit_event_t;
//}}}
//{{{  sdt layout
#define SDT_LEN 11
typedef struct {
   unsigned char table_id                    :8;
   unsigned char section_length_hi           :4;
   unsigned char                             :3;
   unsigned char section_syntax_indicator    :1;
   unsigned char section_length_lo           :8;
   unsigned char transport_stream_id_hi      :8;
   unsigned char transport_stream_id_lo      :8;
   unsigned char current_next_indicator      :1;
   unsigned char version_number              :5;
   unsigned char                             :2;
   unsigned char section_number              :8;
   unsigned char last_section_number         :8;
   unsigned char original_network_id_hi      :8;
   unsigned char original_network_id_lo      :8;
   unsigned char                             :8;
} sdt_t;

#define GetSDTTransportStreamId(x) (HILO(((sdt_t*)x)->transport_stream_id))
#define GetSDTOriginalNetworkId(x) (HILO(((sdt_t*)x)->original_network_id))

#define SDT_DESCR_LEN 5
typedef struct {
   unsigned char service_id_hi                :8;
   unsigned char service_id_lo                :8;
   unsigned char eit_present_following_flag   :1;
   unsigned char eit_schedule_flag            :1;
   unsigned char                              :6;
   unsigned char descrs_loop_length_hi        :4;
   unsigned char free_ca_mode                 :1;
   unsigned char running_status               :3;
   unsigned char descrs_loop_length_lo        :8;
} sdt_descr_t;
//}}}
//{{{  tdt layout
#define TDT_LEN 8
typedef struct {
  unsigned char table_id                  :8;
  unsigned char section_length_hi         :4;
  unsigned char                           :3;
  unsigned char section_syntax_indicator  :1;
  unsigned char section_length_lo         :8;
  unsigned char utc_mjd_hi                :8;
  unsigned char utc_mjd_lo                :8;
  unsigned char utc_time_h                :8;
  unsigned char utc_time_m                :8;
  unsigned char utc_time_s                :8;
  } tdt_t;
//}}}

//{{{
typedef struct descr_gen_struct {
  unsigned char descr_tag        :8;
  unsigned char descr_length     :8;
  } descr_gen_t;

#define CastGenericDescr(x) ((descr_gen_t *)(x))

#define GetDescrTag(x) (((descr_gen_t *) x)->descr_tag)
#define GetDescrLength(x) (((descr_gen_t *) x)->descr_length+2)
//}}}

//{{{  0x40 network_name_desciptor
#define DESCR_NETWORK_NAME_LEN 2

typedef struct descr_network_name_struct {
  unsigned char descr_tag     :8;
  unsigned char descr_length  :8;
  } descr_network_name_t;

#define CastNetworkNameDescr(x) ((descr_network_name_t *)(x))
#define CastServiceListDescrLoop(x) ((descr_service_list_loop_t *)(x))
//}}}
//{{{  0x41 service_list_descr
#define DESCR_SERVICE_LIST_LEN 2

typedef struct descr_service_list_struct {
  unsigned char descr_tag     :8;
  unsigned char descr_length  :8;
  } descr_service_list_t;

#define CastServiceListDescr(x) ((descr_service_list_t *)(x))


#define DESCR_SERVICE_LIST_LOOP_LEN 3

typedef struct descr_service_list_loop_struct {
  unsigned char service_id_hi  :8;
  unsigned char service_id_lo  :8;
  unsigned char service_type   :8;
  } descr_service_list_loop_t;

#define CastServiceListDescrLoop(x) ((descr_service_list_loop_t *)(x))
//}}}
//{{{  0x47 bouquet_name_descr
#define DESCR_BOUQUET_NAME_LEN 2

typedef struct descr_bouquet_name_struct {
  unsigned char descr_tag    :8;
  unsigned char descr_length :8;
  } descr_bouquet_name_t;

#define CastBouquetNameDescr(x) ((descr_bouquet_name_t *)(x))
//}}}
//{{{  0x48 service_descr
#define DESCR_SERVICE_LEN  4

typedef struct descr_service_struct {
  unsigned char descr_tag             :8;
  unsigned char descr_length          :8;
  unsigned char service_type          :8;
  unsigned char provider_name_length  :8;
  } descr_service_t;

#define CastServiceDescr(x) ((descr_service_t *)(x))
//}}}
//{{{  0x4D short_event_descr
#define DESCR_SHORT_EVENT_LEN 6

typedef struct descr_short_event_struct {
  unsigned char descr_tag         :8;
  unsigned char descr_length      :8;
  unsigned char lang_code1        :8;
  unsigned char lang_code2        :8;
  unsigned char lang_code3        :8;
  unsigned char event_name_length :8;
  } descr_short_event_t;

#define CastShortEventDescr(x) ((descr_short_event_t *)(x))
//}}}
//{{{  0x4E extended_event_descr
#define DESCR_EXTENDED_EVENT_LEN 7

typedef struct descr_extended_event_struct {
  unsigned char descr_tag          :8;
  unsigned char descr_length       :8;
  /* TBD */
  unsigned char last_descr_number  :4;
  unsigned char descr_number       :4;
  unsigned char lang_code1         :8;
  unsigned char lang_code2         :8;
  unsigned char lang_code3         :8;
  unsigned char length_of_items    :8;
  } descr_extended_event_t;

#define CastExtendedEventDescr(x) ((descr_extended_event_t *)(x))


#define ITEM_EXTENDED_EVENT_LEN 1

typedef struct item_extended_event_struct {
  unsigned char item_description_length               :8;
  } item_extended_event_t;

#define CastExtendedEventItem(x) ((item_extended_event_t *)(x))
//}}}

//{{{  0x50 component_descr
#define DESCR_COMPONENT_LEN  8

typedef struct descr_component_struct {
  unsigned char descr_tag       :8;
  unsigned char descr_length    :8;
  unsigned char stream_content  :4;
  unsigned char reserved        :4;
  unsigned char component_type  :8;
  unsigned char component_tag   :8;
  unsigned char lang_code1      :8;
  unsigned char lang_code2      :8;
  unsigned char lang_code3      :8;
  } descr_component_t;

#define CastComponentDescr(x) ((descr_component_t *)(x))
//}}}
//{{{  0x54 content_descr
#define DESCR_CONTENT_LEN 2

typedef struct descr_content_struct {
  unsigned char descr_tag     :8;
  unsigned char descr_length  :8;
  } descr_content_t;

#define CastContentDescr(x) ((descr_content_t *)(x))


typedef struct nibble_content_struct {
  unsigned char user_nibble_2           :4;
  unsigned char user_nibble_1           :4;
  unsigned char content_nibble_level_2  :4;
  unsigned char content_nibble_level_1  :4;
  } nibble_content_t;

#define CastContentNibble(x) ((nibble_content_t *)(x))
//}}}
//{{{  0x59 subtitling_descr
#define DESCR_SUBTITLING_LEN 2

typedef struct descr_subtitling_struct {
  unsigned char descr_tag      :8;
  unsigned char descr_length   :8;
  } descr_subtitling_t;

#define CastSubtitlingDescr(x) ((descr_subtitling_t *)(x))

#define ITEM_SUBTITLING_LEN 8

typedef struct item_subtitling_struct {
  unsigned char lang_code1             :8;
  unsigned char lang_code2             :8;
  unsigned char lang_code3             :8;
  unsigned char subtitling_type        :8;
  unsigned char composition_page_id_hi :8;
  unsigned char composition_page_id_lo :8;
  unsigned char ancillary_page_id_hi   :8;
  unsigned char ancillary_page_id_lo   :8;
  } item_subtitling_t;

#define CastSubtitlingItem(x) ((item_subtitling_t *)(x))
//}}}
//{{{  0x5A terrestrial_delivery_system_descr
#define DESCR_TERRESTRIAL_DELIVERY_SYSTEM_LEN XX

typedef struct descr_terrestrial_delivery_struct {
  unsigned char descr_tag      :8;
  unsigned char descr_length   :8;

  unsigned char frequency1            :8;
  unsigned char frequency2            :8;
  unsigned char frequency3            :8;
  unsigned char frequency4            :8;

  unsigned char reserved1             :5;
  unsigned char bandwidth             :3;

  unsigned char code_rate_HP          :3;
  unsigned char hierarchy             :3;
  unsigned char constellation         :2;
  unsigned char other_frequency_flag  :1;
  unsigned char transmission_mode     :2;
  unsigned char guard_interval        :2;
  unsigned char code_rate_LP          :3;

  unsigned char reserver2             :8;
  unsigned char reserver3             :8;
  unsigned char reserver4             :8;
  unsigned char reserver5             :8;
  } descr_terrestrial_delivery_system_t;

#define CastTerrestrialDeliverySystemDescr(x) ((descr_terrestrial_delivery_system_t *)(x))
//}}}

//{{{  0x62 frequency_list_descr
#define DESCR_FREQUENCY_LIST_LEN XX

typedef struct descr_frequency_list_struct {
  unsigned char descr_tag      :8;
  unsigned char descr_length   :8;
  /* TBD */
  } descr_frequency_list_t;

#define CastFrequencyListDescr(x) ((descr_frequency_list_t *)(x))
//}}}

//{{{
// CRC32 lookup table for polynomial 0x04c11db7
static unsigned long crcTable[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};
//}}}
//}}}
//{{{  macros
#define HILO(x) (x##_hi << 8 | x##_lo)

#define MjdToEpochTime(x) unsigned int((((x##_hi << 8) | x##_lo) - 40587) * 86400)

#define BcdTimeToSeconds(x) ((3600 * ((10*((x##_h & 0xF0)>>4)) + (x##_h & 0xF))) + \
                               (60 * ((10*((x##_m & 0xF0)>>4)) + (x##_m & 0xF))) + \
                                     ((10*((x##_s & 0xF0)>>4)) + (x##_s & 0xF)))

#define BcdTimeToMinutes(x) ((60 * ((10*((x##_h & 0xF0)>>4)) + (x##_h & 0xF))) + \
                                  (((10*((x##_m & 0xF0)>>4)) + (x##_m & 0xF))))

#define BcdCharToInt(x) (10*((x & 0xF0)>>4) + (x & 0xF))

#define CheckBcdChar(x) ((((x & 0xF0)>>4) <= 9) && \
                          ((x & 0x0F) <= 9))
#define CheckBcdSignedChar(x) ((((x & 0xF0)>>4) >= 0) && (((x & 0xF0)>>4) <= 9) && \
                                ((x & 0x0F) >= 0) && ((x & 0x0F) <= 9))

#define SetRunningStatus(x,s) ((x)|=((s)&RUNNING_STATUS_RUNNING))
//}}}
//{{{
class cPidInfo {
public:
  //{{{
  cPidInfo::cPidInfo (int pid, bool isSection) : mPid(pid), mSid(-1), mIsSection(isSection),
                                                 mStreamType(0), mPts(0), mDts(0), mContinuity(-1), mTotal(0),
                                                 mLength(0), mBufBytes(0), mPesBuf(nullptr), mPesPtr(nullptr) {
    switch (pid) {
      case PID_PAT: wcscpy (mInfo, L"Pat "); break;
      case PID_CAT: wcscpy (mInfo, L"Cat "); break;
      case PID_SDT: wcscpy (mInfo, L"Sdt "); break;
      case PID_NIT: wcscpy (mInfo, L"Nit "); break;
      case PID_EIT: wcscpy (mInfo, L"Eit "); break;
      case PID_RST: wcscpy (mInfo, L"Rst "); break;
      case PID_TDT: wcscpy (mInfo, L"Tdt "); break;
      case PID_SYN: wcscpy (mInfo, L"Syn "); break;
      default: mInfo[0] = L'\0'; break;
      }
    }
  //}}}
  cPidInfo::~cPidInfo() {}

  //{{{
  void cPidInfo::print() {
    printf ("- pid:%d sid:%d total:%d\n", mPid, mSid, mTotal);
    }
  //}}}

  int  mPid;
  int  mSid;
  bool mIsSection;

  int mStreamType;
  int64_t mPts;
  int64_t mDts;

  int  mContinuity;
  int  mTotal;

  // section buffer
  int mLength;
  size_t mBufBytes;
  unsigned char mBuf[10000];

  uint8_t* mPesBuf;
  uint8_t* mPesPtr;

  // render text for speed,locking
  wchar_t mInfo[100];
  };
//}}}
//{{{
class cEpgItem {
public:
  //{{{
  cEpgItem() : mStartTime(0), mDuration(0) {

    mTitle[0] = 0;
    mShortDescription[0] = 0;
    }
  //}}}
  //{{{
  cEpgItem (time_t startTime, int duration, char* title, char* shortDescription)
      : mStartTime(startTime), mDuration(duration) {

    strcpy (mTitle, title);
    strcpy (mShortDescription, shortDescription);
    }
  //}}}
  ~cEpgItem() {}

  //{{{
  void set (time_t startTime, int duration, char* title, char* shortDescription) {

    mStartTime = startTime;
    mDuration = duration;

    strcpy (mTitle, title);
    strcpy (mShortDescription, shortDescription);
    }
  //}}}
  //{{{
  void print (char* prefix) {

    struct tm when = *localtime (&mStartTime);
    char* time = asctime(&when);
    time[24] = 0;

    printf ("%s%s %3dm <%s>\n", prefix, time, mDuration/60, mTitle);
    }
  //}}}

  time_t mStartTime;
  int    mDuration;
  char   mTitle[100];
  char   mShortDescription[400];
  };
//}}}
//{{{
class cService {
public:
  //{{{
  cService (int sid, int tsid, int onid, int type, char* provider, char* name)
    : mSid(sid), mTsid(tsid), mOnid(onid), mType(type),
      mVidPid(-1), mNumAudPids(0), mSubPid(-1), mPcrPid(-1), mProgramPid(-1)  {

    strcpy (mName, name);
    strcpy (mProvider, provider);

    mAudPids[0] = -1;
    mAudPids[1] = -1;
    }
  //}}}
  ~cService() {}

  //  gets
  int getSid() const { return mSid; }
  int getTsid() const { return mTsid; }
  int getOnid() const { return mOnid; }
  int getType() const { return mType; }
  //{{{
  char* getTypeStr() {
    switch (mType) {
      case kServiceTypeTV :           return "TV";
      case kServiceTypeRadio :        return "Radio";
      case kServiceTypeAdvancedSDTV : return "SDTV";
      case kServiceTypeAdvancedHDTV : return "HDTV";
      }
    return "none";
    }
  //}}}

  int getVidPid() const { return mVidPid; }
  int getAudPid() const { return mAudPids[0]; }
  int getSubPid() const { return mSubPid; }
  int getPcrPid() const { return mPcrPid; }
  int getProgramPid() const { return mProgramPid; }

  char* getName() { return mName; }
  char* getProvider() { return mProvider; }
  cEpgItem* getNow() { return &mNow; }

  //  sets
  void setVidPid (const int pid) { mVidPid = pid; }
  void setAudPid (const int pid) {
    if (mNumAudPids == 0)
      mAudPids[mNumAudPids++] = pid;
    else if (mAudPids[0] != pid)
      mAudPids[1] = pid;
    }
  void setSubPid (const int pid) { mSubPid = pid; }
  void setPcrPid (const int pid) { mPcrPid = pid; }
  void setProgramPid (const int pid) { mProgramPid = pid; }

  //{{{
  bool setNow (time_t startTime, int duration, char* str1, char* str2) {

    bool nowChanged = (startTime != mNow.mStartTime);

    mNow.set (startTime, duration, str1, str2);

    return nowChanged;
    }
  //}}}
  //{{{
  void setEpg (time_t startTime, int duration, char* str1, char* str2) {

    mEpgItemMap.insert (tEpgItemMap::value_type (startTime, cEpgItem(startTime, duration, str1, str2)));
    }
  //}}}
  //{{{
  void print() {

    printf ("- sid:%d tsid:%d onid:%d - prog:%d v:%d a:%d sub:%d pcr:%d %s <%s> <%s>\n",
            mSid, mTsid, mOnid, mProgramPid, mVidPid, mNumAudPids, mSubPid, mPcrPid,
            getTypeStr(), mName, mProvider);

    mNow.print ("  - ");

    for (auto epgItem : mEpgItemMap)
      epgItem.second.print ("    - ");
    }
  //}}}

private:
  int mSid;
  int mTsid;
  int mOnid;
  int mType;

  int mVidPid;
  int mNumAudPids;
  int mAudPids[2];
  int mSubPid;
  int mPcrPid;
  int mProgramPid;

  char mName[20];
  char mProvider[20];

  cEpgItem mNow;

  typedef std::map<time_t,cEpgItem> tEpgItemMap;  // startTime, cEpgItem
  tEpgItemMap mEpgItemMap;
  };
//}}}

typedef std::map<int,cPidInfo> tPidInfoMap;  // pid inserts <pid,cPidInfo> into mPidInfoMap
typedef std::map<int,int>      tProgramMap;  // PAT inserts <pid,sid>'s into mProgramMap
                                             //     - pids parsed as programPid PMTs
typedef std::map<int,cService> tServiceMap;  // SDT inserts <sid,cService> into mServiceMap
                                             //     - sets cService ServiceType,Name,Provider
                                             // PMT - sets cService stream pids
                                             // EIT - adds cService Now,Epg events
class cTsSection {
public:
  //{{{
  tPidInfoMap* getPidInfoMap() {

    return &mPidInfoMap;
    }
  //}}}

  //{{{
  void parseEit (unsigned char* buf, unsigned char* bufEnd) {

    eit_t* Eit = (eit_t*)buf;
    if ((bufEnd != 0) && (buf + HILO(Eit->section_length) + 3 >= bufEnd)) {
      //{{{  sectionLength > buffer
      printf ("EIT - length > buffer %d\n", HILO(Eit->section_length) + 3);
      return;
      }
      //}}}
    if (crc32 (buf, HILO (Eit->section_length) + 3)) {
      //{{{  badCrc
      #ifdef TSERROR
      printf ("EIT - bad crc %d\n", HILO (Eit->section_length) + 3);
      #endif
      return;
      }
      //}}}

    int tid = Eit->table_id;
    int sid = HILO (Eit->service_id);
    //int tsid = HILO (Eit->transport_stream_id);
    //int onid = HILO (Eit->original_network_id);
    //printf ("EIT - tid:%x sid:%d\n", tid, sid);

    bool now = (tid == TID_EIT_ACT);
    bool next = (tid == TID_EIT_OTH);
    bool epg = (tid == TID_EIT_ACT_SCH) || (tid == TID_EIT_OTH_SCH) ||
               (tid == TID_EIT_ACT_SCH+1) || (tid == TID_EIT_OTH_SCH+1);
    if (!now && !next && !epg) {
      //{{{  unexpected tid
      printf ("EIT - unexpected tid:%x\n", tid);
      return;
      }
      //}}}

    // parse Descrs
    unsigned char* ptr = buf + EIT_LEN;
    int sectionLength = HILO (Eit->section_length) + 3 - EIT_LEN - 4;
    while (sectionLength > 0) {
      eit_event_t* EitEvent = (eit_event_t*)ptr;

      int loopLength = HILO (EitEvent->descrs_loop_length);
      if (loopLength > sectionLength - EIT_EVENT_LEN)
        return;
      ptr += EIT_EVENT_LEN;

      int DescrLength = 0;
      while ((DescrLength < loopLength) &&
             (GetDescrLength (ptr) > 0) &&
             (GetDescrLength (ptr) <= loopLength - DescrLength)) {

        switch (GetDescrTag(ptr)) {
          case DESCR_SHORT_EVENT: {
            //{{{  shortEvent
            tServiceMap::iterator it = mServiceMap.find (sid);
            if (it != mServiceMap.end()) {
              // recognised service
              time_t startTime = MjdToEpochTime (EitEvent->mjd) + BcdTimeToSeconds (EitEvent->start_time);
              int duration = BcdTimeToSeconds (EitEvent->duration);
              bool running = (EitEvent->running_status == 0x04);

              char* title = huffDecode (ptr + DESCR_SHORT_EVENT_LEN,
                                        CastShortEventDescr(ptr)->event_name_length);

              char siTitle[400];
              if (title == NULL) {
                getDescrName (ptr + DESCR_SHORT_EVENT_LEN,
                                     CastShortEventDescr(ptr)->event_name_length, siTitle);
                title = siTitle;
                }

              char* shortDescription = huffDecode (
                ptr + DESCR_SHORT_EVENT_LEN + CastShortEventDescr(ptr)->event_name_length+1,
                size_t(ptr + DESCR_SHORT_EVENT_LEN + CastShortEventDescr(ptr)->event_name_length));

              char siShortDescription[400];
              if (shortDescription == NULL) {
                getDescrText (
                  ptr + DESCR_SHORT_EVENT_LEN + CastShortEventDescr(ptr)->event_name_length+1,
                  *((unsigned char*)(ptr + DESCR_SHORT_EVENT_LEN + CastShortEventDescr(ptr)->event_name_length)),
                  siShortDescription);
                  shortDescription = siShortDescription;
                  }

              if (now & running) {
                if (it->second.setNow (startTime, duration, title, shortDescription)) {
                  it->second.getNow()->print ("EIT now changed - ");
                  updatePidInfo (it->second.getProgramPid());
                  updatePidInfo (it->second.getVidPid());
                  updatePidInfo (it->second.getAudPid());
                  updatePidInfo (it->second.getSubPid());
                  }
                }
              else if (epg)
                it->second.setEpg (startTime, duration, title, shortDescription);
              }

            break;
            }
            //}}}
          case DESCR_EXTENDED_EVENT: {
            //{{{  extendedEvent
            //0x4E extended_event_descr
            //#define DESCR_EXTENDED_EVENT_LEN 7
            //typedef struct descr_extended_event_struct {
            //  unsigned char descr_tag                         :8;
             // unsigned char descr_length                      :8;
            //  /* TBD */
            //  unsigned char last_descr_number                 :4;
            //  unsigned char descr_number                      :4;
            //  unsigned char lang_code1                             :8;
            //  unsigned char lang_code2                             :8;
            //  unsigned char lang_code3                             :8;
            //  unsigned char length_of_items                        :8;
            //  } descr_extended_event_t;
            //#define CastExtendedEventdescr(x) ((descr_extended_event_t *)(x))
            //#define ITEM_EXTENDED_EVENT_LEN 1
            //typedef struct item_extended_event_struct {
            //  unsigned char item_description_length               :8;
            //  } item_extended_event_t;
            //#define CastExtendedEventItem(x) ((item_extended_event_t *)(x))

            #ifdef EIT_EXTENDED_EVENT_DEBUG
            printf ("EIT extendedEvent sid:%d descLen:%d lastDescNum:%d DescNum:%d item:%d\n",
                    sid, GetDescrLength (ptr),
                    CastExtendedEventDescr(ptr)->last_descr_number,
                    CastExtendedEventDescr(ptr)->descr_number,
                    CastExtendedEventDescr(ptr)->length_of_items);

            for (int i = 0; i < GetDescrLength (ptr) -2; i++) {
              char c = *(ptr + 2 + i);
              if ((c >= 0x20) && (c <= 0x7F))
                printf ("%c", c);
              else
                printf("<%02x> ", c & 0xFF);
              }
            printf ("\n");
            #endif
            break;
            }
            //}}}
          case DESCR_COMPONENT:      // 0x50
          case DESCR_CA_IDENT:       // 0x53
          case DESCR_CONTENT:        // 0x54
          case 0x5f:                 // private_data
          case DESCR_ML_COMPONENT:   // 0x5e
          case DESCR_DATA_BROADCAST: // 0x64
          case 0x76:                 // content_identifier
          case 0x7e:                 // FTA_content_management
            //printf ("EIT - expected tag:%x %d %d\n", GetDescrTag(ptr), tid, sid);
            break;
          case 0x4a:                 // linkage
          case 0x89:                 // user_defined
          default:
            //printf ("EIT - unexpected tag:%x %d %d\n", GetDescrTag(ptr), tid, sid);
            break;
          }

        DescrLength += GetDescrLength (ptr);
        ptr += GetDescrLength (ptr);
        }

      sectionLength -= loopLength + EIT_EVENT_LEN;
      ptr += loopLength;
      }
    }
  //}}}
  //{{{
  void parseSection (int pid, unsigned char* buf, unsigned char* bufEnd) {

    switch (pid) {
      case PID_PAT: parsePat (buf, bufEnd); break;
      case PID_NIT: parseNit (buf, bufEnd); break;
      case PID_SDT: parseSdt (buf, bufEnd); break;
      case PID_EIT: parseEit (buf, bufEnd); break;
      case PID_TDT: parseTdt (buf, bufEnd); break;
      default:      parsePmt (pid, buf, bufEnd); break;
      }
    }
  //}}}

  // public
  //{{{
  void renderPidInfo (ID2D1DeviceContext* d2dContext, D2D1_SIZE_F client,
                      IDWriteTextFormat* textFormat,
                      ID2D1SolidColorBrush* whiteBrush,
                      ID2D1SolidColorBrush* blueBrush,
                      ID2D1SolidColorBrush* blackBrush,
                      ID2D1SolidColorBrush* greyBrush) {

    if (!mPidInfoMap.empty()) {
      // title
      wchar_t wStr[200];
      swprintf (wStr, 200, L"%ls %ls services:%d", wTimeStr, wNetworkNameStr, (int)mServiceMap.size());

      D2D1_RECT_F textRect = D2D1::RectF(0.0f, 0.0f, (float)1024.0f, (float)client.height);
      d2dContext->DrawText (wStr, (UINT32)wcslen(wStr), textFormat, textRect, whiteBrush);

      float top = 20.0f;
      for (auto pidInfo : mPidInfoMap) {
        float total = (float)pidInfo.second.mTotal;
        if (total > mLargest)
          mLargest = total;
        float len = (total/mLargest) * (client.width-50.0f);
        D2D1_RECT_F r = D2D1::RectF(40.0f, top+3.0f, 40.0f + len, top+17.0f);
        d2dContext->FillRectangle (r, blueBrush);

        textRect.top = top;
        swprintf (wStr, 200, L"%4d %d %ls%d", pidInfo.first, pidInfo.second.mStreamType, pidInfo.second.mInfo, pidInfo.second.mTotal);
        d2dContext->DrawText (wStr, (UINT32)wcslen(wStr), textFormat, textRect, whiteBrush);

        top += 14.0f;
        }
      }
    }
  //}}}
  //{{{
  void printServices() {

    printf ("--- ServiceMap -----\n");
    for (auto service : mServiceMap)
      service.second.print();
    }
  //}}}
  //{{{
  void printPrograms() {

    printf ("--- ProgramMap -----\n");
    for (auto map : mProgramMap)
      printf ("- programPid:%d sid:%d\n", map.first, map.second);
    }
  //}}}
  //{{{
  void printPids() {

    printf ("--- PidInfoMap -----\n");
    for (auto pidInfo : mPidInfoMap)
      pidInfo.second.print();
    }
  //}}}

  tProgramMap mProgramMap;
  tServiceMap mServiceMap;

private:
  //{{{
  void recordChange (int sid) {

    tServiceMap::iterator it = mServiceMap.find (sid);
    if (it != mServiceMap.end()) {
      tm curTime = *localtime (&mCurTime);
      wchar_t fileName[100];
      std::locale loc1 ("English");
      const char* str1 = it->second.getNow()->mTitle;
      wchar_t str2[100];
      std::use_facet <std::ctype <wchar_t> >(loc1).widen (str1, str1 + strlen(str1), &str2[0]);
      str2[strlen(str1)] = '\0';

      // cull illegal chars
      for (unsigned int i = 0; i < strlen(str1); i++)
        if ((str2[i] == ':') || (str2[i] == '?') || (str2[i] == '\'?'))
          str2[i] = ' ';

      // form filename from now name amnd time
      swprintf (fileName, 100,  L"c:/%s %02d-%02d-%02d %02d-%02d-%04d.ts",
                str2,
                curTime.tm_hour, curTime.tm_min, curTime.tm_sec,
                curTime.tm_mday, curTime.tm_mon+1, curTime.tm_year + 1900);

      // save fileName root
      printf ("recordChange %ls\n", fileName);
      }
    }
  //}}}
  //{{{
  void updatePidInfo (int pid) {
  // update pid cPidInfo UI text for speed, lock avoidance

    // cPidInfo from pid using mPidInfoMap
    tPidInfoMap::iterator pidInfoIt = mPidInfoMap.find (pid);
    if (pidInfoIt != mPidInfoMap.end()) {

      // cService from cPidInfo.sid using mServiceMap
      tServiceMap::iterator serviceIt = mServiceMap.find (pidInfoIt->second.mSid);
      if (serviceIt != mServiceMap.end()) {

        size_t size = 0;
        wchar_t name[100];
        mbstowcs_s (&size, name,
                    strlen (serviceIt->second.getName())+1,
                    serviceIt->second.getName(), _TRUNCATE);

        wchar_t title[100];
        mbstowcs_s (&size, title,
                    strlen (serviceIt->second.getNow()->mTitle)+1,
                    serviceIt->second.getNow()->mTitle, _TRUNCATE);

        if (pid == serviceIt->second.getVidPid())
          swprintf (pidInfoIt->second.mInfo, 100, L"vid %ls %ls ", name, title);
        else if (pid == serviceIt->second.getAudPid())
          swprintf (pidInfoIt->second.mInfo, 100, L"aud %ls %ls ", name, title);
        else if (pid == serviceIt->second.getSubPid())
          swprintf (pidInfoIt->second.mInfo, 100, L"sub %ls %ls ", name, title);
        else if (pid == serviceIt->second.getProgramPid())
          swprintf (pidInfoIt->second.mInfo, 100, L"pgm %ls ", name);
        }
      }
    }
  //}}}

  //{{{
  unsigned long crc32 (unsigned char* data, int len) {

    unsigned long crc = 0xffffffff;
    for (int i = 0; i < len; i++)
      crc = (crc << 8) ^ crcTable[((crc >> 24) ^ *data++) & 0xff];

    return crc;
    }
  //}}}
  //{{{
  void parsePat (unsigned char* buf, unsigned char* bufEnd) {
  // PAT declares programPid,sid to mPogramMap, recognieses programPid PMT to declare service streams

    pat_t* Pat = (pat_t*)buf;
    if ((bufEnd != 0) && (buf + HILO(Pat->section_length) + 3 >= bufEnd)) {
       //{{{  length problem
       printf ("PAT - length > buffer  %d\n", HILO(Pat->section_length) + 3);
       return;
       }
       //}}}
    if (crc32 (buf, HILO(Pat->section_length) + 3)) {
      //{{{  bad crc
      printf ("PAT - bad crc %d\n", HILO(Pat->section_length) + 3);
      return;
      }
      //}}}
    if (Pat->table_id != TID_PAT) {
      //{{{  wrong tid
      printf ("PAT - unexpected TID %x\n", Pat->table_id);
      return;
      }
      //}}}

    //int tsid = HILO (Pat->transport_stream_id);
    //printf ("PAT tsid:%d\n", tsid);

    unsigned char* ptr = buf + PAT_LEN;
    int sectionLength = HILO(Pat->section_length) + 3 - PAT_LEN - 4;
    while (sectionLength > 0) {
      pat_prog_t* patProgram = (pat_prog_t*)ptr;
      int sid = HILO (patProgram->program_number);
      int pid = HILO (patProgram->network_pid);
      if (pid != 0) {
        tProgramMap::iterator it = mProgramMap.find (pid);
        if (it == mProgramMap.end()) {
          mProgramMap.insert (tProgramMap::value_type (pid, sid));
          printf ("PAT new program pid:%d sid:%d\n", pid, sid);
          }
        }
      sectionLength -= PAT_PROG_LEN;
      ptr += PAT_PROG_LEN;
      }
    }
  //}}}
  //{{{
  void parseNit (unsigned char* buf, unsigned char* bufEnd) {

    nit_t* Nit = (nit_t*)buf;

    if ((bufEnd != 0) && (buf + HILO(Nit->section_length) + 3 >= bufEnd)) {
      //{{{
      printf ("parseNit - length > buffer %d\n", HILO(Nit->section_length) + 3);
      return;
      }
      //}}}
    if (crc32 (buf, HILO (Nit->section_length) + 3)) {
      //{{{  bad crc
      printf ("parseNit - bad crc %d\n", HILO (Nit->section_length) + 3);
      return;
      }
      //}}}
    if ((Nit->table_id != TID_NIT_ACT) &&
        (Nit->table_id != TID_NIT_OTH) &&
        (Nit->table_id != TID_BAT)) {
      //{{{  wrong tid
      printf ("parseNIT - wrong TID %x\n", Nit->table_id);
      return;
      }
      //}}}

    int networkId = HILO (Nit->network_id);

    unsigned char* ptr = buf + NIT_LEN;
    int loopLength = HILO (Nit->network_descr_length);
    int sectionLength = HILO (Nit->section_length) + 3 - NIT_LEN - 4;
    if (loopLength <= sectionLength) {
      if (sectionLength >= 0)
        parseDescrs ("NIT1", networkId, ptr, loopLength, Nit->table_id);
      sectionLength -= loopLength;

      ptr += loopLength;
      nit_mid_t* NitMid = (nit_mid_t*) ptr;
      loopLength = HILO (NitMid->transport_stream_loop_length);
      if ((sectionLength > 0) && (loopLength <= sectionLength)) {
        // iterate nitMids
        sectionLength -= SIZE_NIT_MID;
        ptr += SIZE_NIT_MID;

        while (loopLength > 0) {
          nit_ts_t* TSDesc = (nit_ts_t*)ptr;
          int tsid = HILO (TSDesc->transport_stream_id);
          int onid = HILO (TSDesc->original_network_id);

          int loop2Length = HILO (TSDesc->transport_descrs_length);
          ptr += NIT_TS_LEN;
          if (loop2Length <= loopLength)
            if (loopLength >= 0)
              parseDescrs ("NIT2", tsid, ptr, loop2Length, Nit->table_id);

          loopLength -= loop2Length + NIT_TS_LEN;
          sectionLength -= loop2Length + NIT_TS_LEN;
          ptr += loop2Length;
          }
        }
      }
    }
  //}}}
  //{{{
  void parseSdt (unsigned char* buf, unsigned char* bufEnd) {
  // SDT add new services to mServiceMap declaring serviceType, name, provider

    sdt_t* Sdt = (sdt_t*)buf;
    if ((bufEnd != 0) && (buf + HILO(Sdt->section_length) + 3 >= bufEnd)) {
      //{{{  length > buffer
      printf ("Sdt - length > buffer %d\n", HILO(Sdt->section_length) + 3);
      return;
      }
      //}}}
    if (crc32 (buf, HILO (Sdt->section_length) + 3)) {
      //{{{  wrong crc
      printf ("Sdt - bad crc %d\n", HILO (Sdt->section_length) + 3);
      return;
      }
      //}}}
    if (Sdt->table_id == TID_SDT_OTH) // ignore other multiplex services for now
      return;
    if (Sdt->table_id != TID_SDT_ACT) {
      //{{{  wrong tid
      printf ("SDT - unexpected TID %x\n", Sdt->table_id);
      return;
      }
      //}}}

    //printf ("SDT - tsid:%d onid:%d\n", tsid, onid);

    unsigned char* ptr = buf + SDT_LEN;
    int sectionLength = HILO (Sdt->section_length) + 3 - SDT_LEN - 4;
    while (sectionLength > 0) {
      sdt_descr_t* SdtDescr = (sdt_descr_t*)ptr;
      int sid = HILO (SdtDescr->service_id);
      int free_ca_mode = SdtDescr->free_ca_mode;
      int running_status = SdtDescr->running_status;

      int loopLength = HILO (SdtDescr->descrs_loop_length);
      ptr += SDT_DESCR_LEN;

      int DescrLength = 0;
      while ((DescrLength < loopLength) &&
             (GetDescrLength (ptr) > 0) &&
             (GetDescrLength (ptr) <= loopLength - DescrLength)) {

        switch (GetDescrTag (ptr)) {
          case DESCR_SERVICE: { // 0x48
            //{{{  service
            int serviceType = CastServiceDescr(ptr)->service_type;
            if ((free_ca_mode == 0) &&
                ((serviceType == kServiceTypeTV)  ||
                 (serviceType == kServiceTypeRadio) ||
                 (serviceType == kServiceTypeAdvancedHDTV) ||
                 (serviceType == kServiceTypeAdvancedSDTV))) {

              tServiceMap::iterator it = mServiceMap.find (sid);
              if (it == mServiceMap.end()) {
                // new service
                int tsid = HILO (Sdt->transport_stream_id);
                int onid = HILO (Sdt->original_network_id);

                char provider[100];
                getDescrName (ptr + DESCR_SERVICE_LEN,
                              CastServiceDescr(ptr)->provider_name_length, provider);
                char name[100];
                getDescrName (
                  ptr + DESCR_SERVICE_LEN + CastServiceDescr(ptr)->provider_name_length + 1,
                  *((unsigned char*)(ptr + DESCR_SERVICE_LEN + CastServiceDescr(ptr)->provider_name_length)),
                  name);

                // insert new cService, get serviceIt iterator
                std::pair<tServiceMap::iterator, bool> resultPair =
                  mServiceMap.insert (
                    tServiceMap::value_type (sid, cService (sid, tsid, onid, serviceType, provider, name)));
                tServiceMap::iterator serviceIt = resultPair.first;
                printf ("SDT new cService tsid:%d sid:%d %s name<%s> provider<%s>\n",
                        tsid, sid, serviceIt->second.getTypeStr(), name, provider);
                }
              }

            break;
            }
            //}}}
          case DESCR_PRIV_DATA_SPEC:
          case DESCR_CA_IDENT:
          case DESCR_COUNTRY_AVAIL:
          case DESCR_DATA_BROADCAST:
          case 0x73: // default_authority
          case 0x7e: // FTA_content_management
          case 0x7f: // extension
            break;
          default:
            //{{{  other
            printf ("SDT - unexpected tag:%x\n", GetDescrTag(ptr));
            break;
            //}}}
          }

        DescrLength += GetDescrLength (ptr);
        ptr += GetDescrLength (ptr);
        }

      sectionLength -= loopLength + SDT_DESCR_LEN;
      }
    }
  //}}}
  //{{{
  void parseTdt (unsigned char* buf, unsigned char* bufEnd) {

    tdt_t* Tdt = (tdt_t*)buf;
    if (Tdt->table_id == TID_TDT) {
      mCurTime = MjdToEpochTime (Tdt->utc_mjd) + BcdTimeToSeconds (Tdt->utc_time);

      struct tm when = *localtime (&mCurTime);
      size_t size = 0;
      mbstowcs_s (&size, wTimeStr, 25, asctime (&when), _TRUNCATE);
      }
    }
  //}}}
  //{{{
  void parsePmt (int pid, unsigned char* buf, unsigned char* bufEnd) {
  // PMT declares streams for a service

    pmt_t* pmt = (pmt_t*)buf;
    if ((bufEnd != 0) && (buf + HILO(pmt->section_length) + 3 >= bufEnd)) {
      //{{{  sectionLength > buffer
      printf ("parsepmt - pid:%d length > buffer %d\n", pid, HILO(pmt->section_length) + 3);
      return;
      }
      //}}}
    if (crc32 (buf, HILO (pmt->section_length) + 3)) {
      //{{{  bad crc
      printf ("parsepmt - pid:%d bad crc %d\n", pid, HILO(pmt->section_length) + 3);
      return;
      }
      //}}}
    if (pmt->table_id != TID_PMT) {
      //{{{  wrong tid
      printf ("parsepmt - wrong TID %x\n", pmt->table_id);
      return;
      }
      //}}}

    int sid = HILO (pmt->program_number);
    //printf ("PMT - pid:%d sid:%d\n", pid, sid);

    tServiceMap::iterator serviceIt = mServiceMap.find (sid);
    if (serviceIt != mServiceMap.end()) {
      // service declared by SMT
      serviceIt->second.setProgramPid (pid);

      // point programPid to service by sid
      tPidInfoMap::iterator sectionIt = mPidInfoMap.find (pid);
      if (sectionIt != mPidInfoMap.end())
        sectionIt->second.mSid = sid;
      updatePidInfo (pid);

      serviceIt->second.setPcrPid (HILO (pmt->PCR_PID));

      unsigned char* ptr = buf + PMT_LEN;
      int sectionLength = HILO (pmt->section_length) + 3 - 4;
      int programInfoLength = HILO (pmt->program_info_length);

      int streamLength = sectionLength - programInfoLength - PMT_LEN;
      if (streamLength >= 0)
        parseDescrs ("pmt1", sid, ptr, programInfoLength, pmt->table_id);

      ptr += programInfoLength;
      while (streamLength > 0) {
        pmt_info_t* pmtInfo = (pmt_info_t*)ptr;
        int streamType = pmtInfo->stream_type;
        int esPid = HILO (pmtInfo->elementary_PID);

        bool recognised = true;
        switch (streamType) {
          case 2:  serviceIt->second.setVidPid (esPid); break; // ISO 13818-2 video
          case 3:  serviceIt->second.setAudPid (esPid); break; // ISO 11172-3 audio
          case 4:  serviceIt->second.setAudPid (esPid); break; // ISO 13818-3 audio
          case 6:  serviceIt->second.setSubPid (esPid); break; // subtitle
          case 17: serviceIt->second.setAudPid (esPid); break; // HD aud
          case 27: serviceIt->second.setVidPid (esPid); break; // HD vid
          case 5:  recognised = false; break;
          case 11: recognised = false; break;
          case 13:
          default:
            printf ("PMT - unknown streamType:%d sid:%d esPid:%d\n", streamType, sid, esPid);
            recognised = false;
            break;
          }

        if (recognised) {
          // set sid for each stream pid
          tPidInfoMap::iterator sectionIt = mPidInfoMap.find (esPid);
          if (sectionIt != mPidInfoMap.end())
            sectionIt->second.mSid = sid;
          sectionIt->second.mStreamType = streamType;
          updatePidInfo (esPid);
          }

        int loopLength = HILO (pmtInfo->ES_info_length);
        parseDescrs ("pmt2", sid, ptr, loopLength, pmt->table_id);

        ptr += PMT_INFO_LEN;
        streamLength -= loopLength + PMT_INFO_LEN;
        ptr += loopLength;
        }
      }
    }
  //}}}

  //{{{
  void getDescrText (unsigned char* buf, int len, char* text) {

    for (int i = 0; i < len; i++) {
      if (*buf == 0)
        break;

      if ((*buf >= ' ' && *buf <= '~') || (*buf == '\n') || (*buf >= 0xa0 && *buf <= 0xff))
        *text++ = *buf;

      if (*buf == 0x8A)
        *text++ = '\n';

      if ((*buf == 0x86 || *buf == 0x87))
        *text++ = ' ';

      buf++;
      }

    *text = '\0';
    }
  //}}}
  //{{{
  void getDescrName (unsigned char* buf, int len, char* name) {

    for (int i = 0; i < len; i++) {
      if (*buf == 0)
        break;

      if ((*buf >= ' ' && *buf <= '~') || (*buf == '\n') || (*buf >= 0xa0 && *buf <= 0xff))
        *name++ = *buf;

      if (*buf == 0x8A)
        *name++ = '\n';

      buf++;
      }

    *name = '\0';
    }
  //}}}
  //{{{
  void parseDescr (char* sectionName, int key, unsigned char* buf, int tid) {

    switch (GetDescrTag(buf)) {
      case 0x25: // NIT2
        break;
      case DESCR_EXTENDED_EVENT: {
        //{{{  extended event
        char Text[400];
        char Text2[400];

        getDescrText (buf + DESCR_EXTENDED_EVENT_LEN + CastExtendedEventDescr(buf)->length_of_items + 1,
                      *((unsigned char*)(buf + DESCR_EXTENDED_EVENT_LEN + CastExtendedEventDescr(buf)->length_of_items)),
                      Text);
        printf ("extended event - %d %d %c%c%c %s\n",
          CastExtendedEventDescr(buf)->descr_number,
          CastExtendedEventDescr(buf)->last_descr_number,
          CastExtendedEventDescr(buf)->lang_code1,
          CastExtendedEventDescr(buf)->lang_code2,
          CastExtendedEventDescr(buf)->lang_code3,
          Text);

        unsigned char* ptr = buf + DESCR_EXTENDED_EVENT_LEN;
        int length = CastExtendedEventDescr(buf)->length_of_items;
        while ((length > 0) && (length < GetDescrLength (buf))) {
          getDescrText (ptr + ITEM_EXTENDED_EVENT_LEN,
                        CastExtendedEventItem(ptr)->item_description_length, Text);
          getDescrText (ptr + ITEM_EXTENDED_EVENT_LEN + CastExtendedEventItem(ptr)->item_description_length + 1,
                        *((unsigned char* )(ptr + ITEM_EXTENDED_EVENT_LEN + CastExtendedEventItem(ptr)->item_description_length)),
                        Text2);
          printf ("- %s %s\n", Text, Text2);

          length -= ITEM_EXTENDED_EVENT_LEN + CastExtendedEventItem(ptr)->item_description_length +
                      *((unsigned char* )(ptr + ITEM_EXTENDED_EVENT_LEN +
                      CastExtendedEventItem(ptr)->item_description_length)) + 1;
          ptr += ITEM_EXTENDED_EVENT_LEN + CastExtendedEventItem(ptr)->item_description_length +
                  *((unsigned char* )(ptr + ITEM_EXTENDED_EVENT_LEN +
                   CastExtendedEventItem(ptr)->item_description_length)) + 1;
          }

        break;
        }
        //}}}
      case DESCR_COMPONENT: {
        //{{{  component
        char Text[400];
        getDescrText (buf + DESCR_COMPONENT_LEN, GetDescrLength (buf) - DESCR_COMPONENT_LEN, Text);
        //printf ("component %2d %2d %2x %c%c%c %s\n",
        //        CastComponentDescr(buf)->stream_content,
        //        CastComponentDescr(buf)->component_type,
        //        CastComponentDescr(buf)->component_tag,
        //        Text);
        break;
        }
        //}}}
      case DESCR_NW_NAME: {
        //{{{  networkName
        char networkName [40];
        getDescrName (buf + DESCR_BOUQUET_NAME_LEN,
                      GetDescrLength (buf) - DESCR_BOUQUET_NAME_LEN, networkName);
        size_t size = 0;
        mbstowcs_s (&size, wNetworkNameStr, strlen (networkName) + 1, networkName, _TRUNCATE);

        break;
        }
        //}}}
      case DESCR_TERR_DEL_SYS: {
        //{{{  terr del sys
        descr_terrestrial_delivery_system_t* tds = (descr_terrestrial_delivery_system_t*)buf;
        // ((tds->frequency1 << 24) | (tds->frequency2 << 16) |
        //  (tds->frequency3 << 8)  | tds->frequency4) / 100,
        //   tds->bandwidth)));
        break;
        }
        //}}}
      case DESCR_SERVICE_LIST: {
        //{{{  service list
        unsigned char* ptr = buf;
        int length = GetDescrLength(buf);

        while (length > 0) {
          int sid = HILO (CastServiceListDescrLoop(ptr)->service_id);
          int serviceType = CastServiceListDescrLoop(ptr)->service_type;
          //printf ("serviceList sid:%5d type:%d\n", sid, serviceType);
          length -= DESCR_SERVICE_LIST_LEN;
          ptr += DESCR_SERVICE_LIST_LEN;
          }

        break;
        }
        //}}}
      //{{{  expected Descrs
      case DESCR_COUNTRY_AVAIL:
      case DESCR_CONTENT:
      case DESCR_CA_IDENT:
      case DESCR_ML_COMPONENT:
      case DESCR_DATA_BROADCAST:
      case DESCR_PRIV_DATA_SPEC:
      case DESCR_SYSTEM_CLOCK:
      case DESCR_MULTIPLEX_BUFFER_UTIL:
      case DESCR_MAXIMUM_BITRATE:
      case DESCR_SMOOTHING_BUFFER:
      case DESCR_FREQUENCY_LIST:  // NIT1
      case DESCR_LINKAGE:
      case 0x73:
      case 0x7F:                  // NIT2
      case 0x83:
      case 0xC3:
        break;
      //}}}
      default:
        printf ("parseDescr - %s unexpected descr:%02X\n", sectionName, GetDescrTag(buf));
        break;
      }
    }
  //}}}
  //{{{
  void parseDescrs (char* sectionName, int key, unsigned char* buf, int len, unsigned char tid) {

   unsigned char* ptr = buf;
   int DescrLength = 0;

   while (DescrLength < len) {
     if ((GetDescrLength (ptr) <= 0) || (GetDescrLength (ptr) > len - DescrLength))
       return;

      parseDescr (sectionName, key, ptr, tid);

      DescrLength += GetDescrLength (ptr);
      ptr += GetDescrLength (ptr);
      }
    }
  //}}}

  // vars
  time_t mCurTime;
  wchar_t wTimeStr[25];
  wchar_t wNetworkNameStr[20];

  float mLargest = 10000.0f;

  tPidInfoMap mPidInfoMap;
  };
