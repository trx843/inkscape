// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Matthew Jakeman <mjakeman26@outlook.co.nz>
 *   PBS <pbs3141@gmail.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_UI_WIDGET_EVENTS_CANVAS_EVENT_H
#define INKSCAPE_UI_WIDGET_EVENTS_CANVAS_EVENT_H

#include <cstdint>
#include <memory>
#include <optional>

#include <gdk/gdk.h>
#include <2geom/point.h>

#include "enums.h"
#include "util/gobjectptr.h"

namespace Inkscape {

// Smart pointer for wrapping GdkEvents.
struct GdkEventFreer { void operator()(GdkEvent *ev) const { gdk_event_free(ev); } };
using GdkEventUniqPtr = std::unique_ptr<GdkEvent, GdkEventFreer>;

/**
 * Extended input data associated to events generated by graphics tablets.
 * Present for Motion, ButtonPress and Scroll events.
 */
struct ExtendedInput
{
    std::optional<double> pressure, xtilt, ytilt;
};

/**
 * Read the extended input data from a GdkEvent.
 */
inline ExtendedInput extinput_from_gdkevent(GdkEvent *event)
{
    auto read = [event] (GdkAxisUse axis) -> std::optional<double> {
        double tmp;
        if (gdk_event_get_axis(event, axis, &tmp)) {
            return tmp;
        } else {
            return {};
        }
    };

    return {
        .pressure = read(GDK_AXIS_PRESSURE),
        .xtilt = read(GDK_AXIS_XTILT),
        .ytilt = read(GDK_AXIS_YTILT)
    };
}

/**
 * Abstract base class for events
 */
struct CanvasEvent
{
    virtual ~CanvasEvent() {}

    /// Return the dynamic type of the CanvasEvent.
    virtual EventType type() const = 0;

    /// The modifiers mask immediately before the event.
    unsigned modifiers = 0;

    /// Get the change in the modifiers due to this event.
    virtual unsigned modifiersChange() const { return 0; }

    /// Get the modifiers mask immediately after the event. (Convenience function.)
    unsigned modifiersAfter() const { return modifiers ^ modifiersChange(); }

    /// The device that sourced the event. May be null.
    // Note: This is only used by the tool switcher in desktop-events.cpp.
    // It could be removed if the tool switcher were invoked by the canvas
    // instead of sp_desktop_root_handler.
    Util::GObjectPtr<GdkDevice> source_device;
};

/**
 * Abstract event for mouse button (left/right/middle).
 * May also be used for touch interactions.
 */
struct ButtonEvent : CanvasEvent
{
    unsigned modifiersChange() const override
    {
        switch (button) {
            case 1: return GDK_BUTTON1_MASK;
            case 2: return GDK_BUTTON2_MASK;
            case 3: return GDK_BUTTON3_MASK;
            case 4: return GDK_BUTTON4_MASK;
            case 5: return GDK_BUTTON5_MASK;
            default: return 0; // Buttons can range at least to 9 but mask defined only to 5.
        }
    }

    /// Location of the cursor, in world coordinates.
    Geom::Point pos;
    /// Location of the cursor, in GDK event / canvas widget coordinates.
    Geom::Point orig_pos;

    /// The button that was pressed/released. (Matches GDK_BUTTON_*.)
    unsigned button = 0;

    /// Timestamp of the event in milliseconds.
    uint32_t time = 0;
};

/**
 * A mouse button (left/right/middle) is pressed.
 * May also be used for touch interactions.
 */
struct ButtonPressEvent final : ButtonEvent
{
    EventType type() const override { return EventType::BUTTON_PRESS; }

    /// Counter for repeated clicks (e.g. double clicks). Starts at 1 and increments by 1 each time.
    int num_press = 1;

    /// Extended input data for graphics tablet input. Fields may be empty.
    ExtendedInput extinput;
};

/**
 * A mouse button (left/right/middle) is released.
 * May also be used for touch interactions.
 */
struct ButtonReleaseEvent final : ButtonEvent
{
    EventType type() const override { return EventType::BUTTON_RELEASE; }
};

/**
 * A key has been pressed.
 */
struct KeyEvent : CanvasEvent
{
    unsigned modifiersChange() const override
    {
        switch (keyval) {
            case GDK_KEY_Shift_L:
            case GDK_KEY_Shift_R:
                return GDK_SHIFT_MASK;
            case GDK_KEY_Control_L:
            case GDK_KEY_Control_R:
                return GDK_CONTROL_MASK;
            case GDK_KEY_Alt_L:
            case GDK_KEY_Alt_R:
                return GDK_ALT_MASK;
            case GDK_KEY_Meta_L:
            case GDK_KEY_Meta_R:
                return GDK_META_MASK;
            default:
                return 0;
        }
    }

    /// The key that was pressed/released. (Matches gdkkeysyms.h.)
    uint32_t keyval = 0;

    /// The raw code of the key that was pressed/released.
    uint16_t hardware_keycode = 0;

