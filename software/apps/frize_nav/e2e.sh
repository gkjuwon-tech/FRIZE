#!/usr/bin/env bash
# 진짜 MQTT e2e: nav 서비스 + 콕핏 유도 + 대원 보행 → ar_cue(Route) 스트림 캡처
set -u
NAV=/tmp/fb/apps/frize_nav/frize_nav
pgrep -x mosquitto >/dev/null || mosquitto -d -p 1883
sleep 0.5
pkill -f frize_nav 2>/dev/null; sleep 0.3
FRIZE_NAV_OBSTACLES=/tmp/obs.json FRIZE_NAV_RES=0.12 FRIZE_NAV_HZ=5 nohup "$NAV" >/tmp/nav.log 2>&1 &
sleep 0.8
# ar_cue 구독(대원 visor-9)
mosquitto_sub -t 'frize/command/visor-9/ar' > /tmp/ar_capture.log 2>&1 &
SUB=$!
sleep 0.3
# 콕핏: 유도 요청(목표=요구조자1 22.8,2.2)
mosquitto_pub -t 'frize/guide/visor-9' -m '{"type":"guide_request","source":"cockpit-1","payload":{"wearer_id":"visor-9","target":{"x":22.8,"y":2.2,"z":0},"active":true,"label":"요구조자","color":"#36c0ff"}}'
# 대원 보행: 현관(0,6) → 복도 → 남동 끝방. 위치를 한 스텝씩 발행.
xs="0 2 4 6 8 10 12 14 16 18 20 22 22.6 22.8"
ys="6 6 6 6 6 6 6 6 6 5.8 5.6 4.5 3.2 2.4"
xa=($xs); ya=($ys)
for i in "${!xa[@]}"; do
  mosquitto_pub -t "frize/device/visor-9/telemetry" -m "{\"type\":\"telemetry\",\"source\":\"visor-9\",\"payload\":{\"device_id\":\"visor-9\",\"pose\":{\"position\":{\"x\":${xa[$i]},\"y\":${ya[$i]},\"z\":1.5}}}}"
  sleep 0.45
done
sleep 0.6
kill $SUB 2>/dev/null
echo "=== nav 서비스 로그 ==="; grep -E "유도|도착|격자" /tmp/nav.log | head
echo; echo "=== 캡처된 ar_cue(Route) 개수: $(grep -c route /tmp/ar_capture.log) ==="
echo "처음/중간/끝 라우트(경유점 수 + 끝점):"
python3 - <<'PY'
import json
lines=[l for l in open('/tmp/ar_capture.log') if l.strip()]
def info(l):
    e=json.loads(l); p=e.get('payload',{}); r=p.get('route',[]); k=p.get('kind'); t=p.get('text','')
    end=r[-1] if r else None
    return f"kind={k} text={t} wp={len(r)} end={'(%.1f,%.1f)'%(end['x'],end['y']) if end else '-'}"
for idx in [0, len(lines)//2, len(lines)-1]:
    if 0<=idx<len(lines): print(f"  [{idx}] {info(lines[idx])}")
print(f"  총 {len(lines)} 메시지")
PY
