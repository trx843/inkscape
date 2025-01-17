// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2004-2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "find.h"

#include <glibmm/i18n.h>
#include <glibmm/regex.h>
#include <gtkmm/entry.h>
#include <gtkmm/enums.h>
#include <gtkmm/sizegroup.h>

#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "inkscape.h"
#include "layer-manager.h"
#include "message-stack.h"
#include "selection.h"
#include "selection-chemistry.h"
#include "text-editing.h"
#include "object/sp-defs.h"
#include "object/sp-ellipse.h"
#include "object/sp-flowdiv.h"
#include "object/sp-flowtext.h"
#include "object/sp-image.h"
#include "object/sp-line.h"
#include "object/sp-offset.h"
#include "object/sp-path.h"
#include "object/sp-polyline.h"
#include "object/sp-rect.h"
#include "object/sp-root.h"
#include "object/sp-spiral.h"
#include "object/sp-star.h"
#include "object/sp-text.h"
#include "object/sp-tref.h"
#include "object/sp-tspan.h"
#include "object/sp-use.h"
#include "ui/dialog-events.h"
#include "ui/icon-names.h"
#include "ui/pack.h"
#include "xml/attribute-record.h"
#include "xml/node-iterators.h"

namespace Inkscape::UI::Dialog {

Find::Find()
    : DialogBase("/dialogs/find", "Find"),

