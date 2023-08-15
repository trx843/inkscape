// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Marker edit mode toolbar - onCanvas marker editing of marker orientation, position, scale
 *//*
 * Authors:
 * see git history
 * Rachana Podaralla <rpodaralla3@gatech.edu>
 * Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "marker-toolbar.h"

namespace Inkscape {
namespace UI {
namespace Toolbar {

MarkerToolbar::MarkerToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
{
}

GtkWidget* MarkerToolbar::create(SPDesktop *desktop)
{
    auto toolbar = Gtk::manage(new MarkerToolbar(desktop));
    return toolbar->Gtk::Widget::gobj();
}

}}}
