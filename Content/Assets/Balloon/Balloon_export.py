"""Turns the "AirBalloon" source scene (built by Balloon_build.py) into the game asset.

Creates scene "AirBalloon_Game" with:
  Armature   - skeleton: one bone per interactive/animated part + item socket bones
  SK_Balloon - one mesh, every part rigidly skinned (weight 1.0) to its bone, one UV atlas
and writes into the asset folder:
  SK_Balloon.fbx            skeletal mesh for Unreal
  T_Balloon_BaseColor.png   1024 atlas, ~1.5 cm per texel, AO grime + ordered dither (sRGB)
  T_Balloon_ORM.png         R = ambient occlusion, G = roughness, B = metallic (linear)
  T_Balloon_Radar.png       radar CRT (balloon icon, 3 range rings), fills the radar disc's 0..1 UV (sRGB)
Materials: M_Balloon (atlas), M_Glow (atlas as emissive), M_Display (radar screen).
Use Nearest filtering in Unreal for the PS1 look.
"""
import bpy, bmesh, json, math, os
import numpy as np
from mathutils import Vector

FOLDER = r"D:/Docs/Unreal Projects/MyProject/Content/Assets/Balloon"
ATLAS = 1024
MARGIN_PX = 4
SRC = bpy.data.scenes["AirBalloon"]
RIG = json.loads(SRC["AB_rig"])
GS = "AirBalloon_Game"
win = bpy.context.window_manager.windows[0]
area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
region = next(r for r in area.regions if r.type == 'WINDOW')

# ---------------------------------------------------------------- reset previous game scene
if GS in bpy.data.scenes:
    if win.scene.name == GS:
        win.scene = SRC
    gsc = bpy.data.scenes[GS]
    for o in list(gsc.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.scenes.remove(gsc)
PREFIX = ("SK_Balloon", "BAKE_", "M_Balloon", "M_Glow", "M_Display", "T_Balloon", "REF_Human")
for o in list(bpy.data.objects):
    if o.name.startswith("REF_Human") and not o.users_scene:
        bpy.data.objects.remove(o)
for coll in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials, bpy.data.images, bpy.data.worlds):
    for d in list(coll):
        if d.name.startswith(PREFIX) and d.users == 0:
            coll.remove(d)
gs = bpy.data.scenes.new(GS)
vl = gs.view_layers[0]

def ctx(active, **extra):
    return bpy.context.temp_override(window=win, area=area, region=region, scene=gs, view_layer=vl,
                                     active_object=active, object=active, selected_objects=[active],
                                     selected_editable_objects=[active], **extra)

# ---------------------------------------------------------------- 1. merge all parts into one mesh
win.scene = SRC
dg = bpy.context.evaluated_depsgraph_get()
dg.update()
MATS, ranges = [], []
bm = bmesh.new()
for o in sorted(SRC.objects, key=lambda x: x.name):
    if o.type != 'MESH':
        continue
    part = o.get("part", "basket")
    me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))   # modifiers applied
    me.transform(o.matrix_world)
    remap = []
    for m in me.materials:
        if m not in MATS:
            MATS.append(m)
        remap.append(MATS.index(m))
    idx = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", idx)
    if remap:
        idx = np.array(remap, np.int32)[idx]
    me.polygons.foreach_set("material_index", idx)
    a = len(bm.verts)
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    ranges.append((part, a, len(bm.verts)))
    bpy.data.meshes.remove(me)
me_all = bpy.data.meshes.new("SK_Balloon")
bm.to_mesh(me_all); bm.free()
for m in MATS:
    me_all.materials.append(m)
for p in me_all.polygons:
    p.use_smooth = False
obj = bpy.data.objects.new("SK_Balloon", me_all)
gs.collection.objects.link(obj)
vpart = np.empty(len(me_all.vertices), dtype=object)
for part, a, b in ranges:
    vg = obj.vertex_groups.get(part) or obj.vertex_groups.new(name=part)
    vg.add(list(range(a, b)), 1.0, 'REPLACE')
    vpart[a:b] = part
win.scene = gs

# ---------------------------------------------------------------- 2. UV atlas
me_all.uv_layers.new(name="UVMap")
obj.select_set(True, view_layer=vl); vl.objects.active = obj
with ctx(obj):
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.0, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')

# shrink texture space of surfaces the player barely sees (inside of the envelope, the rope net)
canvas = bpy.data.materials["AB_Canvas"]; rope = bpy.data.materials["AB_Rope"]
CI, RI = MATS.index(canvas), MATS.index(rope)
bm = bmesh.new(); bm.from_mesh(me_all)
uvl = bm.loops.layers.uv.active
bm.faces.ensure_lookup_table()
bal = [f for f in bm.faces if vpart[f.verts[0].index] == "balloon"]
cen = sum((f.calc_center_median() for f in bal if f.material_index == CI), Vector()) / max(1, sum(1 for f in bal if f.material_index == CI))
shrink = {}
for f in bal:
    if f.material_index == CI and f.normal.dot(f.calc_center_median() - cen) < 0:
        shrink[f.index] = 0.2          # inner shell of the envelope
    elif f.material_index == RI:
        shrink[f.index] = 0.5          # net strands
