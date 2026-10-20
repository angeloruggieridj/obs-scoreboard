// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "PluginState.hpp"

class QHBoxLayout;
class QEvent;
class QLabel;
class QPushButton;
class ClockCard;

// The clock block of the dock: big clock with minute arrows on its left and second arrows on its
// right, period stepper, stoppage stepper, start/pause and reset. Presentation only.
class ClockPanel : public QWidget {
    Q_OBJECT
public:
    explicit ClockPanel(QWidget* parent = nullptr);

    void setClock(const QString& text);
    void setPeriod(const QString& label, int regulationPeriods);
    void setStoppage(bool visible, int minutes);
    void setRunning(bool running);
    void setResetEnabled(bool enabled);

    // The border of the clock card as a progress bar. `shareLeft` is the share of the period still
    // to play (0..1; negative = the period has no length: nothing is drawn). `color` empty = the
    // theme's accent.
    void setPeriodProgress(double shareLeft);
    void setPeriodBar(sb::PeriodBarMode mode, const QString& color);
    // The color of the clock digits ("#RRGGBB"); empty = the theme's text color.
    void setDigitColor(const QString& hex);

signals:
    void arrowRequested(qint64 deltaTenths);
    void toggleRequested();
    void resetRequested();
    void periodStepRequested(int direction);
    void stoppageStepRequested(int delta);

protected:
    void resizeEvent(QResizeEvent* event) override;
    // The clock label is resized by the layout after this widget, so its final width is known
    // only here; fitting the font on that width keeps the digits from being clipped.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPushButton* makeArrow(const QString& label, const QString& tooltip, qint64 deltaTenths);
    void updateClockFont();

    QHBoxLayout* top_ = nullptr;
    QLabel* clock_ = nullptr;
    QLabel* period_ = nullptr;
    QLabel* of_ = nullptr;
    QWidget* stoppageBox_ = nullptr;
    QLabel* stoppage_ = nullptr;
    QPushButton* startStop_ = nullptr;
    QPushButton* reset_ = nullptr;
    ClockCard* card_ = nullptr;
    QString digitColor_;
    int shownPixelSize_ = 0;
    bool stoppageShown_ = false;
    int shownRunning_ = -1; // -1 unknown, 0 stopped, 1 running
};
