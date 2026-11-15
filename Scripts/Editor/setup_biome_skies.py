"""Create eight tunable biome skies. Run in editor: py <path to this file>.

Uses the engine's panoramic cloud/star sky, preserving the PS1 post process.
All palette and cloud values below are prototype art tuning (TBD).
Reruns update only these material instances; no levels are loaded or saved.
"""
import unreal

DIRECTORY = '/Game/BalloonLevel/Skies'
PARENT = '/Engine/EngineSky/M_Sky_Panning_Clouds2'
# Zenith, horizon, clouds, sun/moon: linear colors, prototype tuning.
PALETTES = {
    'Forest': {
        'Day': ((.035, .18, .24), (.42, .68, .58), (.64, .78, .69), (1., .84, .48)),
        'Night': ((.003, .012, .025), (.045, .12, .13), (.018, .055, .065), (.55, .82, 1.)),
    },
    'Field': {
        'Day': ((.035, .22, .55), (.65, .79, .83), (.95, .91, .74), (1., .87, .58)),
        'Night': ((.008, .012, .055), (.12, .105, .23), (.065, .055, .13), (.85, .84, 1.)),
    },
    'City': {
        'Day': ((.075, .085, .12), (.55, .32, .18), (.22, .20, .19), (1., .42, .12)),
        'Night': ((.006, .008, .018), (.14, .065, .035), (.035, .032, .045), (.65, .73, .85)),
    },
    'Desert': {
        'Day': ((.13, .18, .33), (.95, .47, .16), (.73, .46, .28), (1., .68, .22)),
        'Night': ((.012, .006, .04), (.18, .075, .12), (.07, .035, .085), (.8, .72, 1.)),
    },
}

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
parent = assets.load_asset(PARENT)
assert parent, PARENT
vectors = {str(n) for n in library.get_vector_parameter_names(parent)}
scalars = {str(n) for n in library.get_scalar_parameter_names(parent)}
unreal.log('BIOME_SKY_PARAMETERS: vectors=%s scalars=%s' % (vectors, scalars))
tools = unreal.AssetToolsHelpers.get_asset_tools()
for biome, presets in PALETTES.items():
    for preset, colors in presets.items():
        name = 'MI_%s_%sSky' % (biome, preset)
        path = DIRECTORY + '/' + name
        material = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
            name, DIRECTORY, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        assert material, path
        library.set_material_instance_parent(material, parent)
        for parameter, color in zip(('Zenith Color', 'Horizon color', 'Cloud color', 'Sun color'), colors):
            assert parameter in vectors, parameter
            assert library.set_material_instance_vector_parameter_value(
                material, parameter, unreal.LinearColor(*color, 1.0))
        # Align the sky disc to the existing fixed Day/Night sun rotations.
        import math
        elevation = math.radians(50 if preset == 'Day' else 35)
        direction = unreal.LinearColor(-math.cos(elevation), 0., -math.sin(elevation), 1.)
        assert library.set_material_instance_vector_parameter_value(material, 'Light direction', direction)
        for parameter, value in {'Stars brightness': 1.25 if preset == 'Night' else 0.,
                                 'Sun brightness': 2.5 if preset == 'Night' else 10.,
                                 'Sun Radius': .0009 if preset == 'Night' else .002}.items():
            assert parameter in scalars, parameter
            assert library.set_material_instance_scalar_parameter_value(material, parameter, value)
        library.update_material_instance(material)
        assert assets.save_loaded_asset(material), path
        assert material.get_editor_property('parent') == parent
unreal.log('BIOME_SKIES_PASS: eight biome Day/Night materials saved and validated')
