#define __SP_OBJECT_EDIT_C__

/*
 * Node editing extension to objects
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Mitsuru Oka
 *
 * Licensed under GNU GPL
 */

#include "config.h"

#include <gdk/gdktypes.h>
#include <glib.h>
#include <math.h>

#include "sp-item.h"
#include "sp-rect.h"
#include "sp-ellipse.h"
#include "sp-star.h"
#include "sp-spiral.h"
#include "sp-offset.h"
#include "sp-flowtext.h"
#include "prefs-utils.h"
#include "inkscape.h"
#include "snap.h"
#include "desktop-affine.h"
#include "desktop-handles.h"
#include "document.h"
#include <style.h>

#include "sp-pattern.h"
#include "sp-path.h"

#include "macros.h"
#include <glibmm/i18n.h>

#include "object-edit.h"

#include <libnr/nr-point-fns.h>
#include <libnr/nr-scale.h>
#include <libnr/nr-scale-ops.h>

#include <libnr/nr-matrix.h>
#include <libnr/nr-matrix-ops.h>
#include <libnr/nr-point-matrix-ops.h>

#include "xml/repr.h"

#include "isnan.h"

#define sp_round(v,m) (((v) < 0.0) ? ((ceil ((v) / (m) - 0.5)) * (m)) : ((floor ((v) / (m) + 0.5)) * (m)))

static SPKnotHolder *sp_rect_knot_holder (SPItem *item, SPDesktop *desktop);
static SPKnotHolder *sp_arc_knot_holder (SPItem *item, SPDesktop *desktop);
static SPKnotHolder *sp_star_knot_holder (SPItem *item, SPDesktop *desktop);
static SPKnotHolder *sp_spiral_knot_holder (SPItem * item, SPDesktop *desktop);
static SPKnotHolder *sp_offset_knot_holder (SPItem * item, SPDesktop *desktop);
static SPKnotHolder *sp_path_knot_holder (SPItem * item, SPDesktop *desktop);
static SPKnotHolder *sp_flowtext_knot_holder (SPItem * item, SPDesktop *desktop);
static void sp_pat_knot_holder (SPItem * item, SPKnotHolder *knot_holder);

SPKnotHolder *
sp_item_knot_holder (SPItem *item, SPDesktop *desktop)
{
	const gchar *name;
	name = sp_object_tagName_get ((SPObject *) item, NULL);
	if (SP_IS_RECT (item)) {
		return sp_rect_knot_holder (item, desktop);
	} else if (SP_IS_ARC (item)) {
		return sp_arc_knot_holder (item, desktop);
	} else if (SP_IS_STAR (item)) {
		return sp_star_knot_holder (item, desktop);
	} else if (SP_IS_SPIRAL (item)) {
		return sp_spiral_knot_holder (item, desktop);
	} else if (SP_IS_OFFSET (item)) {
		return sp_offset_knot_holder (item, desktop);
	} else if (SP_IS_PATH (item)) {
		return sp_path_knot_holder (item, desktop);
	} else if (SP_IS_FLOWTEXT (item) && SP_FLOWTEXT(item)->has_internal_frame()) {
		return sp_flowtext_knot_holder (item, desktop);
	}

	return NULL;
}


/* Pattern manipulation */

static gdouble sp_pattern_extract_theta (SPPattern *pat, gdouble scale)
{
    gdouble theta = asin (pat->patternTransform[1] / scale);
    if (pat->patternTransform[0]<0) theta = M_PI - theta ;
    return  theta;
}

static gdouble sp_pattern_extract_scale (SPPattern *pat)
{
    gdouble s = pat->patternTransform[1];
    gdouble c = pat->patternTransform[0];
    gdouble xscale = sqrt(c * c +s * s);
    return  xscale;
}

static NR::Point sp_pattern_extract_trans (const SPPattern *pat)
{
    return NR::Point(pat->patternTransform[4], pat->patternTransform[5]);
}

static void
sp_pattern_xy_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
    SPPattern *pat = SP_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style));

     if (state)  {
	 const NR::Point q = p - sp_pattern_extract_trans(pat);
         sp_item_adjust_pattern (item, NR::Matrix(NR::translate(q)));
     }

     item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}


static NR::Point sp_pattern_xy_get (SPItem *item)
{
    const SPPattern *pat = SP_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style));
    return sp_pattern_extract_trans(pat);

}

static NR::Point sp_pattern_angle_get (SPItem *item)
{
    SPPattern *pat = SP_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style));

    gdouble x = (pattern_width(pat)*0.5);
    gdouble y = 0;
    NR::Point delta = NR::Point(x,y);
    gdouble scale = sp_pattern_extract_scale(pat);
    gdouble theta = sp_pattern_extract_theta(pat, scale);
    delta = delta * NR::Matrix(NR::rotate(theta))*NR::Matrix(NR::scale(scale,scale));
    delta = delta + sp_pattern_extract_trans(pat);
    return  delta;

}

