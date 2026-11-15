"""Put the player's hand items in the balloon basket (docs/features/weapons.md, Hand and items).

BP_Balloon gets one UWeaponRackComponent per item, snapped to the socket SK_Balloon has for it
(Content/Assets/Balloon/Balloon_build.py), so each item lies in its place and moves with the basket:
- weapon wall (right of the display): revolver on its hook, rifle and shotgun upright in the gun rack;
- tool board (left of the display): grappling hook on its brackets, extinguisher in its cup, wrench on its peg.
Each item's pose is worked out here in balloon space (the wall's axes, where the item's own origin or butt goes)
and stored as the rack's DisplayOffset / DisplayRotation relative to its socket. TBD: poses need tuning by eye.

The test levels with a balloon lose their floating AHandItemPickup items (Scripts/Editor/place_item_pickups.py),
or the player would see every item twice. Hook_Loot_Grab has no balloon and keeps its pickups.

Run in the editor: py "<this file>" [racks|levels] (default both). Re-running replaces the racks it added
before (names Rack_*).
"""
import math
import os
import sys
import unreal

STEPS = set(sys.argv[1:]) or {'racks', 'levels'}

BALLOON = '/Game/Assets/Balloon/BP_Balloon'
LEVELS_WITHOUT_PICKUPS = ['/Game/Scenes/Enemies_Test', '/Game/Scenes/DeathScreen_Test', '/Game/Scenes/Movement_Scene1']
PREFIX = 'Rack_'

V = unreal.Vector  # careful: unary minus on an unreal.Vector negates it in place; use v * -1.0
def norm(v):
    return v * (1.0 / v.length())
UP = V(0, 0, 1)
S2 = math.sqrt(0.5)
# Balloon space. n = from the wall into the basket, u = to the right along the wall as seen from inside.
WEAPON_N, WEAPON_U = V(-S2, -S2, 0), V(-S2, S2, 0)
TOOL_N, TOOL_U = V(-S2, S2, 0), V(S2, S2, 0)
HOOK_TILT = math.radians(30)  # the hook hangs diagonally, claw up to the right (like a fire axe)

# label: (item class, socket, mesh X axis, mesh Z axis, the mesh point (cm, mesh space) that goes on the socket)
# Item meshes: X along the barrel / tool, Y across (thin), Z up. TBD: all poses need tuning by eye.
ITEMS = {
    # hangs by its trigger guard, barrel down, flat against the board
    'Revolver': (unreal.RevolverComponent, 'socket_revolver', UP * -1.0, WEAPON_U * -1.0, V(2, 0, -5)),
    # upright, butt in its pocket, barrel leaning into the notch of the top shelf (78 cm up, 3 cm toward the wall)
    'Rifle': (unreal.RifleComponent, 'socket_rifle', norm(UP * 78 - WEAPON_N * 3), WEAPON_N, None),
    'Shotgun': (unreal.ShotgunComponent, 'socket_shotgun', norm(UP * 78 - WEAPON_N * 3), WEAPON_N, None),
    'Hook': (unreal.HookComponent, 'socket_tool_horizontal', TOOL_U * math.cos(HOOK_TILT) + UP * math.sin(HOOK_TILT),
             TOOL_U * -1.0 * math.sin(HOOK_TILT) + UP * math.cos(HOOK_TILT), V(0, 0, 0)),
    'Extinguisher': (unreal.ExtinguisherComponent, 'socket_tool_vertical_1', TOOL_U * -1.0, UP, V(0, 0, 0)),
    'Wrench': (unreal.WrenchComponent, 'socket_tool_vertical_2', UP, TOOL_U, V(0, 0, 0)),
}

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
bp = unreal.load_asset(BALLOON)

