# 🧯 FRIZE 핸드오프 문서 (a.k.a. "다음 사람한테 떠넘기기")

> 안녕? 이걸 읽고 있다는 건 네가 FRIZE를 이어받았다는 뜻이고, 동시에 누군가
> "이거 마저 좀…" 하고 도망쳤다는 뜻이다. 환영한다. 커피 한 잔 타와라. 길다.
>
> 결론부터: **하드웨어 설계·BOM·회로·차고가이드·렌더, 소프트웨어 풀스택(코어/드론/고글/맵핑/콘솔/IoT장비)·콕핏·3D 트윈까지 다 짜여 있다.** 단, 이 윈도우 박스에선 C++/Qt를 풀빌드 못 했다(툴체인 부재). 빌드 타깃은 Linux/Jetson이다. 그래서 "코드는 완성, 실기 검증은 네 몫" 상태다. 미안하다. 근데 재밌을 거다.

---

## 1. 이게 다 뭐냐 (5초 요약)
소방 현장을 OS로 묶는 시스템. **드론(SCOUT-1)**·**고글(VISOR-1)**·**IoT 배연포트(VENT-1)**·**지휘콘솔(CONSOLE-1)** 을 **코어(C++ 커널)** 가 버스(MQTT)로 묶고, 지휘관은 콘솔에서 3D 디지털 트윈을 보며 딸깍 명령한다. 판단은 VLM(Claude)이 전담, 안전 인터록이 생명 직결 명령을 거른다.

폴더:
- `hardware/` — `scad/`(4기기), `bom/`(xlsx·pdf 생성기), `docs/`(조립가이드·회로설계), `mesh/`(STL), `renders_studio/`(Blender 렌더), `render_blender/`(렌더 스크립트+HDRI)
- `software/` — C++ 모노레포(CMake+vcpkg) + Qt 콕핏
- `landing/` — 제품 랜딩페이지
- `MEMORY`/이 문서 — 정신 건강용

---

## 2. 다 된 것 ✅
- **하드웨어 SCAD 4종**: `frize_scout_frame / frize_visor / frize_console / frize_vent`. STL 익스포트 완료(`hardware/mesh/`). 콘솔은 재질그룹별 STL(`console_body/screen/metal/accent/rubber`)도 있음.
- **BOM**: `hardware/bom/generate_bom.py` → xlsx/pdf. CONSOLE/VISOR/SCOUT/VENT/SW↔HW연결/공구 시트. **지휘소 1식 ≈ $24,754.**
- **회로설계**: `hardware/docs/FRIZE_회로설계.md` (전원트리·핀아웃·배선, BOM/SCAD/SW와 1:1).
- **차고 조립 가이드**: `hardware/docs/FRIZE_조립가이드북.md` (파트 A~S + VENT 파트 V, 번호대로 따라가면 됨).
- **스튜디오 렌더**: `hardware/renders_studio/{scout,console,visor,vent}.png` (Blender + Poly Haven HDRI + 다중재질).
- **소프트웨어(C++17)**: `frize_common`(프로토콜/스키마/버스/기하) · `frize_core`(레지스트리/월드모델/라우터/안전인터록/VLM) · `frize_scout`(MAVSDK+OpenCV HAL+시뮬, **자율 프런티어 탐사**) · `frize_visor`(센서/캡처/AR HAL+시뮬) · `frize_mapping`(복셀 점유+열 융합+프런티어) · `frize_apparatus`(VENT-1) · `frize_console_estop`(E-STOP 안전경로).
- **콕핏(Qt6/QML)**: 라이트 톤 UI + WebSocket 백엔드 + **3D 디지털 트윈 뷰(TwinView.qml, Qt Quick 3D, 미탐사 블러)**.
- **🆕 트윈 재구성 엔진(`libs/frize_recon`, 의존성 0 C++17)**: **TSDF 투영 융합 + 마칭큐브 표면 추출 + 법선/열컬러 + glTF·PLY·OBJ export.** 큐브 복셀(마인크래프트) → **매끈한 표면 메쉬**로 트윈 퀄리티 격상. `frize_mapping`에 통합(실 LiDAR 포인트를 `integrate()` 훅으로 점유격자+TSDF 동시 누적, 주기적으로 `twin.gltf` 굽기). 콕핏 `TwinView.qml`은 `RuntimeLoader`로 그 glTF를 런타임 자동 로드/리로드.
- **🆕 트윈 재구성 시뮬(`apps/frize_recon_sim`)**: 합성 2층 건물 + SCOUT 드론/VISOR 대원 LiDAR(노이즈·드롭아웃) → 프로덕션 엔진으로 3D 표면 재구성. **이 박스 g++로 실제 빌드·실행 검증됨**(~8s, GT 표면 커버리지 **97.8%**, 화점 열 핫스팟 검출). 파이썬 패키징/검증/렌더 래퍼는 `software/sim3d/`(README+증거 프리뷰 `docs/`).
- **통합**: `docker-compose`, `mosquitto.conf`, `Dockerfile.core`, `scripts/run_sim.sh`(시뮬 end-to-end), CONSOLE 배치(systemd/kiosk).

