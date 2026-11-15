"""Builds the crosshair material: /Game/Materials/Crosshair/M_Crosshair (UI, translucent) + MI_Crosshair.

PS1 look done in the material itself (the HUD draws after post-processing): chunky cells, a dark
outline, colour quantized with a 4x4 Bayer dither, dithered opacity and a 12 fps grain.
ACrosshairHUD sets QuadCells/Origin/Gap/DotSize/TickOpacity/DotOpacity/GrainTime/TickOffset every frame;
colours, tick length/thickness, grain and levels are the look, set on MI_Crosshair.

Re-running updates the graph in place (nothing deleted) and keeps MI_Crosshair's overrides. Run in the editor (py "<path>") or headless:
  UnrealEditor-Cmd.exe gunsandballoons.uproject -run=pythonscript -script="Scripts/Editor/create_crosshair_material.py"
"""
import unreal

FOLDER = "/Game/Materials/Crosshair"
MAT_NAME = "M_Crosshair"
MI_NAME = "MI_Crosshair"

HLSL = r"""
// Cells = the crosshair's low-res pixels. p = cell relative to the centre cell.
float2 cell = floor(UV * QuadCells);
float hc = floor(QuadCells * 0.5);
float2 p = cell - hc;                       // the dot: always on the true aim
float2 a = abs(p);
float2 at = abs(p - floor(TickOffset.xy + 0.5)); // the ticks: carried by the inertia (whole cells)
float g = max(0.0, floor(Gap + 0.5));
float L = max(1.0, floor(Len + 0.5));
float th = floor(max(Thick - 1.0, 0.0) * 0.5 + 0.01);   // 1 -> 0, 3 -> 1 (half-thickness)
float dh = floor(max(DotSize - 1.0, 0.0) * 0.5 + 0.01);
float hasDot = DotSize > 0.5 ? 1.0 : 0.0;

// Four ticks and the dot, and the same shapes grown by one cell for the outline.
float tick = ((at.x >= g + 1.0 && at.x <= g + L && at.y <= th) || (at.y >= g + 1.0 && at.y <= g + L && at.x <= th)) ? 1.0 : 0.0;
float tickO = ((at.x >= g && at.x <= g + L + 1.0 && at.y <= th + 1.0) || (at.y >= g && at.y <= g + L + 1.0 && at.x <= th + 1.0)) ? 1.0 : 0.0;
float dotm = (a.x <= dh && a.y <= dh) ? hasDot : 0.0;
float dotO = (a.x <= dh + 1.0 && a.y <= dh + 1.0) ? hasDot * OutlineOn : 0.0;
tickO *= OutlineOn;

// 4x4 Bayer threshold on the screen-wide cell grid (Origin = this quad's first cell).
float2 ac = cell + Origin.xy;
int2 q = int2(ac - 4.0 * floor(ac / 4.0));
const float B[16] = {0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5};
float bay = (B[q.y * 4 + q.x] + 0.5) / 16.0;

// Dithered fades: a cell is either there or not.
float showT = TickOpacity > bay ? 1.0 : 0.0;
float showD = DotOpacity > bay ? 1.0 : 0.0;
float fill = max(tick * showT, dotm * showD);
float outl = max(tickO * showT, dotO * showD);

// Grain (12 fps, per cell), then colour quantize with the same Bayer dither.
float frame = floor(GrainTime * 12.0);
float n = frac(sin(dot(ac + frame * float2(17.0, 59.0), float2(12.9898, 78.233))) * 43758.5453);
float3 c = fill > 0.5 ? Fill.rgb : Outline.rgb;
c *= 1.0 + (n - 0.5) * 2.0 * Grain;
c = floor(saturate(c) * Levels + (bay - 0.5) + 0.5) / Levels;
float alpha = max(fill, outl);
return float4(saturate(c) * alpha, alpha);
"""

SCALARS = [
    # name, default
    ("QuadCells", 121.0),
    ("Gap", 4.0),
    ("Len", 5.0),
    ("Thick", 1.0),
    ("DotSize", 1.0),
    ("TickOpacity", 1.0),
    ("DotOpacity", 1.0),
    ("Grain", 0.06),
    ("Levels", 24.0),
    ("OutlineOn", 1.0),
    ("GrainTime", 0.0),
]
VECTORS = [
    # Bone-white fill and a near-black olive outline: reads on sky and on the basket, fits the oily grade.
    ("Fill", unreal.LinearColor(0.86, 0.80, 0.62, 1.0)),
    ("Outline", unreal.LinearColor(0.04, 0.05, 0.03, 1.0)),
    ("Origin", unreal.LinearColor(0.0, 0.0, 0.0, 0.0)),
    ("TickOffset", unreal.LinearColor(0.0, 0.0, 0.0, 0.0)),  # cells, set by the HUD (inertia)
]

mel = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# Updated in place: nothing is deleted (MaterialEditingLibrary.DeleteAllMaterialExpressions asserts on a
# material the running game/editor holds, and the HUD class references MI_Crosshair). An existing graph
# gets its Custom node's code and inputs refreshed; parameter nodes are reused by name, missing ones added.
MAT_PATH = f"{FOLDER}/{MAT_NAME}"
MI_PATH = f"{FOLDER}/{MI_NAME}"
input_names = ["UV"] + [n for n, _ in SCALARS] + [n for n, _ in VECTORS]

def make_input(n):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    return ci

custom = None
existing = {}  # input name -> node feeding it
if eal.does_asset_exist(MAT_PATH):
    mat = eal.load_asset(MAT_PATH)
    mask = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    feeds = mel.get_inputs_for_material_expression(mat, mask) if mask else []
    if feeds and isinstance(feeds[0], unreal.MaterialExpressionCustom):
        custom = feeds[0]
        old_names = [str(i.get_editor_property("input_name")) for i in custom.get_editor_property("inputs")]
        for name, node in zip(old_names, mel.get_inputs_for_material_expression(mat, custom)):
            if node:
                existing[name] = node
else:
    mat = assets.create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
# Translucent with 0/1 opacity: the canvas HUD ignores an opacity mask (a masked UI material draws its whole quad).
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

fresh = custom is None
if fresh:
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
custom.set_editor_property("code", HLSL)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
custom.set_editor_property("description", "Crosshair")
custom.set_editor_property("inputs", [make_input(n) for n in input_names])  # drops the old links; relinked below

uv = existing.get("UV") or mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -700, -200)
mel.connect_material_expressions(uv, "", custom, "UV")

y = -120
for name, default in SCALARS:
    node = existing.get(name)
    if not node:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, y)
        node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    mel.connect_material_expressions(node, "", custom, name)
    y += 70
for name, default in VECTORS:
    node = existing.get(name)
    if not node:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1000, y)
        node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    mel.connect_material_expressions(node, "", custom, name)
    y += 160

if fresh:
    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 0, -60)
    for ch, on in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(ch, on)
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 0, 60)
    for ch, on in (("r", False), ("g", False), ("b", False), ("a", True)):
        alpha.set_editor_property(ch, on)
    mel.connect_material_expressions(custom, "", rgb, "")
    mel.connect_material_expressions(custom, "", alpha, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
else:
    # Older builds of this graph fed the alpha to Opacity Mask; feed Opacity.
    alpha = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY_MASK) or mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_asset(mat.get_path_name())

if eal.does_asset_exist(MI_PATH):
    mi = eal.load_asset(MI_PATH)
else:
    mi = assets.create_asset(MI_NAME, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi, mat)
eal.save_asset(mi.get_path_name())

unreal.log(f"Crosshair material built: {mat.get_path_name()}, {mi.get_path_name()}")
