#define __SP_ICON_C__

/*
 * Generic icon widget
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2002 Lauris Kaplinski
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "path-prefix.h"

#include <stdlib.h>
#include <string.h>
#include <libnr/nr-macros.h>
#include <libnr/nr-rect.h>
#include <libnr/nr-matrix.h>
#include <libnr/nr-pixblock.h>
#include <libnr/nr-pixblock-pattern.h>
#include <libnr/nr-pixops.h>

#include <map>

#include <gtk/gtkbutton.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstock.h>


#include "forward.h"
#include "prefs-utils.h"
#include "inkscape-private.h"
#include "inkscape.h"
#include "document.h"
#include "sp-item.h"
#include "display/nr-arena.h"
#include "display/nr-arena-item.h"
#include "io/sys.h"

#include "icon.h"

static void sp_icon_class_init (SPIconClass *klass);
static void sp_icon_init (SPIcon *icon);
static void sp_icon_destroy (GtkObject *object);

static void sp_icon_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void sp_icon_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static int sp_icon_expose (GtkWidget *widget, GdkEventExpose *event);

static void sp_icon_paint(SPIcon *icon, GdkRectangle const *area);

static guchar *sp_icon_image_load_pixmap (const gchar *name, unsigned int lsize, unsigned int psize);
static guchar *sp_icon_image_load_svg( const gchar *name, unsigned int lsize, unsigned int psize );

static guchar *sp_icon_image_load ( SPIcon* icon, const gchar *name );
static guchar *sp_icon_image_load_gtk( SPIcon* icon, const gchar *name );

static int sp_icon_get_phys_size( int size );

static void sp_icon_overlay_pixels( guchar *px, int width, int height, int stride, unsigned int r, unsigned int g, unsigned int b );

static GtkWidgetClass *parent_class;

GtkType
sp_icon_get_type (void)
{
	static GtkType type = 0;
	if (!type) {
		GtkTypeInfo info = {
			"SPIcon",
			sizeof (SPIcon),
			sizeof (SPIconClass),
			(GtkClassInitFunc) sp_icon_class_init,
			(GtkObjectInitFunc) sp_icon_init,
			NULL, NULL, NULL
		};
		type = gtk_type_unique (GTK_TYPE_WIDGET, &info);
	}
	return type;
}

static void
sp_icon_class_init (SPIconClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = (GtkWidgetClass*)g_type_class_peek_parent (klass);

	object_class->destroy = sp_icon_destroy;

	widget_class->size_request = sp_icon_size_request;
	widget_class->size_allocate = sp_icon_size_allocate;
	widget_class->expose_event = sp_icon_expose;
}

static void
sp_icon_init (SPIcon *icon)
{
	GTK_WIDGET_FLAGS (icon) |= GTK_NO_WINDOW;
}

static void
sp_icon_destroy (GtkObject *object)
{
	SPIcon *icon;

	icon = SP_ICON (object);

	if (icon->pb) {
		g_object_unref(G_OBJECT(icon->pb));
		icon->pb = NULL;
	}
	if (icon->pb_faded) {
		g_object_unref(G_OBJECT(icon->pb_faded));
		icon->pb_faded = NULL;
	}

	((GtkObjectClass *) (parent_class))->destroy (object);
}

static void
sp_icon_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	SPIcon *icon;

	icon = SP_ICON (widget);

	requisition->width = icon->psize;
	requisition->height = icon->psize;
}

static void
sp_icon_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	widget->allocation = *allocation;

	if (GTK_WIDGET_DRAWABLE (widget)) {
		gtk_widget_queue_draw (widget);
	}
}

static int sp_icon_expose(GtkWidget *widget, GdkEventExpose *event)
{
	if (GTK_WIDGET_DRAWABLE (widget)) {
		sp_icon_paint (SP_ICON (widget), &event->area);
	}

	return TRUE;
}

static GtkWidget *
sp_icon_new_full( GtkIconSize lsize, const gchar *name )
{
	SPIcon *icon;
	guchar *pixels;


	icon = (SPIcon *)g_object_new (SP_TYPE_ICON, NULL);

	icon->lsize = lsize;
	icon->psize = sp_icon_get_phys_size(lsize);

        //g_warning ("loading '%s' (%d:%d)", name, icon->psize, scale);
	pixels = sp_icon_image_load_gtk( icon, name );

	if (pixels) {
		// don't pass the nr_free because we're caching the pixel
		// space loaded through sp_icon_image_load_gtk
		icon->pb = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, TRUE, 8, icon->psize, icon->psize, icon->psize * 4, /*(GdkPixbufDestroyNotify)nr_free*/NULL, NULL);
		icon->pb_faded = gdk_pixbuf_copy(icon->pb);

		pixels = gdk_pixbuf_get_pixels(icon->pb_faded);
		size_t stride = gdk_pixbuf_get_rowstride(icon->pb_faded);
		pixels += 3; // alpha
		for ( int row = 0 ; row < icon->psize ; row++ ) {
			guchar *row_pixels=pixels;
			for ( int column = 0 ; column < icon->psize ; column++ )
			{
				*row_pixels = *row_pixels >> 1;
				row_pixels += 4;
			}
			pixels += stride;
		}
	}
	else {
	    /* we should do something more useful if we can't load the image */
            g_warning ("failed to load icon '%s'", name);
	}

	return (GtkWidget *) icon;
}

