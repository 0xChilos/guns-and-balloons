"""Add the shared health pool without rerunning the historical stove migration.

Run in the Unreal Editor Python console:
    exec(open('G:/UE/gunsandballoons/Scripts/Editor/configure_balloon_health.py').read())
"""
import unreal


def main():
    bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    components = [lib.get_object(lib.get_data(h)) for h in handles]
    health = [c for c in components if isinstance(c, unreal.HealthComponent)]
    if len(health) > 1:
        raise RuntimeError('BP_Balloon has multiple health pools; resolve before configuring.')
    with unreal.ScopedEditorTransaction('Configure balloon health'):
        if not health:
            handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
                parent_handle=handles[0], new_class=unreal.HealthComponent,
                blueprint_context=bp))
            if str(reason):
                raise RuntimeError(str(reason))
            assert subsystem.rename_subobject(handle, 'Health')
            health = [lib.get_object(lib.get_data(handle))]
        health[0].set_editor_property('bListenToActorDamage', True)
        health[0].set_editor_property('DamageBones', ['stove', 'furnace_door'])
        # Preserve MaxHealth tuning; the component default is the TBD prototype 100.
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for actor in actors:
        if actor.get_class() != bp.generated_class():
            continue
        pools = actor.get_components_by_class(unreal.HealthComponent)
        assert len(pools) == 1, actor.get_path_name()
        pool = pools[0]
        assert pool.get_editor_property('bListenToActorDamage')
        assert [str(b) for b in pool.get_editor_property('DamageBones')] == ['stove', 'furnace_door']
        unreal.log('BALLOON_HEALTH_OK ' + actor.get_path_name())


if __name__ == '__main__':
    main()
