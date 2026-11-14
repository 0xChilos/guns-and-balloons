"""Exports every location asset built by Locations_build.py into Content/Assets/Locations/<Level>/.

Run inside Blender after Locations_build.py (Locations.blend must be saved in Content/Assets/Locations).
Set ONLY_LEVELS = ["City"] in the namespace before running to export one level.

Per level it writes:
  SM_*.fbx / SK_*.fbx       one file per asset (UCX_ collision hulls, SOCKET_ empties, bones + animations)
  T_<Group>.png             baked, pixel-dithered colour atlas per texture group (AO grime baked in)
  T_<Level>_Ground.png      256 px tileable ground texture: soft, low contrast, no fine detail (no shimmer)
  <Level>_Placement.csv     hints for the random level generator (import as a DataTable)
  <Level>_Manifest.json     what was exported: tris, size, texture, material slots, sockets, collision
City also gets T_Smoke_Puff.png (factory smoke sprite).
"""
import bpy, bmesh, math, json, os, csv
import numpy as np
from mathutils import Vector, Matrix, Euler, Quaternion

ROOT = os.path.dirname(bpy.data.filepath)
assert ROOT, "Save Locations.blend into Content/Assets/Locations first"
LEVELS = ["Forest", "Field", "City", "Desert", "Loot", "Map_edge"]
ONLY = [l for l in (globals().get("ONLY_LEVELS") or LEVELS)]
MARGIN_PX = 3
win = bpy.context.window_manager.windows[0]
area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
region = next(r for r in area.regions if r.type == 'WINDOW')
HOME = bpy.data.scenes.get("Locations") or bpy.data.scenes.new("Locations")
REPORT = {}


def ctx(es, active, sel):
    return bpy.context.temp_override(window=win, area=area, region=region, scene=es, view_layer=es.view_layers[0],
                                     active_object=active, object=active, selected_objects=sel,
                                     selected_editable_objects=sel)


def px(img):
    a = np.empty(img.size[0] * img.size[1] * 4, np.float32); img.pixels.foreach_get(a)
    return a.reshape(img.size[1], img.size[0], 4)


def save_png(img, path):
    img.filepath_raw = path; img.file_format = 'PNG'; img.save()
    img.filepath = "//" + os.path.relpath(path, ROOT).replace("\\", "/")


def is_glow(m):
    return any(n.type == 'EMISSION' for n in m.node_tree.nodes)


def tris_of(me):
    return sum(len(p.vertices) - 2 for p in me.polygons)


B4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16.0 - 0.5


def grit(rgb, ao, seed):
    rng = np.random.default_rng(seed)
    rgb = rgb * (0.45 + 0.55 * ao[..., None])
    rgb = rgb * (1.0 + rng.normal(0, 0.04, rgb.shape[:2]))[..., None]
    H, W = rgb.shape[:2]
    t = np.tile(B4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None]
    return np.clip(np.floor(rgb * 24.0 + t + 0.5) / 24.0, 0, 1)


def tile_noise(n, size_px, seed):
    """Tileable gaussian-filtered noise, features about size_px wide, mean 0 / std 1."""
    rng = np.random.default_rng(seed)
    fx = np.fft.fftfreq(n)[:, None]; fy = np.fft.fftfreq(n)[None, :]
    F = np.fft.fft2(rng.normal(size=(n, n))) * np.exp(-(fx ** 2 + fy ** 2) * (size_px ** 2) * 2.0)
    g = np.real(np.fft.ifft2(F))
    return (g - g.mean()) / (g.std() + 1e-9)


GROUND = {   # base colour, second colour (large patches), patch size px, brightness variation, extra
    "Forest": ((0.15, 0.19, 0.10), (0.21, 0.16, 0.09), 48, 0.05, None),
    "Field": ((0.33, 0.40, 0.15), (0.44, 0.42, 0.20), 56, 0.045, None),
    "City": ((0.27, 0.26, 0.24), (0.22, 0.21, 0.20), 40, 0.04, None),
    "Desert": ((0.76, 0.60, 0.38), (0.70, 0.52, 0.32), 64, 0.035, "ripples"),
}


