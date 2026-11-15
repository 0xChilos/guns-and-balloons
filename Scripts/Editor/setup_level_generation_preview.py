"""Run with: py Scripts/Editor/setup_level_generation_preview.py

Requires a project build containing BalloonLevelGenerator. Uses the existing
BalloonFlight_Test-derived sandbox and its copied original BP_Balloon actor.
Fixture values below are for debugging only, not biome design defaults.
"""
import unreal

assert hasattr(unreal, 'BalloonLevelGenerator'), (
    'Rebuild the project first: the loaded editor has no BalloonLevelGenerator. '
    'Resolve the merge markers in FirstPersonCameraPawn.h before building.')

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Scenes/MainLevel')
balloons = [a for a in actors.get_all_level_actors()
            if a.get_component_by_class(unreal.BalloonFlightComponent)]
assert len(balloons) == 1, 'Expected the single BalloonFlight_Test-derived balloon'
balloon = balloons[0]
# Move attached riders with the balloon; ground must clear its complete envelope.
location = balloon.get_actor_location()
location.z = 10000.0
balloon.set_actor_location(location, False, False)
balloon.set_actor_rotation(unreal.Rotator(), False)
keyboard = balloon.get_component_by_class(unreal.BalloonKeyboardControlComponent)
assert keyboard

config_path = '/Game/Blueprints/Tests/DA_LevelGenerationPreview'
config = unreal.load_asset(config_path)
if config is None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.BalloonLevelConfig)
    config = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'DA_LevelGenerationPreview', '/Game/Blueprints/Tests',
        unreal.BalloonLevelConfig, factory)
assert config
for name, value in dict(seed=12345, length_meters=500.0,
                        base_width_meters=200.0, segment_target_meters=100.0,
                        flight_height_meters=location.z / 100.0,
                        max_heading_degrees=0.0, scenery_enabled=False,
                        obstacles_enabled=False, enemies_enabled=False,
                        borders_enabled=True, border_exact_count=8,
                        start_transform=unreal.Transform(location=location)).items():
    config.set_editor_property(name, value)
unreal.EditorAssetLibrary.save_loaded_asset(config)

generators = [a for a in actors.get_all_level_actors()
              if isinstance(a, unreal.BalloonLevelGenerator)]
assert len(generators) <= 1, 'Multiple generators: choose one before setup'
generator = generators[0] if generators else actors.spawn_actor_from_class(
    unreal.BalloonLevelGenerator, location)
generator.set_actor_label('LevelGeneration_Preview')
generator.set_editor_property('level_config', config)
generator.set_editor_property('route_pawn', balloon)
assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
generator.draw_border_debug(600.0)
actors.set_selected_level_actors([generator])
assert levels.save_current_level()
unreal.log('LEVEL_GENERATION_PREVIEW_PASS: Ready; debug visible for 600 seconds')
for diagnostic in generator.get_diagnostics():
    unreal.log(diagnostic)
