# FRIZE CONSOLE-1 ― 소프트웨어 연결 (전용 지휘 콘솔 구동)

노트북 대신 **전용 콘솔(CONSOLE-1)** 에서 콕핏을 구동한다. UI 일관성·견고성·현장 시인성 확보.

## 구성
- **컴퓨트:** Jetson AGX Orin 64GB, Ubuntu(JetPack) 기반.
- **콕핏:** `frize_cockpit`(Qt6)를 **EGLFS 키오스크**로 부팅 시 전체화면 실행(데스크톱 환경 없음).
- **코어 연결:** `FRIZE_CORE_WS`로 코어(WS)에 접속. 코어는 콘솔 자체 또는 지휘차량 노드에서 구동.
- **물리 컨트롤:** 컨트롤덱 버튼/인코더/조그/E-STOP → evdev 입력.
  - 내비/선택/명령: 콕핏으로 매핑.
  - **E-STOP: UI와 독립된 안전 경로.** `frize_console_estop` 데몬이 직접 버스(MQTT)로
    전 대원 `retreat` + 전 드론 `rtl` 을 브로드캐스트(콕핏이 멈춰도 동작).

```
[컨트롤덱 / E-STOP] --evdev--> frize_console_estop --MQTT--> 코어 --> 전 디바이스
[터치/인코더]       --------> frize_cockpit (EGLFS) --WS--> 코어
```

## 설치 (Jetson)
```bash
# 1) 콕핏/이스톱 빌드
cmake -S software/apps/frize_cockpit -B build-cockpit -DCMAKE_PREFIX_PATH=/opt/Qt/6.x/gcc_arm64
cmake --build build-cockpit -j
cmake -S software -B build -DFRIZE_BUILD_CONSOLE=ON && cmake --build build --target frize_console_estop

# 2) 배치
sudo install build-cockpit/frize_cockpit /usr/local/bin/
sudo install build/apps/frize_console_estop/frize_console_estop /usr/local/bin/
sudo install software/apps/frize_cockpit/deploy/kiosk.sh /usr/local/bin/frize-kiosk
sudo cp software/apps/frize_cockpit/deploy/*.service /etc/systemd/system/
sudo systemctl enable --now frize-cockpit frize-console-estop
```

## 키오스크 동작
- `frize-cockpit.service` → `frize-kiosk`(EGLFS, 커서 숨김, 화면꺼짐 방지) → `frize_cockpit`.
- 크래시 시 systemd가 자동 재시작(`Restart=always`). 현장에서 죽으면 안 됨.
- 자동 밝기: 조도센서 → backlight sysfs(별도 udev rule, 본 문서 범위 외).

## E-STOP 매핑
- E-STOP은 USB-HID 또는 GPIO 키로 잡혀 evdev `KEY_*` 코드로 들어온다.
- `FRIZE_ESTOP_DEVICE`(예: `/dev/input/by-id/...-estop`), `FRIZE_ESTOP_KEYCODE`(기본 KEY_ESC).
- 누르면: 모든 visor에 `retreat`(탈출 내비), 모든 scout에 `rtl`. 코어 인터록은 후퇴/복귀를 항상 허용.

자세한 환경변수는 `.env.example` 참고.
