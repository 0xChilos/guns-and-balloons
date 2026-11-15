"""Place grappling-hook test pickups (AHookPickup) in Movement_Scene1 at several distances.

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script="<this file>"
or in the editor: py "<this file>". Re-running replaces the pickups it placed before (label prefix HookTest_).
Writes a report next to this file's output path given by HOOK_PICKUP_REPORT (optional).
"""
import os
import unreal

LEVEL = '/Game/Scenes/Movement_Scene1'
PREFIX = 'HookTest_'
REPORT = os.environ.get('HOOK_PICKUP_REPORT')

# (forward cm, right cm, height above the eye cm, floating, colour). Default hook range is 2500 cm,
# so the last one is out of reach on purpose.
LAYOUT = [
    (400, 120, 0, True, (0.9, 0.2, 0.1)),
    (700, -250, 80, True, (0.1, 0.6, 0.9)),
    (1000, 200, -60, False, (0.2, 0.8, 0.2)),
    (1500, -150, 150, True, (0.9, 0.8, 0.1)),
    (2000, 300, 250, True, (0.8, 0.2, 0.8)),
    (2400, -100, 50, True, (1.0, 0.5, 0.0)),
    (3200, 0, 100, True, (0.5, 0.5, 0.5)),
]

lines = []
def log(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s)
    unreal.log(s)

unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith(PREFIX):
        log('removing old', a.get_actor_label())
        actors.destroy_actor(a)

start = next(a for a in actors.get_all_level_actors() if a.get_class().get_name() == 'PlayerStart')
eye = start.get_actor_location()
yaw = start.get_actor_rotation().yaw
forward = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0, yaw, 0))
right = unreal.MathLibrary.get_right_vector(unreal.Rotator(0, yaw, 0))
log('eye', eye, 'yaw', yaw)

for fwd, side, up, floating, color in LAYOUT:
    loc = eye + forward * fwd + right * side + unreal.Vector(0, 0, up)
    if not floating:
        # Rest it on whatever is below.
        hit = unreal.SystemLibrary.line_trace_single(world, loc + unreal.Vector(0, 0, 500), loc - unreal.Vector(0, 0, 5000),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        if hit:
            impact = hit.to_tuple()[4]
            loc = unreal.Vector(loc.x, loc.y, impact.z + 16)
    # Is it in plain sight from the eye?
    sight = unreal.SystemLibrary.line_trace_single(world, eye, loc, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)
    blocked = sight.to_tuple()[9].get_actor_label() if sight and sight.to_tuple()[9] else None if not sight else 'something'
    pickup = actors.spawn_actor_from_class(unreal.HookPickup, loc, unreal.Rotator(0, yaw, 0))
    label = '%s%02dm' % (PREFIX, round(fwd / 100))
    pickup.set_actor_label(label)
    pickup.set_editor_property('floating', floating)
    pickup.set_editor_property('color', unreal.LinearColor(*color, 1.0))
    pickup.set_folder_path('HookTest')
    log(label, 'at', loc, 'floating' if floating else 'on ground', 'blocked by ' + str(blocked) if sight else 'clear line of sight')

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log('saved')
if REPORT:
    open(REPORT, 'w', encoding='utf-8').write('\n'.join(lines))