    /// The keyboard group.
    uint8_t group = 0;

    /// Timestamp of the event in milliseconds.
    uint32_t time = 0;

    // Todo: (GTK4) Remove me! Currently only used by text-tool.cpp (input method).
    GdkEventUniqPtr original;

    /// Location of the cursor, in world coordinates.
    std::optional<Geom::Point> pos;
    /// Location of the cursor, in GDK event / canvas widget coordinates.
    std::optional<Geom::Point> orig_pos;
};

/**
 * A key has been pressed.
 */
struct KeyPressEvent final : KeyEvent
{
    EventType type() const override { return EventType::KEY_PRESS; }
};

/**
 * A key has been released.
 */
struct KeyReleaseEvent final : KeyEvent
{
    EventType type() const override { return EventType::KEY_RELEASE; }
};

/**
 * Movement of the mouse pointer. May also be used
 * for touch interactions.
 */
struct MotionEvent final : CanvasEvent
{
    EventType type() const override { return EventType::MOTION; }

    /// Location of the cursor.
    Geom::Point pos;

    /// Timestamp of the event in milliseconds.
    uint32_t time = 0;

    /// Whether this is a fake motion event synthesized by a control point.
    bool control_point_synthesized = false;

    /// Extended input data for graphics tablet input. Fields may be empty.
    ExtendedInput extinput;
};

/**
 * The pointer has entered a widget or item.
 */
struct EnterEvent final : CanvasEvent
{
    EventType type() const override { return EventType::ENTER; }

    /// Location of the cursor.
    Geom::Point pos;
};

/**
 * The pointer has exited a widget or item.
 *
 * Note the coordinates will always be (0, 0) for this event.
 */
struct LeaveEvent final : CanvasEvent
{
    EventType type() const override { return EventType::LEAVE; }
};

/**
 * Scroll the item or widget by the provided amount
 */
struct ScrollEvent final : CanvasEvent
{
    EventType type() const override { return EventType::SCROLL; }

    /// The amount scrolled.
    Geom::Point delta;

    /// The direction to scroll.
    GdkScrollDirection direction = GDK_SCROLL_SMOOTH;

