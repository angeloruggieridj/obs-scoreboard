// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QString>

#include <string>
#include <vector>

#include "SportCatalog.hpp"

class QPushButton;
class QLabel;
class QLineEdit;
class QSpinBox;
class QVBoxLayout;

// The add and edit windows for one team's timed penalties. Both are non-modal: the clock keeps
// running while they are open, so they hold no game state and act by penalty id. The dock sends
// the command and reports the engine's answer (a penalty that disappeared is "penalty.notFound").
class PenaltyDialog : public QDialog {
    Q_OBJECT
public:
    // Add mode: choose one of the sport's penalty options and a player number.
    PenaltyDialog(const std::vector<sb::PenaltyOption>& options, const std::string& labelFormat,
                  QWidget* parent = nullptr);
    // Edit mode: change the remaining time of one penalty, addressed by id.
    PenaltyDialog(unsigned id, const QString& player, sb::Tenths remaining, sb::Tenths nextPhase,
                  const std::string& labelFormat, QWidget* parent = nullptr);

    void accept() override;
    // The chip selected in the add window (-1 in the edit window), and a way to pick one.
    int optionIndex() const;
    void selectOption(int index);
    int optionCount() const { return static_cast<int>(options_.size()); }
    // Whether the add window has a player number (the selftest checks it without a QMessageBox).
    bool validPlayer() const;

signals:
    void addRequested(int optionIndex, const QString& player);
    void editRequested(unsigned id, qint64 remainingTenths);

private:
    void finishLayout(QVBoxLayout* outer);
    void updatePreview();
    QString previewText() const;
    QString playerText() const;
    sb::Tenths timeTenths() const;
    sb::Tenths phase2Tenths() const;

    bool editMode_ = false;
    unsigned id_ = 0;
    QString fixedPlayer_;
    sb::Tenths fixedPhase2_ = 0;
    sb::Tenths initialTenths_ = 0; // the edit spins as they were when the window opened
    std::string labelFormat_;
    std::vector<sb::PenaltyOption> options_;

    std::vector<QPushButton*> chips_;
    QLineEdit* player_ = nullptr;
    QLabel* playerShown_ = nullptr;
    QSpinBox* minutes_ = nullptr;
    QSpinBox* seconds_ = nullptr;
    QLabel* preview_ = nullptr;
};
