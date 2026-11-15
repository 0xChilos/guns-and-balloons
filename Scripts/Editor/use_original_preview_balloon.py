"""Place the shared BP_Balloon class without copying level-specific overrides."""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Scenes/LevelGeneration_Test')
target_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
source_world = unreal.load_asset('/Game/Scenes/BalloonFlight_Test')
sources = [a for a in unreal.GameplayStatics.get_all_actors_of_class(source_world, unreal.Actor)
           if a.get_component_by_class(unreal.BalloonFlightComponent)]
existing = [a for a in actors.get_all_level_actors()
            if a.get_component_by_class(unreal.BalloonFlightComponent)]
assert len(sources) == len(existing) == 1
old = existing[0]
old_location = old.get_actor_location()
old_rotation = old.get_actor_rotation()
owned_children = [c.get_editor_property('child_actor') for c in old.get_components_by_class(unreal.ChildActorComponent)]
riders = [a for a in old.get_attached_actors() if a not in owned_children]
balloon = actors.spawn_actor_from_class(sources[0].get_class(), old_location, old_rotation)
assert balloon, 'Could not spawn the shared balloon'
assert balloon.get_class() == sources[0].get_class()
balloon.set_actor_label('LevelGeneration_Balloon')
balloon.set_actor_location(old_location, False, False)
balloon.set_actor_rotation(old_rotation, False)
for rider in riders:
    rider.attach_to_actor(balloon, '', unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
for generator in actors.get_all_level_actors():
    if isinstance(generator, unreal.BalloonLevelGenerator):
        generator.set_editor_property('route_pawn', balloon)
assert actors.destroy_actor(old)
assert levels.save_current_level()
unreal.log('ORIGINAL_BALLOON_COPY_PASS: ' + balloon.get_class().get_path_name())
exec(compile(open(unreal.Paths.project_dir() + 'Scripts/Editor/setup_level_generation_preview.py',
                  encoding='utf-8').read(), 'setup_level_generation_preview.py', 'exec'))