static void
sp_pattern_angle_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);

    SPPattern *pat = SP_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style));

    // get the angle from pattern 0,0 to the cursor pos
    NR::Point delta = p - sp_pattern_extract_trans(pat);
    gdouble theta = atan2 (delta );

	if ( state & GDK_CONTROL_MASK ) {
		theta = sp_round(theta, M_PI/snaps);
	}

    // get the scale from the current transform so we can keep it.
    gdouble scl = sp_pattern_extract_scale(pat);
    NR::Matrix rot =  NR::Matrix(NR::rotate(theta)) * NR::Matrix(NR::scale(scl,scl));
    const NR::Point t = sp_pattern_extract_trans(pat);
    rot[4] = t[NR::X];
    rot[5] = t[NR::Y];
    sp_item_adjust_pattern (item, rot, true);
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static void
sp_pattern_scale_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
    SPPattern *pat = SP_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style));

    // Get the scale from the position of the knotholder,
    NR::Point d = p - sp_pattern_extract_trans(pat);
    gdouble s = NR::L2(d);
    gdouble pat_x = pattern_width(pat) * 0.5;
    gdouble pat_y = pattern_height(pat) * 0.5;
    gdouble pat_h = hypot(pat_x, pat_y);
    gdouble scl = s / pat_h;

    // get angle from current transform, (need get current scale first to calculate angle)
    gdouble oldscale = sp_pattern_extract_scale(pat);
    gdouble theta = sp_pattern_extract_theta(pat,oldscale);

    NR::Matrix rot =  NR::Matrix(NR::rotate(theta)) * NR::Matrix(NR::scale(scl,scl));
    const NR::Point t = sp_pattern_extract_trans(pat);
    rot[4] = t[NR::X];
    rot[5] = t[NR::Y];
    sp_item_adjust_pattern (item, rot, true);
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);

}


static NR::Point sp_pattern_scale_get (SPItem *item)
{
    SPPattern *pat = SP_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style));

    gdouble x = pattern_width(pat)*0.5;
    gdouble y = pattern_height(pat)*0.5;
    NR::Point delta = NR::Point(x,y);
    NR::Matrix a = pat->patternTransform;
    a[4] = 0;
    a[5] = 0;
    delta = delta * a;
    delta = delta + sp_pattern_extract_trans(pat);
    return  delta;

}

/* SPRect */

static NR::Point sp_rect_rx_get(SPItem *item)
{
    SPRect *rect = SP_RECT(item);

    return NR::Point(rect->x.computed + rect->width.computed - rect->rx.computed, rect->y.computed);
}

static void sp_rect_rx_set(SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
    SPRect *rect = SP_RECT(item);

    if (state & GDK_CONTROL_MASK) {
        gdouble temp = MIN(rect->height.computed, rect->width.computed) / 2.0;
        rect->rx.computed = rect->ry.computed = CLAMP(rect->x.computed + rect->width.computed - p[NR::X], 0.0, temp);
        rect->rx.set = rect->ry.set = TRUE;
        
    } else {
        rect->rx.computed = CLAMP(rect->x.computed + rect->width.computed - p[NR::X], 0.0, rect->width.computed / 2.0);
        rect->rx.set = TRUE;
    }
    
    ((SPObject*)rect)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}


static NR::Point sp_rect_ry_get(SPItem *item)
{
    SPRect *rect = SP_RECT(item);

    return NR::Point(rect->x.computed + rect->width.computed, rect->y.computed + rect->ry.computed);
}

