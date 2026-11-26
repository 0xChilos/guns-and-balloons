"""Builds the steampunk air balloon game model in the Blender scene "AirBalloon".

Run inside Blender (Text Editor > Run Script). Rebuilds the scene from scratch.
Units: metres, Z up, the balloon flies toward +Y (propeller at -Y).

Interactive / animatable pieces (origin placed at their pivot):
  AB_FurnaceDoor   - furnace door, hinge on its local Z axis (closed = 0 deg)
  AB_WingL/AB_WingR - side air-brake stoppers, hinge on local Z (children follow)
  AB_PropPivot     - propeller, spins around its local Y axis
Fuel: 3 buckets (bones fuel_bucket = wood, fuel_bucket_coal, fuel_bucket_crystal), each with 6 slot bones
  fuel_<kind>_1..6 holding one piece; RIG["items"] lists the loose hand items exported as SM_Fuel_*.
Damage: 10 bullet holes on the stove, bones stove_damage_1..10 (show 1..N, N = missing HP / 10).
"""
import bpy, bmesh, math, random
from mathutils import Vector, Matrix

random.seed(11)
SC = "AirBalloon"

# ---------------------------------------------------------------- cleanup
if SC in bpy.data.scenes:
    s = bpy.data.scenes[SC]
    for o in list(s.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.scenes.remove(s)
for c in list(bpy.data.collections):
    if c.name.startswith("AB_"):
        bpy.data.collections.remove(c)
for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.lights, bpy.data.cameras, bpy.data.worlds):
    for d in list(coll):
        if d.name.startswith("AB_") and d.users == 0:
            coll.remove(d)

sc = bpy.data.scenes.new(SC)

# Every mesh object is tagged obj["part"] = <game part / bone name>; RIG holds bone pivots + item sockets.
# Each mesh also gets obj["piece"] (see PIECES at the end); Balloon_export.py reads all three to build one
# skinned game mesh per piece, all sharing one skeleton and one texture atlas.
import json
PART = "basket"
RIG = {"bones": {}, "sockets": {}, "display": {}}

def bone(name, head, parent="basket", tail=None):
    """tail (optional): bone points head->tail, i.e. the bone's Y axis = the part's rotation / slide axis."""
    RIG["bones"][name] = {"head": [float(c) for c in head], "parent": parent}
    if tail is not None:
        RIG["bones"][name]["tail"] = [float(c) for c in tail]

def socket(name, head, axis):
    head = Vector(head); tail = head + Vector(axis).normalized() * 0.2
    RIG["sockets"][name] = {"head": list(head), "tail": list(tail), "parent": "basket"}

bone("root", (0, 0, 0), None)
bone("basket", (0, 0, 0), "root")

# ---------------------------------------------------------------- materials
M = {}

def _sock(sockets, name, typ):
    for s in sockets:
        if s.name == name and s.type == typ:
            return s
    raise KeyError(name)

def mk(name, c1, c2, metal=0.0, rough=0.8, snap=0.05, nscale=5.0, ramp=(0.3, 0.7),
       pattern=None, pcol=(0.6, 0.6, 0.6), pfreq=16, patch=None, emit=None):
    m = bpy.data.materials.new("AB_" + name)
    m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; L = nt.links
    for n in list(N):
        N.remove(n)
    out = N.new('ShaderNodeOutputMaterial')
    if emit:
        e = N.new('ShaderNodeEmission')
        e.inputs['Color'].default_value = (*emit[0], 1)
        e.inputs['Strength'].default_value = emit[1]
        L.new(e.outputs[0], out.inputs['Surface'])
        m.diffuse_color = (*emit[0], 1)
        M[name] = m
        return m
    m.diffuse_color = (*c1, 1)
    b = N.new('ShaderNodeBsdfPrincipled')
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    L.new(b.outputs[0], out.inputs['Surface'])
    tc = N.new('ShaderNodeTexCoord')
    sn = N.new('ShaderNodeVectorMath'); sn.operation = 'SNAP'
    sn.inputs[1].default_value = (snap, snap, snap)
    L.new(tc.outputs['Object'], sn.inputs[0])
    nz = N.new('ShaderNodeTexNoise')
    nz.inputs['Scale'].default_value = nscale
    nz.inputs['Detail'].default_value = 4.0
    L.new(sn.outputs[0], nz.inputs['Vector'])
    cr = N.new('ShaderNodeValToRGB')
    cr.color_ramp.elements[0].position = ramp[0]; cr.color_ramp.elements[0].color = (*c2, 1)
    cr.color_ramp.elements[1].position = ramp[1]; cr.color_ramp.elements[1].color = (*c1, 1)
    L.new(nz.outputs['Fac'], cr.inputs['Fac'])
    col = cr.outputs['Color']

    def mix(fac, a, bcol, blend):
        mx = N.new('ShaderNodeMix'); mx.data_type = 'RGBA'; mx.blend_type = blend
        L.new(fac, _sock(mx.inputs, 'Factor', 'VALUE'))
        L.new(a, _sock(mx.inputs, 'A', 'RGBA'))
        _sock(mx.inputs, 'B', 'RGBA').default_value = (*bcol, 1)
        return _sock(mx.outputs, 'Result', 'RGBA')

    def stripes(src, freq):
        m1 = N.new('ShaderNodeMath'); m1.operation = 'MULTIPLY'; m1.inputs[1].default_value = freq
        L.new(src, m1.inputs[0])
        m2 = N.new('ShaderNodeMath'); m2.operation = 'FLOOR'
        L.new(m1.outputs[0], m2.inputs[0])
        m3 = N.new('ShaderNodeMath')
        try:
            m3.operation = 'FLOORED_MODULO'
        except Exception:
            m3.operation = 'MODULO'
        m3.inputs[1].default_value = 2.0
        L.new(m2.outputs[0], m3.inputs[0])
        return m3.outputs[0]

    if pattern == 'gores':
        g = N.new('ShaderNodeTexGradient'); g.gradient_type = 'RADIAL'
        L.new(tc.outputs['Object'], g.inputs['Vector'])
        col = mix(stripes(g.outputs['Fac'], pfreq), col, pcol, 'MULTIPLY')
    elif pattern == 'bands':
        s = N.new('ShaderNodeSeparateXYZ'); L.new(sn.outputs[0], s.inputs[0])
        col = mix(stripes(s.outputs['Z'], pfreq), col, pcol, 'MULTIPLY')
    if patch:
        sn2 = N.new('ShaderNodeVectorMath'); sn2.operation = 'SNAP'
        sn2.inputs[1].default_value = (0.25, 0.25, 0.25)
        L.new(tc.outputs['Object'], sn2.inputs[0])
        n2 = N.new('ShaderNodeTexNoise'); n2.inputs['Scale'].default_value = 1.3
        L.new(sn2.outputs[0], n2.inputs['Vector'])
        c2r = N.new('ShaderNodeValToRGB'); c2r.color_ramp.interpolation = 'CONSTANT'
        c2r.color_ramp.elements[0].color = (0, 0, 0, 1)
        c2r.color_ramp.elements[1].position = patch[1]
        c2r.color_ramp.elements[1].color = (1, 1, 1, 1)
        L.new(n2.outputs['Fac'], c2r.inputs['Fac'])
        col = mix(c2r.outputs['Color'], col, patch[0], 'MIX')
    L.new(col, b.inputs['Base Color'])
    M[name] = m
    return m

mk("Canvas", (0.50, 0.38, 0.22), (0.22, 0.15, 0.08), rough=0.95, snap=0.08, nscale=1.6,
   pattern='gores', pcol=(0.50, 0.42, 0.32), pfreq=16, patch=((0.16, 0.09, 0.05), 0.60))
mk("Canvas2", (0.40, 0.36, 0.24), (0.17, 0.15, 0.09), rough=0.95, snap=0.05, nscale=4,
   patch=((0.25, 0.12, 0.06), 0.6))
mk("Wicker", (0.30, 0.18, 0.08), (0.10, 0.06, 0.03), rough=0.9, snap=0.035, nscale=9,
   pattern='bands', pcol=(0.55, 0.5, 0.45), pfreq=14)
mk("Iron", (0.22, 0.10, 0.04), (0.04, 0.05, 0.045), metal=0.6, rough=0.7, snap=0.03, nscale=7, ramp=(0.45, 0.8))
mk("Brass", (0.55, 0.40, 0.14), (0.20, 0.15, 0.06), metal=0.9, rough=0.45, snap=0.03, nscale=6)
mk("Rope", (0.36, 0.29, 0.17), (0.17, 0.13, 0.08), rough=1.0, snap=0.03, nscale=12)
mk("PropWood", (0.42, 0.25, 0.10), (0.18, 0.10, 0.04), rough=0.7, snap=0.03, nscale=8,
   pattern='bands', pcol=(0.75, 0.7, 0.65), pfreq=20)
mk("Barnwood", (0.33, 0.29, 0.24), (0.13, 0.11, 0.09), rough=0.95, snap=0.03, nscale=10,
   pattern='bands', pcol=(0.70, 0.68, 0.66), pfreq=26)
mk("Burlap", (0.38, 0.32, 0.20), (0.18, 0.15, 0.09), rough=1.0, snap=0.04, nscale=9)
mk("Coal", (0.05, 0.05, 0.05), (0.01, 0.01, 0.01), rough=0.6, snap=0.02, nscale=15)
mk("Gauge", (0.70, 0.64, 0.48), (0.45, 0.40, 0.28), rough=0.5, snap=0.01, nscale=20)
mk("GaugeGreen", (0.12, 0.40, 0.08), (0.07, 0.26, 0.05), rough=0.5, snap=0.01, nscale=20)
mk("GaugeOrange", (0.80, 0.36, 0.05), (0.55, 0.22, 0.03), rough=0.5, snap=0.01, nscale=20)
mk("GaugeRed", (0.66, 0.06, 0.03), (0.42, 0.03, 0.02), rough=0.5, snap=0.01, nscale=20)
mk("Fire", None, None, emit=((1.0, 0.33, 0.04), 30.0))
mk("Lamp", None, None, emit=((1.0, 0.75, 0.35), 12.0))
mk("Display", None, None, emit=((0.12, 0.30, 0.16), 2.0))
mk("WarnLamp", (0.42, 0.36, 0.30), (0.30, 0.25, 0.21), rough=0.2, snap=0.01, nscale=20)   # glass, unlit = "off"
mk("Bark", (0.22, 0.13, 0.06), (0.08, 0.05, 0.025), rough=0.95, snap=0.015, nscale=16)
mk("WoodEnd", (0.72, 0.55, 0.32), (0.52, 0.37, 0.18), rough=0.8, snap=0.01, nscale=30)
mk("CoalLump", (0.17, 0.165, 0.16), (0.035, 0.035, 0.035), metal=0.3, rough=0.35, snap=0.012, nscale=22, ramp=(0.35, 0.6))
mk("Crystal", None, None, emit=((0.25, 0.85, 1.0), 8.0))            # glows in game (M_Glow)
mk("Ember", None, None, emit=((1.0, 0.10, 0.02), 20.0))            # bullet-hole cores, glow in game (M_Glow)
mk("BareMetal", (0.52, 0.48, 0.42), (0.30, 0.28, 0.25), metal=0.9, rough=0.35, snap=0.01, nscale=20)   # torn edges

