#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Runs the in-OBS fps selftest with the system OBS under xvfb (Linux).
#
#   run-selftest.sh --fps N [--seconds 20] --plugin <.so> --locale <dir> --fonts <dir>
#                       [--artifacts <dir>] [--canvas WxH] [--reopen]
#                       (default canvas 1280x720; CI uses a smaller one on software rendering)
#
# With --reopen, after a first run that passed, OBS is started a second time on
# the SAME throw-away config with OBS_SCOREBOARD_SELFTEST_PASS=reopen: it
# commands nothing, it only checks that the source links came back and that the
# saved match was rotated without being rewritten. The script then checks that
# previous-match.json is byte-identical to the first run's last-match.json and
# that last-match.json no longer exists.
#
# Exit 0 = PASS, 1 = FAIL (report says FAIL or the shutdown was not clean; with
# --reopen, both reports say pass and both file checks hold),
# 2 = no report (OBS did not start, timeout, crash) or setup problem.
set -euo pipefail

fps=""
seconds=20
plugin=""
locale=""
fonts=""
artifacts=""
canvas="1280x720"
reopen=0

usage() {
    echo "usage: $0 --fps N [--seconds S] --plugin <obs-scoreboard.so> --locale <dir> --fonts <dir> [--artifacts <dir>] [--canvas WxH] [--reopen]" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --fps) fps="${2:-}"; shift 2 ;;
        --seconds) seconds="${2:-}"; shift 2 ;;
        --plugin) plugin="${2:-}"; shift 2 ;;
        --locale) locale="${2:-}"; shift 2 ;;
        --fonts) fonts="${2:-}"; shift 2 ;;
        --artifacts) artifacts="${2:-}"; shift 2 ;;
        --canvas) canvas="${2:-}"; shift 2 ;;
        --reopen) reopen=1; shift ;;
        *) usage ;;
    esac
done
[ -n "$fps" ] && [ -n "$plugin" ] && [ -n "$locale" ] && [ -n "$fonts" ] || usage
case "$fps" in
    25|30|50|60) ;;
    *) echo "invalid --fps '$fps' (expected 25, 30, 50 or 60)" >&2; usage ;;
esac
case "$seconds" in
    ''|*[!0-9]*) echo "invalid --seconds '$seconds' (expected an integer 5-120)" >&2; usage ;;
esac
if [ "$seconds" -lt 5 ] || [ "$seconds" -gt 120 ]; then
    echo "invalid --seconds '$seconds' (expected an integer 5-120)" >&2
    usage
fi
case "$canvas" in
    [0-9]*x[0-9]*) ;;
    *) echo "invalid --canvas '$canvas' (expected WxH, e.g. 1280x720)" >&2; usage ;;
esac
canvas_w="${canvas%x*}"
canvas_h="${canvas#*x}"
[ -f "$plugin" ] || { echo "missing plugin: $plugin" >&2; exit 2; }
[ -d "$locale" ] || { echo "missing locale dir: $locale" >&2; exit 2; }
[ -d "$fonts" ] || { echo "missing fonts dir: $fonts" >&2; exit 2; }
for tool in obs xvfb-run python3 sha256sum; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 2; }
done

plugin="$(realpath "$plugin")"
locale="$(realpath "$locale")"
fonts="$(realpath "$fonts")"
[ -n "$artifacts" ] || artifacts="${TMPDIR:-/tmp}/obs-scoreboard-selftest/artifacts"
mkdir -p "$artifacts"
artifacts="$(realpath "$artifacts")"
report="$artifacts/selftest-${fps}fps.json"
logcopy="$artifacts/obs-${fps}fps.log"

# OBS resolves its config directory through XDG_CONFIG_HOME
# (libobs/util/platform-nix.c, os_get_config_path), so this isolates it.
XDG_CONFIG_HOME="$(mktemp -d)"
export XDG_CONFIG_HOME
start_marker=""
# Removes the throw-away config dir (the logs were copied to the artifacts dir
# before this runs) and the marker file, whatever the exit path. The trap fires
# at EXIT, after both the first run and (with --reopen) the second one, so the
# config the restart reuses is still on disk.
# shellcheck disable=SC2329  # invoked through the EXIT trap
cleanup() {
    rm -rf "$XDG_CONFIG_HOME"
    [ -z "$start_marker" ] || rm -f "$start_marker"
}
trap cleanup EXIT
cfg="$XDG_CONFIG_HOME/obs-studio"
mkdir -p "$cfg/basic/profiles/SBSelftest"
printf '[General]\nFirstRun=true\n' > "$cfg/user.ini"
printf '[General]\nEnableAutoUpdates=false\n' > "$cfg/global.ini"
printf '[General]\nName=SBSelftest\n\n[Video]\nBaseCX=%s\nBaseCY=%s\nOutputCX=%s\nOutputCY=%s\nFPSType=1\nFPSInt=%s\n' \
    "$canvas_w" "$canvas_h" "$canvas_w" "$canvas_h" "$fps" > "$cfg/basic/profiles/SBSelftest/basic.ini"

