"""Capture the hanging revolver from the placed player's eye; save no level."""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
balloon = next(a for a in actors.get_all_level_actors() if a.get_class().get_name() == 'BP_Balloon_C')
rack_live = balloon.get_component_by_class(unreal.WeaponRackComponent)
display = rack_live.get_editor_property('display_mesh')
pawn = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.FirstPersonCameraPawn))
camera_location = pawn.get_component_by_class(unreal.CameraComponent).get_world_location()
rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, rack_live.get_world_location())
capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, camera_location, rotation, transient=True)
try:
    display.set_visibility(True)
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property('capture_every_frame', False)
    component.set_editor_property('capture_on_movement', False)
    component.set_editor_property('show_flag_settings', [unreal.EngineShowFlagsSetting('Lighting', False)])
    component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.set_editor_property('fov_angle', 80.0)
    component.hide_actor_components(pawn)
    target = unreal.RenderingLibrary.create_render_target2d(capture, 1280, 960, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    component.set_editor_property('texture_target', target)
    component.capture_scene()
    unreal.RenderingLibrary.export_render_target(capture, target,
        unreal.Paths.project_saved_dir() + 'Phase6', 'rack_player_view.png')
    print('PHASE6_CAPTURE', camera_location, rotation)
finally:
    actors.destroy_actor(capture)
# Leave preview visible until the next editor tick; caller restores SetHasWeapon(false).
