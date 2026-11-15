"""Imports the wrench: T_Wrench (PS1 texture settings), SK_Wrench with its four clips, M_Wrench textured.

Wrench.fbx is one rigid mesh with no bones; its takes (Equip, Idle, ToFix, Fix) animate the mesh node itself.
Interchange turns such a mesh into a skeletal mesh with a one-bone skeleton (bone `wrench`), like the other
hand items (UGunAnimInstance plays the clips), but it only imports the first take of a rigid mesh. So the takes
are split into one FBX each first (fbx_takes.py -> Content/Animations/Wrench/A_Wrench_<Take>.fbx) and each is
imported as animation only onto SK_Wrench_Skeleton.

- /Game/Textures/Wrench/T_Wrench        from T_Wrench.png; nearest, no mips, UserInterface2D (like T_Hook_256)
- /Game/Assets/Wrench/SK_Wrench         + SK_Wrench_Skeleton, SK_Wrench_PhysicsAsset
- /Game/Assets/Wrench/M_Wrench          instance of Interchange's FBXLegacyPhongSurfaceMaterial, DiffuseColorMap = T_Wrench
- /Game/Animations/Wrench/SK_Wrench_<Take>   Equip 0.96 s, Idle 2 s (loop), ToFix 0.67 s, Fix 4.63 s (loop)

The first editor import of Wrench.fbx (assets Wrench, Wrench_Anim, Wrench_Skeleton, Wrench_PhysicsAsset) is
replaced and deleted when nothing else uses it. Safe to re-run.
Run in the editor: py "<this file>", or headless with the editor closed:
  UnrealEditor-Cmd <project> -run=pythonscript -script="<this file>"
"""
import os
import shutil
import sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib  # noqa: E402
import fbx_takes  # noqa: E402
importlib.reload(fbx_takes)  # the editor keeps modules loaded between runs

L = unreal.EditorAssetLibrary
CONTENT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
FBX = os.path.join(CONTENT, 'Assets/Wrench/Wrench.fbx')
PNG = os.path.join(CONTENT, 'Textures/Wrench/T_Wrench.png')
TAKES_DIR = os.path.join(CONTENT, 'Animations/Wrench')
MESH_DIR = '/Game/Assets/Wrench'
ANIM_DIR = '/Game/Animations/Wrench'
TEXTURE = '/Game/Textures/Wrench/T_Wrench'
MESH = MESH_DIR + '/SK_Wrench'
SKELETON = MESH_DIR + '/SK_Wrench_Skeleton'
MATERIAL = MESH_DIR + '/M_Wrench'
OLD_IMPORT = [MESH_DIR + '/Wrench_Anim', MESH_DIR + '/Wrench', MESH_DIR + '/Wrench_PhysicsAsset', MESH_DIR + '/Wrench_Skeleton']
FRAME_RATE = 24.0  # Wrench.fbx (CustomFrameRate)


