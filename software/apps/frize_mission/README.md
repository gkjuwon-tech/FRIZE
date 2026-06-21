# frize_mission ― 풀 작전 시연 파이프라인 (디지털 트윈 + 드론 VLM + 고글 AR 구조)

내가 만든 트윈/엔진 위에서 도는 **콕핏 조종 시연 영상**을 끝까지 생성하는 파이프라인.
목업이 아니라 실제 시뮬(TSDF 융합·LOS 가시성)이 만든 타임라인을 콕핏에 입힌다.

```
frize_mission (C++)  →  points.bin + mission.json       작전 시뮬 + 타임라인
render.cpp    (GL )  →  frames/twin_XXXX.png + cam.bin   자라나는 트윈(포인트클라우드)
compose.py    (PIL)  →  comp/c_XXXX.png                  콕핏 HUD + 고글 POV + AR 합성
cards.py + ffmpeg    →  frize_cockpit_demo.mp4           타이틀/아웃트로 + 인코딩
```

## 시나리오(실제 시뮬이 구동)
- **드론 3대(SCOUT-1/2/3)** 가 층별로 자율 정찰하며 LiDAR 스윕을 공유 TSDF 에 융합
  → 트윈이 프레임마다 채워진다. 벽에 막히면(blocked) 콕핏 로스터/마커가 amber.
- 건물엔 실제 시맨틱: **가스레인지(발화원)·화점 2·가구·요구조자 2**.
- **드론 VLM**: 드론 카메라가 생존자(person 박스)를 LOS 로 처음 포착하는 순간을 탐지
  이벤트로 기록(`detect`, 신뢰도/층/좌표). → 콕핏 알림 배너 + VLM 카드 + 트윈 핑.
- **지휘소 → 고글 AR**: 탐지 직후 `ar_cmd`(경로 지시) → 고글 POV 패널에 AR 셰브론/
  타깃 브라켓("비반응 인원")·거리 오버레이 → `rescue`(요구조자 확보 스탬프).
- 고글 1인칭은 **실제 소방 내부수색 영상**(Exeter FD Search & Rescue, archive.org,
  퍼블릭 도메인 추정 — 배포 전 라이선스 확인) 을 합성.

## 실행
```bash
# 0) 의존: g++, OpenMP, GLFW/GL, Xvfb, python3(PIL,numpy), ffmpeg, /tmp/imgui(stb 헤더)
# 1) 시뮬 타임라인
g++ -O2 -fopenmp -std=c++17 -I../../libs/frize_recon/include -I../frize_recon_sim/include \
    main.cpp -o /tmp/frize_mission
/tmp/frize_mission /tmp/mission 480 0.07 24          # out, NF, voxel, fps
# 2) 트윈 성장 렌더(1080p, MSAA4)
g++ -O2 -std=c++17 -I/tmp/imgui render.cpp -o /tmp/frize_mrender -lglfw -lGL -ldl -lpthread
xvfb-run -a -s "-screen 0 1920x1080x24" /tmp/frize_mrender /tmp/mission /tmp/mission/frames 1920 1080 0 -1 4
# 3) 고글 POV 프레임(내부수색 구간만 큐레이션) → /tmp/pov2/p_XXXX.jpg
# 4) 콕핏 합성
python3 compose.py /tmp/mission /tmp/pov2 0 480
# 5) 카드 + 인코딩
python3 cards.py /tmp/mission
ffmpeg ... (title + main + outro concat)  →  frize_cockpit_demo.mp4
```

## 산출물(레포 미포함)
`points.bin`·`frames/`·`comp/`·`*.mp4` 와 제3자 영상은 용량/라이선스로 커밋하지 않는다.
코드만 커밋 — 위 절차로 재생성. 콕핏 합성은 단일 톤(모노크롬)·control_map SSOT 유지.
