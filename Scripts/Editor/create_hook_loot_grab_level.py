"""Create /Game/Scenes/Hook_Loot_Grab, a copy of Movement_Scene1 (player start, hand-item racks, ammo crate,
balloon): the player starts with the grappling hook in hand, and the five loot models from
/Game/Assets/Locations/Loot lie or float at different distances as AHookPickup actors with the NS_LootSparkle
sparkle, their contents (resource ranges) and grab sound. Each one disappears when the hook brings it in (bConsumeOnDelivery)
and counts on the resource counter board (AResourceCounterBoard) to the left of the player.

Run in the editor: py "<this file>". Re-running opens the existing copy and replaces what this script placed.
Also gives the loot meshes a simple box collision if they have none (the hook sweeps simple collision).
Writes a report to the file named by HOOK_LOOT_REPORT (optional).

Movement_Scene1 is a World Partition map. Do NOT copy it with EditorAssetLibrary.duplicate_asset and then load
the copy: the copy stays in memory as a newly created, initialized world that Map_Load cannot unload, and the
editor dies with a fatal "World Memory Leaks". This script copies it the way File > Save Current Level As does
(EditorLoadingAndSavingUtils.save_map, which writes the copy and its external actors to disk, then load_level).
"""
import os
import unreal

SRC = '/Game/Scenes/Movement_Scene1'
DST = '/Game/Scenes/Hook_Loot_Grab'
LOOT_DIR = '/Game/Assets/Locations/Loot'
SPARKLE = '/Game/Assets/Locations/Blueprints/NS_LootSparkle'
PREFIX = 'HookLoot_'
PAWN_LABEL = 'Player_HookLoot'
HOOK_INDEX = 3  # AFirstPersonCameraPawn::StartingGunIndex: 0 revolver, 1 rifle, 2 shotgun, 3 hook
REPORT = os.environ.get('HOOK_LOOT_REPORT')

# (mesh, forward cm, right cm, height above the eye cm or None = on the ground, yaw, colour = BP_Loot_* HighlightColor).
# Forward is the way the player faces (away from the racks, see below).
# The hook's default MaxRange is 2500 cm, so all of them are in reach.
LAYOUT = [
    ('SM_Loot_AmmoBox', 500, 220, None, 20, (1.0, 0.7, 0.2)),
    ('SM_Loot_MoneyPouch', 850, -260, 60, -35, (1.0, 0.85, 0.25)),
    ('SM_Loot_Log', 1250, 300, None, 70, (0.6, 1.0, 0.4)),
    ('SM_Loot_Coal', 1750, -200, 140, 10, (1.0, 0.35, 0.1)),
    ('SM_Loot_Crystal', 2250, 120, 250, -60, (0.35, 0.9, 1.0)),
]
# What each model brings in (AHookPickup::Contents: resource, min, max, rolled on arrival) and the sound it makes
# when the hook grabs it. The ammo box also holds some money.
RESOURCES = {
    'SM_Loot_AmmoBox': ([('AMMO', 1, 3), ('MONEY', 1, 10)], 'SFX_Loot_Ammo'),
    'SM_Loot_MoneyPouch': ([('MONEY', 1, 1)], 'SFX_Loot_Money'),
    'SM_Loot_Log': ([('WOOD', 1, 1)], 'SFX_Loot_Wood'),
    'SM_Loot_Coal': ([('COAL', 1, 1)], 'SFX_Loot_Coal'),
    'SM_Loot_Crystal': ([('CRYSTAL', 1, 1)], 'SFX_Loot_Crystal'),
}
SOUNDS_DIR = '/Game/Audio/Sounds'
# The resource counter (AResourceCounterBoard): forward cm, left cm, height below the eye cm. Off to the left,
# in view but clear of every line to the loot.
BOARD = (170, 95, 15)
# Tried in order when the straight line from the eye to a spot is blocked.
SIDE_NUDGES = [0, 150, -150, 300, -300, 450, -450]

lines = []
def log(*a):
    s = ' '.join(str(x) for x in a)
    lines.append(s)
    unreal.log('HOOK_LOOT: ' + s)

eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

def open_map_name():
    world = editor.get_editor_world()
    return world.get_outermost().get_name() if world else None

