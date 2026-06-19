#!/usr/bin/env bash
# FRIZE SCAD 렌더 배치 (백그라운드 실행용)
OSC="/c/Program Files/OpenSCAD/openscad.exe"
cd "$(dirname "$0")" || exit 1
mkdir -p renders
SZ="1400,1050"
log() { echo "[$(date +%H:%M:%S)] $*"; }

render() { # file out angle
  log "render $2 ($3)"
  "$OSC" -o "renders/$2" "$1" --viewall --autocenter --camera="0,0,0,$3" --imgsize=$SZ --projection=p 2>>renders/render.log
}

log "=== VISOR ==="
render scad/frize_visor.scad visor_hero.png      "62,0,28,300"
render scad/frize_visor.scad visor_front.png     "90,0,0,300"
render scad/frize_visor.scad visor_side.png      "90,0,90,300"
render scad/frize_visor.scad visor_top.png       "0,0,0,300"
render scad/frize_visor.scad visor_threequarter.png "58,0,-35,300"

log "=== SCOUT ==="
render scad/frize_scout_frame.scad scout_hero.png   "60,0,30,400"
render scad/frize_scout_frame.scad scout_top.png    "0,0,0,400"
render scad/frize_scout_frame.scad scout_side.png   "88,0,0,400"
render scad/frize_scout_frame.scad scout_front.png  "90,0,90,400"

log "=== DONE ==="