static void sp_rect_ry_set(SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
    SPRect *rect = SP_RECT(item);

    if (state & GDK_CONTROL_MASK) {
        
        gdouble temp = MIN(rect->height.computed, rect->width.computed) / 2.0;
        rect->rx.computed = rect->ry.computed = CLAMP(p[NR::Y] - rect->y.computed, 0.0, temp);
        rect->ry.set = rect->rx.set = TRUE;
        
    } else {
        
        if (!rect->rx.set || rect->rx.computed == 0) {
            
            rect->ry.computed = CLAMP(
                p[NR::Y] - rect->y.computed,
                0.0,
                MIN(rect->height.computed / 2.0, rect->width.computed / 2.0)
                );
            
        } else {
            
            rect->ry.computed = CLAMP(
                p[NR::Y] - rect->y.computed, 0.0,
                rect->height.computed / 2.0
                );
        }
            
        rect->ry.set = TRUE;
    }
        
    ((SPObject *)rect)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

/**
 *  Remove rounding from a rectangle.
 */
static void rect_remove_rounding(SPRect* rect)
{
    sp_repr_set_attr(SP_OBJECT_REPR(rect), "rx", NULL);
    sp_repr_set_attr(SP_OBJECT_REPR(rect), "ry", NULL);
}

/**
 *  Called when the horizontal rounding radius knot is clicked.
 */
static void sp_rect_rx_knot_click(SPItem *item, guint state)
{
    SPRect *rect = SP_RECT(item);
    
    if (state & GDK_SHIFT_MASK) {
        rect_remove_rounding(rect);
    } else if (state & GDK_CONTROL_MASK) {
        /* Ctrl-click sets the vertical rounding to be the same as the horizontal */
        sp_repr_set_attr(SP_OBJECT_REPR(rect), "ry", SP_OBJECT_REPR(rect)->attribute("rx"));
    }
}

/**
 *  Called when the vertical rounding radius knot is clicked.
 */
static void sp_rect_ry_knot_click(SPItem *item, guint state)
{
    SPRect *rect = SP_RECT(item);
    
    if (state & GDK_SHIFT_MASK) {
        rect_remove_rounding(rect);
    } else if (state & GDK_CONTROL_MASK) {
        /* Ctrl-click sets the vertical rounding to be the same as the horizontal */
        sp_repr_set_attr(SP_OBJECT_REPR(rect), "rx", SP_OBJECT_REPR(rect)->attribute("ry"));
    }
}

#define SGN(x) ((x)>0?1:((x)<0?-1:0))

static void sp_rect_clamp_radii(SPRect *rect)
{
    // clamp rounding radii so that they do not exceed width/height
    if (2 * rect->rx.computed > rect->width.computed) {
        rect->rx.computed = 0.5 * rect->width.computed;
        rect->rx.set = TRUE;
    }
    if (2 * rect->ry.computed > rect->height.computed) {
        rect->ry.computed = 0.5 * rect->height.computed;
        rect->ry.set = TRUE;
    }
}

static NR::Point sp_rect_wh_get(SPItem *item)
{
    SPRect *rect = SP_RECT(item);

    return NR::Point(rect->x.computed + rect->width.computed, rect->y.computed + rect->height.computed);
}

static NR::Point rect_snap_knot_position(NR::Point const &p)
{
    SPDesktop const *desktop = inkscape_active_desktop();
    NR::Point s = sp_desktop_dt2root_xy_point(desktop, p);
    namedview_free_snap_all_types(SP_DT_NAMEDVIEW(desktop), s);
    return sp_desktop_root2dt_xy_point(desktop, s);
}

static void sp_rect_wh_set_internal(SPRect *rect, const NR::Point &p, const NR::Point &origin, guint state)
{
    NR::Point const s = rect_snap_knot_position(p);
    
    if (state & GDK_CONTROL_MASK) {
        // original width/height when drag started
        gdouble const w_orig = (origin[NR::X] - rect->x.computed);
        gdouble const h_orig = (origin[NR::Y] - rect->y.computed);
        
        //original ratio
        gdouble const ratio = (w_orig / h_orig);
        
        // mouse displacement since drag started
        gdouble const minx = s[NR::X] - origin[NR::X];
        gdouble const miny = s[NR::Y] - origin[NR::Y];
        
        if (fabs(minx) > fabs(miny)) {

            // snap to horizontal or diagonal
            rect->width.computed = MAX(w_orig + minx, 0);
            if (minx != 0 && fabs(miny/minx) > 0.5 * 1/ratio && (SGN(minx) == SGN(miny))) {
                // closer to the diagonal and in same-sign quarters, change both using ratio
                rect->height.computed = MAX(h_orig + minx / ratio, 0);
            } else {
                // closer to the horizontal, change only width, height is h_orig
                rect->height.computed = MAX(h_orig, 0);
            }
            
        } else {
            // snap to vertical or diagonal
            rect->height.computed = MAX(h_orig + miny, 0);
            if (miny != 0 && fabs(minx/miny) > 0.5 * ratio && (SGN(minx) == SGN(miny))) {
                // closer to the diagonal and in same-sign quarters, change both using ratio
                rect->width.computed = MAX(w_orig + miny * ratio, 0);
            } else {
                // closer to the vertical, change only height, width is w_orig
                rect->width.computed = MAX(w_orig, 0);
            }
        }
        
        rect->width.set = rect->height.set = TRUE;

    } else {
        // move freely
        rect->width.computed = MAX(s[NR::X] - rect->x.computed, 0);
        rect->height.computed = MAX(s[NR::Y] - rect->y.computed, 0);
        rect->width.set = rect->height.set = TRUE;
    }
    
    sp_rect_clamp_radii(rect);
    
    ((SPObject *)rect)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static void sp_rect_wh_set(SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
    SPRect *rect = SP_RECT(item);

    sp_rect_wh_set_internal(rect, p, origin, state);
}

static NR::Point sp_rect_xy_get(SPItem *item)
{
    SPRect *rect = SP_RECT(item);

    return NR::Point(rect->x.computed, rect->y.computed);
}

static void sp_rect_xy_set(SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
    SPRect *rect = SP_RECT(item);

    // opposite corner (unmoved)
    gdouble opposite_x = (rect->x.computed + rect->width.computed);
    gdouble opposite_y = (rect->y.computed + rect->height.computed);
    
    // original width/height when drag started
    gdouble w_orig = opposite_x - origin[NR::X];
    gdouble h_orig = opposite_y - origin[NR::Y];

    NR::Point const s = rect_snap_knot_position(p);
    
    // mouse displacement since drag started
    gdouble minx = s[NR::X] - origin[NR::X];
    gdouble miny = s[NR::Y] - origin[NR::Y];
    
    if (state & GDK_CONTROL_MASK) {
        //original ratio
        gdouble ratio = (w_orig / h_orig);
        
        if (fabs(minx) > fabs(miny)) {

            // snap to horizontal or diagonal
            rect->x.computed = MIN(s[NR::X], opposite_x);
            rect->width.computed = MAX(w_orig - minx, 0);
            if (minx != 0 && fabs(miny/minx) > 0.5 * 1/ratio && (SGN(minx) == SGN(miny))) {
                // closer to the diagonal and in same-sign quarters, change both using ratio
                rect->y.computed = MIN(origin[NR::Y] + minx / ratio, opposite_y);
                rect->height.computed = MAX(h_orig - minx / ratio, 0);
            } else {
                // closer to the horizontal, change only width, height is h_orig
                rect->y.computed = MIN(origin[NR::Y], opposite_y);
                rect->height.computed = MAX(h_orig, 0);
            }
            
        } else {

            // snap to vertical or diagonal
            rect->y.computed = MIN(s[NR::Y], opposite_y);
            rect->height.computed = MAX(h_orig - miny, 0);
            if (miny != 0 && fabs(minx/miny) > 0.5 *ratio && (SGN(minx) == SGN(miny))) {
                // closer to the diagonal and in same-sign quarters, change both using ratio
                rect->x.computed = MIN(origin[NR::X] + miny * ratio, opposite_x);
                rect->width.computed = MAX(w_orig - miny * ratio, 0);
            } else {
                // closer to the vertical, change only height, width is w_orig
                rect->x.computed = MIN(origin[NR::X], opposite_x);
                rect->width.computed = MAX(w_orig, 0);
            }
            
        }

        rect->width.set = rect->height.set = rect->x.set = rect->y.set = TRUE;
        
    } else {
        // move freely
        rect->x.computed = MIN(s[NR::X], opposite_x);
        rect->width.computed = MAX(w_orig - minx, 0);
        rect->y.computed = MIN(s[NR::Y], opposite_y);
        rect->height.computed = MAX(h_orig - miny, 0);
        rect->width.set = rect->height.set = rect->x.set = rect->y.set = TRUE;
    }
    
    sp_rect_clamp_radii(rect);

    ((SPObject *)rect)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static SPKnotHolder *sp_rect_knot_holder(SPItem *item, SPDesktop *desktop)
{
    SPKnotHolder *knot_holder = sp_knot_holder_new(desktop, item, NULL);
    
    sp_knot_holder_add_full(
      knot_holder, sp_rect_rx_set, sp_rect_rx_get, sp_rect_rx_knot_click,
      SP_KNOT_SHAPE_CIRCLE, SP_KNOT_MODE_XOR,
      _("Adjust the <b>horizontal rounding</b> radius; with <b>Ctrl</b> to make the vertical "
	"radius the same"));

    sp_knot_holder_add_full(
      knot_holder, sp_rect_ry_set, sp_rect_ry_get, sp_rect_ry_knot_click,
      SP_KNOT_SHAPE_CIRCLE, SP_KNOT_MODE_XOR,
      _("Adjust the <b>vertical rounding</b> radius; with <b>Ctrl</b> to make the horizontal "
	"radius the same")
      );

    sp_knot_holder_add_full(
      knot_holder, sp_rect_wh_set, sp_rect_wh_get, NULL,
      SP_KNOT_SHAPE_SQUARE, SP_KNOT_MODE_XOR,
      _("Adjust the <b>width and height</b> of the rectangle; with <b>Ctrl</b> to lock ratio "
	"or stretch in one dimension only")
      );
    
    sp_knot_holder_add_full(
      knot_holder, sp_rect_xy_set, sp_rect_xy_get, NULL,
      SP_KNOT_SHAPE_SQUARE, SP_KNOT_MODE_XOR,
      _("Adjust the <b>width and height</b> of the rectangle; with <b>Ctrl</b> to lock ratio "
	"or stretch in one dimension only")
      );
    
    sp_pat_knot_holder(item, knot_holder);
    return knot_holder;
}

/* SPArc */

/*
 * return values:
 *   1  : inside
 *   0  : on the curves
 *   -1 : outside
 */
static gint
sp_genericellipse_side (SPGenericEllipse *ellipse, const NR::Point &p)
{
	gdouble dx = (p[NR::X] - ellipse->cx.computed) / ellipse->rx.computed;
	gdouble dy = (p[NR::Y] - ellipse->cy.computed) / ellipse->ry.computed;

	gdouble s = dx * dx + dy * dy;
	if (s < 1.0) return 1;
	if (s > 1.0) return -1;
	return 0;
}

static void
sp_arc_start_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);

	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);
	SPArc *arc = SP_ARC(item);

	ge->closed = (sp_genericellipse_side (ge, p) == -1) ? TRUE : FALSE;

	NR::Point delta = p - NR::Point(ge->cx.computed, ge->cy.computed);
	NR::scale sc(ge->rx.computed, ge->ry.computed);
	ge->start = atan2 (delta * sc.inverse());
	if ( ( state & GDK_CONTROL_MASK )
	     && snaps )
	{
		ge->start = sp_round(ge->start, M_PI/snaps);
	}
	sp_genericellipse_normalize (ge);
	((SPObject *)arc)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static NR::Point sp_arc_start_get (SPItem *item)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);
	SPArc *arc = SP_ARC (item);

	return sp_arc_get_xy (arc, ge->start);
}

