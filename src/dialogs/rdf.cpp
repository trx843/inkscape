/**
 * \brief  RDF manipulation functions
 *
 * FIXME: move these to xml/ instead of dialogs/
 *
 * Authors:
 *   Kees Cook <kees@outflux.net>
 *
 * Copyright (C) Kees Cook 2004
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <config.h>

#include <stdio.h>

#include <string.h>
#include <glib.h>

#include "inkscape.h"
#include "document.h"
#include "xml/repr.h"
#include "rdf.h"

/*

   Example RDF XML from various places...
 
<rdf:RDF xmlns="http://web.resource.org/cc/"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<Work rdf:about="">
   <dc:title>title of work</dc:title>
   <dc:date>year</dc:date>
   <dc:description>description of work</dc:description>
   <dc:creator><Agent>
      <dc:title>creator</dc:title>
   </Agent></dc:creator>
   <dc:rights><Agent>
      <dc:title>holder</dc:title>
   </Agent></dc:rights>
   <dc:type rdf:resource="http://purl.org/dc/dcmitype/StillImage" />
   <dc:source rdf:resource="source"/>
   <license rdf:resource="http://creativecommons.org/licenses/by/2.0/" 
/>
</Work>


  <rdf:RDF xmlns="http://web.resource.org/cc/"
      xmlns:dc="http://purl.org/dc/elements/1.1/"
      xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <Work rdf:about="">
     <dc:title>SVG Road Signs</dc:title>
     <dc:rights><Agent>
        <dc:title>John Cliff</dc:title>
     </Agent></dc:rights>
     <dc:type rdf:resource="http://purl.org/dc/dcmitype/StillImage" />
     <license rdf:resource="http://web.resource.org/cc/PublicDomain" />
  </Work>
  
  <License rdf:about="http://web.resource.org/cc/PublicDomain">
     <permits rdf:resource="http://web.resource.org/cc/Reproduction" />
     <permits rdf:resource="http://web.resource.org/cc/Distribution" />
     <permits rdf:resource="http://web.resource.org/cc/DerivativeWorks" />
  </License>
  
</rdf:RDF>
*/


struct rdf_license_t rdf_licenses [] = {
    { "Creative Commons Attribution", 
          "http://creativecommons.org/licenses/by/2.0/",
      "\
   <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Notice\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Attribution\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/DerivativeWorks\" />\
",
    },

    { "Creative Commons Attribution-ShareAlike", 
          "http://creativecommons.org/licenses/by/2.0/",
      "\
   <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Notice\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Attribution\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/DerivativeWorks\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/ShareAlike\" />\
",
    },

    { "Creative Commons Attribution-NoDerivs", 
          "http://creativecommons.org/licenses/by/2.0/",
      "\
   <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Notice\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Attribution\" />\
",
    },

    { "Creative Commons Attribution-NonCommercial", 
          "http://creativecommons.org/licenses/by/2.0/",
      "\
   <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Notice\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Attribution\" />\
   <prohibits rdf:resource=\"http://web.resource.org/cc/CommercialUse\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/DerivativeWorks\" />\
",
    },

    { "Creative Commons Attribution-NonCommercial-ShareAlike", 
          "http://creativecommons.org/licenses/by/2.0/",
      "\
   <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Notice\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Attribution\" />\
   <prohibits rdf:resource=\"http://web.resource.org/cc/CommercialUse\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/DerivativeWorks\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/ShareAlike\" />\
",
    },

    { "Creative Commons Attribution-NonCommercial-NoDerivs", 
          "http://creativecommons.org/licenses/by/2.0/",
      "\
   <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
   <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Notice\" />\
   <requires rdf:resource=\"http://web.resource.org/cc/Attribution\" />\
   <prohibits rdf:resource=\"http://web.resource.org/cc/CommercialUse\" />\
",
    },

    { "Public Domain",
      "http://web.resource.org/cc/PublicDomain",
      "\
     <permits rdf:resource=\"http://web.resource.org/cc/Reproduction\" />\
     <permits rdf:resource=\"http://web.resource.org/cc/Distribution\" />\
     <permits rdf:resource=\"http://web.resource.org/cc/DerivativeWorks\" />\
",
    },

    { NULL, NULL, NULL }
};

#define XML_TAG_NAME_METADATA "metadata"
#define XML_TAG_NAME_RDF      "rdf:RDF"
#define XML_TAG_NAME_WORK     "cc:Work"

struct rdf_work_entity_t rdf_work_entities [] = {
    { "title", N_("Title"), "dc:title", RDF_CONTENT },
    { "date", N_("Date"), "dc:date", RDF_CONTENT },

