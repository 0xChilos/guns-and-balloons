"""Phase 6: configure BP_Balloon rack and approved prototype lever values.

Run in Unreal Editor through MCP: py ".../configure_weapon_rack.py".
ABP, physics assets and lever collision components are untouched.
"""
import unreal

bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
handles = sub.k2_gather_subobject_data_for_blueprint(bp)
def obj(handle):
    return lib.get_object(lib.get_data(handle))
by_name = {obj(h).get_name().removesuffix('_GEN_VARIABLE'): h for h in handles}
with unreal.ScopedEditorTransaction('Configure weapon rack and wing grab defaults'):
    handle = by_name.get('WeaponRack')
    if handle is None:
        handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=by_name['Mesh'], new_class=unreal.WeaponRackComponent,
            blueprint_context=bp))
        assert not str(reason), reason
        assert sub.rename_subobject(handle, 'WeaponRack')
    rack = obj(handle)
    assert sub.attach_subobject(by_name['Mesh'], handle)
    assert rack.attach_to_component(obj(by_name['Mesh']), 'socket_small_weapon',
        unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
        unreal.AttachmentRule.KEEP_WORLD, False)
    rack.set_editor_property('relative_location', unreal.Vector())
    rack.set_editor_property('relative_rotation', unreal.Rotator())
    rack.set_editor_property('absolute_scale', True)
    rack.set_editor_property('relative_scale3d', unreal.Vector(1, 1, 1))
    offset = unreal.Vector(-2, -11, .6)
    rotation = unreal.Rotator(pitch=-3.6, yaw=90, roll=0)
    rack.set_editor_property('display_offset', offset)
    rack.set_editor_property('display_rotation', rotation)
    display = rack.get_editor_property('display_mesh')
    pax = unreal.load_asset('/Game/Assets/Revolver/SK_Pax')
    display.set_skeletal_mesh_asset(pax)
    # Single-node mode with no animation asset evaluates the static reference pose.
    display.set_editor_property('pause_anims', False)
    display.set_update_animation_in_editor(True)
    bounds = pax.get_bounds()
    shape = rack.get_editor_property('interact_shape')
    shape.set_editor_property('relative_location', offset + unreal.MathLibrary.greater_greater_vector_rotator(bounds.origin, rotation))
    shape.set_editor_property('relative_rotation', rotation)
    shape.set_box_extent(bounds.box_extent + unreal.Vector(1.5, 1.5, 1.5), False)
    for suffix in ['L', 'R']:
        lever = obj(by_name['WingLever' + suffix])
        for name, value in dict(detent_break_angle=12.0, max_slip_chance=.05,
                detent_strain_decay=30.0, detent_give=.25, slip_check_interval=1.0,
                slip_move_speed=600.0, slip_while_grabbed=False, slip_seed=0,
                handle_length=42.0, min_grab_radius=8.0, lever_drag_max_speed=360.0,
                drag_sign=1.0, lever_axis=unreal.Vector(1, 0, 0),
                release_mode=unreal.WingLeverReleaseMode.RETURN_TO_LAST_DETENT).items():
            lever.set_editor_property(name, value)
        # Keep existing angles and flight modifiers, update only the new state fields.
        states = lever.get_editor_property('states')
        for i, state in enumerate(states):
            state.set_editor_property('detent_break_angle', 12.0)
            state.set_editor_property('slip_chance', .05 * i / (len(states) - 1))
        lever.set_editor_property('states', states)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
print('PHASE6_CONFIGURED', rack.get_path_name())
