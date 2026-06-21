# FRIZE 다크 제품샷 4종 (Blender 4.2 headless, Cycles)
#   콘솔 / 드론(SCOUT) / 고글(VISOR) / 배연포트(VENT) ― 다크 무드 스튜디오.
#   HDRI는 조명·반사용, 카메라가 보는 배경은 어둡게(Light Path 트릭),
#   반사 바닥 + 쿨 림라이트 + DOF + AgX. 합성 없음(콘솔 스크린=꺼진 다크글래스).
#   blender -b -P render_products_dark.py        (전체)
#   FRIZE_ONLY=scout blender -b -P render_products_dark.py
import bpy, os, math, mathutils

ROOT=os.path.dirname(os.path.abspath(__file__))
MESH=os.path.join(ROOT,"..","mesh")
OUT =os.path.join(ROOT,"..","renders_studio")
HDRI=os.path.join(ROOT,"hdri","brown_photostudio_02_2k.hdr")
os.makedirs(OUT,exist_ok=True)
RES=(1920,1280); SAMPLES=300
ONLY=os.environ.get("FRIZE_ONLY","")

def setup_render():
    sc=bpy.context.scene; sc.render.engine='CYCLES'
    sc.cycles.samples=SAMPLES; sc.cycles.use_denoising=True
    sc.render.resolution_x,sc.render.resolution_y=RES
    sc.render.image_settings.file_format='PNG'
    try: sc.view_settings.view_transform='AgX'
    except Exception: pass
    for lk in ('AgX - Medium High Contrast','Medium High Contrast'):
        try: sc.view_settings.look=lk; break
        except Exception: continue
    sc.view_settings.exposure=-0.35
    try:
        prefs=bpy.context.preferences.addons['cycles'].preferences
        for dt in ('OPTIX','CUDA','HIP','ONEAPI'):
            try:
                prefs.compute_device_type=dt; prefs.get_devices()
                if any(d.type==dt for d in prefs.devices):
                    for d in prefs.devices: d.use=(d.type==dt or d.type=='CPU')
                    sc.cycles.device='GPU'; print("[render] GPU",dt); return
            except Exception: continue
    except Exception: pass
    sc.cycles.device='CPU'; print("[render] CPU")

def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for blk in (bpy.data.meshes,bpy.data.materials,bpy.data.lights,bpy.data.worlds,bpy.data.cameras):
        for b in list(blk):
            try: blk.remove(b)
            except Exception: pass