def add_racks():
    # A balloon placed for a moment: the sockets' reference-pose transforms in balloon space.
    probe = eas.spawn_actor_from_class(bp.generated_class(), V(0, 0, -200000))
    probe_mesh = [c for c in probe.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == 'Mesh'][0]
    to_balloon = probe.get_actor_transform().inverse()

    def socket_in_balloon(name):
        return to_balloon.transform_location(probe_mesh.get_socket_location(name))
    # A rack takes its socket's position and the balloon mesh's axes (UWeaponRackComponent::AttachSocket).
    mesh_rot = probe_mesh.get_world_rotation()
    mesh_axes = (mesh_rot.get_forward_vector(), mesh_rot.get_right_vector(), mesh_rot.get_up_vector())

    def item_mesh(item_class):
        p = eas.spawn_actor_from_class(unreal.HandItemPickup, V(0, 0, -200000))
        rack = p.get_editor_property('rack')
        rack.set_editor_property('item_class', item_class)
        mesh = rack.get_editor_property('display_mesh').get_skeletal_mesh_asset()
        eas.destroy_actor(p)
        return mesh

    poses = {}
    for label, (item_class, socket, x_axis, z_axis, anchor) in ITEMS.items():
        bounds = item_mesh(item_class).get_bounds()
        if anchor is None:  # a long gun: the middle of its butt end
            anchor = V(bounds.origin.x - bounds.box_extent.x, 0, bounds.origin.z - bounds.box_extent.z * 0.35)
        # The item's axes in balloon space, and where its origin goes.
        x = norm(x_axis)
        z = norm(z_axis - x * z_axis.dot(x))
        y = z.cross(x)
        s = socket_in_balloon(socket)
        origin = s - (x * anchor.x + y * anchor.y + z * anchor.z)
        # The same in the rack's frame: on the socket, with the mesh's axes (plain dot products).
        local = lambda v: V(v.dot(mesh_axes[0]), v.dot(mesh_axes[1]), v.dot(mesh_axes[2]))
        offset = local(origin - s)
        rotation = unreal.MathLibrary.make_rot_from_xz(local(x), local(z))
        poses[label] = (item_class, socket, offset, rotation)
        unreal.log('BASKET_RACKS %s on %s: offset %s rotation %s' % (label, socket, offset, rotation))
    eas.destroy_actor(probe)

    # Replace the racks added before, then add one per item under the root.
    def scene_root():
        handles = sub.k2_gather_subobject_data_for_blueprint(bp)
        for h in handles:
            d = lib.get_data(h)
            if lib.is_root_component(d) and not lib.is_inherited_component(d):
                return h, handles
        return handles[0], handles
    root, handles = scene_root()
    for h in handles:
        obj = lib.get_associated_object(lib.get_data(h))
        if obj and obj.get_name().startswith(PREFIX):
            sub.delete_subobject(root, h, bp)
    root, handles = scene_root()
    for label, (item_class, socket, offset, rotation) in poses.items():
        handle, fail = sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.WeaponRackComponent, blueprint_context=bp))
        if fail and not str(fail).strip() == '':
            unreal.log_warning('BASKET_RACKS could not add %s: %s' % (label, fail))
            continue
        sub.rename_subobject(handle, unreal.Text(PREFIX + label))
        rack = lib.get_associated_object(lib.get_data(handle))
        rack.set_editor_property('item_class', item_class)
        rack.set_editor_property('attach_socket', socket)
        rack.set_editor_property('display_offset', offset)
        rack.set_editor_property('display_rotation', rotation)
        rack.set_editor_property('fit_interact_shape', True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    unreal.log('BASKET_RACKS BP_Balloon: %d racks' % len(poses))



def remove_pickups():
    # The floating pickups go from the levels that have the balloon.
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    for level in LEVELS_WITHOUT_PICKUPS:
        if not unreal.EditorAssetLibrary.does_asset_exist(level):
            continue
        les.load_level(level, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        files = []
        try:  # world-partitioned levels: load the pickups first, and drop their actor files after saving
            descs = [d for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
                     if d.native_class is not None and d.native_class.get_name() == 'HandItemPickup']
            unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
            files = [os.path.join(content_dir, str(d.actor_package)[len('/Game/'):] + '.uasset') for d in descs]
        except Exception:
            pass
        gone = 0
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.HandItemPickup):
                eas.destroy_actor(a)
                gone += 1
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        for f in files:
            if os.path.exists(f):
                os.remove(f)
        unreal.log('BASKET_RACKS %s: removed %d pickups' % (level, gone))


if 'racks' in STEPS:
    add_racks()
if 'levels' in STEPS:
    remove_pickups()
unreal.log('BASKET_RACKS done')
