"""Place the player's hand items (AHandItemPickup) in BalloonFlight_Test, the enemy test level.

Same layout as place_item_pickups.py (Movement_Scene1): columns around the player's eye, below the horizon,
revolver over hook to the left, rifle over shotgun straight ahead, extinguisher over wrench to the right.
Here the player rides the flying balloon, so every pickup is attached to the BP_Balloon instance and travels
with it (otherwise the balloon would fly away from them).

Run in the editor: py "<this file>". Re-running replaces the pickups it placed before (label prefix HandItem_).
Saves the level.
"""
import math
import unreal

LEVEL = '/Game/Scenes/BalloonFlight_Test'
PREFIX = 'HandItem_'
COLUMNS = [
    (-48.0, [('Revolver', unreal.RevolverComponent), ('Hook', unreal.HookComponent)]),
    (0.0, [('Rifle', unreal.RifleComponent), ('Shotgun', unreal.ShotgunComponent)]),
    (48.0, [('Extinguisher', unreal.ExtinguisherComponent), ('Wrench', unreal.WrenchComponent)]),
]
# TBD: prototype layout, same as Movement_Scene1 (cm).
DISTANCE = 120.0
TOP = -12.0
ITEM_GAP = 10.0

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_actors = actors.get_all_level_actors()

pawn = next(a for a in level_actors if a.get_class().get_name() == 'FirstPersonCameraPawn')
balloon = pawn.get_attach_parent_actor()
eye = pawn.get_actor_location()
yaw = pawn.get_actor_rotation().yaw
up = unreal.Vector(0, 0, 1)
unreal.log('PICKUPS eye %s yaw %.1f balloon %s' % (eye, yaw, balloon.get_name() if balloon else None))

for a in level_actors:
    if a.get_actor_label().startswith(PREFIX):
        actors.destroy_actor(a)

for angle, items in COLUMNS:
    facing = math.radians(yaw + angle)
    forward = unreal.Vector(math.cos(facing), math.sin(facing), 0)
    right = unreal.Vector(-math.sin(facing), math.cos(facing), 0)
    rotation = unreal.Rotator(roll=0, pitch=0, yaw=yaw + angle - 90)
    column = eye + forward * DISTANCE
    top = TOP
    for label, item_class in items:
        pickup = actors.spawn_actor_from_class(unreal.HandItemPickup, eye, rotation)
        pickup.set_actor_label(PREFIX + label)
        pickup.set_folder_path('HandItems')
        rack = pickup.get_editor_property('rack')
        rack.set_editor_property('item_class', item_class)
        b = rack.get_editor_property('display_mesh').get_skeletal_mesh_asset().get_bounds()
        centre = column + up * (top - b.box_extent.z)
        loc = centre - (right * -b.origin.x + forward * b.origin.y + up * b.origin.z)
        pickup.set_actor_location_and_rotation(loc, rotation, False, True)
        if balloon:
            pickup.attach_to_actor(balloon, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                                   unreal.AttachmentRule.KEEP_WORLD, False)
        unreal.log('PICKUPS %s at %s, %d cm from the eye' % (label, loc, round((centre - eye).length())))
        top -= 2 * b.box_extent.z + ITEM_GAP

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('PICKUPS saved')
