/*
 */

#include <config.h>
#include <string.h>

#include "attributes.h"
#include "xml/repr.h"
#include "svg/svg.h"
#include "style.h"
#include "inkscape.h"
#include "document.h"
#include "selection.h"
#include "desktop-handles.h"
#include "desktop.h"
#include "print.h"

#include "libnr/nr-matrix.h"
#include "libnr/nr-point.h"
#include "xml/repr.h"

#include "sp-flowdiv.h"
#include "sp-flowregion.h"
#include "sp-flowtext.h"
#include "sp-string.h"

#include "libnr/nr-matrix.h"
#include "libnr/nr-translate.h"
#include "libnr/nr-scale.h"
#include "libnr/nr-matrix-ops.h"
#include "libnr/nr-translate-ops.h"
#include "libnr/nr-scale-ops.h"

#include "libnrtype/FlowDest.h"
#include "livarot/Shape.h"

#include "display/nr-arena-item.h"
#include "display/nr-arena-group.h"
#include "display/nr-arena-glyphs.h"

#include <pango/pango.h>

static void sp_flowtext_class_init(SPFlowtextClass *klass);
static void sp_flowtext_init(SPFlowtext *group);
static void sp_flowtext_dispose(GObject *object);

static void sp_flowtext_child_added(SPObject *object, Inkscape::XML::Node *child, Inkscape::XML::Node *ref);
static void sp_flowtext_remove_child(SPObject *object, Inkscape::XML::Node *child);
static void sp_flowtext_update(SPObject *object, SPCtx *ctx, guint flags);
static void sp_flowtext_modified(SPObject *object, guint flags);
static Inkscape::XML::Node *sp_flowtext_write(SPObject *object, Inkscape::XML::Node *repr, guint flags);
static void sp_flowtext_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr);
static void sp_flowtext_set(SPObject *object, unsigned key, gchar const *value);

static void sp_flowtext_bbox(SPItem const *item, NRRect *bbox, NR::Matrix const &transform, unsigned const flags);
static void sp_flowtext_print(SPItem *item, SPPrintContext *ctx);
static gchar *sp_flowtext_description(SPItem *item);
static NRArenaItem *sp_flowtext_show(SPItem *item, NRArena *arena, unsigned key, unsigned flags);
static void sp_flowtext_hide(SPItem *item, unsigned key);

static SPItemClass *parent_class;

GType
sp_flowtext_get_type(void)
{
    static GType group_type = 0;
    if (!group_type) {
        GTypeInfo group_info = {
            sizeof(SPFlowtextClass),
            NULL,   /* base_init */
            NULL,   /* base_finalize */
            (GClassInitFunc) sp_flowtext_class_init,
            NULL,   /* class_finalize */
            NULL,   /* class_data */
            sizeof(SPFlowtext),
            16,     /* n_preallocs */
            (GInstanceInitFunc) sp_flowtext_init,
            NULL,   /* value_table */
        };
        group_type = g_type_register_static(SP_TYPE_ITEM, "SPFlowtext", &group_info, (GTypeFlags)0);
    }
    return group_type;
}

static void
sp_flowtext_class_init(SPFlowtextClass *klass)
{
    GObjectClass *object_class = (GObjectClass *) klass;
    SPObjectClass *sp_object_class = (SPObjectClass *) klass;
    SPItemClass *item_class = (SPItemClass *) klass;

    parent_class = (SPItemClass *)g_type_class_ref(SP_TYPE_ITEM);

    object_class->dispose = sp_flowtext_dispose;

    sp_object_class->child_added = sp_flowtext_child_added;
    sp_object_class->remove_child = sp_flowtext_remove_child;
    sp_object_class->update = sp_flowtext_update;
    sp_object_class->modified = sp_flowtext_modified;
    sp_object_class->write = sp_flowtext_write;
    sp_object_class->build = sp_flowtext_build;
    sp_object_class->set = sp_flowtext_set;

    item_class->bbox = sp_flowtext_bbox;
    item_class->print = sp_flowtext_print;
    item_class->description = sp_flowtext_description;
    item_class->show = sp_flowtext_show;
    item_class->hide = sp_flowtext_hide;
}

