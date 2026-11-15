"""Light beam for searchlights (beacon, AA gun): M_NemesisBeam (unlit, additive, two-sided, fades
along the cone via UV.x, at grazing angles (soft edges; from inside the cone it almost vanishes)
and near the camera) and SM_LightBeam (exported by the Blender helper) using it.
Run in the editor: py Scripts/Editor/setup_light_beam.py"""
import os
import unreal

MEL = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = "/Game/Assets/Enemies"
mpath = FOLDER + "/M_NemesisBeam"

# Rebuild from scratch (MEL.delete_all_material_expressions crashes the 5.9 editor on a loaded material).
if eal.does_asset_exist(mpath):
    eal.delete_asset(mpath)
mat = tools.create_asset("M_NemesisBeam", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("two_sided", True)

color = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 0)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.8, 0.3, 1.0))
inten = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 200)
inten.set_editor_property("parameter_name", "Intensity")
inten.set_editor_property("default_value", 0.1)
uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 350)
mask = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -700, 350)
mask.set_editor_property("r", True); mask.set_editor_property("g", False)
one_minus = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -500, 350)
power = MEL.create_material_expression(mat, unreal.MaterialExpressionPower, -350, 350)
exp = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 450)
exp.set_editor_property("r", 1.6)
MEL.connect_material_expressions(uv, "", mask, "")
MEL.connect_material_expressions(mask, "", one_minus, "")
MEL.connect_material_expressions(one_minus, "", power, "Base")
MEL.connect_material_expressions(exp, "", power, "Exp")
m1 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
MEL.connect_material_expressions(color, "", m1, "A")
MEL.connect_material_expressions(inten, "", m1, "B")
m2 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 200)
MEL.connect_material_expressions(m1, "", m2, "A")
MEL.connect_material_expressions(power, "", m2, "B")

# soft edges: |N . V|^2 (faces seen edge-on fade out)
nrm = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 600)
cam = MEL.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, 700)
dot = MEL.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -700, 650)
MEL.connect_material_expressions(nrm, "", dot, "A")
MEL.connect_material_expressions(cam, "", dot, "B")
ab = MEL.create_material_expression(mat, unreal.MaterialExpressionAbs, -550, 650)
MEL.connect_material_expressions(dot, "", ab, "")
sq = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 650)
MEL.connect_material_expressions(ab, "", sq, "A")
MEL.connect_material_expressions(ab, "", sq, "B")

# near fade: saturate(depth / 2500) so the cone around the camera doesn't paint the screen
depth = MEL.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -900, 850)
div = MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, -700, 850)
div.set_editor_property("const_b", 2500.0)
MEL.connect_material_expressions(depth, "", div, "A")
sat = MEL.create_material_expression(mat, unreal.MaterialExpressionSaturate, -550, 850)
MEL.connect_material_expressions(div, "", sat, "")

m3 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 750)
MEL.connect_material_expressions(sq, "", m3, "A")
MEL.connect_material_expressions(sat, "", m3, "B")
m4 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -50, 300)
MEL.connect_material_expressions(m2, "", m4, "A")
MEL.connect_material_expressions(m3, "", m4, "B")
MEL.connect_material_property(m4, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(mat)
eal.save_loaded_asset(mat)

# the beam mesh (Scripts/Blender/make_light_beam.py): (re)import, then give it the beam material
mesh_path = FOLDER + "/FBX/SM_LightBeam"
if True:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Content/Assets/Enemies/FBX/SM_LightBeam.fbx")
    t.destination_path = FOLDER + "/FBX"
    t.replace_existing = True
    t.automated = True
    tools.import_asset_tasks([t])
sm = unreal.load_asset(mesh_path)
nanite = sm.get_editor_property("nanite_settings")
nanite.set_editor_property("enabled", False)  # Nanite can't draw additive (translucent) materials
sm.set_editor_property("nanite_settings", nanite)
sm.set_material(0, mat)
eal.save_loaded_asset(sm)
for p in eal.list_assets(FOLDER + "/FBX", True, False):
    if "MI_Nemesis_Beam" in p:
        eal.delete_asset(p)
unreal.log("BEAM_SETUP_OK")