GtkWidget *
sp_icon_new( GtkIconSize lsize, const gchar *name )
{
        return sp_icon_new_full( lsize, name );
}

GtkWidget *
sp_icon_new_scaled( GtkIconSize lsize, const gchar *name )
{
	return sp_icon_new_full( lsize, name );
}

// Try to load the named svg, falling back to pixmaps
guchar *
sp_icon_image_load( SPIcon* icon, const gchar *name )
{
	guchar *px;

	px = sp_icon_image_load_svg( name, icon->lsize, icon->psize );
	if (!px) px = sp_icon_image_load_pixmap (name, icon->lsize, icon->psize);

	return px;
}

GtkIconSize
sp_icon_get_gtk_size (int size)
{
	static GtkIconSize map[64] = {(GtkIconSize)0};
	size = CLAMP (size, 4, 63);
	if (!map[size]) {
		static int count = 0;
		char c[64];
		g_snprintf (c, 64, "InkscapeIcon%d", count++);
		map[size] = gtk_icon_size_register (c, size, size);
	}
	return map[size];
}

static int sp_icon_get_phys_size(int size)
{
    static bool init = false;
    static int vals[GTK_ICON_SIZE_DIALOG + 1];

    size = CLAMP( size, GTK_ICON_SIZE_MENU, GTK_ICON_SIZE_DIALOG );

    if ( !init ) {
        gint dump = prefs_get_int_attribute_limited( "debug.icons", "dumpDefault", 0, 0, 1 );
        if ( dump ) {
            g_message( "Default icon sizes:" );
        }
        memset( vals, 0, sizeof(vals) );
        GtkIconSize const gtkSizes[] = {
            GTK_ICON_SIZE_MENU,
            GTK_ICON_SIZE_SMALL_TOOLBAR,
            GTK_ICON_SIZE_LARGE_TOOLBAR,
            GTK_ICON_SIZE_BUTTON,
            GTK_ICON_SIZE_DND,
            GTK_ICON_SIZE_DIALOG
        };
        gchar const *const names[] = {
            "GTK_ICON_SIZE_MENU",
            "GTK_ICON_SIZE_SMALL_TOOLBAR",
            "GTK_ICON_SIZE_LARGE_TOOLBAR",
            "GTK_ICON_SIZE_BUTTON",
            "GTK_ICON_SIZE_DND",
            "GTK_ICON_SIZE_DIALOG"
        };

        GtkWidget *icon = (GtkWidget *)g_object_new (SP_TYPE_ICON, NULL);

        for (unsigned i = 0; i < G_N_ELEMENTS(gtkSizes); ++i) {
            unsigned const val_ix(gtkSizes[i]);
            g_assert( val_ix < G_N_ELEMENTS(vals) );

            gint width = 0;
            gint height = 0;
            bool used = false;
            if ( gtk_icon_size_lookup(gtkSizes[i], &width, &height ) ) {
                vals[val_ix] = std::max(width, height);
                used = true;
            }
            if (dump) {
                g_message(" =--  %u  size:%d  %c(%d, %d)   '%s'",
                          i, gtkSizes[i],
                          ( used ? ' ' : 'X' ), width, height, names[i]);
            }
            gchar const *id = GTK_STOCK_OPEN;
            GdkPixbuf *pb = gtk_widget_render_icon( icon, id, gtkSizes[i], NULL);
            if (pb) {
                width = gdk_pixbuf_get_width(pb);
                height = gdk_pixbuf_get_height(pb);
                int newSize = std::max( width, height );
                // TODO perhaps check a few more stock icons to get a range on sizes.
                if ( newSize > 0 ) {
                    vals[val_ix] = newSize;
                }
                if (dump) {
                    g_message("      %u  size:%d   (%d, %d)", i, gtkSizes[i], width, height);
                }

                g_object_unref(G_OBJECT(pb));
            }
        }
        //g_object_unref(icon);
        init = true;
    }

    return vals[size];
}