# ---------------------------------------------------------------- helpers
def finish(name, bm, mat, loc=(0, 0, 0), rot=(0, 0, 0)):
    me = bpy.data.meshes.new("AB_" + name)
    bm.to_mesh(me); bm.free()
    for p in me.polygons:
        p.use_smooth = False
    if mat:
        me.materials.append(M[mat])
    o = bpy.data.objects.new("AB_" + name, me)
    o.location = loc; o.rotation_euler = rot
    o["part"] = PART
    sc.collection.objects.link(o)
    return o

def prism(name, n, r0, r1, z0, z1, mat, phase=None, top=True, bottom=True, **kw):
    if phase is None:
        phase = math.pi / n
    bm = bmesh.new()
    ang = [phase + 2 * math.pi * i / n for i in range(n)]
    lo = [bm.verts.new((r0 * math.cos(a), r0 * math.sin(a), z0)) for a in ang]
    hi = [bm.verts.new((r1 * math.cos(a), r1 * math.sin(a), z1)) for a in ang]
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    if top:
        bm.faces.new(hi)
    if bottom:
        bm.faces.new(list(reversed(lo)))
    return finish(name, bm, mat, **kw)

def profile_prism(name, pts, z0, z1, mat, **kw):
    bm = bmesh.new()
    lo = [bm.verts.new((x, y, z0)) for x, y in pts]
    hi = [bm.verts.new((x, y, z1)) for x, y in pts]
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.faces.new(hi); bm.faces.new(list(reversed(lo)))
    return finish(name, bm, mat, **kw)

def solid(o, t, off=-1.0):
    m = o.modifiers.new("Solid", "SOLIDIFY"); m.thickness = t; m.offset = off
    return o

def box(name, sx, sy, sz, mat, loc=(0, 0, 0), rot=(0, 0, 0), offset=(0, 0, 0)):
    """Box of size sx,sy,sz; `offset` shifts the mesh relative to the object origin (for pivots)."""
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * sx + offset[0], v.co.y * sy + offset[1], v.co.z * sz + offset[2]))
    return finish(name, bm, mat, loc=loc, rot=rot)

def bar(name, a, b, th, mat, n=4):
    a = Vector(a); b = Vector(b); d = b - a
    o = prism(name, n, th, th, 0, d.length, mat, loc=a)
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = d.to_track_quat('Z', 'Y')
    return o

def ball(name, r, loc, mat, u=8, v=6, scale=(1, 1, 1)):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=r)
    for vv in bm.verts:
        vv.co.x *= scale[0]; vv.co.y *= scale[1]; vv.co.z *= scale[2]
    return finish(name, bm, mat, loc=loc)

def polar(r, a, z):
    return (r * math.cos(a), r * math.sin(a), z)

def empty(name, loc):
    o = bpy.data.objects.new("AB_" + name, None)
    o.empty_display_type = 'ARROWS'; o.empty_display_size = 0.3
    o.location = loc
    sc.collection.objects.link(o)
    return o

def parent(child, par):
    child.parent = par
    child.matrix_parent_inverse = par.matrix_world.inverted()
    return child

def update():
    sc.view_layers[0].update()

# ---- instrument helpers: geometry built in a local frame (U = right, V = up, N = out of the surface)
def frame(origin, U, V, N):
    U, V, N = Vector(U), Vector(V), Vector(N)
    o = Vector(origin)
    return Matrix(((U.x, V.x, N.x, o.x), (U.y, V.y, N.y, o.y), (U.z, V.z, N.z, o.z), (0, 0, 0, 1)))

def fobj(name, F, build, mat):
    bm = bmesh.new(); build(bm)
    bmesh.ops.transform(bm, matrix=F, verts=bm.verts)
    return finish(name, bm, mat)

def b_box(c, s):
    return lambda bm: bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(c) @ Matrix.Diagonal((*s, 1)))

def b_cyl(c, r, n0, n1, seg=12):
    return lambda bm: bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=n1 - n0,
                                            matrix=Matrix.Translation((c[0], c[1], (n0 + n1) / 2)))

def b_disc(c, r, n, seg=24):
    def f(bm):
        vs = [bm.verts.new((c[0] + r * math.cos(2 * math.pi * i / seg), c[1] + r * math.sin(2 * math.pi * i / seg), n))
              for i in range(seg)]
        bm.faces.new(vs)                       # CCW in (U,V) -> faces +N
    return f

def b_ring(c, r0, r1, n0, n1, seg=16):
    def f(bm):
        q = [[bm.verts.new((c[0] + r * math.cos(2 * math.pi * i / seg), c[1] + r * math.sin(2 * math.pi * i / seg), z))
              for r, z in ((r0, n0), (r1, n0), (r1, n1), (r0, n1))] for i in range(seg)]
        for i in range(seg):
            j = (i + 1) % seg
            for k in range(4):
                bm.faces.new((q[i][k], q[j][k], q[j][(k + 1) % 4], q[i][(k + 1) % 4]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return f

def dial(r, phi):
    """Point on a dial: phi in degrees, 0 = straight up (V), positive = clockwise toward U."""
    p = math.radians(phi)
    return Vector((r * math.sin(p), r * math.cos(p)))

def b_arc(c, r0, r1, a0, a1, n, step=8.0):
    def f(bm):
        k = max(1, int(math.ceil(abs(a1 - a0) / step)))
        ang = [a0 + (a1 - a0) * i / k for i in range(k + 1)]
        inn = [bm.verts.new((*(Vector(c) + dial(r0, a)), n)) for a in ang]
        out = [bm.verts.new((*(Vector(c) + dial(r1, a)), n)) for a in ang]
        for i in range(k):
            fc = bm.faces.new((inn[i], inn[i + 1], out[i + 1], out[i]))
            fc.normal_update()
            if fc.normal.z < 0:
                fc.normal_flip()
    return f

def b_needle(c, phi, length, w, n0, n1, back=0.012):
    """Thin bar from the dial centre pointing at phi."""
    M_ = Matrix.Translation((c[0], c[1], (n0 + n1) / 2)) @ Matrix.Rotation(-math.radians(phi), 4, 'Z')
    return lambda bm: bmesh.ops.create_cube(bm, size=1.0, matrix=M_ @ Matrix.Translation((0, (length - back) / 2, 0))
                                            @ Matrix.Diagonal((w, length + back, n1 - n0, 1)))

def b_tick(c, phi, r0, r1, w, n0, n1):
    """Radial tick mark from r0 to r1 at angle phi."""
    M_ = (Matrix.Translation((c[0], c[1], (n0 + n1) / 2)) @ Matrix.Rotation(-math.radians(phi), 4, 'Z')
          @ Matrix.Translation((0, (r0 + r1) / 2, 0)) @ Matrix.Diagonal((w, r1 - r0, n1 - n0, 1)))
    return lambda bm: bmesh.ops.create_cube(bm, size=1.0, matrix=M_)

def b_prism2d(pts, n0, n1):
    def f(bm):
        lo = [bm.verts.new((x, y, n0)) for x, y in pts]
        hi = [bm.verts.new((x, y, n1)) for x, y in pts]
        for i in range(len(pts)):
            j = (i + 1) % len(pts)
            bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
        bm.faces.new(hi); bm.faces.new(list(reversed(lo)))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return f

SEG7 = {"0": "abcdef", "1": "bc"}
def digit(name, F, ch, cu, cv, h, w, s, n0, n1, mat):
    """Seven-segment style digit made of small bars (reads clearly at low resolution)."""
    nc, t = (n0 + n1) / 2, n1 - n0
    seg = {"a": ((cu, cv + h / 2 - s / 2), (w, s)), "d": ((cu, cv - h / 2 + s / 2), (w, s)),
           "g": ((cu, cv), (w, s)),
           "b": ((cu + w / 2 - s / 2, cv + h / 4), (s, h / 2)), "c": ((cu + w / 2 - s / 2, cv - h / 4), (s, h / 2)),
           "f": ((cu - w / 2 + s / 2, cv + h / 4), (s, h / 2)), "e": ((cu - w / 2 + s / 2, cv - h / 4), (s, h / 2))}
    for k in SEG7[ch]:
        (u_, v_), (su, sv) = seg[k]
        fobj(f"{name}_{k}", F, b_box((u_, v_, nc), (su, sv, t)), mat)

# ================================================================ BASKET (octagonal, 8 walls)
R = 0.975                                 # wall circumradius (25% smaller than the first 1.3): 1.66 m inside
APO = R * math.cos(math.pi / 8)          # 0.901 outer wall distance
WALL_T = 0.07
INNER = APO - WALL_T                      # 0.831 inner wall face
FLOOR_Z = WALL_T                          # floor top (solidified bottom face)
solid(prism("BasketWall", 8, R, R, 0.0, 1.0, "Wicker", phase=math.pi / 8, top=False), WALL_T)
solid(prism("BasketRim", 8, R + 0.07, R + 0.07, 0.96, 1.09, "Iron", phase=math.pi / 8, top=False, bottom=False), 0.15)
solid(prism("BasketBandLow", 8, R + 0.035, R + 0.035, 0.04, 0.15, "Iron", phase=math.pi / 8, top=False, bottom=False), 0.07)
solid(prism("BasketBandMid", 8, R + 0.02, R + 0.02, 0.50, 0.56, "Brass", phase=math.pi / 8, top=False, bottom=False), 0.05)
for k in range(8):
    a = math.pi / 8 + k * math.pi / 4
    box(f"Post{k}", 0.08, 0.08, 1.18, "Iron", loc=polar(R + 0.02, a, 0.56), rot=(0, 0, a))
    ball(f"PostCap{k}", 0.055, polar(R + 0.02, a, 1.17), "Brass", u=6, v=4)
box("Skid0", 0.08, 1.7 * R, 0.06, "Iron", loc=(-0.45 * R, 0, -0.03))
box("Skid1", 0.08, 1.7 * R, 0.06, "Iron", loc=(0.45 * R, 0, -0.03))

# ================================================================ ENVELOPE
PART = "balloon"
ER, EZS = 3.4, 1.2
ENV_Z = 2.92 + ER * EZS * 0.951          # keeps the mouth at ~2.9 m
EK = ER / 2.7
bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=1.0)
for v in bm.verts:
    x, y, z = v.co
    f = 1.0 + 0.32 * z if z < 0 else 1.0 - 0.06 * z * z
    v.co = Vector((x * f * ER, y * f * ER, z * EZS * ER))
    if abs(z) < 0.99:
        v.co += Vector((random.uniform(-.05, .05), random.uniform(-.05, .05), random.uniform(-.04, .04)))
bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.97 * EZS * ER], context='VERTS')
env = finish("Envelope", bm, "Canvas", loc=(0, 0, ENV_Z))
solid(env, 0.04)
net = bpy.data.objects.new("AB_Net", env.data.copy())
net.data.materials.clear(); net.data.materials.append(M["Rope"])
net.location = env.location; net.scale = (1.015, 1.015, 1.015)
net["part"] = "balloon"
sc.collection.objects.link(net)
w = net.modifiers.new("Wire", "WIREFRAME"); w.thickness = 0.02; w.use_replace = True
solid(prism("BeltEq", 16, 2.80 * EK, 2.80 * EK, ENV_Z - 0.10, ENV_Z + 0.10, "Brass", top=False, bottom=False), 0.06)
solid(prism("BeltLow", 16, 2.40 * EK, 2.40 * EK, ENV_Z - 1.08 * EK, ENV_Z - 0.94 * EK, "Iron", top=False, bottom=False), 0.06)
top_z = ENV_Z + ER * EZS
prism("Crown", 8, 0.42, 0.30, top_z - 0.08, top_z + 0.10, "Brass")
prism("CrownValve", 8, 0.10, 0.08, top_z + 0.10, top_z + 0.32, "Iron")
ball("CrownKnob", 0.09, (0, 0, top_z + 0.36), "Brass", u=6, v=4)
MOUTH_Z = ENV_Z - ER * EZS * 0.951
solid(prism("Skirt", 16, 0.78, 0.60, MOUTH_Z - 0.50, MOUTH_Z + 0.02, "Canvas2", top=False, bottom=False), 0.03)
solid(prism("MouthRing", 16, 0.64, 0.64, MOUTH_Z - 0.04, MOUTH_Z + 0.05, "Iron", top=False, bottom=False), 0.06)
bone("balloon", (0, 0, MOUTH_Z), "basket")
PART = "ropes"
LOAD_Z = 2.40
bone("ropes", (0, 0, 1.15), "basket")
solid(prism("LoadRing", 8, 0.76, 0.76, LOAD_Z - 0.04, LOAD_Z + 0.04, "Iron", phase=math.pi / 8, top=False, bottom=False), 0.07)
for k in range(8):
    a = math.pi / 8 + k * math.pi / 4
    bar(f"RopeUp{k}", polar(0.74, a, LOAD_Z), polar(0.62, a, MOUTH_Z), 0.018, "Rope")
    bar(f"RopeDn{k}", polar(R + 0.02, a, 1.15), polar(0.74, a, LOAD_Z), 0.018, "Rope")