    /* these are "agent" tags actually... */
    { "creator", N_("Creator"), "dc:creator", RDF_AGENT },
    { "owner", N_("Owner"), "dc:rights", RDF_AGENT },
    { "publisher", N_("Publisher"), "dc:publisher", RDF_AGENT },

    { "source", N_("Source"), "dc:source", RDF_CONTENT },
    { "keywords", N_("Keywords"), "dc:subject", RDF_CONTENT },

    /* this is a multi-line tag */
    { "description", N_("Description"), "dc:description", RDF_CONTENT },

    /* this uses an element */
    { "license", N_("License"), "cc:license", RDF_RESOURCE },
    
    { NULL, NULL, NULL, RDF_CONTENT }
};

/**
 *  \brief   Retrieves a known RDF/Work entity by name
 *  \return  A pointer to an RDF/Work entity
 *  \param   name  The desired RDF/Work entity
 *  
 */
struct rdf_work_entity_t *
rdf_find_entity(char * name)
{
    struct rdf_work_entity_t *entity;
    for (entity=rdf_work_entities; entity->name; entity++) {
        if (strcmp(entity->name,name)==0) break;
    }
    if (entity->name) return entity;
    return NULL;
}

/*
 * Takes the inkscape rdf struct and spits out a static RDF, which is only
 * useful for testing.  We must merge the rdf struct into the XML DOM for
 * changes to be saved.
 */
/*

   Since g_markup_printf_escaped doesn't exist for most people's glib
   right now, this function will remain commented out since it's only
   for generic debug anyway.  --Kees

gchar *
rdf_string(struct rdf_t * rdf)
{
    gulong overall=0;
    gchar *string=NULL;

    gchar *rdf_head="\
<rdf:RDF xmlns=\"http://web.resource.org/cc/\"\
    xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\
    xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\
";
    gchar *work_head="\
<Work rdf:about=\"\">\
   <dc:type rdf:resource=\"http://purl.org/dc/dcmitype/StillImage\" />\
";
    gchar *work_title=NULL;
    gchar *work_date=NULL;
    gchar *work_description=NULL;
    gchar *work_creator=NULL;
    gchar *work_owner=NULL;
    gchar *work_source=NULL;
    gchar *work_license=NULL;
    gchar *license_head=NULL;
    gchar *license=NULL;
    gchar *license_end="</License>\n";
    gchar *work_end="</Work>\n";
    gchar *rdf_end="</rdf:RDF>\n";

    if (rdf && rdf->work_title && rdf->work_title[0]) {
        work_title=g_markup_printf_escaped("   <dc:title>%s</dc:title>\n",
            rdf->work_title);
    overall+=strlen(work_title);
    }
    if (rdf && rdf->work_date && rdf->work_date[0]) {
        work_date=g_markup_printf_escaped("   <dc:date>%s</dc:date>\n",
            rdf->work_date);
    overall+=strlen(work_date);
    }
    if (rdf && rdf->work_description && rdf->work_description[0]) {
        work_description=g_markup_printf_escaped("   <dc:description>%s</dc:description>\n",
            rdf->work_description);
    overall+=strlen(work_description);
    }
    if (rdf && rdf->work_creator && rdf->work_creator[0]) {
        work_creator=g_markup_printf_escaped("   <dc:creator><Agent>\
      <dc:title>%s</dc:title>\
   </Agent></dc:creator>\n",
            rdf->work_creator);
    overall+=strlen(work_creator);
    }
    if (rdf && rdf->work_owner && rdf->work_owner[0]) {
        work_owner=g_markup_printf_escaped("   <dc:rights><Agent>\
      <dc:title>%s</dc:title>\
   </Agent></dc:rights>\n",
            rdf->work_owner);
    overall+=strlen(work_owner);
    }
    if (rdf && rdf->work_source && rdf->work_source[0]) {
        work_source=g_markup_printf_escaped("   <dc:source rdf:resource=\"%s\" />\n",
            rdf->work_source);
    overall+=strlen(work_source);
    }
    if (rdf && rdf->license && rdf->license->work_rdf && rdf->license->work_rdf[0]) {
        work_license=g_markup_printf_escaped("   <license rdf:resource=\"%s\" />\n",
            rdf->license->work_rdf);
    overall+=strlen(work_license);

    license_head=g_markup_printf_escaped("<License rdf:about=\"%s\">\n",
            rdf->license->work_rdf);
    overall+=strlen(license_head);
    overall+=strlen(rdf->license->license_rdf);
    overall+=strlen(license_end);
    }

    overall+=strlen(rdf_head)+strlen(rdf_end);
    overall+=strlen(work_head)+strlen(work_end);

    overall++; // NULL term

    if (!(string=(gchar*)g_malloc(overall))) {
        return NULL;
    }

    string[0]='\0';
    strcat(string,rdf_head);
    strcat(string,work_head);

    if (work_title)       strcat(string,work_title);
    if (work_date)        strcat(string,work_date);
    if (work_description) strcat(string,work_description);
    if (work_creator)     strcat(string,work_creator);
    if (work_owner)       strcat(string,work_owner);
    if (work_source)      strcat(string,work_source);
    if (work_license)     strcat(string,work_license);

    strcat(string,work_end);
    if (license_head) {
        strcat(string,license_head);
    strcat(string,rdf->license->license_rdf);
    strcat(string,license_end);
    }
    strcat(string,rdf_end);

    return string;
}
*/