static void
sp_arc_end_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);

	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);
	SPArc *arc = SP_ARC(item);

	ge->closed = (sp_genericellipse_side (ge, p) == -1) ? TRUE : FALSE;

	NR::Point delta = p - NR::Point(ge->cx.computed, ge->cy.computed);
	NR::scale sc(ge->rx.computed, ge->ry.computed);
	ge->end = atan2 (delta * sc.inverse());
	if ( ( state & GDK_CONTROL_MASK )
	     && snaps )
	{
		ge->end = sp_round(ge->end, M_PI/snaps);
	}
	sp_genericellipse_normalize (ge);
	((SPObject *)arc)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static NR::Point sp_arc_end_get (SPItem *item)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);
	SPArc *arc = SP_ARC (item);

	return sp_arc_get_xy (arc, ge->end);
}

static void
sp_arc_startend_click (SPItem *item, guint state)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);

	if (state & GDK_SHIFT_MASK) {
		ge->end = ge->start = 0;
		((SPObject *)ge)->updateRepr();
	}
}


static void
sp_arc_rx_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);
	SPArc *arc = SP_ARC(item);

	ge->rx.computed = fabs( ge->cx.computed - p[NR::X] );

	if ( state & GDK_CONTROL_MASK ) {
		ge->ry.computed = ge->rx.computed;
	}

	((SPObject *)arc)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static NR::Point sp_arc_rx_get (SPItem *item)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);

	return (NR::Point(ge->cx.computed, ge->cy.computed) -  NR::Point(ge->rx.computed, 0));
}