    /// Extended input data for graphics tablet input. Fields may be empty.
    ExtendedInput extinput;
};

namespace canvas_event_detail {

template <typename E>
E &cast_helper(CanvasEvent &event)
{
    return static_cast<E &>(event);
}

template <typename E>
E const &cast_helper(CanvasEvent const &event)
{
    return static_cast<E const &>(event);
}

template <typename E>
E &&cast_helper(CanvasEvent &&event)
{
    return static_cast<E &&>(event);
}

template <typename... Fs>
struct Overloaded : Fs...
{
    using Fs::operator()...;
};

// Todo: Delete in C++20.
// …BUT only once all CI runners, etc. support C++20 changes to deduction guides
template <typename... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

} // namespace canvas_event_detail

/**
 * @brief Perform pattern-matching on a CanvasEvent.
 *
 * This function takes an event and a list of function objects,
 * and passes the event to the function object whose argument
 * type best matches the dynamic type of the event.
 *
 * @arg event The CanvasEvent to inspect.
 * @arg funcs A list of function objects, each taking a single argument.
 */
template <typename E, typename... Fs>
void inspect_event(E &&event, Fs... funcs)
{
    using namespace canvas_event_detail;

    auto overloaded = Overloaded{funcs...};

    switch (event.type()) {
        case EventType::ENTER:
            overloaded(cast_helper<EnterEvent>(std::forward<E>(event)));
            break;
        case EventType::LEAVE:
            overloaded(cast_helper<LeaveEvent>(std::forward<E>(event)));
            break;
        case EventType::MOTION:
            overloaded(cast_helper<MotionEvent>(std::forward<E>(event)));
            break;
        case EventType::BUTTON_PRESS:
            overloaded(cast_helper<ButtonPressEvent>(std::forward<E>(event)));
            break;
        case EventType::BUTTON_RELEASE:
            overloaded(cast_helper<ButtonReleaseEvent>(std::forward<E>(event)));
            break;
        case EventType::KEY_PRESS:
            overloaded(cast_helper<KeyPressEvent>(std::forward<E>(event)));
            break;
        case EventType::KEY_RELEASE:
            overloaded(cast_helper<KeyReleaseEvent>(std::forward<E>(event)));
            break;
        case EventType::SCROLL:
            overloaded(cast_helper<ScrollEvent>(std::forward<E>(event)));
            break;
        default:
            break;
    }
}

/*
 * Modifier-testing functions. ("mod" variant.)
 */

/// "Primary" modifier: Ctrl on Linux/Windows and Cmd on macOS.
#ifdef GDK_WINDOWING_QUARTZ
inline constexpr auto INK_GDK_PRIMARY_MASK = GDK_MOD2_MASK;
#else
inline constexpr auto INK_GDK_PRIMARY_MASK = GDK_CONTROL_MASK;
#endif
// Todo: (GTK4) Replace INK_GDK_PRIMARY_MASK with GDK_CONTROL_MASK.
// Reference: https://docs.gtk.org/gtk4/migrating-3to4.html#adapt-to-changes-in-keyboard-modifier-handling

/// All modifiers used by Inkscape.
inline constexpr auto INK_GDK_MODIFIER_MASK_MOD = GDK_SHIFT_MASK | INK_GDK_PRIMARY_MASK | GDK_ALT_MASK;

inline bool mod_shift(unsigned modifiers) { return modifiers & GDK_SHIFT_MASK; }
inline bool mod_ctrl(unsigned modifiers) { return modifiers & INK_GDK_PRIMARY_MASK; }
inline bool mod_alt(unsigned modifiers) { return modifiers & GDK_ALT_MASK; }
inline bool mod_shift_only(unsigned modifiers) { return (modifiers & INK_GDK_MODIFIER_MASK_MOD) == GDK_SHIFT_MASK; }
inline bool mod_ctrl_only(unsigned modifiers) { return (modifiers & INK_GDK_MODIFIER_MASK_MOD) == INK_GDK_PRIMARY_MASK; }
inline bool mod_alt_only(unsigned modifiers) { return (modifiers & INK_GDK_MODIFIER_MASK_MOD) == GDK_ALT_MASK; }

inline bool mod_shift(KeyEvent const &event) { return mod_shift(event.modifiers); }
inline bool mod_ctrl(KeyEvent const &event) { return mod_ctrl(event.modifiers); }
inline bool mod_alt(KeyEvent const &event) { return mod_alt(event.modifiers); }
inline bool mod_shift_only(KeyEvent const &event) { return mod_shift_only(event.modifiers); }
inline bool mod_ctrl_only(KeyEvent const &event) { return mod_ctrl_only(event.modifiers); }
inline bool mod_alt_only(KeyEvent const &event) { return mod_alt_only(event.modifiers); }

/*
 * Modifier-testing functions. ("held" variant.)
 * Todo: (GTK4) Merge these into the above, since they will be the same.
 */

/// All modifiers used by Inkscape.
inline constexpr auto INK_GDK_MODIFIER_MASK_HELD = GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK;

inline bool state_held_shift(unsigned state) { return state & GDK_SHIFT_MASK; }
inline bool state_held_ctrl(unsigned state) { return state & GDK_CONTROL_MASK; }
inline bool state_held_alt(unsigned state) { return state & GDK_ALT_MASK; }
inline bool state_held_only_shift(unsigned state) { return (state & INK_GDK_MODIFIER_MASK_HELD) == GDK_SHIFT_MASK; }
inline bool state_held_only_ctrl(unsigned state) { return (state & INK_GDK_MODIFIER_MASK_HELD) == GDK_CONTROL_MASK; }
inline bool state_held_only_alt(unsigned state) { return (state & INK_GDK_MODIFIER_MASK_HELD) == GDK_ALT_MASK; }
inline bool state_held_any_modifiers(unsigned state) { return state & INK_GDK_MODIFIER_MASK_HELD; }
inline bool state_held_no_modifiers(unsigned state) { return !state_held_any_modifiers(state); }

template <unsigned button>
inline bool state_held_button(unsigned state) { return (1 <= button || button <= 5) && (state & (GDK_BUTTON1_MASK << (button - 1))); }

inline bool held_shift(CanvasEvent const &event) { return state_held_shift(event.modifiers); }
inline bool held_ctrl(CanvasEvent const &event) { return state_held_ctrl(event.modifiers); }
inline bool held_alt(CanvasEvent const &event) { return state_held_alt(event.modifiers); }
inline bool held_only_shift(CanvasEvent const &event) { return state_held_only_shift(event.modifiers); }
inline bool held_only_ctrl(CanvasEvent const &event) { return state_held_only_ctrl(event.modifiers); }
inline bool held_only_alt(CanvasEvent const &event) { return state_held_only_alt(event.modifiers); }
inline bool held_any_modifiers(CanvasEvent const &event) { return state_held_any_modifiers(event.modifiers); }
inline bool held_no_modifiers(CanvasEvent const &event) { return state_held_no_modifiers(event.modifiers); }

template <unsigned button>
inline bool held_button(CanvasEvent const &event) { return state_held_button<button>(event.modifiers); }

/*
 * Shortcut key handling
 */
inline unsigned shortcut_key(KeyEvent const &event)
{
    unsigned shortcut_key = 0;
    gdk_keymap_translate_keyboard_state(
        gdk_keymap_get_for_display(gdk_display_get_default()),
        event.hardware_keycode,
        static_cast<GdkModifierType>(event.modifiers),
        0, // group
        &shortcut_key, nullptr, nullptr, nullptr);
    return shortcut_key;
}

} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_EVENTS_CANVAS_EVENT_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