/**
 *  \brief   Pull the text out of an RDF entity, depends on how it's stored
 *  \return  A pointer to the entity's static contents as a string
 *  \param   repr    The XML element to extract from
 *  \param   entity  The desired RDF/Work entity
 *  
 */
const gchar *
rdf_get_repr_text ( SPRepr * repr, struct rdf_work_entity_t * entity )
{
    g_return_val_if_fail (repr != NULL, NULL);
    g_return_val_if_fail (entity != NULL, NULL);

    //printf("Getting '%s' contents from '%s'\n",entity->tag,SP_REPR_NAME(repr));

    SPRepr * temp=NULL;
    switch (entity->datatype) {
        case RDF_CONTENT:
            temp = sp_repr_children(repr);
            if ( temp == NULL ) return "";
            //g_return_val_if_fail (temp != NULL, NULL);
            return sp_repr_content(temp);
        case RDF_AGENT:
            temp = sp_repr_lookup_name ( repr, "cc:Agent" );
            if ( temp == NULL ) return "";
            //g_return_val_if_fail (temp != NULL, NULL);

            temp = sp_repr_lookup_name ( temp, "dc:title" );
            if ( temp == NULL ) return "";
            //g_return_val_if_fail (temp != NULL, NULL);

            temp = sp_repr_children(temp);
            if ( temp == NULL ) return "";
            //g_return_val_if_fail (temp != NULL, NULL);
            return sp_repr_content(temp);
        case RDF_RESOURCE:
            return sp_repr_attr(repr, "rdf:resource");
    }
    return "";
}

unsigned int
rdf_set_repr_text ( SPRepr * repr,
                    struct rdf_work_entity_t * entity,
                    gchar const * text )
{
    g_return_val_if_fail ( repr != NULL, 0);
    g_return_val_if_fail ( entity != NULL, 0);
    g_return_val_if_fail ( text != NULL, 0);

    SPRepr * temp=NULL;
    SPRepr * parent=repr;
    switch (entity->datatype) {
        case RDF_CONTENT:
            temp = sp_repr_children(parent);
            if ( temp == NULL ) {
                temp = sp_repr_new_text( text );
                g_return_val_if_fail (temp != NULL, 0);

                sp_repr_append_child ( parent, temp );
                sp_repr_unref ( temp );

                return TRUE;
            }
            else {
                return sp_repr_set_content( temp, text );
            }

        case RDF_AGENT:
            temp = sp_repr_lookup_name ( parent, "cc:Agent" );
            if ( temp == NULL ) {
                temp = sp_repr_new ( "cc:Agent" );
                g_return_val_if_fail (temp != NULL, 0);

                sp_repr_append_child ( parent, temp );
                sp_repr_unref ( temp );
            }
            parent = temp;

            temp = sp_repr_lookup_name ( parent, "dc:title" );
            if ( temp == NULL ) {
                temp = sp_repr_new ( "dc:title" );
                g_return_val_if_fail (temp != NULL, 0);

                sp_repr_append_child ( parent, temp );
                sp_repr_unref ( temp );
            }
            parent = temp;

            temp = sp_repr_children(parent);
            if ( temp == NULL ) {
                temp = sp_repr_new_text( text );
                g_return_val_if_fail (temp != NULL, 0);

                sp_repr_append_child ( parent, temp );
                sp_repr_unref ( temp );

                return TRUE;
            }
            else {
                return sp_repr_set_content( temp, text );
            }

        case RDF_RESOURCE:
            return sp_repr_set_attr ( parent, "rdf:resource", text );
    }
    return 0;
}


