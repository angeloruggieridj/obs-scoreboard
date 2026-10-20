// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// One look for the dock and every window of the plugin.
//
// OBS hands its theme to the widgets as a QPalette (Yami: window #1D1F26, button #3C404D,
// highlight #284CB8, ...), so the style is derived from the palette of the widget it is applied to:
// it follows the dark and light themes by itself. Only the colors that carry a meaning stay fixed
// (green start, red stop, yellow score, orange fouls). Widgets opt in through dynamic properties
// (see the `Role` / `Card` / `Text` names below), so nothing else in OBS is restyled.

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QString>

class QDialog;
class QWidget;

namespace dockstyle {

// Button / card / text roles: the value of the dynamic property of the same name.
constexpr const char* kRole = "sbRole"; // btn ghost go stop seg segval icon small chip chipOn
constexpr const char* kPos = "sbPos";   // first mid last (segmented controls)
constexpr const char* kCard = "sbCard"; // clock team form
constexpr const char* kTeam = "sbTeam"; // home away
constexpr const char* kText = "sbText"; // caption name muted strong pill pillLabel status section
constexpr const char* kSize = "sbSize"; // large
constexpr const char* kLink = "sbLink"; // ok warn none (the link indicator pill)

enum class Density { Compact, Comfortable };

Density density();
void setDensity(Density density);
// Heights that follow the density.
int controlHeight();
int smallHeight();
// Gives a button or value cell the exact height of its role (and size) at the current density.
// Style-sheet heights are not reliable across styles (a button's min-height counts the border and
// padding differently from a label's), so the heights of the controls that must line up are set
// here. Called by setProp() and again for every child when the density changes.
void fitHeight(QWidget* widget);

// The style sheet for widgets whose palette is `palette`.
QString styleSheet(const QPalette& palette, Density density);

// Applies the style to `root` and keeps it in step with the theme (a palette change) and the
// density. Safe to call again; the sheet is only reset when it actually changed.
void install(QWidget* root, bool dialog = false);

// Colors with a meaning; `dark` is whether the theme is a dark one.
QColor goColor(const QPalette& palette);
QColor stopColor(const QPalette& palette);
// The theme's highlight, darkened where needed so white text stays readable on it.
QColor accentColor(const QPalette& palette);
// The default accent of the away team's card.
QColor accentAwayColor();
bool isDark(const QPalette& palette);

// Gives a window of the plugin the same look as the dock: the shared sheet (with the tab bar), a
// "primary" accept button and ghost cancel/close buttons, and the plain role on every other button.
// Call it once the window's widgets exist.
void styleDialog(QDialog* dialog);

// Sets a dynamic property and re-polishes the widget, only when the value changes.
void setProp(QWidget* widget, const char* name, const QString& value);

// Icons drawn with QPainter (crisp at any scale, independent of the UI font's glyphs).
enum class Icon { Menu, Gear, Plus, Cross, Pencil, Play, Pause, Reset, Prev, Next };

// A button whose icon is one of the above, tinted with the button's text color and redrawn when the
// theme or the enabled state changes.
class IconButton : public QPushButton {
    Q_OBJECT
public:
    explicit IconButton(Icon icon, QWidget* parent = nullptr);
    void setIconKind(Icon icon);
    // A fixed tint (white on the green/red button); an invalid color goes back to the text color.
    void setTint(const QColor& color);

protected:
    void changeEvent(QEvent* event) override;

private:
    void rebuild();

    Icon icon_;
    QColor tint_;
    QColor builtFor_;
    bool builtEnabled_ = true;
};

} // namespace dockstyle
