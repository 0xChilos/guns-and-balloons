"""Create /Game/Scenes/Enemies_Test: a copy of BalloonFlight_Test (balloon, player, hand items in front of
the player) with a 3 x 3 km desert ground 50 m below the balloon and an ANemesisWaveDirector, which brings
in every enemy one after another (see its Events). Run in the editor: py "<this file>". Re-running rebuilds it.
"""
import unreal

SRC = '/Game/Scenes/BalloonFlight_Test'
DST = '/Game/Scenes/Enemies_Test'
GROUND_Z = -5000.0     # cm; the balloon flies at about 0
GROUND_SIZE = 300000.0  # cm (3 km), the balloon covers ~0.5 km in the 3 min timeline

eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(SRC, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
if eal.does_asset_exist(DST):
    eal.delete_asset(DST)
eal.duplicate_asset(SRC, DST)
les.load_level(DST, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    if a.get_class().get_name().startswith('Nemesis') or a.get_actor_label() in ('EnemyGround',):
        actors.destroy_actor(a)

ground = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, GROUND_Z))
ground.set_actor_location(unreal.Vector(0, 0, GROUND_Z), False, True)  # spawn ignores the location in the editor
ground.set_actor_label('EnemyGround')
mc = ground.static_mesh_component
mc.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
mc.set_world_scale3d(unreal.Vector(GROUND_SIZE / 100.0, GROUND_SIZE / 100.0, 1.0))
mat = None
for p in ('/Game/Assets/Locations/Materials/MI_Desert_Ground', '/Game/Assets/Locations/Desert/MI_Desert_Ground'):
    if eal.does_asset_exist(p):
        mat = unreal.load_asset(p)
        break
if mat:
    mc.set_material(0, mat)
mc.set_collision_profile_name('BlockAll')

director = actors.spawn_actor_from_class(unreal.load_class(None, '/Script/gunsandballoons.NemesisWaveDirector'), unreal.Vector(0, 0, 0))
director.set_actor_location(unreal.Vector(0, 0, 0), False, True)
director.set_actor_label('WaveDirector')

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('ENEMIES_TEST created (ground material: %s)' % (mat.get_name() if mat else 'default'))
