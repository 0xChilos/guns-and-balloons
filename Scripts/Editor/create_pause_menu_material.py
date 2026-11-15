"""Create /Game/UI/PauseMenu/M_PauseMenuPanel for APauseMenuScreen (docs/tech/main-menu.md).

A copy of the engine's widget material (Widget3DPassThrough: translucent, unlit, one-sided) with
depth test off, so the pause menu panel is drawn over everything near the camera (the gun in the
player's hands, the basket). The engine's Widget3DPassThrough_Translucent(_OneSided) are material
instances and can't switch depth test off, so the base Material is copied.
Run in the editor: py "Scripts/Editor/create_pause_menu_material.py". Rerunning only reapplies the
settings to the existing asset; nothing else is touched or saved.
"""

import unreal

SOURCE = "/Engine/EngineMaterials/Widget3DPassThrough"
DEST = "/Game/UI/PauseMenu/M_PauseMenuPanel"

assets = unreal.EditorAssetLibrary

if assets.does_asset_exist(DEST):
    material = assets.load_asset(DEST)
    unreal.log(f"PauseMenu: {DEST} exists, updating it")
else:
    material = assets.duplicate_asset(SOURCE, DEST)
    unreal.log(f"PauseMenu: created {DEST} from {SOURCE}")

if not isinstance(material, unreal.Material):
    raise RuntimeError(f"PauseMenu: {DEST} is not a Material")

material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("two_sided", False)
material.set_editor_property("disable_depth_test", True)
unreal.MaterialEditingLibrary.recompile_material(material)
assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log(
    "PauseMenu: M_PauseMenuPanel ready (blend %s, two sided %s, depth test off %s)"
    % (
        material.get_editor_property("blend_mode"),
        material.get_editor_property("two_sided"),
        material.get_editor_property("disable_depth_test"),
    )
)
