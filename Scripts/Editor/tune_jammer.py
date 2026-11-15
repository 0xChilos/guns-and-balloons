"""Jammer tower tuning (docs/features/biomes.md, City; docs/features/ui-menus.md, Radar jamming).

- BP_JammerTower.JamRadiusM: how far the jamming reaches. TBD: needs tuning in play.
- BP_JammerTower.DishHealth: hits that shoot the dish down (one, at the user's request; was 3).
- SM_JammerTower collides by its real geometry (complex as simple). Its simple collision was two convex hulls that
  wrapped the dish, so bullets stopped at the tower and the dish could not be shot down.

Run in the editor: py "<this file>". Re-running is harmless.
"""
import unreal

JAM_RADIUS_M = 120.0  # TBD (was 250)
DISH_HITS = 1

bp = unreal.load_asset('/Game/Assets/Locations/Blueprints/BP_JammerTower')
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('JamRadiusM', JAM_RADIUS_M)
cdo.set_editor_property('DishHealth', DISH_HITS)
unreal.EditorAssetLibrary.save_loaded_asset(bp, False)

tower = unreal.load_asset('/Game/Assets/Locations/City/SM_JammerTower')
body = tower.get_editor_property('body_setup')
body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
tower.modify()
unreal.EditorAssetLibrary.save_loaded_asset(tower, False)

unreal.log('TUNE_JAMMER radius %.0f m, dish %d hit(s), tower collides by its mesh' % (cdo.get_editor_property('JamRadiusM'), cdo.get_editor_property('DishHealth')))
