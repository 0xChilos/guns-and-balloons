"""Builds every location asset (Forest, Field, City, Desert) in the Blender scenes LOC_<Level>.

Run inside Blender (Text Editor > Run Script), then run Locations_export.py.
Same art style as the balloon: low-poly, flat-shaded, procedural materials that the export bakes into small
pixel-dithered atlases. Units: metres, Z up, every asset is modelled around its own origin (base at z = 0),
then laid out in a row in its level scene for previewing.

Per asset (see ASSETS):
  group   - texture group: assets in one group share one baked texture (fillers share a small atlas;
            assets the player can bump into get their own, sharper texture)
  ucx     - convex collision hulls (exported as UCX_ meshes). Assets without ucx get no collision.
  sockets - attachment points (exported as SOCKET_ empties: smoke emitters, windmill blades, lights ...)
  kind    - "static" or "skeletal" (bones + optional animations)
  place   - hints for the random level generator (exported to <Level>_Placement.csv)
"""
import bpy, bmesh, math, random, json
from mathutils import Vector, Matrix, Euler

LEVELS = ["Forest", "Field", "City", "Desert"]

# ---------------------------------------------------------------- cleanup
HOME = bpy.data.scenes.get("Locations") or bpy.data.scenes.new("Locations")   # empty scene that always stays
for w in bpy.context.window_manager.windows:
    if w.scene.name.startswith(("LOC_", "EXP_")):
        w.scene = HOME
for s in list(bpy.data.scenes):
    if s.name.startswith(("LOC_", "EXP_")):
        for o in list(s.objects):
            d = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            if d is not None and d.users == 0:
                (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures if isinstance(d, bpy.types.Armature)
                 else bpy.data.lights if isinstance(d, bpy.types.Light) else bpy.data.cameras).remove(d)
        bpy.data.scenes.remove(s)
for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.armatures, bpy.data.actions):
    for d in list(coll):
        if d.name.startswith(("LOC_", "BAKE_", "M_Loc", "EXP_")) and d.users == 0:
            coll.remove(d)

SCENES = {lv: bpy.data.scenes.new("LOC_" + lv) for lv in LEVELS}
ASSETS, GROUPS = {}, {lv: {} for lv in LEVELS}
LAYOUT = {lv: {} for lv in LEVELS}
ROW_Y = {0: 0.0, 1: 90.0, 2: 200.0, -1: -1500.0}

# ---------------------------------------------------------------- materials (same recipe as the balloon)
M = {}

def _sock(sockets, name, typ):
    for s in sockets:
        if s.name == name and s.type == typ:
            return s
    raise KeyError(name)