dest="$cfg/plugins/obs-scoreboard"
mkdir -p "$dest/bin/64bit" "$dest/data"
cp "$plugin" "$dest/bin/64bit/obs-scoreboard.so"
cp -r "$locale" "$dest/data/locale"
cp -r "$fonts" "$dest/data/fonts"

# $! is xvfb-run, not OBS: find the real obs process, looking only among the
# descendants of xvfb-run so that an OBS the developer runs is never touched.
find_obs_under() {
    local child
    for child in $(pgrep -P "$1" || true); do
        if [ "$(ps -o comm= -p "$child" 2>/dev/null | tr -d ' ')" = "obs" ]; then
            echo "$child"
            return 0
        fi
        find_obs_under "$child" && return 0
    done
    return 1
}

# One OBS start: waits for the report, then SIGINTs OBS (the only signal it
# turns into a clean shutdown) with the capture still running. Sets the globals
# obs_closed, obs_stopped, obs_unloaded and obs_crashes. $1 "first" starts the
# selftest, "reopen" only looks at what the first start left behind.
run_pass() {
    local pass="$1" report_file="$2" log_copy="$3"
    rm -f "$report_file" "$report_file.tmp" "$log_copy"

    export OBS_SCOREBOARD_SELFTEST=1
    export OBS_SCOREBOARD_SELFTEST_FPS="$fps"
    export OBS_SCOREBOARD_SELFTEST_SECS="$seconds"
    export OBS_SCOREBOARD_SELFTEST_OUT="$report_file"
    # The live vendor check runs through obs-websocket's call_request: require it, so an OBS built
    # without the bundle fails the selftest instead of silently passing (finding 12). The system OBS
    # layout is not ours to inspect, so the verdict (not a file probe) enforces its presence.
    export OBS_SCOREBOARD_SELFTEST_REQUIRE_VENDOR=1
    if [ "$pass" = reopen ]; then
        export OBS_SCOREBOARD_SELFTEST_PASS=reopen
    else
        unset OBS_SCOREBOARD_SELFTEST_PASS
    fi

    [ -z "$start_marker" ] || rm -f "$start_marker"
    start_marker="$(mktemp)"
    # No --minimize-to-tray: Xvfb has no notification area.
    xvfb-run -a -s "-screen 0 1280x720x24" obs --multi --profile SBSelftest &
    local xvfb_pid=$!
    local obs_pid=""
    find_obs() { find_obs_under "$xvfb_pid" || true; }

    local deadline=$((SECONDS + seconds + 90))
    while [ "$SECONDS" -lt "$deadline" ]; do
        [ -n "$obs_pid" ] || obs_pid="$(find_obs)"
        [ -f "$report_file" ] && break
        kill -0 "$xvfb_pid" 2>/dev/null || break
        sleep 0.5
    done
    [ -n "$obs_pid" ] || obs_pid="$(find_obs)"

    # SIGINT is the only signal OBS turns into a clean shutdown (obs_module_unload
    # runs); the capture is deliberately still running here.
    obs_closed=0
    if [ -n "$obs_pid" ] && kill -0 "$obs_pid" 2>/dev/null; then
        kill -INT "$obs_pid" 2>/dev/null || true
        for _ in $(seq 1 40); do
            kill -0 "$obs_pid" 2>/dev/null || { obs_closed=1; break; }
            sleep 0.5
        done
        if [ "$obs_closed" -eq 0 ]; then kill -KILL "$obs_pid" 2>/dev/null || true; fi
    fi
    # Only now let xvfb-run finish.
    for _ in $(seq 1 20); do
        kill -0 "$xvfb_pid" 2>/dev/null || break
        sleep 0.5
    done
    kill -KILL "$xvfb_pid" 2>/dev/null || true
    wait "$xvfb_pid" 2>/dev/null || true

    local log text
    log="$(find "$cfg/logs" -type f -name '*.txt' 2>/dev/null | sort | tail -n 1 || true)"
    text=""
    if [ -n "$log" ]; then
        cp "$log" "$log_copy"
        text="$(cat "$log")"
    fi
    obs_stopped=0
    obs_unloaded=0
    case "$text" in *"[obs-scoreboard] selftest: capture stopped on exit"*) obs_stopped=1 ;; esac
    case "$text" in *"[obs-scoreboard] unloaded"*) obs_unloaded=1 ;; esac
    obs_crashes=0
    if [ -d "$cfg/crashes" ]; then
        obs_crashes="$(find "$cfg/crashes" -type f -newer "$start_marker" | wc -l)"
    fi
}

