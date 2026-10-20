// SPDX-License-Identifier: GPL-2.0-or-later
#include "DockGrab.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QPalette>
#include <QPixmap>

#include "ClockDriver.hpp"
#include "DockStyle.hpp"
#include "GlobalSettings.hpp"
#include "MatchSettingsDialog.hpp"
#include "PenaltyDialog.hpp"
#include "ScoreboardDock.hpp"
#include "SportCatalog.hpp"
#include <obs-module.h>

#include "plugin-support.h"

namespace {

// A match in progress, so the pictures show what the dock looks like in use.
void stage(ClockDriver* driver, const sb::SportPreset& sport) {
    driver->replaceMatch(sb::MatchEngine(sb::MatchSettings::forSport(sport)));
    driver->apply(sb::cmd::SetTeamName{sb::Team::Home, "Roseville"});
    driver->apply(sb::cmd::SetTeamName{sb::Team::Away, "Stillwater"});
    const auto add = [driver](sb::Team team, sb::Stat stat, int n) {
        for (int i = 0; i < n; ++i) driver->apply(sb::cmd::AddStat{team, stat, 1});
    };
    // First the clock: moving it afterwards would also run down the penalties.
    const bool down = sport.direction == sb::Direction::Down;
    driver->apply(sb::cmd::ClockSet{down ? sb::minutes(14) + sb::seconds(32)
                                         : sb::minutes(32) + sb::seconds(15)});
    add(sb::Team::Home, sb::Stat::Score, 2);
    add(sb::Team::Away, sb::Stat::Score, 1);
    if (sport.shots) {
        add(sb::Team::Home, sb::Stat::Shots, 14);
        add(sb::Team::Away, sb::Stat::Shots, 9);
    }
    if (sport.fouls) {
        add(sb::Team::Home, sb::Stat::Fouls, 3);
        add(sb::Team::Away, sb::Stat::Fouls, 5);
    }
    if (sport.fouls2) add(sb::Team::Away, sb::Stat::Fouls2, 1);
    if (sport.hasPenalties()) {
        const auto& phases = sport.penaltyOptions.front().phases;
        driver->apply(sb::cmd::PenaltyAdd{sb::Team::Home, "24", phases});
        driver->apply(sb::cmd::PenaltyAdd{sb::Team::Home, "7", phases});
        driver->apply(sb::cmd::PenaltyAdd{sb::Team::Home, "9", phases}); // queued
        driver->apply(sb::cmd::PenaltyAdd{sb::Team::Away, "11", phases});
    }
}

bool save(ScoreboardDock* dock, int width, const QString& path) {
    // Shown off-screen: a hidden widget gets no resize events, so the clock font would not fit.
    dock->setAttribute(Qt::WA_DontShowOnScreen, true);
    dock->show();
    dock->resize(width, 10);
    if (dock->layout()) dock->layout()->activate();
    dock->resize(width, dock->minimumSizeHint().height());
    QApplication::processEvents();
    dock->resize(width, dock->minimumSizeHint().height());
    QApplication::processEvents();
    obs_log(LOG_INFO, "grab rows home=%zu away=%zu", dock->penaltyRowCount(sb::Team::Home),
            dock->penaltyRowCount(sb::Team::Away));
    for (QWidget* w : dock->findChildren<QWidget*>()) {
        const QString role = w->property("sbRole").toString();
        if (role == QLatin1String("segval") || role == QLatin1String("seg") ||
            role == QLatin1String("go") || role == QLatin1String("stop") ||
            role == QLatin1String("ghost"))
            obs_log(LOG_INFO, "geometry %s y=%d h=%d min=%d max=%d w=%d", qPrintable(role),
                    w->mapTo(dock, QPoint()).y(), w->height(), w->minimumHeight(),
                    w->maximumHeight(), w->width());
    }
    for (QFrame* card : dock->findChildren<QFrame*>()) {
        if (card->property("sbCard").toString() != QLatin1String("team")) continue;
        for (QWidget* w : card->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (w->isVisibleTo(card))
                obs_log(LOG_INFO, "team %s %s y=%d h=%d min=%d max=%d",
                        qPrintable(card->property("sbTeam").toString()),
                        w->metaObject()->className(), w->y(), w->height(), w->minimumHeight(),
                        w->maximumHeight());
    }
    const bool ok = dock->grab().save(path);
    dock->hide();
    return ok;
}

// A window shown off-screen at its natural size and saved.
bool saveWindow(QWidget* window, const QString& path) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    window->show();
    window->resize(window->size().expandedTo(window->sizeHint()));
    QApplication::processEvents();
    window->resize(window->size().expandedTo(window->sizeHint()));
    QApplication::processEvents();
    const bool ok = window->grab().save(path);
    window->hide();
    return ok;
}

} // namespace