# ================================================================ STOVE ENGINE (inside the basket, against the back wall)
SR = 0.30
SAPO = SR * math.cos(math.pi / 8)        # 0.277
SX, SY = 0.0, -(INNER - SAPO - 0.01)      # back of the stove nearly touches the inner back wall
Z0 = FLOOR_Z
PART = "stove"
bone("stove", (SX, SY, Z0), "basket")
prism("StovePlinth", 8, 0.35, 0.35, Z0, Z0 + 0.07, "Iron", loc=(SX, SY, 0))
prism("StoveBody", 8, SR, SR, Z0 + 0.07, 0.95, "Iron", loc=(SX, SY, 0))
for i, z in enumerate((0.50, 0.86)):
    prism(f"StoveBand{i}", 8, 0.318, 0.318, z, z + 0.05, "Brass", loc=(SX, SY, 0))
prism("StoveDome", 8, 0.32, 0.15, 0.95, 1.09, "Iron", loc=(SX, SY, 0))
prism("StoveCollar", 8, 0.11, 0.11, 1.09, 1.21, "Brass", loc=(SX, SY, 0))

# ---------------- thermometer on the sloped lid (dome), tilted up toward the player
# The dome's +Y face runs from apothem 0.2956 at z 0.95 to 0.1386 at z 1.09; the gauge sits at its middle.
_d0 = Vector((0, 0.32 * math.cos(math.pi / 8), 0.95)); _d1 = Vector((0, 0.15 * math.cos(math.pi / 8), 1.09))
GV = (_d1 - _d0).normalized()                         # up the slope
GN = Vector((0, GV.z, -GV.y))                         # out of the lid, toward the basket and up
GU = GV.cross(GN)                                     # right as seen by the player (-X)
FG = frame(Vector((SX, SY, 0)) + (_d0 + _d1) / 2, GU, GV, GN)
PART = "gauge"
bone("gauge", FG @ Vector((0, 0, 0)), "stove", tail=FG @ Vector((0, 0, 0.15)))
fobj("GaugeBody", FG, b_cyl((0, 0), 0.07, 0.0, 0.03), "Brass")
fobj("GaugeFace", FG, b_cyl((0, 0), 0.058, 0.03, 0.034), "Gauge")
fobj("GaugeBezel", FG, b_ring((0, 0), 0.056, 0.07, 0.03, 0.04), "Brass")
# scale: 240 deg sweep, 0 at lower-left (-120), full at lower-right (+120); 60% green, 30% orange, 10% red
G0, GSW = -120.0, 240.0
for nm, a, b_, mt in (("Green", 0.0, 0.6, "GaugeGreen"), ("Orange", 0.6, 0.9, "GaugeOrange"), ("Red", 0.9, 1.0, "GaugeRed")):
    fobj(f"GaugeZone{nm}", FG, b_arc((0, 0), 0.036, 0.052, G0 + GSW * a, G0 + GSW * b_, 0.0345), mt)
for i in range(11):                                   # tick marks every 10%
    ph = G0 + GSW * i / 10
    fobj(f"GaugeTick{i}", FG, b_tick((0, 0), ph, 0.030 if i % 5 == 0 else 0.038, 0.054, 0.003, 0.0345, 0.0365), "Coal")
PART = "gauge_needle"            # rotates around the bone axis (gauge normal); rest pose = 0 (start of the scale)
bone("gauge_needle", FG @ Vector((0, 0, 0.038)), "gauge", tail=FG @ Vector((0, 0, 0.188)))
fobj("GaugeNeedle", FG, b_needle((0, 0), G0, 0.05, 0.006, 0.037, 0.041), "Coal")
fobj("GaugeNeedlePin", FG, b_cyl((0, 0), 0.008, 0.034, 0.045, seg=6), "Brass")
PART = "stove"

# ---------------- furnace: firebox at the bottom of the stove, opening into the basket
# Nearly flush with the stove's front face so it doesn't eat basket space.
SFACE = SY + SAPO                      # stove front face (y)
FACE_W = 2 * SR * math.sin(math.pi / 8)  # width of one octagon face (~0.23)
FW, FH = FACE_W + 0.02, 0.26           # furnace frame size
FZ = 0.705                             # furnace centre: between the two brass bands, where the thermometer was
fy0 = SFACE - 0.06                     # frame back (sunk into the stove body)
fy1 = SFACE + 0.012                    # frame front: only ~1 cm proud of the stove
box("FurnaceHousing", FW, fy1 - fy0, FH, "Iron", loc=(SX, (fy0 + fy1) / 2, FZ))
box("FurnaceLip", FW + 0.02, 0.016, 0.03, "Brass", loc=(SX, fy1 + 0.004, FZ + FH / 2 - 0.005))
box("FurnaceSill", FW + 0.02, 0.022, 0.03, "Brass", loc=(SX, fy1 + 0.007, FZ - FH / 2 + 0.005))
# glowing coal bed inside the firebox mouth (recessed)
OW, OH = 0.16, 0.15
box("FurnaceGlow", OW, 0.02, OH, "Fire", loc=(SX, fy1 - 0.008, FZ))
for i in range(4):
    box(f"FurnaceCoal{i}", 0.04, 0.025, 0.035, "Coal",
        loc=(SX - 0.06 + i * 0.04, fy1 - 0.004, FZ - OH / 2 + 0.025 + random.uniform(0, .015)),
        rot=(random.random(), random.random(), random.random()))
# hinged door (closed), sitting right on the frame; origin on the hinge at its left edge
DW, DH, DT = 0.19, 0.18, 0.016
HX = SX - DW / 2
DY = fy1 + DT / 2 + 0.002
PART = "furnace_door"            # swings open around world Z (positive = open)
bone("furnace_door", (HX, DY, FZ), "stove")
door = box("FurnaceDoor", DW, DT, DH, "Iron", loc=(HX, DY, FZ), offset=(DW / 2, 0, 0))
PART = "stove"
for i, dz in enumerate((-0.055, 0.055)):
    bar(f"FurnaceHinge{i}", (HX - 0.008, DY, FZ + dz - 0.022),
        (HX - 0.008, DY, FZ + dz + 0.022), 0.012, "Brass", n=6)
door_parts = []
PART = "furnace_door"
FRONT = DY + DT / 2
for i, dz in enumerate((-0.03, 0.0, 0.03)):
    door_parts.append(box(f"FurnaceDoorVent{i}", 0.10, 0.004, 0.01, "Fire",
                          loc=(SX, FRONT + 0.001, FZ + 0.035 + dz)))
door_parts.append(box("FurnaceDoorLatch", 0.045, 0.014, 0.02, "Brass", loc=(SX + DW / 2 - 0.035, FRONT + 0.006, FZ - 0.03)))
door_parts.append(ball("FurnaceDoorKnob", 0.016, (SX + DW / 2 - 0.03, FRONT + 0.02, FZ - 0.03), "Brass", u=6, v=4))
update()
for p in door_parts:
    parent(p, door)

# ---------------- remaining burn time scale: where the furnace used to be (bottom of the stove front)
# 6 evenly spaced lines, "0" at the bottom line, "10" at the top; the arrow slides up/down (bone burn_arrow).
PART = "stove"
FB = frame((SX, SFACE, 0), (-1, 0, 0), (0, 0, 1), (0, 1, 0))     # U = right as seen from inside the basket
fobj("BurnFrame", FB, b_box((0, 0.30, 0.006), (0.22, 0.32, 0.012)), "Iron")
fobj("BurnPlate", FB, b_box((0, 0.30, 0.0135), (0.19, 0.29, 0.003)), "Gauge")
BURN_V0, BURN_DV = 0.19, 0.044                                     # bottom line height, line spacing
for k in range(6):
    fobj(f"BurnLine{k}", FB, b_box((0.005, BURN_V0 + k * BURN_DV, 0.016), (0.07, 0.007, 0.004)), "Coal")
DG = dict(h=0.034, w=0.018, s=0.005, n0=0.0145, n1=0.0185, mat="Coal")
digit("BurnNum0", FB, "0", -0.060, BURN_V0, **DG)
digit("BurnNum10a", FB, "1", -0.090, BURN_V0 + 5 * BURN_DV, **DG)
digit("BurnNum10b", FB, "0", -0.060, BURN_V0 + 5 * BURN_DV, **DG)
fobj("BurnRail", FB, b_box((0.074, 0.30, 0.017), (0.008, 0.27, 0.006)), "Iron")
PART = "burn_arrow"              # slides along its bone (world +Z); rest pose = 0, top line = +0.22 m
bone("burn_arrow", FB @ Vector((0.045, BURN_V0, 0.02)), "stove")
fobj("BurnArrow", FB, b_prism2d([(0.045, BURN_V0), (0.068, BURN_V0 - 0.014), (0.068, BURN_V0 + 0.014)], 0.016, 0.024), "GaugeRed")
fobj("BurnSlider", FB, b_box((0.074, BURN_V0, 0.022), (0.016, 0.028, 0.012)), "Brass")
PART = "stove"

