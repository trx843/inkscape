/** \file
 * SVG <feDiffuseLighting> implementation.
 *
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Jean-Rene Reinhard <jr@komite.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *               2007 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "strneq.h"

#include "attributes.h"
#include "svg/svg.h"
#include "sp-object.h"
#include "svg/svg-color.h"
#include "svg/svg-icc-color.h"
#include "filters/diffuselighting.h"
#include "filters/distantlight.h"
#include "filters/pointlight.h"
#include "filters/spotlight.h"
#include "display/nr-filter.h"
#include "xml/repr.h"
#include "display/nr-filter-diffuselighting.h"

/* FeDiffuseLighting base class */
static void sp_feDiffuseLighting_children_modified(SPFeDiffuseLighting *sp_diffuselighting);

G_DEFINE_TYPE(SPFeDiffuseLighting, sp_feDiffuseLighting, SP_TYPE_FILTER_PRIMITIVE);

static void
sp_feDiffuseLighting_class_init(SPFeDiffuseLightingClass *klass)
{
}

CFeDiffuseLighting::CFeDiffuseLighting(SPFeDiffuseLighting* dl) : CFilterPrimitive(dl) {
	this->spfediffuselighting = dl;
}

CFeDiffuseLighting::~CFeDiffuseLighting() {
}

static void
sp_feDiffuseLighting_init(SPFeDiffuseLighting *feDiffuseLighting)
{
	feDiffuseLighting->cfediffuselighting = new CFeDiffuseLighting(feDiffuseLighting);

	delete feDiffuseLighting->cfilterprimitive;
	feDiffuseLighting->cfilterprimitive = feDiffuseLighting->cfediffuselighting;
	feDiffuseLighting->cobject = feDiffuseLighting->cfediffuselighting;

    feDiffuseLighting->surfaceScale = 1;
    feDiffuseLighting->diffuseConstant = 1;
    feDiffuseLighting->lighting_color = 0xffffffff;
    feDiffuseLighting->icc = NULL;

    //TODO kernelUnit
    feDiffuseLighting->renderer = NULL;

    feDiffuseLighting->surfaceScale_set = FALSE;
    feDiffuseLighting->diffuseConstant_set = FALSE;
    feDiffuseLighting->lighting_color_set = FALSE;
}

/**
 * Reads the Inkscape::XML::Node, and initializes SPFeDiffuseLighting variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */
void CFeDiffuseLighting::onBuild(SPDocument *document, Inkscape::XML::Node *repr) {
	CFilterPrimitive::onBuild(document, repr);

	SPFeDiffuseLighting* object = this->spfediffuselighting;

	/*LOAD ATTRIBUTES FROM REPR HERE*/
	object->readAttr( "surfaceScale" );
	object->readAttr( "diffuseConstant" );
	object->readAttr( "kernelUnitLength" );
	object->readAttr( "lighting-color" );
}

/**
 * Drops any allocated memory.
 */
void CFeDiffuseLighting::onRelease() {
	CFilterPrimitive::onRelease();
}

/**
 * Sets a specific value in the SPFeDiffuseLighting.
 */
