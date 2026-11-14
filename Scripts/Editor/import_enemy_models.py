"""Import the enemy model FBXs (Content/Assets/Enemies/FBX, exported by export_enemies.py) as static
meshes into /Game/Assets/Enemies/FBX (the editor usually auto-imports them there itself; this only
imports what is missing), point every material slot at the shared /Game/Assets/Nemesis/MI_Nemesis_*
instance of the same name, and delete the duplicate materials the importer creates.
Run in the editor: py Scripts/Editor/import_enemy_models.py"""
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Content/Assets/Enemies/FBX")
DEST = "/Game/Assets/Enemies/FBX"
SHARED = "/Game/Assets/Nemesis"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for f in sorted(os.listdir(SRC)):
    if f.endswith(".fbx") and not eal.does_asset_exist(DEST + "/" + f[:-4]):
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SRC, f)
        t.destination_path = DEST
        t.destination_name = f[:-4]
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
if tasks:
    tools.import_asset_tasks(tasks)
meshes = [a for a in (unreal.load_asset(p) for p in eal.list_assets(DEST, True, False)) if isinstance(a, unreal.StaticMesh)]

missing = set()
for sm in meshes:
    mats = sm.get_editor_property("static_materials")
    for i, m in enumerate(mats):
        slot = str(m.get_editor_property("material_slot_name"))
        shared = SHARED + "/" + slot
        if eal.does_asset_exist(shared):
            sm.set_material(i, unreal.load_asset(shared))
        else:
            missing.add(slot)
    eal.save_loaded_asset(sm)

# duplicates the importer made next to the meshes (materials / textures) are no longer referenced
removed = []
for p in eal.list_assets(DEST, True, False):
    a = unreal.load_asset(p)
    if not isinstance(a, unreal.StaticMesh):
        eal.delete_asset(p)
        removed.append(p)
unreal.log("ENEMY_IMPORT meshes=%s missing_materials=%s removed=%d" % ([m.get_name() for m in meshes], sorted(missing), len(removed)))