static void
sp_arc_ry_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);
	SPArc *arc = SP_ARC(item);

	ge->ry.computed = fabs( ge->cy.computed - p[NR::Y] );

	if ( state & GDK_CONTROL_MASK ) {
		ge->rx.computed = ge->ry.computed;
	}

	((SPObject *)arc)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static NR::Point sp_arc_ry_get (SPItem *item)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);

	return (NR::Point(ge->cx.computed, ge->cy.computed) -  NR::Point(0, ge->ry.computed));
}

static void
sp_arc_rx_click (SPItem *item, guint state)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);

	if (state & GDK_CONTROL_MASK) {
		ge->ry.computed = ge->rx.computed;
		((SPObject *)ge)->updateRepr();
	}
}

static void
sp_arc_ry_click (SPItem *item, guint state)
{
	SPGenericEllipse *ge = SP_GENERICELLIPSE (item);

	if (state & GDK_CONTROL_MASK) {
		ge->rx.computed = ge->ry.computed;
		((SPObject *)ge)->updateRepr();
	}
}

static SPKnotHolder *
sp_arc_knot_holder (SPItem *item, SPDesktop *desktop)
{
	SPKnotHolder *knot_holder = sp_knot_holder_new (desktop, item, NULL);

	sp_knot_holder_add_full (knot_holder, sp_arc_rx_set, sp_arc_rx_get, sp_arc_rx_click,
													 SP_KNOT_SHAPE_SQUARE, SP_KNOT_MODE_XOR,
					_("Adjust ellipse <b>width</b>, with <b>Ctrl</b> to make circle"));
	sp_knot_holder_add_full (knot_holder, sp_arc_ry_set, sp_arc_ry_get, sp_arc_ry_click,
													 SP_KNOT_SHAPE_SQUARE, SP_KNOT_MODE_XOR,
					_("Adjust ellipse <b>height</b>, with <b>Ctrl</b> to make circle"));
	sp_knot_holder_add_full (knot_holder, sp_arc_start_set, sp_arc_start_get, sp_arc_startend_click,
													 SP_KNOT_SHAPE_CIRCLE, SP_KNOT_MODE_XOR,
					_("Position the <b>start point</b> of the arc or segment; with <b>Ctrl</b> to snap angle; drag <b>inside</b> the ellipse for arc, <b>outside</b> for segment"));
	sp_knot_holder_add_full (knot_holder, sp_arc_end_set, sp_arc_end_get, sp_arc_startend_click,
													 SP_KNOT_SHAPE_CIRCLE, SP_KNOT_MODE_XOR,
					_("Position the <b>end point</b> of the arc or segment; with <b>Ctrl</b> to snap angle; drag <b>inside</b> the ellipse for arc, <b>outside</b> for segment"));

	sp_pat_knot_holder (item, knot_holder);

	return knot_holder;
}