static void
sp_flowtext_init(SPFlowtext *group)
{
    group->justify = false;
    group->par_indent = 0;
    group->algo = 0;

    new (&group->layout) Inkscape::Text::Layout();
}

static void
sp_flowtext_dispose(GObject *object)
{
    SPFlowtext *group = (SPFlowtext*)object;

    group->layout.~Layout();
}

static void
sp_flowtext_child_added(SPObject *object, Inkscape::XML::Node *child, Inkscape::XML::Node *ref)
{
    if (((SPObjectClass *) (parent_class))->child_added)
        (* ((SPObjectClass *) (parent_class))->child_added)(object, child, ref);

    object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: hide (Lauris) */

static void
sp_flowtext_remove_child(SPObject *object, Inkscape::XML::Node *child)
{
    if (((SPObjectClass *) (parent_class))->remove_child)
        (* ((SPObjectClass *) (parent_class))->remove_child)(object, child);

    object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

//RH:fixme: these should be member functions
static void BuildLayoutInput(SPObject *root, Inkscape::Text::Layout *layout, Shape const *exclusion_shape, std::list<Shape> *shapes)
{
    if (SP_IS_FLOWDIV(root) || SP_IS_FLOWPARA(root))
        if (layout->inputExists())
            layout->appendControlCode(Inkscape::Text::Layout::PARAGRAPH_BREAK, root);
    if (SP_IS_FLOWREGIONBREAK(root))
        if (layout->inputExists())
            layout->appendControlCode(Inkscape::Text::Layout::SHAPE_BREAK, root);

    for (SPObject *child = sp_object_first_child(root) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
        if (SP_IS_STRING(child)) {
            layout->appendText(SP_STRING(child)->string, root->style, child);
        } else if (SP_IS_FLOWREGION(child)) {
            for (int i = 0 ; i < SP_FLOWREGION(child)->nbComp ; i++) {
                shapes->push_back(Shape());
                if (exclusion_shape->hasEdges())
                    shapes->back().Booleen(SP_FLOWREGION(child)->computed[i]->rgn_dest, const_cast<Shape*>(exclusion_shape), bool_op_diff);
                else
                    shapes->back().Copy(SP_FLOWREGION(child)->computed[i]->rgn_dest);
                layout->appendWrapShape(&shapes->back());
            }
        }
        else if (!SP_IS_FLOWREGIONEXCLUDE(child))
            BuildLayoutInput(child, layout, exclusion_shape, shapes);
    }

    if (SP_IS_FLOWLINE(root))     // yep, the spec specifies that we break after the content for these. I suspect most people will use it like <br/>, though, so it won't have any content
        layout->appendControlCode(Inkscape::Text::Layout::PARAGRAPH_BREAK, root);
}

static Shape* BuildExclusionShape(SPObject *object)
{
    Shape *shape = new Shape;
    Shape *shape_temp = new Shape;

    for (SPObject *child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
        // RH: is it right that this shouldn't be recursive?
        if ( SP_IS_FLOWREGIONEXCLUDE(child) ) {
            SPFlowregionExclude *c_child = SP_FLOWREGIONEXCLUDE(child);
            if (shape->hasEdges()) {
                shape_temp->Booleen(shape, c_child->computed->rgn_dest, bool_op_union);
                std::swap(shape, shape_temp);
            } else
                shape->Copy(c_child->computed->rgn_dest);
        }
    }
    delete shape_temp;
    return shape;
}

static void
sp_flowtext_update(SPObject *object, SPCtx *ctx, unsigned flags)
{
    SPFlowtext *group = SP_FLOWTEXT(object);
    SPItemCtx *ictx = (SPItemCtx *) ctx;
    SPItemCtx cctx = *ictx;

    if (((SPObjectClass *) (parent_class))->update)
        ((SPObjectClass *) (parent_class))->update(object, ctx, flags);

    if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    flags &= SP_OBJECT_MODIFIED_CASCADE;

    GSList *l = NULL;
    for (SPObject *child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
        g_object_ref(G_OBJECT(child));
        l = g_slist_prepend(l, child);
    }
    l = g_slist_reverse(l);
    while (l) {
        SPObject *child = SP_OBJECT(l->data);
        l = g_slist_remove(l, child);
        if (flags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            if (SP_IS_ITEM(child)) {
                SPItem const &chi = *SP_ITEM(child);
                cctx.i2doc = chi.transform * ictx->i2doc;
                cctx.i2vp = chi.transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, flags);
            } else {
                child->updateDisplay(ctx, flags);
            }
        }
        g_object_unref(G_OBJECT(child));
    }

    for (SPItemView *v = group->display; v != NULL; v = v->next) {
        group->ClearFlow(NR_ARENA_GROUP(v->arenaitem));
    }

    std::list<Shape> shapes;

    group->layout.clear();
    Shape *exclusion_shape = BuildExclusionShape(group);
    BuildLayoutInput(group, &group->layout, exclusion_shape, &shapes);
    delete exclusion_shape;
    group->layout.calculateFlow();
    //g_print(group->layout.dumpAsText().c_str());

    // pass the bbox of the flowtext object as paintbox (used for paintserver fills)	
    NRRect paintbox;
    sp_item_invoke_bbox(group, &paintbox, NR::identity(), TRUE);
    for (SPItemView *v = group->display; v != NULL; v = v->next) {
        group->BuildFlow(NR_ARENA_GROUP(v->arenaitem), &paintbox);
    }
}

static void
sp_flowtext_modified(SPObject */*object*/, guint /*flags*/)
{
}

static void
sp_flowtext_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr)
{
    sp_object_read_attr(object, "inkscape:layoutOptions");

    if (((SPObjectClass *) (parent_class))->build) {
        (* ((SPObjectClass *) (parent_class))->build)(object, document, repr);
    }
}