def make_ground(level):
    base, alt, size, var, extra = GROUND[level]
    n = 256
    big = np.clip(tile_noise(n, size, 11) * 0.5 + 0.5, 0, 1)
    small = tile_noise(n, 6, 12) * 0.6 + tile_noise(n, 16, 13) * 0.4
    rgb = np.array(base)[None, None] * (1 - big[..., None]) + np.array(alt)[None, None] * big[..., None]
    shade = 1.0 + var * small
    if extra == "ripples":                                  # very soft wind ripples: 8 waves per tile
        yy = np.arange(n)[:, None] / n
        warp = tile_noise(n, 40, 14) * 0.04
        shade = shade + 0.025 * np.sin(2 * math.pi * (8 * yy + warp))
    rgb = np.clip(rgb * shade[..., None], 0, 1)
    rgb = np.floor(rgb * 96 + 0.5) / 96                     # fine steps only: no visible banding, no dither
    img = bpy.data.images.new(f"T_{level}_Ground", n, n, alpha=False)
    a = np.ones((n, n, 4), np.float32); a[..., :3] = rgb
    img.pixels.foreach_set(a.ravel())
    path = os.path.join(ROOT, level, f"T_{level}_Ground.png")
    save_png(img, path)
    avg = [float(c) for c in rgb.reshape(-1, 3).mean(0)]
    bpy.data.images.remove(img)
    return {"texture": f"T_{level}_Ground.png", "average_linear": [round(c, 4) for c in avg],
            "average_srgb": [round(((c ** (1 / 2.4)) * 1.055 - 0.055) if c > 0.0031308 else c * 12.92, 4) for c in avg]}