/* SPStar */

static void
sp_star_knot1_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	SPStar *star = SP_STAR (item);

	NR::Point d = p - star->center;

	double arg1 = atan2(d);
	double darg1 = arg1 - star->arg[0];

	if (state & GDK_MOD1_MASK) {
		star->randomized = darg1/(star->arg[0] - star->arg[1]); 
	} else if (state & GDK_SHIFT_MASK) {
		star->rounded = darg1/(star->arg[0] - star->arg[1]); 
	} else if (state & GDK_CONTROL_MASK) {
		star->r[0]    = L2(d);
	} else {
		star->r[0]    = L2(d);
		star->arg[0]  = arg1;
		star->arg[1] += darg1;
	}
	((SPObject *)star)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static void
sp_star_knot2_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	SPStar *star = SP_STAR (item);
	if (star->flatsided == false) {
		NR::Point d = p - star->center;

		double arg1 = atan2(d);
		double darg1 = arg1 - star->arg[1];

		if (state & GDK_MOD1_MASK) {
			star->randomized = darg1/(star->arg[0] - star->arg[1]); 
		} else if (state & GDK_SHIFT_MASK) {
			star->rounded = fabs (darg1/(star->arg[0] - star->arg[1])); 
		} else if (state & GDK_CONTROL_MASK) {
			star->r[1]   = L2(d);
			star->arg[1] = star->arg[0] + M_PI / star->sides;
		}
		else {
			star->r[1]   = L2(d);
			star->arg[1] = atan2 (d);
		}
		((SPObject *)star)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
	}
}

static NR::Point sp_star_knot1_get (SPItem *item)
{
	g_assert (item != NULL);

	SPStar *star = SP_STAR(item);

	return sp_star_get_xy (star, SP_STAR_POINT_KNOT1, 0);

}

static NR::Point sp_star_knot2_get (SPItem *item)
{
	g_assert (item != NULL);

	SPStar *star = SP_STAR(item);

	return sp_star_get_xy (star, SP_STAR_POINT_KNOT2, 0);
}

static void
sp_star_knot_click (SPItem *item, guint state)
{
	SPStar *star = SP_STAR(item);

	if (state & GDK_MOD1_MASK) {
		star->randomized = 0;
		((SPObject *)star)->updateRepr();
	} else if (state & GDK_SHIFT_MASK) {
		star->rounded = 0;
		((SPObject *)star)->updateRepr();
	} else if (state & GDK_CONTROL_MASK) {
		star->arg[1] = star->arg[0] + M_PI / star->sides;
		((SPObject *)star)->updateRepr();
	}
}

static SPKnotHolder *
sp_star_knot_holder (SPItem *item, SPDesktop *desktop)
{
	/* we don't need to get parent knot_holder */
	SPKnotHolder *knot_holder = sp_knot_holder_new (desktop, item, NULL);
	g_assert (item != NULL);

	SPStar *star = SP_STAR(item);

	sp_knot_holder_add (knot_holder, sp_star_knot1_set, sp_star_knot1_get, sp_star_knot_click,
						_("Adjust the <b>tip radius</b> of the star or polygon; with <b>Shift</b> to round; with <b>Alt</b> to randomize"));
	if (star->flatsided == false)
		sp_knot_holder_add (knot_holder, sp_star_knot2_set, sp_star_knot2_get, sp_star_knot_click,
						_("Adjust the <b>base radius</b> of the star; with <b>Ctrl</b> to keep star rays radial (no skew); with <b>Shift</b> to round; with <b>Alt</b> to randomize"));

	sp_pat_knot_holder (item, knot_holder);

	return knot_holder;
}

/* SPSpiral */

/*
 * set attributes via inner (t=t0) knot point:
 *   [default] increase/decrease inner point
 *   [shift]   increase/decrease inner and outer arg synchronizely
 *   [control] constrain inner arg to round per PI/4
 */