static void
sp_flowtext_set(SPObject *object, unsigned key, gchar const *value)
{
    SPFlowtext *group = (SPFlowtext *) object;

    switch (key) {
        case SP_ATTR_LAYOUT_OPTIONS: {
            SPCSSAttr *opts = sp_repr_css_attr((SP_OBJECT(group))->repr, "inkscape:layoutOptions");
            {
                gchar const *val = sp_repr_css_property(opts, "justification", NULL);
                if ( val == NULL ) {
                    group->justify = false;
                } else {
                    if ( strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ) {
                        group->justify = false;
                    } else {
                        group->justify = true;
                    }
                }
            }
            {
                gchar const *val = sp_repr_css_property(opts, "layoutAlgo", NULL);
                if ( val == NULL ) {
                    group->algo = 0;
                } else {
                    if ( strcmp(val, "better") == 0 ) {
                        group->algo = 2;
                    } else if ( strcmp(val, "simple") == 0 ) {
                        group->algo = 1;
                    } else if ( strcmp(val, "default") == 0 ) {
                        group->algo = 0;
                    }
                }
            }
            {
                gchar const *val = sp_repr_css_property(opts, "par-indent", NULL);
                if ( val == NULL ) {
                    group->par_indent = 0.0;
                } else {
                    sp_repr_get_double((Inkscape::XML::Node*)opts, "par-indent", &group->par_indent);
                }
            }
            sp_repr_css_attr_unref(opts);
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        }
        default:
            if (((SPObjectClass *) (parent_class))->set) {
                (* ((SPObjectClass *) (parent_class))->set)(object, key, value);
            }
            break;
    }
}

