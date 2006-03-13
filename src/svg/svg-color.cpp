#define __SP_SVG_COLOR_C__

/**
 * \file
 * Reading \& writing of SVG/CSS colors.
 */
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "svg-color.h"
#include <cassert>
#include <math.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/ghash.h>
#include <glib/gutils.h>
#include <cstdio> // sprintf
#include "strneq.h"
using std::sprintf;

struct SPSVGColor {
    unsigned long rgb;
    char const *name;
};

/*
 * These are the colors defined in the SVG standard
 */
static SPSVGColor const sp_svg_color_named[] = {
    { 0xF0F8FF, "aliceblue" },
    { 0xFAEBD7, "antiquewhite" },
    { 0x00FFFF, "aqua" },
    { 0x7FFFD4, "aquamarine" },
    { 0xF0FFFF, "azure" },
    { 0xF5F5DC, "beige" },
    { 0xFFE4C4, "bisque" },
    { 0x000000, "black" },
    { 0xFFEBCD, "blanchedalmond" },
    { 0x0000FF, "blue" },
    { 0x8A2BE2, "blueviolet" },
    { 0xA52A2A, "brown" },
    { 0xDEB887, "burlywood" },
    { 0x5F9EA0, "cadetblue" },
    { 0x7FFF00, "chartreuse" },
    { 0xD2691E, "chocolate" },
    { 0xFF7F50, "coral" },
    { 0x6495ED, "cornflowerblue" },
    { 0xFFF8DC, "cornsilk" },
    { 0xDC143C, "crimson" },
    { 0x00FFFF, "cyan" },
    { 0x00008B, "darkblue" },
    { 0x008B8B, "darkcyan" },
    { 0xB8860B, "darkgoldenrod" },
    { 0xA9A9A9, "darkgray" },
    { 0x006400, "darkgreen" },
    { 0xA9A9A9, "darkgrey" },
    { 0xBDB76B, "darkkhaki" },
    { 0x8B008B, "darkmagenta" },
    { 0x556B2F, "darkolivegreen" },
    { 0xFF8C00, "darkorange" },
    { 0x9932CC, "darkorchid" },
    { 0x8B0000, "darkred" },
    { 0xE9967A, "darksalmon" },
    { 0x8FBC8F, "darkseagreen" },
    { 0x483D8B, "darkslateblue" },
    { 0x2F4F4F, "darkslategray" },
    { 0x2F4F4F, "darkslategrey" },
    { 0x00CED1, "darkturquoise" },
    { 0x9400D3, "darkviolet" },
    { 0xFF1493, "deeppink" },
    { 0x00BFFF, "deepskyblue" },
    { 0x696969, "dimgray" },
    { 0x696969, "dimgrey" },
    { 0x1E90FF, "dodgerblue" },
    { 0xB22222, "firebrick" },
    { 0xFFFAF0, "floralwhite" },
    { 0x228B22, "forestgreen" },
    { 0xFF00FF, "fuchsia" },
    { 0xDCDCDC, "gainsboro" },
    { 0xF8F8FF, "ghostwhite" },
    { 0xFFD700, "gold" },
    { 0xDAA520, "goldenrod" },
    { 0x808080, "gray" },
    { 0x808080, "grey" },
    { 0x008000, "green" },
    { 0xADFF2F, "greenyellow" },
    { 0xF0FFF0, "honeydew" },
    { 0xFF69B4, "hotpink" },
    { 0xCD5C5C, "indianred" },
    { 0x4B0082, "indigo" },
    { 0xFFFFF0, "ivory" },
    { 0xF0E68C, "khaki" },
    { 0xE6E6FA, "lavender" },
    { 0xFFF0F5, "lavenderblush" },
    { 0x7CFC00, "lawngreen" },
    { 0xFFFACD, "lemonchiffon" },
    { 0xADD8E6, "lightblue" },
    { 0xF08080, "lightcoral" },
    { 0xE0FFFF, "lightcyan" },
    { 0xFAFAD2, "lightgoldenrodyellow" },
    { 0xD3D3D3, "lightgray" },
    { 0x90EE90, "lightgreen" },
    { 0xD3D3D3, "lightgrey" },
    { 0xFFB6C1, "lightpink" },
    { 0xFFA07A, "lightsalmon" },
    { 0x20B2AA, "lightseagreen" },
    { 0x87CEFA, "lightskyblue" },
    { 0x778899, "lightslategray" },
    { 0x778899, "lightslategrey" },
    { 0xB0C4DE, "lightsteelblue" },
    { 0xFFFFE0, "lightyellow" },
    { 0x00FF00, "lime" },
    { 0x32CD32, "limegreen" },
    { 0xFAF0E6, "linen" },
    { 0xFF00FF, "magenta" },
    { 0x800000, "maroon" },
    { 0x66CDAA, "mediumaquamarine" },
    { 0x0000CD, "mediumblue" },
    { 0xBA55D3, "mediumorchid" },
    { 0x9370DB, "mediumpurple" },
    { 0x3CB371, "mediumseagreen" },
    { 0x7B68EE, "mediumslateblue" },
    { 0x00FA9A, "mediumspringgreen" },
    { 0x48D1CC, "mediumturquoise" },
    { 0xC71585, "mediumvioletred" },
    { 0x191970, "midnightblue" },
    { 0xF5FFFA, "mintcream" },
    { 0xFFE4E1, "mistyrose" },
    { 0xFFE4B5, "moccasin" },
    { 0xFFDEAD, "navajowhite" },
    { 0x000080, "navy" },
    { 0xFDF5E6, "oldlace" },
    { 0x808000, "olive" },
    { 0x6B8E23, "olivedrab" },
    { 0xFFA500, "orange" },
    { 0xFF4500, "orangered" },
    { 0xDA70D6, "orchid" },
    { 0xEEE8AA, "palegoldenrod" },
    { 0x98FB98, "palegreen" },
    { 0xAFEEEE, "paleturquoise" },
    { 0xDB7093, "palevioletred" },
    { 0xFFEFD5, "papayawhip" },
    { 0xFFDAB9, "peachpuff" },
    { 0xCD853F, "peru" },
    { 0xFFC0CB, "pink" },
    { 0xDDA0DD, "plum" },
    { 0xB0E0E6, "powderblue" },
    { 0x800080, "purple" },
    { 0xFF0000, "red" },
    { 0xBC8F8F, "rosybrown" },
    { 0x4169E1, "royalblue" },
    { 0x8B4513, "saddlebrown" },
    { 0xFA8072, "salmon" },
    { 0xF4A460, "sandybrown" },
    { 0x2E8B57, "seagreen" },
    { 0xFFF5EE, "seashell" },
    { 0xA0522D, "sienna" },
    { 0xC0C0C0, "silver" },
    { 0x87CEEB, "skyblue" },
    { 0x6A5ACD, "slateblue" },
    { 0x708090, "slategray" },
    { 0x708090, "slategrey" },
    { 0xFFFAFA, "snow" },
    { 0x00FF7F, "springgreen" },
    { 0x4682B4, "steelblue" },
    { 0xD2B48C, "tan" },
    { 0x008080, "teal" },
    { 0xD8BFD8, "thistle" },
    { 0xFF6347, "tomato" },
    { 0x40E0D0, "turquoise" },
    { 0xEE82EE, "violet" },
    { 0xF5DEB3, "wheat" },
    { 0xFFFFFF, "white" },
    { 0xF5F5F5, "whitesmoke" },
    { 0xFFFF00, "yellow" },
    { 0x9ACD32, "yellowgreen" }
};

