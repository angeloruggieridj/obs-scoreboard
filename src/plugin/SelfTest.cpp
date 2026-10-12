// SPDX-License-Identifier: GPL-2.0-or-later
#include "SelfTest.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>
#include <obs.hpp>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <util/platform.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPointer>
#include <QSpinBox>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "AtomicFile.hpp"
#include "ClockDriver.hpp"
#include "FieldSinks.hpp"
#include "Format.hpp"
#include "GlobalSettings.hpp"
#include "Hotkeys.hpp"
#include "MatchJson.hpp"
#include "PanelLayout.hpp"
#include "PanelModel.hpp"
#include "PenaltyDialog.hpp"
#include "PluginState.hpp"
#include "ScoreboardDock.hpp"
#include "SelfTestVerdict.hpp"
#include "SportCatalog.hpp"
#include "TextSourceFactory.hpp"
#include "VendorApi.hpp"
#include "VendorRegistry.hpp"
#include "plugin-support.h"

namespace {

// The clock is set this far past the window so it is still running when OBS is closed.
constexpr int kLeadSeconds = 60;
constexpr const char* kSourceName = "SB Selftest Clock";
constexpr const char* kRenamed = "SB Selftest Clock (renamed)";
constexpr const char* kColorId = "color_source";
#ifdef _WIN32
constexpr const char* kTextId = "text_gdiplus";
constexpr const char* kFileKey = "read_from_file";
constexpr const char* kFontFace = "Arial";
#else
constexpr const char* kTextId = "text_ft2_source";
constexpr const char* kFileKey = "from_file";
#ifdef __APPLE__
constexpr const char* kFontFace = "Helvetica";
#else
constexpr const char* kFontFace = "DejaVu Sans";
#endif
#endif

std::string env(const char* key) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : std::string();
}

// A path from the environment, as UTF-8 (on Windows getenv would give the ANSI code page).
std::string envPathUtf8(const char* key) {
#ifdef _WIN32
    const std::wstring wide(key, key + std::strlen(key));
    const wchar_t* v = _wgetenv(wide.c_str());
    return v ? std::filesystem::path(v).u8string() : std::string();
#else
    return env(key);
#endif
}

int envInt(const char* key, int fallback) {
    const std::string v = env(key);
    if (v.empty()) return fallback;
    char* end = nullptr;
    const long n = std::strtol(v.c_str(), &end, 10);
    return end && *end == '\0' ? static_cast<int>(n) : fallback;
}

std::string sourceText(obs_source_t* source) {
    OBSDataAutoRelease settings = obs_source_get_settings(source);
    const char* t = obs_data_get_string(settings, "text");
    return t ? std::string(t) : std::string();
}

// Source lookups by uuid. The selftest keeps only uuid and name of the sources it creates and
// finds them again in a local variable: a strong reference held across a scene collection
// change is reported by OBS as a leaked source and blocks saving.
std::string textByUuid(const std::string& uuid, bool* found = nullptr) {
    OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
    if (found) *found = source != nullptr;
    return source ? sourceText(source) : std::string();
}

std::string uuidByName(const std::string& name) {
    OBSSourceAutoRelease source = obs_get_source_by_name(name.c_str());
    const char* uuid = source ? obs_source_get_uuid(source) : nullptr;
    return uuid ? std::string(uuid) : std::string();
}

bool writeJsonAtomic(const std::string& out, const nlohmann::json& report) {
    std::string error;
    const bool ok = writeFileAtomic(
        out, report.dump(2, ' ', false, nlohmann::json::error_handler_t::replace), &error);
    if (!ok) obs_log(LOG_ERROR, "selftest could not write %s: %s", out.c_str(), error.c_str());
    return ok;
}