# Loading maps below discards unsaved changes, so never run over someone's unsaved work.
dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()] + \
        [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
if dirty:
    raise RuntimeError('Unsaved packages, save or discard them first: %s' % dirty)

# --- Loot meshes need simple collision for the hook's sweep and for physics --------------------
sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for name, *_ in LAYOUT:
    mesh = unreal.load_asset('%s/%s' % (LOOT_DIR, name))
    if sme.get_simple_collision_count(mesh) == 0:
        sme.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        eal.save_loaded_asset(mesh, False)
        log('added box collision to', name)

# --- The level -------------------------------------------------------------------------------
if eal.does_asset_exist(DST):
    if open_map_name() != DST:
        les.load_level(DST, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
else:
    if unreal.find_object(None, DST) is not None:
        raise RuntimeError('%s is in memory but not on disk (an unsaved copy?); restart the editor first' % DST)
    if open_map_name() != SRC:
        les.load_level(SRC, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
    # File > Save Current Level As: duplicates the world with its external actors and saves the copy.
    if not unreal.EditorLoadingAndSavingUtils.save_map(editor.get_editor_world(), DST):
        raise RuntimeError('could not save %s as %s' % (SRC, DST))
    les.load_level(DST, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
    log('copied', SRC, '->', DST)
if open_map_name() != DST:
    raise RuntimeError('expected %s to be open, the open map is %s' % (DST, open_map_name()))

world = editor.get_editor_world()
all_actors = actors.get_all_level_actors()

# The cube test pickups of Movement_Scene1 and whatever this script placed last time.
for a in all_actors:
    label = a.get_actor_label()
    if label.startswith('HookTest_') or label.startswith(PREFIX) or label == PAWN_LABEL:
        actors.destroy_actor(a)

start = next(a for a in actors.get_all_level_actors() if a.get_class().get_name() == 'PlayerStart')
eye = start.get_actor_location()
# The PlayerStart faces the hand-item racks, right in front of it. Their invisible click boxes block the hook
# head (it leaves from the gun, below and right of the eye), so the loot goes the other way, onto open ground,
# and the player starts facing it; the racks are one turn away.
yaw = start.get_actor_rotation().yaw + 180.0
forward = unreal.MathLibrary.get_forward_vector(unreal.Rotator(roll=0, pitch=0, yaw=yaw))
right = unreal.MathLibrary.get_right_vector(unreal.Rotator(roll=0, pitch=0, yaw=yaw))
log('eye', eye, 'facing yaw', yaw)

# --- The player, hook in hand ----------------------------------------------------------------
pawn_class = unreal.load_class(None, '/Script/gunsandballoons.FirstPersonCameraPawn')
pawn = actors.spawn_actor_from_class(pawn_class, eye, unreal.Rotator(roll=0, pitch=0, yaw=yaw))
pawn.set_actor_location_and_rotation(eye, unreal.Rotator(roll=0, pitch=0, yaw=yaw), False, True)
pawn.set_actor_label(PAWN_LABEL)
pawn.set_editor_property('auto_possess_player', unreal.AutoReceiveInput.PLAYER0)
pawn.set_editor_property('initial_hand_state', unreal.PlayerHandState.ARMED)
pawn.set_editor_property('starting_gun_index', HOOK_INDEX)
log('pawn at', pawn.get_actor_location(), 'starts with the hook')

# The hook is in hand, so its place starts empty (LMB on it puts the hook back).
for a in actors.get_all_level_actors():
    if a.get_actor_label() == 'HandItem_Hook':
        a.get_editor_property('rack').set_editor_property('starts_with_weapon', False)
        log('HandItem_Hook starts empty')

# --- Loot ------------------------------------------------------------------------------------
sparkle = unreal.load_asset(SPARKLE)
ignore = [pawn]
HOOK_RADIUS = 12.0  # UHookComponent::HookRadius

def first_hit(a, b, radius=0.0):
    if radius > 0:
        hit = unreal.SystemLibrary.sphere_trace_single(world, a, b, radius, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
            ignore, unreal.DrawDebugTrace.NONE, True)
    else:
        hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
            unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple() if hit else None

def spot(fwd, side, up):
    loc = eye + forward * fwd + right * side
    if up is None:
        hit = first_hit(loc + unreal.Vector(0, 0, 2000), loc - unreal.Vector(0, 0, 5000))
        return unreal.Vector(loc.x, loc.y, hit[5].z + 2 if hit else eye.z - 100), 40.0  # ~80 cm models
    return unreal.Vector(loc.x, loc.y, eye.z + up), 30.0

def blocker(target):
    """What the hook would hit first on its way from the gun (about 60 cm ahead, 18 right, 9 down) to target."""
    rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
    gun = eye + unreal.MathLibrary.get_forward_vector(rot) * 60 + unreal.MathLibrary.get_right_vector(rot) * 18 \
        - unreal.MathLibrary.get_up_vector(rot) * 9
    for start, radius in ((eye, 0.0), (gun, HOOK_RADIUS)):
        hit = first_hit(start, target, radius)
        if hit:
            return hit[9].get_actor_label() if hit[9] else 'something'
    return None

for name, fwd, side, up, item_yaw, color in LAYOUT:
    chosen = None
    for nudge in SIDE_NUDGES:
        loc, mid = spot(fwd, side + nudge, up)
        blocked = blocker(loc + unreal.Vector(0, 0, mid))
        if not blocked:
            chosen = (loc, None)
            break
        if chosen is None:
            chosen = (loc, blocked)
    loc, blocked = chosen

    mesh = unreal.load_asset('%s/%s' % (LOOT_DIR, name))
    pickup = actors.spawn_actor_from_class(unreal.HookPickup, loc, unreal.Rotator(roll=0, pitch=0, yaw=yaw + item_yaw))
    pickup.set_actor_location_and_rotation(loc, unreal.Rotator(roll=0, pitch=0, yaw=yaw + item_yaw), False, True)
    pickup.set_actor_scale3d(unreal.Vector(1, 1, 1))
    pickup.set_actor_label(PREFIX + name.replace('SM_Loot_', ''))
    pickup.set_folder_path('HookLoot')
    mc = pickup.get_editor_property('mesh')
    mc.set_static_mesh(mesh)
    for i, slot in enumerate(mesh.static_materials):
        mc.set_material(i, slot.material_interface)
    # No physics even on the ground: the World Partition landscape streams in after BeginPlay in PIE, and a
    # simulated pickup would fall through before it arrives. Resting still looks the same.
    pickup.set_editor_property('floating', True)
    pickup.set_editor_property('color', unreal.LinearColor(*color, 1.0))
    pickup.set_editor_property('sparkle_system', sparkle)
    pickup.set_editor_property('consume_on_delivery', True)
    contents, sound = RESOURCES[name]
    pickup.set_editor_property('contents', [unreal.ResourceYield(resource_type=getattr(unreal.ResourceType, r), min_amount=lo,
        max_amount=hi) for r, lo, hi in contents])
    pickup.set_editor_property('grab_sound', unreal.load_asset('%s/%s' % (SOUNDS_DIR, sound)))
    if hasattr(pickup, 'rerun_construction_scripts'):
        pickup.rerun_construction_scripts()
    log(pickup.get_actor_label(), 'at %.0f cm' % (loc - eye).length(), loc, 'in the air' if up is not None else 'on the ground',
        'BLOCKED by ' + blocked if blocked else 'clear path for the hook')

# --- The resource counter -------------------------------------------------------------------
fwd, left, below = BOARD
board_loc = eye + forward * fwd - right * left - unreal.Vector(0, 0, below)
board_yaw = unreal.MathLibrary.find_look_at_rotation(board_loc, eye).yaw
board = actors.spawn_actor_from_class(unreal.ResourceCounterBoard, board_loc, unreal.Rotator(roll=0, pitch=0, yaw=board_yaw))
board.set_actor_location_and_rotation(board_loc, unreal.Rotator(roll=0, pitch=0, yaw=board_yaw), False, True)
board.set_actor_label(PREFIX + 'CounterBoard')
board.set_folder_path('HookLoot')
log(board.get_actor_label(), 'at', board_loc, 'facing yaw %.0f' % board_yaw)

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log('saved', DST)
if REPORT:
    open(REPORT, 'w', encoding='utf-8').write('\n'.join(lines))
