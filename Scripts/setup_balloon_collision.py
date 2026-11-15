"""Apply the model-fitting collision setup after compiling the C++ module."""
import unreal
import math

PATH = '/Game/Assets/Balloon/BP_Balloon'
bp = unreal.load_asset(PATH)
# The C++ setup disables legacy solid approximations while keeping existing references valid.
assert unreal.BalloonCollisionSetupLibrary.add_balloon_collision_primitives(PATH)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)

data_subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
data_lib = unreal.SubobjectDataBlueprintFunctionLibrary
templates = {}
for handle in data_subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = data_lib.get_data(handle)
    component = data_lib.get_object(data)
    if isinstance(component, unreal.ShapeComponent):
        templates[str(data_lib.get_variable_name(data))] = component

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    if actor.get_class() == unreal.load_class(None, PATH + '.BP_Balloon_C'):
        # Clear any placed-instance override left by the previous enclosing box.
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            if component.get_name().startswith('BalloonCollision'):
                component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            name = component.get_name()
            if name.startswith(('Floor_', 'Wall_', 'Rim_', 'Envelope_')) or name in ('StoveBlock', 'ConsoleBlock'):
                template = templates[name]
                component.modify()
                for prop in ('relative_location', 'relative_rotation'):
                    component.set_editor_property(prop, template.get_editor_property(prop))
                if isinstance(component, unreal.BoxComponent):
                    component.set_box_extent(template.get_editor_property('box_extent'))
                    tags = list(component.get_editor_property('component_tags'))
                    if 'BasketDamage' not in [str(t) for t in tags]:
                        component.set_editor_property('component_tags', tags + ['BasketDamage'])
                elif isinstance(component, unreal.SphereComponent):
                    component.set_sphere_radius(template.get_editor_property('sphere_radius'))
                component.set_collision_profile_name('BlockAllDynamic')
        shapes = actor.get_components_by_class(unreal.ShapeComponent)
        envelope = [s for s in shapes if s.get_name().startswith('Envelope_')]
        assert len(envelope) == 6, len(envelope)
        assert len([s for s in shapes if s.get_name().startswith('Wall_')]) == 8
        assert len([s for s in shapes if s.get_name().startswith('Rim_')]) == 8
        print('BALLOON COLLISION:', actor.get_actor_label(), 'envelope spheres:', len(envelope))
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
# A commandlet starts in a temporary map; only persist a real current level.
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# Compare against the current imported skeleton, rather than just counting shapes.
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
probe = subsystem.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, 10000), transient=True)
try:
    components = {c.get_name(): c for c in probe.get_components_by_class(unreal.PrimitiveComponent)}
    mesh = probe.get_component_by_class(unreal.SkeletalMeshComponent)
    stove = mesh.get_socket_transform('stove', unreal.RelativeTransformSpace.RTS_COMPONENT).translation
    console = mesh.get_socket_transform('display', unreal.RelativeTransformSpace.RTS_COMPONENT).translation
    assert abs(components['StoveBlock'].get_editor_property('relative_location').y - stove.y) < .1
    assert abs(components['ConsoleBlock'].get_editor_property('relative_location').y - console.y) < 1
    inner = 97.5 * math.cos(math.pi / 8) - 7
    assert abs(components['Floor_0'].get_editor_property('box_extent').x - inner) < .1
    assert abs(components['Wall_0'].get_editor_property('relative_location').x - (inner + 3.5)) < .1
    for name, component in components.items():
        if name.startswith(('Wall_', 'Floor_', 'Rim_')) or name in ('StoveBlock', 'ConsoleBlock'):
            assert 'BasketDamage' in [str(t) for t in component.get_editor_property('component_tags')], name
    assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    unreal.log('BALLOON_COLLISION_OK: basket dimensions, stove/display alignment and damage tags match current model')
finally:
    subsystem.destroy_actor(probe)
    if world.get_path_name().startswith('/Game/'):
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
