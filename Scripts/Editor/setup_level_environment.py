"""Bind M7 environment in the generation sandbox; verify all authored presets.

Run with UnrealEditor-Cmd -run=pythonscript -script=<absolute path>.
Does not modify the source generation config or post process.
"""
import json
import os
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world.get_path_name() != '/Game/Scenes/LevelGeneration_Test.LevelGeneration_Test':
    assert levels.load_level('/Game/Scenes/LevelGeneration_Test')
all_actors = actors.get_all_level_actors()


def single(cls):
    found = [a for a in all_actors if isinstance(a, cls)]
    assert len(found) == 1, f'Expected one {cls}: {len(found)}'
    return found[0]


generator = single(unreal.BalloonLevelGenerator)
generator.set_editor_property('generation_mode', unreal.BalloonGenerationMode.ON_LEVEL_START)
ground = [a for a in all_actors if a.get_actor_label() == 'Ground']
assert len(ground) == 1
sun = single(unreal.DirectionalLight)
sky = single(unreal.SkyLight)
controllers = [a for a in all_actors if isinstance(a, unreal.BalloonEnvironmentPresetController)]
assert len(controllers) <= 1
controller = controllers[0] if controllers else actors.spawn_actor_from_class(
    unreal.BalloonEnvironmentPresetController, unreal.Vector())
controller.set_actor_label('LevelGeneration_Environment')
fogs = [a for a in all_actors if isinstance(a, unreal.ExponentialHeightFog)]
assert len(fogs) <= 1
fog = fogs[0] if fogs else actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector())
fog.set_actor_label('LevelGeneration_Fog')
for name, value in dict(generator=generator, ground=ground[0], sun=sun, sky=sky,
                        fog=fog, apply_on_begin_play=True).items():
    controller.set_editor_property(name, value)
sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
sun_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
mesh = ground[0].get_component_by_class(unreal.StaticMeshComponent)
config = generator.get_editor_property('level_config')
assert config
# Cover a route of any heading from its start, including its corridor width.
extent = (config.get_editor_property('length_meters') + config.get_editor_property('base_width_meters')) * 100
origin, bounds = mesh.get_local_bounds()
size = bounds - origin
scale = ground[0].get_actor_scale3d()
scale.x = max(scale.x, extent * 2 / size.x)
scale.y = max(scale.y, extent * 2 / size.y)
ground[0].set_actor_scale3d(scale)
location = ground[0].get_actor_location()
start = config.get_editor_property('start_transform').translation
location.x, location.y = start.x, start.y
ground[0].set_actor_location(location, False, False)
fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
expected = {'Forest': ((.35, .45, .45), .03), 'Field': ((.75, .70, .55), .015),
            'City': ((.40, .35, .28), .05), 'Desert': ((.85, .75, .60), .02)}
results = []


def close(actual, target):
    assert abs(actual - target) < .0001, (actual, target)


for biome, (color, density) in expected.items():
    for label, preset in [('Day', unreal.BalloonLightingPreset.DAY), ('Night', unreal.BalloonLightingPreset.NIGHT)]:
        assert controller.apply_environment(biome, preset), controller.get_diagnostics()
        assert mesh.get_material(0).get_name() == f'MI_{biome}_Ground'
        close(fog_component.get_editor_property('fog_density'), density)
        actual_color = fog_component.get_editor_property('fog_inscattering_luminance')
        for channel, value in zip(('r', 'g', 'b'), color):
            close(getattr(actual_color, channel), value)
        values = controller.get_editor_property(label.lower())
        close(sun_component.get_editor_property('intensity'), values.sun_intensity)
        close(sky_component.get_editor_property('intensity'), values.sky_intensity)
        for channel in ('pitch', 'yaw', 'roll'):
            delta = (getattr(sun.get_actor_rotation(), channel) - getattr(values.sun_rotation, channel) + 180) % 360 - 180
            close(delta, 0)
        results.append(dict(biome=biome, preset=label, material=mesh.get_material(0).get_path_name(),
                            density=density, diagnostics=list(controller.get_diagnostics())))

# Preview the selected config without generating scenery or changing that asset.
config = generator.get_editor_property('level_config')
assert config
assert controller.apply_environment(config.get_editor_property('biome_id'), config.get_editor_property('preset'))
assert levels.save_current_level()
output = os.path.join(unreal.Paths.project_saved_dir(), 'M7-environment-checks.json')
with open(output, 'w', encoding='utf-8') as stream:
    json.dump(results, stream, indent=2)
unreal.log('M7_ENVIRONMENT_PASS: saved controller, ground, fog and movable lights; 8 preset combinations verified')


