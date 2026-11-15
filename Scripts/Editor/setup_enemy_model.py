"""Bring an enemy model built in Blender into Unreal.

The models are built by Scripts/Blender/build_<model>.py and exported by
Content/Assets/Enemies/export_enemies.py to Content/Assets/Enemies/FBX/*.fbx.

For each model the script:
- imports its pixel textures (Content/Assets/Nemesis/Textures/*.png, nearest, no mips);
- makes the MI_Nemesis_* instances it needs (children of M_Nemesis, like the others);
- re-imports its meshes;
- points each mesh's slots at /Game/Assets/Nemesis/<slot name>.
Solid parts get complex-as-simple collision, so shots and the balloon hit the model itself.
It doesn't touch M_Nemesis.

Run in the editor: py Scripts/Editor/setup_enemy_model.py <Rocket|TowerGun> (no argument = all)."""
import os
import sys
import unreal

# model -> (textures it introduces, meshes, meshes that need per-poly collision)
MODELS = {
    "Rocket": (["Plates", "Copper"], ["SM_Rocket_Body", "SM_Rocket_Flame"], []),
    "TowerGun": (["FuelPaint", "Hazard"], ["SM_TowerGun_Mount", "SM_TowerGun_Barrel", "SM_TowerGun_FuelTank"],
                 ["SM_TowerGun_Mount", "SM_TowerGun_Barrel", "SM_TowerGun_FuelTank"]),
}

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
SHARED = "/Game/Assets/Nemesis"
DEST = "/Game/Assets/Enemies/FBX"


def import_files(files, dest):
    tasks = []
    for f, name in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest
        t.destination_name = name
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)


def setup(model):
    textures, meshes, complex_collision = MODELS[model]
    # 1. Textures: point filtering, no mips -> PS1 pixels.
    import_files([(os.path.join(PROJECT, "Content/Assets/Nemesis/Textures/T_Nemesis_%s.png" % n), "T_Nemesis_" + n)
                  for n in textures], SHARED + "/Textures")
    for n in textures:
        tex = unreal.load_asset("%s/Textures/T_Nemesis_%s" % (SHARED, n))
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
        eal.save_loaded_asset(tex)
    # 2. Material instances.
    parent = unreal.load_asset(SHARED + "/M_Nemesis")
    for n in textures:
        path = "%s/MI_Nemesis_%s" % (SHARED, n)
        mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
            "MI_Nemesis_" + n, SHARED, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, parent)
        MEL.set_material_instance_texture_parameter_value(mi, "BaseColor", unreal.load_asset("%s/Textures/T_Nemesis_%s" % (SHARED, n)))
        MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(1, 1, 1, 1))
        MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", 0.0)
        MEL.update_material_instance(mi)
        eal.save_loaded_asset(mi)
    # 3. Meshes: re-import, map slots to the shared instances, collision.
    import_files([(os.path.join(PROJECT, "Content/Assets/Enemies/FBX/%s.fbx" % n), n) for n in meshes], DEST)
    missing = []
    for n in meshes:
        sm = unreal.load_asset("%s/%s" % (DEST, n))
        for i, m in enumerate(sm.get_editor_property("static_materials")):
            slot = str(m.get_editor_property("material_slot_name"))
            if eal.does_asset_exist(SHARED + "/" + slot):
                sm.set_material(i, unreal.load_asset(SHARED + "/" + slot))
            else:
                missing.append("%s:%s" % (n, slot))
        if n in complex_collision:
            body = sm.get_editor_property("body_setup")
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(sm)
    # Duplicate materials/textures the importer made next to the meshes.
    for p in eal.list_assets(DEST, True, False):
        if not isinstance(unreal.load_asset(p), unreal.StaticMesh):
            eal.delete_asset(p)
    unreal.log("ENEMY_MODEL_OK %s missing_slots=%s" % (model, missing))


for model in (sys.argv[1:] or list(MODELS)):
    setup(model)
