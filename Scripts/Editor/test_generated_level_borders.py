"""Nonvisual checks for seeded border generation, replacement, rollback and Clear.

Run in the editor: py G:/UE/gunsandballoons/Scripts/Editor/test_generated_level_borders.py
Restores generator settings and leaves a newly generated preview. Does not save.
"""
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before running this editor check'
world = editor.get_editor_world()
if not world or world.get_path_name() != '/Game/Scenes/LevelGeneration_Test.LevelGeneration_Test':
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Scenes/LevelGeneration_Test')
    world = editor.get_editor_world()
generator_class = unreal.load_class(None, '/Script/gunsandballoons.BalloonLevelGenerator')
proxy_class = unreal.load_class(None, '/Script/gunsandballoons.BalloonBorderThreat')
rocket_class = unreal.load_class(
    None, '/Game/Assets/Map_edge/Blueprints/BP_RocketBalloon.BP_RocketBalloon_C')
generators = unreal.GameplayStatics.get_all_actors_of_class(world, generator_class)
assert len(generators) == 1
generator = generators[0]
original = {name: generator.get_editor_property(name) for name in
            ('level_config', 'biome_selection', 'randomize_seed')}
config = unreal.BalloonLevelConfig()
for name in ('length_meters', 'base_width_meters', 'min_width_meters',
             'max_heading_degrees', 'minimum_turning_radius_meters',
             'flight_height_meters', 'start_transform', 'borders_enabled',
             'border_exact_count', 'border_min_spacing_meters', 'border_allow_overlap'):
    config.set_editor_property(name, original['level_config'].get_editor_property(name))
config.set_editor_property('scenery_enabled', False)
config.set_editor_property('seed', 12345)
config.set_editor_property('BorderActorClass', rocket_class)


def generated(cls):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls)
            if a.get_owner() == generator and a.actor_has_tag('BalloonLevel.Generated')]


def snapshot():
    borders = generator.get_border_influences()
    proxies = generated(proxy_class)
    rockets = generated(rocket_class)
    assert len(borders) == len(proxies) == len(rockets) == 8
    for proxy in proxies:
        border = proxy.get_influence()
        rocket = next(a for a in rockets if a.get_attach_parent_actor() == proxy)
        assert (rocket.get_actor_location() - border.center).length() < 0.01
        assert abs(rocket.get_editor_property('AggroRangeM') - border.radius_cm / 100.) < 0.001
        assert rocket.get_editor_property('DriftRangeM') == 0.0
    return [(b.instance_id, b.center.x, b.center.y, b.center.z, b.radius_cm) for b in borders]


try:
    generator.set_editor_property('level_config', config)
    generator.set_editor_property('biome_selection', unreal.BalloonBiomeSelection.SELECTED)
    generator.set_editor_property('randomize_seed', False)
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
    first = snapshot()
    first_names = {a.get_path_name() for a in generated(proxy_class) + generated(rocket_class)}
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
    assert snapshot() == first, 'Fixed seed must reproduce borders'
    assert first_names.isdisjoint(a.get_path_name() for a in generated(proxy_class) + generated(rocket_class))

    generator.set_editor_property('randomize_seed', True)
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
    random_first = snapshot()
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
    random_second = snapshot()
    assert random_first != random_second, 'Random generation must change border layout'
    before_failure = {a.get_path_name() for a in generated(proxy_class) + generated(rocket_class)}
    config.set_editor_property('BorderActorClass', unreal.Actor)
    assert not generator.generate_level(), 'Incompatible threat class should fail transactionally'
    assert snapshot() == random_second
    assert before_failure == {a.get_path_name() for a in generated(proxy_class) + generated(rocket_class)}

    generator.clear_generated_level()
    assert not generated(proxy_class) and not generated(rocket_class), 'Clear must remove every generated border'
    assert not generator.get_border_influences()
    unreal.log('GENERATED_LEVEL_BORDERS_PASS: fixed seed; randomized layout; 8 proxies/rockets; '
               'range synchronization; replacement; failure rollback; Clear')
finally:
    for name, value in original.items():
        generator.set_editor_property(name, value)
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
