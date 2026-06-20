# FRIZE 디지털 트윈 히어로 렌더 (Blender 4.2 headless, Cycles)
#   재구성 메쉬(twin.ply, 정점색=열화상)를 '진짜 같은' 다크 트윈으로 렌더:
#   콘크리트풍 PBR + 화점 발광(emission) + 홀로그래픽 그리드 바닥 + 안개/갓레이.
#   blender -b -P render_twin_hero.py -- [twin.ply] [out.png]
import bpy, os, sys, math, mathutils, bmesh

ROOT=os.path.dirname(os.path.abspath(__file__))
OUT_DEFAULT=os.path.join(ROOT,"..","renders_studio","twin_hero.png")
HDRI=os.path.join(ROOT,"hdri","studio_small_08_2k.hdr")
argv=sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else []
PLY=argv[0] if len(argv)>0 else "/tmp/dashtwin/twin.ply"
OUT=argv[1] if len(argv)>1 else OUT_DEFAULT
RES=(1920,1280); SAMPLES=340; CUT_Z=2.72     # 지붕/2층 컷 → 1층 내부 노출

def setup_render():
    sc=bpy.context.scene; sc.render.engine='CYCLES'
    sc.cycles.samples=SAMPLES; sc.cycles.use_denoising=True
    sc.cycles.volume_bounces=2
    sc.render.resolution_x,sc.render.resolution_y=RES
    sc.render.image_settings.file_format='PNG'
    try: sc.view_settings.view_transform='AgX'
    except Exception: pass
    for lk in ('AgX - Medium High Contrast','Medium High Contrast'):
        try: sc.view_settings.look=lk; break
        except Exception: continue
    sc.view_settings.exposure=-0.2
    sc.cycles.device='CPU'; print("[render] CPU")

def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for blk in (bpy.data.meshes,bpy.data.materials,bpy.data.lights,bpy.data.worlds,bpy.data.cameras):
        for b in list(blk):
            try: blk.remove(b)
            except Exception: pass

def world_dark():
    w=bpy.data.worlds.new("w"); bpy.context.scene.world=w; w.use_nodes=True
    nt=w.node_tree; nt.nodes.clear()
    out=nt.nodes.new("ShaderNodeOutputWorld"); bg=nt.nodes.new("ShaderNodeBackground")
    bg.inputs[0].default_value=(0.012,0.018,0.030,1); bg.inputs[1].default_value=0.35  # 미세 청색 앰비언트
    nt.links.new(bg.outputs[0],out.inputs[0])

def import_twin():
    try: bpy.ops.wm.ply_import(filepath=PLY)
    except Exception: bpy.ops.import_mesh.ply(filepath=PLY)
    o=bpy.context.selected_objects[0]; o.name="twin"
    # 지붕/2층 컷
    me=o.data; bm=bmesh.new(); bm.from_mesh(me)
    todel=[f for f in bm.faces if (sum((v.co.z for v in f.verts))/len(f.verts))>CUT_Z]
    bmesh.ops.delete(bm,geom=todel,context='FACES')
    bm.to_mesh(me); bm.free(); me.update()
    bpy.ops.object.shade_smooth()
    # 바닥에 안착
    minz=min((o.matrix_world@v.co).z for v in me.vertices)
    o.location.z-=minz
    return o