static void
sp_spiral_inner_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);

	SPSpiral *spiral = SP_SPIRAL (item);

	gdouble   dx = p[NR::X] - spiral->cx;
	gdouble   dy = p[NR::Y] - spiral->cy;

	if (state & GDK_MOD1_MASK) {
		// adjust divergence by vertical drag, relative to rad
		double new_exp = (spiral->rad + dy)/(spiral->rad);
		spiral->exp = new_exp > 0? new_exp : 0;
	} else {
		// roll/unroll from inside
		gdouble   arg_t0;
		sp_spiral_get_polar (spiral, spiral->t0, NULL, &arg_t0);

		gdouble   arg_tmp = atan2(dy, dx) - arg_t0;
		gdouble   arg_t0_new = arg_tmp - floor((arg_tmp+M_PI)/(2.0*M_PI))*2.0*M_PI + arg_t0;
		spiral->t0 = (arg_t0_new - spiral->arg) / (2.0*M_PI*spiral->revo);

		/* round inner arg per PI/snaps, if CTRL is pressed */
		if ( ( state & GDK_CONTROL_MASK )
				 && ( fabs(spiral->revo) > SP_EPSILON_2 )
				 && ( snaps != 0 ) )	{
			gdouble arg = 2.0*M_PI*spiral->revo*spiral->t0 + spiral->arg;
			spiral->t0 = (sp_round(arg, M_PI/snaps) - spiral->arg)/(2.0*M_PI*spiral->revo);
		}

		spiral->t0 = CLAMP (spiral->t0, 0.0, 0.999);
	}

	((SPObject *)spiral)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

/*
 * set attributes via outer (t=1) knot point:
 *   [default] increase/decrease revolution factor
 *   [control] constrain inner arg to round per PI/4
 */
static void
sp_spiral_outer_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);

	SPSpiral *spiral = SP_SPIRAL (item);

	gdouble  dx = p[NR::X] - spiral->cx;
	gdouble  dy = p[NR::Y] - spiral->cy;

	if (state & GDK_SHIFT_MASK) { // rotate without roll/unroll
		spiral->arg = atan2(dy, dx) - 2.0*M_PI*spiral->revo;
		if (!(state & GDK_MOD1_MASK)) { 
			// if alt not pressed, change also rad; otherwise it is locked
			spiral->rad = MAX (hypot (dx, dy), 0.001);
		}
		if ( ( state & GDK_CONTROL_MASK )
				 && snaps ) {
			spiral->arg = sp_round(spiral->arg, M_PI/snaps);
		}
	} else { // roll/unroll
		// arg of the spiral outer end
		double arg_1;
		sp_spiral_get_polar (spiral, 1, NULL, &arg_1);

		// its fractional part after the whole turns are subtracted
		double arg_r = arg_1 - sp_round (arg_1, 2.0*M_PI);

		// arg of the mouse point relative to spiral center
		double mouse_angle = atan2(dy, dx);
		if (mouse_angle < 0)
			mouse_angle += 2*M_PI;

		// snap if ctrl
		if ( ( state & GDK_CONTROL_MASK ) && snaps ) {
			mouse_angle = sp_round (mouse_angle, M_PI/snaps);
		}

		// by how much we want to rotate the outer point
		double diff = mouse_angle - arg_r;
		if (diff > M_PI) 
			diff -= 2*M_PI;
		else if (diff < -M_PI)
			diff += 2*M_PI;

		// calculate the new rad;
		// the value of t corresponding to the angle arg_1 + diff:
		double t_temp = ((arg_1 + diff) - spiral->arg)/(2*M_PI*spiral->revo);
		// the rad at that t:
		double rad_new = 0;
		if (t_temp > spiral->t0)
			sp_spiral_get_polar (spiral, t_temp, &rad_new, NULL);

		// change the revo (converting diff from radians to the number of turns)
		spiral->revo += diff/(2*M_PI);
		if (spiral->revo < 1e-3)
			spiral->revo = 1e-3;

		// if alt not pressed and the values are sane, change the rad
		if (!(state & GDK_MOD1_MASK) && rad_new > 1e-3 && rad_new/spiral->rad < 2) {
			// adjust t0 too so that the inner point stays unmoved
			double r0;
			sp_spiral_get_polar (spiral, spiral->t0, &r0, NULL);
			spiral->rad = rad_new;
			spiral->t0 = pow (r0 / spiral->rad, 1/spiral->exp);
		}
		if (!isFinite(spiral->t0)) spiral->t0 = 0.0;
		spiral->t0 = CLAMP (spiral->t0, 0.0, 0.999);	
	}

	((SPObject *)spiral)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

static NR::Point sp_spiral_inner_get (SPItem *item)
{
	SPSpiral *spiral = SP_SPIRAL (item);

	return sp_spiral_get_xy (spiral, spiral->t0);
}

static NR::Point sp_spiral_outer_get (SPItem *item)
{
	SPSpiral *spiral = SP_SPIRAL (item);

	return sp_spiral_get_xy (spiral, 1.0);
}