      entry_find(_("F_ind:"), _("Find objects by their content or properties (exact or partial match)")),
      entry_replace(_("R_eplace:"), _("Replace match with this value")),
      label_group{Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL)},

      check_scope_all(_("_All")),
      check_scope_layer(_("Current _layer")),
      check_scope_selection(_("Sele_ction")),
      check_searchin_text(_("_Text")),
      check_searchin_property(_("_Properties")),
      frame_searchin(_("Search in")),
      frame_scope(_("Scope")),

      check_case_sensitive(_("Case sensiti_ve")),
      check_exact_match(_("E_xact match")),
      check_include_hidden(_("Include _hidden")),
      check_include_locked(_("Include loc_ked")),
      expander_options(_("Options")),
      frame_options(_("General")),

      check_ids(_("_ID")),
      check_attributename(_("Attribute _name")),
      check_attributevalue(_("Attri_bute value")),
      check_style(_("_Style")),
      check_font(_("F_ont")),
      check_desc(_("_Desc")),
      check_title(_("Title")),
      frame_properties(_("Properties")),

      check_alltypes(_("All types")),
      check_rects(_("Rectangles")),
      check_ellipses(_("Ellipses")),
      check_stars(_("Stars")),
      check_spirals(_("Spirals")),
      check_paths(_("Paths")),
      check_texts(_("Texts")),
      check_groups(_("Groups")),
      check_clones(
                    //TRANSLATORS: "Clones" is a noun indicating type of object to find
                    C_("Find dialog", "Clones")),

      check_images(_("Images")),
      check_offsets(_("Offsets")),
      frame_types(_("Object types")),

      _left_size_group(Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL)),
      _right_size_group(Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL)),

      status(""),
      button_find(_("_Find")),
      button_replace(_("_Replace All")),
      _action_replace(false),
      blocked(false),

      hbox_searchin(Gtk::Orientation::HORIZONTAL),
      vbox_scope(Gtk::Orientation::VERTICAL),
      vbox_searchin(Gtk::Orientation::VERTICAL),
      vbox_options1(Gtk::Orientation::VERTICAL),
      vbox_options2(Gtk::Orientation::VERTICAL),
      hbox_options(Gtk::Orientation::HORIZONTAL),
      vbox_expander(Gtk::Orientation::VERTICAL),
      hbox_properties(Gtk::Orientation::HORIZONTAL),
      vbox_properties1(Gtk::Orientation::VERTICAL),
      vbox_properties2(Gtk::Orientation::VERTICAL),
      vbox_types1(Gtk::Orientation::VERTICAL),
      vbox_types2(Gtk::Orientation::VERTICAL),
      hbox_types(Gtk::Orientation::HORIZONTAL),
      hboxbutton_row(Gtk::Orientation::HORIZONTAL)
{
    auto const label1 = entry_find.getLabel();
    entry_find.getEntry()->set_hexpand();
    entry_find.getEntry()->set_halign(Gtk::Align::FILL);
    label_group->add_widget(*label1);
    label1->set_xalign(0);
    label1->set_hexpand(false);
    auto const label2 = entry_replace.getLabel();
    entry_replace.getEntry()->set_hexpand();
    entry_replace.getEntry()->set_halign(Gtk::Align::FILL);
    label_group->add_widget(*label2);
    label2->set_xalign(0);
    label2->set_hexpand(false);

    static constexpr int MARGIN = 4;
    set_margin_start(MARGIN);
    set_margin_end(MARGIN);
    entry_find.set_margin_top(MARGIN);
    entry_replace.set_margin_top(MARGIN);
    frame_searchin.set_margin_top(MARGIN);
    frame_scope.set_margin_top(MARGIN);
    button_find.set_use_underline();
    button_find.set_tooltip_text(_("Select all objects matching the selection criteria"));
    button_replace.set_use_underline();
    button_replace.set_tooltip_text(_("Replace all matches"));
    check_scope_all.set_use_underline();
    check_scope_all.set_tooltip_text(_("Search in all layers"));
    check_scope_layer.set_use_underline();
    check_scope_layer.set_tooltip_text(_("Limit search to the current layer"));
    check_scope_selection.set_use_underline();
    check_scope_selection.set_tooltip_text(_("Limit search to the current selection"));
    check_searchin_text.set_use_underline();
    check_searchin_text.set_tooltip_text(_("Search in text objects"));
    check_searchin_property.set_use_underline();
    check_searchin_property.set_tooltip_text(_("Search in object properties, styles, attributes and IDs"));
    check_case_sensitive.set_use_underline();
    check_case_sensitive.set_tooltip_text(_("Match upper/lower case"));
    check_case_sensitive.set_active(false);
    check_exact_match.set_use_underline();
    check_exact_match.set_tooltip_text(_("Match whole objects only"));
    check_exact_match.set_active(false);
    check_include_hidden.set_use_underline();
    check_include_hidden.set_tooltip_text(_("Include hidden objects in search"));
    check_include_hidden.set_active(false);
    check_include_locked.set_use_underline();
    check_include_locked.set_tooltip_text(_("Include locked objects in search"));
    check_include_locked.set_active(false);
    check_ids.set_use_underline();
    check_ids.set_tooltip_text(_("Search ID name"));
    check_ids.set_active(true);
    check_attributename.set_use_underline();
    check_attributename.set_tooltip_text(_("Search attribute name"));
    check_attributename.set_active(false);
    check_attributevalue.set_use_underline();
    check_attributevalue.set_tooltip_text(_("Search attribute value"));
    check_attributevalue.set_active(true);
    check_style.set_use_underline();
    check_style.set_tooltip_text(_("Search style"));
    check_style.set_active(true);
    check_font.set_use_underline();
    check_font.set_tooltip_text(_("Search fonts"));
    check_font.set_active(false);
    check_desc.set_use_underline();
    check_desc.set_tooltip_text(_("Search description"));
    check_desc.set_active(false);
    check_title.set_use_underline();
    check_title.set_tooltip_text(_("Search title"));
    check_title.set_active(false);
    check_alltypes.set_use_underline();
    check_alltypes.set_tooltip_text(_("Search all object types"));
    check_alltypes.set_active(true);
    check_rects.set_use_underline();
    check_rects.set_tooltip_text(_("Search rectangles"));
    check_rects.set_active(false);
    check_ellipses.set_use_underline();
    check_ellipses.set_tooltip_text(_("Search ellipses, arcs, circles"));
    check_ellipses.set_active(false);
    check_stars.set_use_underline();
    check_stars.set_tooltip_text(_("Search stars and polygons"));
    check_stars.set_active(false);
    check_spirals.set_use_underline();
    check_spirals.set_tooltip_text(_("Search spirals"));
    check_spirals.set_active(false);
    check_paths.set_use_underline();
    check_paths.set_tooltip_text(_("Search paths, lines, polylines"));
    check_paths.set_active(false);
    check_texts.set_use_underline();
    check_texts.set_tooltip_text(_("Search text objects"));
    check_texts.set_active(false);
    check_groups.set_use_underline();
    check_groups.set_tooltip_text(_("Search groups"));
    check_groups.set_active(false),
    check_clones.set_use_underline();
    check_clones.set_tooltip_text(_("Search clones"));
    check_clones.set_active(false);
    check_images.set_use_underline();
    check_images.set_tooltip_text(_("Search images"));
    check_images.set_active(false);
    check_offsets.set_use_underline();
    check_offsets.set_tooltip_text(_("Search offset objects"));
    check_offsets.set_active(false);

    entry_find.getEntry()->set_width_chars(25);
    entry_replace.getEntry()->set_width_chars(25);

    check_searchin_property.set_group(check_searchin_text);
    UI::pack_start(vbox_searchin, check_searchin_text, UI::PackOptions::shrink);
    UI::pack_start(vbox_searchin, check_searchin_property, UI::PackOptions::shrink);
    frame_searchin.set_child(vbox_searchin);

    check_scope_layer    .set_group(check_scope_all);
    check_scope_selection.set_group(check_scope_all);
    UI::pack_start(vbox_scope, check_scope_all, UI::PackOptions::shrink);
    UI::pack_start(vbox_scope, check_scope_layer, UI::PackOptions::shrink);
    UI::pack_start(vbox_scope, check_scope_selection, UI::PackOptions::shrink);
    hbox_searchin.set_spacing(12);
    UI::pack_start(hbox_searchin, frame_searchin, UI::PackOptions::shrink);
    UI::pack_start(hbox_searchin, frame_scope, UI::PackOptions::shrink);
    frame_scope.set_child(vbox_scope);

    UI::pack_start(vbox_options1, check_case_sensitive, UI::PackOptions::shrink);
    UI::pack_start(vbox_options1, check_include_hidden, UI::PackOptions::shrink);
    UI::pack_start(vbox_options2, check_exact_match, UI::PackOptions::shrink);
    UI::pack_start(vbox_options2, check_include_locked, UI::PackOptions::shrink);
    _left_size_group->add_widget(check_case_sensitive);
    _left_size_group->add_widget(check_include_hidden);
    _right_size_group->add_widget(check_exact_match);
    _right_size_group->add_widget(check_include_locked);
    hbox_options.set_spacing(4);
    UI::pack_start(hbox_options, vbox_options1, UI::PackOptions::shrink);
    UI::pack_start(hbox_options, vbox_options2, UI::PackOptions::shrink);
    frame_options.set_child(hbox_options);

    UI::pack_start(vbox_properties1, check_ids, UI::PackOptions::shrink);
    UI::pack_start(vbox_properties1, check_style, UI::PackOptions::shrink);
    UI::pack_start(vbox_properties1, check_font, UI::PackOptions::shrink);
    UI::pack_start(vbox_properties1, check_desc, UI::PackOptions::shrink);
    UI::pack_start(vbox_properties1, check_title, UI::PackOptions::shrink);
    UI::pack_start(vbox_properties2, check_attributevalue, UI::PackOptions::shrink);
    UI::pack_start(vbox_properties2, check_attributename, UI::PackOptions::shrink);
    vbox_properties2.set_valign(Gtk::Align::START);
    _left_size_group->add_widget(check_ids);
    _left_size_group->add_widget(check_style);
    _left_size_group->add_widget(check_font);
    _left_size_group->add_widget(check_desc);
    _left_size_group->add_widget(check_title);
    _right_size_group->add_widget(check_attributevalue);
    _right_size_group->add_widget(check_attributename);
    hbox_properties.set_spacing(4);
    UI::pack_start(hbox_properties, vbox_properties1, UI::PackOptions::shrink);
    UI::pack_start(hbox_properties, vbox_properties2, UI::PackOptions::shrink);
    frame_properties.set_child(hbox_properties);

    UI::pack_start(vbox_types1, check_alltypes, UI::PackOptions::shrink);
    UI::pack_start(vbox_types1, check_paths, UI::PackOptions::shrink);
    UI::pack_start(vbox_types1, check_texts, UI::PackOptions::shrink);
    UI::pack_start(vbox_types1, check_groups, UI::PackOptions::shrink);
    UI::pack_start(vbox_types1, check_clones, UI::PackOptions::shrink);
    UI::pack_start(vbox_types1, check_images, UI::PackOptions::shrink);
    UI::pack_start(vbox_types2, check_offsets, UI::PackOptions::shrink);
    UI::pack_start(vbox_types2, check_rects, UI::PackOptions::shrink);
    UI::pack_start(vbox_types2, check_ellipses, UI::PackOptions::shrink);
    UI::pack_start(vbox_types2, check_stars, UI::PackOptions::shrink);
    UI::pack_start(vbox_types2, check_spirals, UI::PackOptions::shrink);
    vbox_types2.set_valign(Gtk::Align::END);
    _left_size_group->add_widget(check_alltypes);
    _left_size_group->add_widget(check_paths);
    _left_size_group->add_widget(check_texts);
    _left_size_group->add_widget(check_groups);
    _left_size_group->add_widget(check_clones);
    _left_size_group->add_widget(check_images);
    _right_size_group->add_widget(check_offsets);
    _right_size_group->add_widget(check_rects);
    _right_size_group->add_widget(check_ellipses);
    _right_size_group->add_widget(check_stars);
    _right_size_group->add_widget(check_spirals);
    hbox_types.set_spacing(4);
    UI::pack_start(hbox_types, vbox_types1, UI::PackOptions::shrink);
    UI::pack_start(hbox_types, vbox_types2, UI::PackOptions::shrink);
    frame_types.set_child(hbox_types);

    vbox_expander.set_spacing(4);
    UI::pack_start(vbox_expander, frame_options, true, true);
    UI::pack_start(vbox_expander, frame_properties, true, true);
    UI::pack_start(vbox_expander, frame_types, true, true);

    expander_options.set_use_underline();
    expander_options.set_child(vbox_expander);

    box_buttons.set_spacing(6);
    box_buttons.set_homogeneous(true);
    UI::pack_end(box_buttons, button_replace, false, true);
    UI::pack_end(box_buttons, button_find, false, true);
    hboxbutton_row.set_spacing(6);
    UI::pack_start(hboxbutton_row, status, true, true);
    UI::pack_end(hboxbutton_row, box_buttons, false, true);

    set_spacing(6);
    UI::pack_start(*this, entry_find, false, false);
    UI::pack_start(*this, entry_replace, false, false);
    UI::pack_start(*this, hbox_searchin, false, false);
    UI::pack_start(*this, expander_options, false, false);
    UI::pack_end(*this, hboxbutton_row, false, false);

    checkProperties.push_back(&check_ids);
    checkProperties.push_back(&check_style);
    checkProperties.push_back(&check_font);
    checkProperties.push_back(&check_desc);
    checkProperties.push_back(&check_title);
    checkProperties.push_back(&check_attributevalue);
    checkProperties.push_back(&check_attributename);

    checkTypes.push_back(&check_paths);
    checkTypes.push_back(&check_texts);
    checkTypes.push_back(&check_groups);
    checkTypes.push_back(&check_clones);
    checkTypes.push_back(&check_images);
    checkTypes.push_back(&check_offsets);
    checkTypes.push_back(&check_rects);
    checkTypes.push_back(&check_ellipses);
    checkTypes.push_back(&check_stars);
    checkTypes.push_back(&check_spirals);

    // set signals to handle clicks
    expander_options.property_expanded().signal_changed().connect(sigc::mem_fun(*this, &Find::onExpander));
    button_find.signal_clicked().connect(sigc::mem_fun(*this, &Find::onFind));
    button_replace.signal_clicked().connect(sigc::mem_fun(*this, &Find::onReplace));
    check_searchin_text.signal_toggled().connect(sigc::mem_fun(*this, &Find::onSearchinText));
    check_searchin_property.signal_toggled().connect(sigc::mem_fun(*this, &Find::onSearchinProperty));
    check_alltypes.signal_toggled().connect(sigc::mem_fun(*this, &Find::onToggleAlltypes));

    for (auto & checkProperty : checkProperties) {
        checkProperty->signal_toggled().connect(sigc::mem_fun(*this, &Find::onToggleCheck));
    }

    for (auto & checkType : checkTypes) {
        checkType->signal_toggled().connect(sigc::mem_fun(*this, &Find::onToggleCheck));
    }

    onSearchinText();
    onToggleAlltypes();

    button_find.set_receives_default();
    //button_find.grab_default(); // activatable by Enter

    entry_find.getEntry()->grab_focus();
}

