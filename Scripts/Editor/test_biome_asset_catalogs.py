"""Nonvisual check: every biome/preset generates shared and biome mesh assets."""
import json
import os
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before checking all catalogs'
world = editor.get_editor_world()
if not world or world.get_path_name() != '/Game/Scenes/LevelGeneration_Test.LevelGeneration_Test':
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Scenes/LevelGeneration_Test')
    world = editor.get_editor_world()
cls = unreal.load_class(None, '/Script/gunsandballoons.BalloonLevelGenerator')
generator = unreal.GameplayStatics.get_all_actors_of_class(world, cls)[0]
original = {n: generator.get_editor_property(n) for n in
            ('LevelConfig', 'biome_selection', 'randomize_seed')}
shared = {f'/Game/Assets/Locations/Forest/SM_Rock_{suffix}.SM_Rock_{suffix}' for suffix in ('A', 'B')}
results = []
try:
    generator.set_editor_property('biome_selection', unreal.BalloonBiomeSelection.SELECTED)
    generator.set_editor_property('randomize_seed', False)
    for biome in ('Forest', 'Field', 'City', 'Desert'):
        for preset in ('Day', 'Night'):
            config = unreal.load_asset(f'/Game/Blueprints/Tests/Environment/DA_Environment_{biome}_{preset}')
            generator.set_editor_property('LevelConfig', config)
            assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
            groups = generator.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            meshes = {c.get_editor_property('static_mesh').get_path_name() for c in groups if c.get_instance_count()}
            assert shared <= meshes, f'Missing shared meshes: {biome}/{preset}'
            own = {m for m in meshes if f'/Locations/{biome}/' in m} - shared
            assert own, f'Missing biome scenery: {biome}/{preset}'
            assert all(m in shared or f'/Locations/{biome}/' in m for m in meshes), meshes
            obstacles = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
                         if a.get_owner() == generator and a.actor_has_tag('BalloonLevel.Generated')]
            assert biome == 'Field' or obstacles, f'Missing mesh obstacles: {biome}'
            results.append(dict(biome=biome, preset=preset, meshes=len(meshes),
                                instances=sum(c.get_instance_count() for c in groups), obstacles=len(obstacles)))
    unreal.log('BIOME_ASSETS_MATRIX_PASS: ' + json.dumps(results))
    with open(os.path.join(unreal.Paths.project_saved_dir(), 'BiomeAssetCatalogChecks.json'), 'w') as stream:
        json.dump(results, stream, indent=2)
finally:
    for name, value in original.items():
        generator.set_editor_property(name, value)
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