std::string readTextFile(const std::string& path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string outPath() {
    std::string out = envPathUtf8("OBS_SCOREBOARD_SELFTEST_OUT");
    if (out.empty()) {
        std::error_code ec;
        out =
            (std::filesystem::temp_directory_path(ec) / "obs-scoreboard-selftest.json").u8string();
    }
    return out;
}

// A vendor transport that never touches obs-websocket: it records the requests and the events so
// the selftest can drive VendorApi (the real registry is Task 7.1.5, via call_request).
class FakeRegistry : public VendorRegistry {
public:
    bool available = true;
    bool registerVendor() override { return available; }
    bool registerRequest(std::string_view type, Handler handler) override {
        if (!available) return false;
        if (!fallback_) fallback_ = handler;
        handlers_[std::string(type)] = std::move(handler);
        return true;
    }
    bool emitEvent(std::string_view type, std::string_view json) override {
        events_.emplace_back(std::string(type), std::string(json));
        return true;
    }
    void unregisterAll() override { handlers_.clear(); }
    // The plugin registers exactly the 20 protocol names, so an unknown type has no handler; route
    // it to a registered one anyway, so the request still reaches the parser and answers
    // "unknownRequest" (as the real transport would only ever see registered types).
    std::string request(const char* type, const char* json = "{}") {
        const auto it = handlers_.find(type);
        if (it != handlers_.end()) return it->second(type, json);
        return fallback_ ? fallback_(type, json) : std::string{};
    }
    std::map<std::string, Handler> handlers_;
    std::vector<std::pair<std::string, std::string>> events_;

private:
    Handler fallback_;
};

class Run : public QObject {
public:
    Run(ClockDriver* driver, FieldSinks* sinks, Hotkeys* hotkeys)
        : driver_(driver), sinks_(sinks), hotkeys_(hotkeys) {}
    ~Run() override {
        if (sinks_) sinks_->setPushObserver({}); // the observer captures `this`
        stopCapture();
    }

    void begin();
    // Returns true when a live capture was stopped.
    bool stopCapture();

private:
    static void onFrame(void* param, video_data* frame);
    void after(int ms, void (Run::*step)()) {
        QTimer::singleShot(ms, this, [this, step]() { (this->*step)(); });
    }
    void fail(const std::string& why) { problems_.push_back(why); }

    void startCapture();
    void startClock();
    void endWindow();
    void checkRename();
    void checkFileMode();
    void checkDelete();
    void checkWrongType();
    void startMultiField();
    void checkMultiField();
    void startCollection();
    void checkCollectionLeft();
    void checkCollectionBack();
    void checkDuplicate();
    void checkDuplicateRename();
    void startPersist();
    void checkPersist();
    void checkSportsAndDock();
    void checkFoulsAndPenalties();
    void checkVendorApi();
    void checkVendorLive();
    void finish();
    std::string clockText() const {
        return driver_->fields()[static_cast<std::size_t>(sb::FieldId::Clock)];
    }
    static void snapshotCounters(std::uint32_t& lagged, std::uint32_t& skipped);

    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
    QPointer<Hotkeys> hotkeys_;
    int secs_ = 20;
    int requestedFps_ = 0;
    double actualFps_ = 0.0;
    std::string out_;
    OBSSourceAutoRelease source_;
    OBSSourceAutoRelease color_;
    std::uint64_t startNs_ = 0;
    sbtest::VerdictInput input_;
    sbtest::Verdict verdict_;
    bool rename_ = false, fileMode_ = false, delete_ = false, wrongType_ = false;
    bool multiField_ = false, collection_ = false, duplicate_ = false, persistSaved_ = false;
    bool dock14_ = false, sportSwitch_ = false, penaltyRefresh_ = false, hotkeySaved_ = false;
    bool futsalFouls_ = false, basketFouls_ = false, penaltyControls_ = false;
    bool vendorApi_ = false;
    bool vendorLive_ = false;
    // Only uuid and name of the sources made by the later checks (see textByUuid).
    std::string homeUuid_, collUuid_, collName_, persistName_, collectionName_;
    bool leftCollection_ = false;
    std::vector<std::string> problems_;

    std::mutex mutex_; // guards everything the video thread touches
    bool capturing_ = false;
    std::uint32_t outW_ = 0, outH_ = 0;
    int boxX_ = 0, boxY_ = 0, boxW_ = 0, boxH_ = 0;
    std::vector<sbtest::FrameSample> frames_;
    std::uint64_t badTimestamps_ = 0;
    // OBS reports totals since it started; the report also gives the deltas over the window.
    std::uint32_t laggedAtStart_ = 0, skippedAtStart_ = 0, laggedAtEnd_ = 0, skippedAtEnd_ = 0;
};

QPointer<Run> g_run;
std::atomic<bool> g_started{false};

void Run::begin() {
    secs_ = std::clamp(envInt("OBS_SCOREBOARD_SELFTEST_SECS", 20), 5, 120);
    requestedFps_ = envInt("OBS_SCOREBOARD_SELFTEST_FPS", 0);
    out_ = outPath();
    if (!driver_ || !sinks_) {
        fail("plugin not initialised");
        finish();
        return;
    }

    obs_video_info ovi{};
    if (!obs_get_video_info(&ovi) || ovi.fps_den == 0) {
        fail("no video info");
        finish();
        return;
    }
    actualFps_ = static_cast<double>(ovi.fps_num) / ovi.fps_den;
    if (requestedFps_ > 0 && std::abs(actualFps_ - requestedFps_) > 0.01)
        fail("profile runs at " + std::to_string(actualFps_) + " fps, not " +
             std::to_string(requestedFps_));

    OBSSourceAutoRelease sceneSource = obs_frontend_get_current_scene();
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    if (!scene) {
        fail("no current scene");
        finish();
        return;
    }
    OBSDataAutoRelease font = obs_data_create();
    obs_data_set_string(font, "face", kFontFace);
    obs_data_set_int(font, "size", 128);
    OBSDataAutoRelease settings = obs_data_create();
    obs_data_set_string(settings, "text", "--:--");
    obs_data_set_obj(settings, "font", font);
    // Versioned types are registered as "<id>_v<N>" (libobs/obs-module.c): creating by the bare
    // id would give the obsolete v1. Ask libobs for the current version of the type.
    const char* textType = obs_get_latest_input_type_id(kTextId);
    source_ = obs_source_create(textType ? textType : kTextId, kSourceName, settings, nullptr);
    obs_sceneitem_t* item = source_ ? obs_scene_add(scene, source_) : nullptr;
    if (!item) {
        fail(std::string("could not create a ") + kTextId + " source");
        finish();
        return;
    }
    const vec2 pos{{{40.0f, 40.0f}}};
    obs_sceneitem_set_pos(item, &pos);

    checkSportsAndDock(); // before the clock starts and any save is armed: nothing is written
    checkFoulsAndPenalties();
    checkVendorApi();
    checkVendorLive(); // real obs-websocket in the portable profile; no-op when it is absent
    driver_->apply(sb::cmd::ClockStop{});
    driver_->apply(sb::cmd::ClockSet{sb::seconds(secs_ + kLeadSeconds)});
    sinks_->setPushObserver([this](sb::FieldId field, const std::string& text, std::uint64_t ns) {
        if (field == sb::FieldId::Clock) input_.pushes.push_back({ns, text});
    });
    if (!sinks_->bindTo(sb::FieldId::Clock, {"", kSourceName})) {
        fail("could not bind the clock to the selftest source by name");
        finish();
        return;
    }
    after(1500, &Run::startCapture);
}

// The dock must follow every one of the 14 presets, and a queued penalty cancelled must drop its
// row even though no text field changed. Runs before the clock starts, so nothing is written.
void Run::checkSportsAndDock() {
    const sb::MatchSettings original = driver_->engine().settings();
    GlobalSettings scratch; // defaults only: never loaded from or saved to disk
    ScoreboardDock probe(driver_, sinks_, &scratch, nullptr);
    dock14_ = true;
    sportSwitch_ = true;
    for (const sb::SportPreset& sport : sb::builtInSports()) {
        driver_->replaceMatch(sb::MatchEngine(sb::MatchSettings::forSport(sport)));
        const sb::PanelLayout layout = sb::panelLayout(driver_->engine().settings());
        QStringList expected;
        if (layout.shots) expected << "shots";
        if (layout.fouls) expected << "fouls";
        if (layout.fouls2) expected << "fouls2";
        if (layout.penalties) expected << "penalties";
        if (layout.strength) expected << "strength";
        if (layout.stoppage) expected << "stoppage";
        if (probe.shownRows() != expected) {
            dock14_ = false;
            fail("dock14: the dock rows for " + sport.id + " differ from the layout");
        }
        const sb::FieldValues& f = driver_->fields();
        const auto at = [&f](sb::FieldId id) -> const std::string& {
            return f[static_cast<std::size_t>(id)];
        };
        const bool coherent = at(sb::FieldId::Period) == "1" && !at(sb::FieldId::Clock).empty() &&
                              at(sb::FieldId::HomeShots).empty() != sport.shots &&
                              at(sb::FieldId::HomeFouls).empty() != sport.fouls &&
                              at(sb::FieldId::HomeFouls2).empty() != sport.fouls2;
        if (!coherent) {
            sportSwitch_ = false;
            fail("sportSwitch: the fields for " + sport.id + " are not coherent");
        }
    }
    // A queued penalty cancelled: no text field changes, the rows must still follow.
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    for (const char* player : {"1", "2", "3"})
        driver_->apply(sb::cmd::PenaltyAdd{sb::Team::Home, player, {{sb::minutes(2), true}}});
    penaltyRefresh_ = probe.penaltyRowCount(sb::Team::Home) == 3;
    for (const sb::PenaltyRow& row : sb::penaltyRows(driver_->engine().penalties(sb::Team::Home)))
        if (row.queued) driver_->apply(sb::cmd::PenaltyCancel{row.id});
    const std::size_t expectedRows =
        sb::penaltyRows(driver_->engine().penalties(sb::Team::Home)).size();
    penaltyRefresh_ = penaltyRefresh_ && expectedRows == 2 &&
                      probe.penaltyRowCount(sb::Team::Home) == expectedRows;
    if (!penaltyRefresh_)
        fail("penaltyRefresh: the dock rows differ from the engine after a cancel");
    driver_->replaceMatch(sb::MatchEngine(original));
}

// Futsal and basketball fouls per period (and the restore when going back or resuming), and the
// penalty add/edit/cancel path through the real dock. Runs before the clock starts, so nothing
// is written; the settings are restored at the end, like checkSportsAndDock().
void Run::checkFoulsAndPenalties() {
    const auto field = [this](sb::FieldId id) -> std::string {
        return driver_->fields()[static_cast<std::size_t>(id)];
    };
    const sb::MatchSettings original = driver_->engine().settings();
    GlobalSettings scratch; // defaults only: never loaded from or saved to disk
    ScoreboardDock probe(driver_, sinks_, &scratch, nullptr);

    // Futsal: fouls restart each half; coming back restores the 1st half; a snapshot keeps both.
    futsalFouls_ = true;
    driver_->replaceMatch(sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("futsal"))));
    if (probe.penaltyAddShown(sb::Team::Home)) {
        futsalFouls_ = false;
        fail("futsalFouls: the add button is shown without timed penalties");
    }
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Fouls, 3});
    if (field(sb::FieldId::HomeFouls) != "3") {
        futsalFouls_ = false;
        fail("futsalFouls: the 1st half fouls are wrong");
    }
    driver_->apply(sb::cmd::PeriodNext{});
    if (field(sb::FieldId::HomeFouls) != "0") {
        futsalFouls_ = false;
        fail("futsalFouls: the 2nd half should restart the fouls");
    }
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Fouls, 2});
    driver_->apply(sb::cmd::PeriodPrev{});
    if (field(sb::FieldId::HomeFouls) != "3") {
        futsalFouls_ = false;
        fail("futsalFouls: going back must restore the 1st half");
    }
    {
        sb::MatchEngine back = sb::MatchEngine::fromSnapshot(driver_->engine().snapshot());
        if (back.fields()[static_cast<std::size_t>(sb::FieldId::HomeFouls)] != "3") {
            futsalFouls_ = false;
            fail("futsalFouls: the snapshot did not keep the 1st half fouls");
        }
        back.apply(sb::cmd::PeriodNext{}, 0); // the snapshot must keep the 2nd half bucket too
        if (back.fields()[static_cast<std::size_t>(sb::FieldId::HomeFouls)] != "2") {
            futsalFouls_ = false;
            fail("futsalFouls: the snapshot did not keep the 2nd half fouls");
        }
    }

    // Basketball: fouls per quarter; overtime continues the 4th.
    basketFouls_ = true;
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("basketball"))));
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Fouls, 2}); // Q1
    driver_->apply(sb::cmd::PeriodNext{});                                // Q2
    driver_->apply(sb::cmd::PeriodNext{});                                // Q3
    driver_->apply(sb::cmd::PeriodNext{});                                // Q4
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Fouls, 4});
    driver_->apply(sb::cmd::PeriodNext{}); // overtime
    if (field(sb::FieldId::HomeFouls) != "4") {
        basketFouls_ = false;
        fail("basketFouls: overtime must continue the 4th quarter");
    }
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Fouls, 1});
    driver_->apply(sb::cmd::PeriodPrev{}); // back to Q4
    if (field(sb::FieldId::HomeFouls) != "5") {
        basketFouls_ = false;
        fail("basketFouls: going back to the 4th quarter");
    }
    driver_->apply(sb::cmd::PeriodPrev{}); // Q3
    driver_->apply(sb::cmd::PeriodPrev{}); // Q2
    driver_->apply(sb::cmd::PeriodPrev{}); // Q1
    if (field(sb::FieldId::HomeFouls) != "2") {
        basketFouls_ = false;
        fail("basketFouls: going back to the 1st quarter");
    }

    // Penalties: the real dock adds, edits and cancels by id; the strength text follows.
    penaltyControls_ = true;
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    if (!probe.penaltyAddShown(sb::Team::Home)) {
        penaltyControls_ = false;
        fail("penaltyControls: the add button is hidden with timed penalties");
    }
    const int widthBefore = probe.minimumSizeHint().width();
    probe.dispatch(sb::cmd::PenaltyAdd{sb::Team::Home, "24", {{sb::minutes(2), true}}});
    if (probe.penaltyRowCount(sb::Team::Home) != 1 ||
        field(sb::FieldId::HomePenalty1Player) != "24" || field(sb::FieldId::Strength) != "4-5") {
        penaltyControls_ = false;
        fail("penaltyControls: adding a penalty through the dock");
    }
    // The second is compound (2+2): its dock row carries the phase-2 time "+2:00".
    probe.dispatch(sb::cmd::PenaltyAdd{
        sb::Team::Home, "11", {{sb::minutes(2), true}, {sb::minutes(2), true}}});
    if (field(sb::FieldId::Strength) != "3-5" ||
        field(sb::FieldId::HomePenalty2Label) != "11 2:00") {
        penaltyControls_ = false;
        fail("penaltyControls: the strength text after a second penalty");
    }
    const sb::PenaltyId first = driver_->engine().penalties(sb::Team::Home).all().front().id;
    probe.dispatch(sb::cmd::PenaltyEdit{first, sb::minutes(1)});
    if (field(sb::FieldId::HomePenalty1Time) != "1:00") {
        penaltyControls_ = false;
        fail("penaltyControls: editing the remaining time by id");
    }
    probe.dispatch(sb::cmd::PenaltyAdd{sb::Team::Home, "7", {{sb::minutes(2), true}}}); // queued
    if (field(sb::FieldId::Strength) != "3-5") {
        penaltyControls_ = false;
        fail("penaltyControls: a queued penalty must not change the strength");
    }
    // Three rows, one of them compound: the dock must not have grown wider (Task 6.2 rule).
    if (probe.penaltyRowCount(sb::Team::Home) != 3 ||
        probe.minimumSizeHint().width() > widthBefore) {
        penaltyControls_ = false;
        fail("penaltyControls: the penalty rows widened the dock");
    }
    const std::size_t withQueued = probe.penaltyRowCount(sb::Team::Home);
    const sb::PenaltyId queued = driver_->engine().penalties(sb::Team::Home).all().back().id;
    probe.dispatch(sb::cmd::PenaltyCancel{queued});
    if (withQueued != 3 || probe.penaltyRowCount(sb::Team::Home) != 2) {
        penaltyControls_ = false;
        fail("penaltyControls: cancelling a queued penalty through the dock");
    }
    // A penalty that is gone is reported in the status line, never a crash (Review Focus 1).
    probe.dispatch(sb::cmd::PenaltyEdit{queued, sb::minutes(1)});
    {
        const QLabel* status = probe.findChild<QLabel*>(QStringLiteral("dockStatus"));
        const QString notFound = QString::fromUtf8(obs_module_text("Reason.penalty.notFound"));
        if (!status || status->text() != notFound) {
            penaltyControls_ = false;
            fail("penaltyControls: a gone penalty is not reported in the status line");
        }
    }

    // The real add window: pick the compound option, type a player, accept.
    {
        const sb::SportPreset& hockey = *sb::findSport("ice_hockey");
        auto* add = new PenaltyDialog(hockey.penaltyOptions, "{player} {time} {phase2}", &probe);
        auto* combo = add->findChild<QComboBox*>(QStringLiteral("penaltyOption"));
        auto* player = add->findChild<QLineEdit*>(QStringLiteral("penaltyPlayer"));
        if (!combo || !player) {
            penaltyControls_ = false;
            fail("penaltyControls: the add window widgets are missing");
        } else {
            if (add->validPlayer()) { // an empty player must be invalid (no QMessageBox opened)
                penaltyControls_ = false;
                fail("penaltyControls: the add window accepted an empty player");
            }
            int compound = -1;
            for (int i = 0; i < static_cast<int>(hockey.penaltyOptions.size()); ++i)
                if (hockey.penaltyOptions[static_cast<std::size_t>(i)].phases.size() > 1) {
                    compound = i;
                    break;
                }
            combo->setCurrentIndex(compound);
            player->setText(QStringLiteral("24"));
            int seenIndex = -1;
            QString seenPlayer;
            connect(add, &PenaltyDialog::addRequested, &probe,
                    [&seenIndex, &seenPlayer](int index, const QString& p) {
                        seenIndex = index;
                        seenPlayer = p;
                    });
            const bool valid = add->validPlayer();
            add->accept();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            if (!valid || seenIndex != compound || seenPlayer != QStringLiteral("24")) {
                penaltyControls_ = false;
                fail("penaltyControls: the add window did not emit the option and player");
            }
        }
    }

    // The real edit window: the preview, then a real change and the no-op accept.
    {
        auto* edit = new PenaltyDialog(7, QStringLiteral("24"), sb::minutes(1) + sb::seconds(32),
                                       sb::minutes(2), "{player} {time} {phase2}", &probe);
        auto* preview = edit->findChild<QLabel*>(QStringLiteral("penaltyPreview"));
        auto* minutes = edit->findChild<QSpinBox*>(QStringLiteral("penaltyMinutes"));
        auto* seconds = edit->findChild<QSpinBox*>(QStringLiteral("penaltySeconds"));
        if (!preview || !minutes || !seconds ||
            preview->text() != QStringLiteral("24 1:32 +2:00") || minutes->value() != 1 ||
            seconds->value() != 32) {
            penaltyControls_ = false;
            fail("penaltyControls: the edit window preview or initial time");
        } else {
            int seen = 0;
            connect(edit, &PenaltyDialog::editRequested, &probe,
                    [&seen](unsigned, qint64) { ++seen; });
            edit->accept(); // no change: must not emit (the running clock has consumed time)
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            if (seen != 0) {
                penaltyControls_ = false;
                fail("penaltyControls: OK without changes re-sent the old time");
            }
        }
    }
    {
        auto* edit = new PenaltyDialog(9, QStringLiteral("24"), sb::minutes(1) + sb::seconds(32),
                                       sb::minutes(2), "{player} {time} {phase2}", &probe);
        auto* minutes = edit->findChild<QSpinBox*>(QStringLiteral("penaltyMinutes"));
        auto* seconds = edit->findChild<QSpinBox*>(QStringLiteral("penaltySeconds"));
        minutes->setValue(0);
        seconds->setValue(45);
        unsigned seenId = 0;
        qint64 seenTenths = 0;
        connect(edit, &PenaltyDialog::editRequested, &probe,
                [&seenId, &seenTenths](unsigned id, qint64 tenths) {
                    seenId = id;
                    seenTenths = tenths;
                });
        edit->accept();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (seenId != 9 || seenTenths != sb::seconds(45)) {
            penaltyControls_ = false;
            fail("penaltyControls: the edit window did not emit the new time");
        }
    }

    // The dock opens the add window; replacing the match closes it (Review Focus 2).
    {
        auto* plus = probe.findChild<QToolButton*>(QStringLiteral("penaltyAdd"));
        if (!plus) {
            penaltyControls_ = false;
            fail("penaltyControls: the add button is not found");
        } else {
            plus->click();
            if (probe.findChildren<QDialog*>().isEmpty()) {
                penaltyControls_ = false;
                fail("penaltyControls: the dock did not open the add window");
            }
            const QList<PenaltyDialog*> opened = probe.findChildren<PenaltyDialog*>();
            if (!opened.isEmpty() && opened.first()->isModal()) {
                penaltyControls_ = false;
                fail("penaltyControls: the penalty window must not be modal");
            }
            driver_->replaceMatch(
                sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            if (!probe.findChildren<QDialog*>().isEmpty()) {
                penaltyControls_ = false;
                fail("penaltyControls: a replaced match left the penalty window open");
            }
        }
    }

    driver_->replaceMatch(sb::MatchEngine(original));
}

