"""Low-poly modelling helpers for the enemy models (PS1 style: octagonal forms, flat shading,
pixel textures from the MI_Nemesis_* materials, box-projected UVs). exec() this file inside
Blender before building. Units are metres, +X is the model's forward."""
import math
import bmesh
import bpy
from mathutils import Matrix, Vector

UV_SCALE = 0.5  # texture repeats every 2 m


def collection(name, location=(0, 0, 0)):
    """Fresh collection with a root empty (deletes an existing one of that name)."""
    col = bpy.data.collections.get(name)
    if col:
        for o in list(col.all_objects):
            bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.collections.remove(col)
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    root = bpy.data.objects.new(name, None)
    root.empty_display_size = 1.0
    root.location = location
    col.objects.link(root)
    return col, root


def mat(name):
    return bpy.data.materials[name]


def box_uv(bm):
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            c = l.vert.co
            if ax == 0:
                l[uv].uv = (c.y * UV_SCALE, c.z * UV_SCALE)
            elif ax == 1:
                l[uv].uv = (c.x * UV_SCALE, c.z * UV_SCALE)
            else:
                l[uv].uv = (c.x * UV_SCALE, c.y * UV_SCALE)


def finish(bm, name, col, root, material, origin=(0, 0, 0)):
    """bmesh in model space -> object parented to root; origin = pivot (model space)."""
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.translate(bm, verts=bm.verts, vec=-Vector(origin))
    box_uv(bm)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    me.materials.append(mat(material))
    o = bpy.data.objects.new(name, me)
    col.objects.link(o)
    o.parent = root
    o.location = Vector(origin)
    return o


def _xform(verts, rot=None, loc=(0, 0, 0)):
    m = Matrix.Translation(Vector(loc))
    if rot is not None:
        m = m @ rot
    for v in verts:
        v.co = m @ v.co


def box(bm, center, size, rot=None):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r["verts"]:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    _xform(r["verts"], rot, center)
    return r["verts"]


def axis_rot(axis):
    """Rotation that maps local +Z to the given axis ('X', 'Y', 'Z', '-X', ...)."""
    sign = -1 if axis.startswith("-") else 1
    a = axis[-1]
    if a == "Z":
        return Matrix.Rotation(0 if sign > 0 else math.pi, 4, "X")
    if a == "X":
        return Matrix.Rotation(sign * math.pi / 2, 4, "Y")
    return Matrix.Rotation(-sign * math.pi / 2, 4, "X")


def cyl(bm, center, radius, depth, sides=8, axis="Z", radius2=None):
    """Prism/frustum along axis; radius2 = top radius (cone if 0)."""
    r2 = radius if radius2 is None else radius2
    r = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides,
                              radius1=radius, radius2=r2, depth=depth)
    for v in r["verts"]:  # rotate half a segment so an octagon has flat sides up/down
        v.co.rotate(Matrix.Rotation(math.pi / sides, 3, "Z"))
    _xform(r["verts"], axis_rot(axis), center)
    return r["verts"]


def loft(bm, rings, sides=8, axis="X", cap=True):
    """rings: [(position along axis, radius_a, radius_b)] -> closed octagonal tube.
    radius_a is across the first perpendicular axis, radius_b the second (oval sections)."""
    verts_rings = []
    for (p, ra, rb) in rings:
        ring = []
        for i in range(sides):
            ang = 2 * math.pi * (i + 0.5) / sides
            a, b = math.cos(ang) * ra, math.sin(ang) * rb
            if axis == "X":
                co = (p, a, b)
            elif axis == "Y":
                co = (a, p, b)
            else:
                co = (a, b, p)
            ring.append(bm.verts.new(co))
        verts_rings.append(ring)
    for r0, r1 in zip(verts_rings, verts_rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    if cap:
        bm.faces.new(list(reversed(verts_rings[0])))
        bm.faces.new(verts_rings[-1])
    return verts_rings


def beam(bm, a, b, thickness):
    """Square strut from point a to point b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    rot = d.to_track_quat("Z", "Y").to_matrix().to_4x4()
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r["verts"]:
        v.co = Vector((v.co.x * thickness, v.co.y * thickness, v.co.z * d.length))
    _xform(r["verts"], rot, (a + b) / 2)
    return r["verts"]


def slab(bm, sections):
    """Wing/fin from rectangular sections: [(center (x,y,z), chord along X, thickness along Z, axis)]
    joined in order along the span. Each section is a rectangle perpendicular to the span."""
    rings = []
    for (c, chord, thick, span_axis) in sections:
        c = Vector(c)
        if span_axis == "Y":   # wing: rectangle in XZ
            pts = [(-chord / 2, -thick / 2), (chord / 2, -thick / 2), (chord / 2, thick / 2), (-chord / 2, thick / 2)]
            ring = [bm.verts.new((c.x + a, c.y, c.z + b)) for a, b in pts]
        else:                   # fin: span along Z, rectangle in XY
            pts = [(-chord / 2, -thick / 2), (chord / 2, -thick / 2), (chord / 2, thick / 2), (-chord / 2, thick / 2)]
            ring = [bm.verts.new((c.x + a, c.y + b, c.z)) for a, b in pts]
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])


def prop(bm, center, radius, blade_w=0.16, axis="X"):
    """Two-blade propeller + hub, spinning around axis."""
    rot = axis_rot(axis)
    vs = box(bm, (0, 0, 0), (blade_w, radius * 2, 0.05))
    vs += cyl(bm, (0, 0, 0.0), blade_w * 0.9, 0.18, sides=8)
    _xform(vs, rot, center)
