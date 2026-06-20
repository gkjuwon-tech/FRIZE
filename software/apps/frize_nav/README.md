# frize_nav ― AR 내비게이션 서비스 (프로덕션)

목업 화살표를 손으로 그리는 게 아니라, **트윈에서 목표점·대원을 찍으면 그 대원
실시간 위치마다 경로를 재계산해 고글 AR 에 계속 갱신되는 화살표로 목표까지
안내**하는 실제 코드.

```
콕핏(CockpitLink.guide)  --GuideRequest-->  frize_nav 서비스
                                              │  점유격자(트윈 MapPatch/정적)
대원 telemetry(위치) ──────────────────────▶  │  A* + 가시선 단순화
                                              ▼
                              ArCue(Route) ── 고글(frize_visor) ── 바닥 셰브론
```

## 구성 (libs/frize_common/include/frize/nav/)
- `grid.hpp`   `NavGrid` ― 층 2D 점유격자. `from_mappatch`(라이브 트윈) /
  `from_rects`(정적), `inflate`(대원 반경), `line_of_sight`.
- `astar.hpp`  8-이웃 A*(옥타일) + `simplify_los`(스트링풀링) → 최소 경유점.
- `ar_project.hpp` 경로 → 고글 카메라 핀홀 투영(`project_route`) → 바닥 셰브론
  스크린좌표. `turn_hint`(좌/직/우). 대원 자세가 바뀌면 투영이 자동 갱신.
- `guide.hpp`  `Guidance` ― 격자+계획+`route_cue`(ArCue Route 빌드)+도착판정.

프로토콜: `GuideRequest`(schemas) + `MessageType::GuideRequest` +
`Topic::guide(id)`. 콕핏은 `CockpitLink::guide(wearer,target)` /
`stop_guide(wearer)`. 서비스는 `frize/command/<id>/ar` 로 `ArCue(Route)` 발행.

## 서비스 실행
```bash
# 점유격자: 라이브 트윈(frize/map/patch) 자동 수신, 또는 정적 시드:
FRIZE_NAV_OBSTACLES=obstacles.json FRIZE_NAV_RES=0.12 FRIZE_NAV_HZ=5 ./frize_nav
# env: FRIZE_MQTT_HOST/PORT, FRIZE_NAV_INFLATE, FRIZE_NAV_ZLO/ZHI(층 슬라이스)
```

## 검증
- `frize_nav_test` (ctest `nav_pathfinding`): 벽 회피·문 통과·벽 비관통 경로.
- `frize_nav_demo`: 대원이 목표로 걸어갈 때 경로 길이/회전힌트/투영이 매 스텝
  갱신되는지 출력(실시간 재유도).
- `e2e.sh`: mosquitto + frize_nav + (mosquitto_pub 로) 유도요청·대원 보행 →
  `frize/command/visor-9/ar` 로 흐르는 `ArCue(Route)` 스트림 캡처(도착 시 종료).
  `frize_nav_dump_obstacles > obstacles.json` 로 sim 건물 격자 시드 생성.
