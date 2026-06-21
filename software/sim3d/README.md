# 🧊 FRIZE 디지털 트윈 재구성 (3D Twin Reconstruction)

> "실제 건물 만들고 내부에 드론이랑 사람 돌려서 3D 만들어지는지" — 이걸 **실제로 돌려서** 증명한다.

소방 현장 3D 디지털 트윈의 **퀄리티를 큐브 복셀(마인크래프트) → 매끈한 표면 메쉬**로
끌어올린 프로덕션 파이프라인 + 그걸 검증하는 시뮬레이션.

## 무슨 일이 일어나는가

```
합성 건물(미지)  →  SCOUT 드론 + VISOR 대원이 내부 비행/도보
                 →  매 자세 LiDAR/열화상 스윕(노이즈·드롭아웃 포함)
                 →  [프로덕션 C++ 엔진 frize_recon]
                      · TSDF 투영 융합(점유 + 부호거리 + 열 채널)
                      · 마칭큐브 표면 추출(미탐사 = 구멍/프런티어)
                      · 정점 법선 + 열화상 컬러
                 →  twin.gltf / twin.ply / twin.obj  (상용 교환포맷)
                 →  품질 리포트(커버리지/핫스팟/프런티어)
```

엔진은 건물 지오메트리를 **절대 보지 않는다.** 오직 LiDAR 히트만 받는다 →
복원된 메쉬가 원래 건물과 닮으면(=GT 표면 커버리지 높으면) 트윈이 작동한 것.

## 핵심: 엔진은 C++, 이 폴더는 패키징

무거운 일(건물·LiDAR·TSDF·마칭큐브·glTF export)은 **전부 프로덕션 C++**(`libs/frize_recon`,
`apps/frize_recon_sim`)이 한다. 이 `sim3d/`의 파이썬은 그걸 **빌드·실행·검증·렌더**만 한다.
같은 엔진이 `frize_mapping`(실 LiDAR 경로)에도 그대로 들어가 콕핏 트윈에 표면 메쉬를 공급한다.

## 실행

```bash
# 의존성 0 C++만 돌리려면(엔진 직접):
g++ -std=c++17 -O3 -fopenmp \
  -Ilibs/frize_recon/include -Iapps/frize_recon_sim/include \
  apps/frize_recon_sim/src/main.cpp -o /tmp/frize_recon_sim
/tmp/frize_recon_sim out 0.07          # → out/twin.gltf + report.json

# 검증 + 프리뷰 렌더까지(패키징 래퍼):
pip install -r software/sim3d/requirements.txt
python3 software/sim3d/package.py --voxel 0.07
#   → out/{twin.gltf,ply,obj}, report.json,
#     preview_section.png(1층 단면) / preview_points.png(열 점군)
#     progression.png(25→50→75→100% 성장) / frontier.png

# CMake 모노레포 빌드(기본 ON):
cmake -S software -B build && cmake --build build -j
ctest --test-dir build -R recon_sim_smoke   # 스모크 테스트
```

## 샘플 결과 (voxel 0.07m, ~8s)

| 지표 | 값 |
|---|---|
| 스캔 포인트 | 1.26 M |
| 메쉬 정점 / 삼각형 | 1.45 M / 0.72 M |
| **GT 표면 커버리지** | **97.8 %** |
| 열 핫스팟 정점(>100℃) | 8.6 k (화점 검출) |
| 표면적 | 1297 m² |

증거 이미지는 [`docs/`](docs/) 참고:
- `progression.png` — 드론+대원이 돌수록 트윈이 자라남(25→100%)
- `preview_section.png` — 1층 단면: 방·칸막이·가구·화점(열 후광)
- `preview_points.png` — 열화상 점군

## 콕핏 연동

`frize_mapping` 을 `FRIZE_TWIN_MESH=1 FRIZE_TWIN_DIR=/run/frize` 로 띄우면
TSDF 표면 메쉬 `twin.gltf` 를 주기적으로 굽는다. 콕핏을 같은 `FRIZE_TWIN_DIR` 로
띄우면 `TwinView.qml` 의 `RuntimeLoader` 가 갱신 시마다 자동 리로드한다.

## 튜닝 노브 (env)

| 변수 | 기본 | 설명 |
|---|---|---|
| `FRIZE_TWIN_MESH` | 0 | 매핑 서비스 TSDF 메쉬 export on/off |
| `FRIZE_TWIN_VOXEL` | 0.10 | 트윈 복셀 크기(m). 작을수록 정밀↑ 메모리↑ |
| `FRIZE_TWIN_NX/NY/NZ` | 400/400/140 | TSDF 볼륨 셀 수 |
| `FRIZE_TWIN_MESH_SEC` | 3.0 | 메쉬 재굽기 주기(s) |