Find::~Find() = default;

void Find::desktopReplaced()
{
    if (auto selection = getSelection()) {
        SPItem *item = selection->singleItem();
        if (item && entry_find.getEntry()->get_text_length() == 0) {
            Glib::ustring str = sp_te_get_string_multiline(item);
            if (!str.empty()) {
                entry_find.getEntry()->set_text(str);
            }
        }
    }
}

void Find::selectionChanged(Selection *selection)
{
    if (!blocked) {
        status.set_text("");
    }
}

/*########################################################################
# FIND helper functions
########################################################################*/

Glib::ustring Find::find_replace(const gchar *str, const gchar *find, const gchar *replace, bool exact, bool casematch, bool replaceall)
{
    Glib::ustring ustr = str;
    Glib::ustring ufind = find;
    gsize replace_length = Glib::ustring(replace).length();
    if (!casematch) {
        ufind = ufind.lowercase();
    }
    gsize n = find_strcmp_pos(ustr.c_str(), ufind.c_str(), exact, casematch);
    while (n != std::string::npos) {
        ustr.replace(n, ufind.length(), replace);
        if (!replaceall) {
            return ustr;
        }
        // Start the next search after the last replace character to avoid infinite loops (replace "a" with "aaa" etc)
        n = find_strcmp_pos(ustr.c_str(), ufind.c_str(), exact, casematch, n + replace_length);
    }
    return ustr;
}