# ---------------- damage: 10 bullet holes on the stove (engine body = balloon health, 100 HP)
# Each hole is its own part / bone stove_damage_1 .. _10 (child of "stove"): Unreal shows one more hole per 10 HP
# missing (in a random order) and hides them again as the engine is repaired. A hole = black soot blot, torn bright metal
# petals curled outward and a glowing red core (we see into the firebox; the core glows through M_Glow).
def bullet_hole(name, F, S=1.4):
    """S scales the hole; at 1.4 it reads as a bullet hole from the player's standing height."""
    k = 9
    soot = [(r_ * math.cos(2 * math.pi * i / k + 0.3), r_ * math.sin(2 * math.pi * i / k + 0.3))
            for i, r_ in enumerate(S * random.uniform(0.028, 0.04) for _ in range(k))]
    fobj(name + "Soot", F, b_prism2d(soot, 0.0, 0.002), "Coal")
    fobj(name + "Core", F, b_disc((0, 0), 0.012 * S, 0.0032, seg=7), "Ember")
    def petals(bm):
        for i in range(7):
            a = 2 * math.pi * i / 7 + random.uniform(-0.2, 0.2)
            ln = S * random.uniform(0.024, 0.032)
            v0 = bm.verts.new((0.012 * S * math.cos(a - 0.3), 0.012 * S * math.sin(a - 0.3), 0.0034))
            v1 = bm.verts.new((0.012 * S * math.cos(a + 0.3), 0.012 * S * math.sin(a + 0.3), 0.0034))
            v2 = bm.verts.new((ln * math.cos(a), ln * math.sin(a), S * random.uniform(0.006, 0.01)))
            f = bm.faces.new((v0, v1, v2))
            f.normal_update()
            if f.normal.z < 0:
                f.normal_flip()
    fobj(name + "Petals", F, petals, "BareMetal")

def stove_side(deg, u, z):
    n = Vector((math.cos(math.radians(deg)), math.sin(math.radians(deg)), 0))
    U = Vector((0, 0, 1)).cross(n)                 # right as seen from outside the face
    o = Vector((SX, SY, 0)) + n * SAPO + U * u + Vector((0, 0, z))
    return frame(o, U, (0, 0, 1), n)

def stove_lid(deg):
    """Frame on the middle of the sloped lid (dome) face whose outward direction is deg."""
    rot = Matrix.Rotation(math.radians(deg - 90), 3, 'Z')
    d0 = rot @ Vector((0, 0.32 * math.cos(math.pi / 8), 0.95)); d1 = rot @ Vector((0, 0.15 * math.cos(math.pi / 8), 1.09))
    V = (d1 - d0).normalized()
    N = (rot @ Vector((0, 1, 0))) * V.z - Vector((0, 0, 1)) * (rot @ Vector((0, 1, 0))).dot(V)
    N.normalize()
    return frame(Vector((SX, SY, 0)) + (d0 + d1) / 2, V.cross(N), V, N)

# all on the faces the player sees: the two front-diagonal faces (the front itself is furnace + burn scale) and the
# lid beside the thermometer. Unreal picks the order they appear in at random.
DAMAGE = [stove_side(45, 0.02, 0.68), stove_side(135, -0.03, 0.70), stove_side(45, -0.05, 0.36), stove_side(135, 0.04, 0.40),
          stove_lid(45), stove_side(45, 0.04, 0.25), stove_side(135, -0.05, 0.29), stove_side(45, -0.04, 0.78),
          stove_side(135, 0.05, 0.79), stove_lid(135)]
for i, F in enumerate(DAMAGE):
    PART = f"stove_damage_{i + 1}"
    bone(PART, F @ Vector((0, 0, 0)), "stove", tail=F @ Vector((0, 0, 0.05)))   # bone axis = out of the surface
    bullet_hole(f"Damage{i + 1}", F)
PART = "stove"

# ---------------- fuel: 3 stores (wood crate, coal sack, crystal bucket), up to 6 pieces each
# Each store looks like its fuel and is open / low toward the basket centre, so the player sees every piece from
# where they stand. Sized so six pieces fill it edge to edge (it looks packed when full). Each piece has its own
# fixed slot and bone (fuel_<kind>_1 .. _6, filled in that order): Unreal shows pieces 1..N and hides the rest,
# so the count reads at a glance.
FUEL_MAX = 6
BK_RB, BK_RT, BK_HB, BK_HF, BK_FLOOR = 0.145, 0.16, 0.29, 0.17, 0.14   # radius bottom/top, back/front height, false floor
FUEL_ITEMS = {}                                                       # kind -> builder(name, F), item axis = local Z

def many(*fs):
    return lambda bm: [f(bm) for f in fs]

def b_cone6(r0, r1, z0, z1, M_=Matrix(), seg=6):
    return lambda bm: bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r0, radius2=r1, depth=z1 - z0,
                                            matrix=M_ @ Matrix.Translation((0, 0, (z0 + z1) / 2)))

def log_item(name, F, rr=0.034, ln=0.15):
    """Split log: 7-sided bark, light end grain with a dark heart at both ends."""
    fobj(name + "Bark", F, b_cyl((0, 0), rr, -ln / 2, ln / 2, seg=7), "Bark")
    for k, Fk in enumerate((F, F @ Matrix.Rotation(math.pi, 4, 'X'))):
        fobj(f"{name}End{k}", Fk, b_disc((0, 0), rr * 0.88, ln / 2 + 0.0015, seg=7), "WoodEnd")
        fobj(f"{name}Heart{k}", Fk, b_disc((0, 0), rr * 0.25, ln / 2 + 0.003, seg=5), "Bark")

def coal_item(name, F, rr=0.04):
    """Glossy faceted lump; origin = centre, rests 0.75 * rr above the floor."""
    def build(bm):
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=rr)
        for v in bm.verts:
            v.co *= 1.0 + random.uniform(-0.22, 0.22)
            v.co.z *= 0.75
    fobj(name, F, build, "CoalLump")

def crystal_item(name, F):
    """Glowing hexagonal shard with a small side shard; origin = base, grows along local Z."""
    side = Matrix.Translation((0.016, 0, 0.004)) @ Matrix.Rotation(math.radians(28), 4, 'Y')
    fobj(name, F, many(b_cone6(0.022, 0.022, 0.0, 0.095), b_cone6(0.022, 0.0, 0.095, 0.14),
                       b_cone6(0.012, 0.012, 0.0, 0.055, side), b_cone6(0.012, 0.0, 0.055, 0.08, side)), "Crystal")

FUEL_ITEMS.update(wood=log_item, coal=coal_item, crystal=crystal_item)

CR_FLOOR, SK_FLOOR = 0.035, 0.115                          # inner floor height: wood crate, coal sack

def crystal_bucket(tag, c, fwd):
    """Iron scuttle: high back, low front facing the player, brass rim, false floor at BK_FLOOR."""
    ac = math.atan2(fwd.y, fwd.x)
    phase = ac - math.pi / 8                               # a flat face points at the player
    rad = lambda z: BK_RB + (BK_RT - BK_RB) * z / BK_HB
    hgt = lambda ang: BK_HF + (BK_HB - BK_HF) * (1 - math.cos(ang - ac)) / 2
    ang = [phase + 2 * math.pi * i / 8 for i in range(8)]
    bm = bmesh.new()
    lo = [bm.verts.new((BK_RB * math.cos(t), BK_RB * math.sin(t), Z0)) for t in ang]
    hi = [bm.verts.new((rad(hgt(t)) * math.cos(t), rad(hgt(t)) * math.sin(t), Z0 + hgt(t))) for t in ang]
    for i in range(8):
        j = (i + 1) % 8
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.faces.new(list(reversed(lo)))
    solid(finish(f"Bucket{tag}", bm, "Iron", loc=c), 0.012)
    bm = bmesh.new()
    r0 = [bm.verts.new(((rad(hgt(t)) + 0.005) * math.cos(t), (rad(hgt(t)) + 0.005) * math.sin(t), Z0 + hgt(t) - 0.028)) for t in ang]
    r1 = [bm.verts.new(((rad(hgt(t)) + 0.005) * math.cos(t), (rad(hgt(t)) + 0.005) * math.sin(t), Z0 + hgt(t) + 0.004)) for t in ang]
    for i in range(8):
        j = (i + 1) % 8
        bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    solid(finish(f"Bucket{tag}Rim", bm, "Brass", loc=c), 0.012)
    prism(f"Bucket{tag}Foot", 8, BK_RB + 0.006, BK_RB + 0.006, Z0, Z0 + 0.025, "Iron", phase=phase, loc=c)
    rf = rad(BK_FLOOR) - 0.016                             # false floor: lifts the shards over the low front
    prism(f"Bucket{tag}Floor", 8, rf, rf, Z0 + BK_FLOOR - 0.012, Z0 + BK_FLOOR, "Barnwood", phase=phase, loc=c)
    return BK_FLOOR

def wood_crate(tag, c, fwd, right):
    """Plank crate, open top, a single low plank at the front so the log ends show."""
    W, D, T = 0.23, 0.17, 0.016                            # inner width (across), inner depth (along the logs), plank
    Fc = frame(c + Vector((0, 0, Z0)), right, fwd, (0, 0, 1))     # local x = right, y = toward the player, z = up
    def plank(nm, x0, x1, y0, y1, z0, z1, mat="Barnwood"):
        fobj(f"{tag}{nm}", Fc, b_box(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0)), mat)
    xo, yo = W / 2 + T, D / 2 + T
    for k, x in enumerate((-0.07, 0.07)):                  # two battens under the floor
        plank(f"Batten{k}", x - 0.02, x + 0.02, -yo, yo, 0.0, 0.012, "PropWood")
    for k in range(3):                                     # floor boards along the logs
        plank(f"Floor{k}", -W / 2 + k * W / 3 + 0.002, -W / 2 + (k + 1) * W / 3 - 0.002, -D / 2, D / 2, 0.012, CR_FLOOR, "PropWood")
    rows = ((0.012, 0.081), (0.089, 0.158), (0.166, 0.235))  # three planks per wall with gaps between them
    for k, (z0, z1) in enumerate(rows):
        plank(f"SideL{k}", -xo, -W / 2, -yo, yo, z0, z1)
        plank(f"SideR{k}", W / 2, xo, -yo, yo, z0, z1)
        plank(f"Back{k}", -W / 2, W / 2, -yo, -D / 2, z0, z1)
    plank("Front", -W / 2, W / 2, D / 2, yo, 0.012, 0.062)        # low front lip
    for sx in (-1, 1):                                     # corner posts outside the walls, iron strap round each side
        for sy in (-1, 1):
            px, py = sx * (xo + 0.011), sy * (yo - 0.011)
            plank(f"Post{sx}{sy}", px - 0.011, px + 0.011, py - 0.011, py + 0.011, 0.0, 0.235 if sy < 0 else 0.062, "PropWood")
        x0, x1 = (xo + 0.022, xo + 0.026) if sx > 0 else (-xo - 0.026, -xo - 0.022)
        plank(f"Strap{sx}", x0, x1, -yo, yo, 0.11, 0.13, "Iron")
    return CR_FLOOR

