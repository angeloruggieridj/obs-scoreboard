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

#include "ClockDriver.hpp"
#include "FieldSinks.hpp"
#include "Format.hpp"
#include "SelfTestVerdict.hpp"
#include "plugin-support.h"

namespace {

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
    void finish();

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
    std::vector<std::string> problems_;

    std::mutex mutex_; // guards everything the video thread touches
    bool capturing_ = false;
    std::uint32_t outW_ = 0, outH_ = 0;
    int boxX_ = 0, boxY_ = 0, boxW_ = 0, boxH_ = 0;
    std::vector<sbtest::FrameSample> frames_;
    std::uint64_t badTimestamps_ = 0;
};

QPointer<Run> g_run;
std::atomic<bool> g_started{false};

void Run::begin() {
    if (!driver_ || !sinks_) return;
    secs_ = std::clamp(envInt("OBS_SCOREBOARD_SELFTEST_SECS", 20), 5, 120);
    requestedFps_ = envInt("OBS_SCOREBOARD_SELFTEST_FPS", 0);
    out_ = envPathUtf8("OBS_SCOREBOARD_SELFTEST_OUT");
    if (out_.empty()) {
        std::error_code ec;
        out_ =
            (std::filesystem::temp_directory_path(ec) / "obs-scoreboard-selftest.json").u8string();
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
    driver_->apply(sb::cmd::ClockSet{sb::seconds(secs_ + 5)});
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
    const bool twoDigits = driver_->engine().settings().twoDigitMinutes;
    input_.fps = actualFps_;
    for (int k = 0; k <= secs_; ++k) {
        input_.expectedTexts.push_back(
            sb::formatClock(sb::seconds(secs_ + 5 - k), sb::Direction::Down, twoDigits, false));
        input_.expectedNs.push_back(startNs_ + static_cast<std::uint64_t>(k) * 1'000'000'000ULL);
    }
    after(secs_ * 1000 + 700, &Run::endWindow);
}

void Run::endWindow() {
    stopCapture();
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
        color_ = nullptr;
    }
    finish();
}

void Run::finish() {
    std::uint64_t captured = 0, bad = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        captured = frames_.size();
        bad = badTimestamps_;
    }
    if (bad > 0) fail("frame timestamps are not on the os_gettime_ns base");
    const bool fpsOk = requestedFps_ <= 0 || std::abs(actualFps_ - requestedFps_) <= 0.01;
    const bool pass = verdict_.pass() && rename_ && fileMode_ && delete_ && wrongType_ &&
                      bad == 0 && fpsOk && problems_.empty();

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
    video_t* video = obs_get_video();
    const nlohmann::json report = {
        {"version", 1},
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
          {"lagged", obs_get_lagged_frames()},
          {"skipped", video ? video_output_get_skipped_frames(video) : 0u}}},
        {"changes", changes},
        {"survival",
         {{"rename", rename_},
          {"fileMode", fileMode_},
          {"delete", delete_},
          {"wrongType", wrongType_}}},
        {"problems", problems}};

    const std::string tmp = out_ + ".tmp";
    {
        std::ofstream file(std::filesystem::u8path(tmp), std::ios::binary | std::ios::trunc);
        file << report.dump(2);
    }
    std::error_code ec;
    std::filesystem::rename(std::filesystem::u8path(tmp), std::filesystem::u8path(out_), ec);
    if (ec)
        obs_log(LOG_ERROR, "selftest could not write %s: %s", out_.c_str(), ec.message().c_str());
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

} // namespace

namespace selftest {

void maybeStart(ClockDriver* driver, FieldSinks* sinks) {
    if (env("OBS_SCOREBOARD_SELFTEST") != "1") return;
    if (g_started.exchange(true)) return;
    g_run = new Run(driver, sinks);
    // FINISHED_LOADING fires while OBSBasic is still building docks (a lesson from
    // obs-multireplay): wait on a timer, never by sleeping on the UI thread.
    QTimer::singleShot(2000, g_run, []() {
        if (g_run) g_run->begin();
    });
}

void stop() {
    if (!g_run) return;
    if (g_run->stopCapture()) obs_log(LOG_INFO, "selftest: capture stopped on exit");
    delete g_run.data();
}

} // namespace selftest
