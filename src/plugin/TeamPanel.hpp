// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFrame>
#include <QString>

#include <optional>
#include <vector>

#include "PanelLayout.hpp"
#include "PanelModel.hpp"

class CounterControl;
class PenaltyList;
class QLabel;
class QLineEdit;
class QPushButton;

// One team's column: name, score, shots, fouls, second fouls, penalties. Shows what it is told;
// every user action leaves as a signal and goes through the dock's dispatch().
class TeamPanel : public QFrame {
    Q_OBJECT
public:
    explicit TeamPanel(QWidget* parent = nullptr);

    // Which side this column is: sets the color of the bar on top of the card.
    void setSide(bool away);

    void applyLayout(const sb::PanelLayout& layout);
    void setName(const QString& name);
    void setCounts(int score, int shots, int fouls, int fouls2);
    void setColors(const QString& scoreHex, const QString& foulHex);
    void setPenalties(const std::vector<sb::PenaltyRow>& rows);
    // "shots", "fouls", "fouls2" or "penalties": whether applyLayout() left that row shown.
    bool rowShown(const char* id) const;
    std::size_t penaltyRowCount() const;
    // Whether the "+" add-penalty button is currently shown (same as the penalties row).
    bool penaltyAddShown() const { return penaltiesShown_; }

signals:
    void nameEdited(const QString& name);
    void statDelta(int stat, int delta); // stat = static_cast<int>(sb::Stat)
    void penaltyCancelRequested(unsigned id);
    void penaltyEditRequested(unsigned id);
    void penaltyAddRequested();

private:
    struct Row {
        QLabel* caption = nullptr;
        CounterControl* counter = nullptr;
        bool shown = false;
    };
    Row makeRow(int stat);
    void setRowShown(Row& row, bool shown);

    QLineEdit* name_ = nullptr;
    QLabel* scoreCaption_ = nullptr;
    CounterControl* score_ = nullptr;
    Row shots_;
    Row fouls_;
    Row fouls2_;
    QLabel* penaltiesCaption_ = nullptr;
    QPushButton* penaltiesAdd_ = nullptr;
    PenaltyList* penalties_ = nullptr;
    bool penaltiesShown_ = false;
    std::optional<sb::PanelLayout> applied_;
};