static GHashTable *sp_svg_create_color_hash();

guint32
sp_svg_read_color(gchar const *str, guint32 def)
{
    static GHashTable *colors = NULL;
    gchar c[32];
    guint32 val = 0;

    if (str == NULL) return def;
    while ((*str <= ' ') && *str) str++;
    if (!*str) return def;

    if (str[0] == '#') {
        gint i;
        for (i = 1; str[i]; i++) {
            int hexval;
            if (str[i] >= '0' && str[i] <= '9')
                hexval = str[i] - '0';
            else if (str[i] >= 'A' && str[i] <= 'F')
                hexval = str[i] - 'A' + 10;
            else if (str[i] >= 'a' && str[i] <= 'f')
                hexval = str[i] - 'a' + 10;
            else
                break;
            val = (val << 4) + hexval;
        }
        /* handle #rgb case */
        if (i == 1 + 3) {
            val = ((val & 0xf00) << 8) |
                ((val & 0x0f0) << 4) |
                (val & 0x00f);
            val |= val << 4;
        } else if (i != 1 + 6) {
            /* must be either 3 or 6 digits. */
            return def;
        }
    } else if (strneq(str, "rgb(", 4)) {
        gboolean hasp, hasd;
        gchar *s, *e;
        gdouble r, g, b;

        s = (gchar *) str + 4;
        hasp = FALSE;
        hasd = FALSE;

        r = g_ascii_strtod(s, &e);
        if (s == e) return def;
        s = e;
        if (*s == '%') {
            hasp = TRUE;
            s += 1;
        } else {
            hasd = TRUE;
        }
        while (*s && g_ascii_isspace(*s)) s += 1;
        if (*s != ',') return def;
        s += 1;
        while (*s && g_ascii_isspace(*s)) s += 1;
        g = g_ascii_strtod(s, &e);
        if (s == e) return def;
        s = e;
        if (*s == '%') {
            hasp = TRUE;
            s += 1;
        } else {
            hasd = TRUE;
        }
        while (*s && g_ascii_isspace(*s)) s += 1;
        if (*s != ',') return def;
        s += 1;
        while (*s && g_ascii_isspace(*s)) s += 1;
        b = g_ascii_strtod(s, &e);
        if (s == e) return def;
        s = e;
        if (*s == '%') {
            hasp = TRUE;
        } else {
            hasd = TRUE;
        }
        if (hasp && hasd) return def;
        if (hasp) {
            val = (guint) floor(CLAMP(r, 0.0, 100.0) * 2.559999) << 24;
            val |= ((guint) floor(CLAMP(g, 0.0, 100.0) * 2.559999) << 16);
            val |= ((guint) floor(CLAMP(b, 0.0, 100.0) * 2.559999) << 8);
        } else {
            val = (guint) CLAMP(r, 0, 255) << 24;
            val |= ((guint) CLAMP(g, 0, 255) << 16);
            val |= ((guint) CLAMP(b, 0, 255) << 8);
        }
        return val;
    } else {
        gint i;
        if (!colors) {
            colors = sp_svg_create_color_hash();
        }
        for (i = 0; i < 31; i++) {
            if (str[i] == ';') {
                c[i] = '\0';
                break;
            }
            c[i] = g_ascii_tolower(str[i]);
            if (!str[i]) break;
        }
        c[31] = '\0';

        gpointer const rgb_ptr = g_hash_table_lookup(colors, c);
        if (rgb_ptr) {
            val = *(static_cast<unsigned long *>(rgb_ptr));
        } else {
            return def;
        }
    }

    return (val << 8);
}

