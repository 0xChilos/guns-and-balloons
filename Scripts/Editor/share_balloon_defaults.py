"""Make existing test balloons inherit their component settings from BP_Balloon."""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before sharing balloon defaults'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save level edits first'
balloon_class = unreal.load_class(None, '/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C')

for path in ['/Game/Scenes/BalloonFlight_Test', '/Game/Scenes/LevelGeneration_Test']:
    assert levels.load_level(path)
    balloons = [a for a in actors.get_all_level_actors()
                if a.get_component_by_class(unreal.BalloonFlightComponent)]
    assert len(balloons) == 1 and balloons[0].get_class() == balloon_class
    old = balloons[0]
    owned_children = [c.get_editor_property('child_actor') for c in old.get_components_by_class(unreal.ChildActorComponent)]
    riders = [a for a in old.get_attached_actors() if a not in owned_children]
    balloon = actors.spawn_actor_from_class(balloon_class, old.get_actor_location(), old.get_actor_rotation())
    assert balloon
    balloon.set_actor_scale3d(old.get_actor_scale3d())
    balloon.set_actor_label(old.get_actor_label())
    balloon.set_folder_path(old.get_folder_path())
    balloon.set_editor_property('tags', old.get_editor_property('tags'))
    for rider in riders:
        rider.attach_to_actor(balloon, '', unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.BalloonLevelGenerator) and actor.get_editor_property('route_pawn') == old:
            actor.set_editor_property('route_pawn', balloon)
    assert actors.destroy_actor(old)
    assert levels.save_current_level()
    unreal.log('SHARED_BALLOON_DEFAULTS ' + path + ': fresh shared instance; riders and generator preserved')
unreal.log('SHARED_BALLOON_PASS: both maps inherit BP_Balloon component settings')
