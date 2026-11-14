"""Create a separate generation sandbox from the working balloon flight map."""
import unreal

SOURCE = '/Game/Scenes/BalloonFlight_Test'
TARGET = '/Game/Scenes/LevelGeneration_Test'

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(SOURCE)
assert not unreal.EditorAssetLibrary.does_asset_exist(TARGET), 'Target already exists; do not overwrite edits.'
assert unreal.EditorLoadingAndSavingUtils.save_map(
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), TARGET)
assert levels.load_level(TARGET)
blueprint_path = '/Game/Blueprints/Tests/BP_LevelGenerationBalloon'
assert not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path)
bp = unreal.EditorAssetLibrary.duplicate_asset('/Game/Assets/Balloon/BP_Balloon', blueprint_path)
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
    template = lib.get_object(lib.get_data(handle))
    if isinstance(template, unreal.BalloonKeyboardControlComponent):
        template.set_editor_property('enabled', True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
old = next(a for a in actors.get_all_level_actors() if a.get_component_by_class(unreal.BalloonFlightComponent))
balloon = actors.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(blueprint_path),
                                       old.get_actor_location(), old.get_actor_rotation())
balloon.set_actor_label('LevelGeneration_Balloon')
for child in old.get_attached_actors():
    child.attach_to_actor(balloon, '', unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
actors.destroy_actor(old)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith(('Obstacle_', 'Landmark_')):
        actors.destroy_actor(actor)
        continue
    if actor.get_actor_label() == 'Ground':
        # Test fixture dimensions only; not biome design parameters.
        actor.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    for component in actor.get_components_by_class(unreal.BalloonKeyboardControlComponent):
        component.set_editor_property('enabled', True)
    for component in actor.get_components_by_class(unreal.BalloonFlightComponent):
        component.set_editor_property('debug_log', False)
        component.set_editor_property('auto_test', False)
    if isinstance(actor, unreal.FirstPersonCameraPawn):
        actor.set_editor_property('auto_possess_player', unreal.AutoReceiveInput.PLAYER0)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.FirstPersonCameraGameMode)
assert levels.save_current_level()
assert levels.load_level(TARGET)
balloons = [a for a in actors.get_all_level_actors()
            if a.get_component_by_class(unreal.BalloonFlightComponent)]
pawns = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.FirstPersonCameraPawn)]
assert len(balloons) == len(pawns) == 1
assert pawns[0].get_attach_parent_actor() == balloons[0]
assert pawns[0].get_editor_property('auto_possess_player') == unreal.AutoReceiveInput.PLAYER0
assert balloons[0].get_component_by_class(unreal.BalloonKeyboardControlComponent).get_editor_property('enabled')
unreal.log('LEVEL_GENERATION_SETUP_PASS: saved map, keyboard enabled, attached auto-possessed pawn')
