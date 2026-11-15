"""Create /Game/Scenes/DeathScreen_Test: a copy of Enemies_Test (balloon, player, hand items, desert ground,
the enemy waves as a backdrop) with an ADeathScreenPreview that shows the death screen a few seconds
after Play (docs/features/ui-menus.md). RETURN TO CHECKPOINT restarts the level, which shows it again.
Run in the editor: py "<this file>". Re-running rebuilds it.
"""
import unreal

SRC = '/Game/Scenes/Enemies_Test'
DST = '/Game/Scenes/DeathScreen_Test'

eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(SRC, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
if eal.does_asset_exist(DST):
    eal.delete_asset(DST)
eal.duplicate_asset(SRC, DST)
les.load_level(DST, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    if a.get_class().get_name() == 'DeathScreenPreview':
        actors.destroy_actor(a)
preview = actors.spawn_actor_from_class(unreal.load_class(None, '/Script/gunsandballoons.DeathScreenPreview'), unreal.Vector(0, 0, 0))
preview.set_actor_location(unreal.Vector(0, 0, 0), False, True)
preview.set_actor_label('DeathScreenPreview')

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('DEATHSCREEN_TEST created')
