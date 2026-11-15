"""Background Blender (factory startup): build the nemesis airship's rocket and save
Content/Assets/Enemies/Rocket.blend, textures included (packed).

Look (team decision 2026-11-15): a cruise missile in the game's steampunk PS1 style.
- Angular body: flat bottom, chamfered top, wedge nose, raised dorsal spine.
- Riveted steel plates, brass bands, a copper nose cap.
- Copper steam pipes along the sides, a brass pressure gauge on the spine.
- Small X tail fins.

The pixel textures T_Nemesis_Plates and T_Nemesis_Copper (64x64) are generated here from the
T_Nemesis_Iron grain and written to Content/Assets/Nemesis/Textures. The materials are copies of
MI_Nemesis_Iron with those images; slot names match the Unreal MI_Nemesis_* instances.
+X = forward, origin = centre, ~2.7 m long, 0.4 m across.
Usage: Blender -b --factory-startup --python build_rocket.py [-- preview.png]"""
import bpy, bmesh, math, sys, random
import numpy as np
from mathutils import Vector
ASSETS = "/Users/shmyha/Desktop/GameJam/guns-and-balloons/Content/Assets"
TEX_DIR = ASSETS + "/Nemesis/Textures"
OUT_BLEND = ASSETS + "/Enemies/Rocket.blend"
PREVIEW = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else None

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
with bpy.data.libraries.load(ASSETS + "/Nemesis/Nemesis.blend", link=False) as (src, dst):
    dst.materials = [m for m in src.materials if m.startswith("MI_Nemesis_")]

# ---------------------------------------------------------------- textures
N = 64
grain = bpy.data.images.load(TEX_DIR + "/T_Nemesis_Iron.png")
px = np.array(grain.pixels[:]).reshape(N, N, 4)          # rows bottom-up, linear-ish floats
noise = px[:, :, :3].mean(axis=2)
noise = (noise - noise.mean()) / (noise.std() + 1e-6)     # zero-mean grain
bpy.data.images.remove(grain)
rng = random.Random(7)


def srgb(c):
    return np.array(c, dtype=float) / 255.0


def save_texture(name, rgb):
    img = bpy.data.images.new(name, N, N)
    out = np.ones((N, N, 4))
    out[:, :, :3] = np.clip(rgb, 0, 1)
    img.pixels = out.ravel()
    img.filepath_raw = "%s/%s.png" % (TEX_DIR, name)
    img.file_format = 'PNG'
    img.save()
    return img


def rivet(rgb, x, y, light, dark):
    rgb[y % N, x % N] = light
    rgb[(y - 1) % N, (x + 1) % N] = dark   # shadow below-right (rows go bottom-up)


# Plates: pale weathered steel, staggered 32x16 px plates (1 m x 0.5 m) with seams and rivet rows.
base = srgb((168, 168, 158))
rgb = base + noise[:, :, None] * 0.018
for y in range(N):
    for x in range(N):
        if rng.random() < 0.05:                       # grime speckle
            rgb[y, x] *= 0.92
seam, light, dark = srgb((104, 102, 95)), srgb((192, 190, 180)), srgb((112, 110, 102))
for row in range(4):
    y0 = row * 16
    rgb[y0, :] = seam
    off = 0 if row % 2 == 0 else 16
    for xs in (off, off + 32):
        rgb[y0:y0 + 16, xs % N] = seam
    for x in range(2, N, 4):
        rivet(rgb, x, y0 + 2, light, dark)
    for xs in (off, off + 32):
        for y in range(y0 + 5, y0 + 16, 5):
            rivet(rgb, xs + 2, y, light, dark)
for x in range(N):                                    # streaks of soot running back
    if rng.random() < 0.12:
        y = rng.randrange(N)
        for k in range(rng.randrange(3, 9)):
            rgb[(y - k) % N, x] *= 0.8
plates = save_texture("T_Nemesis_Plates", rgb)

# Copper: warm copper with verdigris spots and rivets every 8 px.
rgb = srgb((156, 84, 46)) + noise[:, :, None] * 0.05
for _ in range(26):
    x, y = rng.randrange(N), rng.randrange(N)
    rgb[y, x] = srgb((88, 140, 112))
    if rng.random() < 0.5:
        rgb[y, (x + 1) % N] = srgb((70, 118, 94))