guchar *
sp_icon_image_load_gtk( SPIcon* icon, const gchar *name )
{
    gint dump = prefs_get_int_attribute_limited( "debug.icons", "dumpGtk", 0, 0, 1 );

    /* fixme: Make stock/nonstock configurable */
    if (!strncmp (name, "gtk-", 4)) {
        GtkWidget * host = gtk_button_new();
        if ( dump ) {
            g_message( "loading gtk '%s' %d:%d", name, icon->lsize, icon->psize );
        }
        GdkPixbuf *pb;
        guchar *px, *spx; // pixel data is unsigned
        int srs;
        int y;
        pb = gtk_widget_render_icon( host, name, icon->lsize, NULL );

        int width = gdk_pixbuf_get_width(pb);
        int height = gdk_pixbuf_get_height(pb);
        if ( dump ) {
            g_message( "          --'%s'  (%d,%d)", name, width, height );
        }
        icon->psize = std::max( width, height );

        if (!gdk_pixbuf_get_has_alpha (pb)) gdk_pixbuf_add_alpha (pb, FALSE, 0, 0, 0);
        spx = gdk_pixbuf_get_pixels (pb);
        srs = gdk_pixbuf_get_rowstride (pb);
        size_t howBig = 4 * icon->psize * icon->psize;
        px = nr_new (guchar, howBig);
        memset( px, 0, howBig );
        int dstStride = 4 * icon->psize;
        for (y = 0; y < height; y++) {
            memcpy( px + y * dstStride, spx + y * srs, 4 * width );
        }

        gint useOverlay = prefs_get_int_attribute_limited( "debug.icons", "overlayGtk", 0, 0, 1 );
        if ( useOverlay ) {
            sp_icon_overlay_pixels( px, icon->psize, icon->psize, 4 * icon->psize, 0xff, 0x00, 0xff );
        }

        g_object_unref ((GObject *) pb);
        return px;
    } else {
        return sp_icon_image_load( icon, name );
    }
}

static void sp_icon_paint(SPIcon *icon, GdkRectangle const *area)
{
	GtkWidget &widget = *GTK_WIDGET(icon);

	GdkPixbuf *image = GTK_WIDGET_IS_SENSITIVE(&widget) ? icon->pb : icon->pb_faded;

	if (image) {
		int const padx = ( ( widget.allocation.width > icon->psize )
				   ? ( widget.allocation.width - icon->psize ) / 2
				   : 0 );
		int const pady = ( ( widget.allocation.height > icon->psize )
				   ? ( widget.allocation.height - icon->psize ) / 2
				   : 0 );

		int const x0 = std::max(area->x, widget.allocation.x + padx);
		int const y0 = std::max(area->y, widget.allocation.y + pady);
		int const x1 = std::min(area->x + area->width,  widget.allocation.x + padx + static_cast<int>(icon->psize) );
		int const y1 = std::min(area->y + area->height, widget.allocation.y + pady + static_cast<int>(icon->psize) );

		gdk_draw_pixbuf(GDK_DRAWABLE(widget.window), NULL, image,
				x0 - widget.allocation.x - padx,
				y0 - widget.allocation.y - pady,
				x0, y0,
				x1 - x0, y1 - y0,
				GDK_RGB_DITHER_NORMAL, x0, y0);
	}
}

