"""Builds the steampunk air balloon game model in the Blender scene "AirBalloon".

Run inside Blender (Text Editor > Run Script). Rebuilds the scene from scratch.
Units: metres, Z up, the balloon flies toward +Y (propeller at -Y).

Interactive / animatable pieces (origin placed at their pivot):
  AB_FurnaceDoor   - furnace door, hinge on its local Z axis (closed = 0 deg)
  AB_WingL/AB_WingR - side air-brake stoppers, hinge on local Z (children follow)
  AB_PropPivot     - propeller, spins around its local Y axis
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
# Balloon_export.py reads both to build the single skinned game mesh.
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
mk("Burlap", (0.38, 0.32, 0.20), (0.18, 0.15, 0.09), rough=1.0, snap=0.04, nscale=9)
mk("Coal", (0.05, 0.05, 0.05), (0.01, 0.01, 0.01), rough=0.6, snap=0.02, nscale=15)
mk("Gauge", (0.70, 0.64, 0.48), (0.45, 0.40, 0.28), rough=0.5, snap=0.01, nscale=20)
mk("GaugeGreen", (0.12, 0.40, 0.08), (0.07, 0.26, 0.05), rough=0.5, snap=0.01, nscale=20)
mk("GaugeOrange", (0.80, 0.36, 0.05), (0.55, 0.22, 0.03), rough=0.5, snap=0.01, nscale=20)
mk("GaugeRed", (0.66, 0.06, 0.03), (0.42, 0.03, 0.02), rough=0.5, snap=0.01, nscale=20)
mk("Fire", None, None, emit=((1.0, 0.33, 0.04), 30.0))
mk("Lamp", None, None, emit=((1.0, 0.75, 0.35), 12.0))
mk("Display", None, None, emit=((0.12, 0.30, 0.16), 2.0))

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
R = 1.3                                   # wall circumradius: 2.26 m inside, room for a 180 cm character
APO = R * math.cos(math.pi / 8)          # 1.201 outer wall distance
WALL_T = 0.07
INNER = APO - WALL_T                      # 1.131 inner wall face
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
solid(prism("LoadRing", 8, 0.98, 0.98, LOAD_Z - 0.04, LOAD_Z + 0.04, "Iron", phase=math.pi / 8, top=False, bottom=False), 0.07)
for k in range(8):
    a = math.pi / 8 + k * math.pi / 4
    bar(f"RopeUp{k}", polar(0.96, a, LOAD_Z), polar(0.62, a, MOUTH_Z), 0.018, "Rope")
    bar(f"RopeDn{k}", polar(R + 0.02, a, 1.15), polar(0.96, a, LOAD_Z), 0.018, "Rope")

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

# coal scuttle on the floor beside the furnace
CB = Vector((0.55, -0.62, 0))
PART = "fuel_bucket"
bone("fuel_bucket", (CB.x, CB.y, Z0), "basket")
solid(prism("CoalScuttle", 8, 0.13, 0.16, Z0, Z0 + 0.22, "Iron", loc=CB, top=False), 0.012)   # empty, walls have an inside
bar("CoalScuttleHandle", CB + Vector((-0.15, 0, Z0 + 0.24)), CB + Vector((0.15, 0, Z0 + 0.24)), 0.012, "Brass")

# chimney pipe -> into the balloon mouth
PART = "stove"
P = [Vector((SX, SY, 1.21)), Vector((SX, SY, 1.80)), Vector((0, -0.18, 2.50)), Vector((0, 0, MOUTH_Z + 0.25))]
for i in range(len(P) - 1):
    bar(f"Pipe{i}", P[i], P[i + 1], 0.08, "Iron", n=8)
    mid = (P[i] + P[i + 1]) / 2
    dd = (P[i + 1] - P[i]).normalized()
    bar(f"PipeCollar{i}", mid - dd * 0.04, mid + dd * 0.04, 0.10, "Brass", n=8)
for i in (1, 2):
    ball(f"Elbow{i}", 0.105, P[i], "Iron", u=8, v=6)
prism("Nozzle", 8, 0.08, 0.17, MOUTH_Z + 0.25, MOUTH_Z + 0.45, "Brass", top=False)

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
    outline = [(0, 0.50), (0.9, 0.60), (1.28, 0.18), (1.20, -0.24), (0.90, -0.52), (0, -0.46)]
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
    kids.append(bar(f"Wing{tag}Hinge", W(0, -0.50), W(0, 0.54), 0.035, "Brass", n=8))
    PART = "basket"
    for i, z in enumerate((-0.35, 0.35)):
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
    piv = Vector((side * ((R + 0.07) * math.cos(math.pi / 8) - 0.075), 0.05, 1.13))
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

def hook(name, fr, du, z):
    bar(name + "Stem", wp(fr, du, z, 0.02), wp(fr, du, z, 0.09), 0.012, "Brass", n=6)
    bar(name + "Tip", wp(fr, du, z, 0.09), wp(fr, du, z + 0.045, 0.10), 0.01, "Brass", n=6)

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

# ---- weapon rack: wall to the right of the display
PART = "basket"
FR = wall_frame(45)
ang = math.atan2(FR[1].y, FR[1].x)
BW = 2 * INNER * math.tan(math.pi / 8) - 0.06    # board width = wall width minus a margin
box("WeaponRackBoard", BW, 0.02, 0.60, "PropWood", loc=wp(FR, 0, 0.62, 0.01), rot=(0, 0, ang))
for k, z in enumerate((0.50, 0.68)):          # two long guns, hooks at both edges of the wall
    for s in (-1, 1):
        hook(f"WeaponHookLong{k}{'L' if s < 0 else 'R'}", FR, s * (BW / 2 - 0.07), z)
    socket(f"socket_long_gun_{k + 1}", wp(FR, 0, z + 0.03, 0.08), FR[1])
for s in (-1, 1):                              # small weapon, centre top
    hook(f"WeaponHookSmall{'L' if s < 0 else 'R'}", FR, s * 0.08, 0.84)
socket("socket_small_weapon", wp(FR, 0, 0.87, 0.08), FR[1])

# ---- tool rack: wall to the left of the display
FR = wall_frame(135)
ang = math.atan2(FR[1].y, FR[1].x)
box("ToolRackBoard", BW, 0.02, 0.60, "PropWood", loc=wp(FR, 0, 0.62, 0.01), rot=(0, 0, ang))
for s in (-1, 1):                              # one horizontal slot at the bottom
    hook(f"ToolHookH{'L' if s < 0 else 'R'}", FR, s * (BW / 2 - 0.09), 0.45)
socket("socket_tool_horizontal", wp(FR, 0, 0.48, 0.08), FR[1])
for k, s in enumerate((-1, 1)):                # two vertical slots above it
    du = s * 0.18
    for j, z in enumerate((0.62, 0.87)):
        bar(f"ToolClip{k}{j}Arm", wp(FR, du, z, 0.02), wp(FR, du, z, 0.04), 0.01, "Brass", n=6)
        solid(prism(f"ToolClip{k}{j}Ring", 8, 0.035, 0.035, -0.012, 0.012, "Brass", top=False, bottom=False,
                    loc=wp(FR, du, z, 0.075)), 0.01)
    socket(f"socket_tool_vertical_{k + 1}", wp(FR, du, 0.64, 0.075), (0, 0, 1))

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
sc["AB_rig"] = json.dumps(RIG)
result = {"objects": len(sc.objects), "bones": list(RIG["bones"]), "sockets": list(RIG["sockets"])}