def mat_twin(o):
    # 정점색(열) → 콘크리트 베이스 + 따뜻한 부분만 발광
    m=bpy.data.materials.new("twin"); m.use_nodes=True; nt=m.node_tree; nt.nodes.clear()
    out=nt.nodes.new("ShaderNodeOutputMaterial"); bsdf=nt.nodes.new("ShaderNodeBsdfPrincipled")
    vc=nt.nodes.new("ShaderNodeVertexColor")
    if o.data.color_attributes: vc.layer_name=o.data.color_attributes[0].name
    # base: 정점색을 살짝 어둡게(콘크리트)
    darken=nt.nodes.new("ShaderNodeMixRGB"); darken.blend_type='MULTIPLY'; darken.inputs[0].default_value=1.0
    darken.inputs[2].default_value=(0.72,0.74,0.78,1)
    nt.links.new(vc.outputs['Color'],darken.inputs[1])
    nt.links.new(darken.outputs[0],bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value=0.62; bsdf.inputs['Metallic'].default_value=0.0
    # heat = R - B  → 화점/요구조자만 양수 → emission strength
    sep=nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(vc.outputs['Color'],sep.inputs['Color'])
    sub=nt.nodes.new("ShaderNodeMath"); sub.operation='SUBTRACT'
    nt.links.new(sep.outputs['Red'],sub.inputs[0]); nt.links.new(sep.outputs['Blue'],sub.inputs[1])
    heat=nt.nodes.new("ShaderNodeMath"); heat.operation='MULTIPLY'; heat.inputs[1].default_value=9.0
    mx=nt.nodes.new("ShaderNodeMath"); mx.operation='MAXIMUM'; mx.inputs[1].default_value=0.0
    nt.links.new(sub.outputs[0],mx.inputs[0]); nt.links.new(mx.outputs[0],heat.inputs[0])
    nt.links.new(vc.outputs['Color'],bsdf.inputs['Emission Color'])
    nt.links.new(heat.outputs[0],bsdf.inputs['Emission Strength'])
    nt.links.new(bsdf.outputs[0],out.inputs[0])
    o.data.materials.clear(); o.data.materials.append(m)

def holo_floor(size):
    bpy.ops.mesh.primitive_plane_add(size=size, location=(0,0,-0.02))
    o=bpy.context.active_object; m=bpy.data.materials.new("holo"); m.use_nodes=True
    nt=m.node_tree; nt.nodes.clear(); out=nt.nodes.new("ShaderNodeOutputMaterial")
    # 베이스 다크 + 발광 그리드(시안)
    tc=nt.nodes.new("ShaderNodeTexCoord"); mp=nt.nodes.new("ShaderNodeMapping")
    mp.inputs['Scale'].default_value=(size,size,size)
    nt.links.new(tc.outputs['Generated'],mp.inputs['Vector'])
    sep=nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(mp.outputs['Vector'],sep.inputs['Vector'])
    def lines(axis):
        fr=nt.nodes.new("ShaderNodeMath"); fr.operation='FRACT'; nt.links.new(sep.outputs[axis],fr.inputs[0])
        d=nt.nodes.new("ShaderNodeMath"); d.operation='ABSOLUTE'
        s=nt.nodes.new("ShaderNodeMath"); s.operation='SUBTRACT'; s.inputs[1].default_value=0.5
        nt.links.new(fr.outputs[0],s.inputs[0]); nt.links.new(s.outputs[0],d.inputs[0])
        th=nt.nodes.new("ShaderNodeMath"); th.operation='LESS_THAN'; th.inputs[1].default_value=0.015
        nt.links.new(d.outputs[0],th.inputs[0]); return th
    lx=lines('X'); ly=lines('Y')
    grid=nt.nodes.new("ShaderNodeMath"); grid.operation='MAXIMUM'
    nt.links.new(lx.outputs[0],grid.inputs[0]); nt.links.new(ly.outputs[0],grid.inputs[1])
    emi=nt.nodes.new("ShaderNodeEmission"); emi.inputs['Color'].default_value=(0.18,0.55,0.75,1)
    estr=nt.nodes.new("ShaderNodeMath"); estr.operation='MULTIPLY'; estr.inputs[1].default_value=1.6
    nt.links.new(grid.outputs[0],estr.inputs[0]); nt.links.new(estr.outputs[0],emi.inputs['Strength'])
    diff=nt.nodes.new("ShaderNodeBsdfPrincipled"); diff.inputs['Base Color'].default_value=(0.01,0.012,0.015,1)
    diff.inputs['Roughness'].default_value=0.25
    mix=nt.nodes.new("ShaderNodeAddShader")
    nt.links.new(diff.outputs[0],mix.inputs[0]); nt.links.new(emi.outputs[0],mix.inputs[1])
    nt.links.new(mix.outputs[0],out.inputs[0]); o.data.materials.append(m)

def haze(center,rad):
    # 큰 도메인에 옅은 볼륨 스캐터 → 화점 갓레이/대기감
    bpy.ops.mesh.primitive_cube_add(size=rad*5, location=(center.x,center.y,center.z))
    o=bpy.context.active_object; m=bpy.data.materials.new("haze"); m.use_nodes=True
    nt=m.node_tree; nt.nodes.clear(); out=nt.nodes.new("ShaderNodeOutputMaterial")
    vs=nt.nodes.new("ShaderNodeVolumeScatter"); vs.inputs['Density'].default_value=0.012
    vs.inputs['Color'].default_value=(0.6,0.72,0.9,1)
    nt.links.new(vs.outputs[0],out.inputs['Volume']); o.data.materials.append(m)

def lights(center,rad):
    def area(loc,e,sz,col):
        l=bpy.data.lights.new("L",'AREA'); l.energy=e; l.size=sz; l.color=col
        ob=bpy.data.objects.new("L",l); bpy.context.collection.objects.link(ob); ob.location=loc
        ob.rotation_euler=(center-mathutils.Vector(loc)).to_track_quat('-Z','Y').to_euler()
    area(center+mathutils.Vector((-1.2, 1.4,2.2))*rad, 600*rad*rad, 2*rad,(0.55,0.7,1.0))   # 쿨 키(상단)
    area(center+mathutils.Vector(( 1.6,-1.0,1.4))*rad, 300*rad*rad, 2*rad,(0.6,0.74,1.0))    # 쿨 림

def main():
    setup_render(); clear(); world_dark()
    o=import_twin(); mat_twin(o)
    pts=[o.matrix_world@v.co for v in o.data.vertices]
    ctr=sum(pts,mathutils.Vector((0,0,0)))/len(pts)
    rad=max((v-ctr).length for v in pts)
    holo_floor(max(40,rad*3)); haze(ctr,rad); lights(ctr,rad)
    cd=bpy.data.cameras.new("cam"); cd.lens=42
    cam=bpy.data.objects.new("cam",cd); bpy.context.collection.objects.link(cam); bpy.context.scene.camera=cam
    az,el=math.radians(-52),math.radians(38)
    cam.location=ctr+mathutils.Vector((math.cos(el)*math.sin(az),-math.cos(el)*math.cos(az),math.sin(el)))*rad*2.1
    cam.rotation_euler=(ctr-cam.location).normalized().to_track_quat('-Z','Y').to_euler()
    cd.dof.use_dof=True; cd.dof.focus_distance=(ctr-cam.location).length; cd.dof.aperture_fstop=3.2
    bpy.context.scene.render.filepath=OUT
    print("[render] twin hero →",OUT); bpy.ops.render.render(write_still=True); print("[done]")

main()
