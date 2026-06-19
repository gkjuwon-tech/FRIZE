# ⚡ FRIZE 회로 설계 (Electrical / Wiring Design)

> 목적: **이 문서 + BOM + SCAD + 차고 가이드대로 배선하면, 소프트웨어가 의도대로 그대로 작동한다.**
> 모든 전원/신호/커넥터를 명시한다. 전압·극성·핀번호를 지키면 동작, 안 지키면 향냄새.
> 규칙: 🟥=극성/전압 치명, 🟦=꿀팁. 빨강(+) 검정(−). 신호 GND 공통.

각 기기의 **전원 트리 → 신호 버스 → 커넥터 핀아웃 → 소프트웨어 매핑** 순.

---

## 0. 공통 규칙
- **로직 레벨:** 3.3V 기준. 5V 센서는 레벨 시프터(BOM 인터페이스 시트) 경유.
- **접지:** 모든 보드 GND 1점 공통(스타 그라운드). 모터 전원 GND와 로직 GND는 보드에서 합류.
- **데이터 매핑:** 리눅스 디바이스 노드(`/dev/tty*`, `/dev/video*`, eth)가 곧 소프트웨어 env(`FRIZE_*`)다. 표의 "SW 매핑" 열 참고.
- **퓨즈:** 각 배터리 출력 직후 퓨즈(과전류). 드론/콘솔/VENT 필수.

---

## 1. VISOR-1 (스마트 고글)

### 전원 트리
```
[21700 듀얼셀 핫스왑팩 7.4V] ─FUSE─> [PMIC 다중레일]
   ├─ 5V  ─> Jetson Orin NX (캐리어 5V in)         (피크 4A → 굵은 선)
   ├─ 5V  ─> 카메라(열화상/RGB) GMSL2 디시리얼라이저
   ├─ 3.3V─> 센서 MCU(STM32), IMU, GNSS, 가스 AFE
   ├─ 3.8V─> 5G/메시/WiFi 라디오 (각 LDO)
   └─ 5V  ─> AR 디스플레이 드라이버 + 마이크로 블로워
```
🟥 Jetson 5V 레일은 피크 전류 크다. AWG20 이상, PMIC 전류한계 ≥5A.

### 신호 버스
| 블록 | 인터페이스 | 연결 | SW 매핑 |
|---|---|---|---|
| 센서 MCU(STM32) ↔ Jetson | USB-CDC | 가스4/IMU 집약 → USB | `FRIZE_SENSOR_SERIAL=/dev/ttyACM0` |
| 가스 CO/O2/LEL/H2S | I2C/아날로그 → MCU | AFE→MCU ADC/I2C | (MCU 펌웨어가 CSV로) |
| 열화상 FLIR Boson | CSI(GMSL2) | SerDes→Jetson CSI0 | `FRIZE_THERMAL_DEV=/dev/video0` |
| RGB 저조도 | CSI(GMSL2) | SerDes→Jetson CSI1 | `/dev/video1` |
| GNSS ZED-F9P | UART | →Jetson UART | gpsd/내부 |
| AR 디스플레이 | DSI/DP | →Jetson DP | EGL |
| 5G/메시/WiFi | M.2/USB | →Jetson | 자동 |

### 센서 MCU 커넥터 핀아웃 (JST-GH 6핀, → Jetson USB)
| 핀 | 신호 | 비고 |
|---|---|---|
| 1 | 5V | MCU 전원 |
| 2 | GND | 🟥 공통 |
| 3 | USB D+ | |
| 4 | USB D− | |
| 5 | BOOT0 | ST-Link 플래시용 |
| 6 | NRST | 리셋 |

🟦 첫 통전은 전류제한 PSU 0.5A. 합선 없으면 배터리.

---

## 2. SCOUT-1 (정찰 드론)

### 전원 트리
```
[6S LiPo 22.2V] ─XT90─FUSE─> [PDB]
   ├─ 22.2V ─> ESC(6ch) ─> 모터6                     (수십 A, 굵은 실리콘선)
   ├─ 5V(BEC)─> Pixhawk 6X, 수신기, GNSS, 텔레메트리
   ├─ 5V(BEC)─> Jetson Orin NX + 캐리어
   ├─ 12V(DC-DC)─> Livox LiDAR, 짐벌
   └─ 5V ─> 가스 포드, VTX
```
🟥 XT90 극성! 역결선=ESC 즉사. PDB +/− 통전 전 멀티미터 단락 체크.

### 신호 버스
| 블록 | 인터페이스 | 연결 | SW 매핑 |
|---|---|---|---|
| Jetson ↔ Pixhawk | UART(TELEM2) via USB-UART | MAVLink | `FRIZE_FLIGHT_BACKEND=mavsdk`, `FRIZE_MAVLINK_URL=serial:///dev/ttyUSB0:921600` |
| Pixhawk ↔ ESC | PWM/DShot x6 | FC 모터출력 | (FC 설정) |
| Livox LiDAR | Ethernet | →Jetson eth(스위치) | `frize_mapping` |
| 열화상/RGB(짐벌) | CSI(GMSL2) | →Jetson | `FRIZE_THERMAL_DEV=/dev/video0` |
| GNSS RTK | UART/CAN | →Pixhawk | FC |
| ESC/배터리 텔레메트리 | CAN | →Jetson CAN | 상태 |
| 메시/VTX | USB/UART | →Jetson | 자동 |

