// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QWidget>

class QHBoxLayout;
class QEvent;
class QLabel;
class QPushButton;

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

    // Places a widget at the top-right of the clock row (the dock puts the link indicator and the
    // menu there so they do not need a row of their own).
    void addTopRightWidget(QWidget* widget);

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
    int shownPixelSize_ = 0;
    int cornerWidth_ = 0; // width reserved for the top-right widget(s)
    bool stoppageShown_ = false;
    int shownRunning_ = -1; // -1 unknown, 0 stopped, 1 running
};
