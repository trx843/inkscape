#define __NR_ARENA_IMAGE_C__

/*
 * RGBA display list system for inkscape
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <math.h>
#include <libnr/nr-rect.h>
#include <libnr/nr-matrix.h>
#include <libnr/nr-point-matrix-ops.h>
#include <libnr/nr-compose-transform.h>
#include "../prefs-utils.h"
#include "nr-arena-image.h"

int nr_arena_image_x_sample = 1;
int nr_arena_image_y_sample = 1;

/*
 * NRArenaCanvasImage
 *
 */

static void nr_arena_image_class_init (NRArenaImageClass *klass);
static void nr_arena_image_init (NRArenaImage *image);
static void nr_arena_image_finalize (NRObject *object);

static unsigned int nr_arena_image_update (NRArenaItem *item, NRRectL *area, NRGC *gc, unsigned int state, unsigned int reset);
static unsigned int nr_arena_image_render (NRArenaItem *item, NRRectL *area, NRPixBlock *pb, unsigned int flags);
static NRArenaItem *nr_arena_image_pick (NRArenaItem *item, NR::Point p, double delta, unsigned int sticky);

static NRArenaItemClass *parent_class;

NRType
nr_arena_image_get_type (void)
{
	static NRType type = 0;
	if (!type) {
		type = nr_object_register_type (NR_TYPE_ARENA_ITEM,
						"NRArenaImage",
						sizeof (NRArenaImageClass),
						sizeof (NRArenaImage),
						(void (*) (NRObjectClass *)) nr_arena_image_class_init,
						(void (*) (NRObject *)) nr_arena_image_init);
	}
	return type;
}

static void
nr_arena_image_class_init (NRArenaImageClass *klass)
{
	NRObjectClass *object_class;
	NRArenaItemClass *item_class;

	object_class = (NRObjectClass *) klass;
	item_class = (NRArenaItemClass *) klass;

	parent_class = (NRArenaItemClass *) ((NRObjectClass *) klass)->parent;

	object_class->finalize = nr_arena_image_finalize;
	object_class->cpp_ctor = NRObject::invoke_ctor<NRArenaImage>;

	item_class->update = nr_arena_image_update;
	item_class->render = nr_arena_image_render;
	item_class->pick = nr_arena_image_pick;
}

static void
nr_arena_image_init (NRArenaImage *image)
{
	image->px = NULL;

	image->pxw = image->pxh = image->pxrs = 0;
	image->x = image->y = 0.0;
	image->width = 256.0;
	image->height = 256.0;

	nr_matrix_set_identity (&image->grid2px);
}

static void
nr_arena_image_finalize (NRObject *object)
{
	NRArenaImage *image = NR_ARENA_IMAGE (object);

	image->px = NULL;

	((NRObjectClass *) parent_class)->finalize (object);
}

static unsigned int
nr_arena_image_update (NRArenaItem *item, NRRectL *area, NRGC *gc, unsigned int state, unsigned int reset)
{
	NRMatrix grid2px;

	NRArenaImage *image = NR_ARENA_IMAGE (item);

	/* Request render old */
	nr_arena_item_request_render (item);

	/* Copy affine */
	nr_matrix_invert (&grid2px, &gc->transform);
	double hscale, vscale; // todo: replace with NR::scale
	if (image->px) {
		hscale = image->pxw / image->width;
		vscale = image->pxh / image->height;
	} else {
		hscale = 1.0;
		vscale = 1.0;
	}

	image->grid2px[0] = grid2px.c[0] * hscale;
	image->grid2px[2] = grid2px.c[2] * hscale;
	image->grid2px[4] = grid2px.c[4] * hscale;
	image->grid2px[1] = grid2px.c[1] * vscale;
	image->grid2px[3] = grid2px.c[3] * vscale;
	image->grid2px[5] = grid2px.c[5] * vscale;

	image->grid2px[4] -= image->x * hscale;
	image->grid2px[5] -= image->y * vscale;

	/* Calculate bbox */
	if (image->px) {
		NRRect bbox;

		bbox.x0 = image->x;
		bbox.y0 = image->y;
		bbox.x1 = image->x + image->width;
		bbox.y1 = image->y + image->height;
		nr_rect_d_matrix_transform (&bbox, &bbox, &gc->transform);

		item->bbox.x0 = (int) floor (bbox.x0);
		item->bbox.y0 = (int) floor (bbox.y0);
		item->bbox.x1 = (int) ceil (bbox.x1);
		item->bbox.y1 = (int) ceil (bbox.y1);
	} else {
		item->bbox.x0 = (int) gc->transform[4];
		item->bbox.y0 = (int) gc->transform[5];
		item->bbox.x1 = item->bbox.x0 - 1;
		item->bbox.y1 = item->bbox.y0 - 1;
	}

	nr_arena_item_request_render (item);

	return NR_ARENA_ITEM_STATE_ALL;
}