void CFeDiffuseLighting::onSet(unsigned int key, gchar const *value) {
	SPFeDiffuseLighting* object = this->spfediffuselighting;

    SPFeDiffuseLighting *feDiffuseLighting = SP_FEDIFFUSELIGHTING(object);
    gchar const *cend_ptr = NULL;
    gchar *end_ptr = NULL;
    
    switch(key) {
	/*DEAL WITH SETTING ATTRIBUTES HERE*/
//TODO test forbidden values
        case SP_ATTR_SURFACESCALE:
            end_ptr = NULL;
            if (value) {
                feDiffuseLighting->surfaceScale = g_ascii_strtod(value, &end_ptr);
                if (end_ptr) {
                    feDiffuseLighting->surfaceScale_set = TRUE;
                }
            } 
            if (!value || !end_ptr) {
                feDiffuseLighting->surfaceScale = 1;
                feDiffuseLighting->surfaceScale_set = FALSE;
            }
            if (feDiffuseLighting->renderer) {
                feDiffuseLighting->renderer->surfaceScale = feDiffuseLighting->surfaceScale;
            }
            object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_DIFFUSECONSTANT:
            end_ptr = NULL;
            if (value) {
                feDiffuseLighting->diffuseConstant = g_ascii_strtod(value, &end_ptr);
                if (end_ptr && feDiffuseLighting->diffuseConstant >= 0) {
                    feDiffuseLighting->diffuseConstant_set = TRUE;
                } else {
                    end_ptr = NULL;
                    g_warning("feDiffuseLighting: diffuseConstant should be a positive number ... defaulting to 1");
                }
            } 
            if (!value || !end_ptr) {
                feDiffuseLighting->diffuseConstant = 1;
                feDiffuseLighting->diffuseConstant_set = FALSE;
            }
            if (feDiffuseLighting->renderer) {
                feDiffuseLighting->renderer->diffuseConstant = feDiffuseLighting->diffuseConstant;
    }
            object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_KERNELUNITLENGTH:
            //TODO kernelUnit
            //feDiffuseLighting->kernelUnitLength.set(value);
            /*TODOif (feDiffuseLighting->renderer) {
                feDiffuseLighting->renderer->surfaceScale = feDiffuseLighting->renderer;
            }
            */
            object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_PROP_LIGHTING_COLOR:
            cend_ptr = NULL;
            feDiffuseLighting->lighting_color = sp_svg_read_color(value, &cend_ptr, 0xffffffff);
            //if a value was read
            if (cend_ptr) {
                while (g_ascii_isspace(*cend_ptr)) {
                    ++cend_ptr;
                }
                if (strneq(cend_ptr, "icc-color(", 10)) {
                    if (!feDiffuseLighting->icc) feDiffuseLighting->icc = new SVGICCColor();
                    if ( ! sp_svg_read_icc_color( cend_ptr, feDiffuseLighting->icc ) ) {
                        delete feDiffuseLighting->icc;
                        feDiffuseLighting->icc = NULL;
                    }
                }
                feDiffuseLighting->lighting_color_set = TRUE; 
            } else {
                //lighting_color already contains the default value
                feDiffuseLighting->lighting_color_set = FALSE; 
            }
            if (feDiffuseLighting->renderer) {
                feDiffuseLighting->renderer->lighting_color = feDiffuseLighting->lighting_color;
            }
            object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
        	CFilterPrimitive::onSet(key, value);
            break;
    }
}

/**
 * Receives update notifications.
 */
void CFeDiffuseLighting::onUpdate(SPCtx *ctx, guint flags) {
	SPFeDiffuseLighting* object = this->spfediffuselighting;

    if (flags & (SP_OBJECT_MODIFIED_FLAG)) {
        object->readAttr( "surfaceScale" );
        object->readAttr( "diffuseConstant" );
        object->readAttr( "kernelUnit" );
        object->readAttr( "lighting-color" );
    }

    CFilterPrimitive::onUpdate(ctx, flags);
}

/**
 * Writes its settings to an incoming repr object, if any.
 */
Inkscape::XML::Node* CFeDiffuseLighting::onWrite(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
	SPFeDiffuseLighting* object = this->spfediffuselighting;

    SPFeDiffuseLighting *fediffuselighting = SP_FEDIFFUSELIGHTING(object);
    
    /* TODO: Don't just clone, but create a new repr node and write all
     * relevant values _and children_ into it */
    if (!repr) {
        repr = object->getRepr()->duplicate(doc);
        //repr = doc->createElement("svg:feDiffuseLighting");
    }
    
    if (fediffuselighting->surfaceScale_set)
        sp_repr_set_css_double(repr, "surfaceScale", fediffuselighting->surfaceScale);
    else
        repr->setAttribute("surfaceScale", NULL);
    if (fediffuselighting->diffuseConstant_set)
        sp_repr_set_css_double(repr, "diffuseConstant", fediffuselighting->diffuseConstant);
    else
        repr->setAttribute("diffuseConstant", NULL);
   /*TODO kernelUnits */ 
    if (fediffuselighting->lighting_color_set) {
        gchar c[64];
        sp_svg_write_color(c, sizeof(c), fediffuselighting->lighting_color);
        repr->setAttribute("lighting-color", c);
    } else
        repr->setAttribute("lighting-color", NULL);
        
    CFilterPrimitive::onWrite(doc, repr, flags);

    return repr;
}

/**
 * Callback for child_added event.
 */