// The vendor API glue, driven by a fake registry: every request in the protocol, the envelopes,
// the thread hop, the events and the shutdown path. No obs-websocket is touched. Split by theme.
void Run::checkVendorApi() {
    const sb::MatchSettings original = driver_->engine().settings();
    // rememberSettings() writes through moduleConfigPath even on a scratch object: keep the bytes.
    const std::string settingsPath = moduleConfigPath("settings.json");
    const std::optional<std::string> settingsBytes = readFile(settingsPath);
    GlobalSettings scratch;
    ScoreboardDock probe(driver_, sinks_, &scratch, nullptr);
    vendorApi_ = true;

    const auto parse = [](const std::string& s) { return nlohmann::json::parse(s); };
    const auto newMatch = [&probe](const std::string& sportId) {
        return probe.newMatchForSport(sportId);
    };
    const auto isOk = [&parse](const std::string& s) {
        return parse(s) == nlohmann::json{{"result", "ok"}};
    };
    const auto isError = [&parse](const std::string& s, const char* code) {
        return parse(s) == nlohmann::json{{"result", "error"}, {"error", code}};
    };

    // --- obs-websocket absent: start() fails, nothing is registered, no crash. ---
    {
        FakeRegistry fake;
        fake.available = false;
        VendorApi api(driver_, newMatch, &fake, nullptr);
        if (api.start() || !fake.handlers_.empty()) {
            vendorApi_ = false;
            fail("vendorApi: start() must fail and register nothing without obs-websocket");
        }
    }

    FakeRegistry fake;
    VendorApi api(driver_, newMatch, &fake, nullptr);
    if (!api.start()) {
        vendorApi_ = false;
        fail("vendorApi: start() failed with a working registry");
    }

    // Declared after `fake`, the map `hasEvent` reads (the capture must see a live variable).
    const auto hasEvent = [&fake, &parse](const char* type, const auto& predicate) {
        for (const auto& e : fake.events_) {
            if (e.first != type) continue;
            if (predicate(parse(e.second))) return true;
        }
        return false;
    };

    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    driver_->apply(sb::cmd::ClockStop{});

    // --- Commands, envelopes and ScoreChanged. ---
    if (!isOk(fake.request("ClockStart")) || !driver_->engine().clock().running()) {
        vendorApi_ = false;
        fail("vendorApi: ClockStart");
    }
    if (!isOk(fake.request("ClockStop"))) {
        vendorApi_ = false;
        fail("vendorApi: ClockStop");
    }
    if (!isOk(fake.request("AddStat", R"({"team":"home","stat":"score","delta":2})"))) {
        vendorApi_ = false;
        fail("vendorApi: AddStat");
    }
    if (!hasEvent("ScoreChanged",
                  [](const nlohmann::json& j) { return j["team"] == "home" && j["score"] == 2; })) {
        vendorApi_ = false;
        fail("vendorApi: no ScoreChanged after AddStat");
    }

    // --- GetFields exposes all 28 fields; GetState summarises the match. ---
    const nlohmann::json fields = parse(fake.request("GetFields"));
    if (fields["result"] != "ok" || fields["fields"].size() != sb::kFieldCount ||
        fields["fields"]["home.score"] != "2") {
        vendorApi_ = false;
        fail("vendorApi: GetFields");
    }
    const nlohmann::json state = parse(fake.request("GetState"));
    if (state["state"]["sport"] != "ice_hockey" || state["state"]["period"] != 1 ||
        state["state"]["clockRunning"] != false || state["state"]["protocol"] != 1) {
        vendorApi_ = false;
        fail("vendorApi: GetState");
    }

    // --- A rejection, a confirmation and the confirmed retry. ---
    if (parse(fake.request("PenaltyCancel", R"({"id":999})")) !=
        nlohmann::json{{"result", "rejected"}, {"reason", "penalty.notFound"}}) {
        vendorApi_ = false;
        fail("vendorApi: penalty.notFound");
    }
    driver_->apply(sb::cmd::ClockAdjust{-sb::minutes(1)}); // mid-period, not at a limit
    if (parse(fake.request("PeriodNext")) !=
        nlohmann::json{{"result", "confirmation"}, {"reason", "period.midPeriod"}}) {
        vendorApi_ = false;
        fail("vendorApi: PeriodNext confirmation");
    }
    if (!isOk(fake.request("PeriodNext", R"({"confirmed":true})")) ||
        driver_->engine().periods().index() != 2) {
        vendorApi_ = false;
        fail("vendorApi: PeriodNext confirmed");
    }

    // --- Errors: unknown request, bad payload, unknown sport. ---
    if (!isError(fake.request("Nope"), "unknownRequest") ||
        !isError(fake.request("ClockStart", "[]"), "badRequest") ||
        !isError(fake.request("NewMatch", R"({"sport":"quidditch"})"), "unknownSport")) {
        vendorApi_ = false;
        fail("vendorApi: error envelopes");
    }

    // --- NewMatch on a live match needs the confirmation; the confirmed call succeeds. ---
    if (parse(fake.request("NewMatch", R"({"sport":"basketball"})")) !=
        nlohmann::json{{"result", "confirmation"}, {"reason", "match.inProgress"}}) {
        vendorApi_ = false;
        fail("vendorApi: NewMatch must ask for confirmation on a live match");
    }
    if (!isOk(fake.request("NewMatch", R"({"sport":"basketball","confirmed":true})")) ||
        driver_->engine().settings().sport.id != "basketball") {
        vendorApi_ = false;
        fail("vendorApi: NewMatch confirmed");
    }

    // --- Full StateChanged after NewMatch: hockey's "4-5" strength must be cleared to "". ---
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    driver_->apply(sb::cmd::PenaltyAdd{sb::Team::Home, "24", {{sb::minutes(2), true}}});
    if (driver_->fields()[static_cast<std::size_t>(sb::FieldId::Strength)] != "4-5") {
        vendorApi_ = false;
        fail("vendorApi: the hockey strength setup is wrong");
    }
    fake.events_.clear();
    fake.request("NewMatch", R"({"sport":"basketball","confirmed":true})");
    if (!hasEvent("StateChanged", [](const nlohmann::json& j) {
            return j["changed"].size() == sb::kFieldCount && j["changed"]["strength"] == "";
        })) {
        vendorApi_ = false;
        fail("vendorApi: NewMatch did not publish a full StateChanged");
    }

    // --- NewMatch closes an open penalty window (the Fase 6 rule). ---
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Score, 1}); // make it a live match
    {
        auto* plus = probe.findChild<QToolButton*>(QStringLiteral("penaltyAdd"));
        if (plus) plus->click();
        if (probe.findChildren<QDialog*>().isEmpty()) {
            vendorApi_ = false;
            fail("vendorApi: the penalty window did not open");
        }
        fake.request("NewMatch", R"({"sport":"ice_hockey","confirmed":true})");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (!probe.findChildren<QDialog*>().isEmpty()) {
            vendorApi_ = false;
            fail("vendorApi: NewMatch left the penalty window open");
        }
    }

    // --- StateChanged carries only the fields that changed. ---
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    fake.events_.clear();
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Score, 1});
    if (!hasEvent("StateChanged", [](const nlohmann::json& j) {
            return j["changed"].size() == 1 && j["changed"].contains("home.score");
        })) {
        vendorApi_ = false;
        fail("vendorApi: StateChanged did not carry exactly the changed field");
    }

    // --- MatchEnded once; a new match does not re-announce the old events. ---
    fake.events_.clear();
    driver_->apply(sb::cmd::EndMatch{});
    driver_->apply(sb::cmd::EndMatch{}); // rejected: no second event
    {
        int matchEnded = 0;
        for (const auto& e : fake.events_)
            if (e.first == "MatchEnded") ++matchEnded;
        if (matchEnded != 1) {
            vendorApi_ = false;
            fail("vendorApi: MatchEnded was not emitted exactly once");
        }
    }
    fake.events_.clear();
    driver_->replaceMatch(
        sb::MatchEngine(sb::MatchSettings::forSport(*sb::findSport("ice_hockey"))));
    {
        int reAnnounced = 0;
        for (const auto& e : fake.events_)
            if (e.first == "MatchEnded" || e.first == "PeriodEnded") ++reAnnounced;
        if (reAnnounced != 0) {
            vendorApi_ = false;
            fail("vendorApi: a new match re-announced the previous events");
        }
    }

    // --- Thread hop: a request from std::thread must reach the UI thread and come back. ---
    {
        std::atomic<bool> done{false};
        std::string answer;
        std::thread worker([&api, &answer, &done]() {
            answer =
                api.handleRequest(QStringLiteral("GetState"), QStringLiteral("{}")).toStdString();
            done = true;
        });
        const std::uint64_t deadline = os_gettime_ns() + 5'000'000'000ULL;
        while (!done && os_gettime_ns() < deadline)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        worker.join();
        if (!done || answer.find("\"sport\"") == std::string::npos) {
            vendorApi_ = false;
            fail("vendorApi: a request from another thread failed");
        }
    }

    // --- Thread timeout: with the UI not pumping events the request must answer notReady. ---
    {
        std::atomic<bool> done{false};
        std::string answer;
        std::thread worker([&api, &answer, &done]() {
            answer =
                api.handleRequest(QStringLiteral("GetState"), QStringLiteral("{}")).toStdString();
            done = true;
        });
        // Deliberately no processEvents(): the queued call cannot run, so it must time out.
        const std::uint64_t deadline = os_gettime_ns() + 4'000'000'000ULL;
        while (!done && os_gettime_ns() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        worker.join();
        if (!done || answer.find("\"notReady\"") == std::string::npos) {
            vendorApi_ = false;
            fail("vendorApi: a request with a stalled UI did not time out to notReady");
        }
    }

    // --- Shutdown: a request answers "notReady", never a crash. A throwaway driver: the real one
    // must stay alive for the rest of the selftest. ---
    {
        ClockDriver dying(driver_->engine().settings());
        FakeRegistry lateFake;
        VendorApi lateApi(&dying, newMatch, &lateFake, nullptr);
        lateApi.start();
        dying.shutdown();
        if (!isError(lateFake.request("GetState"), "notReady")) {
            vendorApi_ = false;
            fail("vendorApi: a request after shutdown was not notReady");
        }
    }

    driver_->replaceMatch(sb::MatchEngine(original));
    // Restore settings.json: rememberSettings() wrote through moduleConfigPath (item 20).
    if (settingsBytes) writeFileAtomic(settingsPath, *settingsBytes, nullptr);
}

