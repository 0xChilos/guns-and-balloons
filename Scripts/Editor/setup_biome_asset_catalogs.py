"""Import native shared/biome catalogs and enable preview scenery and mesh obstacles.

Run: py G:/UE/gunsandballoons/Scripts/Editor/setup_biome_asset_catalogs.py
Sources: Content/BalloonLevel/Rules/BalloonAssets_*.json. Legacy tables stay intact.
Density, spacing and street dimensions are prototype tuning, not final design.
"""
import json
import os
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before configuring catalogs'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = editor.get_editor_world()
assert world and world.get_path_name() == '/Game/Scenes/MainLevel.MainLevel'
generator_class = unreal.load_class(None, '/Script/gunsandballoons.BalloonLevelGenerator')
generators = unreal.GameplayStatics.get_all_actors_of_class(world, generator_class)
assert len(generators) == 1
generator = generators[0]
tables = {}
rows_by_biome = {}
tools = unreal.AssetToolsHelpers.get_asset_tools()
root = os.path.join(unreal.Paths.project_content_dir(), 'BalloonLevel', 'Rules')
for biome in ('Shared', 'Forest', 'Field', 'City', 'Desert'):
    with open(os.path.join(root, f'BalloonAssets_{biome}.json'), encoding='utf-8') as stream:
        rows = json.load(stream)
    rows_by_biome[biome] = rows
    name = f'DT_BalloonAssets_{biome}'
    table = unreal.load_asset(f'/Game/BalloonLevel/Rules/{name}')
    if not table:
        factory = unreal.DataTableFactory()
        factory.set_editor_property('struct', unreal.BalloonLevelAssetRow.static_struct())
        table = tools.create_asset(name, '/Game/BalloonLevel/Rules', unreal.DataTable, factory)
    assert table
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
    assert unreal.EditorAssetLibrary.save_loaded_asset(table)
    tables[biome] = table

configs = [unreal.load_asset('/Game/Blueprints/Tests/DA_LevelGenerationPreview')]
configs += [unreal.load_asset(f'/Game/Blueprints/Tests/Environment/DA_Environment_{b}_{p}')
            for b in ('Forest', 'Field', 'City', 'Desert') for p in ('Day', 'Night')]
for config in configs:
    assert config
    biome = str(config.get_editor_property('biome_id'))
    config.set_editor_property('shared_asset_table', tables['Shared'])
    config.set_editor_property('biome_asset_table', tables[biome])
    config.set_editor_property('scenery_enabled', True)
    config.set_editor_property('obstacles_enabled', True)
    config.set_editor_property('catalog_version', 'native-biome-assets-v1')
    # Smaller grid supplies enough eligible lots for the short curved preview.
    config.set_editor_property('street_cell_meters', 60.0)
    config.set_editor_property('street_road_meters', 12.0)
    config.set_editor_property('street_lot_depth_meters', 24.0)
    metadata = dict(config.get_editor_property('placement_metadata'))
    for row in rows_by_biome['Shared'] + rows_by_biome[biome]:
        meta = unreal.BalloonPlacementMetadata()
        name = row['Name']
        meta.set_editor_property('street_lamp', name == 'SM_Lamppost')
        cull = 0 if row['Rule'] == 'ring' or row['Collision'] else (
            30000 if name.startswith('SM_Pine_Small') or name.startswith('SM_Tree_') else
            80000 if row['Rule'] in ('street', 'wind_aligned') else 15000)
        meta.set_editor_property('cull_end_cm', cull)
        metadata[unreal.Name(name)] = meta
    config.set_editor_property('placement_metadata', metadata)
    assert unreal.EditorAssetLibrary.save_loaded_asset(config)

generator.set_editor_property('generation_mode', unreal.BalloonGenerationMode.ON_LEVEL_START)
assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
assert levels.save_current_level()
unreal.log('BIOME_ASSET_CATALOGS_PASS: five native catalogs; shared plus selected biome; '
           'scenery and mesh obstacles enabled in all nine configs; generated and saved')
