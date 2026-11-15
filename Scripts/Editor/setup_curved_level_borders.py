"""Configure curved procedural borders; migrate away from saved edge markers.

Run in the editor: py G:/UE/gunsandballoons/Scripts/Editor/setup_curved_level_borders.py
Requires the current native build. Values are preview tuning, except 600 m width.
"""
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before configuring generation'
assert editor.get_editor_world().get_path_name() == (
    '/Game/Scenes/MainLevel.MainLevel'), 'Open MainLevel first'
generators = [a for a in actors.get_all_level_actors()
              if isinstance(a, unreal.BalloonLevelGenerator)]
assert len(generators) == 1
generator = generators[0]
border_class = unreal.load_class(
    None, '/Game/Assets/Map_edge/Blueprints/BP_RocketBalloon.BP_RocketBalloon_C')
assert border_class
proxy_class = unreal.load_class(None, '/Script/gunsandballoons.BalloonBorderThreat')

selected = generator.get_editor_property('level_config')
configs = {c.get_path_name(): c for c in
           [selected] + list(generator.get_editor_property('random_biome_configs')) if c}
source = unreal.load_asset('/Game/Blueprints/Tests/DA_LevelGenerationPreview')
if source:
    configs[source.get_path_name()] = source
for config in configs.values():
    for name, value in dict(base_width_meters=600.0, max_heading_degrees=25.0,
                            minimum_turning_radius_meters=100.0,
                            flight_height_meters=100.0,
                            borders_enabled=True, border_exact_count=8,
                            border_min_spacing_meters=80.0,
                            border_allow_overlap=False,
                            BorderActorClass=border_class).items():
        config.set_editor_property(name, value)

generator.set_editor_property('flight_height_meters', 100.0)
generator.set_editor_property('randomize_seed', True)
generator.set_editor_property('generation_mode', unreal.BalloonGenerationMode.ON_LEVEL_START)
old_borders = [a for a in actors.get_all_level_actors()
               if a.actor_has_tag('LevelGeneration.AuthoredBorder')]
for actor in old_borders:
    actor.set_actor_enable_collision(False)
try:
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
    assert len(generator.get_border_influences()) == 8
    generated = [a for a in unreal.GameplayStatics.get_all_actors_of_class(
                 editor.get_editor_world(), unreal.Actor)
                 if a.actor_has_tag('BalloonLevel.Generated') and a.get_owner() == generator]
    assert len([a for a in generated if a.get_class() == proxy_class]) == 8
    assert len([a for a in generated if a.get_class() == border_class]) == 8
except Exception:
    for actor in old_borders:
        actor.set_actor_enable_collision(True)
    raise
for actor in old_borders:
    actors.destroy_actor(actor)
for config in configs.values():
    assert unreal.EditorAssetLibrary.save_loaded_asset(config)
assert levels.save_current_level()
actors.set_selected_level_actors([generator])
unreal.log('CURVED_LEVEL_BORDERS_PASS: randomized seed; 8 generated rocket balloons '
           'and proxies; old authored borders removed; settings saved')