gsize Find::find_strcmp_pos(const gchar *str, const gchar *find, bool exact, bool casematch, gsize start/*=0*/)
{
    Glib::ustring ustr = str ? str : "";
    Glib::ustring ufind = find;

    if (!casematch) {
        ustr = ustr.lowercase();
        ufind = ufind.lowercase();
    }

    gsize pos = std::string::npos;
    if (exact) {
        if (ustr == ufind) {
            pos = 0;
        }
    } else {
        pos = ustr.find(ufind, start);
    }

    return pos;
}


bool Find::find_strcmp(const gchar *str, const gchar *find, bool exact, bool casematch)
{
    return (std::string::npos != find_strcmp_pos(str, find, exact, casematch));
}

bool Find::item_desc_match (SPItem *item, const gchar *text, bool exact, bool casematch, bool replace)
{
    gchar* desc  = item->desc();
    bool found = find_strcmp(desc, text, exact, casematch);
    if (found && replace) {
        Glib::ustring r = find_replace(desc, text, entry_replace.getEntry()->get_text().c_str(), exact, casematch, replace);
        item->setDesc(r.c_str());
    }
    g_free(desc);
    return found;
}

bool Find::item_title_match (SPItem *item, const gchar *text, bool exact, bool casematch, bool replace)
{
    gchar* title = item->title();
    bool found = find_strcmp(title, text, exact, casematch);
    if (found && replace) {
        Glib::ustring r = find_replace(title, text, entry_replace.getEntry()->get_text().c_str(), exact, casematch, replace);
        item->setTitle(r.c_str());
    }
    g_free(title);
    return found;
}

