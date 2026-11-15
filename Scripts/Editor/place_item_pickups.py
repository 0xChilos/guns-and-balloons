"""Place the player's hand items (AHandItemPickup) in Movement_Scene1, in front of the player start.

The player starts with an empty hand and takes an item with LMB, so every item has to lie within reach
(AFirstPersonCameraPawn.InteractRange, 400 cm) of the stationary camera. The items hang side-on in columns
around the eyes, below the horizon so they do not cover the view ahead: revolver over hook to the left,
rifle over shotgun straight ahead, extinguisher over wrench to the right. Sizes come from each mesh's bounds.

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script="<this file>"
or in the editor: py "<this file>". Re-running replaces the pickups it placed before (label prefix HandItem_).
Writes a report to the path given by ITEM_PICKUP_REPORT (optional).
ITEM_PICKUP_ONLY=Wrench,... (optional) places only those items and leaves the others where they are; the column
below them is still laid out from the others' sizes (read from their placed pickups).
"""
import math
import os
import unreal

LEVEL = '/Game/Scenes/Movement_Scene1'
PREFIX = 'HandItem_'
REPORT = os.environ.get('ITEM_PICKUP_REPORT')
ONLY = {x.strip() for x in os.environ.get('ITEM_PICKUP_ONLY', '').split(',') if x.strip()}

# (degrees to the right of straight ahead, items from the top down); an item is
# (label, its component class on the player pawn).
COLUMNS = [
    (-48.0, [('Revolver', unreal.RevolverComponent), ('Hook', unreal.HookComponent)]),
    (0.0, [('Rifle', unreal.RifleComponent), ('Shotgun', unreal.ShotgunComponent)]),
    (48.0, [('Extinguisher', unreal.ExtinguisherComponent), ('Wrench', unreal.WrenchComponent)]),
]
# TBD: prototype layout, needs tuning in the level (all cm).
DISTANCE = 120.0    # from the eye to each column
TOP = -12.0         # top edge of each column, relative to the eye height
ITEM_GAP = 10.0     # between the items of a column

lines = []
def log(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s)
    unreal.log(s)

unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

start = next(a for a in actors.get_all_level_actors() if a.get_class().get_name() == 'PlayerStart')
eye = start.get_actor_location()
yaw = start.get_actor_rotation().yaw
up = unreal.Vector(0, 0, 1)
log('eye', eye, 'yaw', yaw)

# The level is world-partitioned: a headless run loads none of its spatially loaded actors. Load the
# pickups placed before (to replace them) and the landscape under the player (for the ground check).
partition = unreal.WorldPartitionBlueprintLibrary
def is_needed(desc):
    if str(desc.label).startswith(PREFIX):
        return True
    box = desc.bounds
    return (desc.native_class is not None and 'Landscape' in desc.native_class.get_name()
        and box.min.x <= eye.x <= box.max.x and box.min.y <= eye.y <= box.max.y)
descs = [d for d in partition.get_actor_descs() if is_needed(d)]
partition.load_actors([d.guid for d in descs])

# Each actor is its own file (/Game/__ExternalActors__/...); the old pickups' files, to check after saving.
content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
old_files = [os.path.join(content_dir, str(d.actor_package)[len('/Game/'):] + '.uasset')
    for d in descs if str(d.label).startswith(PREFIX)]

def placed(label):
    return not ONLY or label[len(PREFIX):] in ONLY

old_files = [f for f, d in zip(old_files, [d for d in descs if str(d.label).startswith(PREFIX)]) if placed(str(d.label))]
kept = {}
for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith(PREFIX):
        if placed(a.get_actor_label()):
            log('removing old', a.get_actor_label())
            actors.destroy_actor(a)
        else:
            kept[a.get_actor_label()] = a

for angle, items in COLUMNS:
    facing = math.radians(yaw + angle)
    forward = unreal.Vector(math.cos(facing), math.sin(facing), 0)
    right = unreal.Vector(-math.sin(facing), math.cos(facing), 0)
    # Side-on to the player: the mesh's X (barrel) points to the player's left, its Y away from the player.
    rotation = unreal.Rotator(roll=0, pitch=0, yaw=yaw + angle - 90)
    column = eye + forward * DISTANCE

    # What is under the column, to report an item that ends up in the ground.
    hit = unreal.SystemLibrary.line_trace_single(world, column, column - up * 5000, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [], unreal.DrawDebugTrace.NONE, True)
    ground = hit.to_tuple()[4].z if hit else None

    top = TOP
    for label, item_class in items:
        if not placed(PREFIX + label):
            # Left where it is; only its size counts, for what goes below it.
            other = kept.get(PREFIX + label)
            if other:
                b = other.get_editor_property('rack').get_editor_property('display_mesh').get_skeletal_mesh_asset().get_bounds()
                top -= 2 * b.box_extent.z + ITEM_GAP
            continue
        pickup = actors.spawn_actor_from_class(unreal.HandItemPickup, eye, rotation)
        pickup.set_actor_label(PREFIX + label)
        pickup.set_folder_path('HandItems')
        # The rack shows its item's mesh, whose bounds give the size.
        rack = pickup.get_editor_property('rack')
        rack.set_editor_property('item_class', item_class)
        b = rack.get_editor_property('display_mesh').get_skeletal_mesh_asset().get_bounds()

        # Where the middle of the mesh goes, then back from there to the actor's origin (the mesh's root).
        centre = column + up * (top - b.box_extent.z)
        loc = centre - (right * -b.origin.x + forward * b.origin.y + up * b.origin.z)
        pickup.set_actor_location_and_rotation(loc, rotation, False, True)
        reach = (centre - eye).length()
        bottom = centre.z - b.box_extent.z
        log(pickup.get_actor_label(), 'at', loc, 'size', round(2 * b.box_extent.x), 'x', round(2 * b.box_extent.z), 'cm,',
            round(reach), 'cm from the eye,', 'bottom at z', round(bottom), 'ground at z', None if ground is None else round(ground),
            '(OUT OF REACH)' if reach >= 400 else '', '(IN THE GROUND)' if ground is not None and bottom < ground else '')
        top -= 2 * b.box_extent.z + ITEM_GAP

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
# A headless save leaves the file of a destroyed actor on disk, and the actor would be back on the next load.
for path in old_files:
    if os.path.exists(path):
        os.remove(path)
        log('deleted', path)
log('saved')
if REPORT:
    open(REPORT, 'w', encoding='utf-8').write('\n'.join(lines))