def coal_sack(tag, c, fwd):
    """Open burlap sack, sides slumped, mouth rolled down; coal heaped on the stuffing."""
    n = 10
    prof = ((0.0, 0.115), (0.025, 0.148), (0.08, 0.158), (0.13, 0.152), (0.165, 0.145))
    bm = bmesh.new()
    rings = []
    for k, (z, r) in enumerate(prof):
        ring = []
        for i in range(n):
            t = 2 * math.pi * (i + 0.5 * (k % 2)) / n
            rr = r + (random.uniform(-0.008, 0.008) if 0 < k < len(prof) - 1 else 0)
            ring.append(bm.verts.new((rr * math.cos(t), rr * math.sin(t), Z0 + z + (random.uniform(-0.006, 0.006) if k else 0))))
        rings.append(ring)
    for a_, b_ in zip(rings[:-1], rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a_[i], a_[j], b_[j], b_[i]))
    bm.faces.new(list(reversed(rings[0])))
    solid(finish(tag, bm, "Burlap", loc=c), 0.008)
    # rolled-down mouth: a fat lumpy burlap roll around the top
    bm = bmesh.new()
    cross = 5
    grid = []
    for i in range(n):
        t = 2 * math.pi * i / n
        R_ = 0.148 + random.uniform(-0.004, 0.004)
        grid.append([bm.verts.new(((R_ + 0.018 * math.cos(2 * math.pi * k / cross)) * math.cos(t),
                                   (R_ + 0.018 * math.cos(2 * math.pi * k / cross)) * math.sin(t),
                                   Z0 + 0.158 + 0.016 * math.sin(2 * math.pi * k / cross))) for k in range(cross)])
    for i in range(n):
        j = (i + 1) % n
        for k in range(cross):
            l = (k + 1) % cross
            bm.faces.new((grid[i][k], grid[j][k], grid[j][l], grid[i][l]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    finish(f"{tag}Roll", bm, "Burlap", loc=c)
    prism(f"{tag}Stuffing", n, 0.14, 0.14, Z0 + SK_FLOOR - 0.03, Z0 + SK_FLOOR, "Burlap", loc=c)   # what the coal lies on
    return SK_FLOOR

def fuel_bucket(kind, bone_name, cx, cy):
    """Fuel store standing at (cx, cy) on the floor, its open / low side facing the basket centre.
    wood = plank crate, coal = open sack, crystal = iron scuttle; then the 6 slot bones and pieces."""
    global PART
    c = Vector((cx, cy, 0))
    fwd = -c.normalized()                                  # toward the basket centre (the player)
    right = Vector((0, 0, 1)).cross(fwd)                   # right as seen by the player facing the store
    PART = bone_name
    bone(bone_name, (c.x, c.y, Z0), "basket")
    tag = kind.capitalize()
    if kind == "wood":
        floor = wood_crate("WoodCrate", c, fwd, right)
    elif kind == "coal":
        floor = coal_sack("CoalSack", c, fwd)
    else:
        floor = crystal_bucket(tag, c, fwd)
    # slots: frame on the inner floor, x = right, y = toward the player, z = up
    base = c + Vector((0, 0, Z0 + floor))
    L = lambda x, y, z: base + right * x + fwd * y + Vector((0, 0, z))
    slots = []
    if kind == "wood":                     # end-on pyramid 3-2-1: the player looks at six end-grain circles
        rr, s_ = 0.034, 0.071                # logs touching
        dv = math.sqrt(s_ ** 2 - (s_ / 2) ** 2)
        for x, z in ((-s_, rr), (0, rr), (s_, rr), (-s_ / 2, rr + dv), (s_ / 2, rr + dv), (0, rr + 2 * dv)):
            slots.append(frame(L(x, 0, z), right, Vector((0, 0, 1)), fwd) @ Matrix.Rotation(random.uniform(0, 6.3), 4, 'Z'))
    else:                                  # ring of six, slot 1 at the front-left, then clockwise
        rr = 0.08 if kind == "coal" else 0.075    # coal lumps touch their neighbours
        for i in range(FUEL_MAX):
            t = math.radians(-120 - 60 * i)
            x, y = rr * math.cos(t), rr * math.sin(t)
            if kind == "coal":                # dark mark under each lump: an empty slot reads as a gap
                fobj(f"CoalSackMark{i + 1}", Matrix.Translation(L(x, y, 0)), b_disc((0, 0), 0.034, 0.0015, seg=8), "Coal")
                slots.append(Matrix.Translation(L(x, y, 0.034)) @ Matrix.Rotation(random.uniform(0, 6.3), 4, 'Z'))
            else:                             # iron collar per shard: an empty collar reads as a gap
                Fc = Matrix.Translation(L(x, y, 0))
                fobj(f"Bucket{tag}Collar{i + 1}", Fc, b_ring((0, 0), 0.024, 0.032, 0.0, 0.024, seg=8), "Iron")
                out_ = (right * x + fwd * y).normalized()
                tilt = Matrix.Rotation(math.radians(9), 4, Vector((-out_.y, out_.x, 0)))     # lean outward
                slots.append(Fc @ Matrix.Translation((0, 0, 0.004)) @ tilt @ Matrix.Rotation(random.uniform(0, 6.3), 4, 'Z'))
    for i, F in enumerate(slots):
        PART = f"fuel_{kind}_{i + 1}"
        head = F @ Vector((0, 0, 0)); tail = F @ Vector((0, 0, 0.12))
        bone(PART, head, bone_name, tail=tail)
        FUEL_ITEMS[kind](f"Fuel{tag}{i + 1}", F)
    PART = "basket"

# wood and coal right beside the stove (1 cm from its plinth); crystal in front of the wood, 1 cm from the side wall.
# Seen by the player facing the stove: wood and crystal on the right (-X), coal on the left (+X).
fuel_bucket("wood", "fuel_bucket", -0.505, -0.39)        # keeps the old bone name: BP_Balloon / WoodFuelSupply use it
fuel_bucket("coal", "fuel_bucket_coal", 0.505, -0.39)
fuel_bucket("crystal", "fuel_bucket_crystal", -0.652, -0.076)

# loose items for the player's hand (exported as SM_Fuel_*), laid out far from the basket so they don't touch its bakes
RIG["items"] = {}
for k, (kind, nm) in enumerate((("wood", "SM_Fuel_Wood"), ("coal", "SM_Fuel_Coal"), ("crystal", "SM_Fuel_Crystal"))):
    PART = "item_" + kind
    o_ = Vector((8.0 + 0.5 * k, 0, 0))
    F = Matrix.Translation(o_) @ (Matrix.Rotation(math.pi / 2, 4, 'Y') if kind == "wood" else Matrix())   # log along X
    FUEL_ITEMS[kind]("Item" + kind.capitalize(), F)
    RIG["items"][nm] = {"part": PART, "origin": list(o_)}
PART = "basket"

# chimney pipe: short rise from the collar, horizontal run to the back-right wall, up along that wall (~4 cm off it,
# away from where the player stands), a short bend inward through the rope load ring, then up the narrowing skirt
# to end just inside the balloon's mouth
PART = "stove"
PR = 0.05                                          # pipe radius
pa = math.radians(-45)                             # middle of the back-right wall
ZH, ZB, ZT = 1.30, 2.05, MOUTH_Z + 0.06            # horizontal run, start of the inward bend, pipe top (in the mouth)
WALL = Vector(polar(INNER - 0.015 - 0.04 - PR, pa, 0))      # 4 cm from the wall's inner face
TOP = Vector(polar(0.55, pa, 0))                   # inside the load ring (inner face ~0.63)
MOUTH = Vector(polar(0.35, pa, 0))                 # inside the mouth ring (inner face ~0.55)
P = [Vector((SX, SY, 1.21)), Vector((SX, SY, ZH)), WALL + Vector((0, 0, ZH)), WALL + Vector((0, 0, ZB)),
     TOP + Vector((0, 0, LOAD_Z - 0.07)), TOP + Vector((0, 0, LOAD_Z + 0.07)),
     MOUTH + Vector((0, 0, MOUTH_Z - 0.10)), MOUTH + Vector((0, 0, ZT))]
for i in range(len(P) - 1):
    bar(f"Pipe{i}", P[i], P[i + 1], PR, "Iron", n=8)
    if i in (1, 2):
        mid = (P[i] + P[i + 1]) / 2
        dd = (P[i + 1] - P[i]).normalized()
        bar(f"PipeCollar{i}", mid - dd * 0.03, mid + dd * 0.03, PR + 0.015, "Brass", n=8)
for i in range(1, len(P) - 1):
    ball(f"Elbow{i}", PR + 0.015, P[i], "Iron", u=8, v=6)
prism("Nozzle", 8, PR, PR + 0.035, ZT, ZT + 0.06, "Brass", top=False, loc=(MOUTH.x, MOUTH.y, 0))
bar("PipeBrace", TOP + Vector((0, 0, LOAD_Z)), Vector(polar(0.66, pa, LOAD_Z)), 0.014, "Iron", n=6)   # to the load ring
prism("PipeBraceClamp", 8, PR + 0.012, PR + 0.012, LOAD_Z - 0.025, LOAD_Z + 0.025, "Brass", top=False, bottom=False,
      loc=(TOP.x, TOP.y, 0))
prism("PipeWallClamp", 8, PR + 0.012, PR + 0.012, 1.62, 1.67, "Brass", top=False, bottom=False, loc=(WALL.x, WALL.y, 0))
bar("PipeWallStay", WALL + Vector((0, 0, 1.645)), Vector(polar(0.89, pa, 1.09)), 0.012, "Iron", n=6)   # down to the rim top
box("PipeWallStayFoot", 0.05, 0.05, 0.015, "Iron", loc=polar(0.89, pa, 1.095), rot=(0, 0, pa))

# ================================================================ PROPELLER (outside, behind the back wall)
PZ = 0.34
PART = "basket"
bar("DriveLink", (0, SY - SAPO + 0.02, PZ), (0, -(APO + 0.08), PZ), 0.05, "Brass", n=8)   # passes through the back wall
box("Gearbox", 0.30, 0.24, 0.26, "Iron", loc=(0, -(APO + 0.20), PZ))
teeth = 10; gpts = []
for i in range(teeth):
    a0 = 2 * math.pi * i / teeth; s = 2 * math.pi / teeth
    for fa, rr in ((0.0, 0.14), (0.15, 0.185), (0.45, 0.185), (0.6, 0.14)):
        gpts.append((rr * math.cos(a0 + fa * s), rr * math.sin(a0 + fa * s)))
profile_prism("BigGear", gpts, 0, 0.04, "Brass", loc=(-0.15, -(APO + 0.20), PZ + 0.02), rot=(0, -math.pi / 2, 0))
bar("Shaft", (0, -(APO + 0.32), PZ), (0, -(APO + 1.38), PZ), 0.04, "Brass", n=8)
bar("Bearing", (0, -(APO + 1.02), PZ), (0, -(APO + 1.16), PZ), 0.09, "Iron", n=8)
HUB_Y = -(APO + 1.40)
prop = empty("PropPivot", (0, HUB_Y, PZ))
PART = "propeller"               # spins around world Y
bone("propeller", (0, HUB_Y, PZ), "basket")
prop_parts = [bar("Hub", (0, HUB_Y + 0.04, PZ), (0, HUB_Y - 0.12, PZ), 0.10, "Brass", n=8),
              prism("Spinner", 8, 0.10, 0.015, 0, 0.16, "Brass", loc=(0, HUB_Y - 0.12, PZ), rot=(math.pi / 2, 0, 0))]
for k in range(4):
    bm = bmesh.new(); rings = []
    for r_, wdt, pit in ((0.08, 0.10, 35), (0.45, 0.20, 24), (0.88, 0.13, 12)):
        p = math.radians(pit); ring = []
        for sx_, sy_ in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            x = sx_ * wdt / 2; y = sy_ * 0.018
            ring.append(bm.verts.new((x * math.cos(p) - y * math.sin(p), x * math.sin(p) + y * math.cos(p), r_)))
        rings.append(ring)
    for a_, b_ in zip(rings[:-1], rings[1:]):
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((a_[i], a_[j], b_[j], b_[i]))
    bm.faces.new(rings[-1]); bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.transform(bm, matrix=Matrix.Rotation(math.radians(k * 90 + 20), 4, 'Y'), verts=bm.verts)
    prop_parts.append(finish(f"Blade{k}", bm, "PropWood", loc=(0, HUB_Y - 0.04, PZ)))
update()
for p in prop_parts:
    parent(p, prop)
PART = "basket"
for s in (-1, 1):
    bar(f"StrutLo{s}", polar(R + 0.02, math.radians(270 + s * 22.5), 0.04), (s * 0.06, -(APO + 1.08), PZ - 0.06), 0.028, "Iron")
    bar(f"StrutHi{s}", polar(R + 0.02, math.radians(270 + s * 22.5), 0.85), (s * 0.06, -(APO + 1.08), PZ + 0.06), 0.028, "Iron")

# ================================================================ SIDE WINGS / AIR-BRAKE STOPPERS
def wing(side, open_deg):
    """Stopper panel hinged at its front edge. Rest pose = folded flat along the side wall.
    Opening rotates around world Z: left stopper negative, right stopper positive (up to ~90 deg)."""
    global PART
    hinge = Vector((side * (APO + 0.09), R * math.sin(math.pi / 8) + 0.02, 0.55))
    th = -math.pi / 2 - math.radians(open_deg) if side < 0 else -math.pi / 2 + math.radians(open_deg)
    MW = Matrix.Translation(hinge) @ Matrix.Rotation(th, 4, 'Z')
    WS = 0.75                                    # scaled with the basket so it folds flat along the shorter side wall
    outline = [(x * WS, z * WS) for x, z in ((0, 0.50), (0.9, 0.60), (1.28, 0.18), (1.20, -0.24), (0.90, -0.52), (0, -0.46))]
    bm = bmesh.new()
    vs = [bm.verts.new((x, 0, z)) for x, z in outline]
    bm.faces.new(vs)
    tag = "L" if side < 0 else "R"
    PART = f"stopper_{tag}"
    bone(PART, hinge, "basket")
    o = finish(f"Wing{tag}", bm, "Canvas2"); o.matrix_world = MW
    solid(o, 0.025, 0.0)
    W = lambda x, z: MW @ Vector((x, 0, z))
    kids = []
    for i, (x, z) in enumerate(outline[1:-1]):
        kids.append(bar(f"Wing{tag}Rib{i}", W(0.03, 0.0), W(x, z), 0.022, "Brass"))
    for i in range(len(outline) - 1):
        kids.append(bar(f"Wing{tag}Edge{i}", W(*outline[i]), W(*outline[i + 1]), 0.016, "Iron"))
    kids.append(bar(f"Wing{tag}Hinge", W(0, -0.50 * WS), W(0, 0.54 * WS), 0.035, "Brass", n=8))
    PART = "basket"
    for i, z in enumerate((-0.27, 0.27)):
        box(f"Wing{tag}HingeBracket{i}", 0.12, 0.05, 0.05, "Iron", loc=hinge + Vector((-side * 0.06, 0, z)))
    update()
    for k_ in kids:
        parent(k_, o)

wing(-1, 0)    # rest pose: folded
wing(+1, 0)

# ================================================================ STOPPER LEVERS (on the rim above each stopper)
def lever(side, rest_deg=-25):
    """Lever pivots around world X at the rim. rest_deg tilts the handle (negative = toward the back)."""
    global PART
    tag = "L" if side < 0 else "R"
    piv = Vector((side * ((R + 0.07) * math.cos(math.pi / 8) - 0.075), 0.0, 1.13))   # centre of the side wall, mid-rim
    PART = "basket"
    # notched quadrant plate bolted to the rim
    arc = [(0.0, 0.0)] + [(0.15 * math.sin(math.radians(a)), 0.15 * math.cos(math.radians(a)))
                          for a in range(-55, 56, 10)]
    q = profile_prism(f"Lever{tag}Quadrant", arc, -0.012, 0.012, "Brass",
                      loc=piv, rot=(0, math.pi / 2, 0))
    q.rotation_mode = 'XYZ'
    q.rotation_euler = (math.pi / 2, 0, math.pi / 2)    # plate in the world YZ plane
    for i, a in enumerate(range(-45, 46, 30)):
        r_ = math.radians(a)
        box(f"Lever{tag}Notch{i}", 0.03, 0.02, 0.02, "Iron",
            loc=piv + Vector((0, 0.155 * math.sin(r_), 0.155 * math.cos(r_))), rot=(-r_, 0, 0))
    box(f"Lever{tag}Foot", 0.07, 0.24, 0.03, "Iron", loc=piv + Vector((0, 0, -0.03)))
    PART = f"lever_{tag}"
    bone(PART, piv, "basket")
    t = math.radians(rest_deg)
    tip = piv + Vector((0, 0.42 * math.sin(t), 0.42 * math.cos(t)))
    bar(f"Lever{tag}Handle", piv, tip, 0.016, "Iron", n=6)
    ball(f"Lever{tag}Knob", 0.04, tip, "Brass", u=8, v=6)
    bar(f"Lever{tag}Hub", piv + Vector((-0.03, 0, 0)), piv + Vector((0.03, 0, 0)), 0.03, "Brass", n=8)
    PART = "basket"

lever(-1)
lever(+1)

# ================================================================ INNER WALL FITTINGS
def wall_frame(deg):
    """Inner face of the basket wall whose outward normal points at `deg`.
    Returns (centre on inner face at z=0, u = right-hand direction seen from inside, n = into the basket)."""
    a = math.radians(deg)
    n_out = Vector((math.cos(a), math.sin(a), 0))
    u = Vector((math.sin(a), -math.cos(a), 0))
    return n_out * INNER, u, -n_out

def wp(fr, du, z, dn):
    c, u, n = fr
    return c + u * du + n * dn + Vector((0, 0, z))

# ---- navigation display: front wall (+Y), top part just below the rim
# Sloped console (like a desk lectern): the top edge sits against the wall below the rim, the bottom edge
# sticks out into the basket, so the face tilts up 45 deg toward a standing player's eyes.
# Left: old military radar CRT (balloon at the centre, 3 range rings). Right: mechanical speedometer.
PART = "display"
FR = wall_frame(90)
SW, SH, TILT = 0.58, 0.26, math.radians(45)      # face width / slant height / tilt from vertical
ZT, DT = 0.93, 0.03                              # face top height, gap to the wall at the top
DV = -FR[2] * math.sin(TILT) + Vector((0, 0, math.cos(TILT)))   # up the slope (toward the wall)
DN = FR[2] * math.cos(TILT) + Vector((0, 0, math.sin(TILT)))    # face normal: into the basket and up
DU = DV.cross(DN)                                                # right as seen by the player
PT = wp(FR, 0, ZT, DT)
PC = PT - DV * (SH / 2)
FD = frame(PC, DU, DV, DN)
bone("display", PC, "basket")
# housing: convex hull of the wall-side and face-side corners
bm = bmesh.new()
for s in (-1, 1):
    uu = s * (SW / 2 + 0.04)
    for p in (wp(FR, uu, ZT + 0.035, 0.002), wp(FR, uu, ZT - SH * math.cos(TILT) - 0.24, 0.002),
              FD @ Vector((uu, SH / 2 + 0.035, 0)), FD @ Vector((uu, -SH / 2 - 0.035, 0)),
              FD @ Vector((uu, -SH / 2 - 0.035, 0)) - Vector((0, 0, 0.05))):
        bm.verts.new(p)
bmesh.ops.convex_hull(bm, input=bm.verts)
bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(1), verts=bm.verts, edges=bm.edges)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
finish("DisplayHousing", bm, "Iron")
for i, (du, dv) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
    fobj(f"DisplayRivet{i}", FD, b_cyl((du * (SW / 2 + 0.015), dv * (SH / 2 + 0.012)), 0.011, 0.0, 0.008, seg=6), "Brass")
# radar (left)
RC, RR = (-0.145, 0.0), 0.106
fobj("RadarBezel", FD, b_ring(RC, RR, RR + 0.018, 0.0, 0.016), "Brass")
for i in range(4):                                    # bezel bolts
    p = dial(RR + 0.009, 45 + 90 * i)
    fobj(f"RadarBolt{i}", FD, b_cyl((RC[0] + p.x, RC[1] + p.y), 0.006, 0.016, 0.021, seg=6), "Iron")
fobj("RadarScreen", FD, b_disc(RC, RR, 0.004, seg=24), "Display")
RIG["display"] = {"center": list(FD @ Vector((*RC, 0.004))), "u": list(DU), "v": list(DV), "w": 2 * RR, "h": 2 * RR}
# speedometer (right): 180 deg scale across the top, 0 at the left
SPC, SPR = (0.150, 0.0), 0.092
fobj("SpeedBezel", FD, b_ring(SPC, SPR, SPR + 0.014, 0.0, 0.016), "Brass")
fobj("SpeedFace", FD, b_cyl(SPC, SPR, 0.0, 0.004, seg=16), "Gauge")
for i in range(11):
    ph = -90 + 18 * i
    fobj(f"SpeedTick{i}", FD, b_tick(SPC, ph, 0.056 if i % 5 == 0 else 0.066, 0.082, 0.004, 0.004, 0.0065), "Coal")
PART = "speed_needle"            # rotates around the bone axis (dial normal); rest pose = 0 (pointing left)
bone("speed_needle", FD @ Vector((*SPC, 0.008)), "display", tail=FD @ Vector((*SPC, 0.158)))
fobj("SpeedNeedle", FD, b_needle(SPC, -90, 0.072, 0.006, 0.0065, 0.0105, back=0.016), "GaugeRed")
fobj("SpeedNeedlePin", FD, b_cyl(SPC, 0.009, 0.004, 0.015, seg=6), "Brass")
PART = "display"
# warning lamp: top centre of the face, between the radar and the speedometer. Off / orange / red is driven in
# Unreal through its own material slot (M_WarningLamp); socket_warning_lamp marks where to attach a point light.
LC = (0.012, 0.088)
def b_cone(c, r0, r1, n0, n1, seg=8):
    return lambda bm: bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r0, radius2=r1, depth=n1 - n0,
                                            matrix=Matrix.Translation((c[0], c[1], (n0 + n1) / 2)))
