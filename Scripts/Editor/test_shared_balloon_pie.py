"""Verify shared balloon ownership and unique player possession in the open map."""
import time
import traceback
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
started = time.monotonic()

def tick(_dt):
    try:
        assert time.monotonic() - started < 30, 'PIE timed out'
        world = editor.get_game_world()
        if not world:
            return
        players = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FirstPersonCameraPawn)
        balloons = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                    if a.get_component_by_class(unreal.BalloonFlightComponent)]
        assert len(players) == len(balloons) == 1, (len(players), len(balloons))
        pawn, balloon = players[0], balloons[0]
        assert balloon.get_class().get_path_name() == '/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C'
        assert unreal.GameplayStatics.get_player_pawn(world, 0) == pawn
        assert pawn.get_attach_parent_actor() == balloon
        assert balloon.get_component_by_class(unreal.StoveComponent)
        unreal.log('SHARED_BALLOON_PIE_PASS ' + world.get_name())
        finish()
    except Exception:
        unreal.log_error('SHARED_BALLOON_PIE_FAILED ' + traceback.format_exc())
        finish()

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()

handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
