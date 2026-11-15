"""Check current balloon collision on placed instances without saving any maps."""
import math
import unreal

cls = unreal.load_class(None, '/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
apothem = 97.5 * math.cos(math.pi / 8)
checked = 0
for path in ('/Game/Scenes/BalloonFlight_Test', '/Game/Scenes/Movement_Scene1', '/Game/PostProcess'):
    assert levels.load_level(path), path
    for actor in actors.get_all_level_actors():
        if actor.get_class() != cls:
            continue
        shapes = {c.get_name(): c for c in actor.get_components_by_class(unreal.ShapeComponent)}
        for prefix, count in (('Wall_', 8), ('Rim_', 8), ('Floor_', 4), ('Envelope_', 6)):
            assert len([n for n in shapes if n.startswith(prefix)]) == count, (path, prefix)
        assert shapes['BalloonCollision'].get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        for name, c in shapes.items():
            p = c.get_editor_property('relative_location')
            if name.startswith('Wall_'):
                assert abs(math.hypot(p.x, p.y) - (apothem - 3.5)) < .1, (path, name, p)
            if name.startswith('Floor_'):
                assert abs(c.get_editor_property('box_extent').x - (apothem - 7)) < .1, (path, name)
            if name.startswith(('Wall_', 'Rim_', 'Floor_')) or name in ('StoveBlock', 'ConsoleBlock'):
                assert 'BasketDamage' in [str(t) for t in c.get_editor_property('component_tags')], (path, name)
                assert c.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS, (path, name)
        mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        for name, bone, tolerance in (('StoveBlock', 'stove', .1), ('ConsoleBlock', 'display', 1)):
            bone_y = mesh.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation.y
            assert abs(shapes[name].get_editor_property('relative_location').y - bone_y) < tolerance, (path, name)
        checked += 1
        unreal.log('BALLOON_COLLISION_INSTANCE_OK ' + actor.get_path_name())
assert checked > 0
unreal.log('BALLOON_COLLISION_MAPS_OK instances={}'.format(checked))