static guchar *
sp_icon_image_load_pixmap (const gchar *name, unsigned int lsize, unsigned int psize)
{
	gchar *path;
	guchar *px;
	GdkPixbuf *pb;

	path = (gchar *) g_strdup_printf ("%s/%s.png", INKSCAPE_PIXMAPDIR, name);
	// TODO: bulia, please look over
	gsize bytesRead = 0;
	gsize bytesWritten = 0;
	GError* error = NULL;
	gchar* localFilename = g_filename_from_utf8 ( path,
												  -1,
												  &bytesRead,
												  &bytesWritten,
												  &error);
	pb = gdk_pixbuf_new_from_file (localFilename, NULL);
	g_free (localFilename);
	g_free (path);
	if (!pb) {
		path = (gchar *) g_strdup_printf ("%s/%s.xpm", INKSCAPE_PIXMAPDIR, name);
		// TODO: bulia, please look over
		gsize bytesRead = 0;
		gsize bytesWritten = 0;
		GError* error = NULL;
		gchar* localFilename = g_filename_from_utf8 ( path,
													  -1,
													  &bytesRead,
													  &bytesWritten,
													  &error);
		pb = gdk_pixbuf_new_from_file (localFilename, NULL);
		g_free (localFilename);
		g_free (path);
	}
	if (pb) {
		guchar *spx;
		int srs;
		unsigned int y;
		if (!gdk_pixbuf_get_has_alpha (pb))
			gdk_pixbuf_add_alpha (pb, FALSE, 0, 0, 0);
		if (  (static_cast< unsigned int > (gdk_pixbuf_get_width(pb))  != psize)
			||(static_cast< unsigned int > (gdk_pixbuf_get_height(pb)) != psize)) {
			GdkPixbuf *spb;
			spb = gdk_pixbuf_scale_simple (pb, psize, psize, GDK_INTERP_HYPER);
			g_object_unref (G_OBJECT (pb));
			pb = spb;
		}
		spx = gdk_pixbuf_get_pixels (pb);
		srs = gdk_pixbuf_get_rowstride (pb);
		px = nr_new (guchar, 4 * psize * psize);
		for (y = 0; y < psize; y++) {
			memcpy (px + 4 * y * psize, spx + y * srs, 4 * psize);
		}
		g_object_unref (G_OBJECT (pb));

		return px;
	}

	return NULL;
}

