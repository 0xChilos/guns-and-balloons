"""PIE checks for the actual saved balloon: stove traces, fuel and lever drag."""
import time
import traceback
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
state = {'start': time.monotonic()}

def aim(pawn, target):
    origin = pawn.get_editor_property('camera').get_world_location()
    rotation = unreal.MathLibrary.find_look_at_rotation(origin, target)
    pawn.get_controller().set_control_rotation(rotation)
    return origin

def trace(pawn, origin, target):
    return unreal.SystemLibrary.line_trace_single(pawn, origin, target,
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, False)

def tick(_dt):
    try:
        assert time.monotonic() - state['start'] < 45
        world = editor.get_game_world()
        if not world:
            return
        if 'stove' not in state:
            balloon = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                           if a.get_component_by_class(unreal.BalloonFlightComponent))
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            stove = balloon.get_component_by_class(unreal.StoveComponent)
            mesh = balloon.get_component_by_class(unreal.SkeletalMeshComponent)
            assert stove and pawn.is_hand_empty()
            origin = pawn.get_editor_property('camera').get_world_location()
            supply = balloon.get_component_by_class(unreal.FuelSupplyComponent)
            assert supply and supply.get_count() > 0
            count = supply.get_count()
            aim(pawn, supply.get_editor_property('interact_shape').get_world_location())
            assert pawn.try_interact(), 'Fuel bucket click not consumed'
            assert not pawn.is_hand_empty() and supply.get_count() == count - 1, 'Fuel pickup failed'
            assert pawn.try_interact(), 'Fuel return click not consumed'
            assert pawn.is_hand_empty() and supply.get_count() == count, 'Fuel return failed'
            # Search across the animated door rather than assuming a point at its hinge.
            pivot = mesh.get_socket_location('furnace_door')
            door_target = None
            hits = set()
            for x in range(-20, 21, 5):
                for z in range(-15, 16, 5):
                    target = pivot + unreal.Vector(x, 0, z)
                    hit = trace(pawn, origin, target)
                    d = hit.to_dict() if hit else {}
                    hits.add((str(d.get('hit_bone_name')), str(d.get('hit_component'))))
                    if str(d.get('hit_bone_name')) == 'furnace_door':
                        door_target = target
                        break
                if door_target:
                    break
            assert door_target, 'Door occluded: ' + str(hits)
            aim(pawn, door_target)
            assert pawn.try_interact() and stove.is_door_open(), 'Door click failed'
            assert stove.add_fuel(unreal.load_asset('/Game/Data/Stove/DA_Fuel_Crystal'))
            assert stove.get_temperature() >= 50 and stove.get_burn_time_left() > 0
            assert stove.get_speed_multiplier() > 1
            supply = balloon.get_component_by_class(unreal.FuelSupplyComponent)
            assert supply and supply.get_count() > 0
            # Real bone hit supplies the grab location. Drag tangent must open the lever.
            for lever in balloon.get_components_by_class(unreal.BalloonWingLeverComponent):
                assert lever.get_editor_property('lever_axis') == unreal.Vector(1, 0, 0)
                bone = str(lever.get_editor_property('lever_bone'))
                pivot = mesh.get_socket_location(bone)
                found = None
                for y in range(-40, 41, 5):
                    for z in range(-20, 41, 5):
                        hit = trace(pawn, origin, pivot + unreal.Vector(0, y, z))
                        if hit and str(hit.to_dict().get('hit_bone_name')) == bone:
                            if found is None or ((hit.to_dict()['impact_point'] - pivot).length() >
                                                 (found.to_dict()['impact_point'] - pivot).length()):
                                found = hit
                assert found, 'No hit on ' + bone
                assert lever.can_grab(pawn, found), bone + ' cannot grab'
                lever.begin_grab(pawn, found)
                transform = mesh.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_WORLD)
                point = found.to_dict()['impact_point']
                axis = unreal.MathLibrary.transform_direction(transform, unreal.Vector(1, 0, 0))
                offset = point - transform.translation
                tangent = unreal.MathLibrary.cross_vector_vector(axis, offset).normal()
                if bone == 'lever_L':
                    aim(pawn, point)
                    tangent = unreal.MathLibrary.get_right_vector(pawn.get_controller().get_control_rotation())
                before = lever.get_lever_angle()
                for _ in range(30):
                    lever.update_grab(pawn, tangent * 100.0)
                assert lever.get_lever_angle() > before, bone + ' drag did not open: ' + str((before, lever.get_lever_angle(), tangent, offset))
                lever.end_grab(pawn)
            state.update(stove=stove, mesh=mesh, time=unreal.GameplayStatics.get_time_seconds(world))
        elif unreal.GameplayStatics.get_time_seconds(world) - state['time'] > 0.7:
            assert abs(state['stove'].get_door_angle() + 100.0) < 1
            anim = state['mesh'].get_anim_instance()
            assert abs(anim.get_editor_property('StoveDoorAngle') + 100.0) < 1
            unreal.log('BALLOON_INTERACTIONS_PIE_PASS: door trace/click, heat, burn time, speed, animation, both lever grabs/drags')
            finish()
    except Exception:
        unreal.log_error('BALLOON_INTERACTIONS_PIE_FAILED ' + traceback.format_exc())
        finish()

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()

handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