bool Find::item_text_match (SPItem *item, const gchar *find, bool exact, bool casematch, bool replace/*=false*/)
{
    if (item->getRepr() == nullptr) {
        return false;
    }

    Glib::ustring item_text = sp_te_get_string_multiline(item);

    if (!item_text.empty()) {
        bool found = find_strcmp(item_text.c_str(), find, exact, casematch);

        if (found && replace) {
            Glib::ustring ufind = find;
            if (!casematch) {
                ufind = ufind.lowercase();
            }

            Inkscape::Text::Layout const *layout = te_get_layout (item);
            if (!layout) {
                return found;
            }

            Glib::ustring replace = entry_replace.getEntry()->get_text();
            gsize n = find_strcmp_pos(item_text.c_str(), ufind.c_str(), exact, casematch);
            static Inkscape::Text::Layout::iterator _begin_w;
            static Inkscape::Text::Layout::iterator _end_w;
            while (n != std::string::npos) {
                _begin_w = layout->charIndexToIterator(n);
                _end_w = layout->charIndexToIterator(n + ufind.length());
                sp_te_replace(item, _begin_w, _end_w, replace.c_str());
                item_text = sp_te_get_string_multiline (item);
                n = find_strcmp_pos(item_text.c_str(), ufind.c_str(), exact, casematch, n + replace.length());
            }
        }

        return found;
    }
    return false;
}


bool Find::item_id_match (SPItem *item, const gchar *id, bool exact, bool casematch, bool replace/*=false*/)
{
    if (!item->getRepr()) {
        return false;
    }

    const gchar *item_id = item->getRepr()->attribute("id");
    if (!item_id) {
        return false;
    }

    bool found = find_strcmp(item_id, id, exact, casematch);

    if (found && replace) {
        gchar * replace_text  = g_strdup(entry_replace.getEntry()->get_text().c_str());
        Glib::ustring new_item_style = find_replace(item_id, id, replace_text , exact, casematch, true);
        if (new_item_style != item_id) {
            item->setAttribute("id", new_item_style);
        }
        g_free(replace_text);
    }

    return found;
}

bool Find::item_style_match (SPItem *item, const gchar *text, bool exact, bool casematch, bool replace/*=false*/)
{
    if (item->getRepr() == nullptr) {
        return false;
    }

    gchar *item_style = g_strdup(item->getRepr()->attribute("style"));
    if (item_style == nullptr) {
        return false;
    }

    bool found = find_strcmp(item_style, text, exact, casematch);

    if (found && replace) {
        gchar * replace_text  = g_strdup(entry_replace.getEntry()->get_text().c_str());
        Glib::ustring new_item_style = find_replace(item_style, text, replace_text , exact, casematch, true);
        if (new_item_style != item_style) {
            item->setAttribute("style", new_item_style);
        }
        g_free(replace_text);
    }

    g_free(item_style);
    return found;
}

bool Find::item_attr_match(SPItem *item, const gchar *text, bool exact, bool /*casematch*/, bool replace/*=false*/)
{
    bool found = false;

    if (item->getRepr() == nullptr) {
        return false;
    }

    gchar *attr_value = g_strdup(item->getRepr()->attribute(text));
    if (exact) {
        found =  (attr_value != nullptr);
    } else {
        found = item->getRepr()->matchAttributeName(text);
    }
    g_free(attr_value);

    // TODO - Rename attribute name ?
    if (found && replace) {
        found = false;
    }

    return found;
}

bool Find::item_attrvalue_match(SPItem *item, const gchar *text, bool exact, bool casematch, bool replace/*=false*/)
{
    bool ret = false;

    if (item->getRepr() == nullptr) {
        return false;
    }

    for (const auto & iter:item->getRepr()->attributeList()) {
        const gchar* key = g_quark_to_string(iter.key);
        gchar *attr_value = g_strdup(item->getRepr()->attribute(key));
        bool found = find_strcmp(attr_value, text, exact, casematch);
        if (found) {
            ret = true;
        }

        if (found && replace) {
            gchar * replace_text  = g_strdup(entry_replace.getEntry()->get_text().c_str());
            Glib::ustring new_item_style = find_replace(attr_value, text, replace_text , exact, casematch, true);
            if (new_item_style != attr_value) {
                item->setAttribute(key, new_item_style);
            }
        }

        g_free(attr_value);
    }

    return ret;
}


