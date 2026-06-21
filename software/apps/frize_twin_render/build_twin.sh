#!/usr/bin/env bash
# 디지털 트윈 실시간 컷어웨이 렌더(오프스크린 GL). 블렌더 대체, 초 단위.
#   ./build_twin.sh [twin.ply] [out.png] [cutZ] [W] [H]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
IMGUI="${IMGUI_DIR:-/tmp/imgui}"   # stb_image_write.h 재사용
BUILD=/tmp/twin_build; mkdir -p "$BUILD"
g++ -O2 -std=c++17 -I"$IMGUI" "$HERE/main.cpp" -o "$BUILD/frize_twin_render" \
    -lglfw -lGL -ldl -lpthread
echo "[build] ok → $BUILD/frize_twin_render"
PLY="${1:-/tmp/twinout/twin.ply}"
OUT="${2:-/tmp/twin_cockpit.png}"
CUTZ="${3:-3.0}"
W="${4:-1920}"; H="${5:-1080}"
xvfb-run -a -s "-screen 0 ${W}x${H}x24" "$BUILD/frize_twin_render" "$PLY" "$OUT" "$CUTZ" "$W" "$H"
echo "[twin] → $OUT"