// The live vendor path, in-process: obs-websocket is bundled in the portable profile, so
// call_request reaches our registered callback without a websocket server. Vendor events are not
// observable here (they go to the server): the fake registry covers them.
void Run::checkVendorLive() {
    vendorLive_ = true;
    proc_handler_t* global = obs_get_proc_handler();
    if (!global) {
        vendorLive_ = false;
        fail("vendorLive: no global proc handler");
        return;
    }
    calldata_t phprobe = {};
    const bool havePh = proc_handler_call(global, "obs_websocket_api_get_ph", &phprobe);
    // call_request lives on obs-websocket's own proc handler, returned by obs_websocket_api_get_ph
    // (the global handler only carries the getter): the "same ph" note in the plan's pre-task
    // check.
    proc_handler_t* apiPh =
        havePh ? static_cast<proc_handler_t*>(calldata_ptr(&phprobe, "ph")) : nullptr;
    calldata_free(&phprobe);
    if (!apiPh) return; // obs-websocket absent from the profile: nothing to test

    // Same layout as obs_websocket_request_response (obs-websocket-api.h), replicated to avoid the
    // include; WebSocketApi heap-allocates it and we free it with bfree like the official helper.
    struct RequestResponse {
        unsigned int status_code;
        char* comment;
        char* response_data;
    };
    const auto call = [apiPh](const char* requestType, const char* requestData) {
        std::string responseData;
        unsigned int status = 0;
        const std::uint64_t deadline = os_gettime_ns() + 5'000'000'000ULL;
        for (;;) {
            calldata_t cd = {};
            std::string body =
                std::string("{\"vendorName\":\"obs-scoreboard\",\"requestType\":\"") + requestType +
                "\",\"requestData\":" + requestData + "}";
            calldata_set_string(&cd, "request_type", "CallVendorRequest");
            calldata_set_string(&cd, "request_data", body.c_str());
            const bool ok = proc_handler_call(apiPh, "call_request", &cd);
            auto* resp =
                ok ? static_cast<RequestResponse*>(calldata_ptr(&cd, "response")) : nullptr;
            if (resp) {
                status = resp->status_code;
                if (resp->response_data) responseData = resp->response_data;
                if (resp->comment) bfree(resp->comment);
                if (resp->response_data) bfree(resp->response_data);
                bfree(resp);
            }
            calldata_free(&cd);
            if (resp) break;
            if (os_gettime_ns() >= deadline) break; // obs-websocket not ready: leave status 0
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return std::make_pair(status, responseData);
    };
    const auto parse = [](const std::string& s) { return nlohmann::json::parse(s); };

    // Response_data is the CallVendorRequest response: {vendorName, requestType, responseData}.
    // The plugin's envelope (result/state/fields/…) lives inside "responseData".
    const auto state = call("GetState", "{}");
    if (state.first != 100 || parse(state.second)["responseData"]["result"] != "ok" ||
        parse(state.second)["responseData"]["state"]["sport"].get<std::string>().empty()) {
        vendorLive_ = false;
        fail("vendorLive: GetState through call_request failed");
    }
    const auto toggle = call("ClockToggle", "{}");
    if (toggle.first != 100 || parse(toggle.second)["responseData"]["result"] != "ok") {
        vendorLive_ = false;
        fail("vendorLive: ClockToggle through call_request failed");
    }
    const auto bad = call("ClockAdjust", R"({"deltaTenths":"soon"})");
    if (bad.first != 100 || parse(bad.second)["responseData"]["error"] != "badField") {
        vendorLive_ = false;
        fail("vendorLive: a malformed request did not answer badField");
    }
    {
        std::atomic<bool> done{false};
        std::string answer;
        std::thread worker([&]() {
            answer = call("GetState", "{}").second;
            done = true;
        });
        const std::uint64_t deadline = os_gettime_ns() + 5'000'000'000ULL;
        while (!done && os_gettime_ns() < deadline)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        worker.join();
        if (!done || answer.find("\"sport\"") == std::string::npos) {
            vendorLive_ = false;
            fail("vendorLive: a call_request from another thread failed");
        }
    }
}

void Run::startCapture() {
    OBSSourceAutoRelease sceneSource = obs_frontend_get_current_scene();
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    obs_sceneitem_t* item = scene ? obs_scene_find_source(scene, kSourceName) : nullptr;
    obs_video_info ovi{};
    if (!item || !obs_get_video_info(&ovi)) {
        fail("selftest scene item disappeared");
        finish();
        return;
    }
    matrix4 box{};
    obs_sceneitem_get_box_transform(item, &box);
    const double sx = static_cast<double>(ovi.output_width) / ovi.base_width;
    const double sy = static_cast<double>(ovi.output_height) / ovi.base_height;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        outW_ = ovi.output_width;
        outH_ = ovi.output_height;
        boxX_ = static_cast<int>(std::floor(box.t.x * sx));
        boxY_ = static_cast<int>(std::floor(box.t.y * sy));
        boxW_ = static_cast<int>(std::ceil(box.x.x * sx));
        boxH_ = static_cast<int>(std::ceil(box.y.y * sy));
        capturing_ = true;
    }
    if (boxW_ <= 0 || boxH_ <= 0) fail("the selftest source has an empty box");
    video_scale_info conversion{};
    conversion.format = VIDEO_FORMAT_RGBA;
    conversion.width = ovi.output_width;
    conversion.height = ovi.output_height;
    conversion.range = VIDEO_RANGE_DEFAULT;
    conversion.colorspace = VIDEO_CS_DEFAULT;
    obs_add_raw_video_callback(&conversion, &Run::onFrame, this);
    after(500, &Run::startClock);
}

