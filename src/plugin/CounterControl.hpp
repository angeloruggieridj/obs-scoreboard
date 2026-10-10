// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

class QHBoxLayout;
class QLabel;
class QPushButton;

// A segmented counter: [-1 | value | +N ...]. Presentation only: it reports the delta the user
// asked for and shows whatever value it is given.
class CounterControl : public QWidget {
    Q_OBJECT
public:
    explicit CounterControl(QWidget* parent = nullptr);

    void setPlusValues(const std::vector<int>& values);
    void setValue(int value);
    // "#RRGGBB" background of the value box; empty = the theme's look. Restyles only on change.
    void setColor(const QString& hex);

signals:
    void deltaRequested(int delta);

private:
    QPushButton* makeButton(const QString& text, int delta);

    QHBoxLayout* top_ = nullptr;
    QHBoxLayout* extra_ = nullptr; // second row, used with two or more plus buttons
    QLabel* value_ = nullptr;
    std::vector<QPushButton*> plusButtons_;
    std::vector<int> plusValues_;
    std::optional<int> shownValue_;
    std::optional<QString> shownColor_;
};
