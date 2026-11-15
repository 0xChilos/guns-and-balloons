"""Add/configure the shared health pool without rerunning the historical stove migration.

The whole basket takes damage, the envelope does not (approved 2026-11-15):
- Health: DamageBones = [basket] with bIncludeChildBones (stove, furnace_door, fuel_bucket, display, ...),
  IgnoredBones = [balloon] (envelope branch), DamageComponentTag = BasketDamage.
- The bone-less basket collision boxes (Wall_*, Floor_*, StoveBlock, ConsoleBlock) get the BasketDamage tag.
  Envelope_* spheres stay untagged.
Needs the C++ build that adds IgnoredBones / bIncludeChildBones / DamageComponentTag. Idempotent.

Run in the Unreal Editor Python console:
    exec(open('G:/UE/gunsandballoons/Scripts/Editor/configure_balloon_health.py').read())
"""
import unreal

DAMAGE_BONES = ['basket']
IGNORED_BONES = ['balloon']
TAG = 'BasketDamage'


def is_basket_box(name):
    return name.startswith(('Wall_', 'Floor_')) or name in ('StoveBlock', 'ConsoleBlock')


def main():
    bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    named = {}
    for h in handles:
        data = lib.get_data(h)
        obj = lib.get_object(data)
        if isinstance(obj, unreal.ActorComponent):
            named[str(lib.get_variable_name(data))] = obj
    health = [c for c in named.values() if isinstance(c, unreal.HealthComponent)]
    if len(health) > 1:
        raise RuntimeError('BP_Balloon has multiple health pools; resolve before configuring.')
    tagged = []
    with unreal.ScopedEditorTransaction('Configure balloon health'):
        if not health:
            handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
                parent_handle=handles[0], new_class=unreal.HealthComponent,
                blueprint_context=bp))
            if str(reason):
                raise RuntimeError(str(reason))
            assert subsystem.rename_subobject(handle, 'Health')
            health = [lib.get_object(lib.get_data(handle))]
        pool = health[0]
        pool.modify()
        pool.set_editor_property('bListenToActorDamage', True)
        pool.set_editor_property('DamageBones', DAMAGE_BONES)
        pool.set_editor_property('bIncludeChildBones', True)
        pool.set_editor_property('IgnoredBones', IGNORED_BONES)
        pool.set_editor_property('DamageComponentTag', TAG)
        # Preserve MaxHealth tuning; the component default is the TBD prototype 100.
        for name, comp in named.items():
            if not (isinstance(comp, unreal.BoxComponent) and is_basket_box(name)):
                continue
            tags = [str(t) for t in comp.get_editor_property('ComponentTags')]
            if TAG not in tags:
                comp.modify()
                comp.set_editor_property('ComponentTags', tags + [TAG])
            tagged.append(name)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.log('BALLOON_HEALTH tagged ' + ', '.join(sorted(tagged)))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for actor in actors:
        if actor.get_class() != bp.generated_class():
            continue
        pools = actor.get_components_by_class(unreal.HealthComponent)
        assert len(pools) == 1, actor.get_path_name()
        pool = pools[0]
        # Placed instances can carry stale overrides; report them instead of silently passing.
        assert pool.get_editor_property('bListenToActorDamage'), actor.get_path_name()
        assert [str(b) for b in pool.get_editor_property('DamageBones')] == DAMAGE_BONES, actor.get_path_name()
        assert [str(b) for b in pool.get_editor_property('IgnoredBones')] == IGNORED_BONES, actor.get_path_name()
        unreal.log('BALLOON_HEALTH_OK ' + actor.get_path_name())


if __name__ == '__main__':
    main()
