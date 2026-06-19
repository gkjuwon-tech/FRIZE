# 🔥 FRIZE Software ― Firefighting Command OS

소방 현장을 하나의 운영체제로 묶는 C++ 분산 시스템. 콕핏 '딸깍' → 안전 인터록 → 디바이스 동작,
디바이스 시야/센서 → VLM 판단 → 콕핏 역송출. **판단은 VLM 전담(룰 디텍터 없음).**

> 언어 선택: 생명 직결 실시간 경로(비행/인지/명령)는 전부 **C++17**. 콕핏 UI는 **C++/Qt6**.
> Python은 쓰지 않는다(GC 멈춤/동적타입 리스크).

---

## 아키텍처

```
                 ┌──────────────────────────────┐
                 │   CONSOLE-1 (전용 지휘 콘솔)    │  러기드 기기
                 │  frize_cockpit(Qt6) 키오스크    │  + 물리 컨트롤덱/E-STOP
                 │  + frize_console_estop(안전경로) │
                 └───────────────┬──────────────┘
                          WebSocket (ws/cockpit) + MQTT(E-STOP)
                 ┌───────────────┴──────────────┐
                 │        frize_core (C++)        │  커널
                 │  레지스트리·월드모델·라우터      │
                 │  안전 인터록·VLM 판단(Claude)   │
                 └───────────────┬──────────────┘
                            MQTT (frize/...)            ← frize_common 프로토콜
        ┌───────────────┬───────┴────────┬───────────────────┐
   frize_scout      frize_visor       frize_visor        frize_mapping
   (드론/MAVSDK)    (고글/센서·AR)    (고글 N대)         (3D 복셀맵/SLAM)
```

- **frize_common** (lib): 프로토콜(토픽/봉투), 스키마, MQTT 버스, 기하(ENU). 단일 진실원천.
- **frize_core**: FastAPI 아님 ― Crow 기반 HTTP/WS. 디바이스 레지스트리(프로세스 테이블),
  월드모델(공유 메모리), 명령 라우터(시스콜 디스패치), 안전 인터록(보호 모드),
  VLM 판단(Anthropic Claude vision, tool-use 구조화 출력).
- **frize_scout**: 드론. `FlightController` HAL(sim / MAVSDK), `Perception`(OpenCV/가스).
- **frize_visor**: 고글. 가스·생체·자세 센서, 열화상 캡처, AR 명령 출력. 로컬 안전(즉시 대피).
- **frize_mapping**: 점유격자+열 융합 복셀맵 → MapPatch. 실제 LiDAR는 PCL 경로.
- **CONSOLE-1**: 콕핏 전용 러기드 콘솔(노트북 대체). `frize_cockpit`을 EGLFS 키오스크로 부팅 구동,
  컨트롤덱/E-STOP을 evdev로 매핑. **E-STOP은 `frize_console_estop`이 UI와 독립된 안전경로**로
  전 대원 후퇴/전 드론 복귀를 버스에 직접 브로드캐스트. 배치: `apps/frize_cockpit/deploy/`(systemd+kiosk),
  하드웨어 BOM/SCAD: `hardware/`(`frize_console.scad`, BOM `CONSOLE-1` 시트).

모든 디바이스 앱은 **HAL + 시뮬레이터**를 내장 → 하드웨어 없이 end-to-end 검증, 하드웨어 꽂으면 그대로 작동.

---

## 빌드 (Linux / Jetson, vcpkg)

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# 코어 + 공용 + 테스트 (가벼움)
cmake -S software -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DFRIZE_BUILD_CORE=ON -DFRIZE_BUILD_SCOUT=ON -DFRIZE_BUILD_VISOR=ON -DFRIZE_BUILD_MAPPING=ON
cmake --build build -j

# 콕핏(Qt6 필요)
cmake -S software/apps/frize_cockpit -B build-cockpit \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build-cockpit -j
```

vcpkg 의존성은 `software/vcpkg.json` 참고(nlohmann-json, spdlog, paho-mqttpp3, openssl, crow, cpp-httplib;
feature: scout=mavsdk+opencv4, mapping=pcl+eigen, visor=opencv4).

---

## 실행 (시뮬 end-to-end)

```bash
# 1) 버스
docker compose up -d mosquitto      # 또는 mosquitto -c docker/mosquitto.conf

# 2) VLM 판단 켜기(선택)
export ANTHROPIC_API_KEY=sk-ant-...

# 3) 코어 + 디바이스(시뮬)
cp .env.example .env
BUILD=build scripts/run_sim.sh

# 4) 콕핏
FRIZE_CORE_WS=ws://localhost:8000/ws/cockpit build-cockpit/frize_cockpit
```

흐름: 고글/드론 시뮬이 가스·자세·**열화상 키프레임(fireground.jpg)** 송출 → 코어가 Claude로 판단 →
감지/경보가 콕핏에 표시 → 콕핏에서 진입/후퇴/정찰/방수 '딸깍' → 인터록 통과분만 디바이스로 →
고글 AR / 드론 비행 반영.

도커로 버스+코어만: `docker compose up --build`.

---

## 포트 / 토픽

| 포트 | 용도 |
|---|---|
| 1883 | MQTT 버스 |
| 8000 | 코어 HTTP(`/api/*`) + WS(`/ws/cockpit`) |

토픽: `frize/device/<id>/{heartbeat,telemetry,detection,video}`,
`frize/command/<id>[/ack]`, `frize/{alert,world/state,map/patch,judgment}`.

REST: `GET /healthz`, `GET /api/world`, `GET /api/rescue_priority`,
`POST /api/command`, `POST /api/command/<id>/confirm`, `POST /api/site_origin`.

---

## 안전 인터록 정책(요약)

- 사람 향한 방수(`aim_and_spray`) → **거부**
- 대원 SCBA 잔압 부족/낙상 시 진입 → **거부**, 후퇴 권고
- 붕괴/백드래프트/가스통 치명구역 대원 진입 → **2차 확인 필요**(휴먼 인 더 루프)
- 후퇴/복귀/착륙/강조 → 항상 허용
- 모든 판정에 사람이 읽는 사유(reason) 첨부.

---

## 검증 상태

- `frize_common` 단위 테스트(`test/test_schemas.cpp`): 직렬화 왕복/도메인 로직.
  로컬(Windows)에는 Paho/Crow/Qt 부재로 전체 링크 미수행 ― 빌드 타깃은 Linux/Jetson.
  Docker(`Dockerfile.core`)가 코어+테스트를 vcpkg로 빌드/실행한다.
- 디바이스 앱은 시뮬 내장으로 하드웨어 없이 통합 동작.
