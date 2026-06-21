# FRIZE CONSOLE-1 제품샷 (Blender 4.2 headless, Cycles)
#   콘솔 STL(재질그룹) → PBR + 스크린에 콕핏 대시보드 emissive 텍스처 +
#   HDRI 조명 + '다크 배경'(Light Path: 카메라레이만 어둡게) → 진짜 제품샷.
#   blender -b -P render_console_product.py
import bpy, os, math, mathutils

ROOT = os.path.dirname(os.path.abspath(__file__))
MESH = os.path.join(ROOT, "..", "mesh")
OUT  = os.path.join(ROOT, "..", "renders_studio")
HDRI = os.path.join(ROOT, "hdri", "brown_photostudio_02_2k.hdr")
DASH = os.path.join(ROOT, "..", "..", "software", "apps", "frize_cockpit", "assets", "dashboard.png")
os.makedirs(OUT, exist_ok=True)
S = 0.01                       # mm → (작업단위) 스케일
RES = (1920, 1280); SAMPLES = 320

C_BODY=(0.045,0.05,0.058); C_METAL=(0.62,0.64,0.67); C_ACCENT=(0.55,0.07,0.05)
C_RUBBER=(0.012,0.012,0.014); C_SCREEN=(0.02,0.022,0.027)

def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for blk in (bpy.data.meshes,bpy.data.materials,bpy.data.lights,bpy.data.images,bpy.data.worlds):
        for b in list(blk):
            try: blk.remove(b)
            except Exception: pass

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
    sc.view_settings.exposure=-0.2
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

