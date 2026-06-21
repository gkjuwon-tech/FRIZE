#!/usr/bin/env python3
"""FRIZE 디지털 트윈 재구성 시뮬 ― 패키징/검증 래퍼.

무거운 일(합성 건물·LiDAR·TSDF 융합·마칭큐브·glTF export)은 전부 프로덕션 C++
엔진(frize_recon)이 한다. 이 스크립트는 그걸 '패키징'할 뿐이다:
  1) C++ 시뮬(frize_recon_sim)을 빌드  (cmake 있으면 cmake, 없으면 g++ 직접)
  2) 실행 → out/ 에 twin.{gltf,ply,obj} + report.json + 진행 스냅샷 생성
  3) glTF를 trimesh로 로드·검증(상용 툴체인이 먹는지)
  4) matplotlib로 멀티앵글 프리뷰 PNG + 프런티어 진행 차트 렌더
  5) 품질 리포트 요약 출력

사용:  python3 package.py [--voxel 0.07] [--out out] [--no-build]
"""
from __future__ import annotations
import argparse, json, os, shutil, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SW   = HERE.parent                                  # software/
RECON_INC = SW / "libs/frize_recon/include"
SIM_INC   = SW / "apps/frize_recon_sim/include"
SIM_SRC   = SW / "apps/frize_recon_sim/src/main.cpp"