bool Find::item_font_match(SPItem *item, const gchar *text, bool exact, bool casematch, bool /*replace*/ /*=false*/)
{
    bool ret = false;

    if (item->getRepr() == nullptr) {
        return false;
    }

    const gchar *item_style = item->getRepr()->attribute("style");
    if (item_style == nullptr) {
        return false;
    }

    std::vector<Glib::ustring> vFontTokenNames;
    vFontTokenNames.emplace_back("font-family:");
    vFontTokenNames.emplace_back("-inkscape-font-specification:");

    std::vector<Glib::ustring> vStyleTokens = Glib::Regex::split_simple(";", item_style);
    for (auto & vStyleToken : vStyleTokens) {
        Glib::ustring token = vStyleToken;
        for (const auto & vFontTokenName : vFontTokenNames) {
            if ( token.find(vFontTokenName) != std::string::npos) {
                Glib::ustring font1 = Glib::ustring(vFontTokenName).append(text);
                bool found = find_strcmp(token.c_str(), font1.c_str(), exact, casematch);
                if (found) {
                    ret = true;
                    if (_action_replace) {
                        gchar *replace_text  = g_strdup(entry_replace.getEntry()->get_text().c_str());
                        gchar *orig_str = g_strdup(token.c_str());
                        // Exact match fails since the "font-family:" is in the token, since the find was exact it still works with false below
                        Glib::ustring new_item_style = find_replace(orig_str, text, replace_text , false /*exact*/, casematch, true);
                        if (new_item_style != orig_str) {
                            vStyleToken = new_item_style;
                        }
                        g_free(orig_str);
                        g_free(replace_text);
                    }
                }
            }
        }
    }

    if (ret && _action_replace) {
        Glib::ustring new_item_style;
        for (const auto & vStyleToken : vStyleTokens) {
            new_item_style.append(vStyleToken).append(";");
        }
        new_item_style.erase(new_item_style.size()-1);
        item->setAttribute("style", new_item_style);
    }

    return ret;
}


std::vector<SPItem*> Find::filter_fields (std::vector<SPItem*> &l, bool exact, bool casematch)
{
    Glib::ustring tmp = entry_find.getEntry()->get_text();
    if (tmp.empty()) {
        return l;
    }
    gchar* text = g_strdup(tmp.c_str());

    std::vector<SPItem*> in = l;
    std::vector<SPItem*> out;

    if (check_searchin_text.get_active()) {
        for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
            SPObject *obj = *i;
            auto item = cast<SPItem>(obj);
            g_assert(item != nullptr);
            if (item_text_match(item, text, exact, casematch)) {
                if (out.end()==find(out.begin(),out.end(), *i)) {
                    out.push_back(*i);
                    if (_action_replace) {
                        item_text_match(item, text, exact, casematch, _action_replace);
                    }
                }
            }
        }
    }
    else if (check_searchin_property.get_active()) {

        bool ids = check_ids.get_active();
        bool style = check_style.get_active();
        bool font = check_font.get_active();
        bool desc = check_desc.get_active();
        bool title = check_title.get_active();
        bool attrname  = check_attributename.get_active();
        bool attrvalue = check_attributevalue.get_active();

        if (ids) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                if (item_id_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(), *i)) {
                        out.push_back(*i);
                        if (_action_replace) {
                            item_id_match(item, text, exact, casematch, _action_replace);
                        }
                    }
                }
            }
        }


        if (style) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                g_assert(item != nullptr);
                if (item_style_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(), *i)){
                            out.push_back(*i);
                            if (_action_replace) {
                                item_style_match(item, text, exact, casematch, _action_replace);
                            }
                        }
                }
            }
        }


        if (attrname) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                g_assert(item != nullptr);
                if (item_attr_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(), *i)) {
                        out.push_back(*i);
                        if (_action_replace) {
                            item_attr_match(item, text, exact, casematch, _action_replace);
                        }
                    }
                }
            }
        }


        if (attrvalue) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                g_assert(item != nullptr);
                if (item_attrvalue_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(), *i)) {
                        out.push_back(*i);
                        if (_action_replace) {
                            item_attrvalue_match(item, text, exact, casematch, _action_replace);
                        }
                    }
                }
            }
        }


        if (font) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                g_assert(item != nullptr);
                if (item_font_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(),*i)) {
                        out.push_back(*i);
                        if (_action_replace) {
                            item_font_match(item, text, exact, casematch, _action_replace);
                        }
                    }
                }
            }
        }
        if (desc) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                g_assert(item != nullptr);
                if (item_desc_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(),*i)) {
                        out.push_back(*i);
                        if (_action_replace) {
                            item_desc_match(item, text, exact, casematch, _action_replace);
                        }
                    }
                }
            }
        }
        if (title) {
            for (std::vector<SPItem*>::const_reverse_iterator i=in.rbegin(); in.rend() != i; ++i) {
                SPObject *obj = *i;
                auto item = cast<SPItem>(obj);
                g_assert(item != nullptr);
                if (item_title_match(item, text, exact, casematch)) {
                    if (out.end()==find(out.begin(),out.end(),*i)) {
                        out.push_back(*i);
                        if (_action_replace) {
                            item_title_match(item, text, exact, casematch, _action_replace);
                        }
                    }
                }
            }
        }

    }

    g_free(text);

    return out;
}