/**
 * Converts an RGB colour expressed in form 0x00rrggbb to a CSS/SVG representation of that colour.
 * The result is valid even in SVG Tiny or non-SVG CSS.
 */
static void
rgb24_to_css(char *const buf, unsigned const rgb24)
{
    assert(rgb24 < (1u << 24));

    /* SVG 1.1 Full allows additional colour names not supported by SVG Tiny, but we don't bother
     * with them: it's good for these colours to be copyable to non-SVG CSS stylesheets and for
     * documents to be more viewable in SVG Tiny/Basic viewers; and some of the SVG Full names are
     * less meaningful than hex equivalents anyway.  And it's easier for a person to map from the
     * restricted set because the only component values are {00,80,ff}, other than silver 0xc0c0c0.
     */

    char const *src = NULL;
    switch (rgb24) {
        /* Extracted mechanically from the table at
         * http://www.w3.org/TR/REC-html40/types.html#h-6.5 .*/
        case 0x000000: src = "black"; break; 		
        case 0xc0c0c0: src = "silver"; break; 		
        case 0x808080: src = "gray"; break; 		
        case 0xffffff: src = "white"; break; 		
        case 0x800000: src = "maroon"; break; 		
        case 0xff0000: src = "red"; break; 		
        case 0x800080: src = "purple"; break; 		
        case 0xff00ff: src = "fuchsia"; break; 		
        case 0x008000: src = "green"; break; 
        case 0x00ff00: src = "lime"; break;  
        case 0x808000: src = "olive"; break; 
        case 0xffff00: src = "yellow"; break;
        case 0x000080: src = "navy"; break;  
        case 0x0000ff: src = "blue"; break;  
        case 0x008080: src = "teal"; break;  
        case 0x00ffff: src = "aqua"; break;  

        default: {
            if ((rgb24 & 0xf0f0f) * 0x11 == rgb24) {
                /* Can use the shorter three-digit form #rgb instead of #rrggbb. */
                sprintf(buf, "#%x%x%x",
                        (rgb24 >> 16) & 0xf,
                        (rgb24 >> 8) & 0xf,
                        rgb24 & 0xf);
            } else {
                sprintf(buf, "#%06x", rgb24);
            }
            break;
        }
    }
    if (src) {
        strcpy(buf, src);
    }

    assert(sp_svg_read_color(buf, 0xff) == (rgb24 << 8));
}

/**
 * Converts an RGBA32 colour to a CSS/SVG representation of the RGB portion of that colour.  The
 * result is valid even in SVG Tiny or non-SVG CSS.
 *
 * \param rgba32 Colour expressed in form 0xrrggbbaa.
 * \pre buflen \>= 8.
 */
void
sp_svg_write_color(gchar *buf, unsigned const buflen, guint32 const rgba32)
{
    g_assert(8 <= buflen);

    unsigned const rgb24 = rgba32 >> 8;
    rgb24_to_css(buf, rgb24);
}

static GHashTable *
sp_svg_create_color_hash()
{
    GHashTable *colors = g_hash_table_new(g_str_hash, g_str_equal);

    for (unsigned i = 0 ; i < G_N_ELEMENTS(sp_svg_color_named) ; i++) {
        g_hash_table_insert(colors,
                            (gpointer)(sp_svg_color_named[i].name),
                            (gpointer)(&sp_svg_color_named[i].rgb));
    }

    return colors;
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
