// SPDX-License-Identifier: GPL-2.0-or-later
#include "DockStyle.hpp"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyle>
#include <QWidget>

#include <algorithm>
#include <vector>

namespace dockstyle {

namespace {

Density g_density = Density::Compact;

class Watcher;
std::vector<Watcher*>& watchers() {
    static std::vector<Watcher*> list;
    return list;
}

QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

// The theme's highlight is pale in the light themes; white text on it would not be readable.
QColor withContrast(QColor color) {
    for (int i = 0; i < 4 && color.lightnessF() > 0.42; ++i) color = color.darker(125);
    return color;
}

QString hex(const QColor& c) {
    return c.name(QColor::HexRgb);
}

struct Tokens {
    QColor win, base, inset, card, btn, btnHi, btnDn, bdr, bdrHi;
    QColor txt, txt2, muted, prim, go, goHi, stop, stopHi, accentAway;
};

Tokens tokensFor(const QPalette& pal) {
    Tokens t;
    const bool dark = isDark(pal);
    t.win = pal.color(QPalette::Window);
    t.txt = pal.color(QPalette::WindowText);
    t.btn = pal.color(QPalette::Button);
    t.base = pal.color(QPalette::Base);
    t.prim = withContrast(pal.color(QPalette::Highlight));
    t.inset = dark ? t.win.darker(140) : t.base;
    t.card = dark ? t.base : t.base.darker(103);
    t.btnHi = dark ? t.btn.lighter(118) : t.btn.darker(108);
    t.btnDn = dark ? t.btn.darker(150) : t.btn.darker(120);
    t.bdr = mix(t.win, t.txt, dark ? 0.16 : 0.20);
    t.bdrHi = mix(t.win, t.txt, 0.35);
    t.txt2 = mix(t.txt, t.win, 0.25);
    t.muted = mix(t.txt, t.win, 0.45);
    t.go = goColor(pal);
    t.goHi = t.go.lighter(122);
    t.stop = stopColor(pal);
    t.stopHi = t.stop.lighter(122);
    t.accentAway = QColor(QStringLiteral("#EF7D57"));
    return t;
}

} // namespace

Density density() {
    return g_density;
}

int controlHeight() {
    return g_density == Density::Compact ? 26 : 30;
}

int smallHeight() {
    return g_density == Density::Compact ? 20 : 22;
}

bool isDark(const QPalette& pal) {
    return pal.color(QPalette::Window).lightness() < 128;
}

QColor goColor(const QPalette& pal) {
    return isDark(pal) ? QColor(QStringLiteral("#25A231")) : QColor(QStringLiteral("#2E9D3A"));
}

QColor accentAwayColor() {
    return QColor(QStringLiteral("#EF7D57"));
}

QColor accentColor(const QPalette& pal) {
    return withContrast(pal.color(QPalette::Highlight));
}

QColor stopColor(const QPalette& pal) {
    return isDark(pal) ? QColor(QStringLiteral("#C01C37")) : QColor(QStringLiteral("#CF2B45"));
}

QString styleSheet(const QPalette& pal, Density density) {
    const Tokens t = tokensFor(pal);
    const int h = density == Density::Compact ? 26 : 30;
    const int s = density == Density::Compact ? 20 : 22;
    QString css;
    const auto add = [&css](const QString& rule) { css += rule + QLatin1Char('\n'); };

    // --- Buttons --------------------------------------------------------------------------
    add(QStringLiteral("QPushButton[sbRole]{border:1px solid %1;border-radius:4px;background:%1;"
                       "color:%2;padding:0 10px;min-height:%3px;font-weight:600;}")
            .arg(hex(t.btn), hex(t.txt))
            .arg(h));
    add(QStringLiteral("QPushButton[sbRole]:hover{background:%1;border-color:%2;}")
            .arg(hex(t.btnHi), hex(t.bdrHi)));
    add(QStringLiteral("QPushButton[sbRole]:pressed{background:%1;}").arg(hex(t.btnDn)));
    add(QStringLiteral("QPushButton[sbRole]:disabled{color:%1;background:%2;border-color:%3;}")
            .arg(hex(t.muted), hex(t.win), hex(t.bdr)));
    add(QStringLiteral("QPushButton[sbRole=\"ghost\"]{background:transparent;border-color:%1;"
                       "font-weight:500;}QPushButton[sbRole=\"ghost\"]:hover{background:%2;}")
            .arg(hex(t.bdr), hex(t.btn)));
    add(QStringLiteral(
            "QPushButton[sbRole=\"go\"]{background:%1;border-color:%1;color:#FFFFFF;"
            "font-size:13px;}QPushButton[sbRole=\"go\"]:hover{background:%2;border-color:%2;}")
            .arg(hex(t.go), hex(t.goHi)));
    add(QStringLiteral(
            "QPushButton[sbRole=\"stop\"]{background:%1;border-color:%1;color:#FFFFFF;"
            "font-size:13px;}QPushButton[sbRole=\"stop\"]:hover{background:%2;border-color:%2;}")
            .arg(hex(t.stop), hex(t.stopHi)));
    add(QStringLiteral("QPushButton[sbRole=\"icon\"]{padding:0;min-width:%1px;max-width:%1px;}")
            .arg(h));
    add(QStringLiteral(
            "QPushButton[sbRole=\"iconghost\"]{background:transparent;border:1px solid %1;"
            "padding:0;min-width:%2px;max-width:%2px;}"
            "QPushButton[sbRole=\"iconghost\"]:hover{background:%3;}"
            "QPushButton[sbRole=\"iconghost\"]::menu-indicator{image:none;width:0;}")
            .arg(hex(t.bdr))
            .arg(h)
            .arg(hex(t.btn)));
    add(QStringLiteral("QPushButton[sbRole=\"go\"],QPushButton[sbRole=\"stop\"]{min-height:%1px;}")
            .arg(h + 6));
    add(QStringLiteral("QPushButton[sbRole=\"small\"]{padding:0;min-height:%1px;max-height:%1px;"
                       "min-width:%1px;max-width:%1px;font-size:11px;}")
            .arg(s));
    // Segmented controls: [ -1 | value | +1 ] is one rounded group with thin separators.
    add(QStringLiteral(
            "QPushButton[sbRole=\"seg\"]{border-radius:0;border:1px solid %1;"
            "border-left-width:0;padding:0;min-width:26px;min-height:%2px;max-height:%2px;}")
            .arg(hex(t.bdr))
            .arg(h));
    add(QStringLiteral("QPushButton[sbRole=\"seg\"][sbPos=\"first\"]{border-left-width:1px;"
                       "border-top-left-radius:4px;border-bottom-left-radius:4px;}"));
    add(QStringLiteral("QPushButton[sbRole=\"seg\"][sbPos=\"last\"]{border-top-right-radius:4px;"
                       "border-bottom-right-radius:4px;}"));
    add(QStringLiteral(
            "QLabel[sbRole=\"segval\"]{background:%1;color:%2;border:1px solid %3;"
            "border-left-width:0;font-weight:700;padding:0 6px;min-height:%4px;max-height:%4px;}")
            .arg(hex(t.inset), hex(t.txt), hex(t.bdr))
            .arg(h));
    add(QStringLiteral(
        "QPushButton[sbRole=\"seg\"][sbPos=\"only\"]{border-left-width:1px;border-radius:4px;}"));
    // The score counter is the most used control of a match: a little larger than the rest.
    add(QStringLiteral(
            "QPushButton[sbRole=\"seg\"][sbSize=\"large\"]{min-width:38px;font-size:13px;"
            "min-height:%1px;max-height:%1px;}")
            .arg(h + 6));
    add(QStringLiteral("QLabel[sbRole=\"segval\"][sbSize=\"large\"]{font-size:20px;min-height:%1px;"
                       "max-height:%1px;}")
            .arg(h + 6));
    // Penalty type chips (add window).
    add(QStringLiteral(
        "QPushButton[sbRole=\"chip\"],QPushButton[sbRole=\"chipOn\"]{min-width:44px;}"));
    add(QStringLiteral(
            "QPushButton[sbRole=\"chipOn\"]{background:%1;border-color:%1;color:#FFFFFF;}")
            .arg(hex(t.prim)));
    // The dialog's main action.
    add(QStringLiteral(
            "QPushButton[sbRole=\"primary\"]{background:%1;border-color:%1;color:#FFFFFF;}"
            "QPushButton[sbRole=\"primary\"]:hover{background:%2;border-color:%2;}")
            .arg(hex(t.prim), hex(t.prim.lighter(118))));

    // --- Cards ----------------------------------------------------------------------------
    add(QStringLiteral(
            "QFrame[sbCard=\"clock\"]{background:%1;border:1px solid %2;border-radius:6px;}")
            .arg(hex(t.inset), hex(t.bdr)));
    add(QStringLiteral(
            "QFrame[sbCard=\"team\"]{background:%1;border:1px solid %2;border-top:3px solid %3;"
            "border-radius:6px;}")
            .arg(hex(t.card), hex(t.bdr), hex(t.prim)));
    add(QStringLiteral("QFrame[sbCard=\"team\"][sbTeam=\"away\"]{border-top-color:%1;}")
            .arg(hex(t.accentAway)));
    add(QStringLiteral(
            "QFrame[sbCard=\"pen\"]{background:%1;border:1px solid %2;border-radius:4px;}")
            .arg(hex(t.inset), hex(t.bdr)));

    // --- Text -----------------------------------------------------------------------------
    add(QStringLiteral("QLabel[sbText=\"caption\"]{color:%1;font-size:10px;font-weight:600;}")
            .arg(hex(t.muted)));
    add(QStringLiteral("QLabel[sbText=\"muted\"]{color:%1;}").arg(hex(t.muted)));
    add(QStringLiteral("QLabel[sbText=\"strong\"]{color:%1;font-weight:700;}").arg(hex(t.txt)));
    add(QStringLiteral(
            "QLineEdit[sbText=\"name\"]{background:%1;border:1px solid %2;border-radius:4px;"
            "min-height:%3px;font-weight:700;font-size:13px;padding:0 6px;color:%4;"
            "selection-background-color:%5;}QLineEdit[sbText=\"name\"]:focus{border-color:%5;}")
            .arg(hex(t.inset), hex(t.bdr))
            .arg(h - 2)
            .arg(hex(t.txt), hex(t.prim)));
    add(QStringLiteral(
            "QPushButton[sbText=\"pill\"]{background:%1;border:1px solid %2;border-radius:10px;"
            "min-height:20px;max-height:20px;padding:0 "
            "9px;color:%3;font-weight:500;font-size:11px;}"
            "QPushButton[sbText=\"pill\"]:hover{border-color:%4;}")
            .arg(hex(t.base), hex(t.bdr), hex(t.txt2), hex(t.bdrHi)));
    // The link indicator: green when everything is linked, yellow when a link was lost.
    add(QStringLiteral("QPushButton[sbText=\"pill\"][sbLink=\"ok\"]{color:%1;}"
                       "QPushButton[sbText=\"pill\"][sbLink=\"warn\"]{color:#F2C94C;}"
                       "QPushButton[sbText=\"pill\"][sbLink=\"none\"]{color:%2;}")
            .arg(hex(t.goHi), hex(t.muted)));
    add(QStringLiteral(
            "QLabel[sbText=\"pillLabel\"]{background:%1;border:1px solid %2;border-radius:10px;"
            "padding:1px 10px;color:%3;font-size:11px;}")
            .arg(hex(t.base), hex(t.bdr), hex(t.txt2)));
    add(QStringLiteral("QLabel[sbText=\"status\"]{color:%1;font-size:11px;}").arg(hex(t.muted)));
    return css;
}

static QString dialogSheet(const QPalette& pal) {
    const Tokens t = tokensFor(pal);
    QString css;
    const auto add = [&css](const QString& rule) { css += rule + QLatin1Char('\n'); };
    // Tabs: a plain bar with an underline under the current one, like OBS's own settings.
    add(QStringLiteral("QTabWidget::pane{border:0;border-top:1px solid %1;top:-1px;}")
            .arg(hex(t.bdr)));
    add(QStringLiteral("QTabBar::tab{background:transparent;color:%1;padding:7px 14px;border:0;"
                       "border-bottom:2px solid transparent;}")
            .arg(hex(t.txt2)));
    add(QStringLiteral(
            "QTabBar::tab:selected{color:%1;font-weight:600;border-bottom:2px solid %2;}")
            .arg(hex(t.txt), hex(t.prim)));
    add(QStringLiteral("QTabBar::tab:hover:!selected{color:%1;}").arg(hex(t.txt)));
    add(QStringLiteral("QLabel[sbText=\"section\"]{color:%1;font-size:10px;font-weight:600;"
                       "padding-top:6px;}")
            .arg(hex(t.muted)));
    return css;
}

namespace {

// Keeps one root widget styled: reapplies the sheet on a palette change and on a density change.
class Watcher : public QObject {
public:
    Watcher(QWidget* root, bool dialog) : QObject(root), root_(root), dialog_(dialog) {
        root_->installEventFilter(this);
        watchers().push_back(this);
    }
    ~Watcher() override {
        auto& list = watchers();
        list.erase(std::remove(list.begin(), list.end(), this), list.end());
    }
    QWidget* root() const { return root_; }
    void apply() {
        if (applying_) return; // setStyleSheet() re-polishes and may send a PaletteChange back
        // The application palette is what OBS's theme sets; the widget's own palette is changed by
        // the style sheet machinery itself and must not feed back into the sheet.
        const QPalette& palette = QApplication::palette();
        QString sheet = styleSheet(palette, density());
        if (dialog_) sheet += dialogSheet(palette);
        if (root_->styleSheet() == sheet) return;
        applying_ = true;
        root_->setStyleSheet(sheet);
        applying_ = false;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == root_ && event->type() == QEvent::PaletteChange) apply();
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget* root_;
    bool dialog_;
    bool applying_ = false;
};

} // namespace

void setDensity(Density value) {
    if (g_density == value) return;
    g_density = value;
    for (Watcher* watcher : watchers()) watcher->apply();
}

void install(QWidget* root, bool dialog) {
    if (!root) return;
    // (Not findChild<Watcher*>: Watcher has no Q_OBJECT, so qobject_cast would accept any child.)
    for (Watcher* existing : watchers()) {
        if (existing->root() == root) {
            existing->apply();
            return;
        }
    }
    auto* watcher = new Watcher(root, dialog);
    watcher->apply();
}

void styleDialog(QDialog* dialog) {
    if (!dialog) return;
    for (QDialogButtonBox* box : dialog->findChildren<QDialogButtonBox*>()) {
        for (QAbstractButton* button : box->buttons()) {
            const bool accept = box->buttonRole(button) == QDialogButtonBox::AcceptRole ||
                                box->buttonRole(button) == QDialogButtonBox::YesRole;
            setProp(button, kRole, accept ? QStringLiteral("primary") : QStringLiteral("ghost"));
            button->setFocusPolicy(Qt::StrongFocus);
        }
    }
    for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
        if (button->property(kRole).toString().isEmpty())
            setProp(button, kRole, QStringLiteral("btn"));
    }
    install(dialog, true);
}

