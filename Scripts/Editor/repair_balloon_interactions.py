"""Restore missing stove authoring and current lever axes without resetting tuning."""
import runpy
import unreal

root = unreal.Paths.project_dir() + 'Scripts/Editor/'
setup = runpy.run_path(root + 'configure_stove.py')
# Fuel assets already contain the approved values; preserve their hand art/tuning.
bp = setup['configure_stove_blueprint']()
setup['configure_animation']()
runpy.run_path(root + 'configure_balloon_health.py')['main']()
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    comp = lib.get_object(lib.get_data(handle))
    if isinstance(comp, unreal.StoveComponent):
        comp.modify()
        comp.set_editor_property('door_open_angle', -100.0)
    if isinstance(comp, unreal.BalloonWingLeverComponent):
        comp.modify()
        comp.set_editor_property('lever_axis', unreal.Vector(1, 0, 0))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.save_current_level()
for path in ['/Game/Scenes/BalloonFlight_Test', '/Game/Scenes/MainLevel']:
    assert levels.load_level(path)
    for actor in actors.get_all_level_actors():
        if actor.get_class() != bp.generated_class():
            continue
        assert actor.get_component_by_class(unreal.StoveComponent)
        assert actor.get_component_by_class(unreal.FuelSupplyComponent)
    assert levels.save_current_level()
unreal.log('BALLOON_INTERACTIONS_REPAIRED')