parent = {i: i for i in shrink}
def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]; i = parent[i]
    return i
key_faces = {}
for fi in shrink:
    for l in bm.faces[fi].loops:
        k = (l.vert.index, round(l[uvl].uv.x, 5), round(l[uvl].uv.y, 5))
        if k in key_faces:
            ra, rb = find(fi), find(key_faces[k])
            if ra != rb:
                parent[ra] = rb
        else:
            key_faces[k] = fi
islands = {}
for fi in shrink:
    islands.setdefault(find(fi), []).append(fi)
for faces in islands.values():
    loops = [l for fi in faces for l in bm.faces[fi].loops]
    c = sum((l[uvl].uv for l in loops), Vector((0, 0))) / len(loops)
    s = shrink[faces[0]]
    for l in loops:
        l[uvl].uv = c + (l[uvl].uv - c) * s
bm.to_mesh(me_all); bm.free()

gs.tool_settings.use_uv_select_sync = True
with ctx(obj):
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    try:
        bpy.ops.uv.pack_islands(rotate=True, margin=MARGIN_PX / ATLAS, shape_method='CONCAVE')
    except TypeError:
        bpy.ops.uv.pack_islands(rotate=True, margin=MARGIN_PX / ATLAS)
    bpy.ops.object.mode_set(mode='OBJECT')

# radar screen gets the whole 0..1 UV space (own material / texture), projected on the tilted console face
disp_mat = bpy.data.materials["AB_Display"]
DI = MATS.index(disp_mat)
d = RIG["display"]; c = Vector(d["center"]); u = Vector(d["u"]); v = Vector(d["v"])
uvd = me_all.uv_layers["UVMap"].data
for p in me_all.polygons:
    if p.material_index == DI:
        for li in p.loop_indices:
            co = me_all.vertices[me_all.loops[li].vertex_index].co
            uvd[li].uv = ((co - c).dot(u) / d["w"] + 0.5, (co - c).dot(v) / d["h"] + 0.5)

# ---------------------------------------------------------------- 3. bakes
def new_img(name, w, h, data=False):
    im = bpy.data.images.new(name, w, h, alpha=False)
    if data:
        im.colorspace_settings.name = 'Non-Color'
    return im

atlas = new_img("T_Balloon_BaseColor", ATLAS, ATLAS)
rough_img = new_img("BAKE_Rough", ATLAS, ATLAS, True)
metal_img = new_img("BAKE_Metal", ATLAS, ATLAS, True)
ao_img = new_img("BAKE_AO", ATLAS, ATLAS, True)
disp_img = new_img("T_Balloon_Radar", 256, 256)
dump_img = new_img("BAKE_Dump", 64, 64, True)          # display faces write here in data passes
GLOW = {"AB_Fire", "AB_Lamp"}

bake_mats, metal_vals = [], []
for i, m in enumerate(MATS):
    bmat = m.copy(); bmat.name = "BAKE_" + m.name
    nt = bmat.node_tree
    out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
    em = next((n for n in nt.nodes if n.type == 'EMISSION'), None)
    if em is not None:                       # emission-only: bake its colour as a rough diffuse
        col = tuple(em.inputs['Color'].default_value)
        for n in list(nt.nodes):
            if n.type != 'OUTPUT_MATERIAL':
                nt.nodes.remove(n)
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        pr = nt.nodes.new('ShaderNodeBsdfPrincipled')
        pr.inputs['Base Color'].default_value = col; pr.inputs['Roughness'].default_value = 1.0
        nt.links.new(pr.outputs[0], out.inputs['Surface'])
    pr = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    metal_vals.append(pr.inputs['Metallic'].default_value)
    tex = nt.nodes.new('ShaderNodeTexImage'); tex.name = "BAKE_TEX"
    nt.nodes.active = tex
    me_all.materials[i] = bmat
    bake_mats.append(bmat)

def target(img):
    for i, bmat in enumerate(bake_mats):
        t = bmat.node_tree.nodes["BAKE_TEX"]
        t.image = (disp_img if img is atlas else dump_img) if MATS[i] == disp_mat else img
        bmat.node_tree.nodes.active = t

def bake(kind, img, **kw):
    target(img)
    with ctx(obj):
        bpy.ops.object.bake(type=kind, margin=MARGIN_PX, use_clear=True, target='IMAGE_TEXTURES', **kw)

