#include <stdio.h>
#include <string.h>
#include <math.h>
#include "art_misc.h"
#include "art_vpath.h"
#include "art_svp.h"
#include "art_svp_vpath.h"
#include "art_gray_svp.h"
#include "art_rgb_svp.h"
#include "art_svp_vpath_stroke.h"
#include "art_svp_ops.h"
#include "art_affine.h"
#include "art_rgb_affine.h"
#include "art_rgb_bitmap_affine.h"
#include "art_rgb_rgba_affine.h"
#include "art_alphagamma.h"
#include "art_svp_point.h"
#include "art_vpath_dash.h"
#include "art_render.h"
#include "art_render_gradient.h"
#include "art_render_svp.h"
#include "art_svp_intersect.h"


static unsigned char * render_path (const ArtSVP *path) {
  art_u8 *buffer = NULL;
  art_u32 color = (0xC0 << 24) | (0x80 <<16) | (0x40<<8) | (0xFF) ; /* RRGGBBAA */

  //buffer = art_new (art_u8, WIDTH*HEIGHT*BYTES_PER_PIXEL);
  //art_rgb_run_alpha (buffer, 0xFF, 0xFF, 0xFF, 0xFF, WIDTH*HEIGHT);
  //art_rgb_svp_alpha (path, 0, 0, WIDTH, HEIGHT, color, buffer, ROWSTRIDE, NULL);
  buffer = art_new (art_u8, 20*20*4);
  art_rgb_run_alpha (buffer, 0xFF, 0xFF, 0xFF, 0xFF, 20*20);
  art_rgb_svp_alpha (path, 0, 0, 20, 20, color, buffer, 20*4, NULL);

  return (unsigned char*) buffer;
}


static ArtSVP * make_path (void) {

  ArtVpath *vec = NULL;
  ArtSVP *svp = NULL;

  vec = art_new (ArtVpath, 10);
  vec[0].code = ART_MOVETO;
  vec[0].x = 0;
  vec[0].y = 0;

  vec[1].code = ART_LINETO;
  vec[1].x = 20;
  vec[1].y = 20;

  vec[2].code = ART_LINETO;
  vec[2].x = 0;
  vec[2].y = 20;

  vec[3].code = ART_LINETO;
  vec[3].x = 0;
  vec[3].y = 0;

  vec[4].code = ART_END;

  svp = art_svp_from_vpath (vec);

  return svp;
}



int main (int argc, char *argv[]) {
  ArtSVP *path;
  char *buffer;

  path = make_path ();

  buffer = render_path (path);
  for (int y = 0; y < 20; y++) {
    for (int x = 0; x < 20; x++) {
      printf ("%02x:%02x:%02x:%02x ", *buffer++ & 0xFF, *buffer++ & 0xFF, *buffer++ & 0xFF, *buffer++ & 0xFF);
      }
    printf ("\n");
    }
  //save_buffer (buffer, "foo.png");
  Sleep (1000);
  return 0;
}
