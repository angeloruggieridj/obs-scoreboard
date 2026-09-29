#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Runs the in-OBS fps selftest with the system OBS under xvfb (Linux).
#
#   run-selftest.sh --fps N [--seconds 20] --plugin <.so> --locale <dir> [--artifacts <dir>]
#
# Exit 0 = PASS, 1 = FAIL (report says FAIL or the shutdown was not clean),
# 2 = no report (OBS did not start, timeout, crash) or setup problem.
set -euo pipefail

fps=""
seconds=20
plugin=""
locale=""
artifacts=""

usage() {
    echo "usage: $0 --fps N [--seconds S] --plugin <obs-scoreboard.so> --locale <dir> [--artifacts <dir>]" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --fps) fps="${2:-}"; shift 2 ;;
        --seconds) seconds="${2:-}"; shift 2 ;;
        --plugin) plugin="${2:-}"; shift 2 ;;
        --locale) locale="${2:-}"; shift 2 ;;
        --artifacts) artifacts="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done
[ -n "$fps" ] && [ -n "$plugin" ] && [ -n "$locale" ] || usage
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
[ -f "$plugin" ] || { echo "missing plugin: $plugin" >&2; exit 2; }
[ -d "$locale" ] || { echo "missing locale dir: $locale" >&2; exit 2; }
for tool in obs xvfb-run python3; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 2; }
done

plugin="$(realpath "$plugin")"
locale="$(realpath "$locale")"
[ -n "$artifacts" ] || artifacts="${TMPDIR:-/tmp}/obs-scoreboard-selftest/artifacts"
mkdir -p "$artifacts"
artifacts="$(realpath "$artifacts")"
report="$artifacts/selftest-${fps}fps.json"
logcopy="$artifacts/obs-${fps}fps.log"
rm -f "$report" "$report.tmp" "$logcopy"

# OBS resolves its config directory through XDG_CONFIG_HOME
# (libobs/util/platform-nix.c, os_get_config_path), so this isolates it.
XDG_CONFIG_HOME="$(mktemp -d)"
export XDG_CONFIG_HOME
start_marker=""
# Removes the throw-away config dir (the log was copied to the artifacts dir
# before this runs) and the marker file, whatever the exit path.
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
printf '[General]\nName=SBSelftest\n\n[Video]\nBaseCX=1280\nBaseCY=720\nOutputCX=1280\nOutputCY=720\nFPSType=1\nFPSInt=%s\n' \
    "$fps" > "$cfg/basic/profiles/SBSelftest/basic.ini"

dest="$cfg/plugins/obs-scoreboard"
mkdir -p "$dest/bin/64bit" "$dest/data"
cp "$plugin" "$dest/bin/64bit/obs-scoreboard.so"
cp -r "$locale" "$dest/data/locale"

export OBS_SCOREBOARD_SELFTEST=1
export OBS_SCOREBOARD_SELFTEST_FPS="$fps"
export OBS_SCOREBOARD_SELFTEST_SECS="$seconds"
export OBS_SCOREBOARD_SELFTEST_OUT="$report"

start_marker="$(mktemp)"
# No --minimize-to-tray: Xvfb has no notification area.
xvfb-run -a -s "-screen 0 1280x720x24" obs --multi --profile SBSelftest &
xvfb_pid=$!

# $! is xvfb-run, not OBS: find the real obs process, looking only among the
# descendants of xvfb-run so that an OBS the developer runs is never touched.
obs_pid=""
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
find_obs() { find_obs_under "$xvfb_pid" || true; }

deadline=$((SECONDS + seconds + 90))
while [ "$SECONDS" -lt "$deadline" ]; do
    [ -n "$obs_pid" ] || obs_pid="$(find_obs)"
    [ -f "$report" ] && break
    kill -0 "$xvfb_pid" 2>/dev/null || break
    sleep 0.5
done
[ -n "$obs_pid" ] || obs_pid="$(find_obs)"

# SIGINT is the only signal OBS turns into a clean shutdown (obs_module_unload
# runs); the capture is deliberately still running here.
closed=0
if [ -n "$obs_pid" ] && kill -0 "$obs_pid" 2>/dev/null; then
    kill -INT "$obs_pid" 2>/dev/null || true
    for _ in $(seq 1 40); do
        kill -0 "$obs_pid" 2>/dev/null || { closed=1; break; }
        sleep 0.5
    done
    if [ "$closed" -eq 0 ]; then kill -KILL "$obs_pid" 2>/dev/null || true; fi
fi
# Only now let xvfb-run finish.
for _ in $(seq 1 20); do
    kill -0 "$xvfb_pid" 2>/dev/null || break
    sleep 0.5
done
kill -KILL "$xvfb_pid" 2>/dev/null || true
wait "$xvfb_pid" 2>/dev/null || true

log="$(find "$cfg/logs" -type f -name '*.txt' 2>/dev/null | sort | tail -n 1 || true)"
text=""
if [ -n "$log" ]; then
    cp "$log" "$logcopy"
    text="$(cat "$log")"
fi
stopped=0
unloaded=0
case "$text" in *"[obs-scoreboard] selftest: capture stopped on exit"*) stopped=1 ;; esac
case "$text" in *"[obs-scoreboard] unloaded"*) unloaded=1 ;; esac
crashes=0
if [ -d "$cfg/crashes" ]; then
    crashes="$(find "$cfg/crashes" -type f -newer "$start_marker" | wc -l)"
fi

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
echo "  clean shutdown: closed=$closed captureStopped=$stopped unloaded=$unloaded crashes=$crashes"
if [ "$pass" = "1" ] && [ "$closed" -eq 1 ] && [ "$stopped" -eq 1 ] && [ "$unloaded" -eq 1 ] && [ "$crashes" -eq 0 ]; then
    echo "SELFTEST PASS"
    exit 0
fi
echo "SELFTEST FAIL"
exit 1