## 3. 안 된 것 / 네가 할 것 ⚠️ (= 핸드오프의 본체)
1. **C++/Qt 풀빌드 검증.** 이 박스엔 Paho/Crow/OpenCV/MAVSDK/PCL/Qt가 없다. Linux/Jetson + vcpkg로 빌드해라.
   - 빠른 길: `docker compose up --build` (코어+버스), 나머지는 호스트 빌드. `software/README.md` 참고.
   - `frize_common` 헤더 단위테스트(`test/test_schemas.cpp`)는 있다. 먼저 그거부터 통과시켜라(자신감 +10).
2. **TwinView를 Main에 배선.** TwinView.qml은 만들어 등록까지 했지만, 안전하게 두려고 `Main.qml` 중앙엔 아직 안 꽂았다. 중앙 카드를 TwinView로 바꾸고 영상은 PiP로 내려라. Qt Quick 3D 미설치면 `find_package(... Quick3D)`부터.
3. **3D 트윈 실측 튜닝.** ~~합리적 추정값~~ → **트윈 재구성 엔진(TSDF→마칭큐브 표면 메쉬)이 들어왔다**(`libs/frize_recon`). 시뮬로 GT 커버리지 97.8% 검증 완료. 남은 일: 실제 Livox 포인트로 `FRIZE_TWIN_VOXEL`/`trunc`/카빙거리 튜닝, 콕핏에서 `FRIZE_TWIN_DIR` 물려 glTF 표면 로드 확인(Quick3D `RuntimeLoader` 필요). 큐브 복셀 경로는 폴백으로 유지됨.
4. **프런티어 탐사 실비행 튜닝.** `frize_scout`의 그리디 최근접 프런티어는 동작하지만, 실전에선 정보이득(information gain) 가중·충돌회피·배터리 예산을 더해라.
5. **MCU 펌웨어.** 센서팩(STM32)·VENT-1(ESP32) 펌웨어는 인터페이스만 정의(회로설계 §). 실제 펌웨어는 네가 굽는다(프로토콜은 문서에 박제됨).
6. **보안.** MQTT TLS+ACL, WS 인증. 지금은 데모라 anonymous. 현장 가기 전 필수.
7. **인증/규제.** 소방·인명 장비는 규제·시험이 있다. 이건 프로토타입이다. 향 피우지 말고 절차 밟아라.

## 4. 어떻게 굴리냐 (시뮬, 하드웨어 0개로)
```bash
docker compose up -d mosquitto
export ANTHROPIC_API_KEY=sk-ant-...        # VLM 판단 켜기(선택)
cmake -S software -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DFRIZE_BUILD_CORE=ON -DFRIZE_BUILD_SCOUT=ON -DFRIZE_BUILD_VISOR=ON \
  -DFRIZE_BUILD_MAPPING=ON -DFRIZE_BUILD_APPARATUS=ON && cmake --build build -j
BUILD=build scripts/run_sim.sh             # 코어+드론+고글2+VENT+맵핑(전부 시뮬)
# 콕핏(Qt) 별도 빌드 후 실행 → 딸깍하면 시뮬 디바이스가 반응한다
```
시뮬 흐름: 고글/드론이 가스·자세·열화상 키프레임(`fireground.jpg`) 송출 → 코어 VLM 판단 → 감지/경보가 콕핏에 → 딸깍(진입/후퇴/정찰/열기/방수) → 인터록 통과분만 디바이스로 → 드론 자율탐사로 프런티어 채움 → 맵핑이 트윈 복셀 송출 → 콕핏 트윈에 렌더.

## 5. 지뢰밭 (밟지 마라)
- 🟥 이 박스 g++는 `MSYSTEM=UCRT64 /c/msys64/usr/bin/bash.exe -l` 로만 정상 동작(그냥 PATH로 부르면 cc1plus/as DLL 못 찾음).
- 🟥 STL은 색 정보가 없다 → 단일재질로 렌더된다. 콘솔처럼 **재질그룹별 STL 익스포트**(`-D MAT="screen"` 등)해야 다중재질. 다른 기기도 이 방식으로 가면 더 예쁘다.
- 🟥 Blender HIP(AMD)은 실패하고 OPTIX(NVIDIA)로 GPU 렌더된다(로그에 경고는 정상).
- 🟥 안전 인터록은 후퇴/복귀/개폐를 **항상 허용**으로 둠. 새 명령 추가 시 정책 검토.

## 6. 한 줄 인수인계
**"코드는 다 있다. Linux에서 빌드하고, 트윈을 Main에 꽂고, 펌웨어 굽고, 인증 받아라. 그리고 너도 다음 사람한테 이 문서 끝에 한 줄 추가하고 도망쳐라."**

— 인수인계 로그:
- 2026-06: 초기 풀스택 + 4기기 + 트윈 1차. (이전 담당)
- 2026-06: 트윈 퀄리티 격상 ― 프로덕션 TSDF+마칭큐브 재구성 엔진(`frize_recon`, 의존성0 C++) 신설, `frize_mapping`/콕핏(`RuntimeLoader`) 통합, glTF/PLY/OBJ export. 합성건물+드론/대원 LiDAR 시뮬(`frize_recon_sim`)을 **g++로 실제 빌드·실행해 표면 재구성 검증(커버리지 97.8%, 화점 검출)**. 파이썬 패키징 래퍼 `software/sim3d/`. (다음 사람: 실 LiDAR로 튜닝 + 콕핏 메쉬 로드 실기 확인하고 도망쳐라.)
- (여기에 네 줄 추가)

🔥 FRIZE — We freeze the chaos. 🧊
