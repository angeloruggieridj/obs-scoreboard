// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "MatchEngine.hpp"
#include "PluginState.hpp"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

// Match settings in four tabs (Match / Texts / Look / Output). Non-modal: the clock keeps running,
// so the dialog keeps no copy of the game state; it hands the edited settings to an Applier (the
// dock), which classifies the change against the match as it is at that moment.
class MatchSettingsDialog : public QDialog {
    Q_OBJECT
public:
    struct Result {
        sb::MatchSettings settings;
        bool homeNameEdited = false; // false: keep the engine's current name
        bool awayNameEdited = false;
        std::string scoreColor; // "#RRGGBB" or empty = default
        std::string foulColor;
        bool fileOutputEnabled = false;
        std::string fileOutputDir;
        bool comfortableDock = false;
        std::string homeColor; // "#RRGGBB" or empty = the side's default
        std::string awayColor;
        sb::PeriodBarMode periodBar = sb::PeriodBarMode::Remaining;
        std::string periodBarColor; // empty = the theme's accent
        sb::ClockColorSettings clockColor;
    };
    using Applier = std::function<bool(const Result&)>;

    MatchSettingsDialog(const sb::MatchSettings& current, const sb::GlobalPrefs& prefs,
                        Applier applier, QWidget* parent = nullptr);
    void accept() override;

private:
    void buildMatchTab(QWidget* page);
    void buildTextsTab(QWidget* page);
    void buildLookTab(QWidget* page);
    void buildOutputTab(QWidget* page);
    void chooseFileOutputDir();
    void refreshFileOutput();
    void loadFrom(const sb::MatchSettings& settings, bool keepNames);
    bool collect(Result& out, QString* problem) const;
    void updateEnabled();
    void updatePreview();
    void pickColor(std::string& target, QPushButton* swatch);
    void showColor(QPushButton* swatch, const std::string& hex);
    // One labelled color row: a swatch that opens the color dialog and a "default" button that
    // clears `target`. `fallback` is the color shown (and used) while `target` is empty.
    QPushButton* addColorRow(QFormLayout* form, const QString& label, std::string* target,
                             const std::string& fallback);
    void updateClockColorEnabled();
    void sportActivated();
    void resetToSportDefaults();
    std::string currentSportId() const;

    sb::GlobalPrefs prefs_;
    sb::MatchSettings base_;
    std::string openedHome_;
    std::string openedAway_;
    Applier applier_;
    QTabWidget* tabs_ = nullptr;

    QComboBox* sport_ = nullptr;
    QLineEdit* home_ = nullptr;
    QLineEdit* away_ = nullptr;
    QSpinBox* periods_ = nullptr;
    QSpinBox* durationMinutes_ = nullptr;
    QSpinBox* durationSeconds_ = nullptr;
    QComboBox* direction_ = nullptr;
    QCheckBox* overtimeUnlimited_ = nullptr;
    QSpinBox* overtimeCount_ = nullptr;
    QSpinBox* overtimeMinutes_ = nullptr;
    QSpinBox* players_ = nullptr;
    QSpinBox* minPlayers_ = nullptr;
    QLineEdit* scoreValues_ = nullptr;

    QLineEdit* regulationLabels_ = nullptr;
    QLineEdit* overtimeLabels_ = nullptr;
    QLineEdit* penaltyFormat_ = nullptr;
    QLineEdit* strengthEven_ = nullptr;
    QLineEdit* strengthUneven_ = nullptr;
    QLabel* penaltyPreview_ = nullptr;
    QCheckBox* twoDigit_ = nullptr;
    QCheckBox* tenths_ = nullptr;
    QComboBox* stoppage_ = nullptr;
    QComboBox* playTime_ = nullptr;
    QPushButton* scoreSwatch_ = nullptr;
    QPushButton* foulSwatch_ = nullptr;
    std::string scoreColor_;
    std::string foulColor_;
    QComboBox* density_ = nullptr;
    std::string homeColor_;
    std::string awayColor_;
    std::string barColor_;
    std::string clockBase_;
    QComboBox* periodBar_ = nullptr;
    QComboBox* clockMode_ = nullptr;
    QCheckBox* clockBlend_ = nullptr;
    QPushButton* clockBaseSwatch_ = nullptr;
    struct RuleRow {
        QCheckBox* on = nullptr;
        QSpinBox* seconds = nullptr;
        QPushButton* swatch = nullptr;
        std::string color;
    };
    std::array<RuleRow, sb::kMaxClockColorRules> rules_;
    // The color each swatch shows while its value is empty.
    std::vector<std::pair<QPushButton*, std::string>> swatchDefaults_;
    QCheckBox* fileOutput_ = nullptr;
    QLabel* fileOutputDirLabel_ = nullptr;
    QPushButton* fileOutputChoose_ = nullptr;
    std::string fileOutputDir_;
};
