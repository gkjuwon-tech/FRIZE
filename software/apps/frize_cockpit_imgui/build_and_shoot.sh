#!/usr/bin/env bash
# FRIZE 콕핏(Dear ImGui) 빌드 + 헤드리스 스크린샷.
#   IMGUI_DIR=/tmp/imgui ./build_and_shoot.sh [out.png] [twin.png] [pov.jpg]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
IMGUI="${IMGUI_DIR:-/tmp/imgui}"
OUT="${1:-$HERE/frize_cockpit_imgui.png}"
TWIN="${2:-$HERE/../../../hardware/renders_studio/twin_cockpit.png}"
POV="${3:-$HERE/../frize_visor/assets/goggle_ar_view.png}"
ROSTER="${4:-/tmp/cockpit_roster.json}"
BUILD=/tmp/fci_build; mkdir -p "$BUILD"

g++ -O2 -std=c++17 \
  -I"$IMGUI" -I"$IMGUI/backends" -I"$HERE" -I"$HERE/../../third_party/nlohmann" \
  "$HERE/main.cpp" \
  "$IMGUI/imgui.cpp" "$IMGUI/imgui_draw.cpp" "$IMGUI/imgui_tables.cpp" "$IMGUI/imgui_widgets.cpp" \
  "$IMGUI/backends/imgui_impl_glfw.cpp" "$IMGUI/backends/imgui_impl_opengl3.cpp" \
  -o "$BUILD/frize_cockpit_imgui" \
  -lglfw -lGL -ldl -lpthread

echo "[build] ok → $BUILD/frize_cockpit_imgui"
xvfb-run -a -s "-screen 0 1920x1080x24" "$BUILD/frize_cockpit_imgui" "$OUT" "$IMGUI" "$TWIN" "$POV" "$HERE/control_map.json" "$ROSTER"
echo "[shoot] → $OUT"