gs.render.engine = 'CYCLES'
gs.cycles.device = 'CPU'
gs.render.bake.margin = MARGIN_PX
bw = bpy.data.worlds.new("BAKE_World"); gs.world = bw
bw.light_settings.distance = 0.35            # AO reach: crevices, joints, under the rim

gs.cycles.samples = 1
bake('DIFFUSE', atlas, pass_filter={'COLOR'})
bake('ROUGHNESS', rough_img)
# metallic: temporarily drive each material's output with an emission of its metallic value
saved = []
for bmat, mv in zip(bake_mats, metal_vals):
    nt = bmat.node_tree
    out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
    src = out.inputs['Surface'].links[0].from_socket if out.inputs['Surface'].links else None
    e = nt.nodes.new('ShaderNodeEmission'); e.inputs['Color'].default_value = (mv, mv, mv, 1)
    nt.links.new(e.outputs[0], out.inputs['Surface'])
    saved.append((nt, out, src, e))
bake('EMIT', metal_img)
for nt, out, src, e in saved:
    nt.nodes.remove(e)
    if src is not None:
        nt.links.new(src, out.inputs['Surface'])
gs.cycles.samples = 48
bake('AO', ao_img)

# ---------------------------------------------------------------- 4. grit: AO grime, grain, ordered dither
def px(img):
    a = np.empty(img.size[0] * img.size[1] * 4, np.float32); img.pixels.foreach_get(a)
    return a.reshape(img.size[1], img.size[0], 4)

base, ao = px(atlas), px(ao_img)[..., 0]
rough, metal = px(rough_img)[..., 0], px(metal_img)[..., 0]
rng = np.random.default_rng(7)
rgb = base[..., :3] * (0.42 + 0.58 * ao[..., None])                 # dirt collects in crevices
rgb *= (1.0 + rng.normal(0, 0.045, rgb.shape[:2]))[..., None]        # per-texel grit
B4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16.0 - 0.5
H, W = rgb.shape[:2]
t = np.tile(B4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None]
LV = 24.0                                                            # ~5-bit colour, PS1-like banding
rgb = np.clip(np.floor(rgb * LV + t + 0.5) / LV, 0, 1)
base[..., :3] = rgb; base[..., 3] = 1
atlas.pixels.foreach_set(base.ravel())
atlas.filepath_raw = os.path.join(FOLDER, "T_Balloon_BaseColor.png"); atlas.file_format = 'PNG'; atlas.save()

orm = np.stack([ao, np.clip(rough + rng.normal(0, 0.03, rough.shape), 0, 1), metal, np.ones_like(ao)], -1)
orm_img = new_img("T_Balloon_ORM", ATLAS, ATLAS, True)
orm_img.pixels.foreach_set(orm.astype(np.float32).ravel())
orm_img.filepath_raw = os.path.join(FOLDER, "T_Balloon_ORM.png"); orm_img.file_format = 'PNG'; orm_img.save()

# radar CRT: dark phosphor, 3 range rings, faint crosshair + bearing ticks, balloon icon at the centre.
# The rotating sweep and the enemy blips are added live in the Unreal material.
W_ = H_ = 256
yy, xx = np.mgrid[0:H_, 0:W_].astype(np.float32)
X = (xx + 0.5) / W_ * 2 - 1; Y = (yy + 0.5) / H_ * 2 - 1           # -1..1, Y up (Blender pixel rows go up)
r = np.hypot(X, Y); th = np.degrees(np.arctan2(X, Y)) % 360
PX = 2.0 / W_
g = np.full((H_, W_), 0.10, np.float32)                               # phosphor intensity
g += 0.05 * (1 - r)                                                   # brighter centre glow
for rr in (0.30, 0.60, 0.90):                                          # 3 distance rings
    g[np.abs(r - rr) < 1.3 * PX] = 0.85
g[(np.abs(X) < 0.6 * PX) & (r < 0.9)] = 0.32                           # crosshair
g[(np.abs(Y) < 0.6 * PX) & (r < 0.9)] = 0.32
tk = (np.abs(((th + 15) % 30) - 15) < 0.8) & (r > 0.90) & (r < 0.97)   # bearing ticks every 30 deg
g[tk] = 0.6
# balloon icon (side view): envelope ellipse, two ropes, basket
env = ((X / 0.075) ** 2 + ((Y - 0.035) / 0.085) ** 2) < 1
bask = (np.abs(X) < 0.03) & (Y > -0.11) & (Y < -0.075)
ropes = ((np.abs(X - (-0.045 + (Y + 0.075) * -0.6)) < PX) | (np.abs(X - (0.045 - (Y + 0.075) * -0.6)) < PX)) & (Y > -0.075) & (Y < -0.03)
g[env | bask | ropes] = 1.0
g[r > 0.985] = 0.0                                                     # outside the tube
g = np.clip(g + np.random.default_rng(3).normal(0, 0.025, g.shape), 0, 1)
dp = np.zeros((H_, W_, 4), np.float32); dp[..., 3] = 1
dp[..., 0] = 0.06 + 0.40 * g ** 1.3; dp[..., 1] = 0.05 + 0.90 * g; dp[..., 2] = 0.04 + 0.45 * g ** 1.2
disp_img.scale(W_, H_)
disp_img.pixels.foreach_set(dp.ravel())
disp_img.filepath_raw = os.path.join(FOLDER, "T_Balloon_Radar.png"); disp_img.file_format = 'PNG'; disp_img.save()
old = os.path.join(FOLDER, "T_Balloon_Display.png")
if os.path.exists(old):
    os.remove(old)