for y in range(4, N, 16):
    for x in range(3, N, 8):
        rivet(rgb, x, y, srgb((222, 160, 110)), srgb((80, 40, 22)))
copper = save_texture("T_Nemesis_Copper", rgb)

brass = bpy.data.images.load(TEX_DIR + "/T_Nemesis_Brass.png")  # not in Nemesis.blend
brass.name = "T_Nemesis_Brass"
for name, img in (("Plates", plates), ("Copper", copper), ("Brass", brass)):
    m = bpy.data.materials["MI_Nemesis_Iron"].copy()
    m.name = "MI_Nemesis_" + name
    for n in m.node_tree.nodes:
        if n.type == 'TEX_IMAGE':
            n.image = img
            n.interpolation = 'Closest'
for m in bpy.data.materials:
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image and not n.image.packed_file:
                n.image.pack()
exec(open(ASSETS + "/Nemesis/bl_helpers.py").read())

sc = bpy.context.scene
sc.name = "Rocket"
col, root = collection("Rocket")

# ---------------------------------------------------------------- body
# Stations: (x, half width, bottom z, top z). Flat bottom, chamfered top, wedge nose.
BODY = [(-1.18, 0.12, -0.11, 0.10), (-0.98, 0.19, -0.17, 0.16), (0.62, 0.19, -0.17, 0.16)]
NOSE = [(0.62, 0.19, -0.17, 0.16), (0.88, 0.178, -0.155, 0.135), (1.08, 0.135, -0.115, 0.085)]
CAP = [(1.08, 0.135, -0.115, 0.085), (1.24, 0.075, -0.075, 0.025), (1.34, 0.012, -0.045, -0.025)]


def section(w, zb, zt, grow=0.0):
    """Six-point section (y, z): flat bottom, vertical lower sides, chamfered top."""
    w, zb, zt = w + grow, zb - grow, zt + grow
    zs = zb + 0.6 * (zt - zb)
    return [(-w, zb), (w, zb), (w, zs), (0.55 * w, zt), (-0.55 * w, zt), (-w, zs)]


def hull(bm, stations, grow=0.0, cap=True):
    rings = [[bm.verts.new((x, y, z)) for y, z in section(w, zb, zt, grow)] for x, w, zb, zt in stations]
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(6):
            j = (i + 1) % 6
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    if cap:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])


def at(stations, x):
    """Section values interpolated at x along a station list."""
    for (x0, *a), (x1, *b) in zip(stations, stations[1:]):
        if x0 <= x <= x1:
            t = (x - x0) / (x1 - x0)
            return [p + (q - p) * t for p, q in zip(a, b)]
    raise ValueError(x)


bm = bmesh.new()
hull(bm, BODY + NOSE[1:])
finish(bm, "SM_Rocket_Hull", col, root, "MI_Nemesis_Plates")

bm = bmesh.new()
hull(bm, CAP)
finish(bm, "SM_Rocket_NoseCap", col, root, "MI_Nemesis_Copper")

# Brass bands: the reference's yellow ring, one at the tail end of the body.
bm = bmesh.new()
for x0, x1, src in ((0.66, 0.72, NOSE), (-0.96, -0.9, BODY)):
    hull(bm, [(x0, *at(src, x0)), (x1, *at(src, x1))], grow=0.012)
finish(bm, "SM_Rocket_Bands", col, root, "MI_Nemesis_Brass")

# Dorsal spine: a riveted ridge on the top, tapering at both ends.
bm = bmesh.new()
top = 0.16
spine = [(-0.92, 0.035, 0.0), (-0.8, 0.06, 0.045), (0.3, 0.06, 0.045), (0.5, 0.035, 0.0)]
rings = []
for x, w, h in spine:
    rings.append([bm.verts.new((x, y, top + z)) for y, z in ((-w, 0), (w, 0), (w * 0.7, h), (-w * 0.7, h))])
for r0, r1 in zip(rings, rings[1:]):
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
finish(bm, "SM_Rocket_Spine", col, root, "MI_Nemesis_Plates")

# Steam pipes along both sides with brass clamps, bending into the body at the ends.
bm = bmesh.new()
for s in (-1, 1):
    y = s * 0.205
    cyl(bm, (-0.15, y, -0.03), 0.024, 1.3, sides=6, axis="X")
    for x in (-0.8, 0.5):
        cyl(bm, (x, s * 0.198, -0.03), 0.022, 0.03, sides=6, axis="Y")
