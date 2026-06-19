#!/usr/bin/env bash
# FRIZE 시뮬 통합 실행 ― 하드웨어 없이 코어+디바이스 end-to-end.
# 전제: build/ 에 빌드 완료(아래 README 참고), mosquitto 1883 동작 중.
set -e
BUILD="${BUILD:-build}"
export FRIZE_MQTT_HOST="${FRIZE_MQTT_HOST:-localhost}"
export FRIZE_SIM_FRAME="${FRIZE_SIM_FRAME:-apps/frize_cockpit/assets/fireground.jpg}"

echo "▶ core"
"$BUILD/apps/frize_core/frize_core" &  CORE=$!
sleep 1
echo "▶ scout-01 (드론, 시뮬)"
FRIZE_DEVICE_ID=scout-01 "$BUILD/apps/frize_scout/frize_scout" &  SCOUT=$!
echo "▶ visor-01 (고글, 시뮬)"
FRIZE_DEVICE_ID=visor-01 FRIZE_CALLSIGN=ALPHA-1 "$BUILD/apps/frize_visor/frize_visor" &  VISOR=$!
echo "▶ visor-02 (고글, 시뮬)"
FRIZE_DEVICE_ID=visor-02 FRIZE_CALLSIGN=BRAVO-2 "$BUILD/apps/frize_visor/frize_visor" &  VISOR2=$!
echo "▶ vent-01 (IoT 배연/진입 포트, 시뮬)"
FRIZE_DEVICE_ID=vent-01 "$BUILD/apps/frize_apparatus/frize_apparatus" &  VENT=$!
echo "▶ mapping"
"$BUILD/apps/frize_mapping/frize_mapping" &  MAP=$!

trap "kill $CORE $SCOUT $VISOR $VISOR2 $VENT $MAP 2>/dev/null" EXIT
echo "── 실행 중. 콕핏: build/apps/frize_cockpit/frize_cockpit (별도 실행). Ctrl-C 종료 ──"
wait
