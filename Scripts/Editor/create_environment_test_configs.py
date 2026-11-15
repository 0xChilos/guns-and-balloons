"""Create eight selectable M7 environment fixtures from the existing preview config.

Run in the editor: py G:/UE/gunsandballoons/Scripts/Editor/create_environment_test_configs.py
Geometry remains test tuning inherited from the preview fixture. These fixtures
exercise ground, fog and lighting; scenery/hazard catalogs are intentionally off.
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
if world.get_path_name() != '/Game/Scenes/MainLevel.MainLevel':
    assert levels.load_level('/Game/Scenes/MainLevel')
generators = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.BalloonLevelGenerator)]
assert len(generators) == 1
generator = generators[0]
source = unreal.load_asset('/Game/Blueprints/Tests/DA_LevelGenerationPreview')
assert source, 'Missing source preview fixture'
configs = []
for biome in ('Forest', 'Field', 'City', 'Desert'):
    for label, preset in (('Day', unreal.BalloonLightingPreset.DAY),
                          ('Night', unreal.BalloonLightingPreset.NIGHT)):
        path = f'/Game/Blueprints/Tests/Environment/DA_Environment_{biome}_{label}'
        config = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else (
            unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), path))
        assert isinstance(config, unreal.BalloonLevelConfig), path
        config.set_editor_property('biome_id', biome)
        config.set_editor_property('preset', preset)
        # Prototype terrain tuning: city/field stay level; forest/desert have relief.
        config.set_editor_property('terrain_mode', unreal.BalloonTerrainMode.GENERATED
                                   if biome in ('Forest', 'Desert') else unreal.BalloonTerrainMode.FLAT)
        config.set_editor_property('terrain_height_variation_meters', 15.0)
        for flag in ('scenery_enabled', 'obstacles_enabled', 'enemies_enabled'):
            config.set_editor_property(flag, False)
        assert unreal.EditorAssetLibrary.save_loaded_asset(config)
        configs.append(config)

# Expose all eight fixtures directly in the map's picker/list.
generator.set_editor_property('random_biome_configs', configs)
generator.set_editor_property('biome_selection', unreal.BalloonBiomeSelection.SELECTED)
generator.set_editor_property('randomize_seed', False)
generator.set_editor_property('generation_mode', unreal.BalloonGenerationMode.ON_LEVEL_START)
generator.set_editor_property('level_config', configs[0])
generator.setup_level_controller()
assert levels.save_current_level()
actors.set_selected_level_actors([generator])
unreal.log('M7_CONFIGS_PASS: saved 8 biome/preset configs; selected Forest Day; generator selected in Outliner')