bool Find::item_type_match (SPItem *item)
{
    bool all  =check_alltypes.get_active();

    if (is<SPRect>(item)) {
        return ( all ||check_rects.get_active());

    } else if (is<SPGenericEllipse>(item)) {
        return ( all ||  check_ellipses.get_active());

    } else if (is<SPStar>(item) || is<SPPolygon>(item)) {
        return ( all || check_stars.get_active());

    } else if (is<SPSpiral>(item)) {
        return ( all || check_spirals.get_active());

    } else if (is<SPPath>(item) || is<SPLine>(item) || is<SPPolyLine>(item)) {
        return (all || check_paths.get_active());

    } else if (is<SPText>(item) || is<SPTSpan>(item) ||
               is<SPTRef>(item) ||
               is<SPFlowtext>(item) || is<SPFlowdiv>(item) ||
               is<SPFlowtspan>(item) || is<SPFlowpara>(item)) {
        return (all || check_texts.get_active());

    } else if (is<SPGroup>(item) &&
               !getDesktop()->layerManager().isLayer(item)) { // never select layers!
        return (all || check_groups.get_active());

    } else if (is<SPUse>(item)) {
        return (all || check_clones.get_active());

    } else if (is<SPImage>(item)) {
        return (all || check_images.get_active());

    } else if (is<SPOffset>(item)) {
        return (all || check_offsets.get_active());
    }

    return false;
}

std::vector<SPItem*> Find::filter_types (std::vector<SPItem*> &l)
{
    std::vector<SPItem*> n;
    for (std::vector<SPItem*>::const_reverse_iterator i=l.rbegin(); l.rend() != i; ++i) {
        SPObject *obj = *i;
        auto item = cast<SPItem>(obj);
        g_assert(item != nullptr);
        if (item_type_match(item)) {
        	n.push_back(*i);
        }
    }
    return n;
}


std::vector<SPItem*> &Find::filter_list (std::vector<SPItem*> &l, bool exact, bool casematch)
{
    l = filter_types (l);
    l = filter_fields (l, exact, casematch);
    return l;
}

std::vector<SPItem*> &Find::all_items (SPObject *r, std::vector<SPItem*> &l, bool hidden, bool locked)
{
    if (is<SPDefs>(r)) {
        return l; // we're not interested in items in defs
    }

    if (!strcmp(r->getRepr()->name(), "svg:metadata")) {
        return l; // we're not interested in metadata
    }

    auto desktop = getDesktop();
    for (auto& child: r->children) {
        auto item = cast<SPItem>(&child);
        if (item && !child.cloned && !desktop->layerManager().isLayer(item)) {
            if ((hidden || !desktop->itemIsHidden(item)) && (locked || !item->isLocked())) {
                l.insert(l.begin(),(SPItem*)&child);
            }
        }
        l = all_items (&child, l, hidden, locked);
    }
    return l;
}

std::vector<SPItem*> &Find::all_selection_items (Inkscape::Selection *s, std::vector<SPItem*> &l, SPObject *ancestor, bool hidden, bool locked)
{
    auto desktop = getDesktop();
    auto itemlist = s->items();
    for (auto i=boost::rbegin(itemlist); boost::rend(itemlist) != i; ++i) {
        SPObject *obj = *i;
        auto item = cast<SPItem>(obj);
        g_assert(item != nullptr);
        if (item && !item->cloned && !desktop->layerManager().isLayer(item)) {
            if (!ancestor || ancestor->isAncestorOf(item)) {
                if ((hidden || !desktop->itemIsHidden(item)) && (locked || !item->isLocked())) {
                    l.push_back(*i);
                }
            }
        }
        if (!ancestor || ancestor->isAncestorOf(item)) {
            l = all_items(item, l, hidden, locked);
        }
    }
    return l;
}



/*########################################################################
# BUTTON CLICK HANDLERS    (callbacks)
########################################################################*/

void Find::onFind()
{
    _action_replace = false;
    onAction();

    // Return focus to the find entry
    entry_find.getEntry()->grab_focus();
}

