// SPDX-License-Identifier: GPL-2.0-or-later
#include "SelfTest.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <util/platform.h>

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "AtomicFile.hpp"
#include "ClockDriver.hpp"
#include "FieldSinks.hpp"
#include "Format.hpp"
#include "MatchJson.hpp"
#include "PluginState.hpp"
#include "SelfTestVerdict.hpp"
#include "TextSourceFactory.hpp"
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
    const std::string tmp = out + ".tmp";
    {
        std::ofstream file(std::filesystem::u8path(tmp), std::ios::binary | std::ios::trunc);
        file << report.dump(2);
    }
    std::error_code ec;
    std::filesystem::rename(std::filesystem::u8path(tmp), std::filesystem::u8path(out), ec);
    if (ec)
        obs_log(LOG_ERROR, "selftest could not write %s: %s", out.c_str(), ec.message().c_str());
    return !ec;
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

class Run : public QObject {
public:
    Run(ClockDriver* driver, FieldSinks* sinks) : driver_(driver), sinks_(sinks) {}
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
    void finish();
    std::string clockText() const {
        return driver_->fields()[static_cast<std::size_t>(sb::FieldId::Clock)];
    }
    static void snapshotCounters(std::uint32_t& lagged, std::uint32_t& skipped);

    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
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
                      multiField_ && collection_ && duplicate_ && persistSaved_ && bad == 0 &&
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
                                     {"persistSaved", persistSaved_}}},
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
    Reopen(ClockDriver* driver, FieldSinks* sinks) : driver_(driver), sinks_(sinks) {}
    void check();

private:
    QPointer<ClockDriver> driver_;
    QPointer<FieldSinks> sinks_;
};

QPointer<Reopen> g_reopen;

void Reopen::check() {
    std::vector<std::string> problems;
    bool restored = false, receiving = false, settingsFile = false, previousMatch = false;
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
    }
    const bool pass = restored && receiving && settingsFile && previousMatch && problems.empty();
    const nlohmann::json report = {{"version", 1},
                                   {"pass", pass},
                                   {"reopen",
                                    {{"bindingRestored", restored},
                                     {"receiving", receiving},
                                     {"settingsFile", settingsFile},
                                     {"previousMatch", previousMatch},
                                     {"problems", problems}}}};
    const std::string out = outPath();
    writeJsonAtomic(out, report);
    obs_log(LOG_INFO, "selftest reopen %s: %s", pass ? "PASS" : "FAIL", out.c_str());
}

} // namespace

namespace selftest {

void maybeStart(ClockDriver* driver, FieldSinks* sinks) {
    if (env("OBS_SCOREBOARD_SELFTEST") != "1") return;
    if (g_started.exchange(true)) return;
    if (env("OBS_SCOREBOARD_SELFTEST_PASS") == "reopen") {
        g_reopen = new Reopen(driver, sinks);
        QTimer::singleShot(2000, g_reopen, []() {
            if (g_reopen) g_reopen->check();
        });
        return;
    }
    g_run = new Run(driver, sinks);
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