def build(out_dir: Path) -> Path:
    """frize_recon_sim 빌드. 바이너리 경로 반환."""
    binpath = out_dir / "frize_recon_sim"
    cxx = os.environ.get("CXX", "g++")
    if not shutil.which(cxx):
        sys.exit(f"[package] C++ 컴파일러({cxx})가 없다. build-essential 설치 필요.")
    cmd = [cxx, "-std=c++17", "-O3", "-fopenmp",
           f"-I{RECON_INC}", f"-I{SIM_INC}", str(SIM_SRC), "-o", str(binpath)]
    print("[package] 빌드:", " ".join(cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        # OpenMP 없으면 빼고 재시도
        cmd = [c for c in cmd if c != "-fopenmp"]
        print("[package] OpenMP 없이 재시도")
        subprocess.run(cmd, check=True)
    return binpath


def run_sim(binpath: Path, out_dir: Path, voxel: float):
    print(f"[package] 시뮬 실행 (voxel={voxel}m) …")
    t = time.time()
    subprocess.run([str(binpath), str(out_dir), str(voxel)], check=True)
    print(f"[package] 시뮬 {time.time()-t:.1f}s")


def validate_gltf(out_dir: Path) -> dict:
    """상용 3D 툴체인(trimesh)이 glTF를 먹는지 검증."""
    import trimesh, numpy as np
    g = out_dir / "twin.gltf"
    m = trimesh.load(str(g), force="mesh")
    info = dict(vertices=int(len(m.vertices)), faces=int(len(m.faces)),
                bounds=m.bounds.tolist(), extents=m.extents.tolist())
    print(f"[validate] glTF OK  정점={info['vertices']:,} 면={info['faces']:,} "
          f"치수(m)={[round(x,2) for x in info['extents']]}")
    return info


def render(out_dir: Path, report: dict):
    """멀티앵글 프리뷰 + 진행 차트(헤드리스 matplotlib)."""
    import numpy as np, trimesh, matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    def load(ply):
        m = trimesh.load(str(ply), process=False)
        return np.asarray(m.vertices), np.asarray(m.faces), \
               np.asarray(m.visual.vertex_colors)[:, :3] / 255.0

    def shade(tris, base):
        n = np.cross(tris[:, 1]-tris[:, 0], tris[:, 2]-tris[:, 0])
        n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
        sh = np.clip(np.abs(n @ np.array([0.3, 0.4, 0.86])), 0.3, 1)[:, None]
        return np.clip(base*0.45 + base*sh*0.85, 0, 1)

    def style(ax, ext):
        ax.set_xlim(0, ext[0]); ax.set_ylim(0, ext[1]); ax.set_zlim(0, ext[2])
        ax.set_box_aspect(ext); ax.set_axis_off()

    ext = report["building_extent_m"]
    V, F, C = load(out_dir / "twin.ply")

    # 1) 1층 단면 히어로(지붕/2층 제거) → 방·가구·화점이 보임
    cz = V[F][:, :, 2].mean(1)
    sec = F[cz < 2.7]; tris = V[sec]
    fig = plt.figure(figsize=(11, 8)); ax = fig.add_subplot(111, projection="3d")
    ax.add_collection3d(Poly3DCollection(tris, facecolors=shade(tris, C[sec].mean(1)), linewidths=0))
    style(ax, [ext[0], ext[1], ext[2]+0.6]); ax.view_init(elev=55, azim=-62)
    fig.savefig(out_dir/"preview_section.png", dpi=95, bbox_inches="tight", facecolor="#0c0f13")
    plt.close(fig)

    # 2) 열화상 점군(전체 정점, 컬러=온도)
    fig = plt.figure(figsize=(11, 8)); ax = fig.add_subplot(111, projection="3d")
    ax.scatter(V[:, 0], V[:, 1], V[:, 2], c=C, s=0.5, marker=".", linewidths=0, depthshade=False)
    style(ax, [ext[0], ext[1], ext[2]+0.6]); ax.view_init(elev=30, azim=-60)
    fig.savefig(out_dir/"preview_points.png", dpi=95, bbox_inches="tight", facecolor="#0c0f13")
    plt.close(fig)

    # 3) 진행 트립틱 (랩별 재방문 → 화재가 번지며 메쉬 열색이 자라남)
    laps = int(report.get("laps", 0))
    lap_s = float(report.get("lap_seconds", 0))
    snaps = [(out_dir/f"twin_lap{i}.ply", f"t={int((i+1)*lap_s)}s") for i in range(laps)]
    snaps = [(p, l) for p, l in snaps if p.exists()]
    if not snaps:  # 폴백(구 포맷)
        snaps = [(out_dir/f"twin_{p}.ply", f"{p}%") for p in (25,50,75)]
        snaps = [(p, l) for p, l in snaps if p.exists()] + [(out_dir/"twin.ply", "100%")]
    fig = plt.figure(figsize=(4.4*len(snaps), 4))
    for i, (ply, lab) in enumerate(snaps):
        v, f, c = load(ply)
        cz = v[f][:, :, 2].mean(1); sec = f[cz < 2.7]; tr = v[sec]
        ax = fig.add_subplot(1, len(snaps), i+1, projection="3d")
        ax.add_collection3d(Poly3DCollection(tr, facecolors=shade(tr, c[sec].mean(1)), linewidths=0))
        style(ax, [ext[0], ext[1], ext[2]+0.6]); ax.view_init(elev=62, azim=-62)
        ax.set_title(f"{lab}  ·  {len(v)//1000}k verts", color="#cfd4da", fontsize=10)
    fig.patch.set_facecolor("#0c0f13")
    fig.savefig(out_dir/"progression.png", dpi=92, bbox_inches="tight", facecolor="#0c0f13")
    plt.close(fig)

    # 4) 화재 확산 진행 차트(재방문할 때마다 측정 → 불이 번지는 곡선)
    fp = report.get("fire_progression", [])
    if fp:
        ts = [p["t"] for p in fp]
        hv = [p["hot_vertices"] for p in fp]
        fr = [p["front"] for p in fp]
        fig, ax = plt.subplots(figsize=(6.2, 3.4))
        ax.plot(ts, hv, "-o", color="#ff6a2c", label="화재 표면 정점 (>100℃)")
        ax.fill_between(ts, hv, color="#ff6a2c", alpha=0.15)
        ax.set_xlabel("sim time (s)  ·  드론 재방문 시점마다 측정")
        ax.set_ylabel("fire surface vertices", color="#ff6a2c")
        ax2 = ax.twinx()
        ax2.plot(ts, fr, "--s", color="#ffd23c", label="확산 전선 셀")
        ax2.set_ylabel("spreading-front cells", color="#ffd23c")
        ax.set_title("Fire spread over time (revisit-driven)"); ax.grid(alpha=0.3)
        fig.tight_layout()
        fig.savefig(out_dir/"fire_spread.png", dpi=100, bbox_inches="tight", facecolor="white")
        plt.close(fig)

    print(f"[render] preview_section.png / preview_points.png / progression.png / fire_spread.png")


def summarize(report: dict):
    r = report
    print("\n" + "="*52)
    print("  FRIZE 디지털 트윈 재구성 ― 품질 리포트")
    print("="*52)
    rows = [
        ("건물 치수 (m)",        f"{r['building_extent_m']}"),
        ("TSDF 볼륨 (voxels)",   f"{r['volume_dims']}  @ {r['voxel_m']}m"),
        ("드론 / 대원 자세",      f"{r['drone_poses']} / {r['visor_poses']}"),
        ("스캔 포인트",          f"{r['scan_points']:,}"),
        ("관측 셀",              f"{r['observed_cells']:,}  ({r['explored_fraction']*100:.1f}% of volume)"),
        ("메쉬 정점 / 삼각형",    f"{r['mesh_vertices']:,} / {r['mesh_triangles']:,}"),
        ("표면적 (m²)",          f"{r['surface_area_m2']:.1f}"),
        ("열 핫스팟 정점(>100℃)", f"{r['thermal_hotspot_vertices']:,}"),
        ("★ GT 표면 커버리지",   f"{r['gt_surface_coverage_pct']:.1f}%"),
        ("프런티어(미탐사)",      f"{r['frontier_final']:,}"),
        ("엔진 런타임",          f"{r['runtime_s']:.1f}s"),
    ]
    for k, v in rows:
        print(f"  {k:<22} {v}")
    # ── 화재 확산 분석(있으면) ──
    fs = r.get("fire_summary")
    fp = r.get("fire_progression", [])
    if fs:
        print("-"*52)
        print("  🔥 화재 확산 분석 (열화상 시계열)")
        if fp:
            h0, hN = fp[0]["hot_vertices"], fp[-1]["hot_vertices"]
            grow = (hN / h0) if h0 else 0
            print(f"  {'화재 표면 성장':<22} {h0:,} → {hN:,} 정점  (×{grow:.2f})")
        print(f"  {'확산 전선 셀':<22} {fs['front_cells']:,}  (평균 {fs['mean_front_rate_cps']:.1f}℃/s)")
        print(f"  {'화점 핵 / 면적':<22} {fs['burning_cells']:,} 셀  ~{fs['burning_area_m2']:.0f} m²")
        print(f"  {'최고 온도 / 가열률':<22} {fs['max_temp_c']:.0f}℃  /  {fs['max_rate_cps']:.1f}℃/s")
        sv = r.get("spread_vectors", [])
        for s in sv[:3]:
            c, d = s["c"], s["dir"]
            print(f"    · 화점({c[0]:.1f},{c[1]:.1f}) → 방향({d[0]:+.2f},{d[1]:+.2f},{d[2]:+.2f}) "
                  f"{s['rate']:.1f}℃/s")
        print(f"  {'재정찰 목표':<22} {r.get('revisit_targets',0)}개 (드론 재방문 지점)")
    print("="*52)
    cov = r["gt_surface_coverage_pct"]
    verdict = "✅ 트윈 재구성 성공" if cov >= 80 else "⚠️ 커버리지 낮음 — 경로/센서 점검"
    print(f"  {verdict}  (드론+대원 LiDAR → 3D 표면 메쉬)")
    ver = r.get("verification")
    if ver:
        ok = all(ver.values())
        mark = lambda b: "✅" if b else "❌"
        print(f"  {mark(ver['twin_shows_spread'])} 트윈에 화재 확산 표시   "
              f"{mark(ver['spread_detected'])} 확산 전선/벡터 검출")
        print(f"  {mark(ver['has_direction'])} 확산 방향 추정        "
              f"{mark(ver['revisit_ready'])} 재정찰 목표 산출")
        print(f"  {'🔥 화재 확산 분석 검증 통과' if ok else '⚠️ 일부 검증 실패'}")
    print("="*52 + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--voxel", type=float, default=0.07)
    ap.add_argument("--out", default=str(HERE / "out"))
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-render", action="store_true")
    a = ap.parse_args()

    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    binpath = out / "frize_recon_sim"
    if not a.no_build or not binpath.exists():
        binpath = build(out)
    run_sim(binpath, out, a.voxel)

    report = json.loads((out / "report.json").read_text())
    try:
        validate_gltf(out)
    except Exception as e:
        print(f"[validate] glTF 검증 건너뜀: {e}")
    if not a.no_render:
        try:
            render(out, report)
        except Exception as e:
            print(f"[render] 렌더 건너뜀: {e}")
    summarize(report)


if __name__ == "__main__":
    main()