def log(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    unreal.log('import_wrench: ' + s)


def interchange_import(path, dest_dir, configure):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property('use_source_name_for_asset', False)
    pipeline.get_editor_property('common_meshes_properties').set_editor_property(
        'force_all_mesh_as_type', unreal.InterchangeForceMeshType.IFMT_SKELETAL_MESH)
    mesh = pipeline.get_editor_property('mesh_pipeline')
    mesh.set_editor_property('import_static_meshes', False)
    mesh.set_editor_property('build_nanite', False)
    configure(pipeline)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(unreal.SoftObjectPath(pipeline.get_path_name()))
    source = unreal.InterchangeManager.create_source_data(path)
    unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(dest_dir + '/', source, params)


# --- Texture ------------------------------------------------------------------------------------
task = unreal.AssetImportTask()
task.filename = PNG
task.destination_path = os.path.dirname(TEXTURE)
task.destination_name = os.path.basename(TEXTURE)
task.automated = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = L.load_asset(TEXTURE)
texture.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
L.save_asset(TEXTURE, only_if_is_dirty=False)
log('texture', TEXTURE)

# --- Mesh ---------------------------------------------------------------------------------------
def mesh_options(p):
    p.set_editor_property('asset_name', 'SK_Wrench')
    # Its clips come from the split takes below.
    p.get_editor_property('animation_pipeline').set_editor_property('import_animations', False)
    materials = p.get_editor_property('material_pipeline')
    materials.set_editor_property('material_import', unreal.InterchangeMaterialImportOption.IMPORT_AS_MATERIAL_INSTANCES)
    # The FBX embeds its own copy of the texture; T_Wrench above is the one used.
    materials.get_editor_property('texture_pipeline').set_editor_property('import_textures', False)

def package(path):
    return path.split('.')[0]


def rename_new(folder, cls, target, known):
    """Interchange names what it imports after the source file; give the new cls asset in folder its target name."""
    if L.does_asset_exist(target):
        return
    for path in L.list_assets(folder, recursive=False):
        if package(path) not in known and isinstance(L.load_asset(path), cls):
            L.rename_asset(package(path), target)
            log('renamed', package(path), '->', target)
            return


# The first editor import of Wrench.fbx would be re-imported in place under its old names: remove it first.
for path in OLD_IMPORT:
    if L.does_asset_exist(path):
        users = [u for u in L.find_package_referencers_for_asset(path, False) if u not in OLD_IMPORT]
        if users:
            log('kept', path, '(used by', users, ')')
        else:
            L.delete_asset(path)
            log('deleted old', path)

known = {package(p) for p in L.list_assets(MESH_DIR, recursive=False)}
interchange_import(FBX, MESH_DIR, mesh_options)
rename_new(MESH_DIR, unreal.Skeleton, SKELETON, known)
rename_new(MESH_DIR, unreal.PhysicsAsset, MESH + '_PhysicsAsset', known)
rename_new(MESH_DIR, unreal.SkeletalMesh, MESH, known)
mesh = L.load_asset(MESH)
if not isinstance(mesh, unreal.SkeletalMesh):
    raise RuntimeError('SK_Wrench did not import: ' + str(L.list_assets(MESH_DIR, recursive=False)))
log('mesh', MESH, 'skeleton', mesh.get_editor_property('skeleton').get_path_name())

material = L.load_asset(MATERIAL)
unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, 'DiffuseColorMap', texture)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(material, 'DiffuseColorMapWeight', 1.0)
unreal.MaterialEditingLibrary.update_material_instance(material)
log('material', MATERIAL, '<-', TEXTURE)

# --- Clips --------------------------------------------------------------------------------------
skeleton = L.load_asset(SKELETON)
takes = fbx_takes.split_takes(FBX, TAKES_DIR, 'A_Wrench_')
lengths = fbx_takes.take_lengths(FBX)
for take, path in sorted(takes.items()):
    frames = int(round(lengths[take] * FRAME_RATE))
    name = 'SK_Wrench_' + take

    def anim_options(p, name=name, frames=frames):
        p.set_editor_property('asset_name', name)
        common = p.get_editor_property('common_skeletal_meshes_and_animations_properties')
        common.set_editor_property('import_only_animations', True)
        common.set_editor_property('skeleton', skeleton)
        anim = p.get_editor_property('animation_pipeline')
        anim.set_editor_property('import_animations', True)
        # The whole take, held frames at the end included (the default range trims them).
        anim.set_editor_property('animation_range', unreal.InterchangeAnimationRange.SET_RANGE)
        anim.set_editor_property('frame_import_range', unreal.Int32Interval(min=0, max=frames))
        p.get_editor_property('material_pipeline').set_editor_property('import_materials', False)

    known = {package(p) for p in L.list_assets(ANIM_DIR, recursive=False)} if L.does_directory_exist(ANIM_DIR) else set()
    interchange_import(path, ANIM_DIR, anim_options)
    rename_new(ANIM_DIR, unreal.AnimSequence, ANIM_DIR + '/' + name, known)
    clip = L.load_asset(ANIM_DIR + '/' + name)
    log('clip', ANIM_DIR + '/' + name, round(clip.get_editor_property('sequence_length'), 3) if clip else 'MISSING', 's')

# The FBX SDK unpacks each split file's embedded texture next to it (A_Wrench_<Take>.fbm); T_Wrench is the one used.
for path in takes.values():
    shutil.rmtree(os.path.splitext(path)[0] + '.fbm', ignore_errors=True)

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
for path in sorted(L.list_assets(MESH_DIR, recursive=False) + L.list_assets(ANIM_DIR, recursive=False)):
    log('  ', path)
