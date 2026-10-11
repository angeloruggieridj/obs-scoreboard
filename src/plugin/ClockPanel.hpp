// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QWidget>

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

signals:
    void arrowRequested(qint64 deltaTenths);
    void toggleRequested();
    void resetRequested();
    void periodStepRequested(int direction);
    void stoppageStepRequested(int delta);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QPushButton* makeArrow(const QString& label, const QString& tooltip, qint64 deltaTenths);
    void updateClockFont();

    QLabel* clock_ = nullptr;
    QLabel* period_ = nullptr;
    QLabel* of_ = nullptr;
    QWidget* stoppageBox_ = nullptr;
    QLabel* stoppage_ = nullptr;
    QPushButton* startStop_ = nullptr;
    QPushButton* reset_ = nullptr;
    int shownPixelSize_ = 0;
    bool stoppageShown_ = false;
    int shownRunning_ = -1; // -1 unknown, 0 stopped, 1 running
};