SPRepr *
rdf_get_work_repr( SPDocument * doc, gchar const * name, bool build )
{
    g_return_val_if_fail (name       != NULL, NULL);
    g_return_val_if_fail (doc        != NULL, NULL);
    g_return_val_if_fail (doc->rroot != NULL, NULL);

    SPRepr * rdf = sp_repr_lookup_name ( doc->rroot,
                                          XML_TAG_NAME_RDF );

    if (rdf == NULL) {
        if (!build) return NULL;

        SPRepr * svg = sp_repr_lookup_name ( doc->rroot,
                                             "svg" );
        g_return_val_if_fail ( svg != NULL, NULL );

        SPRepr * parent = sp_repr_lookup_name ( svg, XML_TAG_NAME_METADATA );
        if ( parent == NULL ) {
            parent = sp_repr_new( XML_TAG_NAME_METADATA );
            g_return_val_if_fail ( parent != NULL, NULL);

            sp_repr_append_child(svg, parent);
            sp_repr_unref(parent);
        }

        rdf = sp_repr_new( XML_TAG_NAME_RDF );
        g_return_val_if_fail (rdf != NULL, NULL);

        sp_repr_append_child(parent, rdf);
        sp_repr_unref(rdf);
    }

    /*
     * some implementations do not put RDF stuff inside <metadata>,
     * so we need to check for it and add it if we don't see it
     */
    SPRepr * want_metadata = sp_repr_parent ( rdf );
    g_return_val_if_fail (want_metadata != NULL, NULL);
    if (strcmp( sp_repr_name(want_metadata), XML_TAG_NAME_METADATA )) {
            SPRepr * metadata = sp_repr_new( XML_TAG_NAME_METADATA );
            g_return_val_if_fail (metadata != NULL, NULL);

            /* attach the metadata node */
            sp_repr_append_child ( want_metadata, metadata );
            sp_repr_unref ( metadata );

            /* move the RDF into it */
            sp_repr_unparent ( rdf );
            sp_repr_append_child ( metadata, rdf );
    }
    
    SPRepr * work = sp_repr_lookup_name ( rdf, XML_TAG_NAME_WORK );
    if (work == NULL) {
        if (!build) return NULL;

        work = sp_repr_new( XML_TAG_NAME_WORK );
        g_return_val_if_fail (work != NULL, NULL);

        sp_repr_append_child(rdf, work);
        sp_repr_unref(work);
    }

    SPRepr * item = sp_repr_lookup_name ( work, name );
    if (item == NULL) {
        if (!build) return NULL;

        item = sp_repr_new( name );
        g_return_val_if_fail (item != NULL, NULL);

        sp_repr_append_child(work, item);
        sp_repr_unref(item);
    }

    return item;
}



/**
 *  \brief   Retrieves a known RDF/Work entity's contents from the document XML by name
 *  \return  A pointer to the entity's static contents as a string
 *  \param   entity  The desired RDF/Work entity
 *  
 */
const gchar *
rdf_get_work_entity(SPDocument * doc, struct rdf_work_entity_t * entity)
{
    g_return_val_if_fail (doc    != NULL, NULL);
    g_return_val_if_fail (entity != NULL, NULL);
    //printf("want '%s'\n",entity->title);

    SPRepr * item = rdf_get_work_repr( doc, entity->tag, FALSE );
    if ( item == NULL ) return "";
    //g_return_val_if_fail (item != NULL, NULL);

    const gchar * result = rdf_get_repr_text ( item, entity );
    //printf("found '%s' == '%s'\n", entity->title, result );
    return result;
}

/**
 *  \brief   Stores a string into a named RDF/Work entity in the document XML
 *  \param   entity The desired RDF/Work entity to replace
 *  \param   string The string to replace the entity contents with
 *  
 */
unsigned int
rdf_set_work_entity(SPDocument * doc, struct rdf_work_entity_t * entity,
                    const gchar * text)
{
    g_return_val_if_fail ( entity != NULL, 0 );
    g_return_val_if_fail ( text   != NULL, 0 );

    /*
    printf("need to change '%s' (%s) to '%s'\n",
        entity->title,
        entity->tag,
        text);
    */

    SPRepr * item = rdf_get_work_repr( doc, entity->tag, TRUE );
    g_return_val_if_fail ( item != NULL, 0 );

    return rdf_set_repr_text ( item, entity, text );
}

/**
 *  \brief   Attempts to match and retrieve a known RDF/License from the document XML
 *  \return  A pointer to the static RDF license structure
 *  
 */
struct rdf_license_t *
rdf_get_license()
{
    /* obviously, we need to also pass the document and then extract the
     * desired RDF entity from the XML */
    return NULL;
}

/**
 *  \brief   Stores an RDF/License XML in the document XML
 *  \param   license   The desired RDF/License structure to store
 *  
 */
void
rdf_set_license(struct rdf_license_t * license)
{
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