// takes doc, root, icon, and icon name to produce pixels
static guchar *
sp_icon_doc_icon( SPDocument *doc, NRArenaItem *root,
                  const gchar *name, unsigned int lsize, unsigned int psize )
{
    gint dump = prefs_get_int_attribute_limited( "debug.icons", "dumpSvg", 0, 0, 1 );
    guchar *px = NULL;

    if (doc) {
        SPObject *object;
        object = doc->getObjectById(name);
        if (object && SP_IS_ITEM (object)) {
            /* Find bbox in document */
            NR::Matrix const i2doc(sp_item_i2doc_affine(SP_ITEM(object)));
            NRRect dbox;
            sp_item_invoke_bbox(SP_ITEM(object), &dbox, i2doc, TRUE);

            /* This is in document coordinates, i.e. pixels */
            if (!nr_rect_d_test_empty (&dbox))
            {
                NRRectL ibox, area, ua;
                NRMatrix t;
                NRPixBlock B;
                NRGC gc(NULL);
                double sf;
                int width, height, dx, dy;
                /* Update to renderable state */
                sf = 1.0;
                nr_matrix_set_scale (&t, sf, sf);
                nr_arena_item_set_transform (root, &t);
                nr_matrix_set_identity (&gc.transform);
                nr_arena_item_invoke_update ( root, NULL, &gc,
                                              NR_ARENA_ITEM_STATE_ALL,
                                              NR_ARENA_ITEM_STATE_NONE );
                /* Item integer bbox in points */
                ibox.x0 = (int) floor (sf * dbox.x0 + 0.5);
                ibox.y0 = (int) floor (sf * dbox.y0 + 0.5);
                ibox.x1 = (int) floor (sf * dbox.x1 + 0.5);
                ibox.y1 = (int) floor (sf * dbox.y1 + 0.5);

                if ( dump ) {
                    g_message( "   box    --'%s'  (%f,%f)-(%f,%f)", name, (double)ibox.x0, (double)ibox.y0, (double)ibox.x1, (double)ibox.y1 );
                }

                /* Find button visible area */
                width = ibox.x1 - ibox.x0;
                height = ibox.y1 - ibox.y0;

                if ( dump ) {
                    g_message( "   vis    --'%s'  (%d,%d)", name, width, height );
                }

                {
                    int block = std::max(width, height);
                    if (block != static_cast<int>(psize) ) {
                        if ( dump ) {
                            g_message("      resizing" );
                        }
                        sf = (double)psize / (double)block;

                        nr_matrix_set_scale (&t, sf, sf);
                        nr_arena_item_set_transform (root, &t);
                        nr_matrix_set_identity (&gc.transform);
                        nr_arena_item_invoke_update ( root, NULL, &gc,
                                                      NR_ARENA_ITEM_STATE_ALL,
                                                      NR_ARENA_ITEM_STATE_NONE );
                        /* Item integer bbox in points */
                        ibox.x0 = (int) floor (sf * dbox.x0 + 0.5);
                        ibox.y0 = (int) floor (sf * dbox.y0 + 0.5);
                        ibox.x1 = (int) floor (sf * dbox.x1 + 0.5);
                        ibox.y1 = (int) floor (sf * dbox.y1 + 0.5);

                        if ( dump ) {
                            g_message( "   box2   --'%s'  (%f,%f)-(%f,%f)", name, (double)ibox.x0, (double)ibox.y0, (double)ibox.x1, (double)ibox.y1 );
                        }

                        /* Find button visible area */
                        width = ibox.x1 - ibox.x0;
                        height = ibox.y1 - ibox.y0;
                        if ( dump ) {
                            g_message( "   vis2   --'%s'  (%d,%d)", name, width, height );
                        }
                    }
                }

                //dx = (psize - width) / 2;
                //dy = (psize - height) / 2;
                dx=dy=psize;
                dx=(dx-width)/2; // watch out for psize, since 'unsigned'-'signed' can cause problems if the result is negative
                dy=(dy-height)/2;
                area.x0 = ibox.x0 - dx;
                area.y0 = ibox.y0 - dy;
                area.x1 = area.x0 + psize;
                area.y1 = area.y0 + psize;
                /* Actual renderable area */
                ua.x0 = MAX (ibox.x0, area.x0);
                ua.y0 = MAX (ibox.y0, area.y0);
                ua.x1 = MIN (ibox.x1, area.x1);
                ua.y1 = MIN (ibox.y1, area.y1);

                if ( dump ) {
                    g_message( "   area   --'%s'  (%f,%f)-(%f,%f)", name, (double)area.x0, (double)area.y0, (double)area.x1, (double)area.y1 );
                    g_message( "   ua     --'%s'  (%f,%f)-(%f,%f)", name, (double)ua.x0, (double)ua.y0, (double)ua.x1, (double)ua.y1 );
                }
                /* Set up pixblock */
                px = nr_new (guchar, 4 * psize * psize);
                memset (px, 0x00, 4 * psize * psize);
                /* Render */
                 nr_pixblock_setup_extern ( &B, NR_PIXBLOCK_MODE_R8G8B8A8N,
                                            ua.x0, ua.y0, ua.x1, ua.y1,
                                            px + 4 * psize * (ua.y0 - area.y0) +
                                            4 * (ua.x0 - area.x0),
                                            4 * psize, FALSE, FALSE );
                nr_arena_item_invoke_render ( root, &ua, &B,
                                              NR_ARENA_ITEM_RENDER_NO_CACHE );
                nr_pixblock_release (&B);

                gint useOverlay = prefs_get_int_attribute_limited( "debug.icons", "overlaySvg", 0, 0, 1 );
                if ( useOverlay ) {
                    sp_icon_overlay_pixels( px, psize, psize, 4 * psize, 0x00, 0x00, 0xff );
                }
            }
        }
    }

    return px;
} // end of sp_icon_doc_icon()