void Run::onFrame(void* param, video_data* frame) {
    auto* self = static_cast<Run*>(param);
    const std::uint64_t now = os_gettime_ns();
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (!self->capturing_) return;
    const std::uint64_t hash =
        sbtest::regionHash(frame->data[0], frame->linesize[0], self->outW_, self->outH_,
                           self->boxX_, self->boxY_, self->boxW_, self->boxH_);
    self->frames_.push_back({frame->timestamp, hash});
    // Sanity check of the shared time base: a frame is delivered after it was rendered, and
    // well within two seconds.
    if (frame->timestamp > now || now - frame->timestamp > 2'000'000'000ULL) ++self->badTimestamps_;
}

bool Run::stopCapture() {
    bool wasCapturing = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wasCapturing = capturing_;
        capturing_ = false;
    }
    if (wasCapturing) obs_remove_raw_video_callback(&Run::onFrame, this);
    return wasCapturing;
}

void Run::startClock() {
    if (!driver_) return;
    const sb::Micros t0 = ClockDriver::nowMicros();
    startNs_ = static_cast<std::uint64_t>(t0) * 1000ULL;
    driver_->applyAt(sb::cmd::ClockStart{}, t0);
    snapshotCounters(laggedAtStart_, skippedAtStart_);
    const bool twoDigits = driver_->engine().settings().twoDigitMinutes;
    input_.fps = actualFps_;
    for (int k = 0; k <= secs_; ++k) {
        input_.expectedTexts.push_back(sb::formatClock(sb::seconds(secs_ + kLeadSeconds - k),
                                                       sb::Direction::Down, twoDigits, false));
        input_.expectedNs.push_back(startNs_ + static_cast<std::uint64_t>(k) * 1'000'000'000ULL);
    }
    after(secs_ * 1000 + 700, &Run::endWindow);
}

void Run::endWindow() {
    stopCapture();
    snapshotCounters(laggedAtEnd_, skippedAtEnd_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        input_.frames = frames_;
    }
    // Only what was due inside the window: later pushes belong to the survival checks.
    const std::uint64_t windowEnd =
        startNs_ + static_cast<std::uint64_t>(secs_) * 1'000'000'000ULL + 500'000'000ULL;
    input_.pushes.erase(
        std::remove_if(input_.pushes.begin(), input_.pushes.end(),
                       [windowEnd](const sbtest::TextPush& p) { return p.ns > windowEnd; }),
        input_.pushes.end());
    verdict_ = sbtest::evaluate(input_);
    if (sinks_) sinks_->setPushObserver({});

    obs_source_set_name(source_, kRenamed);
    after(1200, &Run::checkRename);
}

void Run::checkRename() {
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    rename_ =
        sink.bound() && sink.binding().name == kRenamed &&
        sourceText(source_) == driver_->fields()[static_cast<std::size_t>(sb::FieldId::Clock)];
    if (!rename_) fail("rename: the renamed source stopped receiving the clock");

    OBSDataAutoRelease fileOn = obs_data_create();
    obs_data_set_bool(fileOn, kFileKey, true);
    obs_source_update(source_, fileOn);
    after(1200, &Run::checkFileMode);
}

void Run::checkFileMode() {
    OBSDataAutoRelease settings = obs_source_get_settings(source_);
    fileMode_ =
        !obs_data_get_bool(settings, kFileKey) &&
        sourceText(source_) == driver_->fields()[static_cast<std::size_t>(sb::FieldId::Clock)];
    if (!fileMode_) fail("fileMode: reading from a file was not turned off by the next change");

    OBSSourceAutoRelease sceneSource = obs_frontend_get_current_scene();
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    obs_sceneitem_t* item = scene ? obs_scene_find_source(scene, kRenamed) : nullptr;
    if (item) obs_sceneitem_remove(item);
    obs_source_remove(source_);
    source_ = nullptr;
    after(1200, &Run::checkDelete);
}

void Run::checkDelete() {
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    delete_ = !sink.bound() && sink.problem() == TextSink::Problem::Removed;
    if (!delete_) fail("delete: the binding survived the removal of its source");

    OBSSourceAutoRelease sceneSource = obs_frontend_get_current_scene();
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    const char* colorType = obs_get_latest_input_type_id(kColorId);
    color_ = obs_source_create(colorType ? colorType : kColorId, kSourceName, nullptr, nullptr);
    if (scene && color_) obs_scene_add(scene, color_);
    const bool bound = sinks_->bindTo(sb::FieldId::Clock, {"", kSourceName});
    wrongType_ =
        !bound && sinks_->sink(sb::FieldId::Clock).problem() == TextSink::Problem::WrongType;
    after(1200, &Run::checkWrongType);
}

void Run::checkWrongType() {
    if (color_) {
        OBSDataAutoRelease settings = obs_source_get_settings(color_);
        if (obs_data_has_user_value(settings, "text")) wrongType_ = false;
    }
    if (!wrongType_) fail("wrongType: a same-name source of another type was bound or written");
    if (color_) {
        OBSSourceAutoRelease sceneSource = obs_frontend_get_current_scene();
        obs_scene_t* scene = obs_scene_from_source(sceneSource);
        obs_sceneitem_t* item = scene ? obs_scene_find_source(scene, kSourceName) : nullptr;
        if (item) obs_sceneitem_remove(item);
        obs_source_remove(color_);
        color_ = nullptr; // from here on no strong reference outlives a step
    }
    startMultiField();
}

void Run::startMultiField() {
    std::string error;
    {
        OBSSourceAutoRelease source = createTextSource("SB Selftest Home Score", &error);
        if (!source) {
            fail("multiField: " + error);
            startCollection();
            return;
        }
        homeUuid_ = obs_source_get_uuid(source);
        if (!sinks_->bind(sb::FieldId::HomeScore, source))
            fail("multiField: the home score source was not bound");
    }
    driver_->apply(sb::cmd::AddStat{sb::Team::Home, sb::Stat::Score, 1});
    after(1200, &Run::checkMultiField);
}

void Run::checkMultiField() {
    multiField_ = sinks_->sink(sb::FieldId::HomeScore).bound() && textByUuid(homeUuid_) == "1";
    if (!multiField_) fail("multiField: the home score source did not show 1");
    startCollection();
}

void Run::startCollection() {
    std::string error;
    {
        OBSSourceAutoRelease source = createTextSource("SB Selftest Collection Clock", &error);
        if (!source) {
            fail("collection: " + error);
            startPersist();
            return;
        }
        collUuid_ = obs_source_get_uuid(source);
        collName_ = obs_source_get_name(source);
        if (!sinks_->bind(sb::FieldId::Clock, source))
            fail("collection: the clock was not bound to the new source");
    } // the strong reference is gone before the collection changes
    obs_frontend_save();
    char* current = obs_frontend_get_current_scene_collection();
    collectionName_ = current ? current : "";
    bfree(current);
    // Both frontend calls are synchronous on the UI thread (api-interface.cpp:187-205 ->
    // window-basic-main-scene-collections.cpp:134, :444), so the waits only cover the redraw.
    if (collectionName_.empty() || !obs_frontend_add_scene_collection("SB Selftest Other")) {
        fail("collection: could not create the second scene collection");
        startPersist();
        return;
    }
    after(2000, &Run::checkCollectionLeft);
}

void Run::checkCollectionLeft() {
    leftCollection_ = !sinks_->sink(sb::FieldId::Clock).bound();
    if (!leftCollection_) fail("collection: the clock stayed bound in the other collection");
    obs_frontend_set_current_scene_collection(collectionName_.c_str());
    after(2000, &Run::checkCollectionBack);
}

void Run::checkCollectionBack() {
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    bool found = false;
    const std::string text = textByUuid(collUuid_, &found);
    collection_ = leftCollection_ && sink.bound() && sink.binding().uuid == collUuid_ && found &&
                  text == clockText();
    if (!collection_) fail("collection: the clock binding did not come back with its collection");

    // "Duplicate" has no frontend API in OBS 30: do to the sources what
    // OBSBasic::AddSceneCollection(false) does (window-basic-main-scene-collections.cpp:164),
    // then what the plugin's SCENE_COLLECTION_CHANGED handler does afterwards.
    obs_reset_source_uuids();
    sinks_->refreshAll();
    after(1200, &Run::checkDuplicate);
}

void Run::checkDuplicate() {
    const std::string fresh = uuidByName(collName_);
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    duplicate_ =
        !fresh.empty() && fresh != collUuid_ && sink.bound() && sink.binding().uuid == fresh;
    if (!duplicate_) fail("duplicate: the binding did not follow the new uuid");
    collUuid_ = fresh;
    const std::string renamed = collName_ + " (renamed)";
    {
        OBSSourceAutoRelease source = obs_get_source_by_uuid(fresh.c_str());
        if (source) obs_source_set_name(source, renamed.c_str());
    }
    collName_ = renamed;
    after(1200, &Run::checkDuplicateRename);
}

void Run::checkDuplicateRename() {
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    if (!sink.bound() || sink.binding().name != collName_ || textByUuid(collUuid_) != clockText()) {
        duplicate_ = false;
        fail("duplicate: the binding did not follow the rename after the new uuid");
    }
    startPersist();
}

void Run::startPersist() {
    std::string error;
    {
        OBSSourceAutoRelease source = createTextSource("SB Persist Clock", &error);
        if (!source) {
            fail("persistSaved: " + error);
            finish();
            return;
        }
        persistName_ = obs_source_get_name(source);
        if (!sinks_->bind(sb::FieldId::Clock, source))
            fail("persistSaved: the clock was not bound to the persisted source");
    }
    // Give one hotkey a binding OBS can save, on a key no keyboard has (F13), then save the
    // collection: checkPersist() proves the binding went in, the reopen pass that it came back.
    if (hotkeys_) {
        const obs_hotkey_id id = hotkeys_->idOf("obs_scoreboard.clock_toggle");
        if (id != OBS_INVALID_HOTKEY_ID) {
            obs_key_combination_t combo{};
            combo.key = OBS_KEY_F13;
            obs_hotkey_load_bindings(id, &combo, 1);
        } else {
            fail("hotkeySaved: the clock_toggle hotkey was not registered");
        }
    } else {
        fail("hotkeySaved: the hotkeys are not initialised");
    }
    obs_frontend_save(); // queued by OBS (SaveProjectDeferred): the wait lets it run
    after(1200, &Run::checkPersist);
}

void Run::checkPersist() {
    std::error_code ec;
    const std::string settings = moduleConfigPath("settings.json");
    const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
    persistSaved_ = !settings.empty() &&
                    std::filesystem::exists(std::filesystem::u8path(settings), ec) &&
                    sink.bound() && sink.binding().name == persistName_;
    if (!persistSaved_) fail("persistSaved: settings.json is missing or the clock lost its source");
    if (hotkeys_) {
        const obs_hotkey_id id = hotkeys_->idOf("obs_scoreboard.clock_toggle");
        OBSDataArrayAutoRelease array = id != OBS_INVALID_HOTKEY_ID ? obs_hotkey_save(id) : nullptr;
        hotkeySaved_ = array && obs_data_array_count(array) >= 1;
    }
    if (!hotkeySaved_) fail("hotkeySaved: the clock_toggle binding is not in the saved hotkeys");
    finish();
}

void Run::snapshotCounters(std::uint32_t& lagged, std::uint32_t& skipped) {
    video_t* video = obs_get_video();
    lagged = obs_get_lagged_frames();
    skipped = video ? video_output_get_skipped_frames(video) : 0u;
}

void Run::finish() {
    std::uint64_t captured = 0, bad = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        captured = frames_.size();
        bad = badTimestamps_;
    }
    // The run leaves the clock going so that closing OBS with a running clock is exercised.
    if (driver_ && !driver_->engine().clock().running())
        fail("clock stopped before the end of the selftest");
    if (bad > 0) fail("frame timestamps are not on the os_gettime_ns base");
    const bool fpsOk = requestedFps_ <= 0 || std::abs(actualFps_ - requestedFps_) <= 0.01;
    const bool pass = verdict_.pass() && rename_ && fileMode_ && delete_ && wrongType_ &&
                      multiField_ && collection_ && duplicate_ && persistSaved_ && dock14_ &&
                      sportSwitch_ && penaltyRefresh_ && hotkeySaved_ && futsalFouls_ &&
                      basketFouls_ && penaltyControls_ && vendorApi_ && vendorLive_ && bad == 0 &&
                      fpsOk && problems_.empty();

    nlohmann::json changes = nlohmann::json::array();
    for (const sbtest::ChangeResult& c : verdict_.changes) {
        changes.push_back({{"text", c.text},
                           {"expectedNs", c.expectedNs},
                           {"pushNs", c.pushNs},
                           {"shownNs", c.shownNs},
                           {"latencyMs", c.latencyMs},
                           {"ok", c.ok}});
    }
    nlohmann::json problems = problems_;
    for (const std::string& p : verdict_.problems) problems.push_back(p);
    std::uint32_t laggedTotal = 0, skippedTotal = 0;
    snapshotCounters(laggedTotal, skippedTotal);
    const nlohmann::json report = {{"version", 1},
                                   {"pass", pass},
                                   {"fps", {{"requested", requestedFps_}, {"actual", actualFps_}}},
                                   {"window", {{"seconds", secs_}, {"startNs", startNs_}}},
                                   {"criteria",
                                    {{"sequence", verdict_.sequenceOk},
                                     {"drawn", verdict_.drawnOk},
                                     {"latency", verdict_.latencyOk}}},
                                   {"budgetMs", verdict_.budgetMs},
                                   {"maxLatencyMs", verdict_.maxLatencyMs},
                                   {"frames",
                                    {{"captured", captured},
                                     {"badTimestamps", bad},
                                     {"lagged", laggedAtEnd_ - laggedAtStart_},
                                     {"skipped", skippedAtEnd_ - skippedAtStart_},
                                     {"laggedTotal", laggedTotal},
                                     {"skippedTotal", skippedTotal}}},
                                   {"changes", changes},
                                   {"survival",
                                    {{"rename", rename_},
                                     {"fileMode", fileMode_},
                                     {"delete", delete_},
                                     {"wrongType", wrongType_},
                                     {"multiField", multiField_},
                                     {"collection", collection_},
                                     {"duplicate", duplicate_},
                                     {"persistSaved", persistSaved_},
                                     {"dock14", dock14_},
                                     {"sportSwitch", sportSwitch_},
                                     {"penaltyRefresh", penaltyRefresh_},
                                     {"hotkeySaved", hotkeySaved_},
                                     {"futsalFouls", futsalFouls_},
                                     {"basketFouls", basketFouls_},
                                     {"penaltyControls", penaltyControls_},
                                     {"vendorApi", vendorApi_},
                                     {"vendorLive", vendorLive_}}},
                                   {"problems", problems}};

    writeJsonAtomic(out_, report);
    obs_log(LOG_INFO, "selftest %s: %s", pass ? "PASS" : "FAIL", out_.c_str());

    // Keep a capture running until OBS exits, so closing OBS with the video callback live is
    // exercised too (stop() logs "capture stopped on exit"). Frames are no longer judged.
    obs_video_info ovi{};
    if (obs_get_video_info(&ovi)) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frames_.clear();
            capturing_ = true;
        }
        video_scale_info conversion{};
        conversion.format = VIDEO_FORMAT_RGBA;
        conversion.width = ovi.output_width;
        conversion.height = ovi.output_height;
        conversion.range = VIDEO_RANGE_DEFAULT;
        conversion.colorspace = VIDEO_CS_DEFAULT;
        obs_add_raw_video_callback(&conversion, &Run::onFrame, this);
    }
    // There is no public quit API: the runner script closes OBS once the report exists.
}

