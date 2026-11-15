"""Set the scale of every loot_* socket on the static meshes under /Game/Assets to 1.

The location meshes came from Blender with their loot_* sockets at scale 100. AC_LootSpots spawns loot at the
socket's transform, scale included, so every loot item (log, coal, ...) was 100 times its size: a 1 m log became a
100 m one whose collision swallowed whatever was behind it (the jammer tower's dish could not be shot).

Run in the editor: py "<this file>". Re-running only reports.
"""
import unreal

reg = unreal.AssetRegistryHelpers.get_asset_registry()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
probe = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -500000))
comp = probe.static_mesh_component
for a in reg.get_assets_by_path('/Game/Assets', True):
    if str(a.asset_class_path.asset_name) != 'StaticMesh':
        continue
    mesh = unreal.load_asset(str(a.package_name))
    comp.set_static_mesh(mesh)  # the component lists a mesh's socket names
    fixed = []
    for name in comp.get_all_socket_names():
        if not str(name).lower().startswith('loot'):
            continue
        socket = mesh.find_socket(name)
        if abs(socket.relative_scale.x - 1) > 0.001 or abs(socket.relative_scale.z - 1) > 0.001:
            socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
            fixed.append(str(name))
    if fixed:
        mesh.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        unreal.log('LOOT_SOCKETS %s: %s set to scale 1' % (a.package_name, fixed))
eas.destroy_actor(probe)
unreal.log('LOOT_SOCKETS done')
