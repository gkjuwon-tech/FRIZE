# FRIZE 스튜디오 렌더 (Blender 5.x headless) ― 부품별 다중 재질
#   blender -b -P render_studio.py        (전체)
#   FRIZE_RENDER_ONLY=visor blender -b -P render_studio.py   (단일)
# STL을 loose-parts로 분리 → 크기/형상 휴리스틱으로 body/metal/glass 재질 자동 할당.
import bpy, os, math, mathutils

ROOT = os.path.dirname(os.path.abspath(__file__))
MESH = os.path.join(ROOT, "..", "mesh")
WEB  = os.environ.get("FRIZE_WEB", "") != ""      # 투명배경(웹용)
OUT  = os.path.join(ROOT, "..", "renders_web" if WEB else "renders_studio")
HDRI = os.path.join(ROOT, "hdri", "brown_photostudio_02_2k.hdr")
os.makedirs(OUT, exist_ok=True)
RES = (1600, 1600) if WEB else (1600, 1200); SAMPLES = 220
ONLY = os.environ.get("FRIZE_RENDER_ONLY", "")

def setup_render():
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = RES
    sc.render.film_transparent = WEB; sc.render.image_settings.file_format = 'PNG'
    if WEB: sc.render.image_settings.color_mode = 'RGBA'
    try: sc.view_settings.view_transform = 'AgX'
    except Exception: pass
    for lk in ('AgX - Punchy','AgX - Medium High Contrast','Medium High Contrast'):
        try: sc.view_settings.look = lk; break
        except Exception: continue
    sc.view_settings.exposure = -0.5
    try:
        prefs = bpy.context.preferences.addons['cycles'].preferences
        for dt in ('OPTIX','CUDA','HIP','ONEAPI','METAL'):
            try:
                prefs.compute_device_type = dt; prefs.get_devices()
                if any(d.type==dt for d in prefs.devices):
                    for d in prefs.devices: d.use=(d.type==dt or d.type=='CPU')
                    sc.cycles.device='GPU'; print("[render] GPU:",dt); return
            except Exception: continue
    except Exception: pass
    sc.cycles.device='CPU'; print("[render] CPU")

def clear_scene():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for blk in (bpy.data.meshes, bpy.data.materials, bpy.data.lights):
        for b in list(blk): blk.remove(b)

