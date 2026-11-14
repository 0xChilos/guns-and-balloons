"""Apply the model-fitting collision setup after compiling the C++ module."""
import unreal

PATH = '/Game/Assets/Balloon/BP_Balloon'
bp = unreal.load_asset(PATH)
# The C++ setup disables legacy solid approximations while keeping existing references valid.
assert unreal.BalloonCollisionSetupLibrary.add_balloon_collision_primitives(PATH)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    if actor.get_class() == unreal.load_class(None, PATH + '.BP_Balloon_C'):
        # Clear any placed-instance override left by the previous enclosing box.
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            if component.get_name().startswith('BalloonCollision'):
                component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        shapes = actor.get_components_by_class(unreal.ShapeComponent)
        envelope = [s for s in shapes if s.get_name().startswith('Envelope_')]
        assert len(envelope) == 6, len(envelope)
        assert len([s for s in shapes if s.get_name().startswith('Wall_')]) == 8
        print('BALLOON COLLISION:', actor.get_actor_label(), 'envelope spheres:', len(envelope))
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