run_pass first "$report" "$logcopy"

if [ ! -f "$report" ]; then
    echo "selftest ${fps} fps: NO REPORT (log: $logcopy)"
    exit 2
fi

pass="$(python3 - "$report" <<'PY'
import json, sys
r = json.load(open(sys.argv[1]))
print("selftest %s fps: report %s" % (r["fps"]["requested"], sys.argv[1]), file=sys.stderr)
for k, v in r["criteria"].items():
    print("  criterion %-10s %s" % (k, v), file=sys.stderr)
for k, v in r["survival"].items():
    print("  survival  %-10s %s" % (k, v), file=sys.stderr)
print("  maxLatencyMs %.3f (budgetMs %.3f)" % (r["maxLatencyMs"], r["budgetMs"]), file=sys.stderr)
f = r["frames"]
print("  frames captured=%s badTimestamps=%s lagged=%s skipped=%s"
      % (f["captured"], f["badTimestamps"], f["lagged"], f["skipped"]), file=sys.stderr)
for p in r["problems"]:
    print("  problem: %s" % p, file=sys.stderr)
print("1" if r["pass"] else "0")
PY
)"
echo "  clean shutdown: closed=$obs_closed captureStopped=$obs_stopped unloaded=$obs_unloaded crashes=$obs_crashes"
first_ok=0
if [ "$pass" = "1" ] && [ "$obs_closed" -eq 1 ] && [ "$obs_stopped" -eq 1 ] && [ "$obs_unloaded" -eq 1 ] && [ "$obs_crashes" -eq 0 ]; then
    first_ok=1
fi

if [ "$reopen" -eq 0 ]; then
    if [ "$first_ok" -eq 1 ]; then
        echo "SELFTEST PASS"
        exit 0
    fi
    echo "SELFTEST FAIL"
    exit 1
fi

# The restart check only makes sense after a first run that passed.
if [ "$first_ok" -ne 1 ]; then
    echo "SELFTEST FAIL (first run failed; not restarting)"
    exit 1
fi

# The plugin rotates last-match.json to previous-match.json at the next module
# start, before the saver can rewrite it. The first run must have written it.
plugin_config="$cfg/plugin_config/obs-scoreboard"
last="$plugin_config/last-match.json"
previous="$plugin_config/previous-match.json"
if [ ! -f "$last" ]; then
    echo "selftest ${fps} fps reopen: FAIL (the first run wrote no last-match.json: $last)"
    echo "SELFTEST FAIL"
    exit 1
fi
last_hash="$(sha256sum "$last" | cut -d' ' -f1)"

reopen_report="$artifacts/selftest-${fps}fps-reopen.json"
reopen_log="$artifacts/obs-${fps}fps-reopen.log"
run_pass reopen "$reopen_report" "$reopen_log"

if [ ! -f "$reopen_report" ]; then
    echo "selftest ${fps} fps reopen: NO REPORT (log: $reopen_log)"
    exit 2
fi

reopen_pass="$(python3 - "$reopen_report" <<'PY'
import json, sys
r = json.load(open(sys.argv[1]))
print("selftest reopen: report %s" % sys.argv[1], file=sys.stderr)
ro = r.get("reopen", {})
for k in ("bindingRestored", "receiving", "settingsFile", "previousMatch", "hotkeyPersisted"):
    print("  reopen    %-15s %s" % (k, ro.get(k)), file=sys.stderr)
for p in ro.get("problems", []):
    print("  problem: %s" % p, file=sys.stderr)
print("1" if r.get("pass") else "0")
PY
)"

same_hash=0
if [ -f "$previous" ]; then
    previous_hash="$(sha256sum "$previous" | cut -d' ' -f1)"
    if [ "$previous_hash" = "$last_hash" ]; then
        same_hash=1
    fi
fi
last_gone=0
if [ ! -f "$last" ]; then
    last_gone=1
fi
echo "  previous-match.json same SHA-256 as last-match.json: $same_hash"
echo "  last-match.json removed: $last_gone"
echo "  clean shutdown: closed=$obs_closed unloaded=$obs_unloaded crashes=$obs_crashes"
reopen_ok=0
if [ "$obs_closed" -eq 1 ] && [ "$obs_unloaded" -eq 1 ] && [ "$obs_crashes" -eq 0 ]; then
    reopen_ok=1
fi
if [ "$reopen_pass" = "1" ] && [ "$same_hash" -eq 1 ] && [ "$last_gone" -eq 1 ] && [ "$reopen_ok" -eq 1 ]; then
    echo "SELFTEST PASS"
    exit 0
fi
echo "SELFTEST FAIL"
exit 1