def setup_world():
    # HDRI=조명/반사, 카메라 배경=다크(거의 블랙, 미세 청색)
    w=bpy.data.worlds.new("frize"); bpy.context.scene.world=w; w.use_nodes=True
    nt=w.node_tree; nt.nodes.clear()
    out=nt.nodes.new("ShaderNodeOutputWorld")
    tex=nt.nodes.new("ShaderNodeTexEnvironment")
    if os.path.exists(HDRI): tex.image=bpy.data.images.load(HDRI)
    mp=nt.nodes.new("ShaderNodeMapping"); tc=nt.nodes.new("ShaderNodeTexCoord")
    mp.inputs['Rotation'].default_value[2]=math.radians(95)
    nt.links.new(tc.outputs['Generated'],mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'],tex.inputs['Vector'])
    lite=nt.nodes.new("ShaderNodeBackground"); lite.inputs[1].default_value=0.28
    nt.links.new(tex.outputs['Color'],lite.inputs[0])
    dark=nt.nodes.new("ShaderNodeBackground"); dark.inputs[0].default_value=(0.006,0.008,0.011,1); dark.inputs[1].default_value=1.0
    lp=nt.nodes.new("ShaderNodeLightPath"); mix=nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(lp.outputs['Is Camera Ray'],mix.inputs[0])
    nt.links.new(lite.outputs[0],mix.inputs[1]); nt.links.new(dark.outputs[0],mix.inputs[2])
    nt.links.new(mix.outputs[0],out.inputs[0])

def floor():
    bpy.ops.mesh.primitive_plane_add(size=120, location=(0,0,0))
    o=bpy.context.active_object; m=bpy.data.materials.new("floor"); m.use_nodes=True
    b=m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value=(0.010,0.011,0.013,1)
    b.inputs['Roughness'].default_value=0.09; b.inputs['Metallic'].default_value=0.0
    o.data.materials.append(m)

def lights(center, rad):
    # 키(따뜻) + 쿨 림(뒤) + 약한 필
    def area(loc,energy,size,color):
        l=bpy.data.lights.new("L",'AREA'); l.energy=energy; l.size=size; l.color=color
        o=bpy.data.objects.new("L",l); bpy.context.collection.objects.link(o); o.location=loc
        o.rotation_euler=(center-mathutils.Vector(loc)).to_track_quat('-Z','Y').to_euler(); return o
    s=rad*2.2
    area(center+mathutils.Vector((-1.0,-1.3,1.5))*s, 1700*rad*rad, 1.4*rad, (1.0,0.92,0.80))  # 키(따뜻)
    area(center+mathutils.Vector(( 1.4, 1.2,0.8))*s, 1500*rad*rad, 1.1*rad, (0.55,0.70,1.0))  # 쿨 림(엣지 글로)
    area(center+mathutils.Vector(( 1.5,-1.1,0.35))*s, 300*rad*rad, 2.0*rad, (1.0,0.84,0.66))  # 약한 필

# ── 재질 ──
def _p(name):
    m=bpy.data.materials.new(name); m.use_nodes=True; return m,m.node_tree.nodes['Principled BSDF']
def _coat(b,v):
    for c in ('Coat Weight','Coat','Clearcoat'):
        if c in b.inputs: b.inputs[c].default_value=v; break
def mat_body(base):
    m,b=_p("body"); b.inputs['Base Color'].default_value=(*base,1)
    b.inputs['Metallic'].default_value=0.0; b.inputs['Roughness'].default_value=0.52; _coat(b,0.18); return m
def mat_metal():
    m,b=_p("metal"); b.inputs['Base Color'].default_value=(0.64,0.66,0.69,1)
    b.inputs['Metallic'].default_value=1.0; b.inputs['Roughness'].default_value=0.30; return m
def mat_glass():
    m,b=_p("glass"); b.inputs['Base Color'].default_value=(0.010,0.012,0.018,1)
    b.inputs['Metallic'].default_value=0.0; b.inputs['Roughness'].default_value=0.05; _coat(b,0.7); return m
def mat_accent():
    m,b=_p("accent"); b.inputs['Base Color'].default_value=(0.50,0.06,0.045,1)
    b.inputs['Metallic'].default_value=0.1; b.inputs['Roughness'].default_value=0.34; _coat(b,0.3); return m
def mat_rubber():
    m,b=_p("rubber"); b.inputs['Base Color'].default_value=(0.015,0.015,0.018,1)
    b.inputs['Metallic'].default_value=0.0; b.inputs['Roughness'].default_value=0.8; return m
def group_mat(g,base):
    return {"body":mat_body(base),"screen":mat_glass(),"metal":mat_metal(),
            "accent":mat_accent(),"rubber":mat_rubber()}.get(g,mat_body(base))

# ── 임포트 ──
def _import(path):
    before=set(bpy.data.objects)
    try: bpy.ops.wm.stl_import(filepath=path)
    except Exception: bpy.ops.import_mesh.stl(filepath=path)
    objs=[o for o in bpy.data.objects if o not in before and o.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    if len(objs)>1: bpy.ops.object.join()
    return bpy.context.object

def load_groups(name,base,groups):
    parts=[]
    for g in groups:
        p=os.path.join(MESH,f"{name}_{g}.stl")
        if not os.path.exists(p): continue
        o=_import(p); o.data.materials.clear(); o.data.materials.append(group_mat(g,base))
        bpy.ops.object.select_all(action='DESELECT'); o.select_set(True)
        bpy.context.view_layer.objects.active=o; bpy.ops.object.shade_smooth(); parts.append(o)
    return parts

def load_split(path,base):
    o=_import(path)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
    parts=[x for x in bpy.context.selected_objects if x.type=='MESH']
    body=mat_body(base); metal=mat_metal(); glass=mat_glass()
    gmax=max(max(p.dimensions) for p in parts)
    for p in parts:
        d=sorted(p.dimensions); mn,mid,mx=d[0],d[1],d[2]; area=mx*mid
        p.data.materials.clear()
        if mx>1e-6 and mn<0.06*mx and area>0.08*gmax*gmax: p.data.materials.append(glass)
        elif mx<0.05*gmax: p.data.materials.append(metal)
        else: p.data.materials.append(body)
        bpy.ops.object.select_all(action='DESELECT'); p.select_set(True); bpy.context.view_layer.objects.active=p
        bpy.ops.object.shade_smooth()
    return parts

def place(parts,zrot):
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.transform.resize(value=(0.001,0.001,0.001)); bpy.ops.object.transform_apply(scale=True)
    bpy.ops.transform.rotate(value=math.radians(zrot),orient_axis='Z'); bpy.ops.object.transform_apply(rotation=True)
    minz=min(min((p.matrix_world@mathutils.Vector(c)).z for c in p.bound_box) for p in parts)
    for p in parts: p.location.z-=minz

def frame(parts,az,el,lens,margin):
    pts=[p.matrix_world@mathutils.Vector(c) for p in parts for c in p.bound_box]
    ctr=sum(pts,mathutils.Vector((0,0,0)))/len(pts)
    rad=max((v-ctr).length for v in pts)
    cd=bpy.data.cameras.new("cam"); cd.lens=lens
    cam=bpy.data.objects.new("cam",cd); bpy.context.collection.objects.link(cam); bpy.context.scene.camera=cam
    dist=rad/math.tan(cd.angle/2)*margin
    a=math.radians(az); e=math.radians(el)
    cam.location=ctr+mathutils.Vector((math.cos(e)*math.sin(a),-math.cos(e)*math.cos(a),math.sin(e)))*dist
    cam.rotation_euler=(ctr-cam.location).normalized().to_track_quat('-Z','Y').to_euler()
    cd.dof.use_dof=True; cd.dof.focus_distance=(ctr-cam.location).length; cd.dof.aperture_fstop=4.0
    return ctr,rad

JOBS=[
    dict(name="console", base=(0.045,0.048,0.056), az=22, el=15, zrot=0, lens=60, margin=1.5,
         groups=["body","screen","metal","accent","rubber"]),
    dict(name="scout",   stl="scout.stl", base=(0.018,0.020,0.026), az=46, el=17, zrot=25, lens=84, margin=1.18),
    dict(name="visor",   stl="visor.stl", base=(0.024,0.026,0.034), az=40, el=18, zrot=0,  lens=80, margin=1.4),
    dict(name="vent",    stl="vent.stl",  base=(0.030,0.032,0.030), az=42, el=20, zrot=20, lens=80, margin=1.30),
]

def main():
    setup_render()
    for j in JOBS:
        if ONLY and j["name"]!=ONLY: continue
        clear(); setup_world(); floor()
        if "groups" in j:
            parts=load_groups(j["name"],j["base"],j["groups"])
            if not parts: print("[skip]",j["name"]); continue
        else:
            p=os.path.join(MESH,j["stl"])
            if not os.path.exists(p): print("[skip] no",p); continue
            parts=load_split(p,j["base"])
        place(parts,j["zrot"]); ctr,rad=frame(parts,j["az"],j["el"],j["lens"],j["margin"])
        lights(ctr,rad)
        bpy.context.scene.render.filepath=os.path.join(OUT,j["name"]+"_product.png")
        print("[render]",j["name"]); bpy.ops.render.render(write_still=True)
    print("[done]")

main()