namespace dockgrab {

int run(ClockDriver* driver, FieldSinks* sinks, const QString& directory) {
    if (!driver) return 0;
    // A top-level copy of the dock of its own: the real one sits in OBS's (hidden) dock widget.
    GlobalSettings scratch;
    ScoreboardDock probe(driver, sinks, &scratch, nullptr);
    ScoreboardDock* dock = &probe;
    QDir().mkpath(directory);
    // The theme is OBS's own (the script starts OBS with the Dark or the Light one): only the name
    // of the files changes.
    const QString theme = qEnvironmentVariable("OBS_SCOREBOARD_GRAB_THEME", QStringLiteral("dark"));
    int written = 0;
    const auto shoot = [&](const char* sportId, int width, dockstyle::Density density) {
        const sb::SportPreset* sport = sb::findSport(sportId);
        if (!sport) return;
        dockstyle::setDensity(density);
        stage(driver, *sport);
        QApplication::processEvents();
        const QString name =
            QStringLiteral("dock-%1-%2-%3-%4.png")
                .arg(QLatin1String(sportId), theme)
                .arg(density == dockstyle::Density::Compact ? "compact" : "comfortable")
                .arg(width);
        if (save(dock, width, QDir(directory).filePath(name))) ++written;
    };
    for (const char* sport : {"ice_hockey", "soccer", "futsal", "basketball"})
        shoot(sport, 360, dockstyle::Density::Compact);
    shoot("ice_hockey", 320, dockstyle::Density::Compact);
    shoot("ice_hockey", 440, dockstyle::Density::Compact);
    shoot("ice_hockey", 360, dockstyle::Density::Comfortable);
    dockstyle::setDensity(dockstyle::Density::Compact);
    {
        // The clock running: the button reads "Stop".
        stage(driver, *sb::findSport("ice_hockey"));
        driver->apply(sb::cmd::ClockStart{});
        QApplication::processEvents();
        if (save(dock, 360,
                 QDir(directory).filePath(QStringLiteral("dock-running-%1.png").arg(theme))))
            ++written;
        driver->apply(sb::cmd::ClockStop{});
    }

    // The windows of the plugin.
    {
        const sb::SportPreset* hockey = sb::findSport("ice_hockey");
        stage(driver, *hockey);
        const auto file = [&](const QString& name) {
            return QDir(directory).filePath(name + QLatin1Char('-') + theme +
                                            QStringLiteral(".png"));
        };
        {
            PenaltyDialog add(hockey->penaltyOptions, "{player} {time} {phase2}");
            if (saveWindow(&add, file(QStringLiteral("win-penalty-add")))) ++written;
        }
        {
            PenaltyDialog edit(1u, QStringLiteral("24"), sb::minutes(1) + sb::seconds(32),
                               sb::minutes(2), "{player} {time} {phase2}");
            if (saveWindow(&edit, file(QStringLiteral("win-penalty-edit")))) ++written;
        }
        {
            MatchSettingsDialog::Services services;
            services.sinks = sinks;
            services.updateVersion = QStringLiteral("1.1.0");
            services.updateUrl = QStringLiteral("https://example.org");
            services.updateNotes = QStringLiteral(
                "## 1.1.0\n\n### Added\n\n- A progress border around the clock.\n"
                "- Check for updates now.\n\n### Fixed\n\n- The counters line up with their "
                "buttons.");
            MatchSettingsDialog settings(
                driver->engine().settings(), scratch.prefs(),
                [](const MatchSettingsDialog::Result&) { return true; }, services);
            const std::pair<MatchSettingsDialog::Page, const char*> pagesToShow[] = {
                {MatchSettingsDialog::Page::Match, "match"},
                {MatchSettingsDialog::Page::Texts, "texts"},
                {MatchSettingsDialog::Page::Look, "look"},
                {MatchSettingsDialog::Page::Output, "output"},
                {MatchSettingsDialog::Page::Links, "links"},
                {MatchSettingsDialog::Page::Actions, "actions"},
                {MatchSettingsDialog::Page::Updates, "updates"}};
            for (const auto& entry : pagesToShow) {
                settings.showPage(entry.first);
                if (saveWindow(&settings,
                               file(QStringLiteral("win-settings-") + QLatin1String(entry.second))))
                    ++written;
            }
        }
    }
    // The optional looks, switched on: team colors, a consumed-time border and a clock that turns
    // amber, then red as the period runs out.
    {
        scratch.setTeamColors("#2E9E6B", "#B04BD6");
        scratch.setPeriodBar(sb::PeriodBarMode::Consumed, "#F2C94C");
        sb::ClockColorSettings clockColor;
        clockColor.mode = sb::ClockColorMode::ByTime;
        clockColor.base = "#FFFFFF";
        clockColor.rules = {{60, "#F2C94C"}, {30, "#E5484D"}};
        clockColor.blend = true;
        scratch.setClockColor(clockColor);
        const sb::SportPreset* hockey = sb::findSport("ice_hockey");
        ScoreboardDock colored(driver, sinks, &scratch, nullptr);
        stage(driver, *hockey);
        driver->apply(sb::cmd::ClockSet{sb::seconds(22)});
        QApplication::processEvents();
        if (save(&colored, 360,
                 QDir(directory).filePath(QStringLiteral("dock-custom-%1.png").arg(theme))))
            ++written;
        MatchSettingsDialog::Services services;
        MatchSettingsDialog settings(
            driver->engine().settings(), scratch.prefs(),
            [](const MatchSettingsDialog::Result&) { return true; }, services);
        settings.showPage(MatchSettingsDialog::Page::Look);
        if (saveWindow(&settings,
                       QDir(directory).filePath(
                           QStringLiteral("win-settings-look-custom-%1.png").arg(theme))))
            ++written;
    }
    QFile done(QDir(directory).filePath(QStringLiteral("grab.done")));
    if (done.open(QIODevice::WriteOnly)) done.write(QByteArray::number(written));
    return written;
}

} // namespace dockgrab