// The second launch of the same portable OBS: nothing is commanded, it only looks at what the
// first launch left behind.
class Reopen : public QObject {
public:
    Reopen(ClockDriver* driver, FieldSinks* sinks, Hotkeys* hotkeys)
        : driver_(driver), sinks_(sinks), hotkeys_(hotkeys) {}
    void check();

private:
    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
    QPointer<Hotkeys> hotkeys_;
};

QPointer<Reopen> g_reopen;

void Reopen::check() {
    std::vector<std::string> problems;
    bool restored = false, receiving = false, settingsFile = false, previousMatch = false;
    bool hotkeyPersisted = false;
    if (!driver_ || !sinks_) {
        problems.push_back("plugin not initialised");
    } else {
        const TextSink& sink = sinks_->sink(sb::FieldId::Clock);
        restored = sink.bound() && sink.binding().name == "SB Persist Clock";
        if (!restored)
            problems.push_back("bindingRestored: the clock is not bound to SB Persist Clock");
        const std::string clock = driver_->fields()[static_cast<std::size_t>(sb::FieldId::Clock)];
        receiving = restored && textByUuid(sink.binding().uuid) == clock;
        if (!receiving) problems.push_back("receiving: the source does not show the clock");

        std::string error;
        const std::string settings = moduleConfigPath("settings.json");
        settingsFile = !settings.empty() && sb::prefsFromJson(readTextFile(settings), &error);
        if (!settingsFile) problems.push_back("settingsFile: " + error);

        error.clear();
        const std::string previous = moduleConfigPath("previous-match.json");
        previousMatch = !previous.empty() && sb::snapshotFromJson(readTextFile(previous), &error);
        if (!previousMatch) problems.push_back("previousMatch: " + error);

        // The binding the first pass put on the clock_toggle hotkey must be back: Hotkeys::load
        // restored it from the scene collection on start.
        const obs_hotkey_id id =
            hotkeys_ ? hotkeys_->idOf("obs_scoreboard.clock_toggle") : OBS_INVALID_HOTKEY_ID;
        OBSDataArrayAutoRelease array = id != OBS_INVALID_HOTKEY_ID ? obs_hotkey_save(id) : nullptr;
        hotkeyPersisted = array && obs_data_array_count(array) >= 1;
        if (!hotkeyPersisted)
            problems.push_back("hotkeyPersisted: the clock_toggle binding did not come back");
    }
    const bool pass = restored && receiving && settingsFile && previousMatch && hotkeyPersisted &&
                      problems.empty();
    const nlohmann::json report = {{"version", 1},
                                   {"pass", pass},
                                   {"reopen",
                                    {{"bindingRestored", restored},
                                     {"receiving", receiving},
                                     {"settingsFile", settingsFile},
                                     {"previousMatch", previousMatch},
                                     {"hotkeyPersisted", hotkeyPersisted},
                                     {"problems", problems}}}};
    const std::string out = outPath();
    writeJsonAtomic(out, report);
    obs_log(LOG_INFO, "selftest reopen %s: %s", pass ? "PASS" : "FAIL", out.c_str());
}

} // namespace

namespace selftest {

void maybeStart(ClockDriver* driver, FieldSinks* sinks, Hotkeys* hotkeys) {
    if (env("OBS_SCOREBOARD_SELFTEST") != "1") return;
    if (g_started.exchange(true)) return;
    if (env("OBS_SCOREBOARD_SELFTEST_PASS") == "reopen") {
        g_reopen = new Reopen(driver, sinks, hotkeys);
        QTimer::singleShot(2000, g_reopen, []() {
            if (g_reopen) g_reopen->check();
        });
        return;
    }
    g_run = new Run(driver, sinks, hotkeys);
    // FINISHED_LOADING fires while OBSBasic is still building docks (a lesson from
    // obs-multireplay): wait on a timer, never by sleeping on the UI thread.
    QTimer::singleShot(2000, g_run, []() {
        if (g_run) g_run->begin();
    });
}

void stop() {
    delete g_reopen.data();
    if (!g_run) return;
    if (g_run->stopCapture()) obs_log(LOG_INFO, "selftest: capture stopped on exit");
    delete g_run.data();
}

} // namespace selftest
