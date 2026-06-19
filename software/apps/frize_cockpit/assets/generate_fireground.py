# -*- coding: utf-8 -*-
"""콕핏 메인 뷰포트 배경 'fireground.jpg' 생성기.
실제 현장 사진 대신 쓰는 시네마틱 합성 배경(연기/화염광/엠버/헤이즈).
실제 운용 시 대원 고글/드론 카메라 실시간 프레임으로 대체된다."""
import numpy as np
from PIL import Image, ImageFilter, ImageDraw
import os

W, H = 1920, 1080
rng = np.random.default_rng(11)

def fractal(w, h, octaves=5, seed=0):
    r = np.random.default_rng(seed)
    acc = np.zeros((h, w), np.float32); amp = 1.0; tot = 0.0
    for o in range(octaves):
        cw, ch = max(2, w >> (octaves - o)), max(2, h >> (octaves - o))
        layer = r.random((ch, cw)).astype(np.float32)
        layer = np.array(Image.fromarray((layer*255).astype('uint8')).resize((w, h), Image.BICUBIC), np.float32)/255.0
        acc += layer * amp; tot += amp; amp *= 0.55
    return acc / tot

# 1) 베이스: 어두운 실내, 아래쪽 따뜻한 그라데이션
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
ty = yy / H
base = np.zeros((H, W, 3), np.float32)
top = np.array([10, 11, 16]); bot = np.array([46, 24, 14])
for c in range(3):
    base[..., c] = top[c]*(1-ty) + bot[c]*ty

# 2) 화염광 발생원 (문/개구부): 우중앙, 강한 주황-노랑 글로우
fx, fy = W*0.62, H*0.58
r = np.sqrt(((xx-fx)/(W*0.42))**2 + ((yy-fy)/(H*0.5))**2)
glow = np.clip(1 - r, 0, 1)
glow = glow**1.8
fire_col = np.array([255, 138, 42])
core_col = np.array([255, 214, 150])
for c in range(3):
    base[..., c] += glow * fire_col[c] * 0.85
    base[..., c] += (np.clip(1-r*1.8,0,1)**3) * core_col[c] * 0.6

# 개구부(밝은 화염 사각) ― 부드러운 직사각 글로우
door = np.zeros((H, W), np.float32)
dx0, dx1, dy0, dy1 = int(W*0.55), int(W*0.69), int(H*0.30), int(H*0.86)
door[dy0:dy1, dx0:dx1] = 1.0
door = np.array(Image.fromarray((door*255).astype('uint8')).filter(ImageFilter.GaussianBlur(60)), np.float32)/255.0
for c in range(3):
    base[..., c] += door * core_col[c] * 0.5

# 3) 연기: 프랙탈 노이즈, 위로 갈수록 짙게, 회갈색
smoke = fractal(W, H, 6, seed=3)
smoke_mask = np.clip((smoke - 0.42) * 2.2, 0, 1) * (0.45 + 0.55*(1-ty))
smoke_col = np.array([120, 110, 120])
for c in range(3):
    base[..., c] = base[..., c]*(1 - smoke_mask*0.55) + smoke_col[c]*smoke_mask*0.55
# 화염 근처 연기는 주황빛 물듦
warm_smoke = smoke_mask * glow
for c in range(3):
    base[..., c] += warm_smoke * np.array([255,120,40])[c] * 0.35

# 4) 구조물 실루엣(세로 빔 2~3개) ― 어둡게
for bxc, bw in [(W*0.18, 36), (W*0.40, 28), (W*0.88, 44)]:
    m = np.exp(-((xx - bxc)/bw)**2)
    for c in range(3):
        base[..., c] *= (1 - m*0.6)

# 5) 차가운 반대광(좌측 상단, 소방 조명 느낌)
cr = np.sqrt(((xx-W*0.12)/(W*0.5))**2 + ((yy-H*0.2)/(H*0.5))**2)
cool = np.clip(1-cr,0,1)**2
for c, v in enumerate([70, 90, 120]):
    base[..., c] += cool * v * 0.18

base = np.clip(base, 0, 255)

img = Image.fromarray(base.astype('uint8'), 'RGB')

# 6) 엠버(불티): 밝은 주황 점 + 블룸
ember = Image.new('RGB', (W, H), (0,0,0))
ed = ImageDraw.Draw(ember)
n = 480
for _ in range(n):
    # 화염 근처에 더 많이
    if rng.random() < 0.7:
        ex = int(np.clip(rng.normal(fx, W*0.18), 0, W-1))
        ey = int(np.clip(rng.normal(fy, H*0.22), 0, H-1))
    else:
        ex, ey = rng.integers(0, W), rng.integers(0, H)
    s = rng.random()
    rad = 1 + int(s*2.2)
    bright = int(150 + s*105)
    ed.ellipse([ex-rad, ey-rad, ex+rad, ey+rad],
               fill=(bright, int(bright*0.55), int(bright*0.18)))
ember_glow = ember.filter(ImageFilter.GaussianBlur(3))
img = Image.fromarray(np.clip(np.array(img,np.float32) + np.array(ember_glow,np.float32)*0.9
                              + np.array(ember,np.float32), 0,255).astype('uint8'))

# 7) 헤이즈(대기 산란) ― 전체 살짝 들어올림
haze = np.array([60, 45, 42], np.float32)
arr = np.array(img, np.float32)
arr = arr*0.92 + haze*0.08
img = Image.fromarray(np.clip(arr,0,255).astype('uint8'))

# 8) 비네팅
vyy, vxx = np.mgrid[0:H, 0:W].astype(np.float32)
vr = np.sqrt(((vxx-W/2)/(W/2))**2 + ((vyy-H/2)/(H/2))**2)
vig = np.clip(1 - (vr-0.6)*0.9, 0.35, 1.0)
arr = np.array(img, np.float32) * vig[..., None]
img = Image.fromarray(np.clip(arr,0,255).astype('uint8'))

# 9) 필름 그레인 + 약한 샤픈
grain = (rng.random((H, W, 1))-0.5) * 10
img = Image.fromarray(np.clip(np.array(img,np.float32)+grain,0,255).astype('uint8'))
img = img.filter(ImageFilter.UnsharpMask(radius=2, percent=70, threshold=2))

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fireground.jpg")
img.save(out, quality=88)
print("saved", out, img.size)
