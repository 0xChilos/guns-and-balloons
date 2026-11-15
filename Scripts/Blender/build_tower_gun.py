"""Background Blender (factory startup): build the desert tower's cannon and fuel tank and save
Content/Assets/Enemies/TowerGun.blend with all textures packed (opens in Solid/Texture view).

They replace the team's head / twin guns / boiler on the team's SM_TurretTower (which stays).
Style is steampunk PS1, like the airship rocket.
- SM_TowerGun_Mount: the turntable carriage, pivot at its base centre (goes on the tower's head socket).
  It turns around Z: a riveted iron turntable, two carriage cheeks, an armoured shield, a gauge, a steam pipe.
- SM_TowerGun_Barrel: one heavy cannon, pivot at the trunnion; it elevates around Y.
  Muzzle at +X 4.3 m from the pivot; the C++ uses those numbers (TowerGun* constants).
- SM_TowerGun_FuelTank: the weak spot, pivot at its base centre (goes on the tower's engine socket).
  A riveted red-painted tank lying across on a cradle, raised on an iron stand (a gravity-fed tank,
  LIFT above the deck, so the gun shield never hides it), with a hazard band, a filler cap, a valve and
  a feed pipe down to the deck.

+X = the gun's forward, Z up, metres. In this file the tank stands where it sits on the tower:
3.65 m behind the mount. New pixel textures are generated (64 px, nearest) and written to
Content/Assets/Nemesis/Textures: T_Nemesis_FuelPaint, T_Nemesis_Hazard.
Usage: Blender -b --factory-startup --python build_tower_gun.py [-- preview.png]"""
import bpy, bmesh, math, sys, random
import numpy as np
from mathutils import Vector, Matrix
ASSETS = "/Users/shmyha/Desktop/GameJam/guns-and-balloons/Content/Assets"
TEX_DIR = ASSETS + "/Nemesis/Textures"
OUT_BLEND = ASSETS + "/Enemies/TowerGun.blend"
PREVIEW = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else None

TRUNNION = Vector((0.15, 0.0, 1.35))   # barrel pivot in mount space
MUZZLE = 4.3                           # muzzle distance from the trunnion along +X
TANK_AT = Vector((-3.65, 0.0, 0.0))    # tank base centre relative to the mount (tower: engine - head socket)
LIFT = 1.6                             # the tank stands this high on its stand: above the gun shield, at the
                                       # player's eye level, so it can be hit from any side

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
with bpy.data.libraries.load(ASSETS + "/Nemesis/Nemesis.blend", link=False) as (src, dst):
    dst.materials = [m for m in src.materials if m.startswith("MI_Nemesis_")]

# ---------------------------------------------------------------- textures
N = 64
gi = bpy.data.images.load(TEX_DIR + "/T_Nemesis_Iron.png")
g = np.array(gi.pixels[:]).reshape(N, N, 4)[:, :, :3].mean(axis=2)
grain = (g - g.mean()) / (g.std() + 1e-6)
bpy.data.images.remove(gi)
rng = random.Random(5)


def c(*rgb):
    return np.array(rgb, dtype=float) / 255.0


def save_texture(name, rgb):
    h, w = rgb.shape[:2]
    img = bpy.data.images.new(name, w, h)
    out = np.ones((h, w, 4))
    out[:, :, :3] = np.clip(rgb, 0, 1)
    img.pixels = out.ravel()
    img.filepath_raw = "%s/%s.png" % (TEX_DIR, name)
    img.file_format = 'PNG'
    img.save()
    return img


def rivet(rgb, x, y, light, dark):
    h, w = rgb.shape[:2]
    rgb[y % h, x % w] = light
    rgb[(y - 1) % h, (x + 1) % w] = dark


