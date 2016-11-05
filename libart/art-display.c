#include "windows.h"
#include "stdio.h"
#include "art_misc.h"
#include "art_svp.h"
#include "art_vpath.h"
#include "art_svp_vpath.h"
#include "art_rgb_svp.h"
#include "art_rgb.h"
#include "art_vpath_dash.h"
#include "art_svp_vpath_stroke.h"

#define WIDTH 20
#define HEIGHT 20
#define BYTES_PER_PIXEL 3 /* 24 packed rgb bits */
#define ROWSTRIDE (WIDTH*BYTES_PER_PIXEL)


static ArtSVP *make_path (void);
static unsigned char *render_path (const ArtSVP *path);

static unsigned char * render_path (const ArtSVP *path) {
	art_u8 *buffer = NULL;
	art_u32 color = (0xFF << 24) | (0x80 <<16) | (0x40<<8) | (0xFF) ; /* RRGGBBAA */


	buffer = art_new (art_u8, WIDTH*HEIGHT*BYTES_PER_PIXEL);
  memset(buffer, 0, WIDTH*HEIGHT*BYTES_PER_PIXEL);
	art_rgb_run_alpha (buffer, 0xFF, 0xFF, 0xFF, 0xFF, WIDTH*HEIGHT);
	art_rgb_svp_alpha (path, 0, 0, WIDTH, HEIGHT, color, buffer, ROWSTRIDE, NULL);

	return (unsigned char *) buffer;
}

static ArtSVP * make_path (void) {
	ArtVpath *vec = NULL;
	ArtSVP *svp = NULL;

	vec = art_new (ArtVpath, 10);
	vec[0].code = ART_MOVETO;
	vec[0].x = 0;
	vec[0].y = 0;
	vec[1].code = ART_LINETO;
	vec[1].x = 8;
  vec[1].y = 2;
	vec[2].code = ART_LINETO;
	vec[2].x = 10;
	vec[2].y = 10;
	vec[3].code = ART_LINETO;
	vec[3].x = 3;
	vec[3].y = 10;
	vec[4].code = ART_END;

	svp = art_svp_from_vpath (vec);

	return svp;
}


int main (int argc, char *argv[])
{
	ArtSVP *path;
	char *buffer;

	path = make_path ();
	buffer = render_path (path);
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) 
			printf ("%02x%02x%02x ", *buffer++ & 0xFF, *buffer++ & 0xFF, *buffer++ & 0xFF);
	 printf ("\n");
	 }

	Sleep(10000);
	return 0;
}







