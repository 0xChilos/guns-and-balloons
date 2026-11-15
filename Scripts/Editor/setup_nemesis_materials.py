"""Imports the nemesis airship pixel textures and rebuilds its materials.

Run in the editor: py Scripts/Editor/setup_nemesis_materials.py
Source PNGs: Content/Assets/Nemesis/Textures (exported from Nemesis.blend).
"""
import os
import unreal

FOLDER = "/Game/Assets/Nemesis"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Content/Assets/Nemesis/Textures")
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# 1. Textures: point filtering, no mips -> PS1 pixels.
tasks = []
for f in sorted(os.listdir(SRC)):
    if f.endswith(".png"):
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SRC, f)
        t.destination_path = FOLDER + "/Textures"
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
tools.import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        tex = unreal.load_asset(p)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)

# 2. Master material: BaseColor texture * Tint, optional emissive.
mpath = FOLDER + "/M_Nemesis"
if unreal.EditorAssetLibrary.does_asset_exist(mpath):
    mat = unreal.load_asset(mpath)
    MEL.delete_all_material_expressions(mat)
else:
    mat = tools.create_asset("M_Nemesis", FOLDER, unreal.Material, unreal.MaterialFactoryNew())

tex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
tex.set_editor_property("parameter_name", "BaseColor")
tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
tex.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 250)
tint.set_editor_property("parameter_name", "Tint")
tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
MEL.connect_material_expressions(tex, "RGB", mul, "A")
MEL.connect_material_expressions(tint, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

emi = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 400)
emi.set_editor_property("parameter_name", "Emissive")
emi.set_editor_property("default_value", 0.0)
emul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 300)
MEL.connect_material_expressions(mul, "", emul, "A")
MEL.connect_material_expressions(emi, "", emul, "B")
MEL.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 450)
rough.set_editor_property("r", 0.9)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)

# 3. Instances: one per Blender material, same names as the mesh slots.
SLOTS = {
    "Canvas": ("T_Nemesis_Canvas", (1, 1, 1), 0),
    "Iron": ("T_Nemesis_Iron", (1, 1, 1), 0),
    "Red": ("T_Nemesis_Red", (1, 1, 1), 0),
    "Wood": ("T_Nemesis_Wood", (1, 1, 1), 0),
    "PropWood": ("T_Nemesis_PropWood", (1, 1, 1), 0),
    "Brass": ("T_Nemesis_Brass", (1, 1, 1), 0),
    "Plates": ("T_Nemesis_Plates", (1, 1, 1), 0),  # airship rocket hull (2026-11-15)
    "Copper": ("T_Nemesis_Copper", (1, 1, 1), 0),  # airship rocket nose, pipes
    "FuelPaint": ("T_Nemesis_FuelPaint", (1, 1, 1), 0),  # tower cannon fuel tank
    "Hazard": ("T_Nemesis_Hazard", (1, 1, 1), 0),  # hazard band on the fuel tank
    "Glow": ("T_Nemesis_Glow", (1, 1, 1), 4),
    "Dark": (None, (0.02, 0.018, 0.015), 0),
}
for name, (texname, color, emissive) in SLOTS.items():
    path = "%s/MI_Nemesis_%s" % (FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
        if not isinstance(mi, unreal.MaterialInstanceConstant):
            unreal.log_warning("%s is not a material instance, skipped" % path)
            continue
    else:
        mi = tools.create_asset("MI_Nemesis_" + name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, mat)
    MEL.clear_all_material_instance_parameters(mi)
    if texname:
        MEL.set_material_instance_texture_parameter_value(mi, "BaseColor", unreal.load_asset("%s/Textures/%s" % (FOLDER, texname)))
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(color[0], color[1], color[2], 1))
    MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", emissive)
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)

for sm in ("SM_Nemesis_Airship", "SM_Nemesis_Turret", "SM_Nemesis_Prop"):
    unreal.EditorAssetLibrary.save_asset("%s/%s" % (FOLDER, sm))
unreal.log("NEMESIS_MATERIALS_OK")