def setup_world():
    w=bpy.data.worlds.new("frize"); bpy.context.scene.world=w; w.use_nodes=True
    nt=w.node_tree; nt.nodes.clear()
    bg=nt.nodes.new("ShaderNodeBackground"); bg.inputs[1].default_value=0.5
    out=nt.nodes.new("ShaderNodeOutputWorld"); tex=nt.nodes.new("ShaderNodeTexEnvironment")
    if os.path.exists(HDRI): tex.image=bpy.data.images.load(HDRI)
    mp=nt.nodes.new("ShaderNodeMapping"); tc=nt.nodes.new("ShaderNodeTexCoord")
    mp.inputs['Rotation'].default_value[2]=math.radians(120)
    nt.links.new(tc.outputs['Generated'],mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'],tex.inputs['Vector'])
    nt.links.new(tex.outputs['Color'],bg.inputs[0]); nt.links.new(bg.outputs[0],out.inputs[0])

def setup_studio():
    if not WEB:                              # 웹(투명)모드는 바닥 생략 → 제품이 떠 보이게
        bpy.ops.mesh.primitive_plane_add(size=60, location=(0,0,0))
        m=bpy.data.materials.new("floor"); m.use_nodes=True; b=m.node_tree.nodes['Principled BSDF']
        b.inputs['Base Color'].default_value=(0.18,0.19,0.21,1); b.inputs['Roughness'].default_value=0.55
        bpy.context.object.data.materials.append(m)
    bpy.ops.object.light_add(type='AREA', location=(2.4,-2.0,3.4))
    k=bpy.context.object; k.data.energy=2200; k.data.size=5.0; k.rotation_euler=(math.radians(42),0,math.radians(38))
    bpy.ops.object.light_add(type='AREA', location=(-2.8,2.6,2.4))
    r=bpy.context.object; r.data.energy=1600; r.data.size=3.0; r.rotation_euler=(math.radians(62),0,math.radians(-150))
    bpy.ops.object.light_add(type='AREA', location=(-2.0,-2.4,1.6))
    f=bpy.context.object; f.data.energy=500; f.data.size=4.0; f.rotation_euler=(math.radians(70),0,math.radians(-40))

# ---------- 재질 ----------
def _principled(name):
    m=bpy.data.materials.new(name); m.use_nodes=True; return m, m.node_tree.nodes['Principled BSDF']
def mat_body(base):
    m,b=_principled("body"); b.inputs['Base Color'].default_value=(*base,1)
    b.inputs['Metallic'].default_value=0.18; b.inputs['Roughness'].default_value=0.42
    for c in ('Coat Weight','Clearcoat','Coat'):
        if c in b.inputs: b.inputs[c].default_value=0.35; break
    return m
def mat_metal():
    m,b=_principled("metal"); b.inputs['Base Color'].default_value=(0.62,0.63,0.66,1)
    b.inputs['Metallic'].default_value=0.95; b.inputs['Roughness'].default_value=0.34
    return m
def mat_glass():  # 꺼진 스크린/렌즈(다크 글래스)
    m,b=_principled("glass"); b.inputs['Base Color'].default_value=(0.012,0.015,0.022,1)
    b.inputs['Metallic'].default_value=0.0; b.inputs['Roughness'].default_value=0.07
    for c in ('Coat Weight','Clearcoat','Coat'):
        if c in b.inputs: b.inputs[c].default_value=0.6; break
    return m
def mat_accent():
    m,b=_principled("accent"); b.inputs['Base Color'].default_value=(0.45,0.05,0.04,1)
    b.inputs['Metallic'].default_value=0.1; b.inputs['Roughness'].default_value=0.38
    return m
def mat_rubber():
    m,b=_principled("rubber"); b.inputs['Base Color'].default_value=(0.02,0.02,0.025,1)
    b.inputs['Metallic'].default_value=0.0; b.inputs['Roughness'].default_value=0.7
    return m
def group_material(grp, base):
    return {"body":mat_body(base),"screen":mat_glass(),"metal":mat_metal(),
            "accent":mat_accent(),"rubber":mat_rubber()}.get(grp, mat_body(base))

# ---------- 임포트 + loose 분리 + 재질 ----------
def import_one(path):
    before=set(bpy.data.objects)
    try: bpy.ops.wm.stl_import(filepath=path)
    except Exception: bpy.ops.import_mesh.stl(filepath=path)
    objs=[o for o in bpy.data.objects if o not in before and o.type=='MESH']
    if not objs: return None
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    if len(objs)>1: bpy.ops.object.join()
    return bpy.context.object

def load_groups(name, base, groups):
    parts=[]
    for grp in groups:
        p=os.path.join(MESH, f"{name}_{grp}.stl")
        if not os.path.exists(p): continue
        obj=import_one(p)
        if not obj: continue
        obj.data.materials.clear(); obj.data.materials.append(group_material(grp, base))
        bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
        bpy.context.view_layer.objects.active=obj; bpy.ops.object.shade_smooth()
        parts.append(obj)
    return parts

def import_split(path):
    before=set(bpy.data.objects)
    try: bpy.ops.wm.stl_import(filepath=path)
    except Exception: bpy.ops.import_mesh.stl(filepath=path)
    objs=[o for o in bpy.data.objects if o not in before and o.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    if len(objs)>1: bpy.ops.object.join()
    obj=bpy.context.object
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
    return [o for o in bpy.context.selected_objects if o.type=='MESH']

def assign(parts, base):
    body=mat_body(base); metal=mat_metal(); glass=mat_glass(); accent=mat_accent()
    gmax=max(max(p.dimensions) for p in parts)
    for p in parts:
        d=sorted(p.dimensions); mn,mid,mx=d[0],d[1],d[2]
        area=mx*mid
        p.data.materials.clear()
        if mx>1e-6 and mn < 0.06*mx and area > 0.08*gmax*gmax:
            p.data.materials.append(glass)      # 얇고 넓은 패널 = 스크린/렌즈
        elif mx < 0.05*gmax:
            p.data.materials.append(metal)       # 진짜 작은 부품만 금속(나사/LED/베젤)
        else:
            p.data.materials.append(body)
        bpy.ops.object.select_all(action='DESELECT'); p.select_set(True); bpy.context.view_layer.objects.active=p
        bpy.ops.object.shade_smooth()

# ---------- 배치/카메라 (파트 그룹) ----------
def place_parts(parts, zrot):
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.transform.resize(value=(0.001,0.001,0.001)); bpy.ops.object.transform_apply(scale=True)
    bpy.ops.transform.rotate(value=math.radians(zrot), orient_axis='Z'); bpy.ops.object.transform_apply(rotation=True)
    minz=min(min((p.matrix_world @ mathutils.Vector(c)).z for c in p.bound_box) for p in parts)
    for p in parts: p.location.z -= minz

def frame_parts(parts, az, el, lens=72, margin=1.4):
    pts=[p.matrix_world @ mathutils.Vector(c) for p in parts for c in p.bound_box]
    center=sum(pts, mathutils.Vector((0,0,0)))/len(pts)
    rad=max((v-center).length for v in pts)
    cd=bpy.data.cameras.new("cam"); cd.lens=lens
    cam=bpy.data.objects.new("cam",cd); bpy.context.collection.objects.link(cam); bpy.context.scene.camera=cam
    dist=rad/math.tan(cd.angle/2)*margin
    a=math.radians(az); e=math.radians(el)
    cam.location=center+mathutils.Vector((math.cos(e)*math.sin(a),-math.cos(e)*math.cos(a),math.sin(e)))*dist
    cam.rotation_euler=(center-cam.location).normalized().to_track_quat('-Z','Y').to_euler()

JOBS=[
    dict(name="scout",   stl="scout.stl",   base=(0.020,0.022,0.028), az=42, el=26, zrot=25),
    dict(name="console", base=(0.050,0.052,0.060), az=20, el=14, zrot=0, lens=58,
         groups=["body","screen","metal","accent","rubber"]),   # 정면, 명시적 재질
    dict(name="visor",   stl="visor.stl",   base=(0.028,0.030,0.040), az=40, el=20, zrot=0),
    dict(name="vent",    stl="vent.stl",    base=(0.10,0.10,0.045),   az=40, el=22, zrot=20),
]

def main():
    setup_render()
    for j in JOBS:
        if ONLY and j["name"]!=ONLY: continue
        clear_scene(); setup_world(); setup_studio()
        if "groups" in j:
            parts=load_groups(j["name"], j["base"], j["groups"])
            if not parts: print("[skip] no group stl for",j["name"]); continue
        else:
            path=os.path.join(MESH,j["stl"])
            if not os.path.exists(path): print("[skip] no",path); continue
            parts=import_split(path); assign(parts, j["base"])
        place_parts(parts, j["zrot"]); frame_parts(parts, j["az"], j["el"], lens=j.get("lens",72))
        bpy.context.scene.render.filepath=os.path.join(OUT, j["name"]+".png")
        print("[render]", j["name"]); bpy.ops.render.render(write_still=True)
    print("[done] all renders")

main()