static Inkscape::XML::Node *
sp_flowtext_write(SPObject *object, Inkscape::XML::Node *repr, guint flags)
{
    if ( flags & SP_OBJECT_WRITE_BUILD ) {
        if ( repr == NULL ) repr = sp_repr_new("svg:flowRoot");
        GSList *l = NULL;
        for (SPObject *child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
            Inkscape::XML::Node *c_repr = NULL;
            if ( SP_IS_FLOWDIV(child) ) {
                c_repr = child->updateRepr(NULL, flags);
            } else if ( SP_IS_FLOWREGION(child) ) {
                c_repr = child->updateRepr(NULL, flags);
            } else if ( SP_IS_FLOWREGIONEXCLUDE(child) ) {
                c_repr = child->updateRepr(NULL, flags);
            }
            if ( c_repr ) l = g_slist_prepend(l, c_repr);
        }
        while ( l ) {
            sp_repr_add_child(repr, (Inkscape::XML::Node *) l->data, NULL);
            sp_repr_unref((Inkscape::XML::Node *) l->data);
            l = g_slist_remove(l, l->data);
        }
    } else {
        for (SPObject *child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
            if ( SP_IS_FLOWDIV(child) ) {
                child->updateRepr(flags);
            } else if ( SP_IS_FLOWREGION(child) ) {
                child->updateRepr(flags);
            } else if ( SP_IS_FLOWREGIONEXCLUDE(child) ) {
                child->updateRepr(flags);
            }
        }
    }

    if (((SPObjectClass *) (parent_class))->write)
        ((SPObjectClass *) (parent_class))->write(object, repr, flags);

    return repr;
}

static void
sp_flowtext_bbox(SPItem const *item, NRRect *bbox, NR::Matrix const &transform, unsigned const /*flags*/)
{
    SPFlowtext *group = SP_FLOWTEXT(item);
    group->layout.getBoundingBox(bbox, transform);
}

static void
sp_flowtext_print(SPItem *item, SPPrintContext *ctx)
{
    SPFlowtext *group = SP_FLOWTEXT(item);

    NRRect pbox;
    sp_item_invoke_bbox(item, &pbox, NR::identity(), TRUE);
    NRRect bbox;
    sp_item_bbox_desktop(item, &bbox);
    NRRect dbox;
    dbox.x0 = 0.0;
    dbox.y0 = 0.0;
    dbox.x1 = sp_document_width(SP_OBJECT_DOCUMENT(item));
    dbox.y1 = sp_document_height(SP_OBJECT_DOCUMENT(item));
    NRMatrix ctm;
    sp_item_i2d_affine(item, &ctm);

    group->layout.print(ctx, &pbox, &dbox, &bbox, ctm);
}


static gchar *sp_flowtext_description(SPItem *item)
{
    Inkscape::Text::Layout const &layout = SP_FLOWTEXT(item)->layout;
    return g_strdup_printf("<b>Flowed text</b> (%d characters)", layout.iteratorToCharIndex(layout.end()));
}

static NRArenaItem *
sp_flowtext_show(SPItem *item, NRArena *arena, unsigned/* key*/, unsigned /*flags*/)
{
    SPFlowtext *group = (SPFlowtext *) item;
    NRArenaGroup *flowed = NRArenaGroup::create(arena);
    nr_arena_group_set_transparent(flowed, FALSE);

    // pass the bbox of the flowtext object as paintbox (used for paintserver fills)	
    NRRect paintbox;
    sp_item_invoke_bbox(item, &paintbox, NR::identity(), TRUE);
    group->BuildFlow(flowed, &paintbox);

    return flowed;
}

static void
sp_flowtext_hide(SPItem *item, unsigned int key)
{
    if (((SPItemClass *) parent_class)->hide)
        ((SPItemClass *) parent_class)->hide(item, key);
}


/*
 *
 */

void SPFlowtext::ClearFlow(NRArenaGroup *in_arena)
{
    nr_arena_item_request_render(NR_ARENA_ITEM(in_arena));
    for (NRArenaItem *child = in_arena->children; child != NULL; ) {
        NRArenaItem *nchild = child->next;

        nr_arena_glyphs_group_clear(NR_ARENA_GLYPHS_GROUP(child));
        nr_arena_item_remove_child(NR_ARENA_ITEM(in_arena), child);

        child = nchild;
    }
}

void SPFlowtext::BuildFlow(NRArenaGroup *in_arena, NRRect *paintbox)
{
    layout.show(in_arena, paintbox);
}