def _base(name):
    m = bpy.data.materials.new("LOC_" + name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    return m, nt, nt.nodes, nt.links

def _noise_col(N, L, src, c1, c2, snap, nscale, ramp):
    sn = N.new('ShaderNodeVectorMath'); sn.operation = 'SNAP'
    sn.inputs[1].default_value = (snap, snap, snap)
    L.new(src, sn.inputs[0])
    nz = N.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = nscale; nz.inputs['Detail'].default_value = 4.0
    L.new(sn.outputs[0], nz.inputs['Vector'])
    cr = N.new('ShaderNodeValToRGB')
    cr.color_ramp.elements[0].position = ramp[0]; cr.color_ramp.elements[0].color = (*c2, 1)
    cr.color_ramp.elements[1].position = ramp[1]; cr.color_ramp.elements[1].color = (*c1, 1)
    L.new(nz.outputs['Fac'], cr.inputs['Fac'])
    return sn, cr.outputs['Color']

def _mix(N, L, fac, a, b, blend='MIX'):
    mx = N.new('ShaderNodeMix'); mx.data_type = 'RGBA'; mx.blend_type = blend
    if isinstance(fac, float):
        _sock(mx.inputs, 'Factor', 'VALUE').default_value = fac
    else:
        L.new(fac, _sock(mx.inputs, 'Factor', 'VALUE'))
    for key, v in (('A', a), ('B', b)):
        if isinstance(v, tuple):
            _sock(mx.inputs, key, 'RGBA').default_value = (*v, 1)
        else:
            L.new(v, _sock(mx.inputs, key, 'RGBA'))
    return _sock(mx.outputs, 'Result', 'RGBA')

def _stripes(N, L, src, freq):
    m1 = N.new('ShaderNodeMath'); m1.operation = 'MULTIPLY'; m1.inputs[1].default_value = freq
    L.new(src, m1.inputs[0])
    m2 = N.new('ShaderNodeMath'); m2.operation = 'FLOOR'; L.new(m1.outputs[0], m2.inputs[0])
    m3 = N.new('ShaderNodeMath')
    try:
        m3.operation = 'FLOORED_MODULO'
    except Exception:
        m3.operation = 'MODULO'
    m3.inputs[1].default_value = 2.0
    L.new(m2.outputs[0], m3.inputs[0])
    return m3.outputs[0]

def mk(name, c1, c2, metal=0.0, rough=0.8, snap=0.05, nscale=5.0, ramp=(0.3, 0.7), pattern=None,
       pcol=(0.7, 0.7, 0.7), pfreq=4.0):
    """Noise-coloured material. pattern 'bands' = horizontal stripes (z), 'vbands' = vertical stripes."""
    m, nt, N, L = _base(name)
    out = N.new('ShaderNodeOutputMaterial')
    m.diffuse_color = (*c1, 1)
    b = N.new('ShaderNodeBsdfPrincipled')
    b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    L.new(b.outputs[0], out.inputs['Surface'])
    tc = N.new('ShaderNodeTexCoord')
    sn, col = _noise_col(N, L, tc.outputs['Object'], c1, c2, snap, nscale, ramp)
    if pattern:
        s = N.new('ShaderNodeSeparateXYZ'); L.new(sn.outputs[0], s.inputs[0])
        if pattern == 'bands':
            src = s.outputs['Z']
        else:
            ad = N.new('ShaderNodeMath'); ad.operation = 'ADD'
            L.new(s.outputs['X'], ad.inputs[0]); L.new(s.outputs['Y'], ad.inputs[1]); src = ad.outputs[0]
        col = _mix(N, L, _stripes(N, L, src, pfreq), col, pcol, 'MULTIPLY')
    L.new(col, b.inputs['Base Color'])
    M[name] = m
    return m

def glow(name, col, strength):
    m, nt, N, L = _base(name)
    out = N.new('ShaderNodeOutputMaterial')
    e = N.new('ShaderNodeEmission')
    e.inputs['Color'].default_value = (*col, 1); e.inputs['Strength'].default_value = strength
    L.new(e.outputs[0], out.inputs['Surface'])
    m.diffuse_color = (*col, 1)
    M[name] = m
    return m

def facade(name, wall1, wall2, glass, lit, W, H, mortar, z0=0.6, bias=-0.75):
    """Wall with a grid of windows, sizes in metres so every building of one style matches: window columns every
    W m, floors every H m, windows (W - mortar) wide and (H - mortar) tall. Some windows are lit (warm), chosen per
    window. Faces pointing up or down get plain wall (no windows on cornices and ledges)."""
    m, nt, N, L = _base(name)
    out = N.new('ShaderNodeOutputMaterial')
    m.diffuse_color = (*wall1, 1)
    b = N.new('ShaderNodeBsdfPrincipled'); b.inputs['Roughness'].default_value = 0.85
    L.new(b.outputs[0], out.inputs['Surface'])
    tc = N.new('ShaderNodeTexCoord')
    _, wall = _noise_col(N, L, tc.outputs['Object'], wall1, wall2, 0.08, 3.0, (0.3, 0.7))
    sep = N.new('ShaderNodeSeparateXYZ'); L.new(tc.outputs['Object'], sep.inputs[0])
    geo = N.new('ShaderNodeNewGeometry')
    nsep = N.new('ShaderNodeSeparateXYZ'); L.new(geo.outputs['Normal'], nsep.inputs[0])
    def mth(op, a, b_=None):
        n = N.new('ShaderNodeMath'); n.operation = op
        for i, v in enumerate((a, b_)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                L.new(v, n.inputs[i])
        return n.outputs[0]
    ax, ay, az = (mth('ABSOLUTE', nsep.outputs[k]) for k in 'XYZ')
    face_y = mth('GREATER_THAN', ay, ax)                       # wall faces +-Y: windows run along X, else along Y
    h = mth('ADD', mth('MULTIPLY', sep.outputs['X'], face_y), mth('MULTIPLY', sep.outputs['Y'], mth('SUBTRACT', 1.0, face_y)))
    h = mth('ADD', h, mth('MULTIPLY', nsep.outputs['X'], 0.37))   # opposite walls don't line up exactly
    v = mth('SUBTRACT', sep.outputs['Z'], z0)
    cu, cv = mth('DIVIDE', h, W), mth('DIVIDE', v, H)
    fu, fv = mth('FRACT', cu), mth('FRACT', cv)
    mu = (mortar / W) / 2
    win_u = mth('MULTIPLY', mth('GREATER_THAN', fu, mu), mth('LESS_THAN', fu, 1 - mu))
    win_v = mth('MULTIPLY', mth('GREATER_THAN', fv, (mortar / H) * 0.6), mth('LESS_THAN', fv, 1 - (mortar / H) * 0.4))
    win = mth('MULTIPLY', mth('MULTIPLY', win_u, win_v), mth('GREATER_THAN', v, 0.0))
    win = mth('MULTIPLY', win, mth('LESS_THAN', az, 0.5))
    cell = N.new('ShaderNodeCombineXYZ')
    L.new(mth('FLOOR', cu), cell.inputs[0]); L.new(mth('FLOOR', cv), cell.inputs[1]); L.new(face_y, cell.inputs[2])
    wn = N.new('ShaderNodeTexWhiteNoise'); wn.noise_dimensions = '3D'
    L.new(cell.outputs[0], wn.inputs['Vector'])
    lit_mask = mth('GREATER_THAN', wn.outputs['Value'], 0.5 - bias * 0.5)   # bias -0.75 -> ~12% of windows lit
    pane = _mix(N, L, lit_mask, glass, lit)
    L.new(_mix(N, L, win, wall, pane), b.inputs['Base Color'])
    M[name] = m
    return m

# nature
mk("PineNeedle", (0.10, 0.22, 0.12), (0.04, 0.10, 0.06), snap=0.15, nscale=3, rough=0.95)
mk("PineNeedleDark", (0.07, 0.16, 0.10), (0.03, 0.07, 0.05), snap=0.15, nscale=3, rough=0.95)
mk("PineBark", (0.26, 0.15, 0.08), (0.11, 0.06, 0.03), snap=0.05, nscale=6, pattern='vbands', pcol=(0.7, 0.65, 0.6), pfreq=6)
mk("Leaf", (0.22, 0.34, 0.12), (0.10, 0.18, 0.06), snap=0.2, nscale=2.5, rough=0.95)
mk("Bark", (0.30, 0.22, 0.14), (0.14, 0.10, 0.06), snap=0.05, nscale=6)
mk("RockGrey", (0.42, 0.42, 0.40), (0.22, 0.23, 0.22), snap=0.2, nscale=1.2)
mk("Moss", (0.20, 0.30, 0.12), (0.10, 0.16, 0.07), snap=0.2, nscale=2)
mk("WoodEnd", (0.55, 0.40, 0.22), (0.35, 0.24, 0.12), snap=0.02, nscale=12)
mk("MountainRock", (0.25, 0.27, 0.33), (0.16, 0.18, 0.23), snap=8.0, nscale=0.004, rough=1.0)
mk("Snow", (0.86, 0.88, 0.92), (0.66, 0.70, 0.78), snap=8.0, nscale=0.005, rough=0.9)
mk("Hay", (0.70, 0.56, 0.24), (0.42, 0.32, 0.12), snap=0.06, nscale=8, pattern='bands', pcol=(0.8, 0.75, 0.6), pfreq=5)
mk("Hedge", (0.16, 0.28, 0.10), (0.07, 0.14, 0.05), snap=0.25, nscale=2, rough=0.95)
mk("Thatch", (0.52, 0.42, 0.22), (0.30, 0.22, 0.10), snap=0.1, nscale=5, pattern='bands', pcol=(0.75, 0.7, 0.6), pfreq=3)
mk("Canvas", (0.70, 0.64, 0.50), (0.48, 0.42, 0.30), snap=0.2, nscale=2)
mk("CanvasTorn", (0.48, 0.40, 0.28), (0.25, 0.20, 0.13), snap=0.3, nscale=1.5)
mk("Feather", (0.11, 0.11, 0.12), (0.03, 0.03, 0.04), snap=0.01, nscale=30, rough=0.7)
mk("Beak", (0.80, 0.58, 0.14), (0.55, 0.36, 0.08), snap=0.01, nscale=20)
mk("Sand", (0.80, 0.64, 0.40), (0.62, 0.46, 0.26), snap=2.0, nscale=0.05, rough=1.0)
mk("RockDesert", (0.62, 0.38, 0.22), (0.38, 0.22, 0.12), snap=0.5, nscale=0.8, pattern='bands', pcol=(0.82, 0.75, 0.68), pfreq=0.6)
mk("Cactus", (0.26, 0.40, 0.20), (0.14, 0.24, 0.10), snap=0.05, nscale=6, pattern='vbands', pcol=(0.75, 0.8, 0.7), pfreq=10)
mk("Bone", (0.78, 0.72, 0.58), (0.52, 0.46, 0.34), snap=0.05, nscale=5)
# built
mk("Wood", (0.42, 0.28, 0.15), (0.20, 0.12, 0.06), snap=0.03, nscale=8, pattern='vbands', pcol=(0.7, 0.65, 0.6), pfreq=5)
mk("WoodDark", (0.25, 0.16, 0.09), (0.11, 0.07, 0.04), snap=0.03, nscale=8, pattern='vbands', pcol=(0.7, 0.65, 0.6), pfreq=5)
mk("Logs", (0.40, 0.26, 0.13), (0.18, 0.11, 0.05), snap=0.03, nscale=8, pattern='bands', pcol=(0.55, 0.5, 0.45), pfreq=3.2)
mk("BarnRed", (0.48, 0.13, 0.08), (0.26, 0.06, 0.04), snap=0.04, nscale=6, pattern='vbands', pcol=(0.7, 0.62, 0.6), pfreq=3)
mk("Stone", (0.52, 0.49, 0.44), (0.30, 0.28, 0.25), snap=0.15, nscale=2.5)
mk("Plaster", (0.80, 0.76, 0.66), (0.60, 0.56, 0.46), snap=0.1, nscale=3)
mk("StoneDark", (0.30, 0.29, 0.28), (0.15, 0.15, 0.15), snap=0.15, nscale=2.5)
mk("Brick", (0.42, 0.20, 0.13), (0.22, 0.10, 0.07), snap=0.1, nscale=3, pattern='bands', pcol=(0.55, 0.45, 0.4), pfreq=0.7)
mk("SootBrick", (0.24, 0.17, 0.14), (0.10, 0.08, 0.07), snap=0.1, nscale=3, pattern='bands', pcol=(0.6, 0.55, 0.5), pfreq=0.5)
mk("Slate", (0.21, 0.23, 0.28), (0.11, 0.12, 0.15), snap=0.08, nscale=4, pattern='bands', pcol=(0.7, 0.72, 0.78), pfreq=3)
mk("RoofTile", (0.48, 0.22, 0.14), (0.26, 0.11, 0.07), snap=0.08, nscale=4, pattern='bands', pcol=(0.7, 0.62, 0.6), pfreq=3)
mk("Iron", (0.20, 0.13, 0.09), (0.05, 0.05, 0.05), metal=0.6, rough=0.7, snap=0.05, nscale=6, ramp=(0.45, 0.8))
mk("IronDark", (0.10, 0.10, 0.11), (0.04, 0.04, 0.05), metal=0.6, rough=0.6, snap=0.05, nscale=6)
mk("Brass", (0.55, 0.40, 0.14), (0.20, 0.15, 0.06), metal=0.9, rough=0.45, snap=0.03, nscale=6)
mk("Copper", (0.26, 0.48, 0.40), (0.42, 0.26, 0.14), metal=0.7, rough=0.5, snap=0.1, nscale=3)
mk("Glass", (0.20, 0.27, 0.30), (0.08, 0.11, 0.13), rough=0.2, snap=0.1, nscale=4)
mk("Dark", (0.05, 0.05, 0.06), (0.02, 0.02, 0.03), snap=0.1, nscale=4)
mk("PaintGreen", (0.10, 0.24, 0.16), (0.05, 0.12, 0.08), snap=0.05, nscale=6)
mk("PaintRed", (0.42, 0.08, 0.06), (0.22, 0.04, 0.03), snap=0.05, nscale=6)
mk("LighthouseWhite", (0.80, 0.78, 0.72), (0.58, 0.55, 0.48), snap=0.12, nscale=3)
mk("LighthouseRed", (0.56, 0.11, 0.08), (0.32, 0.06, 0.04), snap=0.12, nscale=3)
mk("Sandbag", (0.55, 0.47, 0.30), (0.32, 0.26, 0.15), snap=0.06, nscale=6)
mk("ClockFace", (0.84, 0.80, 0.66), (0.66, 0.62, 0.48), snap=0.05, nscale=6)
facade("FacadeBrick", (0.38, 0.22, 0.15), (0.24, 0.13, 0.09), (0.05, 0.06, 0.08), (0.72, 0.52, 0.24), 2.4, 3.2, 1.2)
facade("FacadeStucco", (0.72, 0.68, 0.57), (0.54, 0.50, 0.41), (0.06, 0.07, 0.09), (0.72, 0.52, 0.24), 2.4, 3.4, 1.15)
facade("FacadeSoot", (0.24, 0.22, 0.21), (0.13, 0.12, 0.12), (0.04, 0.04, 0.05), (0.70, 0.50, 0.22), 2.4, 3.2, 1.2)
facade("FacadeFactory", (0.32, 0.18, 0.12), (0.18, 0.10, 0.07), (0.10, 0.12, 0.12), (0.62, 0.40, 0.16), 3.0, 4.0, 1.0, z0=1.0, bias=-0.6)
facade("FacadeTower", (0.56, 0.52, 0.46), (0.38, 0.35, 0.30), (0.06, 0.07, 0.09), (0.74, 0.55, 0.26), 2.4, 3.4, 1.1)
facade("FacadePlaster", (0.80, 0.76, 0.66), (0.62, 0.58, 0.48), (0.08, 0.09, 0.10), (0.70, 0.52, 0.25), 2.6, 3.0, 1.5, z0=0.9, bias=-0.9)
glow("WindowGlow", (1.0, 0.58, 0.20), 6.0)
glow("LampGlow", (1.0, 0.82, 0.48), 10.0)
glow("RedLight", (1.0, 0.06, 0.03), 12.0)
glow("ClockGlow", (0.95, 0.88, 0.62), 3.0)
glow("LensGlow", (1.0, 0.94, 0.78), 14.0)
glow("Fire", (1.0, 0.36, 0.06), 20.0)
glow("LightBeam", (1.0, 0.95, 0.80), 2.0)            # beam cone: own translucent material in Unreal

# ---------------------------------------------------------------- asset context
CUR, OFF, BONE = None, Vector(), None

def group(level, name, px):
    GROUPS[level][name] = px

def asset(name, level, grp, width, row=0, kind="static", uvscale=1.0, cat="Filler", place=None, notes=""):
    """Start a new asset. Parts made after this belong to it; geometry is modelled around (0, 0, 0)."""
    global CUR, OFF, BONE
    cur = LAYOUT[level].get(row, 0.0)
    OFF = Vector((cur + width / 2, ROW_Y[row], 0))
    LAYOUT[level][row] = cur + width + max(4.0, width * 0.15)
    CUR, BONE = name, None
    random.seed(name)
    ASSETS[name] = dict(level=level, group=grp, kind=kind, offset=list(OFF), ucx=[], sockets={}, bones=[],
                        anims={}, uvscale=uvscale, cat=cat, place=place or {}, notes=notes)

def put(bm, name, mats):
    """Link a finished bmesh (asset-local coordinates) as a part of the current asset."""
    bmesh.ops.translate(bm, vec=OFF, verts=bm.verts)
    me = bpy.data.meshes.new("LOC_" + CUR + "_" + name)
    bm.to_mesh(me); bm.free()
    for p in me.polygons:
        p.use_smooth = False
    for mt in ([mats] if isinstance(mats, str) else mats):
        me.materials.append(M[mt])
    o = bpy.data.objects.new("LOC_" + CUR + "_" + name, me)
    o["asset"] = CUR
    if BONE:
        o["bone"] = BONE
    SCENES[ASSETS[CUR]["level"]].collection.objects.link(o)
    return o

def T(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    return Matrix.Translation(loc) @ Euler(rot).to_matrix().to_4x4() @ Matrix.Diagonal((*scale, 1))

def box(name, size, mat, loc=(0, 0, 0), rot=(0, 0, 0)):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0, matrix=T(loc, rot, size))
    return put(bm, name, mat)

def prism(name, n, r0, r1, z0, z1, mat, loc=(0, 0, 0), rot=(0, 0, 0), top=True, bottom=True, phase=None,
          jitter=0.0, sx=1.0, sy=1.0):
    """n-sided frustum from radius r0 at z0 to r1 at z1 (r1 = 0 makes a cone with an apex)."""
    phase = math.pi / n if phase is None else phase
    bm = bmesh.new()
    ang = [phase + 2 * math.pi * i / n for i in range(n)]
    jit = [1 + random.uniform(-jitter, jitter) for _ in range(n)]
    lo = [bm.verts.new((r0 * jit[i] * math.cos(a) * sx, r0 * jit[i] * math.sin(a) * sy, z0)) for i, a in enumerate(ang)]
    if r1 <= 0:
        ap = bm.verts.new((0, 0, z1))
        for i in range(n):
            bm.faces.new((lo[i], lo[(i + 1) % n], ap))
    else:
        hi = [bm.verts.new((r1 * jit[i] * math.cos(a) * sx, r1 * jit[i] * math.sin(a) * sy, z1)) for i, a in enumerate(ang)]
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
        if top:
            bm.faces.new(hi)
    if bottom:
        bm.faces.new(list(reversed(lo)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.transform(bm, matrix=T(loc, rot), verts=bm.verts)
    return put(bm, name, mat)

def bar(name, a, b, r, mat, n=4):
    a, b = Vector(a), Vector(b); d = b - a
    bm = bmesh.new()
    lo = [bm.verts.new((r * math.cos(2 * math.pi * i / n + math.pi / n), r * math.sin(2 * math.pi * i / n + math.pi / n), 0)) for i in range(n)]
    hi = [bm.verts.new((v.co.x, v.co.y, d.length)) for v in lo]
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.faces.new(hi); bm.faces.new(list(reversed(lo)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    q = d.to_track_quat('Z', 'Y')
    bmesh.ops.transform(bm, matrix=Matrix.Translation(a) @ q.to_matrix().to_4x4(), verts=bm.verts)
    return put(bm, name, mat)

def ball(name, r, loc, mat, u=8, v=5, scale=(1, 1, 1), jitter=0.0):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=r)
    for vv in bm.verts:
        j = 1 + random.uniform(-jitter, jitter)
        vv.co = Vector((vv.co.x * scale[0] * j, vv.co.y * scale[1] * j, vv.co.z * scale[2] * j))
    bmesh.ops.translate(bm, vec=Vector(loc), verts=bm.verts)
    return put(bm, name, mat)

def hull(name, pts, mats, assign=None, drop_bottom=False):
    """Convex hull of points; assign(face_center, normal) -> material index (optional)."""
    bm = bmesh.new()
    for p in pts:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if drop_bottom:
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < -0.95], context='FACES_ONLY')
    if assign:
        for f in bm.faces:
            f.material_index = assign(f.calc_center_median(), f.normal)
    return put(bm, name, mats)

def extrude(name, pts, y0, y1, mat, loc=(0, 0, 0), rot=(0, 0, 0)):
    """2D profile (x, z) extruded along Y from y0 to y1."""
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in pts]
    b = [bm.verts.new((x, y1, z)) for x, z in pts]
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(a); bm.faces.new(list(reversed(b)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.transform(bm, matrix=T(loc, rot), verts=bm.verts)
    return put(bm, name, mat)

def gable(name, w, d, rh, z0, mat, loc=(0, 0, 0), rot=(0, 0, 0), over=0.35):
    """Pitched roof: ridge along X, width w (X), depth d (Y), ridge height rh above z0."""
    hw, hd = w / 2 + over, d / 2 + over
    return extrude(name, [(-hd, z0 - 0.15), (hd, z0 - 0.15), (0, z0 + rh)], -hw, hw, mat, loc,
                   (rot[0], rot[1], rot[2] + math.pi / 2))

def hip(name, w, d, rh, z0, mat, loc=(0, 0, 0), over=0.35):
    hw, hd = w / 2 + over, d / 2 + over
    r = min(hw, hd) * 0.9
    pts = [(-hw, -hd, z0), (hw, -hd, z0), (hw, hd, z0), (-hw, hd, z0)]
    if hw >= hd:
        pts += [(-hw + r, 0, z0 + rh), (hw - r, 0, z0 + rh)]
    else:
        pts += [(0, -hd + r, z0 + rh), (0, hd - r, z0 + rh)]
    return hull(name, [Vector(loc) + Vector(p) for p in pts], mat)

def ring_bars(name, r, z, mat, n=12, th=0.1, c=(0, 0)):
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        bar(f"{name}{i}", (c[0] + r * math.cos(a0), c[1] + r * math.sin(a0), z), (c[0] + r * math.cos(a1), c[1] + r * math.sin(a1), z), th, mat)

# ---- collision, sockets, bones
def ucx_pts(pts):
    ASSETS[CUR]["ucx"].append([list(map(float, p)) for p in pts])

def ucx_box(size, loc=(0, 0, 0), rot=(0, 0, 0)):
    Mx = T(loc, rot, size)
    ucx_pts([Mx @ Vector((x, y, z)) for x in (-.5, .5) for y in (-.5, .5) for z in (-.5, .5)])

def ucx_prism(n, r0, r1, z0, z1, c=(0, 0)):
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n + math.pi / n
        pts += [(c[0] + r0 * math.cos(a), c[1] + r0 * math.sin(a), z0), (c[0] + max(r1, 0.05) * math.cos(a), c[1] + max(r1, 0.05) * math.sin(a), z1)]
    ucx_pts(pts)

def socket(name, loc, rot=(0, 0, 0)):
    ASSETS[CUR]["sockets"][name] = {"loc": list(map(float, loc)), "rot": list(map(float, rot))}

def bone(name, head, tail, parent=None, deform=True):
    ASSETS[CUR]["bones"].append({"name": name, "head": list(map(float, head)), "tail": list(map(float, tail)),
                                 "parent": parent, "deform": deform})

def part_bone(name):
    global BONE
    BONE = name

# ---------------------------------------------------------------- shared shapes
def pine(p, h, r, tiers, seg, trunk_r, needle="PineNeedle", jit=0.12, z_start=0.22):
    prism(p + "Trunk", max(5, seg - 1), trunk_r, trunk_r * 0.35, 0, h * 0.62, "PineBark", jitter=0.05)
    for i in range(tiers):
        f = i / tiers
        zb = h * (z_start + (1 - z_start) * f * 0.93)
        zt = h if i == tiers - 1 else zb + h * (1 - z_start) / tiers * 2.1
        rb = r * (1.0 - 0.78 * f) * random.uniform(0.92, 1.08)
        prism(f"{p}Tier{i}", seg, rb, 0, zb, zt, needle, jitter=jit, phase=random.uniform(0, 6.3),
              loc=(random.uniform(-0.03, 0.03) * r, random.uniform(-0.03, 0.03) * r, 0))

def rock(p, r, loc, mat, flat=0.6, jit=0.25, u=7, v=4):
    return ball(p, r, loc, mat, u=u, v=v, scale=(1, random.uniform(0.75, 1.0), flat), jitter=jit)

def mountain_peak(p, cx, cy, base_r, ph, snow_from=None, n=8):
    """Ultra low poly peak (about 40 tris): 3 jittered rings narrowing fast (concave, craggy slopes) and an
    off-centre summit. The summit fan is snow."""
    sx, sy = random.uniform(-0.15, 0.15) * base_r, random.uniform(-0.1, 0.1) * base_r
    bm = bmesh.new()
    rings = []
    for rf, zf in ((1.0, 0.0), (0.64, 0.32), (0.3, 0.66)):
        ph0 = random.uniform(-0.3, 0.3); ring = []
        for i in range(n):
            a = ph0 + 2 * math.pi * i / n + random.uniform(-0.2, 0.2)
            rr = base_r * rf * random.uniform(0.75, 1.2)
            z = 0.0 if zf == 0 else ph * zf * random.uniform(0.85, 1.12)
            ring.append(bm.verts.new((cx + sx * zf + rr * math.cos(a), cy + sy * zf + rr * math.sin(a), z)))
        rings.append(ring)
    top = bm.verts.new((cx + sx, cy + sy, ph))
    for k in range(2):
        A, B = rings[k], rings[k + 1]
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((A[i], A[j], B[j], B[i]))
    snow = []
    for i in range(n):
        snow.append(bm.faces.new((rings[2][i], rings[2][(i + 1) % n], top)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in snow:
        f.material_index = 1
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    put(bm, p, ["MountainRock", "Snow"])

def house(p, w, d, h, fac, roof="Slate", rh=None, kind="gable", chimneys=2, over=0.35):
    box(p + "Walls", (w, d, h), fac, loc=(0, 0, h / 2))
    rh = rh if rh is not None else d * 0.38
    if kind == "gable":
        gable(p + "Roof", w, d, rh, h, roof, over=over)
    elif kind == "hip":
        hip(p + "Roof", w, d, rh, h, roof, over=over)
    elif kind == "mansard":
        hull(p + "Roof", [Vector(q) for q in ((-w / 2 - .2, -d / 2 - .2, h), (w / 2 + .2, -d / 2 - .2, h), (w / 2 + .2, d / 2 + .2, h), (-w / 2 - .2, d / 2 + .2, h),
                                               (-w / 2 + 1.2, -d / 2 + 1.2, h + rh), (w / 2 - 1.2, -d / 2 + 1.2, h + rh), (w / 2 - 1.2, d / 2 - 1.2, h + rh), (-w / 2 + 1.2, d / 2 - 1.2, h + rh))], roof)
    elif kind == "flat":
        box(p + "Roof", (w + 0.4, d + 0.4, 0.5), "StoneDark", loc=(0, 0, h + 0.25)); rh = 0.5
    for i in range(chimneys):
        x = -w / 2 + w * (i + 0.5) / chimneys
        ch = h + rh * (0.75 if kind in ("gable", "hip") else 1.0)
        box(f"{p}Chimney{i}", (0.9, 0.6, 2.2), "SootBrick", loc=(x, 0, ch + 0.6))
        for k in (-1, 1):
            prism(f"{p}Pot{i}{k}", 5, 0.12, 0.1, ch + 1.7, ch + 2.15, "RoofTile", loc=(x + k * 0.2, 0, 0), bottom=False)

def windows_strip(p, w, z, y, mat, n, size=(1.0, 0.1, 1.6)):
    for i in range(n):
        x = -w / 2 + w * (i + 0.5) / n
        box(f"{p}{i}", size, mat, loc=(x, y, z))

# ================================================================ FOREST
group("Forest", "Forest_Props", 512)
group("Forest", "Forest_PineTall", 512)
group("Forest", "Forest_Mountains", 256)

for k, (h, r, tiers, seg, needle) in enumerate(((4.5, 1.5, 3, 6, "PineNeedle"), (6.0, 1.6, 4, 6, "PineNeedleDark"), (8.0, 2.1, 4, 7, "PineNeedle"))):
    nm = f"SM_Pine_Small_{'ABC'[k]}"
    asset(nm, "Forest", "Forest_Props", 5, cat="Filler",
          place=dict(density=2500, scale=(0.7, 1.4), spacing=3, yaw=True, align=False, cluster=8),
          notes="Small pine, fills the map, no collision")
    pine("", h, r, tiers, seg, 0.14 + 0.03 * k, needle)

for k, (h, r) in enumerate(((28.0, 5.2), (34.0, 5.8))):
    nm = f"SM_Pine_Tall_{'AB'[k]}"
    asset(nm, "Forest", "Forest_PineTall", 14, row=1, cat="Obstacle",
          place=dict(density=120, scale=(0.85, 1.25), spacing=14, yaw=True, align=False, cluster=3),
          notes="Tall pine with collision: the balloon can bump into it")
    pine("", h, r, 8, 8, 0.55, "PineNeedleDark" if k else "PineNeedle", jit=0.1, z_start=0.28)
    for i in range(5):                                   # a few bare dead branches on the lower trunk
        a = random.uniform(0, 6.3); z = h * random.uniform(0.12, 0.26)
        bar(f"Branch{i}", (0, 0, z), (2.2 * math.cos(a), 2.2 * math.sin(a), z + 0.8), 0.07, "PineBark")
    ucx_prism(8, 0.6, 0.25, 0, h * 0.62)
    ucx_prism(8, r * 1.05, 0.2, h * 0.27, h)

for k, (r, flat) in enumerate(((1.6, 0.6), (2.8, 0.5))):
    asset(f"SM_Rock_{'AB'[k]}", "Forest", "Forest_Props", 6, cat="Filler",
          place=dict(density=150, scale=(0.6, 1.8), spacing=6, yaw=True, align=True, cluster=2))
    rock("Rock", r, (0, 0, r * flat * 0.6), "RockGrey", flat=flat)
    ball("Moss", r * 0.75, (r * 0.1, 0, r * flat * 1.0), "Moss", u=6, v=3, scale=(1, 0.8, 0.25), jitter=0.15)

asset("SM_Stump", "Forest", "Forest_Props", 3, place=dict(density=200, scale=(0.8, 1.3), spacing=4, yaw=True, align=True, cluster=1))
prism("Stump", 7, 0.45, 0.38, 0, 0.55, "PineBark", top=False, jitter=0.08)
prism("StumpTop", 7, 0.38, 0.38, 0.55, 0.56, "WoodEnd", bottom=False)
for i in range(4):
    a = i * math.pi / 2 + 0.4
    bar(f"Root{i}", (0.25 * math.cos(a), 0.25 * math.sin(a), 0.2), (0.85 * math.cos(a), 0.85 * math.sin(a), 0.0), 0.11, "PineBark")

asset("SM_FallenLog", "Forest", "Forest_Props", 8, place=dict(density=80, scale=(0.8, 1.3), spacing=8, yaw=True, align=True, cluster=1))
prism("Log", 7, 0.36, 0.3, -3.2, 3.2, "PineBark", rot=(0, math.pi / 2, 0.15), loc=(0, 0, 0.32), top=False, bottom=False)
for s in (-1, 1):
    prism(f"LogEnd{s}", 7, 0.36 if s < 0 else 0.3, 0.36 if s < 0 else 0.3, 0, 0.01, "WoodEnd", rot=(0, s * math.pi / 2, 0.15),
          loc=(s * 3.2 * math.cos(0.15), s * 3.2 * math.sin(0.15), 0.32))
for i in range(3):
    x = random.uniform(-2.5, 2.5)
    bar(f"Stub{i}", (x, 0, 0.4), (x + 0.3, random.choice((-1, 1)) * 0.9, 0.9), 0.06, "PineBark")

asset("SM_LogCabin", "Forest", "Forest_Props", 9, place=dict(density=4, scale=(1, 1), spacing=60, yaw=True, align=True, cluster=1))
box("Walls", (6.0, 4.6, 2.6), "Logs", loc=(0, 0, 1.3))
gable("Roof", 6.0, 4.6, 1.8, 2.6, "WoodDark", over=0.5)
for s in (-1, 1):
    extrude(f"Gable{s}", [(-2.3, 2.6), (2.3, 2.6), (0, 4.35)], s * 3.0 - 0.05, s * 3.0 + 0.05, "Wood", rot=(0, 0, math.pi / 2))
box("Door", (1.0, 0.1, 1.9), "WoodDark", loc=(0.8, -2.32, 0.95))
for x in (-1.6, 2.2):
    box(f"Window{x}", (0.8, 0.1, 0.7), "WindowGlow" if x < 0 else "Dark", loc=(x, -2.32, 1.5))
box("Chimney", (0.8, 0.8, 3.0), "Stone", loc=(-2.7, 1.2, 3.3))
box("Woodpile", (1.6, 0.8, 0.8), "WoodEnd", loc=(2.5, 2.7, 0.4))

asset("SM_RangerTower", "Forest", "Forest_Props", 8, place=dict(density=1, scale=(1, 1), spacing=200, yaw=True, align=False, cluster=1),
      notes="Wooden fire lookout, 13 m: shorter than the tall pines, no collision")
for sx in (-1, 1):
    for sy in (-1, 1):
        bar(f"Leg{sx}{sy}", (sx * 2.2, sy * 2.2, 0), (sx * 1.3, sy * 1.3, 10.5), 0.14, "Wood")
for i, z in enumerate((3.0, 6.5)):
    w = 2.2 - (2.2 - 1.3) * z / 10.5
    for k in range(4):
        a, b_ = [(-w, -w), (w, -w), (w, w), (-w, w)][k], [(-w, -w), (w, -w), (w, w), (-w, w)][(k + 1) % 4]
        bar(f"Rail{i}{k}", (*a, z), (*b_, z), 0.08, "Wood")
box("Floor", (3.4, 3.4, 0.25), "WoodDark", loc=(0, 0, 10.6))
box("Cabin", (2.8, 2.8, 1.1), "Wood", loc=(0, 0, 11.3))
box("CabinGlass", (2.6, 2.6, 0.9), "Dark", loc=(0, 0, 12.3))
hip("Roof", 3.0, 3.0, 1.0, 12.75, "WoodDark", over=0.3)
bar("Ladder", (2.0, -2.0, 0), (1.2, -1.2, 10.5), 0.09, "WoodDark")

group_mount_sizes = ((900, 340, 1), (1400, 520, 2), (650, 230, 1))
for k, (wd, ht, peaks) in enumerate(group_mount_sizes):
    asset(f"SM_Mountain_{'ABC'[k]}", "Forest", "Forest_Mountains", wd, row=-1, cat="Background", uvscale=1.0,
          place=dict(density=0, scale=(0.8, 1.5), spacing=600, yaw=True, align=False, cluster=1, ring=True),
          notes="Ultra low poly background mountain: place in a ring around the playable area")
    if peaks == 1:
        mountain_peak("Peak", 0, 0, wd / 2, ht, ht * 0.62)
    else:
        mountain_peak("PeakA", -wd * 0.18, 0, wd * 0.36, ht, ht * 0.6)
        mountain_peak("PeakB", wd * 0.22, wd * 0.05, wd * 0.30, ht * 0.78, ht * 0.6)

# ================================================================ FIELD
group("Field", "Field_Props", 512)
group("Field", "Field_Bird", 128)

asset("SK_Bird", "Field", "Field_Bird", 2, kind="skeletal", cat="Creature",
      place=dict(flock=(8, 24), speed=(9, 14), height=(15, 120)),
      notes="Flock bird with wing-flap animation; collides with the balloon (sphere ~0.5 m)")
bone("root", (0, 0, 0), (0, 0.15, 0))
bone("body", (0, -0.05, 0), (0, 0.22, 0), "root")
for s, tag in ((-1, "L"), (1, "R")):
    bone(f"wing_{tag}", (s * 0.06, 0.02, 0.03), (s * 0.27, 0.0, 0.03), "body")
    bone(f"wingtip_{tag}", (s * 0.27, 0.0, 0.03), (s * 0.52, -0.04, 0.03), f"wing_{tag}")
bone("tail", (0, -0.12, 0.01), (0, -0.3, 0.01), "body")
part_bone("body")
ball("Body", 0.07, (0, 0.0, 0.0), "Feather", u=6, v=4, scale=(1, 2.6, 1))
ball("Head", 0.055, (0, 0.18, 0.035), "Feather", u=6, v=4)
prism("Beak", 4, 0.022, 0, 0, 0.08, "Beak", loc=(0, 0.22, 0.03), rot=(-math.pi / 2, 0, 0), bottom=True)
part_bone("tail")
extrude("Tail", [(0, -0.1), (0.075, -0.32), (-0.075, -0.32)], -0.007, 0.007, "Feather", rot=(-math.pi / 2, 0, 0), loc=(0, 0, 0.01))
for s, tag in ((-1, "L"), (1, "R")):
    part_bone(f"wing_{tag}")
    extrude(f"Wing{tag}", [(s * 0.04, 0.07), (s * 0.27, 0.05), (s * 0.27, -0.08), (s * 0.04, -0.07)], -0.008, 0.008, "Feather",
            rot=(-math.pi / 2, 0, 0), loc=(0, 0, 0.03))
    part_bone(f"wingtip_{tag}")
    extrude(f"Tip{tag}", [(s * 0.27, 0.05), (s * 0.53, -0.03), (s * 0.5, -0.11), (s * 0.27, -0.08)], -0.006, 0.006, "Feather",
            rot=(-math.pi / 2, 0, 0), loc=(0, 0, 0.03))
FLAP = []
for f in range(13):
    t = 2 * math.pi * f / 12
    a, tip = 38 * math.cos(t), 26 * math.cos(t - 0.9)
    FLAP.append((f + 1, {"wing_L": (0, 1, 0, a), "wing_R": (0, 1, 0, -a), "wingtip_L": (0, 1, 0, tip), "wingtip_R": (0, 1, 0, -tip),
                          "tail": (1, 0, 0, 6 * math.sin(t))}, {"root": (0, 0, -0.03 * math.cos(t))}))
GLIDE = [(f, {"wing_L": (0, 1, 0, 9), "wing_R": (0, 1, 0, -9), "wingtip_L": (0, 1, 0, 5), "wingtip_R": (0, 1, 0, -5)}, {}) for f in (1, 2)]
ASSETS["SK_Bird"]["anims"] = {"Bird_Flap": FLAP, "Bird_Glide": GLIDE}
part_bone(None)

asset("SM_HayBale_Round", "Field", "Field_Props", 3, place=dict(density=60, scale=(0.9, 1.1), spacing=3, yaw=True, align=True, cluster=6))
prism("Bale", 10, 0.75, 0.75, -0.6, 0.6, "Hay", rot=(0, math.pi / 2, 0), loc=(0, 0, 0.75))

asset("SM_HayStack", "Field", "Field_Props", 5, place=dict(density=25, scale=(0.8, 1.2), spacing=8, yaw=True, align=True, cluster=3))
prism("StackLo", 8, 2.0, 2.2, 0, 1.4, "Hay", jitter=0.06, top=False)
prism("StackHi", 8, 2.2, 0, 1.4, 3.6, "Hay", jitter=0.06, bottom=False)
bar("Pole", (0, 0, 3.0), (0, 0, 4.2), 0.06, "WoodDark")

asset("SM_Fence", "Field", "Field_Props", 5, place=dict(density=0, scale=(1, 1), spacing=4, yaw=False, align=True, cluster=1, line=True),
      notes="4 m fence section: place end to end along field borders")
for x in (-2, 2):
    box(f"Post{x}", (0.16, 0.16, 1.3), "WoodDark", loc=(x, 0, 0.65))
for z in (0.5, 1.0):
    box(f"Rail{z}", (4.2, 0.08, 0.14), "Wood", loc=(0, 0, z))

asset("SM_Hedge", "Field", "Field_Props", 8, place=dict(density=0, scale=(0.8, 1.5), spacing=6, yaw=False, align=True, cluster=1, line=True),
      notes="6 m hedge: lines between fields")
for i in range(3):
    ball(f"Bush{i}", 1.3, (-2 + 2 * i, 0, 0.9), "Hedge", u=6, v=4, scale=(1.2, 0.8, 0.85), jitter=0.12)

asset("SM_Farmhouse", "Field", "Field_Props", 12, place=dict(density=3, scale=(1, 1), spacing=120, yaw=True, align=True, cluster=1))
house("", 10.4, 7.0, 5.6, "FacadePlaster", roof="Thatch", rh=3.4, chimneys=2, over=0.6)
box("Door", (1.1, 0.12, 2.1), "PaintGreen", loc=(0, -3.52, 1.05))
box("Porch", (2.4, 1.4, 0.3), "Stone", loc=(0, -4.1, 0.15))

asset("SM_Barn", "Field", "Field_Props", 15, place=dict(density=3, scale=(1, 1), spacing=120, yaw=True, align=True, cluster=1))
prof = [(-5, 0), (5, 0), (5, 5.0), (3.6, 7.6), (0, 8.8), (-3.6, 7.6), (-5, 5.0)]
extrude("Barn", prof, -7, 7, "BarnRed")
roof = [(-5.4, 4.8), (-3.8, 7.75), (0, 9.05), (3.8, 7.75), (5.4, 4.8), (5.4, 5.1), (3.85, 8.05), (0, 9.35), (-3.85, 8.05), (-5.4, 5.1)]
extrude("Roof", roof, -7.3, 7.3, "IronDark")
box("Door", (0.12, 4.4, 4.2), "WoodDark", loc=(5.02, 0, 2.1))
box("Loft", (0.12, 1.6, 1.4), "Dark", loc=(5.02, 0, 6.6))

asset("SM_Windmill", "Field", "Field_Props", 10, place=dict(density=1, scale=(1, 1), spacing=300, yaw=True, align=True, cluster=1),
      notes="Attach SM_Windmill_Blades to SOCKET_blades and spin it around its forward (Y) axis")
prism("Tower", 8, 3.4, 2.2, 0, 11.5, "Plaster", jitter=0.0)
prism("Cap", 8, 2.6, 0.6, 11.5, 14.0, "Thatch")
box("CapBox", (2.0, 2.6, 1.6), "WoodDark", loc=(0, 1.6, 12.5))
box("Door", (1.0, 0.1, 2.0), "WoodDark", loc=(0, -3.2, 1.0), rot=(0, 0, 0))
for i, z in enumerate((4.0, 8.0)):
    box(f"Win{i}", (0.7, 0.1, 0.9), "Dark", loc=(0, -(3.4 - (1.2 * z / 11.5)) + 0.05, z))
socket("blades", (0, 2.95, 12.6))

asset("SM_Windmill_Blades", "Field", "Field_Props", 20, place=dict(density=0), notes="4 sails, pivot = hub, spin around Y")
bar("Hub", (0, -0.4, 0), (0, 0.5, 0), 0.4, "WoodDark", n=8)
for k in range(4):
    a = k * math.pi / 2 + math.pi / 4
    d = Vector((math.cos(a), 0, math.sin(a))); side = Vector((-math.sin(a), 0, math.cos(a)))
    bar(f"Spar{k}", d * 0.3, d * 9.5, 0.12, "WoodDark")
    c0, c1 = d * 2.0, d * 9.3
    quad = [c0, c1, c1 + side * 1.8, c0 + side * 1.8]
    bm = bmesh.new(); vs = [bm.verts.new(q + Vector((0, 0.05, 0))) for q in quad]; bm.faces.new(vs)
    vs2 = [bm.verts.new(q + Vector((0, -0.05, 0))) for q in quad]; bm.faces.new(list(reversed(vs2)))
    put(bm, f"Sail{k}", "Canvas")
    for t in (0.0, 0.5, 1.0):
        p0 = c0 + (c1 - c0) * t
        bar(f"Lat{k}{t}", p0, p0 + side * 1.8, 0.05, "Wood")

for k, (h, cr) in enumerate(((7.0, 3.0), (9.0, 3.8))):
    asset(f"SM_Tree_Oak_{'AB'[k]}", "Field", "Field_Props", 9, place=dict(density=30, scale=(0.8, 1.3), spacing=10, yaw=True, align=False, cluster=2))
    prism("Trunk", 6, 0.45, 0.28, 0, h * 0.55, "Bark", jitter=0.08)
    for i in range(3):
        a = i * 2.1 + random.uniform(0, 0.5)
        ball(f"Crown{i}", cr * random.uniform(0.75, 0.95), (cr * 0.35 * math.cos(a), cr * 0.35 * math.sin(a), h * 0.62 + i * 0.5),
             "Leaf", u=7, v=5, scale=(1, 1, 0.8), jitter=0.12)

asset("SM_Scarecrow", "Field", "Field_Props", 3, place=dict(density=4, scale=(1, 1.1), spacing=40, yaw=True, align=True, cluster=1))
bar("Pole", (0, 0, 0), (0, 0, 2.3), 0.06, "WoodDark")
bar("Arms", (-0.9, 0, 1.6), (0.9, 0, 1.6), 0.05, "WoodDark")
box("Coat", (0.7, 0.3, 0.8), "CanvasTorn", loc=(0, 0, 1.45))
ball("Head", 0.2, (0, 0, 2.05), "Canvas", u=6, v=4)
prism("Hat", 6, 0.32, 0.12, 2.18, 2.5, "WoodDark")

asset("SM_HayCart", "Field", "Field_Props", 6, place=dict(density=3, scale=(1, 1), spacing=40, yaw=True, align=True, cluster=1))
box("Bed", (1.6, 3.2, 0.25), "Wood", loc=(0, 0, 1.0))
box("Load", (1.5, 2.8, 1.0), "Hay", loc=(0, 0, 1.6))
for s in (-1, 1):
    prism(f"Wheel{s}", 8, 0.6, 0.6, -0.06, 0.06, "WoodDark", rot=(0, math.pi / 2, 0), loc=(s * 0.9, -0.3, 0.6))
    bar(f"Shaft{s}", (s * 0.5, 1.5, 1.0), (s * 0.5, 3.5, 0.6), 0.05, "Wood")

# ================================================================ CITY (Victorian London)
group("City", "City_Props", 1024)
group("City", "City_ClockTower", 1024)
group("City", "City_TowerA", 1024)
group("City", "City_TowerB", 1024)
group("City", "City_Jammer", 512)

SHORT = dict(density=180, scale=(0.9, 1.1), spacing=4, yaw=False, align=True, cluster=6, street=True)

asset("SM_House_Terrace_A", "City", "City_Props", 18, place=SHORT, notes="Row of 3 brick houses, 3 floors")
house("", 14.4, 9.0, 9.6, "FacadeBrick", roof="Slate", chimneys=3)
for i in range(3):
    box(f"Bay{i}", (2.4, 1.0, 3.0), "FacadeBrick", loc=(-4.8 + 4.8 * i, -5.0, 1.5))
    box(f"BayRoof{i}", (2.6, 1.2, 0.3), "Slate", loc=(-4.8 + 4.8 * i, -5.0, 3.1))
    box(f"Door{i}", (1.0, 0.12, 2.1), "PaintGreen" if i % 2 else "WoodDark", loc=(-3.2 + 4.8 * i, -4.53, 1.05))

asset("SM_House_Terrace_B", "City", "City_Props", 16, place=SHORT, notes="4-floor stucco terrace with mansard roof and dormers")
house("", 12.0, 10.0, 13.6, "FacadeStucco", roof="Slate", rh=3.0, kind="mansard", chimneys=2)
for i in range(3):
    box(f"Dormer{i}", (1.4, 1.0, 1.6), "FacadeStucco", loc=(-4 + 4 * i, -4.6, 14.6))
    gable(f"DormerRoof{i}", 1.4, 1.0, 0.6, 15.4, "Slate", over=0.15, loc=(-4 + 4 * i, -4.6, 0), rot=(0, 0, math.pi / 2))
box("Cornice", (12.4, 10.4, 0.4), "Stone", loc=(0, 0, 13.6))
box("Portico", (3.0, 1.6, 0.4), "Stone", loc=(0, -5.6, 3.4))
for x in (-1.3, 1.3):
    bar(f"Column{x}", (x, -6.2, 0), (x, -6.2, 3.3), 0.15, "Stone", n=6)

asset("SM_House_Terrace_C", "City", "City_Props", 11, place=SHORT, notes="Narrow 2-floor sooty house, hipped roof")
house("", 7.2, 8.0, 6.4, "FacadeSoot", roof="RoofTile", rh=2.6, kind="hip", chimneys=1)
box("Door", (1.0, 0.12, 2.1), "PaintRed", loc=(1.8, -4.03, 1.05))

asset("SM_Shop_Corner", "City", "City_Props", 14, place=SHORT, notes="Corner shop with awning and a turret")
house("", 9.6, 9.6, 9.6, "FacadeBrick", roof="Slate", kind="hip", rh=2.4, chimneys=1)
for s, rot in ((-1, 0), (1, math.pi / 2)):
    box(f"Shopfront{s}", (9.7, 0.3, 3.0) if s < 0 else (0.3, 9.7, 3.0), "PaintGreen", loc=(0, -4.9, 1.5) if s < 0 else (4.9, 0, 1.5))
    box(f"Awning{s}", (9.6, 1.4, 0.2) if s < 0 else (1.4, 9.6, 0.2), "PaintRed", loc=(0, -5.5, 3.2) if s < 0 else (5.5, 0, 3.2), rot=(0.25, 0, 0) if s < 0 else (0, -0.25, 0))
    box(f"ShopGlass{s}", (7.0, 0.12, 1.8) if s < 0 else (0.12, 7.0, 1.8), "WindowGlow", loc=(-0.5, -5.06, 1.4) if s < 0 else (5.06, 0.5, 1.4))
prism("Turret", 8, 1.5, 1.5, 3.2, 11.0, "FacadeBrick", loc=(4.9, -4.9, 0))
prism("TurretRoof", 8, 1.8, 0, 11.0, 14.0, "Slate", loc=(4.9, -4.9, 0))

asset("SM_Church", "City", "City_Props", 28, cat="Obstacle", place=dict(density=4, scale=(1, 1), spacing=120, yaw=False, align=True, cluster=1),
      notes="Spire reaches 40 m, so it has simple collision")
box("Nave", (10, 22, 11), "Stone", loc=(0, 2, 5.5))
gable("NaveRoof", 22, 10, 5.0, 11, "Slate", rot=(0, 0, math.pi / 2), over=0.4)
for i in range(5):
    for s in (-1, 1):
        box(f"Window{i}{s}", (0.15, 1.2, 4.0), "Glass", loc=(s * 5.02, -5 + 4 * i, 5.5))
box("Tower", (6, 6, 24), "Stone", loc=(0, -11, 12))
box("Belfry", (6.4, 6.4, 1.0), "StoneDark", loc=(0, -11, 24.4))
prism("Spire", 4, 4.2, 0, 24.9, 41.0, "Slate", loc=(0, -11, 0), phase=math.pi / 4)
for s in (-1, 1):
    box(f"Louvre{s}", (0.2, 1.4, 3.0), "Dark", loc=(s * 3.02, -11, 20.5))
box("Door", (2.0, 0.2, 3.6), "WoodDark", loc=(0, -14.05, 1.8))
ucx_box((6, 6, 25), (0, -11, 12.5))
ucx_prism(4, 4.2, 0.1, 24.9, 41.0, c=(0, -11))
ucx_box((10, 22, 16), (0, 2, 8))

asset("SM_Warehouse", "City", "City_Props", 26, place=dict(density=25, scale=(0.9, 1.1), spacing=8, yaw=False, align=True, cluster=3, street=True))
box("Hall", (20, 12, 8), "FacadeSoot", loc=(0, 0, 4))
for i in range(4):
    x0 = -10 + 5 * i
    extrude(f"Tooth{i}", [(x0, 8), (x0 + 5, 8), (x0 + 5, 11)], -6, 6, "IronDark")
    box(f"Skylight{i}", (0.12, 12, 2.9), "Glass", loc=(x0 + 5, 0, 9.5))
box("Door", (4.0, 0.15, 4.5), "WoodDark", loc=(-4, -6.05, 2.25))
box("Crane", (0.6, 2.0, 0.6), "IronDark", loc=(-4, -7, 6.5))

def factory_chimney(p, x, y, r, h, n=8):
    prism(p, n, r, r * 0.72, 0, h, "SootBrick", loc=(x, y, 0))
    for i, z in enumerate((h * 0.35, h * 0.7, h - 0.8)):
        rr = r - (r - r * 0.72) * z / h
        prism(f"{p}Band{i}", n, rr + 0.12, rr + 0.12, z, z + 0.5, "IronDark", loc=(x, y, 0))
    prism(f"{p}Lip", n, r * 0.72 + 0.25, r * 0.72 + 0.25, h - 0.2, h + 0.5, "SootBrick", loc=(x, y, 0))

asset("SM_Factory_A", "City", "City_Props", 34, cat="Obstacle", place=dict(density=10, scale=(1, 1), spacing=40, yaw=False, align=True, cluster=2),
      notes="Smoke from SOCKET_smoke_1 / _2 (chimney tops). Chimneys reach 32 m and collide")
box("Hall", (24, 14, 10), "FacadeFactory", loc=(0, 0, 5))
for i in range(4):
    x0 = -12 + 6 * i
    extrude(f"Tooth{i}", [(x0, 10), (x0 + 6, 10), (x0 + 6, 13.5)], -7, 7, "Slate")
    box(f"Skylight{i}", (0.12, 14, 3.4), "WindowGlow", loc=(x0 + 6, 0, 11.75))
box("Office", (8, 6, 7), "FacadeBrick", loc=(-8, -10, 3.5))
gable("OfficeRoof", 8, 6, 2.0, 7, "Slate")
box("GateDoor", (5.0, 0.2, 5.0), "IronDark", loc=(6, -7.05, 2.5))
for i, x in enumerate((6.0, 11.0)):
    factory_chimney(f"Chimney{i}", x, 9.5, 1.5, 32 - 4 * i)
    socket(f"smoke_{i + 1}", (x, 9.5, 32 - 4 * i + 0.5))
    ucx_prism(8, 1.6, 1.3, 0, 32 - 4 * i + 0.5, c=(x, 9.5))
bar("Pipe", (-12, 7, 6), (5, 9.5, 6), 0.4, "Iron", n=6)
ucx_box((24, 14, 13.5), (0, 0, 6.75))

asset("SM_Factory_B", "City", "City_Props", 30, cat="Obstacle", place=dict(density=8, scale=(1, 1), spacing=40, yaw=False, align=True, cluster=2),
      notes="Smoke from SOCKET_smoke_1 (chimney top). Chimney reaches 40 m and collides")
box("Block", (18, 18, 14), "FacadeFactory", loc=(0, 0, 7))
box("BlockRoof", (18.6, 18.6, 0.8), "StoneDark", loc=(0, 0, 14.4))
box("Chimney", (3.0, 3.0, 40), "Brick", loc=(6, 6, 20))
box("ChimneyCap", (3.6, 3.6, 1.2), "SootBrick", loc=(6, 6, 39.6))
for z in (14, 26):
    box(f"ChimneyBand{z}", (3.3, 3.3, 0.5), "IronDark", loc=(6, 6, z))
socket("smoke_1", (6, 6, 40.5))
prism("Tank", 10, 2.6, 2.6, 18, 22, "Iron", loc=(-5, -4, 0))
for sx in (-1, 1):
    for sy in (-1, 1):
        bar(f"TankLeg{sx}{sy}", (-5 + sx * 1.8, -4 + sy * 1.8, 14.8), (-5 + sx * 1.8, -4 + sy * 1.8, 18), 0.12, "IronDark")
box("Furnace", (4, 3, 3), "IronDark", loc=(-5, -10.5, 1.5))
box("FurnaceMouth", (2.0, 0.1, 1.4), "Fire", loc=(-5, -12.05, 1.1))
ucx_box((18, 18, 14.8), (0, 0, 7.4))
ucx_box((3.6, 3.6, 40.2), (6, 6, 20.1))

asset("SM_Gasholder", "City", "City_Props", 32, place=dict(density=3, scale=(0.9, 1.1), spacing=80, yaw=True, align=True, cluster=1),
      notes="Iron gas holder frame, 20 m")
prism("Drum", 12, 13.0, 13.0, 0, 13.0, "IronDark", top=True, bottom=False)
prism("DrumTop", 12, 13.0, 9.0, 13.0, 14.2, "Iron", bottom=False)
for i in range(12):
    a = 2 * math.pi * i / 12 + math.pi / 12
    bar(f"Col{i}", (14.5 * math.cos(a), 14.5 * math.sin(a), 0), (14.5 * math.cos(a), 14.5 * math.sin(a), 20), 0.35, "Iron")
for z in (10, 20):
    ring_bars(f"Girder{z}_", 14.5, z, "Iron", n=12, th=0.25)

asset("SM_Lamppost", "City", "City_Props", 2, place=dict(density=400, scale=(1, 1), spacing=12, yaw=True, align=False, cluster=1, street=True))
prism("Base", 6, 0.22, 0.16, 0, 0.6, "IronDark")
bar("Post", (0, 0, 0.6), (0, 0, 4.0), 0.07, "IronDark", n=6)
prism("Lantern", 4, 0.22, 0.3, 4.0, 4.6, "LampGlow", phase=math.pi / 4)
prism("LanternCap", 4, 0.36, 0.05, 4.6, 4.85, "IronDark", phase=math.pi / 4)

# -- tall buildings (collide, own textures)
asset("SM_ClockTower", "City", "City_ClockTower", 16, row=1, cat="Obstacle", place=dict(density=1, scale=(1, 1), spacing=400, yaw=True, align=True, cluster=1),
      notes="62 m Victorian clock tower, collision")
box("Base", (10, 10, 4), "StoneDark", loc=(0, 0, 2))
box("Shaft", (8, 8, 38), "FacadeTower", loc=(0, 0, 23))
for z in (12, 24, 36):
    box(f"Cornice{z}", (8.6, 8.6, 0.6), "Stone", loc=(0, 0, z))
box("ClockStage", (9.2, 9.2, 7), "Stone", loc=(0, 0, 45.5))
for k in range(4):
    a = k * math.pi / 2
    n = Vector((math.sin(a), -math.cos(a), 0)); u = Vector((math.cos(a), math.sin(a), 0))
    c = n * 4.62 + Vector((0, 0, 45.5))
    rot = (math.pi / 2, 0, a)
    prism(f"Clock{k}", 16, 2.6, 2.6, 0, 0.12, "ClockGlow", loc=c, rot=rot)
    prism(f"ClockRim{k}", 16, 2.9, 2.9, -0.05, 0.08, "Brass", loc=c, rot=rot)
    for t in range(12):
        ang = 2 * math.pi * t / 12
        p = c + n * 0.14 + u * (2.2 * math.sin(ang)) + Vector((0, 0, 2.2 * math.cos(ang)))
        box(f"Tick{k}{t}", (0.18, 0.06, 0.45) if t % 3 else (0.3, 0.06, 0.7), "Dark", loc=p, rot=(0, -ang, a))
    bar(f"HandH{k}", c + n * 0.2, c + n * 0.2 + u * 1.2 + Vector((0, 0, 0.6)), 0.08, "Dark")
    bar(f"HandM{k}", c + n * 0.22, c + n * 0.22 + Vector((0, 0, 2.0)), 0.06, "Dark")
box("Belfry", (7.6, 7.6, 5), "FacadeTower", loc=(0, 0, 51.5))
for k in range(4):
    a = k * math.pi / 2
    n = Vector((math.sin(a), -math.cos(a), 0))
    box(f"Arch{k}", (0.2, 3.0, 3.6), "Dark", loc=n * 3.82 + Vector((0, 0, 51.5)), rot=(0, 0, a + math.pi / 2))
for sx in (-1, 1):
    for sy in (-1, 1):
        prism(f"Pinnacle{sx}{sy}", 4, 0.6, 0, 54, 57.5, "Copper", loc=(sx * 3.6, sy * 3.6, 0), phase=math.pi / 4)
prism("Spire", 4, 5.6, 0, 54, 62.0, "Copper", phase=math.pi / 4)
bar("Finial", (0, 0, 61.5), (0, 0, 64.5), 0.12, "Brass", n=6)
ucx_box((10, 10, 54), (0, 0, 27))
ucx_prism(4, 5.6, 0.1, 54, 62.0)

asset("SM_Tower_Office_A", "City", "City_TowerA", 20, row=1, cat="Obstacle", place=dict(density=6, scale=(0.9, 1.15), spacing=60, yaw=False, align=True, cluster=2),
      notes="48 m stone office tower, collision")
box("Base", (16, 16, 6), "Stone", loc=(0, 0, 3))
box("Shaft", (14, 14, 40), "FacadeTower", loc=(0, 0, 26))
for z in (6, 18, 32, 46):
    box(f"Cornice{z}", (14.8, 14.8, 0.7), "Stone", loc=(0, 0, z))
hip("Roof", 14, 14, 4, 46, "Copper", over=0.4)
prism("Tank", 8, 1.8, 1.8, 48.2, 51.2, "WoodDark", loc=(2.5, 2.5, 0))
for x in (-4, 0):
    box(f"Chimney{x}", (1.2, 1.2, 3.6), "SootBrick", loc=(x, -4, 49.3))
box("Entrance", (5, 0.4, 4.5), "Dark", loc=(0, -8.1, 2.25))
box("Canopy", (6, 1.5, 0.3), "IronDark", loc=(0, -8.6, 4.7))
ucx_box((16, 16, 46.4), (0, 0, 23.2))
ucx_pts([(-7.4, -7.4, 46), (7.4, -7.4, 46), (7.4, 7.4, 46), (-7.4, 7.4, 46), (-2, -2, 53), (2, -2, 53), (2, 2, 53), (-2, 2, 53)])

asset("SM_Tower_Office_B", "City", "City_TowerB", 24, row=1, cat="Obstacle", place=dict(density=6, scale=(0.9, 1.15), spacing=60, yaw=False, align=True, cluster=2),
      notes="56 m sooty brick tower with mansard top and flagpole, collision")
box("Lower", (20, 14, 22), "FacadeBrick", loc=(0, 0, 11))
box("Upper", (14, 12, 26), "FacadeSoot", loc=(-2, 0, 35))
box("Ledge", (20.6, 14.6, 0.6), "Stone", loc=(0, 0, 22))
box("Ledge2", (14.6, 12.6, 0.6), "Stone", loc=(-2, 0, 48))
hull("Mansard", [Vector(q) for q in ((-9.2, -6.2, 48.3), (5.2, -6.2, 48.3), (5.2, 6.2, 48.3), (-9.2, 6.2, 48.3),
                                       (-7.5, -4.5, 53.5), (3.5, -4.5, 53.5), (3.5, 4.5, 53.5), (-7.5, 4.5, 53.5))], "Slate")
for i in range(4):
    box(f"Dormer{i}", (1.4, 1.0, 1.8), "FacadeSoot", loc=(-7.5 + 3.4 * i, -5.6, 50.0))
bar("Flagpole", (-2, 0, 53.5), (-2, 0, 61), 0.1, "IronDark", n=6)
box("Flag", (2.4, 0.05, 1.4), "PaintRed", loc=(-0.8, 0, 60.2))
box("Entrance", (4, 0.4, 4.5), "Dark", loc=(4, -7.1, 2.25))
ucx_box((20, 14, 22.3), (0, 0, 11.15))
ucx_box((14.6, 12.6, 31.6), (-2, 0, 38))

asset("SM_JammerTower", "City", "City_Jammer", 18, row=1, cat="Obstacle", place=dict(density=2, scale=(0.9, 1.1), spacing=250, yaw=True, align=True, cluster=1),
      notes="Giant antenna: lattice mast with copper coils and red warning lights (glow slot). SOCKET_beacon_1..3 for blinking lights, SOCKET_jam for a jamming effect")
H, B, Tp = 66.0, 5.5, 1.1
segs = 8
lvl = [i * H / segs for i in range(segs + 1)]
wid = lambda z: B - (B - Tp) * z / H
for sx in (-1, 1):
    for sy in (-1, 1):
        bar(f"Leg{sx}{sy}", (sx * B, sy * B, 0), (sx * Tp, sy * Tp, H), 0.35, "IronDark")
corner = lambda z, k: [(-1, -1), (1, -1), (1, 1), (-1, 1)][k % 4]
for i in range(segs):
    z0, z1 = lvl[i], lvl[i + 1]
    for k in range(4):
        a0, b0 = corner(z0, k), corner(z0, k + 1)
        P = lambda c, z: (c[0] * wid(z), c[1] * wid(z), z)
        bar(f"Ring{i}{k}", P(a0, z1), P(b0, z1), 0.18, "IronDark")
        bar(f"X{i}{k}a", P(a0, z0), P(b0, z1), 0.12, "Iron")
        bar(f"X{i}{k}b", P(b0, z0), P(a0, z1), 0.12, "Iron")
for i, z in enumerate((22.0, 44.0)):
    w = wid(z)
    box(f"Platform{i}", (2 * w + 1.4, 2 * w + 1.4, 0.3), "IronDark", loc=(0, 0, z))
    for sx in (-1, 1):
        ball(f"Beacon{i}{sx}", 0.45, (sx * (w + 0.6), -(w + 0.6), z + 0.5), "RedLight", u=6, v=4)
    socket(f"beacon_{i + 1}", (0, 0, z + 0.6))
bar("Mast", (0, 0, H), (0, 0, H + 14), 0.45, "Iron", n=8)
for i, (z, r) in enumerate(((H + 3, 2.6), (H + 7, 2.0), (H + 10.5, 1.4))):
    prism(f"CoilRing{i}", 12, r, r, z - 0.35, z + 0.35, "Copper")
    prism(f"CoilHole{i}", 12, r - 0.4, r - 0.4, z - 0.36, z + 0.36, "IronDark")
for k in range(4):
    a = k * math.pi / 2
    d = Vector((math.cos(a), math.sin(a), 0))
    bar(f"Arm{k}", Vector((0, 0, H + 1.5)), d * 7.0 + Vector((0, 0, H + 1.5)), 0.15, "Iron")
    for t in (2.5, 4.5, 6.5):
        prism(f"Insulator{k}{t}", 6, 0.35, 0.35, -0.12, 0.12, "Glass", loc=d * t + Vector((0, 0, H + 1.5)), rot=(0, math.pi / 2, a))
    bar(f"Rod{k}", d * 7.0 + Vector((0, 0, H + 1.5)), d * 7.0 + Vector((0, 0, H + 6.0)), 0.08, "Copper")
ball("TopSphere", 1.3, (0, 0, H + 15), "Copper", u=8, v=6)
ball("TopBeacon", 0.55, (0, 0, H + 16.5), "RedLight", u=6, v=4)
socket("beacon_3", (0, 0, H + 16.5))
socket("jam", (0, 0, H + 10))
box("Hut", (5, 4, 3), "FacadeSoot", loc=(0, -9, 1.5))
gable("HutRoof", 5, 4, 1.2, 3, "IronDark")
ucx_pts([(sx * B * 1.05, sy * B * 1.05, 0) for sx in (-1, 1) for sy in (-1, 1)] + [(sx * Tp * 1.3, sy * Tp * 1.3, H) for sx in (-1, 1) for sy in (-1, 1)])
ucx_prism(8, 7.2, 7.2, H, H + 17.5)

# ================================================================ DESERT
group("Desert", "Desert_Props", 512)
group("Desert", "Desert_GuardTower", 512)
group("Desert", "Desert_Lighthouse", 1024)
group("Desert", "Desert_Beam", 0)             # no texture: the beam uses its own translucent material

def dune(p, L_, W_, H_, seg=14, rings=(0.0, 0.22, 0.45, 0.68, 0.86, 1.0)):
    """Crescent (barchan) dune on an elliptical grid: gentle windward slope (-Y), steep slip face (+Y),
    horns curving downwind. The outer ring sits just below the ground so it blends in."""
    bm = bmesh.new()
    a, b = L_ / 2, W_ / 2
    crest = Vector((0, 0.18 * b))
    grid = []
    for rf in rings:
        ring = []
        for i in range(seg if rf > 0 else 1):
            th = 2 * math.pi * i / seg
            x = a * rf * math.cos(th)
            y = b * rf * math.sin(th) * (0.75 if math.sin(th) > 0 else 1.0)
            y += 0.45 * b * (x / a) ** 2                      # horns bend downwind
            lee = math.sin(th) > 0
            t = rf ** (0.6 if lee else 1.0)
            h = H_ * max(0.0, 1 - t) ** (1.0 if lee else 1.6)
            q = Vector((x, y)) + crest * (1 - rf)
            z = -0.3 if rf >= 1.0 else h * random.uniform(0.95, 1.05)
            ring.append(bm.verts.new((q.x + random.uniform(-.2, .2), q.y + random.uniform(-.2, .2), z)))
        grid.append(ring)
    c = grid[0][0]
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((c, grid[1][i], grid[1][j]))
    for k in range(1, len(grid) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((grid[k][i], grid[k + 1][i], grid[k + 1][j], grid[k][j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    put(bm, p, "Sand")

for k, (L_, W_, H_) in enumerate(((120, 80, 14), (200, 120, 22), (70, 50, 8))):
    asset(f"SM_Dune_{'ABC'[k]}", "Desert", "Desert_Props", L_, row=-1, cat="Terrain",
          place=dict(density=12, scale=(0.8, 1.4), spacing=60, yaw=False, align=False, cluster=1, wind_aligned=True),
          notes="Crescent dune, all dunes face the same wind (+Y downwind)")
    dune("Dune", L_, W_, H_)

asset("SM_Mesa", "Desert", "Desert_Props", 70, row=2, cat="Obstacle", place=dict(density=1.5, scale=(0.7, 1.3), spacing=300, yaw=True, align=False, cluster=1),
      notes="Flat-topped rock, 34 m: collides")
prism("Mesa", 9, 32, 27, -0.5, 34, "RockDesert", jitter=0.12)
prism("Scree", 9, 38, 31, -0.5, 6, "RockDesert", jitter=0.15, top=False)
ucx_prism(9, 33, 27, 0, 34)

asset("SM_Hoodoo", "Desert", "Desert_Props", 14, row=2, cat="Obstacle", place=dict(density=4, scale=(0.7, 1.4), spacing=60, yaw=True, align=False, cluster=2),
      notes="Rock pillar, 20 m: collides")
prism("Pillar", 7, 5.0, 3.2, -0.3, 16, "RockDesert", jitter=0.15)
rock("Cap", 4.5, (0.5, 0, 17.5), "RockDesert", flat=0.45, jit=0.15)
ucx_prism(7, 5.2, 4.6, 0, 19.5)

for k, r in enumerate((1.8, 3.2)):
    asset(f"SM_Rock_Desert_{'AB'[k]}", "Desert", "Desert_Props", 7, place=dict(density=60, scale=(0.6, 1.6), spacing=8, yaw=True, align=True, cluster=3))
    rock("Rock", r, (0, 0, r * 0.25), "RockDesert", flat=0.55)

for k, (h, arms) in enumerate(((5.0, 2), (3.5, 1))):
    asset(f"SM_Cactus_{'AB'[k]}", "Desert", "Desert_Props", 4, place=dict(density=120, scale=(0.7, 1.3), spacing=6, yaw=True, align=False, cluster=2))
    prism("Trunk", 6, 0.32, 0.24, 0, h, "Cactus")
    prism("TrunkTop", 6, 0.24, 0, h, h + 0.25, "Cactus", bottom=False)
    for i in range(arms):
        s = 1 if i == 0 else -1
        z = h * (0.4 + 0.15 * i)
        bar(f"ArmOut{i}", (0, 0, z), (s * 0.9, 0, z + 0.3), 0.2, "Cactus", n=6)
        bar(f"ArmUp{i}", (s * 0.9, 0, z + 0.2), (s * 0.9, 0, z + h * 0.35), 0.2, "Cactus", n=6)

asset("SM_Tent", "Desert", "Desert_Props", 8, place=dict(density=6, scale=(0.9, 1.1), spacing=10, yaw=True, align=True, cluster=4))
extrude("Tent", [(-2.5, 0), (2.5, 0), (1.6, 2.2), (-1.6, 2.2)], -3, 3, "Canvas")
for y in (-3.3, 3.3):
    bar(f"Pole{y}", (0, y, 0), (0, y, 2.8), 0.05, "WoodDark")
box("Opening", (1.2, 0.05, 1.6), "Dark", loc=(0, -3.03, 0.8))
box("Crate", (0.9, 0.9, 0.7), "Wood", loc=(3.2, -1.0, 0.35))

asset("SM_Wreck_Airship", "Desert", "Desert_Props", 46, row=2, place=dict(density=0.5, scale=(0.9, 1.2), spacing=500, yaw=True, align=True, cluster=1),
      notes="Half-buried crashed zeppelin: ribs, torn canvas, tilted gondola")
for i in range(9):
    y = -18 + 4.5 * i
    r = 7.5 * math.sin(math.pi * (i + 0.6) / 9.6)
    n = 10
    for k in range(n // 2 + 1):
        if random.random() < 0.15:
            continue
        a0, a1 = math.pi * k / (n // 2 + 1), math.pi * (k + 1) / (n // 2 + 1)
        bar(f"Rib{i}_{k}", (r * math.cos(a0), y, r * math.sin(a0) - 1.5), (r * math.cos(a1), y, r * math.sin(a1) - 1.5), 0.18, "IronDark")
for s in (-1, 1):
    bar(f"Keel{s}", (s * 2.0, -18, 3.5), (s * 2.0, 18, 3.5), 0.22, "Iron")
bm = bmesh.new()
pts = [(7.2 * math.cos(a), y, 7.2 * math.sin(a) - 1.5) for y in (-4, 8) for a in (0.15, 0.6, 1.1, 1.6)]
vs = [bm.verts.new(p) for p in pts]
vi = [bm.verts.new((p[0] * 0.98, p[1], (p[2] + 1.5) * 0.98 - 1.5)) for p in pts]   # inner side, so it reads from both sides
for k in range(3):
    bm.faces.new((vs[k], vs[k + 1], vs[k + 5], vs[k + 4]))
    bm.faces.new((vi[k + 4], vi[k + 5], vi[k + 1], vi[k]))
put(bm, "Canvas", "CanvasTorn")
box("Gondola", (3.0, 9.0, 2.4), "WoodDark", loc=(4.5, -3, 0.6), rot=(0.12, 0.35, 0.2))
prism("Engine", 8, 0.9, 0.6, -1.2, 1.2, "Iron", loc=(-6.5, 10, 0.4), rot=(math.pi / 2, 0, 0.4))

asset("SM_Ribcage", "Desert", "Desert_Props", 16, place=dict(density=1, scale=(0.8, 1.3), spacing=200, yaw=True, align=True, cluster=1),
      notes="Bones of a giant beast")
bar("Spine", (0, -6, 2.5), (0, 6, 3.2), 0.35, "Bone", n=6)
for i in range(6):
    y = -4.5 + 1.8 * i
    for s in (-1, 1):
        k_ = 0.55 + 0.45 * (1 - abs(i - 2.5) / 2.5)        # longest ribs in the middle
        bar(f"Rib{i}{s}a", (0, y, 3.0), (s * 1.9 * k_ - s * 0.2, y + 0.2, 3.7), 0.17, "Bone")
        bar(f"Rib{i}{s}b", (s * 1.9 * k_ - s * 0.2, y + 0.2, 3.7), (s * 3.0 * k_, y + 0.4, 1.8), 0.16, "Bone")
        bar(f"Rib{i}{s}c", (s * 3.0 * k_, y + 0.4, 1.8), (s * 2.5 * k_, y + 0.6, -0.3), 0.14, "Bone")
ball("Skull", 1.3, (0, 7.6, 2.0), "Bone", u=7, v=5, scale=(0.9, 1.3, 0.8))
prism("Horn", 5, 0.3, 0, 0, 2.2, "Bone", loc=(0.8, 7.8, 2.6), rot=(-0.6, 0.6, 0))

asset("SM_GuardTower", "Desert", "Desert_GuardTower", 12, row=1, cat="Obstacle", place=dict(density=3, scale=(0.95, 1.1), spacing=120, yaw=True, align=True, cluster=1),
      notes="27 m stilted guard tower with a searchlight (SOCKET_searchlight); collides")
for sx in (-1, 1):
    for sy in (-1, 1):
        bar(f"Leg{sx}{sy}", (sx * 3.6, sy * 3.6, 0), (sx * 2.1, sy * 2.1, 20), 0.28, "WoodDark")
wt = lambda z: 3.6 - 1.5 * z / 20
for i, z in enumerate((5, 10, 15, 20)):
    w0, w1 = wt(z - 5), wt(z)
    for k in range(4):
        c0 = [(-1, -1), (1, -1), (1, 1), (-1, 1)][k]; c1 = [(-1, -1), (1, -1), (1, 1), (-1, 1)][(k + 1) % 4]
        bar(f"Brace{i}{k}", (c0[0] * w0, c0[1] * w0, z - 5), (c1[0] * w1, c1[1] * w1, z), 0.12, "Wood")
        bar(f"Rail{i}{k}", (c0[0] * w1, c0[1] * w1, z), (c1[0] * w1, c1[1] * w1, z), 0.12, "Wood")
box("Platform", (6.4, 6.4, 0.4), "WoodDark", loc=(0, 0, 20.2))
for s in (-1, 1):
    box(f"WallX{s}", (6.0, 0.2, 1.2), "Wood", loc=(0, s * 3.0, 21.0))
    box(f"WallY{s}", (0.2, 6.0, 1.2), "Wood", loc=(s * 3.0, 0, 21.0))
for sx in (-1, 1):
    for sy in (-1, 1):
        bar(f"Post{sx}{sy}", (sx * 2.9, sy * 2.9, 20.4), (sx * 2.9, sy * 2.9, 24.2), 0.12, "WoodDark")
hip("Roof", 6.6, 6.6, 1.8, 24.2, "IronDark", over=0.4)
for i in range(10):                                        # sandbag ring on the platform edge
    a = 2 * math.pi * i / 10
    ball(f"Sandbag{i}", 0.45, (3.3 * math.cos(a) * 0.98, 3.3 * math.sin(a) * 0.98, 21.8), "Sandbag", u=6, v=3, scale=(1.3, 0.8, 0.6))
bar("LadderL", (3.6, -0.4, 0), (2.1, -0.4, 20), 0.07, "WoodDark")
bar("LadderR", (3.6, 0.4, 0), (2.1, 0.4, 20), 0.07, "WoodDark")
prism("Searchlight", 8, 0.45, 0.55, 0, 0.9, "IronDark", loc=(1.6, -2.4, 21.9), rot=(math.pi / 2, 0, 0))
prism("SearchLens", 8, 0.5, 0.5, 0.9, 0.95, "LensGlow", loc=(1.6, -2.4, 21.9), rot=(math.pi / 2, 0, 0))
socket("searchlight", (1.6, -3.4, 21.9))
bar("FlagPole", (-2.9, 2.9, 24.2), (-2.9, 2.9, 28), 0.06, "IronDark", n=6)
box("Flag", (1.4, 0.05, 0.9), "PaintRed", loc=(-2.2, 2.9, 27.4))
ucx_pts([(sx * 3.8, sy * 3.8, 0) for sx in (-1, 1) for sy in (-1, 1)] + [(sx * 2.3, sy * 2.3, 20) for sx in (-1, 1) for sy in (-1, 1)])
ucx_box((7.0, 7.0, 6.0), (0, 0, 23.0))

# -- lighthouse: skeletal (head rotates, lamp can be destroyed)
asset("SK_Lighthouse", "Desert", "Desert_Lighthouse", 14, row=1, kind="skeletal", cat="Hazard",
      place=dict(density=1.5, scale=(1, 1), spacing=300, yaw=True, align=True, cluster=1),
      notes="Rotating head (bone head, spins around Z); lamp bone = light source, can be destroyed. Beam: SM_LighthouseBeam on socket_beam")
LZ = 32.6
bone("root", (0, 0, 0), (0, 0, 2))
bone("head", (0, 0, 30.6), (0, 0, 33), "root")
bone("lamp", (0, 0, LZ), (0, 0, LZ + 1.2), "head")
bone("socket_beam", (0, 0, LZ), (0, 1.0, LZ), "head", deform=False)
part_bone("root")
prism("Plinth", 8, 6.0, 5.6, 0, 1.6, "StoneDark")
for i in range(6):
    z0, z1 = 1.6 + i * 4.8, 1.6 + (i + 1) * 4.8
    r0, r1 = 4.3 - 1.5 * (z0 - 1.6) / 28.8, 4.3 - 1.5 * (z1 - 1.6) / 28.8
    prism(f"Band{i}", 12, r0, r1, z0, z1, "LighthouseRed" if i % 2 else "LighthouseWhite", top=False, bottom=False)
box("Door", (1.4, 0.4, 2.4), "WoodDark", loc=(0, -4.25, 2.8))
for i in range(4):
    a = -math.pi / 2 + i * 1.7
    z = 7 + 5.5 * i
    r = 4.3 - 1.5 * (z - 1.6) / 28.8
    box(f"Window{i}", (0.6, 0.3, 1.0), "Dark", loc=(r * math.cos(a), r * math.sin(a), z), rot=(0, 0, a + math.pi / 2))
prism("Gallery", 12, 3.9, 3.9, 30.4, 30.8, "StoneDark")
for i in range(12):
    a = 2 * math.pi * i / 12
    bar(f"Railpost{i}", (3.7 * math.cos(a), 3.7 * math.sin(a), 30.8), (3.7 * math.cos(a), 3.7 * math.sin(a), 31.9), 0.05, "IronDark")
ring_bars("Rail", 3.7, 31.9, "IronDark", n=12, th=0.06)
part_bone("head")
prism("LanternBase", 12, 2.3, 2.3, 30.8, 31.8, "IronDark")
for i in range(8):
    a = 2 * math.pi * i / 8
    bar(f"Mullion{i}", (2.1 * math.cos(a), 2.1 * math.sin(a), 31.8), (2.1 * math.cos(a), 2.1 * math.sin(a), 34.2), 0.07, "IronDark")
prism("Roof", 12, 2.6, 0.4, 34.2, 35.8, "Copper")
ball("Vent", 0.45, (0, 0, 36.1), "Copper", u=6, v=4)
bar("Rod", (0, 0, 36.4), (0, 0, 38.0), 0.05, "Brass", n=6)
extrude("Reflector", [(math.cos(a) * 1.5, math.sin(a) * 1.5 - 0.0) for a in [math.pi + i * math.pi / 6 for i in range(7)]] +
        [(math.cos(a) * 1.35, math.sin(a) * 1.35) for a in [2 * math.pi - i * math.pi / 6 for i in range(7)]],
        -1.1, 1.1, "Brass", rot=(-math.pi / 2, 0, 0), loc=(0, 0, LZ))
part_bone("lamp")
prism("Lens", 10, 0.95, 0.95, LZ - 0.7, LZ + 0.7, "LensGlow")
prism("LensCap", 10, 0.95, 0.3, LZ + 0.7, LZ + 1.0, "Brass")
prism("LensFoot", 10, 0.3, 0.95, LZ - 1.0, LZ - 0.7, "Brass")
part_bone(None)

asset("SM_LighthouseBeam", "Desert", "Desert_Beam", 24, row=1, cat="Hazard", place=dict(density=0),
      notes="Light cone, 90 m long, origin at the lamp, points +Y. Own translucent material; colour changes when it spots the player")
bm = bmesh.new()
n = 12
r0, r1, Lb = 0.8, 9.0, 90.0
a_ = [bm.verts.new((r0 * math.cos(2 * math.pi * i / n), 0, r0 * math.sin(2 * math.pi * i / n))) for i in range(n)]
b_ = [bm.verts.new((r1 * math.cos(2 * math.pi * i / n), Lb, r1 * math.sin(2 * math.pi * i / n))) for i in range(n)]
for i in range(n):
    j = (i + 1) % n
    bm.faces.new((a_[i], a_[j], b_[j], b_[i]))
put(bm, "Cone", "LightBeam")

# ---------------------------------------------------------------- store metadata for the export
for lv in LEVELS:
    SCENES[lv]["LOC_assets"] = json.dumps({k: v for k, v in ASSETS.items() if v["level"] == lv})
    SCENES[lv]["LOC_groups"] = json.dumps(GROUPS[lv])
result = {lv: sum(1 for a in ASSETS.values() if a["level"] == lv) for lv in LEVELS}
result["objects"] = sum(len(s.objects) for s in SCENES.values())