# Fuel paint: faded red enamel over steel, seams with rivets, chips, dark fuel drips running down.
rgb = c(150, 46, 32) + grain[:, :, None] * 0.02
for row in (0, 32):
    rgb[row, :] = c(92, 26, 18)
    for x in range(1, N, 4):
        rivet(rgb, x, row + 2, c(196, 96, 74), c(80, 22, 15))
for col_ in (0,):
    rgb[:, col_] = c(92, 26, 18)
    for y in range(3, N, 4):
        rivet(rgb, col_ + 2, y, c(196, 96, 74), c(80, 22, 15))
for _ in range(40):                                    # chips down to grey steel
    x, y = rng.randrange(N), rng.randrange(N)
    rgb[y, x] = c(120, 116, 108)
    if rng.random() < 0.4:
        rgb[y, (x + 1) % N] = c(96, 92, 86)
for x in range(N):                                     # fuel drips (rows go bottom-up: run to lower y)
    if rng.random() < 0.18:
        y = rng.randrange(N)
        for k in range(rng.randrange(4, 14)):
            rgb[(y - k) % N, x] *= 0.72
fuel_paint = save_texture("T_Nemesis_FuelPaint", rgb)

# Hazard band: worn yellow/black diagonal stripes, 8 px wide.
rgb = np.zeros((N, N, 3))
for y in range(N):
    for x in range(N):
        rgb[y, x] = c(206, 164, 40) if ((x + y) // 8) % 2 == 0 else c(34, 30, 26)
rgb += grain[:, :, None] * 0.03
for _ in range(60):
    x, y = rng.randrange(N), rng.randrange(N)
    rgb[y, x] = c(110, 100, 86)
hazard = save_texture("T_Nemesis_Hazard", rgb)

loaded = {}
for name in ("Copper", "Brass", "Plates"):
    img = bpy.data.images.load("%s/T_Nemesis_%s.png" % (TEX_DIR, name))
    img.name = "T_Nemesis_" + name
    loaded[name] = img
for name, img in (("FuelPaint", fuel_paint), ("Hazard", hazard), ("Copper", loaded["Copper"]),
                  ("Brass", loaded["Brass"]), ("Plates", loaded["Plates"])):
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
sc.name = "TowerGun"
col, root = collection("TowerGun")


def moved(verts, offset):
    for v in verts:
        v.co += Vector(offset)


def tube(bm, pts, r, sides=6):
    for a, b in zip(pts, pts[1:]):
        a, b = Vector(a), Vector(b)
        d = b - a
        res = bmesh.ops.create_cone(bm, cap_ends=True, segments=sides, radius1=r, radius2=r, depth=d.length)
        rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
        for v in res["verts"]:
            v.co = rot @ v.co + (a + b) / 2


def wheel(bm, center, axis, r, depth, spokes=4):
    """Valve hand-wheel: a ring of short beams plus spokes, in the plane perpendicular to axis."""
    ax = Vector(axis).normalized()
    u = ax.orthogonal().normalized()
    w = ax.cross(u)
    ctr = Vector(center)
    pts = [ctr + (u * math.cos(a) + w * math.sin(a)) * r for a in [i * math.pi / 4 for i in range(9)]]
    tube(bm, pts, depth, sides=4)
    for i in range(spokes):
        a = i * math.pi * 2 / spokes
        tube(bm, [ctr, ctr + (u * math.cos(a) + w * math.sin(a)) * r], depth * 0.7, sides=4)


# ================================================================= mount (turns around Z)
bm = bmesh.new()
loft(bm, [(0.0, 1.75, 1.75), (0.22, 1.75, 1.75), (0.22, 1.55, 1.55), (0.36, 1.55, 1.55)], sides=12, axis="Z")
finish(bm, "SM_TowerGun_Turntable", col, root, "MI_Nemesis_Plates")
bm = bmesh.new()                                                     # brass rim ring on the turntable
loft(bm, [(0.2, 1.79, 1.79), (0.26, 1.79, 1.79)], sides=12, axis="Z", cap=False)
loft(bm, [(0.2, 1.68, 1.68), (0.26, 1.68, 1.68)], sides=12, axis="Z", cap=False)
loft(bm, [(0.26, 1.68, 1.68), (0.26, 1.79, 1.79)], sides=12, axis="Z", cap=False)
finish(bm, "SM_TowerGun_Rim", col, root, "MI_Nemesis_Brass")

# Carriage cheeks: two tall side plates (trapezoids) holding the trunnion, with a cross brace.
bm = bmesh.new()
for s in (-1, 1):
    y = s * 0.62
    prof = [(-1.0, 0.36), (1.0, 0.36), (0.55, 1.6), (-0.35, 1.6)]
    vs = [bm.verts.new((x, y - 0.07, z)) for x, z in prof] + [bm.verts.new((x, y + 0.07, z)) for x, z in prof]
    bm.faces.new(vs[:4]); bm.faces.new(list(reversed(vs[4:])))
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((vs[i], vs[4 + i], vs[4 + j], vs[j]))
box(bm, (-0.75, 0, 0.62), (0.3, 1.24, 0.5))                          # rear brace / breech housing
finish(bm, "SM_TowerGun_Cheeks", col, root, "MI_Nemesis_Iron")

# Armoured shield in front: angled riveted plates with a slot for the barrel.
bm = bmesh.new()
for y0, y1 in ((-1.25, -0.28), (0.28, 1.25)):
    yc, wy = (y0 + y1) / 2, (y1 - y0)
    box(bm, (1.12, yc, 1.1), (0.1, wy, 1.4), rot=Matrix.Rotation(math.radians(-14), 4, "Y"))
box(bm, (1.12, 0, 0.5), (0.1, 0.6, 0.28), rot=Matrix.Rotation(math.radians(-14), 4, "Y"))
box(bm, (1.2, 0, 1.78), (0.1, 0.6, 0.18), rot=Matrix.Rotation(math.radians(-14), 4, "Y"))
for s in (-1, 1):                                                    # side wings, swept back
    box(bm, (0.72, s * 1.32, 1.05), (0.85, 0.08, 1.2), rot=Matrix.Rotation(math.radians(s * -22), 4, "Z"))
finish(bm, "SM_TowerGun_Shield", col, root, "MI_Nemesis_Plates")

# Brass details: trunnion bearings, a pressure gauge on the left cheek, an aiming wheel on the right.
bm = bmesh.new()
for s in (-1, 1):
    cyl(bm, (TRUNNION.x, s * 0.72, TRUNNION.z), 0.2, 0.08, sides=8, axis="Y")
cyl(bm, (-0.45, -0.73, 1.05), 0.17, 0.06, sides=8, axis="Y")
wheel(bm, (-0.6, 0.8, 0.95), (0, 1, 0), 0.26, 0.03)
finish(bm, "SM_TowerGun_MountBrass", col, root, "MI_Nemesis_Brass")
bm = bmesh.new()
cyl(bm, (-0.45, -0.765, 1.05), 0.13, 0.01, sides=8, axis="Y")
finish(bm, "SM_TowerGun_GaugeFace", col, root, "MI_Nemesis_Canvas")
bm = bmesh.new()
box(bm, (-0.41, -0.775, 1.08), (0.1, 0.01, 0.018), rot=Matrix.Rotation(math.radians(35), 4, "Y"))
finish(bm, "SM_TowerGun_GaugeNeedle", col, root, "MI_Nemesis_Red")

# Steam/fuel feed: copper pipe from the back of the carriage down to the turntable edge (towards the tank).
bm = bmesh.new()
tube(bm, [(-0.9, 0.35, 0.75), (-1.25, 0.35, 0.75), (-1.45, 0.35, 0.4)], 0.07)
finish(bm, "SM_TowerGun_MountPipe", col, root, "MI_Nemesis_Copper")

# ================================================================= barrel (elevates around Y at TRUNNION)
T = TRUNNION
bm = bmesh.new()
# breech block, chase, muzzle swell; X from the trunnion
loft(bm, [(T.x - 0.85, 0.28, 0.28), (T.x - 0.55, 0.36, 0.36), (T.x + 0.3, 0.33, 0.33),
          (T.x + 1.4, 0.27, 0.27), (T.x + 3.6, 0.22, 0.22), (T.x + 3.9, 0.22, 0.22),
          (T.x + 4.05, 0.3, 0.3), (T.x + MUZZLE, 0.28, 0.28)], sides=10)
for v in bm.verts:
    v.co.z += T.z
finish(bm, "SM_TowerGun_Tube", col, root, "MI_Nemesis_Iron", origin=T)
bm = bmesh.new()                                                     # dark bore at the muzzle
cyl(bm, (T.x + MUZZLE + 0.005, 0, T.z), 0.17, 0.02, sides=10, axis="X")
finish(bm, "SM_TowerGun_Bore", col, root, "MI_Nemesis_Iron", origin=T)
bm = bmesh.new()                                                     # brass reinforcing bands + trunnion axle
for x0, r in ((0.0, 0.37), (0.9, 0.315), (1.9, 0.275), (3.0, 0.25), (3.95, 0.315)):
    loft(bm, [(T.x + x0, r, r), (T.x + x0 + 0.1, r, r)], sides=10)
for v in bm.verts:
    v.co.z += T.z
cyl(bm, (T.x, 0, T.z), 0.11, 1.5, sides=8, axis="Y")
finish(bm, "SM_TowerGun_Bands", col, root, "MI_Nemesis_Brass", origin=T)
bm = bmesh.new()                                                     # copper recoil/steam cylinders on top
for s in (-1, 1):
    cyl(bm, (T.x + 0.55, s * 0.2, T.z + 0.36), 0.075, 1.3, sides=6, axis="X")
finish(bm, "SM_TowerGun_Recoil", col, root, "MI_Nemesis_Copper", origin=T)

# ================================================================= fuel tank (the weak spot)
K = TANK_AT
bm = bmesh.new()
# Lies across the deck (axis along Y), domed riveted ends.
rs = loft(bm, [(-1.45, 0.25, 0.25), (-1.38, 0.62, 0.62), (-1.28, 0.8, 0.8), (1.28, 0.8, 0.8),
               (1.38, 0.62, 0.62), (1.45, 0.25, 0.25)], sides=12, axis="Y")
moved(bm.verts, (K.x, K.y, K.z + LIFT + 1.05))
finish(bm, "SM_TowerGun_TankShell", col, root, "MI_Nemesis_FuelPaint", origin=K)
bm = bmesh.new()                                                     # hazard band around the middle
loft(bm, [(-0.3, 0.815, 0.815), (0.3, 0.815, 0.815)], sides=12, axis="Y", cap=False)
moved(bm.verts, (K.x, K.y, K.z + LIFT + 1.05))
finish(bm, "SM_TowerGun_TankHazard", col, root, "MI_Nemesis_Hazard", origin=K)
bm = bmesh.new()                                                     # iron cradle and straps
for y in (-0.95, 0.95):
    box(bm, (K.x, y, K.z + LIFT + 0.18), (1.5, 0.22, 0.36))
    for s in (-1, 1):
        box(bm, (K.x + s * 0.55, y, K.z + LIFT + 0.5), (0.18, 0.22, 0.5))
    loft_strap = loft(bm, [(y - 0.06, 0.83, 0.83), (y + 0.06, 0.83, 0.83)], sides=12, axis="Y", cap=False)
    for ring in loft_strap:
        for v in ring:
            v.co += Vector((K.x, 0, K.z + LIFT + 1.05))
# The stand: four iron legs with cross braces from the deck up to the cradle.
for sx in (-1, 1):
    for sy in (-1, 1):
        box(bm, (K.x + sx * 0.6, sy * 0.95, K.z + LIFT / 2), (0.16, 0.16, LIFT))
for sy in (-1, 1):
    tube(bm, [(K.x - 0.6, sy * 0.95, K.z + 0.1), (K.x + 0.6, sy * 0.95, K.z + LIFT - 0.05)], 0.04, sides=4)
    tube(bm, [(K.x + 0.6, sy * 0.95, K.z + 0.1), (K.x - 0.6, sy * 0.95, K.z + LIFT - 0.05)], 0.04, sides=4)
for sx in (-1, 1):
    tube(bm, [(K.x + sx * 0.6, -0.95, K.z + 0.5), (K.x + sx * 0.6, 0.95, K.z + 0.5)], 0.045, sides=4)
finish(bm, "SM_TowerGun_TankCradle", col, root, "MI_Nemesis_Iron", origin=K)
bm = bmesh.new()                                                     # brass filler cap, valve, fuel gauge
cyl(bm, (K.x, 0.75, K.z + LIFT + 1.88), 0.16, 0.12, sides=8, axis="Z")
cyl(bm, (K.x, 0.75, K.z + LIFT + 1.97), 0.1, 0.06, sides=8, axis="Z")
cyl(bm, (K.x + 0.81, -0.55, K.z + LIFT + 1.15), 0.15, 0.06, sides=8, axis="X")
wheel(bm, (K.x + 0.9, 0.5, K.z + LIFT + 0.95), (1, 0, 0), 0.18, 0.025)
finish(bm, "SM_TowerGun_TankBrass", col, root, "MI_Nemesis_Brass", origin=K)
bm = bmesh.new()
cyl(bm, (K.x + 0.845, -0.55, K.z + LIFT + 1.15), 0.11, 0.01, sides=8, axis="X")
finish(bm, "SM_TowerGun_TankGauge", col, root, "MI_Nemesis_Canvas", origin=K)
bm = bmesh.new()                                                     # feed pipe towards the gun
tube(bm, [(K.x + 0.85, 0.5, K.z + LIFT + 0.95), (K.x + 1.25, 0.5, K.z + LIFT + 0.95), (K.x + 1.25, 0.5, K.z + 0.12),
          (K.x + 1.75, 0.5, K.z + 0.12)], 0.065)
finish(bm, "SM_TowerGun_TankPipe", col, root, "MI_Nemesis_Copper", origin=K)

# ---------------------------------------------------------------- scene
deck = bpy.data.objects.new("Preview_Deck", bpy.data.meshes.new("Preview_Deck"))   # not exported
bm = bmesh.new()
box(bm, (-0.9, 0, -0.1), (8.8, 8.8, 0.2))
bm.to_mesh(deck.data); bm.free()
deck.data.materials.append(bpy.data.materials["MI_Nemesis_Wood"])
col.objects.link(deck)
cam = bpy.data.objects.new("TowerGun_Cam", bpy.data.cameras.new("TowerGun_Cam"))
cam.location = Vector((5.0, -12.5, 6.5))
cam.rotation_euler = (Vector((-0.4, 0, 1.4)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
sun = bpy.data.objects.new("TowerGun_Sun", bpy.data.lights.new("TowerGun_Sun", 'SUN'))
sun.rotation_euler = (math.radians(50), 0, math.radians(30))
sc.collection.objects.link(cam); sc.collection.objects.link(sun)
sc.camera = cam
for screen in bpy.data.screens:   # open with the pixel textures visible
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
    sc.render.resolution_x, sc.render.resolution_y = 1200, 700
    sc.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)
tris = {}
for o in col.objects:
    if o.type == 'MESH' and not o.name.startswith("Preview"):
        tris[o.name] = sum(len(p.vertices) - 2 for p in o.data.polygons)
print("TOWERGUN DONE tris=%d" % sum(tris.values()))