static void
sp_spiral_inner_click (SPItem *item, guint state)
{
	SPSpiral *spiral = SP_SPIRAL (item);

	if (state & GDK_MOD1_MASK) {
		spiral->exp = 1;
		((SPObject *)spiral)->updateRepr();
	} else if (state & GDK_SHIFT_MASK) {
		spiral->t0 = 0;
		((SPObject *)spiral)->updateRepr();
	}
}

static SPKnotHolder *
sp_spiral_knot_holder (SPItem * item, SPDesktop *desktop)
{
	SPKnotHolder *knot_holder = sp_knot_holder_new (desktop, item, NULL);

	sp_knot_holder_add (knot_holder, sp_spiral_inner_set, sp_spiral_inner_get, sp_spiral_inner_click,
					_("Roll/unroll the spiral from <b>inside</b>; with <b>Ctrl</b> to snap angle; with <b>Alt</b> to converge/diverge"));
	sp_knot_holder_add (knot_holder, sp_spiral_outer_set, sp_spiral_outer_get, NULL,
					_("Roll/unroll the spiral from <b>outside</b>; with <b>Ctrl</b> to snap angle; with <b>Shift</b> to scale/rotate"));

	sp_pat_knot_holder (item, knot_holder);

	return knot_holder;
}

/* SPOffset */

static void
sp_offset_offset_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	SPOffset *offset = SP_OFFSET (item);

	offset->rad = sp_offset_distance_to_original(offset, p);
	offset->knot = p;
	offset->knotSet=true;

	((SPObject *)offset)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}


static NR::Point sp_offset_offset_get (SPItem *item)
{
	SPOffset *offset = SP_OFFSET (item);

	NR::Point np;
	sp_offset_top_point(offset,&np);
	return np;
}

static SPKnotHolder *
sp_offset_knot_holder (SPItem * item, SPDesktop *desktop)
{
	SPKnotHolder *knot_holder = sp_knot_holder_new (desktop, item, NULL);

	sp_knot_holder_add (knot_holder, sp_offset_offset_set, sp_offset_offset_get, NULL, 
					_("Adjust the <b>offset distance</b>"));

	sp_pat_knot_holder (item, knot_holder);

	return knot_holder;
}

static SPKnotHolder *
sp_path_knot_holder (SPItem * item, SPDesktop *desktop) // FIXME: eliminate, instead make a pattern-drag similar to gradient-drag
{
    if ((SP_OBJECT(item)->style->fill.type == SP_PAINT_TYPE_PAINTSERVER)
        && SP_IS_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style)))
        {
            SPKnotHolder *knot_holder = sp_knot_holder_new (desktop, item, NULL);

            sp_pat_knot_holder (item, knot_holder);

            return knot_holder;
        }
   return NULL;
}

static void
sp_pat_knot_holder (SPItem * item, SPKnotHolder *knot_holder)
{

    if ((SP_OBJECT(item)->style->fill.type == SP_PAINT_TYPE_PAINTSERVER)
        && SP_IS_PATTERN (SP_STYLE_FILL_SERVER (SP_OBJECT(item)->style)))
        {
            sp_knot_holder_add_full (knot_holder, sp_pattern_xy_set, sp_pattern_xy_get, NULL, SP_KNOT_SHAPE_CROSS, SP_KNOT_MODE_XOR,
            // TRANSLATORS: This refers to the pattern that's inside the object
																		 _("<b>Move</b> the pattern fill inside the object"));
            sp_knot_holder_add_full (knot_holder, sp_pattern_scale_set, sp_pattern_scale_get, NULL, SP_KNOT_SHAPE_SQUARE, SP_KNOT_MODE_XOR,
																		 _("<b>Scale</b> the pattern fill uniformly"));
            sp_knot_holder_add_full (knot_holder, sp_pattern_angle_set, sp_pattern_angle_get, NULL, SP_KNOT_SHAPE_CIRCLE, SP_KNOT_MODE_XOR,
																		 _("<b>Rotate</b> the pattern fill; with <b>Ctrl</b> to snap angle"));
        }
}

static NR::Point sp_flowtext_corner_get (SPItem *item)
{
	SPRect *rect = SP_RECT(item);

	return NR::Point(rect->x.computed + rect->width.computed, rect->y.computed + rect->height.computed);
}

static void
sp_flowtext_corner_set (SPItem *item, const NR::Point &p, const NR::Point &origin, guint state)
{
	SPRect *rect = SP_RECT(item);

	sp_rect_wh_set_internal (rect, p, origin, state);
}

static SPKnotHolder *
sp_flowtext_knot_holder (SPItem * item, SPDesktop *desktop)
{
	SPKnotHolder *knot_holder = sp_knot_holder_new (desktop, SP_FLOWTEXT(item)->get_frame(NULL), NULL);

	sp_knot_holder_add (knot_holder, sp_flowtext_corner_set, sp_flowtext_corner_get, NULL, 
					_("Drag to resize the <b>flowed text frame</b>"));

	return knot_holder;
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