struct svg_doc_cache_t
{
    SPDocument *doc;
    NRArenaItem *root;
};


static guchar *
sp_icon_image_load_svg( const gchar *name, unsigned int lsize, unsigned int psize )
{
    gint const dump = prefs_get_int_attribute_limited("debug.icons", "dumpSvg", 0, 0, 1);

    // It would be nice to figure out how to attach "desctructors" to
    // these maps to keep mem-watching tools like valgrind happy.
    static std::map<Glib::ustring, svg_doc_cache_t *> doc_cache;
    static std::map<Glib::ustring, guchar *> px_cache;
    SPDocument *doc = NULL;
    NRArenaItem *root = NULL;
    svg_doc_cache_t * info = NULL;

    Glib::ustring icon_index = name;
    icon_index += ":";
    icon_index += lsize;
    icon_index += ":";
    icon_index += psize;

    // did we already load this icon at this scale/size?
    guchar *px = 0;
    {
        std::map<Glib::ustring, guchar *>::iterator i = px_cache.find(icon_index);
        if ( i != px_cache.end() ) {
            px = i->second;
        }
    }
    if (px) {
        if ( dump ) {
            g_message( "svg icon cached (%s).", icon_index.c_str() );
        }
        return px;
    }

    // Fall back from user prefs dir into system locations.
    Glib::ustring iconsvg = name;
    iconsvg += ".svg";
    std::list<gchar *> sources;
    sources.push_back(g_build_filename(profile_path("icons"),iconsvg.c_str(), NULL));
    sources.push_back(g_build_filename(profile_path("icons"),"icons.svg", NULL));
    sources.push_back(g_build_filename(INKSCAPE_PIXMAPDIR, iconsvg.c_str(), NULL));
    sources.push_back(g_build_filename(INKSCAPE_PIXMAPDIR, "icons.svg", NULL));

    // Use this loop to iterate through a list of possible document locations.
    while (!sources.empty()) {
        gchar *doc_filename = sources.front();

        // Did we already load this doc?
        Glib::ustring key(doc_filename);
        info = 0;
        {
            std::map<Glib::ustring, svg_doc_cache_t *>::iterator i = doc_cache.find(key);
            if ( i != doc_cache.end() ) {
                info = i->second;
            }
        }

        /* Try to load from document. */
        if (!info &&
            Inkscape::IO::file_test( doc_filename, G_FILE_TEST_IS_REGULAR ) &&
            (doc = sp_document_new ( doc_filename, FALSE )) ) {

            // prep the document
            sp_document_ensure_up_to_date (doc);
            /* Create new arena */
            NRArena *arena = NRArena::create();
            /* Create ArenaItem and set transform */
            unsigned int visionkey = sp_item_display_key_new (1);
            /* fixme: Memory manage root if needed (Lauris) */
            root = sp_item_invoke_show ( SP_ITEM (SP_DOCUMENT_ROOT (doc)),
                                         arena, visionkey, SP_ITEM_SHOW_DISPLAY );

            // store into the cache
            info = new svg_doc_cache_t;
            g_assert(info);

            info->doc=doc;
            info->root=root;
            doc_cache[key]=info;
        }
        if (info) {
            doc=info->doc;
            root=info->root;
        }

        // toss the filename
        g_free(doc_filename);
        sources.pop_front();

        // move on to the next document
        if (!info && !doc) continue;

        if ( dump ) {
            g_message( "loading svg '%s' (%d:%d)", name, lsize, psize );
        }
        px = sp_icon_doc_icon( doc, root, name, lsize, psize );
        if (px) {
            px_cache[icon_index] = px;
            break;
        }
    }

    return px;
} // end of sp_icon_image_load_svg()


