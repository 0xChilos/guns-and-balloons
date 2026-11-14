"""Run in the editor; verify sandbox possession and unobstructed flight in PIE."""
import time
import traceback
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Scenes/LevelGeneration_Test')
state = {'start': time.monotonic()}


def tick(_delta):
    try:
        assert time.monotonic() - state['start'] < 60, 'PIE timed out'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        if 'balloon' not in state:
            balloon = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                           if a.get_component_by_class(unreal.BalloonFlightComponent))
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            assert pawn and pawn.get_attach_parent_actor() == balloon
            assert balloon.get_component_by_class(unreal.BalloonKeyboardControlComponent).get_editor_property('enabled')
            state.update(balloon=balloon, location=balloon.get_actor_location(),
                         game_time=unreal.GameplayStatics.get_time_seconds(world))
        elif unreal.GameplayStatics.get_time_seconds(world) - state['game_time'] > 3:
            distance = (state['balloon'].get_actor_location() - state['location']).length()
            assert distance > 100, 'Balloon did not move: ' + str(distance)
            unreal.log('LEVEL_GENERATION_PIE_PASS: possessed attached pawn; keyboard enabled; travel_cm=' + str(distance))
            finish()
    except Exception:
        unreal.log_error('LEVEL_GENERATION_PIE_FAILED ' + traceback.format_exc())
        finish()


def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    if unreal.SystemLibrary.get_command_line().find('SandboxTestExit') >= 0:
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
