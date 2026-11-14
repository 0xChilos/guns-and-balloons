"""Headless phase 5 regression of existing interaction and firing paths; saves no assets."""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
probes = []
base = unreal.Vector(100000, 100000, 100000)

def spawn(cls, offset):
    actor = actors.spawn_actor_from_class(cls, base + offset, unreal.Rotator())
    probes.append(actor)
    return actor

try:
    pawn = spawn(unreal.FirstPersonCameraPawn, unreal.Vector())
    pawn.set_editor_property('hand_state', unreal.PlayerHandState.ARMED)
    crate = spawn(unreal.AmmoCrate, unreal.Vector(200, 0, 0))
    body = crate.get_editor_property('closed_body')
    body.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    body.set_collision_profile_name('BlockAll')
    assert pawn.try_interact(), 'Ammo crate click must be consumed'
    assert crate.is_open(), 'Ammo crate must open'
    revolver = pawn.get_component_by_class(unreal.RevolverComponent)
    assert revolver.get_queued_rounds() == 1, 'Ammo crate must queue one round'
    print('PHASE5_PASS ammo crate opens and queues a round')

    pawn.set_actor_location(base + unreal.Vector(0, 1000, 0), False, True)
    swap = spawn(unreal.InteractSwap, unreal.Vector(200, 1000, 0))
    before = spawn(unreal.StaticMeshActor, unreal.Vector(200, 1000, 0))
    after = spawn(unreal.StaticMeshActor, unreal.Vector(200, 1500, 0))
    for actor in [before, after]:
        actor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        actor.static_mesh_component.set_collision_profile_name('BlockAll')
    swap.set_editor_property('before_actors', [before])
    swap.set_editor_property('after_actors', [after])
    before.set_owner(swap)  # BeginPlay normally establishes this owner chain.
    assert pawn.try_interact(), 'Owned mesh must resolve to InteractSwap'
    assert swap.has_swapped(), 'InteractSwap must activate'
    print('PHASE5_PASS InteractSwap owner fallback')

    pawn.set_actor_location(base + unreal.Vector(0, 2000, 0), False, True)
    assert not pawn.try_interact(), 'Empty aim must fall through to firing'
    # Fresh revolver avoids the ammo test reload state.
    armed = spawn(unreal.FirstPersonCameraPawn, unreal.Vector(0, 3000, 0))
    gun = armed.get_component_by_class(unreal.RevolverComponent)
    gun.pull_trigger()
    assert gun.get_state() == unreal.RevolverState.FIRING, 'Trigger must enter firing state'
    print('PHASE5_PASS firing fallback and trigger')
    print('PHASE5_INTERACTIONS_SUCCESS')
finally:
    for actor in reversed(probes):
        actors.destroy_actor(actor)