def setup_world():
    # HDRI로 조명/반사, 카메라가 보는 배경만 어둡게(Light Path Is Camera Ray)
    w=bpy.data.worlds.new("frize"); bpy.context.scene.world=w; w.use_nodes=True
    nt=w.node_tree; nt.nodes.clear()
    out=nt.nodes.new("ShaderNodeOutputWorld")
    tex=nt.nodes.new("ShaderNodeTexEnvironment")
    if os.path.exists(HDRI): tex.image=bpy.data.images.load(HDRI)
    mp=nt.nodes.new("ShaderNodeMapping"); tc=nt.nodes.new("ShaderNodeTexCoord")
    mp.inputs['Rotation'].default_value[2]=math.radians(70)
    nt.links.new(tc.outputs['Generated'],mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'],tex.inputs['Vector'])
    light=nt.nodes.new("ShaderNodeBackground"); light.inputs[1].default_value=0.55
    nt.links.new(tex.outputs['Color'],light.inputs[0])
    dark=nt.nodes.new("ShaderNodeBackground"); dark.inputs[0].default_value=(0.012,0.014,0.018,1); dark.inputs[1].default_value=1.0
    lp=nt.nodes.new("ShaderNodeLightPath"); mix=nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(lp.outputs['Is Camera Ray'],mix.inputs[0])
    nt.links.new(light.outputs[0],mix.inputs[1])   # 비카메라(조명/반사)=HDRI
    nt.links.new(dark.outputs[0],mix.inputs[2])    # 카메라 배경=다크
    nt.links.new(mix.outputs[0],out.inputs[0])

def mat(name,base,metallic=0.0,rough=0.5,coat=0.0):
    m=bpy.data.materials.new(name); m.use_nodes=True; b=m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value=(*base,1); b.inputs['Metallic'].default_value=metallic
    b.inputs['Roughness'].default_value=rough
    for k,v in (('Coat Weight',coat),):
        if k in b.inputs: b.inputs[k].default_value=v
    return m

def import_group(g,material):
    p=os.path.join(MESH,f"console_{g}.stl")
    if not os.path.exists(p): return None
    try: bpy.ops.wm.stl_import(filepath=p)
    except Exception: bpy.ops.import_mesh.stl(filepath=p)
    o=bpy.context.selected_objects[0]; o.name=f"console_{g}"
    o.scale=(S,S,S); bpy.ops.object.transform_apply(scale=True)
    o.data.materials.clear(); o.data.materials.append(material)
    bpy.ops.object.shade_smooth();
    try: o.data.use_auto_smooth=True
    except Exception: pass
    for mo in o.modifiers: pass
    return o

def add_screen_plane():
    # SCAD 스크린 디스플레이 4코너(mm) → 월드(스케일 적용). 디스플레이는 -Y를 향함.
    a=math.radians(-12); T1=mathutils.Vector((0,154,72))
    def rotx(p):
        x,y,z=p; return mathutils.Vector((x, y*math.cos(a)-z*math.sin(a), y*math.sin(a)+z*math.cos(a)))
    # y를 -13으로 약간 앞에 띄워 베젤 위에 디스플레이가 올라오게
    loc=[(-238,-13,254),(238,-13,254),(238,-13,20),(-238,-13,20)]   # TL,TR,BR,BL
    verts=[ (rotx(p)+T1)*S for p in loc ]
    me=bpy.data.meshes.new("screen_disp"); me.from_pydata([tuple(v) for v in verts],[],[(0,1,2,3)]); me.update()
    uvl=me.uv_layers.new(name="uv")
    for li,uv in zip(me.loops, [(0,1),(1,1),(1,0),(0,0)]): uvl.data[li.index].uv=uv
    ob=bpy.data.objects.new("screen_disp",me); bpy.context.collection.objects.link(ob)
    m=bpy.data.materials.new("dash"); m.use_nodes=True; nt=m.node_tree; nt.nodes.clear()
    out=nt.nodes.new("ShaderNodeOutputMaterial"); emi=nt.nodes.new("ShaderNodeEmission")
    img=nt.nodes.new("ShaderNodeTexImage")
    if os.path.exists(DASH): img.image=bpy.data.images.load(DASH); img.image.colorspace_settings.name='sRGB'
    emi.inputs['Strength'].default_value=2.6
    nt.links.new(img.outputs['Color'],emi.inputs['Color']); nt.links.new(emi.outputs[0],out.inputs[0])
    ob.data.materials.append(m)
    # 디스플레이 위 얇은 글래스(반사) 한 겹
    return ob

def add_floor():
    bpy.ops.mesh.primitive_plane_add(size=80, location=(0,0,-0.001))
    o=bpy.context.active_object
    o.data.materials.append(mat("floor",(0.02,0.022,0.026),metallic=0.0,rough=0.18))

def add_camera_lights():
    cam_d=bpy.data.cameras.new("cam"); cam=bpy.data.objects.new("cam",cam_d); bpy.context.collection.objects.link(cam)
    bpy.context.scene.camera=cam
    cam.location=(3.15,-9.4,4.55); cam_d.lens=58
    tgt=mathutils.Vector((0.0,0.45,0.95))
    cam.rotation_euler=(tgt-mathutils.Vector(cam.location)).to_track_quat('-Z','Y').to_euler()
    cam_d.dof.use_dof=True; cam_d.dof.focus_distance=(tgt-mathutils.Vector(cam.location)).length; cam_d.dof.aperture_fstop=4.5
    # 키 라이트(전상방) + 림(후측) ― HDRI 보조
    key=bpy.data.lights.new("key",'AREA'); key.energy=900; key.size=4
    ko=bpy.data.objects.new("key",key); ko.location=(-3,-4,6); bpy.context.collection.objects.link(ko)
    ko.rotation_euler=(mathutils.Vector((0,0.4,1))-mathutils.Vector(ko.location)).to_track_quat('-Z','Y').to_euler()
    rim=bpy.data.lights.new("rim",'AREA'); rim.energy=500; rim.size=3; rim.color=(0.6,0.72,1.0)
    ro=bpy.data.objects.new("rim",rim); ro.location=(4,5,4); bpy.context.collection.objects.link(ro)
    ro.rotation_euler=(mathutils.Vector((0,0.4,1))-mathutils.Vector(ro.location)).to_track_quat('-Z','Y').to_euler()

def main():
    clear(); setup_render(); setup_world()
    import_group("body",  mat("body",C_BODY,metallic=0.0,rough=0.46,coat=0.15))
    import_group("metal", mat("metal",C_METAL,metallic=1.0,rough=0.34))
    import_group("accent",mat("accent",C_ACCENT,metallic=0.0,rough=0.36,coat=0.4))
    import_group("rubber",mat("rubber",C_RUBBER,metallic=0.0,rough=0.85))
    import_group("screen",mat("screen",C_SCREEN,metallic=0.0,rough=0.18))
    add_screen_plane(); add_floor(); add_camera_lights()
    bpy.context.scene.render.filepath=os.path.join(OUT,"console_product.png")
    print("[render] 렌더 시작 → console_product.png")
    bpy.ops.render.render(write_still=True)
    print("[render] 완료")

main()
