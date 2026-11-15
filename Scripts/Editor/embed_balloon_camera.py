"""Own the player-camera Pawn through a ChildActorComponent in shared BP_Balloon."""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save map edits first'

path = '/Game/Assets/Balloon/BP_BalloonPlayer'
player_bp = unreal.load_asset(path)
if player_bp is None:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.FirstPersonCameraPawn)
    player_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'BP_BalloonPlayer', '/Game/Assets/Balloon', unreal.Blueprint, factory)
unreal.BlueprintEditorLibrary.compile_blueprint(player_bp)
defaults = unreal.get_default_object(player_bp.generated_class())
defaults.set_editor_property('auto_possess_player', unreal.AutoReceiveInput.PLAYER0)
unreal.BlueprintEditorLibrary.compile_blueprint(player_bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(player_bp)

bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
handles = sub.k2_gather_subobject_data_for_blueprint(bp)
by_name = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}
handle = by_name.get('PlayerCamera')
if handle is None:
    handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=by_name['BalloonCollision'], new_class=unreal.ChildActorComponent, blueprint_context=bp))
    assert not str(reason), reason
    assert sub.rename_subobject(handle, 'PlayerCamera')
component = lib.get_object(lib.get_data(handle))
component.set_child_actor_class(player_bp.generated_class())
# Existing BalloonFlight_Test seat, in balloon actor space.
component.set_editor_property('relative_location', unreal.Vector(30, 0, 160))
component.set_editor_property('relative_rotation', unreal.Rotator())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
assert levels.save_current_level()

for map_path in ['/Game/Scenes/BalloonFlight_Test', '/Game/Scenes/MainLevel']:
    assert levels.load_level(map_path)
    balloons = [a for a in actors.get_all_level_actors() if a.get_class() == bp.generated_class()]
    assert len(balloons) == 1
    balloon = balloons[0]
    child_components = balloon.get_components_by_class(unreal.ChildActorComponent)
    children = [c.get_editor_property('child_actor') for c in child_components
                if c.get_name() == 'PlayerCamera']
    assert len(children) == 1 and isinstance(children[0], unreal.FirstPersonCameraPawn)
    child = children[0]
    for pawn in list(actors.get_all_level_actors()):
        if isinstance(pawn, unreal.FirstPersonCameraPawn) and pawn != child:
            assert pawn.get_attach_parent_actor() == balloon, 'Unrelated player Pawn: ' + pawn.get_path_name()
            assert actors.destroy_actor(pawn)
    assert child.get_editor_property('auto_possess_player') == unreal.AutoReceiveInput.PLAYER0
    assert levels.save_current_level()
    unreal.log('EMBEDDED_CAMERA_MAP_PASS ' + map_path)
unreal.log('EMBEDDED_CAMERA_PASS: BP_Balloon owns PlayerCamera; separate level Pawns removed')
