"""Use the shared playable BP_Balloon in every project map containing a balloon."""
import json
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop PIE first'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save current map edits first'
shared_class = unreal.load_class(None, '/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C')
maps = sorted(str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_class(
    unreal.TopLevelAssetPath('/Script/Engine', 'World'), True) if str(a.package_name).startswith('/Game/'))
report = []
for path in maps:
    assert levels.load_level(path)
    all_actors = list(actors.get_all_level_actors())
    balloons = []
    for actor in all_actors:
        mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        asset = mesh.get_editor_property('skeletal_mesh_asset') if mesh else None
        if (actor.get_component_by_class(unreal.BalloonFlightComponent) or
                (asset and asset.get_path_name() == '/Game/Assets/Balloon/SK_Balloon.SK_Balloon')):
            balloons.append(actor)
    assert len(balloons) == 1, path + ': expected one balloon, found ' + str(len(balloons))
    old = balloons[0]
    if old.get_class() != shared_class:
        owned = [c.get_editor_property('child_actor') for c in old.get_components_by_class(unreal.ChildActorComponent)]
        riders = [a for a in old.get_attached_actors() if a not in owned]
        balloon = actors.spawn_actor_from_class(shared_class, old.get_actor_location(), old.get_actor_rotation())
        assert balloon
        balloon.set_actor_scale3d(old.get_actor_scale3d())
        balloon.set_actor_label('BP_Balloon')
        balloon.set_folder_path(old.get_folder_path())
        balloon.set_editor_property('tags', old.get_editor_property('tags'))
        # World-partition gameplay balloon must remain loaded with its player.
        balloon.set_editor_property('is_spatially_loaded', False)
        for rider in riders:
            if isinstance(rider, unreal.FirstPersonCameraPawn):
                assert actors.destroy_actor(rider)
            else:
                rider.attach_to_actor(balloon, '', unreal.AttachmentRule.KEEP_WORLD,
                    unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
        for actor in all_actors:
            if isinstance(actor, unreal.BalloonLevelGenerator) and actor.get_editor_property('route_pawn') == old:
                actor.set_editor_property('route_pawn', balloon)
        assert actors.destroy_actor(old)
    else:
        balloon = old
    children = [c.get_editor_property('child_actor') for c in balloon.get_components_by_class(unreal.ChildActorComponent)
                if c.get_name() == 'PlayerCamera']
    assert len(children) == 1 and isinstance(children[0], unreal.FirstPersonCameraPawn), path
    child = children[0]
    assert child.get_editor_property('auto_possess_player') == unreal.AutoReceiveInput.PLAYER0
    assert balloon.get_component_by_class(unreal.StoveComponent)
    for pawn in list(actors.get_all_level_actors()):
        if isinstance(pawn, unreal.FirstPersonCameraPawn) and pawn != child:
            assert actors.destroy_actor(pawn)
    editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode', unreal.FirstPersonCameraGameMode)
    assert levels.save_current_level()
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    report.append({'map': path, 'balloon_class': balloon.get_class().get_path_name(),
                   'player_class': child.get_class().get_path_name()})
    unreal.log('ALL_LEVELS_SHARED_BALLOON_MAP_PASS ' + path)
with open(unreal.Paths.project_saved_dir() + 'SharedBalloonAllLevels.json', 'w', encoding='utf-8') as output:
    json.dump(report, output, indent=2)
assert levels.load_level('/Game/Scenes/LevelGeneration_Test')
unreal.log('ALL_LEVELS_SHARED_BALLOON_PASS maps=' + str(len(report)))
