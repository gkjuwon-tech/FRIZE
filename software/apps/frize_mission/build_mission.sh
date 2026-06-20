#!/usr/bin/env bash
# frize_mission 파이프라인 빌드(생성기 + 트윈 렌더러). stb 헤더는 /tmp/imgui 재사용.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
IMGUI="${IMGUI_DIR:-/tmp/imgui}"
OUT="${1:-/tmp}"
g++ -O2 -fopenmp -std=c++17 \
    -I"$HERE/../../libs/frize_recon/include" -I"$HERE/../frize_recon_sim/include" \
    "$HERE/main.cpp" -o "$OUT/frize_mission"
g++ -O2 -std=c++17 -I"$IMGUI" "$HERE/render.cpp" -o "$OUT/frize_mrender" \
    -lglfw -lGL -ldl -lpthread
echo "[build] $OUT/frize_mission  +  $OUT/frize_mrender"