# ---------------------------------------------------------------- 5. final game materials (3 slots)
def game_mat(name, img, orm=None, emissive=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = img; t.interpolation = 'Closest'
    nt.links.new(t.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = 0.85
    if orm is not None:
        o = nt.nodes.new('ShaderNodeTexImage'); o.image = orm; o.interpolation = 'Closest'
        sep = nt.nodes.new('ShaderNodeSeparateColor')
        nt.links.new(o.outputs['Color'], sep.inputs[0])
        nt.links.new(sep.outputs[1], bsdf.inputs['Roughness'])
        nt.links.new(sep.outputs[2], bsdf.inputs['Metallic'])
    if emissive:
        nt.links.new(t.outputs['Color'], bsdf.inputs['Emission Color'])
        bsdf.inputs['Emission Strength'].default_value = emissive
    return m

final = [game_mat("M_Balloon", atlas, orm_img), game_mat("M_Glow", atlas, None, 6.0), game_mat("M_Display", disp_img, None, 1.5)]
mapping = np.array([2 if m == disp_mat else 1 if m.name in GLOW else 0 for m in MATS], np.int32)
idx = np.zeros(len(me_all.polygons), np.int32)
me_all.polygons.foreach_get("material_index", idx)
me_all.materials.clear()
for m in final:
    me_all.materials.append(m)
me_all.polygons.foreach_set("material_index", mapping[idx])
for m in bake_mats:
    bpy.data.materials.remove(m)
for im in (rough_img, metal_img, ao_img, dump_img):
    bpy.data.images.remove(im)
gs.world = None
bpy.data.worlds.remove(bw)

# ---------------------------------------------------------------- 6. skeleton
arm_data = bpy.data.armatures.new("SK_Balloon_Skeleton")
arm = bpy.data.objects.new("Armature", arm_data)       # named "Armature" so Unreal doesn't add an extra root
gs.collection.objects.link(arm)
arm.show_in_front = True
for o in gs.objects:
    o.select_set(False, view_layer=vl)
arm.select_set(True, view_layer=vl); vl.objects.active = arm
with ctx(arm):
    bpy.ops.object.mode_set(mode='EDIT')
    eb = arm_data.edit_bones
    for name, b in RIG["bones"].items():
        e = eb.new(name)
        e.head = Vector(b["head"])
        e.tail = Vector(b["tail"]) if "tail" in b else e.head + Vector((0, 0, 0.3 if name == "root" else 0.15))
        e.roll = 0.0
    for name, b in RIG["bones"].items():
        if b["parent"]:
            eb[name].parent = eb[b["parent"]]
    for name, s in RIG["sockets"].items():
        e = eb.new(name)
        e.head = Vector(s["head"]); e.tail = Vector(s["tail"]); e.roll = 0.0
        e.parent = eb[s["parent"]]
        e.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
obj.parent = arm
mod = obj.modifiers.new("Armature", 'ARMATURE'); mod.object = arm

# ---------------------------------------------------------------- 7. FBX for Unreal (armature + mesh only)
fbx = os.path.join(FOLDER, "SK_Balloon.fbx")
for o in gs.objects:
    o.select_set(o in (arm, obj), view_layer=vl)
vl.objects.active = arm
with bpy.context.temp_override(window=win, scene=gs, view_layer=vl, active_object=arm,
                               selected_objects=[arm, obj]):
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={'ARMATURE', 'MESH'},
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False,
                             use_armature_deform_only=False, bake_anim=False, apply_unit_scale=True,
                             primary_bone_axis='Y', secondary_bone_axis='X', path_mode='STRIP')
gs.render.engine = 'BLENDER_EEVEE'

tris = sum(len(p.vertices) - 2 for p in me_all.polygons)
xs = [v.co for v in me_all.vertices]
dims = [round(max(c[i] for c in xs) - min(c[i] for c in xs), 2) for i in range(3)]
result = {"tris": tris, "bones": len(arm_data.bones), "size_m": dims,
          "shrunk_islands": len(islands), "materials": [m.name for m in me_all.materials]}