### Pixhawk TELEM2 ↔ USB-UART 핀아웃 (JST-GH 6핀)
| 핀 | Pixhawk | USB-UART(FTDI) | 비고 |
|---|---|---|---|
| 1 | 5V | (NC, 자가급전) | |
| 2 | TX | RX | 🟥 크로스 |
| 3 | RX | TX | 🟥 크로스 |
| 4 | CTS | RTS | 흐름제어(옵션) |
| 5 | RTS | CTS | |
| 6 | GND | GND | 🟥 공통 |

🟥 모터맵/회전방향은 FC 설정에서. 첫 시동 프롭 제거.

---

## 3. CONSOLE-1 (지휘 콘솔)

### 전원 트리
```
[AC/차량 12~28V or 핫스왑 배터리] ─> [UPS 절체보드] ─> [DC-DC]
   ├─ 19V/DC ─> Jetson AGX Orin (배럴)
   ├─ 12V ─> 메인 21.5" 디스플레이 백라이트/보드
   ├─ 5V ─> 보조 7" 디스플레이, 컨트롤덱 MCU, 이더넷 스위치
   └─ 5V ─> 통신 모듈(5G/메시/WiFi) + 블로워
```
🟥 UPS 절체: 외부전원 끊겨도 콘솔 안 꺼짐(지휘 연속성).

### 신호 버스
| 블록 | 인터페이스 | 연결 | SW 매핑 |
|---|---|---|---|
| 메인 디스플레이 | DP/HDMI + USB(터치) | →AGX | EGLFS 키오스크 |
| 컨트롤덱(버튼16/인코더2/조그) | 덱 MCU → USB-HID | evdev 키/축 | 콕핏 입력 매핑 |
| E-STOP | 전용 라인 → MCU 또는 GPIO | USB-HID 키 | `FRIZE_ESTOP_DEVICE`, `frize_console_estop` |
| 보조 디스플레이 | HDMI/MIPI | →AGX | 상태표시 |
| 이더넷 스위치 | RJ45 x4 | LiDAR/장비 유선 | 코어/맵핑 |
| 통신 5G/메시/WiFi | M.2/USB | →AGX | 다중링크 |

### E-STOP 배선 (🟥 안전 핵심)
```
[E-STOP 버튼(상시폐 NC 권장)] ── 덱 MCU GPIO(풀업) ──> USB-HID KEY_ESC
                                                  └─> frize_console_estop(evdev) ─MQTT─> 전 대원 retreat/전 드론 rtl
```
- NC(평시 닫힘) 사용: 선 끊겨도 트리거(페일세이프).
- `FRIZE_ESTOP_KEYCODE`(기본 1=KEY_ESC)와 일치.

---

## 4. VENT-1 (IoT 자동 배연/진입 포트)

### 전원 트리
```
[리튬팩 + BMS 11.1V] ─FUSE─> [전원보드]
   ├─ 12V ─> 모터 드라이버(H브리지) ─> 리니어 액추에이터   (개방 시 수 A)
   ├─ 12V ─> 전자석 홀드/잠금(평시 ON=고정)
   ├─ 3.3V─> ESP32-S3 MCU, 메시 라디오
   └─ 3.3V─> 온도/CO 센서, 리미트/홀 센서, LED/부저
```
🟥 전자석 홀드는 평시 통전 고정 → OPEN 시 해제 후 액추에이터 구동(순서 중요).

### 신호/제어
| 블록 | 인터페이스 | 연결 | SW 매핑 |
|---|---|---|---|
| MCU ↔ 모터드라이버 | PWM + DIR(2핀) | 개방/폐쇄 구동 | `frize_apparatus` OPEN/CLOSE |
| 리미트 스위치 x2 | GPIO(풀업) | 완전개방/완전폐쇄 검출 | position_pct |
| 홀 위치센서 | ADC | 중간 개방도 | position_pct |
| 온도(열전대)/CO | SPI/I2C | 화재 컨텍스트 | telemetry gas/temp |
| 메시 라디오 | UART/SPI | fleet 버스 | MQTT(게이트웨이 경유) |
| MCU ↔ Jetson(옵션) | USB-CDC | 직결 운용 시 | `FRIZE_ACTUATOR_SERIAL=/dev/ttyUSB0` |

🟦 MCU 펌웨어 프로토콜: 수신 `OPEN\n`/`CLOSE\n`, 송신 `POS,<0-100>,<state>\n`. (frize_apparatus serial 백엔드와 일치)

### 모터 드라이버 핀아웃 (BTS7960급)
| 핀 | 신호 | 연결 |
|---|---|---|
| VCC | 12V | 배터리 |
| GND | GND | 🟥 공통 |
| RPWM | PWM | MCU PWM (개방) |
| LPWM | PWM | MCU PWM (폐쇄) |
| R_EN/L_EN | EN | MCU GPIO |
| IS | 전류감지 | MCU ADC(걸림 검출→fault) |

🟥 액추에이터 스톨(걸림) 시 전류 급증 → IS로 감지해 정지(과부하 보호). 안 하면 모터 탄다.

---

## 5. 시스템 결선 한 장
```
VISOR ─5G/mesh─┐
SCOUT ─5G/mesh─┤
VENT  ─mesh────┤        ┌── CONSOLE-1 (콕핏 키오스크 + E-STOP)
               ├─ MQTT ─┤
            [Core/Bus]   └── (콕핏 WS)
               │
            [Mapping] ← LiDAR/eth
```
모든 무선은 5G 우선, 끊기면 메시 폴백. 유선(LiDAR)은 이더넷 스위치.

— 이대로 납땜하면, `scripts/run_sim.sh`의 env가 실제 `/dev/*`만 가리키게 바꿔 그대로 구동된다.
