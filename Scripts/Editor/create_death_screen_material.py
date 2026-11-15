"""Create /Game/UI/DeathScreen/M_DeathScreenPanel for ADeathScreen (docs/features/ui-menus.md).

Why: the game's filter (MI_OilyDepths) fogs pixels by scene depth. The pause menu's panel material is
translucent and writes no depth, so wherever the sky is behind it the filter treats the panel as
infinitely far and fogs its dark parts into a light haze. The death screen needs real darks, so its
panel is MASKED: the widget's alpha goes through DitherTemporalAA into the opacity mask, so partial
transparency becomes a PS1-style dither, and the panel writes depth right in front of the camera, so
the filter leaves it unfogged.

Graph: TextureSampleParameter2D "SlateUI" (the widget component's render target) RGB -> Emissive;
DitherTemporalAA(alpha) -> Opacity Mask. Unlit, one-sided. The team picked this look (2026-11-15).
Run in the editor: py "Scripts/Editor/create_death_screen_material.py". It rebuilds the material.
"""
import unreal

DEST_DIR = "/Game/UI/DeathScreen"
NAME = "M_DeathScreenPanel"
PATH = DEST_DIR + "/" + NAME
assets = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

if assets.does_asset_exist(PATH):
    assets.delete_asset(PATH)  # rebuilt from scratch (deleting nodes in place crashes the editor)
if True:  # always built from scratch
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DEST_DIR, unreal.Material, unreal.MaterialFactoryNew())
    tex = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    tex.set_editor_property("parameter_name", "SlateUI")
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    dither = MEL.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -200, 200)
    dither.set_editor_property("material_function", unreal.load_asset(
        "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA"))
    MEL.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_expressions(tex, "A", dither, "Alpha Threshold")
    MEL.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.log("DeathScreen: created %s" % PATH)

material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", False)
material.set_editor_property("opacity_mask_clip_value", 0.5)
MEL.recompile_material(material)
assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log("DEATH_MATERIAL_OK blend %s" % material.get_editor_property("blend_mode"))