finish(bm, "SM_Rocket_Pipes", col, root, "MI_Nemesis_Copper")
bm = bmesh.new()
for s in (-1, 1):
    for x in (-0.55, -0.15, 0.25):
        box(bm, (x, s * 0.205, -0.03), (0.035, 0.06, 0.065))
# Pressure gauge on the spine (brass bezel), dial below.
cyl(bm, (0.38, 0.0, 0.225), 0.065, 0.03, sides=8, axis="Z")
finish(bm, "SM_Rocket_Brass", col, root, "MI_Nemesis_Brass")
bm = bmesh.new()
cyl(bm, (0.38, 0.0, 0.243), 0.05, 0.008, sides=8, axis="Z")
finish(bm, "SM_Rocket_GaugeFace", col, root, "MI_Nemesis_Canvas")
bm = bmesh.new()
box(bm, (0.395, 0.008, 0.249), (0.04, 0.008, 0.004), rot=None)  # needle
finish(bm, "SM_Rocket_GaugeNeedle", col, root, "MI_Nemesis_Red")

# Four small swept tail fins, X pattern.
bm = bmesh.new()
for k in range(4):
    ang = math.pi / 4 + k * math.pi / 2
    d = Vector((0, math.cos(ang), math.sin(ang)))
    t = Vector((0, -math.sin(ang), math.cos(ang))) * 0.014
    quad = [Vector((-0.7, 0, 0)) + d * 0.16, Vector((-0.98, 0, 0)) + d * 0.4,
            Vector((-1.12, 0, 0)) + d * 0.4, Vector((-1.12, 0, 0)) + d * 0.12]
    vs = [bm.verts.new(p + t) for p in quad] + [bm.verts.new(p - t) for p in quad]
    bm.faces.new(vs[:4]); bm.faces.new(list(reversed(vs[4:])))
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((vs[i], vs[4 + i], vs[4 + j], vs[j]))
finish(bm, "SM_Rocket_Fins", col, root, "MI_Nemesis_Plates")

# Exhaust: dark nozzle, copper outer ring, and the flame (glow; separate so the game can flicker it).
bm = bmesh.new()
cyl(bm, (-1.24, 0, -0.005), 0.085, 0.12, sides=8, axis="X", radius2=0.1)
finish(bm, "SM_Rocket_Nozzle", col, root, "MI_Nemesis_Dark")
bm = bmesh.new()
cyl(bm, (-1.21, 0, -0.005), 0.11, 0.04, sides=8, axis="X")
finish(bm, "SM_Rocket_NozzleRing", col, root, "MI_Nemesis_Copper")
bm = bmesh.new()
loft(bm, [(-1.31, 0.09, 0.09), (-1.52, 0.13, 0.13), (-1.86, 0.08, 0.08), (-2.1, 0.02, 0.02)])
for v in bm.verts:
    v.co.z -= 0.005
# Pivot at the nozzle: the game flickers the flame by scaling it, and it must stay attached.
finish(bm, "SM_Rocket_Flame", col, root, "MI_Nemesis_Glow", origin=(-1.31, 0, -0.005))

# ---------------------------------------------------------------- scene
cam = bpy.data.objects.new("Rocket_Cam", bpy.data.cameras.new("Rocket_Cam"))
cam.location = Vector((2.2, -3.4, 1.4))
cam.rotation_euler = (Vector((0.0, 0, 0)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
sun = bpy.data.objects.new("Rocket_Sun", bpy.data.lights.new("Rocket_Sun", 'SUN'))
sun.rotation_euler = (math.radians(50), 0, math.radians(30))
sc.collection.objects.link(cam); sc.collection.objects.link(sun)
sc.camera = cam
# Open with the pixel textures visible: every 3D view in Solid mode, colour from the texture.
for screen in bpy.data.screens:
    for area in screen.areas:
        for space in area.spaces:
            if space.type == 'VIEW_3D':
                space.shading.type = 'SOLID'
                space.shading.color_type = 'TEXTURE'
bpy.ops.wm.save_as_mainfile(filepath=OUT_BLEND)

if PREVIEW:
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'TEXTURE'
    sc.render.resolution_x, sc.render.resolution_y = 1200, 600
    sc.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in col.objects if o.type == 'MESH')
print("ROCKET DONE tris=%d" % tris)
