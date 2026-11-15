"""Generate MainLevel and report functional enemies after rebuilding the native module.

Run: py G:/UE/gunsandballoons/Scripts/Editor/verify_level_enemies.py
Does not save the level or change source config assets.
"""
import json
from pathlib import Path
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before generating the editor preview'
assert editor.get_editor_world().get_path_name() == '/Game/Scenes/MainLevel.MainLevel'
generators = [a for a in actors.get_all_level_actors()
              if isinstance(a, unreal.BalloonLevelGenerator)]
assert len(generators) == 1, 'Expected one level generator'
generator = generators[0]
for name in ('spawn_enemies', 'spawn_shared_enemies', 'spawn_biome_enemies'):
    generator.set_editor_property(name, True)
if not generator.get_editor_property('active_config'):
    assert generator.generate_level(), '\n'.join(generator.get_diagnostics())
# EditorActorSubsystem omits transient generated actors; query the world instead.
world_actors = unreal.GameplayStatics.get_all_actors_of_class(editor.get_editor_world(), unreal.Actor)
enemies = [a for a in world_actors
           if a.get_owner() == generator and (
               a.actor_has_tag('BalloonLevel.Enemy') or
               a.get_class().get_name() == 'BP_RocketBalloon_C')]
report = {
    'biome': str(generator.get_editor_property('active_config').get_editor_property('biome_id')),
    'enemies': [{'class': a.get_class().get_name(), 'name': a.get_name(),
                 'position_cm': str(a.get_actor_location())} for a in enemies],
    'diagnostics': list(generator.get_diagnostics()),
}
Path(unreal.Paths.project_saved_dir(), 'EnemyLevelCheck.json').write_text(
    json.dumps(report, indent=2), encoding='utf-8')
assert any(isinstance(a, unreal.NemesisAirship) for a in enemies), 'Missing shared airship'
airship = next(a for a in enemies if isinstance(a, unreal.NemesisAirship))
actors.set_selected_level_actors([airship])
target = airship.get_actor_location()
camera = target + unreal.Vector(-8000.0, -8000.0, 4000.0)
editor.set_level_viewport_camera_info(
    camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
unreal.log('LEVEL_ENEMIES_PASS: biome={}, enemies={}; selected airship'.format(
    report['biome'], len(enemies)))