def make_smoke(level):
    n = 64
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    r = np.hypot((xx + 0.5) / n * 2 - 1, (yy + 0.5) / n * 2 - 1)
    lump = tile_noise(n, 5, 21) * 0.18
    a = np.clip(1.0 - (r + lump) / 0.92, 0, 1) ** 0.8
    t = np.tile(B4, (n // 4, n // 4)) + 0.5
    a = np.floor(a * 4 + t * 0.999) / 4                    # 4 alpha steps, ordered dither: PS1 sprite edge
    shade = np.clip(0.62 + 0.18 * ((yy / n) - 0.5) - 0.1 * r, 0, 1)   # lit from above
    img = bpy.data.images.new("T_Smoke_Puff", n, n, alpha=True)
    out = np.zeros((n, n, 4), np.float32)
    out[..., 0] = shade * 0.92; out[..., 1] = shade * 0.90; out[..., 2] = shade * 0.86; out[..., 3] = a
    img.pixels.foreach_set(out.ravel())
    save_png(img, os.path.join(os.path.normpath(os.path.join(ROOT, "..", level)) if level == "Map_edge" else os.path.join(ROOT, level),
                               "T_FX_Smoke.png" if level == "Map_edge" else "T_Smoke_Puff.png"))
    bpy.data.images.remove(img)


def make_spark(level):
    """32 px sparkle sprite (white, alpha): 4-point star with a soft core, stepped alpha for the PS1 look.
    Tint it per pickup in Unreal."""
    n = 32
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    x = (xx + 0.5) / n * 2 - 1; y = (yy + 0.5) / n * 2 - 1
    r = np.hypot(x, y)
    star = np.clip(1 - (np.abs(x) * 7 + np.abs(y) * 1.1), 0, 1) + np.clip(1 - (np.abs(y) * 7 + np.abs(x) * 1.1), 0, 1)
    diag = np.clip(1 - (np.abs(x - y) * 6 + np.abs(x + y) * 2.2), 0, 1) + np.clip(1 - (np.abs(x + y) * 6 + np.abs(x - y) * 2.2), 0, 1)
    a = np.clip(star + 0.45 * diag + np.clip(1 - r / 0.35, 0, 1) ** 1.5, 0, 1)
    a = np.floor(a * 4 + 0.35) / 4
    out = np.ones((n, n, 4), np.float32); out[..., 3] = a
    img = bpy.data.images.new("T_Loot_Spark", n, n, alpha=True)
    img.pixels.foreach_set(out.ravel())
    save_png(img, os.path.join(ROOT, level, "T_Loot_Spark.png"))
    bpy.data.images.remove(img)


def make_explosion(folder):
    """4 x 4 flipbook (64 px frames, 256 px sheet): white-hot flash -> orange fire -> red -> dark smoke, stepped alpha."""
    n, F = 64, 4
    sheet = np.zeros((n * F, n * F, 4), np.float32)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    x = (xx + 0.5) / n * 2 - 1; y = (yy + 0.5) / n * 2 - 1
    r = np.hypot(x, y)
    t4 = np.tile(B4, (n // 4, n // 4)) + 0.5
    for i in range(16):
        t = i / 15
        lump = tile_noise(n, 6, 40 + i) * 0.12 + tile_noise(n, 14, 80) * 0.1
        rad = 0.35 + 0.6 * (1 - (1 - t) ** 2)
        a = np.clip((rad - (r + lump)) / 0.25, 0, 1) * (1 - t) ** 0.6
        heat = np.clip((1 - r / rad) * (1.4 - 1.6 * t) + lump, 0, 1)
        fire = np.stack([np.ones_like(heat), 0.25 + 0.75 * heat, 0.05 + 0.9 * heat ** 2], -1)
        smoke = np.stack([np.full_like(heat, 0.22), np.full_like(heat, 0.2), np.full_like(heat, 0.19)], -1)
        k = np.clip(t * 1.5 - 0.2 + (1 - heat) * 0.4, 0, 1)[..., None]
        rgb = fire * (1 - k) + smoke * k
        a = np.floor(a * 4 + t4 * 0.999) / 4
        fy, fx = 3 - i // F, i % F                             # Blender rows go up: frame 0 at the top-left
        sheet[fy * n:(fy + 1) * n, fx * n:(fx + 1) * n, :3] = rgb
        sheet[fy * n:(fy + 1) * n, fx * n:(fx + 1) * n, 3] = a
    img = bpy.data.images.new("T_FX_Explosion", n * F, n * F, alpha=True)
    img.pixels.foreach_set(sheet.ravel())
    save_png(img, os.path.join(folder, "T_FX_Explosion.png"))
    bpy.data.images.remove(img)


def remove_scene(name):
    if name in bpy.data.scenes:
        s = bpy.data.scenes[name]
        if win.scene == s:
            win.scene = HOME
        for o in list(s.objects):
            d = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            if d is not None and getattr(d, "users", 1) == 0:
                (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures).remove(d)
        bpy.data.scenes.remove(s)


def export_level(level):
    src = bpy.data.scenes["LOC_" + level]
    meta = json.loads(src["LOC_assets"]); groups = json.loads(src["LOC_groups"])
    folder = os.path.normpath(os.path.join(ROOT, "..", level) if level == "Map_edge" else os.path.join(ROOT, level))
    os.makedirs(folder, exist_ok=True)
    remove_scene("EXP_" + level)
    for coll in (bpy.data.materials, bpy.data.images):
        for d in list(coll):
            if d.name.startswith(("BAKE_", "M_" + level, "T_" + level, "M_LightBeam")) and d.users == 0:
                coll.remove(d)
    es = bpy.data.scenes.new("EXP_" + level); vl = es.view_layers[0]
    win.scene = src
    dg = bpy.context.evaluated_depsgraph_get(); dg.update()

    # ---- 1. one merged mesh per asset, at the origin
    objs = {}
    for name, a in meta.items():
        off = Vector(a["offset"])
        bm = bmesh.new(); mats = []; ranges = []
        parts = sorted((o for o in src.objects if o.type == 'MESH' and o.get("asset") == name), key=lambda o: o.name)
        for o in parts:
            me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
            me.transform(Matrix.Translation(-off) @ o.matrix_world)
            remap = []
            for m in me.materials:
                if m not in mats:
                    mats.append(m)
                remap.append(mats.index(m))
            idx = np.zeros(len(me.polygons), np.int32); me.polygons.foreach_get("material_index", idx)
            if remap:
                me.polygons.foreach_set("material_index", np.array(remap, np.int32)[idx])
            v0 = len(bm.verts); bm.from_mesh(me); bm.verts.ensure_lookup_table()
            ranges.append((o.get("bone", a["bones"][0]["name"] if a["bones"] else None), v0, len(bm.verts)))
            bpy.data.meshes.remove(me)
        me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
        for m in mats:
            me.materials.append(m)
        for p in me.polygons:
            p.use_smooth = False
        ob = bpy.data.objects.new(name, me); es.collection.objects.link(ob)
        ob["ranges"] = json.dumps(ranges)
        objs[name] = ob
    win.scene = es

    # ---- 2. per texture group: shared UV atlas, bake, grit, final materials
    for grp, size in groups.items():
        gobjs = [objs[n] for n, a in meta.items() if a["group"] == grp]
        if not gobjs:
            continue
        for o in gobjs:
            o.data.uv_layers.new(name="UVMap")
        if size == 0:                                       # beam / effect meshes: procedural UVs, own material, no texture
            for o in gobjs:
                me = o.data; uv = me.uv_layers["UVMap"].data
                mode = meta[o.name].get("uv", "axisY")
                cs = [v.co for v in me.vertices]
                L = max(abs(c.y) for c in cs) or 1.0
                R = max(c.xy.length for c in cs) or 1.0
                zmin, zmax = min(c.z for c in cs), max(c.z for c in cs)
                for p in me.polygons:
                    for li in p.loop_indices:
                        co = me.vertices[me.loops[li].vertex_index].co
                        if mode == "axisY":
                            uv[li].uv = ((math.atan2(co.z, co.x) / (2 * math.pi)) % 1.0, abs(co.y) / L)
                        elif mode == "radial":
                            uv[li].uv = ((math.atan2(co.y, co.x) / (2 * math.pi)) % 1.0, (co.xy.length - 0.6 * R) / (0.4 * R))
                        else:
                            uv[li].uv = ((math.atan2(co.y, co.x) / (2 * math.pi)) % 1.0, (co.z - zmin) / max(zmax - zmin, 1e-6))
                mname = meta[o.name].get("fxmat", "M_LightBeam")
                m = bpy.data.materials.get(mname) or bpy.data.materials.new(mname)
                me.materials.clear(); me.materials.append(m)
            continue
        for o in es.objects:
            o.select_set(o in gobjs, view_layer=vl)
        vl.objects.active = gobjs[0]
        with ctx(es, gobjs[0], gobjs):
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.0, area_weight=0.0,
                                     correct_aspect=True, scale_to_bounds=False)
            bpy.ops.uv.select_all(action='SELECT')
            bpy.ops.uv.average_islands_scale()
            bpy.ops.object.mode_set(mode='OBJECT')
        for o in gobjs:
            s = meta[o.name].get("uvscale", 1.0)
            if s != 1.0:
                for d in o.data.uv_layers["UVMap"].data:
                    d.uv = d.uv * s
        es.tool_settings.use_uv_select_sync = True
        with ctx(es, gobjs[0], gobjs):
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='SELECT')
            try:
                bpy.ops.uv.pack_islands(rotate=True, margin=MARGIN_PX / size, shape_method='CONCAVE')
            except TypeError:
                bpy.ops.uv.pack_islands(rotate=True, margin=MARGIN_PX / size)
            bpy.ops.object.mode_set(mode='OBJECT')

        col_img = bpy.data.images.new("BAKE_col_" + grp, size, size, alpha=False)
        ao_img = bpy.data.images.new("BAKE_ao_" + grp, size, size, alpha=False); ao_img.colorspace_settings.name = 'Non-Color'
        bake_of = {}
        for o in gobjs:
            for i, m in enumerate(o.data.materials):
                if m.name not in bake_of:
                    bm_ = m.copy(); bm_.name = f"BAKE_{grp}_{m.name}"
                    nt = bm_.node_tree
                    em = next((n for n in nt.nodes if n.type == 'EMISSION'), None)
                    if em is not None:
                        c = tuple(em.inputs['Color'].default_value)
                        for n in list(nt.nodes):
                            if n.type != 'OUTPUT_MATERIAL':
                                nt.nodes.remove(n)
                        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
                        pr = nt.nodes.new('ShaderNodeBsdfPrincipled'); pr.inputs['Base Color'].default_value = c
                        nt.links.new(pr.outputs[0], out.inputs['Surface'])
                    for pr_ in (n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'):
                        pr_.inputs['Metallic'].default_value = 0.0     # metals would bake dark in the colour pass
                    t = nt.nodes.new('ShaderNodeTexImage'); t.name = "BAKE_TEX"; nt.nodes.active = t
                    bake_of[m.name] = (bm_, is_glow(m))
                o.data.materials[i] = bake_of[m.name][0]
        glow_flags = {n: g for n, (bmat, g) in bake_of.items()}

        def target(img):
            for bmat, _ in bake_of.values():
                t = bmat.node_tree.nodes["BAKE_TEX"]; t.image = img; bmat.node_tree.nodes.active = t

        es.render.engine = 'CYCLES'; es.cycles.device = 'CPU'; es.render.bake.margin = MARGIN_PX
        for i, o in enumerate(gobjs):                          # spread out, so AO doesn't see the other assets
            o.location = (i * 3000.0, 0, 0)
        others = [o for o in es.objects if o not in gobjs and not o.hide_render]
        for o in others:                                       # assets of other groups don't cast AO either
            o.hide_render = True
        vl.update()
        bw = bpy.data.worlds.new("BAKE_World"); es.world = bw; bw.light_settings.distance = 0.5
        for kind, img, samples, kw in (("DIFFUSE", col_img, 1, {"pass_filter": {'COLOR'}}), ("AO", ao_img, 24, {})):
            target(img); es.cycles.samples = samples
            with ctx(es, gobjs[0], gobjs):
                bpy.ops.object.bake(type=kind, margin=MARGIN_PX, use_clear=True, target='IMAGE_TEXTURES', **kw)
        es.world = None; bpy.data.worlds.remove(bw)
        for o in gobjs:
            o.location = (0, 0, 0)
        for o in others:
            o.hide_render = False

        base = px(col_img); ao = px(ao_img)[..., 0]
        base[..., :3] = grit(base[..., :3], ao, sum(map(ord, grp))); base[..., 3] = 1
        tex = bpy.data.images.get("T_" + grp) or bpy.data.images.new("T_" + grp, size, size, alpha=False)
        if tuple(tex.size) != (size, size):
            tex.scale(size, size)
        tex.pixels.foreach_set(base.ravel())
        save_png(tex, os.path.join(folder, f"T_{grp}.png"))
        bpy.data.images.remove(col_img); bpy.data.images.remove(ao_img)

        def final_mat(name, emissive):
            m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
            m.use_nodes = True
            nt = m.node_tree; b = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
            t = next((n for n in nt.nodes if n.type == 'TEX_IMAGE'), None) or nt.nodes.new('ShaderNodeTexImage')
            t.image = tex; t.interpolation = 'Closest'
            nt.links.new(t.outputs['Color'], b.inputs['Base Color'])
            b.inputs['Roughness'].default_value = 0.85
            if emissive:
                nt.links.new(t.outputs['Color'], b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value = 6.0
            return m
        m_base, m_glow = final_mat("M_" + grp, False), final_mat("M_" + grp + "_Glow", True)
        for o in gobjs:
            me = o.data
            flags = [glow_flags[m.name[len("BAKE_" + grp) + 1:]] for m in me.materials]
            idx = np.zeros(len(me.polygons), np.int32); me.polygons.foreach_get("material_index", idx)
            new = np.array([1 if f else 0 for f in flags], np.int32)[idx]
            has_glow = bool(new.any())
            me.materials.clear(); me.materials.append(m_base)
            if has_glow:
                me.materials.append(m_glow)
            me.polygons.foreach_set("material_index", new if has_glow else np.zeros_like(new))
        for bmat, _ in bake_of.values():
            bpy.data.materials.remove(bmat)

    # ---- 3. collision hulls, sockets, skeletons, FBX
    manifest = {}
    for name, a in meta.items():
        ob = objs[name]
        extra = []
        for k, pts in enumerate(a["ucx"]):
            bm = bmesh.new()
            for p in pts:
                bm.verts.new(p)
            bmesh.ops.convex_hull(bm, input=bm.verts)
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
            me = bpy.data.meshes.new(f"UCX_{name}_{k:02d}"); bm.to_mesh(me); bm.free()
            u = bpy.data.objects.new(f"UCX_{name}_{k:02d}", me); es.collection.objects.link(u)
            extra.append(u)
        for sname, s in a["sockets"].items():
            e = bpy.data.objects.new("SOCKET_" + sname, None); es.collection.objects.link(e)
            e.empty_display_size = 0.5
            e.parent = ob; e.location = Vector(s["loc"]); e.rotation_euler = Euler(s["rot"])
            extra.append(e)
        arm = None
        if a["kind"] == "skeletal":
            ad = bpy.data.armatures.new(name + "_Skeleton")
            arm = bpy.data.objects.new("ARM_" + name, ad); es.collection.objects.link(arm)
            for o in es.objects:
                o.select_set(False, view_layer=vl)
            arm.select_set(True, view_layer=vl); vl.objects.active = arm
            with ctx(es, arm, [arm]):
                bpy.ops.object.mode_set(mode='EDIT')
                for b in a["bones"]:
                    eb = ad.edit_bones.new(b["name"]); eb.head = Vector(b["head"]); eb.tail = Vector(b["tail"]); eb.roll = 0.0
                    eb.use_deform = b["deform"]
                for b in a["bones"]:
                    if b["parent"]:
                        ad.edit_bones[b["name"]].parent = ad.edit_bones[b["parent"]]
                bpy.ops.object.mode_set(mode='OBJECT')
            for bname, v0, v1 in json.loads(ob["ranges"]):
                vg = ob.vertex_groups.get(bname) or ob.vertex_groups.new(name=bname)
                vg.add(list(range(v0, v1)), 1.0, 'REPLACE')
            ob.parent = arm
            ob.modifiers.new("Armature", 'ARMATURE').object = arm
            arm.animation_data_create()
            for an, keys in a["anims"].items():
                act = bpy.data.actions.get(an)
                if act:
                    bpy.data.actions.remove(act)
                act = bpy.data.actions.new(an); act.use_fake_user = True
                arm.animation_data.action = act
                for frame, rots, locs in keys:
                    for pb in arm.pose.bones:
                        pb.rotation_mode = 'QUATERNION'
                        R = pb.bone.matrix_local.to_3x3().inverted()
                        q = Quaternion()
                        if pb.name in rots:
                            ax_, ay_, az_, deg = rots[pb.name]
                            q = Quaternion((R @ Vector((ax_, ay_, az_))).normalized(), math.radians(deg))
                        pb.rotation_quaternion = q
                        pb.location = R @ Vector(locs[pb.name]) if pb.name in locs else Vector()
                        pb.keyframe_insert("rotation_quaternion", frame=frame)
                        pb.keyframe_insert("location", frame=frame)
                arm.animation_data.action = None
            for pb in arm.pose.bones:
                pb.rotation_quaternion = Quaternion(); pb.location = Vector()
        # export
        sel = [ob] + extra + ([arm] if arm else [])
        for o in es.objects:
            o.select_set(o in sel, view_layer=vl)
        vl.objects.active = arm or ob
        old_name = None
        if arm:
            clash = bpy.data.objects.get("Armature")
            if clash and clash != arm:
                clash.name = "Armature_old"
            arm.name = "Armature"                           # Unreal then doesn't add an extra root bone
        fbx = os.path.join(folder, name + ".fbx")
        anims = bool(a["anims"])
        with ctx(es, vl.objects.active, sel):
            bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True,
                                     object_types={'ARMATURE', 'MESH', 'EMPTY'} if arm else {'MESH', 'EMPTY'},
                                     use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False,
                                     use_armature_deform_only=False, bake_anim=anims, bake_anim_use_all_actions=anims,
                                     bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                                     apply_unit_scale=True, primary_bone_axis='Y', secondary_bone_axis='X',
                                     path_mode='STRIP')
        if arm:
            arm.name = "ARM_" + name
        me = ob.data
        xs = [v.co for v in me.vertices]
        manifest[name] = {
            "file": name + ".fbx", "kind": a["kind"], "category": a["cat"], "tris": tris_of(me),
            "size_m": [round(max(c[i] for c in xs) - min(c[i] for c in xs), 2) for i in range(3)],
            "texture": None if groups[a["group"]] == 0 else f"T_{a['group']}.png",
            "materials": [m.name for m in me.materials], "collision_hulls": len(a["ucx"]),
            "sockets": list(a["sockets"]), "bones": [b["name"] for b in a["bones"]], "anims": list(a["anims"]),
            "loot_spots": a.get("loot", []), "physics_bodies": a.get("bodies", []), "notes": a["notes"]}
    # ---- 4. ground, smoke, placement table
    if level in GROUND:
        manifest["_ground"] = make_ground(level)
    if level == "City":
        make_smoke(level)
    if level == "Loot":
        make_spark(level)
    if level == "Map_edge":
        make_explosion(folder)
        make_smoke(level)
    with open(os.path.join(folder, f"{level}_Placement.csv"), "w", newline="", encoding="utf-8") if level in GROUND else open(os.devnull, "w") as f:
        w = csv.writer(f)
        w.writerow(["---", "Asset", "Category", "Collision", "DensityPerKm2", "ScaleMin", "ScaleMax", "MinSpacingM",
                    "RandomYaw", "AlignToSlope", "ClusterSize", "Rule", "Notes"])
        for name, a in meta.items():
            p = a["place"]
            if name in ("SM_Windmill_Blades", "SM_LighthouseBeam"):
                continue
            rule = ";".join(k for k in ("ring", "line", "street", "wind_aligned") if p.get(k)) or ("flock" if "flock" in p else "scatter")
            if a["cat"] == "Loot":
                continue
            w.writerow([name, name, a["cat"], "true" if (a["ucx"] or a["kind"] == "skeletal") else "false",
                        p.get("density", 0), p.get("scale", (1, 1))[0], p.get("scale", (1, 1))[1], p.get("spacing", 0),
                        "true" if p.get("yaw", True) else "false", "true" if p.get("align", False) else "false",
                        p.get("cluster", 1), rule, a["notes"]])
    with open(os.path.join(folder, f"{level}_Manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1)
    win.scene = HOME
    return {n: (v["tris"], v["loot_spots"]) for n, v in manifest.items() if not n.startswith("_")}


for lv in ONLY:
    REPORT[lv] = export_level(lv)
result = REPORT
