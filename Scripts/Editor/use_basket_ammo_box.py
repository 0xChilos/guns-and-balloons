"""Reloading moves to the ammo box on the balloon's weapon wall (docs/features/weapons.md, manual reloading).

- BP_Balloon gets a UAmmoSupplyComponent (AmmoBox) limited to the box's bones, ammo_box and ammo_lid: RMB on the
  box hands one round to the gun in hand. The lid already opens on look-at (BP_Balloon, UpdateAmmoLookAt).
- The floor AAmmoCrate goes from the levels with a balloon. Hook_Loot_Grab has no balloon and keeps its crate.

Run in the editor: py "<this file>" [box|levels] (default both). Re-running is harmless.
"""
import os
import sys
import unreal

STEPS = set(sys.argv[1:]) or {'box', 'levels'}
BALLOON = '/Game/Assets/Balloon/BP_Balloon'
LEVELS = ['/Game/Scenes/BalloonFlight_Test', '/Game/Scenes/Enemies_Test', '/Game/Scenes/DeathScreen_Test',
          '/Game/Scenes/LevelGeneration_Test', '/Game/Scenes/Movement_Scene1']
NAME = 'AmmoBox'
BONES = ['ammo_box', 'ammo_lid']


def add_box():
    bp = unreal.load_asset(BALLOON)
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        obj = lib.get_associated_object(lib.get_data(h))
        if isinstance(obj, unreal.AmmoSupplyComponent):
            supply = obj
            break
    else:
        handle, _ = sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.AmmoSupplyComponent, blueprint_context=bp))
        sub.rename_subobject(handle, unreal.Text(NAME))
        supply = lib.get_associated_object(lib.get_data(handle))
    supply.set_editor_property('hit_bones', [unreal.Name(b) for b in BONES])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    unreal.log('BASKET_AMMO %s: %s on %s' % (BALLOON, supply.get_name(), BONES))


def remove_crates():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    for level in LEVELS:
        if not unreal.EditorAssetLibrary.does_asset_exist(level):
            continue
        les.load_level(level, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
        files = []
        try:  # world-partitioned levels: load the crates first, and drop their actor files after saving
            descs = [d for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
                     if d.native_class is not None and d.native_class.get_name() == 'AmmoCrate']
            unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
            files = [os.path.join(content_dir, str(d.actor_package)[len('/Game/'):] + '.uasset') for d in descs]
        except Exception:
            pass
        gone = 0
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.AmmoCrate):
                eas.destroy_actor(a)
                gone += 1
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        for f in files:
            if os.path.exists(f):
                os.remove(f)
        unreal.log('BASKET_AMMO %s: removed %d crates' % (level, gone))


if 'box' in STEPS:
    add_box()
if 'levels' in STEPS:
    remove_crates()
unreal.log('BASKET_AMMO done')
