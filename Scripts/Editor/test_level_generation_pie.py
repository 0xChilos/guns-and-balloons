"""Verify generation, possession and unobstructed flight in the preview sandbox."""
import time
import traceback
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not editor_world or editor_world.get_path_name() != '/Game/Scenes/MainLevel.MainLevel':
    assert levels.load_level('/Game/Scenes/MainLevel')
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
            generators = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BalloonLevelGenerator)
            assert len(generators) == 1, 'Run setup_level_generation_preview.py first'
            generator = generators[0]
            assert generator.get_editor_property('route_pawn') == balloon
            assert generator.get_editor_property('generation_mode') == unreal.BalloonGenerationMode.ON_LEVEL_START, (
                'Preview must generate automatically on BeginPlay')
            assert generator.get_generation_status() == unreal.BalloonLevelGenerationStatus.READY
            assert len(generator.get_border_influences()) == 8
            proxy_class = unreal.load_class(None, '/Script/gunsandballoons.BalloonBorderThreat')
            rocket_class = unreal.load_class(None,
                '/Game/Assets/Map_edge/Blueprints/BP_RocketBalloon.BP_RocketBalloon_C')
            proxies = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, proxy_class)
                       if a.get_owner() == generator]
            rockets = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, rocket_class)
                       if a.get_owner() == generator]
            assert len(proxies) == len(rockets) == 8, 'Expected eight generated border proxies and rocket balloons'
            groups = generator.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            meshes = {c.get_editor_property('static_mesh').get_path_name() for c in groups if c.get_instance_count()}
            assert '/Game/Assets/Locations/Forest/SM_Rock_A.SM_Rock_A' in meshes, 'Missing shared scenery at level start'
            biome = str(generator.get_editor_property('active_config').get_editor_property('biome_id'))
            assert any(f'/Locations/{biome}/' in path and 'SM_Rock_' not in path for path in meshes), 'Missing biome scenery at level start'
            generator.draw_border_debug(60.0)
            assert balloon.get_component_by_class(unreal.BalloonKeyboardControlComponent)
            state.update(balloon=balloon, location=balloon.get_actor_location(),
                         game_time=unreal.GameplayStatics.get_time_seconds(world))
        elif unreal.GameplayStatics.get_time_seconds(world) - state['game_time'] > 3:
            distance = (state['balloon'].get_actor_location() - state['location']).length()
            assert distance > 100, 'Balloon did not move: ' + str(distance)
            unreal.log('LEVEL_GENERATION_PIE_PASS: generation Ready; 8 borders; possessed attached pawn; original balloon controls; travel_cm=' + str(distance))
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