fobj("WarnLampBase", FD, b_cyl(LC, 0.031, 0.0, 0.012, seg=8), "Brass")
fobj("WarnLampGlass", FD, lambda bm: [f(bm) for f in (b_cone(LC, 0.021, 0.021, 0.012, 0.025),
                                                       b_cone(LC, 0.021, 0.012, 0.025, 0.036),
                                                       b_cone(LC, 0.012, 0.003, 0.036, 0.040))], "WarnLamp")
cage = []
for i in range(4):
    p_ = dial(0.025, 45 + 90 * i)
    cage.append(b_box((LC[0] + p_.x, LC[1] + p_.y, 0.028), (0.0035, 0.0035, 0.032)))
cage.append(b_box((LC[0], LC[1], 0.0445), (0.052, 0.0035, 0.0035)))
cage.append(b_box((LC[0], LC[1], 0.0445), (0.0035, 0.052, 0.0035)))
fobj("WarnLampCage", FD, lambda bm: [f(bm) for f in cage], "Iron")
socket("socket_warning_lamp", FD @ Vector((*LC, 0.026)), DN)

# ---- weapon wall: the wall to the right of the display (seen from inside), everything on this one wall.
# Left: revolver hook. Centre: upright gun rack (slotted base + notched top shelf) for a shotgun and a rifle.
# Right: upright ammo box whose lid (bone ammo_lid) lifts a little when the player looks at it.
PART = "basket"
FR = wall_frame(45)
FW = frame(FR[0], FR[1], (0, 0, 1), FR[2])        # local (u = right along the wall, z = height, n = into basket)
FU = frame(FR[0], FR[1], -FR[2], (0, 0, 1))       # local (u, -n, z): cylinders standing upright
FH = frame(FR[0], (0, 0, 1), FR[2], FR[1])        # local (z, n, u): cylinders lying along the wall

