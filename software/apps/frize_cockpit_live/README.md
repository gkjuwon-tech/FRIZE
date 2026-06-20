# frize_cockpit_live — 콕핏 라이브 통신/페어링 데몬

목업·렌더와 분리된 **프로덕션 콕핏 백엔드**. 실제 MQTT 위에서 돈다.

- 디바이스 발견: `frize/device/+/heartbeat` 구독 → 로스터 구성
- **페어링 핸드셰이크**: 콕핏이 `frize/pair/<id>` 로 `PairRequest` 발행 →
  디바이스(`MessageBus::enable_pairing`)가 `frize/pair/<id>/grant` 로
  `PairGrant`(능력·펌웨어·세션) 를 **retained** 응답
- 라이브 텔레메트리: `frize/device/+/telemetry` 에서 위치/배터리/가스 추출
- 명령은 **페어링된 디바이스에만** 발행(안전 인터록)

핵심 로직은 헤더 온리 `frize/cockpit/link.hpp`(`CockpitLink`)에 있고,
이 데몬과 ImGui 콕핏(`frize_cockpit_imgui`)이 **동일 코드를 공유**한다(SSOT).

## 실행

```bash
# 브로커
mosquitto -d -p 1883
# 디바이스(시뮬)
FRIZE_DEVICE_ID=scout-01 ./frize_scout &
FRIZE_DEVICE_ID=visor-01 ./frize_visor &
FRIZE_DEVICE_ID=vent-01  ./frize_apparatus &
# 콕핏: 발견된 고글/드론/벤트 자동 페어링 + 라이브 로스터
./frize_cockpit_live --pair-all
# ImGui 콕핏 페어링 패널용 로스터 스냅샷 1회 저장
./frize_cockpit_live --pair-all --snapshot /tmp/cockpit_roster.json
```

## 옵션
`--host` `--port` `--console` `--pair-all` `--iters N`(0=무한) `--snapshot <json>`

`roster.sample.json` 은 실제 MQTT 페어링 후 저장된 라이브 로스터 예시다.