void Find::onReplace()
{
    if (entry_find.getEntry()->get_text().length() < 1) {
        status.set_text(_("Nothing to replace"));
        return;
    }
    _action_replace = true;
    onAction();

    // Return focus to the find entry
    entry_find.getEntry()->grab_focus();
}

void Find::onAction()
{
    auto desktop = getDesktop();
    bool hidden = check_include_hidden.get_active();
    bool locked = check_include_locked.get_active();
    bool exact = check_exact_match.get_active();
    bool casematch = check_case_sensitive.get_active();
    blocked = true;

    std::vector<SPItem*> l;
    if (check_scope_selection.get_active()) {
        if (check_scope_layer.get_active()) {
            l = all_selection_items (desktop->getSelection(), l, desktop->layerManager().currentLayer(), hidden, locked);
        } else {
            l = all_selection_items (desktop->getSelection(), l, nullptr, hidden, locked);
        }
    } else {
        if (check_scope_layer.get_active()) {
            l = all_items (desktop->layerManager().currentLayer(), l, hidden, locked);
        } else {
            l = all_items(desktop->getDocument()->getRoot(), l, hidden, locked);
        }
    }
    guint all = l.size();

    std::vector<SPItem*> n = filter_list (l, exact, casematch);

    if (!n.empty()) {
        int count = n.size();
        desktop->messageStack()->flashF(Inkscape::NORMAL_MESSAGE,
                                        // TRANSLATORS: "%s" is replaced with "exact" or "partial" when this string is displayed
                                        ngettext("<b>%d</b> object found (out of <b>%d</b>), %s match.",
                                                 "<b>%d</b> objects found (out of <b>%d</b>), %s match.",
                                                 count),
                                        count, all, exact? _("exact") : _("partial"));
        if (_action_replace){
            // TRANSLATORS: "%1" is replaced with the number of matches
            status.set_text(Glib::ustring::compose(ngettext("%1 match replaced","%1 matches replaced",count), count));
        }
        else {
            // TRANSLATORS: "%1" is replaced with the number of matches
            status.set_text(Glib::ustring::compose(ngettext("%1 object found","%1 objects found",count), count));
            bool attributenameyok = !check_attributename.get_active();
            button_replace.set_sensitive(attributenameyok);
        }

        Inkscape::Selection *selection = desktop->getSelection();
        selection->clear();
        selection->setList(n);
        SPObject *obj = n[0];
        auto item = cast<SPItem>(obj);
        g_assert(item != nullptr);
        scroll_to_show_item(desktop, item);

        if (_action_replace) {
            DocumentUndo::done(desktop->getDocument(), _("Replace text or property"), INKSCAPE_ICON("draw-text"));
        }

    } else {
        status.set_text(_("Nothing found"));
        if (!check_scope_selection.get_active()) {
            Inkscape::Selection *selection = desktop->getSelection();
            selection->clear();
        }
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("No objects found"));
    }
    blocked = false;

}

void Find::onToggleCheck ()
{
    bool objectok = false;
    status.set_text("");

    if (check_alltypes.get_active()) {
        objectok = true;
    }
    for (auto & checkType : checkTypes) {
        if (checkType->get_active()) {
            objectok = true;
        }
    }

    if (!objectok) {
        status.set_text(_("Select an object type"));
    }


    bool propertyok = false;

    if (!check_searchin_property.get_active()) {
        propertyok = true;
    } else {

        for (auto & checkProperty : checkProperties) {
            if (checkProperty->get_active()) {
                propertyok = true;
            }
        }
    }

    if (!propertyok) {
        status.set_text(_("Select a property"));
    }

    // Can't replace attribute names
    // bool attributenameyok = !check_attributename.get_active();

    button_find.set_sensitive(objectok && propertyok);
    // button_replace.set_sensitive(objectok && propertyok && attributenameyok);
    button_replace.set_sensitive(false);
}

void Find::onToggleAlltypes ()
{
     bool all  =check_alltypes.get_active();
     for (auto & checkType : checkTypes) {
         checkType->set_sensitive(!all);
     }

     onToggleCheck();
}

void Find::onSearchinText ()
{
    searchinToggle(false);
    onToggleCheck();
}

void Find::onSearchinProperty ()
{
    searchinToggle(true);
    onToggleCheck();
}

void Find::searchinToggle(bool on)
{
    for (auto & checkProperty : checkProperties) {
        checkProperty->set_sensitive(on);
    }
}

void Find::onExpander ()
{
    if (!expander_options.get_expanded())
        squeeze_window();
}

/*########################################################################
# UTILITY
########################################################################*/

void Find::squeeze_window()
{
    // TODO: resize dialog window when the expander is closed
    // set_size_request(-1, -1);
}

} // namespace Inkscape::UI::Dialog

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
