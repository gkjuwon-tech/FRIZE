#!/usr/bin/env python3
# 디지털 트윈 위로 화재가 번지는 영상 렌더(랩별 PLY 열화상 메쉬 → mp4).
#  랩 = 시간축. 카메라는 천천히 공전, 지붕은 잘라 내부를 본다. 정점색=열화상.
import sys, json, math
from pathlib import Path
import numpy as np, trimesh, imageio.v2 as imageio
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

OUT = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/vid_out")
report = json.loads((OUT / "report.json").read_text())
ext = report["building_extent_m"]
prog = report.get("fire_progression", [])
spread = report.get("spread_vectors", [])

laps = sorted(OUT.glob("twin_lap*.ply"), key=lambda p: int(p.stem.split("lap")[1]))
print(f"[vid] {len(laps)} laps, ext={ext}")

def load(ply):
    m = trimesh.load(str(ply), process=False)
    return (np.asarray(m.vertices), np.asarray(m.faces),
            np.asarray(m.visual.vertex_colors)[:, :3] / 255.0)

def shade(tris, base):
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
    sh = np.clip(np.abs(n @ np.array([0.3, 0.4, 0.86])), 0.32, 1)[:, None]
    return np.clip(base * 0.42 + base * sh * 0.9, 0, 1)

# 랩별 단면 삼각형(천장 컷) 미리 로드
frames_data = []
for i, ply in enumerate(laps):
    V, F, C = load(ply)
    cz = V[F][:, :, 2].mean(1)
    sec = F[cz < 2.7]                      # 지붕/상부 제거 → 내부 가시
    tris = V[sec]
    col = shade(tris, C[sec].mean(1))
    hot = int(prog[i]["hot_vertices"]) if i < len(prog) else 0
    t = prog[i]["t"] if i < len(prog) else 0
    front = prog[i]["front"] if i < len(prog) else 0
    frames_data.append((tris, col, t, hot, front))
    print(f"  lap{i}: {len(tris)} tris, t={t:.0f}s hot={hot}")

FPS = 20
PER_LAP = 16                               # 랩당 프레임(공전)
W, H = ext[0], ext[1]

writer = imageio.get_writer(OUT / "fire_spread.mp4", fps=FPS,
                            codec="libx264", quality=8,
                            macro_block_size=None)

total = len(frames_data) * PER_LAP
for fi in range(total):
    lap = fi // PER_LAP
    sub = fi % PER_LAP
    tris, col, t, hot, front = frames_data[lap]
    azim = -75 + 360.0 * fi / total        # 전체 1바퀴 공전
    elev = 48 + 6 * math.sin(fi * 0.13)

    fig = plt.figure(figsize=(9, 6.2), dpi=96)
    ax = fig.add_subplot(111, projection="3d")
    ax.add_collection3d(Poly3DCollection(tris, facecolors=col, linewidths=0))
    # 확산 화살표(최종 분석 기준, 마지막 랩들에서 표시)
    if lap >= len(frames_data) - 3:
        for s in spread[:2]:
            c, d = s["c"], s["dir"]
            L = 3.5
            ax.quiver(c[0], c[1], c[2], d[0]*L, d[1]*L, d[2]*L,
                      color="#ffd23c", linewidth=2.2, arrow_length_ratio=0.35)

    ax.set_xlim(0, W); ax.set_ylim(0, H); ax.set_zlim(0, ext[2] + 0.6)
    ax.set_box_aspect([W, H, ext[2] + 0.6]); ax.set_axis_off()
    ax.view_init(elev=elev, azim=azim)
    fig.patch.set_facecolor("#0c0f13")

    # HUD
    fig.text(0.04, 0.94, "FRIZE · DIGITAL TWIN — FIRE SPREAD",
             color="#cfd4da", fontsize=13, fontweight="bold", family="monospace")
    fig.text(0.04, 0.90, f"t = {t:4.0f}s   |   fire surface = {hot:,} verts   |   spreading front = {front:,} cells",
             color="#ff7a3c", fontsize=10.5, family="monospace")
    bar = (hot / max(1, frames_data[-1][3]))
    fig.patches.append(plt.Rectangle((0.04, 0.05), 0.42 * bar, 0.018,
                       transform=fig.transFigure, color="#ff4b1e", zorder=10))
    fig.patches.append(plt.Rectangle((0.04, 0.05), 0.42, 0.018,
                       transform=fig.transFigure, fill=False, edgecolor="#555",
                       linewidth=0.8, zorder=9))

    fig.canvas.draw()
    buf = np.frombuffer(fig.canvas.buffer_rgba(), dtype=np.uint8)
    img = buf.reshape(fig.canvas.get_width_height()[::-1] + (4,))[:, :, :3]
    img = img[:img.shape[0] // 2 * 2, :img.shape[1] // 2 * 2]   # 짝수 크기(yuv420p)
    img = np.ascontiguousarray(img)
    writer.append_data(img)
    plt.close(fig)
    if fi % 16 == 0:
        print(f"  frame {fi}/{total}")

writer.close()
print(f"[vid] wrote {OUT/'fire_spread.mp4'}")