void convert_to_text(void)
{
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (!SP_IS_DESKTOP(desktop)) return;
    SPSelection *selection = SP_DT_SELECTION(desktop);
    SPItem *item = selection->singleItem();
    if (!SP_IS_FLOWTEXT(item)) return;

    SPFlowtext *group = SP_FLOWTEXT(item);

    if (!group->layout.outputExists()) return;

    Inkscape::XML::Node *repr = sp_repr_new("svg:text");
    repr->setAttribute("xml:space", "preserve");
    repr->setAttribute("style", SP_OBJECT_REPR(group)->attribute("style"));

    for (Inkscape::Text::Layout::iterator it = group->layout.begin() ; it != group->layout.end() ; ) {
        
	    Inkscape::XML::Node *line_tspan = sp_repr_new("svg:tspan");
        line_tspan->setAttribute("sodipodi:role", "line");
        NR::Point anchor_point = group->layout.characterAnchorPoint(it);
        sp_repr_set_double(line_tspan, "x", anchor_point[0]);
        sp_repr_set_double(line_tspan, "y", anchor_point[1]);
        line_tspan->setAttribute("xml:space", "preserve");

        Inkscape::Text::Layout::iterator it_line_end = it;
        it_line_end.nextStartOfLine();
        while (it != it_line_end) {

	        Inkscape::XML::Node *span_tspan = sp_repr_new("svg:tspan");
            NR::Point anchor_point = group->layout.characterAnchorPoint(it);
            sp_repr_set_double(span_tspan, "x", anchor_point[0]);  // FIXME: this will pick up the wrong end of counter-directional runs
                                                                   //   (and Text::Layout will render it wrong too, two wrongs make a right :-) )
            sp_repr_set_double(span_tspan, "y", anchor_point[1]);
            // TODO: dx attributes from justification (or maybe letter-spacing and word-spacing)
            SPObject *source_obj;
            Glib::ustring::iterator span_text_start_iter;
            group->layout.getSourceOfCharacter(it, (void**)&source_obj, &span_text_start_iter);
            gchar *style_text = sp_style_write_difference((SP_IS_STRING(source_obj) ? source_obj->parent : source_obj)->style, group->style);
            if (style_text) {
                span_tspan->setAttribute("style", *style_text ? NULL : style_text);
                g_free(style_text);
            }
            span_tspan->setAttribute("xml:space", "preserve");

            it.nextStartOfSpan();
            if (SP_IS_STRING(source_obj)) {
                Glib::ustring *string = &SP_STRING(source_obj)->string;
                SPObject *span_end_obj;
                Glib::ustring::iterator span_text_end_iter;
                group->layout.getSourceOfCharacter(it, (void**)&span_end_obj, &span_text_end_iter);
                if (span_end_obj != source_obj) span_text_end_iter = string->end();    // spans will never straddle a source boundary

                if (span_text_start_iter != span_text_end_iter) {
                    Glib::ustring new_string;
                    while (span_text_start_iter != span_text_end_iter)
                        new_string += *span_text_start_iter++;    // grr. no substr() with iterators
                    Inkscape::XML::Node *new_text = sp_repr_new_text(new_string.c_str());
                    span_tspan->appendChild(new_text);
                    sp_repr_unref(new_text);
                }
            }

            line_tspan->appendChild(span_tspan);
	        sp_repr_unref(span_tspan);
        }
        repr->appendChild(line_tspan);
	    sp_repr_unref(line_tspan);
    }

    Inkscape::XML::Node *parent = SP_OBJECT_REPR(item)->parent();
    parent->appendChild(repr);
    SPItem *new_item = (SPItem *) SP_DT_DOCUMENT(desktop)->getObjectByRepr(repr);
    sp_item_write_transform(new_item, repr, item->transform);
    SP_OBJECT(new_item)->updateRepr();

    sp_repr_unref(repr);
    selection->setItem(new_item);
    item->deleteObject();

    sp_document_done(SP_DT_DOCUMENT(desktop));
    return;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