def wbox(name, u0, u1, z0, z1, n0, n1, mat):
    return fobj(name, FW, b_box(((u0 + u1) / 2, (z0 + z1) / 2, (n0 + n1) / 2), (u1 - u0, z1 - z0, n1 - n0)), mat)

def hull(name, pts, mat):
    bm = bmesh.new()
    for q in pts:
        bm.verts.new(q)
    bmesh.ops.convex_hull(bm, input=bm.verts)
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(1), verts=bm.verts, edges=bm.edges)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return finish(name, bm, mat)

# The console housing leans into this wall's left end (u < -0.354 + n at heights 0.5 .. 0.97), so everything
# sits right of it.
# -- gun rack (u -0.11 .. 0.18)
GL, GR = -0.085, 0.155                              # inner faces of the two side boards
SLOTS = {"shotgun": -0.03, "rifle": 0.10}          # u of each gun
BASE_Z, SLOT_Z, SHELF_Z = 0.125, 0.095, 0.86         # low base (thin board + 3 cm slat lip), slot floor, top shelf
for k, (u0, u1) in enumerate(((GL - 0.025, GL), (GR, GR + 0.025))):   # tapered side boards, like a lectern
    pts = []
    for u_ in (u0, u1):
        for z_, n_ in ((FLOOR_Z, 0.0), (FLOOR_Z, 0.28), (0.42, 0.28), (0.91, 0.18), (0.91, 0.0)):
            pts.append(wp(FR, u_, z_, n_))
    hull(f"GunRackSide{k}", pts, "PropWood")
wbox("GunRackBase", GL, GR, FLOOR_Z, SLOT_Z, 0.0, 0.26, "PropWood")
# slotted top of the base: slats around two butt pockets (u width 0.09, n 0.06 .. 0.20)
cuts = sorted(SLOTS.values())
SLOT_HW = 0.045                                     # half width of a butt pocket
edges = [GL] + [x for c in cuts for x in (c - SLOT_HW, c + SLOT_HW)] + [GR]
for i in range(0, len(edges), 2):
    wbox(f"GunRackSlat{i // 2}", edges[i], edges[i + 1], SLOT_Z, BASE_Z, 0.06, 0.20, "PropWood")
wbox("GunRackSlatBack", GL, GR, SLOT_Z, BASE_Z, 0.0, 0.06, "PropWood")
wbox("GunRackSlatFront", GL, GR, SLOT_Z, BASE_Z, 0.20, 0.26, "PropWood")
for name, c in SLOTS.items():                       # dark felt in the pocket floor
    wbox(f"GunRackPad_{name}", c - SLOT_HW, c + SLOT_HW, SLOT_Z, SLOT_Z + 0.006, 0.06, 0.20, "Burlap")
# weathered back rail behind the barrels
wbox("GunRackRail", GL, GR, 0.66, 0.82, 0.0, 0.025, "Barnwood")
# top shelf with U-notches open toward the player
wbox("GunRackShelfBack", GL - 0.025, GR + 0.025, SHELF_Z, SHELF_Z + 0.03, 0.0, 0.075, "PropWood")
edges = [GL - 0.025] + [x for c in cuts for x in (c - 0.024, c + 0.024)] + [GR + 0.025]
for i in range(0, len(edges), 2):
    wbox(f"GunRackFinger{i // 2}", edges[i], edges[i + 1], SHELF_Z, SHELF_Z + 0.03, 0.075, 0.175, "PropWood")
for name, c in SLOTS.items():
    # item socket: head = where the butt rests in its pocket, Y axis = up along the gun (leaning into the notch)
    butt = wp(FR, c, SLOT_Z + 0.006, 0.13)
    notch = wp(FR, c, SHELF_Z + 0.015, 0.10)
    socket(f"socket_{name}", butt, notch - butt)

# -- revolver hook (left of the rack)
wbox("RevolverBoard", -0.26, -0.13, 0.52, 0.86, 0.0, 0.02, "PropWood")
for i, z_ in enumerate((0.55, 0.83)):
    fobj(f"RevolverBoardRivet{i}", FW, b_cyl((-0.195, z_), 0.008, 0.02, 0.026, seg=6), "Brass")
# short peg, so the revolver hangs flat against the board, clear of the console housing
bar("RevolverHookStem", wp(FR, -0.195, 0.74, 0.02), wp(FR, -0.195, 0.74, 0.07), 0.012, "Brass", n=6)
bar("RevolverHookTip", wp(FR, -0.195, 0.74, 0.07), wp(FR, -0.195, 0.775, 0.078), 0.01, "Brass", n=6)
# item socket: head on the hook tip (revolver hangs by its trigger guard), Y axis = barrel pointing down
socket("socket_revolver", wp(FR, -0.195, 0.765, 0.05), (0, 0, -1))

# -- ammo box (right of the rack): upright wooden box, iron bands, cartridges inside, hinged lid at the back
AU = 0.26                                            # box centre along the wall
AZ0, AZ1, AN0, AN1, AW = 0.46, 0.82, 0.02, 0.15, 0.07
wbox("AmmoCleat", AU - 0.06, AU + 0.06, AZ0 - 0.04, AZ0, 0.0, 0.06, "PropWood")
PART = "ammo_box"
bone("ammo_box", wp(FR, AU, AZ0, (AN0 + AN1) / 2), "basket")
wbox("AmmoBack", AU - AW, AU + AW, AZ0, AZ1, AN0, AN0 + 0.015, "PropWood")
wbox("AmmoFront", AU - AW, AU + AW, AZ0, AZ1, AN1 - 0.015, AN1, "PropWood")
for k, sg in enumerate((-1, 1)):
    wbox(f"AmmoSide{k}", AU + sg * (AW - 0.0075) - 0.0075, AU + sg * (AW - 0.0075) + 0.0075,
         AZ0, AZ1, AN0 + 0.015, AN1 - 0.015, "PropWood")
wbox("AmmoBottom", AU - AW, AU + AW, AZ0, AZ0 + 0.015, AN0, AN1, "PropWood")
wbox("AmmoFill", AU - AW + 0.015, AU + AW - 0.015, AZ0 + 0.015, 0.70, AN0 + 0.015, AN1 - 0.015, "Coal")
for i, z_ in enumerate((AZ0 + 0.012, AZ1 - 0.035)):
    wbox(f"AmmoBand{i}", AU - AW - 0.003, AU + AW + 0.003, z_, z_ + 0.026, AN0 - 0.001, AN1 + 0.003, "Iron")
wbox("AmmoPlate", AU - 0.045, AU + 0.045, 0.60, 0.66, AN1, AN1 + 0.004, "Brass")
shells, heads = [], []
for r_, dn in enumerate((0.055, 0.085, 0.115)):
    for c_ in range(4):
        uu = AU - 0.042 + 0.028 * c_
        shells.append(b_cyl((uu, -dn), 0.0125, 0.70, 0.80, seg=6))
        heads.append(b_cyl((uu, -dn), 0.013, 0.80, 0.815, seg=6))
fobj("AmmoShells", FU, many(*shells), "GaugeRed")
fobj("AmmoShellHeads", FU, many(*heads), "Brass")
for k, (u0, u1) in enumerate(((AU - AW, AU - 0.03), (AU + 0.03, AU + AW))):   # hinge knuckles on the box
    fobj(f"AmmoHinge{k}", FH, b_cyl((AZ1 + 0.008, AN0 - 0.004), 0.007, u0, u1, seg=6), "Iron")
PART = "ammo_lid"                    # rotates around its bone axis (the hinge, along the wall); rest = closed
bone("ammo_lid", wp(FR, AU - AW, AZ1 + 0.008, AN0 - 0.004), "ammo_box", tail=wp(FR, AU + AW, AZ1 + 0.008, AN0 - 0.004))
wbox("AmmoLid", AU - AW - 0.004, AU + AW + 0.004, AZ1, AZ1 + 0.02, AN0 - 0.004, AN1 + 0.008, "PropWood")
wbox("AmmoLidLip", AU - AW - 0.004, AU + AW + 0.004, AZ1 - 0.02, AZ1, AN1 + 0.003, AN1 + 0.008, "Iron")
wbox("AmmoLidTab", AU - 0.018, AU + 0.018, AZ1 - 0.01, AZ1 + 0.012, AN1 + 0.008, AN1 + 0.022, "Brass")
fobj("AmmoLidHinge", FH, b_cyl((AZ1 + 0.008, AN0 - 0.004), 0.007, AU - 0.028, AU + 0.028, seg=6), "Iron")
PART = "basket"

# ---- tool board: wall to the left of the display. A plain tool board in the balloon's colours: plank back panel,
# iron angle frame with brass corner plates. Holds the wrench (brass peg, top left, in plain view from the middle of
# the basket), the grappling hook (two forked brackets, hung diagonally like a fire axe, grip low on the left, claw up
# to the right: at 59 cm it doesn't fit across this wall next to the console) and the extinguisher (floor cup + strap,
# bottom right, hose toward the middle).
# The console housing leans into this wall's right end (u > 0.354 - n at heights 0.5 .. 0.97), so the stand stops
# short of it. Socket names are kept from the old rack: vertical_1 = extinguisher, horizontal = hook, vertical_2 = wrench.
FR = wall_frame(135)
FT = frame(FR[0], FR[1], (0, 0, 1), FR[2])         # local (u = right along the wall, z = height, n = into basket)
U_, N_, Z_ = FR[1], FR[2], Vector((0, 0, 1))
TL, TR, TB, TT = -0.315, 0.285, 0.10, 0.93          # inner edges of the frame: left, right, bottom, top

