# 콕핏 배경용 디지털 트윈 렌더 ― 순수 검정 배경, 16:9 풀블리드, 화점 발광.
#   blender -b -P render_twin_cockpit.py -- [twin.ply] [out.png]
import bpy, os, sys, math, mathutils, bmesh
ROOT=os.path.dirname(os.path.abspath(__file__))
argv=sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else []
PLY=argv[0] if argv else "/tmp/dashtwin/twin.ply"
OUT=argv[1] if len(argv)>1 else os.path.join(ROOT,"..","renders_studio","twin_cockpit.png")
RES=(1920,1080); SAMPLES=200; CUT_Z=6.45    # 지붕만 제거(3층 내부 다 보이게)

def main():
    sc=bpy.context.scene; sc.render.engine='CYCLES'; sc.cycles.samples=SAMPLES; sc.cycles.use_denoising=True
    sc.render.resolution_x,sc.render.resolution_y=RES; sc.render.image_settings.file_format='PNG'
    sc.cycles.device='CPU'
    try: sc.view_settings.view_transform='AgX'
    except: pass
    for lk in ('AgX - Medium High Contrast','Medium High Contrast'):
        try: sc.view_settings.look=lk; break
        except: continue
    sc.view_settings.exposure=-0.4
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for blk in (bpy.data.meshes,bpy.data.materials,bpy.data.worlds,bpy.data.lights,bpy.data.cameras):
        for b in list(blk):
            try: blk.remove(b)
            except: pass
    # 순수 검정 월드(미세 청색 앰비언트만)
    w=bpy.data.worlds.new("w"); sc.world=w; w.use_nodes=True; nt=w.node_tree; nt.nodes.clear()
    o=nt.nodes.new("ShaderNodeOutputWorld"); bg=nt.nodes.new("ShaderNodeBackground")
    bg.inputs[0].default_value=(0.006,0.010,0.018,1); bg.inputs[1].default_value=0.25
    nt.links.new(bg.outputs[0],o.inputs[0])
    # 트윈 임포트 + 지붕 컷
    try: bpy.ops.wm.ply_import(filepath=PLY)
    except: bpy.ops.import_mesh.ply(filepath=PLY)
    ob=bpy.context.selected_objects[0]
    me=ob.data; bm=bmesh.new(); bm.from_mesh(me)
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if sum(v.co.z for v in f.verts)/len(f.verts)>CUT_Z],context='FACES')
    bm.to_mesh(me); bm.free(); me.update(); bpy.ops.object.shade_smooth()
    minz=min((ob.matrix_world@v.co).z for v in me.vertices); ob.location.z-=minz
    # 머티리얼: 정점색 베이스 + 열(R-B) 발광
    m=bpy.data.materials.new("twin"); m.use_nodes=True; nt=m.node_tree; nt.nodes.clear()
    out=nt.nodes.new("ShaderNodeOutputMaterial"); bsdf=nt.nodes.new("ShaderNodeBsdfPrincipled")
    vc=nt.nodes.new("ShaderNodeVertexColor")
    if me.color_attributes: vc.layer_name=me.color_attributes[0].name
    dk=nt.nodes.new("ShaderNodeMixRGB"); dk.blend_type='MULTIPLY'; dk.inputs[0].default_value=1; dk.inputs[2].default_value=(0.55,0.6,0.68,1)
    nt.links.new(vc.outputs['Color'],dk.inputs[1]); nt.links.new(dk.outputs[0],bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value=0.7
    sep=nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(vc.outputs['Color'],sep.inputs['Color'])
    sub=nt.nodes.new("ShaderNodeMath"); sub.operation='SUBTRACT'
    nt.links.new(sep.outputs['Red'],sub.inputs[0]); nt.links.new(sep.outputs['Blue'],sub.inputs[1])
    mx=nt.nodes.new("ShaderNodeMath"); mx.operation='MAXIMUM'; mx.inputs[1].default_value=0
    mu=nt.nodes.new("ShaderNodeMath"); mu.operation='MULTIPLY'; mu.inputs[1].default_value=11
    nt.links.new(sub.outputs[0],mx.inputs[0]); nt.links.new(mx.outputs[0],mu.inputs[0])
    nt.links.new(vc.outputs['Color'],bsdf.inputs['Emission Color']); nt.links.new(mu.outputs[0],bsdf.inputs['Emission Strength'])
    nt.links.new(bsdf.outputs[0],out.inputs[0]); me.materials.clear(); me.materials.append(m)
    # 쿨 림 라이트 2개(검정 위 형태 분리)
    pts=[ob.matrix_world@v.co for v in me.vertices]; ctr=sum(pts,mathutils.Vector((0,0,0)))/len(pts)
    rad=max((v-ctr).length for v in pts)
    for loc,col,e in [((-1.2,1.4,2.0),(0.5,0.66,1.0),0.9),((1.6,-0.8,1.3),(0.55,0.7,1.0),0.5),((0.2,0.2,2.4),(0.7,0.8,1.0),0.4)]:
        l=bpy.data.lights.new("L",'AREA'); l.energy=e*420*rad*rad; l.size=2.4*rad; l.color=col
        oo=bpy.data.objects.new("L",l); bpy.context.collection.objects.link(oo)
        oo.location=ctr+mathutils.Vector(loc)*rad
        oo.rotation_euler=(ctr-oo.location).to_track_quat('-Z','Y').to_euler()
    # 카메라: 와이드 3/4 부감, 화면 채우게
    cd=bpy.data.cameras.new("cam"); cd.lens=38
    cam=bpy.data.objects.new("cam",cd); bpy.context.collection.objects.link(cam); sc.camera=cam
    az,el=math.radians(-54),math.radians(40)
    cam.location=ctr+mathutils.Vector((math.cos(el)*math.sin(az),-math.cos(el)*math.cos(az),math.sin(el)))*rad*1.75
    cam.rotation_euler=(ctr-cam.location).normalized().to_track_quat('-Z','Y').to_euler()
    cd.dof.use_dof=True; cd.dof.focus_distance=(ctr-cam.location).length; cd.dof.aperture_fstop=4.0
    sc.render.filepath=OUT; print("[twin-cockpit]",OUT); bpy.ops.render.render(write_still=True); print("[done]")

main()