PixBufFactory::ID::ID(Glib::ustring id, GtkIconSize size , unsigned int psize ):
    _id(id),
    _size(size),
    _psize( sp_icon_get_phys_size(psize) )
{
}


PixBufFactory::PixBufFactory()
{}

PixBufFactory & PixBufFactory::get()
{
  static PixBufFactory pbf;
  return pbf;
}

const Glib::RefPtr<Gdk::Pixbuf> PixBufFactory::getIcon(const Glib::ustring &oid)
{
    ID id (oid, GTK_ICON_SIZE_SMALL_TOOLBAR, sp_icon_get_phys_size( GTK_ICON_SIZE_SMALL_TOOLBAR ) );

    return getIcon(id);
}

const Glib::RefPtr<Gdk::Pixbuf> PixBufFactory::getIcon(const Glib::ustring &oid, GtkIconSize size) {
    ID id (oid, size, sp_icon_get_phys_size(size) );

    return getIcon(id);
}


const Glib::RefPtr<Gdk::Pixbuf> PixBufFactory::getIcon(const ID &id)
{
    Glib::RefPtr<Gdk::Pixbuf> inMap = _map[id];
    //cached, return
    if (inMap) return inMap;

    //not cached, loading
    int size(id.size());
    int psize = sp_icon_get_phys_size(size);
    gint dump = prefs_get_int_attribute_limited( "debug.icons", "dumpCache", 0, 0, 1 );
    if ( dump ) {
        g_message( "not cached, loading  '%s':%d:%d", id.id().c_str(), size, psize );
    }
    guchar *data = sp_icon_image_load_svg(id.id().c_str(), size, psize);
    Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gdk::Pixbuf::create_from_data (
            data,
            Gdk::COLORSPACE_RGB,
            true,
            8, psize, psize, psize * 4);
    _map[id]=pixbuf;
    return pixbuf;
}

void sp_icon_overlay_pixels( guchar *px, int width, int height, int stride, unsigned int r, unsigned int g, unsigned int b )
{
    for ( int y = 0; y < height; y += 4 ) {
        guchar *ptr = px + y * stride;
        for ( int x = 0; x < width; x += 4 ) {
            *(ptr++) = r;
            *(ptr++) = g;
            *(ptr++) = b;
            *(ptr++) = 0xff;

            ptr += 4 * 3;
        }
    }

    if ( width > 1 && height > 1 ) {
        // point at the last pixel
        guchar *ptr = px + ((height-1) * stride) + ((width - 1) * 4);

        if ( width > 2 ) {
            px[4] = r;
            px[5] = g;
            px[6] = b;
            px[7] = 0xff;

            ptr[-12] = r;
            ptr[-11] = g;
            ptr[-10] = b;
            ptr[-9] = 0xff;
        }

        ptr[-4] = r;
        ptr[-3] = g;
        ptr[-2] = b;
        ptr[-1] = 0xff;

        px[0 + stride] = r;
        px[1 + stride] = g;
        px[2 + stride] = b;
        px[3 + stride] = 0xff;

        ptr[0 - stride] = r;
        ptr[1 - stride] = g;
        ptr[2 - stride] = b;
        ptr[3 - stride] = 0xff;

        if ( height > 2 ) {
            ptr[0 - stride * 3] = r;
            ptr[1 - stride * 3] = g;
            ptr[2 - stride * 3] = b;
            ptr[3 - stride * 3] = 0xff;
        }
    }
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
