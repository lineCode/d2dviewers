#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rgb_svp.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_vpath_dash.h>
#include <libart_lgpl/art_svp_vpath_stroke.h>

#define WIDTH 100
#define HEIGHT 100
#define BYTES_PER_PIXEL 3 /* 24 packed rgb bits */
#define ROWSTRIDE (WIDTH*BYTES_PER_PIXEL)


static ArtSVP *make_path (void);
static unsigned char *render_path (const ArtSVP *path);
static void build_widget (unsigned char *buffer);

static void destroy_cb (GtkWidget *widget, gpointer data);
static int expose_cb (GtkWidget *widget, GdkEventExpose *evt, gpointer data);


static int
expose_cb (GtkWidget *widget, GdkEventExpose *evt, gpointer data)
{
	art_u8 *buf = (art_u8 *)data;

	gdk_draw_rgb_image (widget->window, widget->style->black_gc,
					0, 0, WIDTH, HEIGHT,
					GDK_RGB_DITHER_NONE,
					buf,
					ROWSTRIDE);
	return FALSE;
}

static void
destroy_cb (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

static void
build_widget (unsigned char *buffer)
{
	GtkWidget *window = NULL, *drawing_area = NULL;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW(window), WIDTH, HEIGHT);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
					GTK_SIGNAL_FUNC (destroy_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
					GTK_SIGNAL_FUNC (destroy_cb), NULL);
	drawing_area = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER (window),
				 GTK_WIDGET (drawing_area));

	gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
					GTK_SIGNAL_FUNC (expose_cb), buffer);
	gtk_signal_connect (GTK_OBJECT (drawing_area), "configure_event",
					GTK_SIGNAL_FUNC (expose_cb), buffer);

	gtk_widget_show_all (window);
}

static unsigned char *
render_path (const ArtSVP *path)
{
	art_u8 *buffer = NULL;
	art_u32 color = (0xFF << 24) | (0x00 <<16) | (0x00<<8) | (0xFF) ; /* RRGGBBAA */


	buffer = art_new (art_u8, WIDTH*HEIGHT*BYTES_PER_PIXEL);
	art_rgb_run_alpha (buffer, 0xFF, 0xFF, 0xFF, 0xFF, WIDTH*HEIGHT);
	art_rgb_svp_alpha (path, 0, 0, WIDTH, HEIGHT,
				 color, buffer, ROWSTRIDE, NULL);

	return (unsigned char *) buffer;
}

static ArtSVP *
make_path (void)
{
	ArtVpath *vec = NULL;
	ArtSVP *svp = NULL;

	vec = art_new (ArtVpath, 10);
	vec[0].code = ART_MOVETO;
	vec[0].x = 0;
	vec[0].y = 0;
	vec[1].code = ART_LINETO;
	vec[1].x = 0;
	vec[1].y = 10;
	vec[2].code = ART_LINETO;
	vec[2].x = 10;
	vec[2].y = 10;
	vec[3].code = ART_LINETO;
	vec[3].x = 10;
	vec[3].y = 0;
	vec[4].code = ART_END;

	svp = art_svp_from_vpath (vec);

	return svp;
}


int main (int argc, char *argv[])
{
	ArtSVP *path;
	char *buffer;

	/* gtk/gdkrgb initialization */
	gtk_init (&argc, &argv);
	gdk_rgb_init ();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

	path = make_path ();
	buffer = render_path (path);

	build_widget (buffer);

	/* gtk main loop */
	gtk_main ();

	return 0;
}