void setProp(QWidget* widget, const char* name, const QString& value) {
    if (!widget || widget->property(name).toString() == value) return;
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

// ----------------------------------------------------------------------------- icons ------

namespace {

QPixmap drawIcon(Icon icon, int size, const QColor& color, qreal dpr) {
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / 16.0, size / 16.0); // every icon is drawn on a 16 x 16 grid
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (icon) {
    case Icon::Menu:
        p.drawLine(QPointF(2.5, 4), QPointF(13.5, 4));
        p.drawLine(QPointF(2.5, 8), QPointF(13.5, 8));
        p.drawLine(QPointF(2.5, 12), QPointF(13.5, 12));
        break;
    case Icon::Plus:
        p.drawLine(QPointF(8, 3), QPointF(8, 13));
        p.drawLine(QPointF(3, 8), QPointF(13, 8));
        break;
    case Icon::Cross:
        p.drawLine(QPointF(4, 4), QPointF(12, 12));
        p.drawLine(QPointF(12, 4), QPointF(4, 12));
        break;
    case Icon::Pencil: {
        QPolygonF body;
        body << QPointF(3, 13) << QPointF(3.6, 10) << QPointF(11.2, 2.4) << QPointF(13.6, 4.8)
             << QPointF(6, 12.4);
        p.drawPolygon(body);
        break;
    }
    case Icon::Play: {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        QPolygonF triangle;
        triangle << QPointF(4, 2.5) << QPointF(4, 13.5) << QPointF(13, 8);
        p.drawPolygon(triangle);
        break;
    }
    case Icon::Pause:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(3.5, 3, 3, 10));
        p.drawRect(QRectF(9.5, 3, 3, 10));
        break;
    case Icon::Reset:
        p.drawArc(QRectF(3, 3, 10, 10), 40 * 16, 290 * 16);
        p.drawLine(QPointF(3, 2.5), QPointF(3, 5.5));
        p.drawLine(QPointF(3, 5.5), QPointF(6, 5.5));
        break;
    case Icon::Prev: {
        QPainterPath path;
        path.moveTo(10, 3);
        path.lineTo(5, 8);
        path.lineTo(10, 13);
        p.drawPath(path);
        break;
    }
    case Icon::Next: {
        QPainterPath path;
        path.moveTo(6, 3);
        path.lineTo(11, 8);
        path.lineTo(6, 13);
        p.drawPath(path);
        break;
    }
    }
    return pixmap;
}

} // namespace

IconButton::IconButton(Icon icon, QWidget* parent) : QPushButton(parent), icon_(icon) {
    setIconSize(QSize(14, 14));
    rebuild();
}

void IconButton::setIconKind(Icon icon) {
    if (icon_ == icon) return;
    icon_ = icon;
    builtFor_ = QColor();
    rebuild();
}

void IconButton::setTint(const QColor& color) {
    if (tint_ == color) return;
    tint_ = color;
    builtFor_ = QColor();
    rebuild();
}

void IconButton::changeEvent(QEvent* event) {
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::EnabledChange ||
        event->type() == QEvent::StyleChange)
        rebuild();
}

void IconButton::rebuild() {
    QColor color = tint_.isValid() ? tint_ : palette().color(QPalette::ButtonText);
    if (!isEnabled()) color.setAlphaF(0.4f);
    if (builtFor_ == color && builtEnabled_ == isEnabled()) return; // nothing to redraw
    builtFor_ = color;
    builtEnabled_ = isEnabled();
    QIcon icon;
    icon.addPixmap(drawIcon(icon_, iconSize().width(), color, devicePixelRatioF()));
    setIcon(icon);
}

} // namespace dockstyle