#define FBITS 12
#define b2i (image->grid2px)

static unsigned int
nr_arena_image_render (NRArenaItem *item, NRRectL *area, NRPixBlock *pb, unsigned int flags)
{
	nr_arena_image_x_sample = prefs_get_int_attribute ("options.bitmapoversample", "value", 1);
	nr_arena_image_y_sample = nr_arena_image_x_sample;

	NRArenaImage *image = NR_ARENA_IMAGE (item);

	if (!image->px) return item->state;

	guint32 Falpha = item->opacity;
	if (Falpha < 1) return item->state;

	unsigned char * dpx = NR_PIXBLOCK_PX (pb);
	const int drs = pb->rs;
	const int dw = pb->area.x1 - pb->area.x0;
	const int dh = pb->area.y1 - pb->area.y0;

	unsigned char * spx = image->px;
	const int srs = image->pxrs;
	const int sw = image->pxw;
	const int sh = image->pxh;

	NR::Matrix d2s;

	d2s[0] = b2i[0];
	d2s[1] = b2i[1];
	d2s[2] = b2i[2];
	d2s[3] = b2i[3];
	d2s[4] = b2i[0] * pb->area.x0 + b2i[2] * pb->area.y0 + b2i[4];
	d2s[5] = b2i[1] * pb->area.x0 + b2i[3] * pb->area.y0 + b2i[5];

	if (pb->mode == NR_PIXBLOCK_MODE_R8G8B8) {
		/* fixme: This is not implemented yet (Lauris) */
		/* nr_R8G8B8_R8G8B8_R8G8B8A8_N_TRANSFORM (dpx, dw, dh, drs, spx, sw, sh, srs, d2s, Falpha, nr_arena_image_x_sample, nr_arena_image_y_sample); */
	} else if (pb->mode == NR_PIXBLOCK_MODE_R8G8B8A8P) {
		nr_R8G8B8A8_P_R8G8B8A8_P_R8G8B8A8_N_TRANSFORM (dpx, dw, dh, drs, spx, sw, sh, srs, d2s, Falpha, nr_arena_image_x_sample, nr_arena_image_y_sample);
	} else if (pb->mode == NR_PIXBLOCK_MODE_R8G8B8A8N) {

		//FIXME: The _N_N_N_ version gives a gray border around images, see bug 906376
		// This mode is only used when exporting, screen rendering always has _P_P_P_, so I decided to simply replace it for now
		// Feel free to propose a better fix

		//nr_R8G8B8A8_N_R8G8B8A8_N_R8G8B8A8_N_TRANSFORM (dpx, dw, dh, drs, spx, sw, sh, srs, d2s, Falpha, nr_arena_image_x_sample, nr_arena_image_y_sample);
		nr_R8G8B8A8_P_R8G8B8A8_P_R8G8B8A8_N_TRANSFORM (dpx, dw, dh, drs, spx, sw, sh, srs, d2s, Falpha, nr_arena_image_x_sample, nr_arena_image_y_sample);
	}

	pb->empty = FALSE;

	return item->state;
}

static NRArenaItem *
nr_arena_image_pick (NRArenaItem *item, NR::Point p, double delta, unsigned int sticky)
{
	NRArenaImage *image = NR_ARENA_IMAGE (item);

	if (!image->px) return NULL;

	unsigned char * const pixels = image->px;
	const int width = image->pxw;
	const int height = image->pxh;
	const int rowstride = image->pxrs;
	NR::Point tp = p * image->grid2px;
	const int ix = (int)(tp[NR::X]);
	const int iy = (int)(tp[NR::Y]);

	if ((ix < 0) || (iy < 0) || (ix >= width) || (iy >= height))
		return NULL;

	unsigned char *pix_ptr = pixels + iy * rowstride + ix * 4;
	// is the alpha not transparent?
	return (pix_ptr[3] > 0) ? item : NULL;
}

/* Utility */

void
nr_arena_image_set_pixels (NRArenaImage *image, const unsigned char *px, unsigned int pxw, unsigned int pxh, unsigned int pxrs)
{
	nr_return_if_fail (image != NULL);
	nr_return_if_fail (NR_IS_ARENA_IMAGE (image));

	image->px = (unsigned char *) px;
	image->pxw = pxw;
	image->pxh = pxh;
	image->pxrs = pxrs;

	nr_arena_item_request_update (NR_ARENA_ITEM (image), NR_ARENA_ITEM_STATE_ALL, FALSE);
}

void
nr_arena_image_set_geometry (NRArenaImage *image, double x, double y, double width, double height)
{
	nr_return_if_fail (image != NULL);
	nr_return_if_fail (NR_IS_ARENA_IMAGE (image));

	image->x = x;
	image->y = y;
	image->width = width;
	image->height = height;

	nr_arena_item_request_update (NR_ARENA_ITEM (image), NR_ARENA_ITEM_STATE_ALL, FALSE);
}

