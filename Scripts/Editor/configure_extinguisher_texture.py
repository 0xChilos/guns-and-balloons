"""Gives the fire extinguisher's texture the PS1 settings the other weapon textures have (as T_Hook_256):
nearest filtering (blocky texels), no mipmaps, UserInterface2D compression (no block artefacts).
The FBX import left T_Extinguisher_256 on Default (bilinear, mipmaps), which smears it.

Safe to re-run. Run in the editor (py "<path>") or headless with the editor closed:
  UnrealEditor-Cmd.exe gunsandballoons.uproject -run=pythonscript -script="Scripts/Editor/configure_extinguisher_texture.py"
"""
import unreal

TEXTURES = [
    "/Game/Textures/Fire_extinguisher/T_Extinguisher_256",
]

for path in TEXTURES:
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        unreal.log_warning(f"configure_extinguisher_texture: {path} not found")
        continue
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log(f"configure_extinguisher_texture: {path} -> nearest, no mips, UserInterface2D")
