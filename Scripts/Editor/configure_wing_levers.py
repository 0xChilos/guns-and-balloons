"""Configure the two wing lever components on BP_Balloon. Run in Unreal Editor."""
import unreal

bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
s = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

def obj(handle):
    return lib.get_object(lib.get_data(handle))

handles = s.k2_gather_subobject_data_for_blueprint(bp)
by_name = {obj(h).get_name().removesuffix('_GEN_VARIABLE'): h for h in handles}
with unreal.ScopedEditorTransaction('Configure balloon wing levers'):
    for suffix, side in [('L', unreal.BalloonSide.LEFT), ('R', unreal.BalloonSide.RIGHT)]:
        name = 'WingLever' + suffix
        h = by_name.get(name)
        if h is None:
            h, reason = s.add_new_subobject(unreal.AddNewSubobjectParams(
                parent_handle=handles[0], new_class=unreal.BalloonWingLeverComponent, blueprint_context=bp))
            assert not str(reason), reason
            assert s.rename_subobject(h, name)
        lever = obj(h)
        lever.set_editor_property('auto_activate', True)
        lever.set_editor_property('side', side)
        lever.set_editor_property('lever_bone', 'lever_' + suffix)
        lever.set_editor_property('wing_bone', 'stopper_' + suffix)
        print('WING_COMPONENT', name, side, lever.get_num_states())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE, bp.get_editor_property('status')
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
print('WING_COMPONENTS_SAVED')


