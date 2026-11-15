"""Background Blender: a unit light-beam cone for searchlights. Apex at the origin, axis +X, length 1 m,
base radius 1 m (scaled in the game per beam angle and range). UV.x = distance along the beam (0..1)
so the material can fade it out. Exports Content/Assets/Enemies/FBX/SM_LightBeam.fbx."""
import bpy, bmesh, math
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
bm = bmesh.new()
N = 32
apex = bm.verts.new((0, 0, 0))
ring = [bm.verts.new((1.0, math.cos(2 * math.pi * i / N), math.sin(2 * math.pi * i / N))) for i in range(N)]
for i in range(N):
    bm.faces.new((apex, ring[i], ring[(i + 1) % N]))
uv = bm.loops.layers.uv.verify()
for f in bm.faces:
    for l in f.loops:
        l[uv].uv = (l.vert.co.x, 0.5)
me = bpy.data.meshes.new("SM_LightBeam")
bm.to_mesh(me); bm.free()
for poly in me.polygons:
    poly.use_smooth = True  # smooth normals: the material fades by N.V, flat faces would show as bands
mat = bpy.data.materials.new("MI_Nemesis_Beam")
me.materials.append(mat)
o = bpy.data.objects.new("SM_LightBeam", me)
bpy.context.scene.collection.objects.link(o)
o.select_set(True)
bpy.context.view_layer.objects.active = o
bpy.ops.export_scene.fbx(filepath="/Users/shmyha/Desktop/GameJam/guns-and-balloons/Content/Assets/Enemies/FBX/SM_LightBeam.fbx",
                         use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', bake_anim=False,
                         apply_unit_scale=True, path_mode='STRIP')
print("BEAM EXPORTED")