def tbox(name, u0, u1, z0, z1, n0, n1, mat):
    return fobj(name, FT, b_box(((u0 + u1) / 2, (z0 + z1) / 2, (n0 + n1) / 2), (u1 - u0, z1 - z0, n1 - n0)), mat)

def tframe(u, z, n, X, Y, Z):
    o = wp(FR, u, z, n)
    return Matrix(((X.x, Y.x, Z.x, o.x), (X.y, Y.y, Z.y, o.y), (X.z, Y.z, Z.z, o.z), (0, 0, 0, 1)))

# where each tool hangs (tool model axes: extinguisher / wrench X across, Z up; hook X along the barrel, Z up)
hd = (U_ * math.cos(math.radians(30)) + Z_ * math.sin(math.radians(30))).normalized()
hz = (-U_ * math.sin(math.radians(30)) + Z_ * math.cos(math.radians(30))).normalized()
TOOL = {"extinguisher": tframe(0.18, 0.573, 0.112, -U_, N_, Z_),       # origin = carry handle, tank axis 7.8 cm along X
        "hook": tframe(-0.27, 0.55, 0.07, hd, hz.cross(hd), hz),         # origin = grip end, claw up and to the right
        "wrench": tframe(-0.12, 0.88, 0.125, -U_, N_, Z_)}               # origin = top of the wrench, body 6.5-9.5 cm along -Y
RIG["tool_frames"] = {k: [list(r) for r in M_] for k, M_ in TOOL.items()}

# back panel: six vertical planks, small gaps
pw = (TR - TL) / 6
for k in range(6):
    tbox(f"ToolStandPlank{k}", TL + k * pw + 0.002, TL + (k + 1) * pw - 0.002, TB, TT, 0.0, 0.02, "Barnwood")
# iron angle frame (the console side is kept thin) + brass corner plates with rivets
tbox("ToolStandFrameTop", TL - 0.02, TR + 0.015, TT, TT + 0.022, 0.0, 0.035, "Iron")
tbox("ToolStandFrameBottom", TL - 0.02, TR + 0.015, TB - 0.022, TB, 0.0, 0.035, "Iron")
tbox("ToolStandFrameL", TL - 0.02, TL, TB - 0.022, TT + 0.022, 0.0, 0.035, "Iron")
tbox("ToolStandFrameR", TR, TR + 0.015, TB - 0.022, TT + 0.022, 0.0, 0.033, "Iron")
for i, (u, z) in enumerate(((TL - 0.01, TB - 0.011), (TR + 0.0075, TB - 0.011), (TL - 0.01, TT + 0.011), (TR + 0.0075, TT + 0.011))):
    sx = 1 if u < 0 else -1
    tbox(f"ToolStandCorner{i}", u - 0.012 + sx * 0.012, u + 0.012 + sx * 0.012, z - 0.022, z + 0.022, 0.035, 0.039, "Brass")
    fobj(f"ToolStandRivet{i}", FT, b_cyl((u + sx * 0.012, z), 0.006, 0.039, 0.044, seg=6), "Brass")

# extinguisher: cup standing on the floor, brass strap round the tank
EX = TOOL["extinguisher"]
tank = EX @ Vector((0.078, 0, -0.443))                       # tank axis at its bottom
tu, tn = (tank - FR[0]).dot(U_), (tank - FR[0]).dot(N_)
solid(prism("ToolStandCup", 10, 0.082, 0.082, FLOOR_Z, 0.155, "Iron", top=False, loc=(tank.x, tank.y, 0)), 0.01)
prism("ToolStandCupBase", 10, 0.075, 0.075, FLOOR_Z, 0.13, "Iron", loc=(tank.x, tank.y, 0))
solid(prism("ToolStandStrap", 10, 0.077, 0.077, 0.40, 0.425, "Brass", top=False, bottom=False, loc=(tank.x, tank.y, 0)), 0.008)
tbox("ToolStandStrapArm", tu - 0.012, tu + 0.012, 0.40, 0.425, 0.0, tn - 0.072, "Brass")

# hook: two forked brackets under the barrel
HK = TOOL["hook"]
for i, x in enumerate((0.2, 0.45)):
    p = HK @ Vector((x, 0, -0.075))                          # just under the barrel
    pu, pz = (p - FR[0]).dot(U_), p.z
    bar(f"ToolStandHookArm{i}", wp(FR, pu, pz, 0.02), p, 0.009, "Iron", n=6)
    for s in (-1, 1):                                       # tines either side of the barrel
        base = p + N_ * (s * 0.05)
        bar(f"ToolStandHookTine{i}{s}", p, base, 0.007, "Iron", n=6)
        bar(f"ToolStandHookTip{i}{s}", base, base + hz * 0.05, 0.007, "Brass", n=6)

# wrench: brass peg with a knob, through the ring at the top of the wrench
WR = TOOL["wrench"]
peg = WR @ Vector((0, -0.08, -0.02))
pu = (peg - FR[0]).dot(U_)
bar("ToolStandPeg", wp(FR, pu, peg.z, 0.02), wp(FR, pu, peg.z, 0.075), 0.007, "Brass", n=6)
ball("ToolStandPegKnob", 0.012, wp(FR, pu, peg.z, 0.075), "Brass", u=6, v=4)

socket("socket_tool_vertical_1", EX @ Vector((0, 0, 0)), Z_)          # extinguisher: handle, long axis up
socket("socket_tool_horizontal", HK @ Vector((0, 0, 0)), hd)          # hook: grip end, long axis along the barrel
socket("socket_tool_vertical_2", WR @ Vector((0, 0, 0)), -Z_)         # wrench: top, long axis down

# ================================================================ DETAILS: sandbags, lantern
for k, deg in enumerate((45, 135, 225, 315)):
    a = math.radians(deg)
    ball(f"Sandbag{k}", 0.14, polar(APO + 0.17, a, 0.70), "Burlap", u=8, v=5, scale=(1, 0.8, 1.35))
    bar(f"SandRope{k}", polar(APO + 0.10, a, 1.07), polar(APO + 0.17, a, 0.86), 0.012, "Rope")
PART = "basket"
la = math.radians(112.5)
bar("LanternArm", polar(R + 0.02, la, 1.14), polar(R + 0.22, la, 1.22), 0.02, "Iron")
bar("LanternHook", polar(R + 0.22, la, 1.22), polar(R + 0.22, la, 1.10), 0.01, "Iron")
lp = Vector(polar(R + 0.22, la, 0))
prism("LanternTop", 6, 0.08, 0.03, 1.05, 1.10, "Brass", loc=lp)
prism("LanternGlass", 6, 0.06, 0.06, 0.92, 1.05, "Lamp", loc=lp)
prism("LanternBase", 6, 0.075, 0.075, 0.89, 0.92, "Brass", loc=lp)

update()

# ================================================================ PIECES: split the balloon into separately editable parts
# The balloon itself (basket + envelope + propeller) is the root; each subsidiary part is a child of it.
# Every mesh gets obj["piece"]; Balloon_export.py writes one skeletal mesh per piece (all on the same skeleton):
#   main -> SK_Balloon, stove -> SK_Balloon_Stove, ... (see PIECES).  Hierarchy in the outliner:
#   PART_Balloon
#     GRP_Basket, GRP_Envelope, GRP_Propeller        (the balloon itself, exported as SK_Balloon)
#     PART_Stove, PART_NavConsole, PART_Stoppers, PART_FuelStorage, PART_ToolStorage, PART_WeaponWall
# The empties sit at the origin: they only group. Pivots of animated parts come from the bones (RIG),
# so change positions in this script, not by moving objects in the .blend.
PIECES = {   # piece: (outliner empty, exported skeletal mesh)
    "main": ("PART_Balloon", "SK_Balloon"),
    "stove": ("PART_Stove", "SK_Balloon_Stove"),
    "console": ("PART_NavConsole", "SK_Balloon_NavConsole"),
    "stoppers": ("PART_Stoppers", "SK_Balloon_Stoppers"),
    "fuel": ("PART_FuelStorage", "SK_Balloon_FuelStorage"),
    "tools": ("PART_ToolStorage", "SK_Balloon_ToolStorage"),
    "weapons": ("PART_WeaponWall", "SK_Balloon_WeaponWall"),
}
STOVE_PARTS = {"stove", "gauge", "gauge_needle", "furnace_door", "burn_arrow"}

def piece_of(o):
    part, n = o.get("part", "basket"), o.name[3:]
    if part in STOVE_PARTS or part.startswith("stove_damage_"):
        return "stove"                                   # stove, chimney, gauge, furnace, burn scale, bullet holes
    if part in ("display", "speed_needle"):
        return "console"                                 # radar, speedometer, warning lamp
    if part.startswith(("stopper_", "lever_")) or n.startswith(("WingL", "WingR", "LeverL", "LeverR")):
        return "stoppers"                                # wings + their levers, quadrants and hinge brackets
    if part.startswith(("fuel_", "item_")):
        return "fuel"                                    # wood crate, coal sack, crystal bucket + pieces
    if n.startswith("ToolStand"):
        return "tools"
    if part in ("ammo_box", "ammo_lid") or n.startswith(("GunRack", "Revolver", "Ammo")):
        return "weapons"                                 # gun rack, revolver hook, ammo box
    return "main"

def group_empty(name, par=None):
    e = bpy.data.objects.new(name, None)
    e.empty_display_type = 'PLAIN_AXES'; e.empty_display_size = 0.25
    sc.collection.objects.link(e)
    if par is not None:
        e.parent = par
    return e

roots = {k: group_empty(v[0]) for k, v in PIECES.items()}
for k, e in roots.items():
    if k != "main":
        e.parent = roots["main"]
subs = {k: group_empty(n, roots["main"]) for k, n in (("basket", "GRP_Basket"), ("envelope", "GRP_Envelope"),
                                                        ("propeller", "GRP_Propeller"))}
PROP_DRIVE = ("DriveLink", "Gearbox", "BigGear", "Shaft", "Bearing", "Strut", "PropPivot")
for o in list(sc.objects):
    if o.name.startswith(("PART_", "GRP_")):
        continue
    top = o
    while top.parent is not None:
        top = top.parent
    pc = piece_of(top) if o.type != 'MESH' else piece_of(o)
    if o.type == 'MESH':
        o["piece"] = pc
    if o.parent is not None:
        continue                                         # children (door vents, wing ribs, blades) follow their parent
    if pc == "main":
        part, n = o.get("part", "basket"), o.name[3:]
        g = (subs["envelope"] if part in ("balloon", "ropes") else
             subs["propeller"] if part == "propeller" or n.startswith(PROP_DRIVE) else subs["basket"])
    else:
        g = roots[pc]
    mw = o.matrix_world.copy()
    o.parent = g
    o.matrix_world = mw
RIG["pieces"] = {k: v[1] for k, v in PIECES.items()}

update()
sc["AB_rig"] = json.dumps(RIG)
result = {"objects": len(sc.objects), "bones": list(RIG["bones"]), "sockets": list(RIG["sockets"]),
          "pieces": {k: sum(1 for o in sc.objects if o.get("piece") == k) for k in PIECES}}