void CFeDiffuseLighting::onChildAdded(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
	SPFeDiffuseLighting* object = this->spfediffuselighting;

    SPFeDiffuseLighting *f = SP_FEDIFFUSELIGHTING(object);

    CFilterPrimitive::onChildAdded(child, ref);

    sp_feDiffuseLighting_children_modified(f);
    object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback for remove_child event.
 */
void CFeDiffuseLighting::onRemoveChild(Inkscape::XML::Node *child) {
	SPFeDiffuseLighting* object = this->spfediffuselighting;

	SPFeDiffuseLighting *f = SP_FEDIFFUSELIGHTING(object);

	CFilterPrimitive::onRemoveChild(child);

	sp_feDiffuseLighting_children_modified(f);
	object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void CFeDiffuseLighting::onOrderChanged(Inkscape::XML::Node *child, Inkscape::XML::Node *old_ref, Inkscape::XML::Node *new_ref) {
	SPFeDiffuseLighting* object = this->spfediffuselighting;

    SPFeDiffuseLighting *f = SP_FEDIFFUSELIGHTING(object);
    CFilterPrimitive::onOrderChanged(child, old_ref, new_ref);

    sp_feDiffuseLighting_children_modified(f);
    object->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

static void sp_feDiffuseLighting_children_modified(SPFeDiffuseLighting *sp_diffuselighting)
{
   if (sp_diffuselighting->renderer) {
        sp_diffuselighting->renderer->light_type = Inkscape::Filters::NO_LIGHT;
        if (SP_IS_FEDISTANTLIGHT(sp_diffuselighting->children)) {
            sp_diffuselighting->renderer->light_type = Inkscape::Filters::DISTANT_LIGHT;
            sp_diffuselighting->renderer->light.distant = SP_FEDISTANTLIGHT(sp_diffuselighting->children);
        }
        if (SP_IS_FEPOINTLIGHT(sp_diffuselighting->children)) {
            sp_diffuselighting->renderer->light_type = Inkscape::Filters::POINT_LIGHT;
            sp_diffuselighting->renderer->light.point = SP_FEPOINTLIGHT(sp_diffuselighting->children);
        }
        if (SP_IS_FESPOTLIGHT(sp_diffuselighting->children)) {
            sp_diffuselighting->renderer->light_type = Inkscape::Filters::SPOT_LIGHT;
            sp_diffuselighting->renderer->light.spot = SP_FESPOTLIGHT(sp_diffuselighting->children);
        }
   }
}

void CFeDiffuseLighting::onBuildRenderer(Inkscape::Filters::Filter* filter) {
	SPFeDiffuseLighting* primitive = this->spfediffuselighting;

    g_assert(primitive != NULL);
    g_assert(filter != NULL);

    SPFeDiffuseLighting *sp_diffuselighting = SP_FEDIFFUSELIGHTING(primitive);

    int primitive_n = filter->add_primitive(Inkscape::Filters::NR_FILTER_DIFFUSELIGHTING);
    Inkscape::Filters::FilterPrimitive *nr_primitive = filter->get_primitive(primitive_n);
    Inkscape::Filters::FilterDiffuseLighting *nr_diffuselighting = dynamic_cast<Inkscape::Filters::FilterDiffuseLighting*>(nr_primitive);
    g_assert(nr_diffuselighting != NULL);

    sp_diffuselighting->renderer = nr_diffuselighting;
    sp_filter_primitive_renderer_common(primitive, nr_primitive);

    nr_diffuselighting->diffuseConstant = sp_diffuselighting->diffuseConstant;
    nr_diffuselighting->surfaceScale = sp_diffuselighting->surfaceScale;
    nr_diffuselighting->lighting_color = sp_diffuselighting->lighting_color;
    nr_diffuselighting->set_icc(sp_diffuselighting->icc);

    //We assume there is at most one child
    nr_diffuselighting->light_type = Inkscape::Filters::NO_LIGHT;
    if (SP_IS_FEDISTANTLIGHT(primitive->children)) {
        nr_diffuselighting->light_type = Inkscape::Filters::DISTANT_LIGHT;
        nr_diffuselighting->light.distant = SP_FEDISTANTLIGHT(primitive->children);
    }
    if (SP_IS_FEPOINTLIGHT(primitive->children)) {
        nr_diffuselighting->light_type = Inkscape::Filters::POINT_LIGHT;
        nr_diffuselighting->light.point = SP_FEPOINTLIGHT(primitive->children);
    }
    if (SP_IS_FESPOTLIGHT(primitive->children)) {
        nr_diffuselighting->light_type = Inkscape::Filters::SPOT_LIGHT;
        nr_diffuselighting->light.spot = SP_FESPOTLIGHT(primitive->children);
    }
        
    //nr_offset->set_dx(sp_offset->dx);
    //nr_offset->set_dy(sp_offset->dy);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
