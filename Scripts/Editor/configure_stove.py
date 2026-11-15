"""Create fuel assets and configure Phase 4 stove animation/collision.

In the Unreal Editor Python console:
    exec(open('G:/UE/gunsandballoons/Scripts/Editor/configure_stove.py', encoding='utf-8').read())
"""
import unreal


def configure_fuel_assets():
    folder = '/Game/Data/Stove'
    fuel_class = unreal.load_class(None, '/Script/gunsandballoons.StoveFuelType')
    if fuel_class is None:
        raise RuntimeError('Compile StoveFuelType and reload the editor before running this script.')

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', fuel_class)
    unreal.EditorAssetLibrary.make_directory(folder)

    fuels = (
        ('Wood', 'Дрова', 120.0, 20.0, False, 'Cube'),
        ('Coal', 'Вугілля', 60.0, 30.0, False, 'Cylinder'),
        ('Crystal', 'Кристал', 10.0, 50.0, True, 'Cone'),
    )
    for suffix, label, burn_time, heat_add, instant, shape in fuels:
        name = 'DA_Fuel_' + suffix
        path = folder + '/' + name
        asset = unreal.load_asset(path)
        if asset is None:
            asset = tools.create_asset(name, folder, fuel_class, factory)
        if asset is None or asset.get_class() != fuel_class:
            raise RuntimeError('Expected a StoveFuelType asset at ' + path)

        values = {
            'DisplayName': unreal.Text(label),
            'BurnTime': burn_time,
            'HeatAdd': heat_add,
            'DecayPauseOverride': {'Wood': 110.0, 'Coal': 40.0, 'Crystal': 10.0}[suffix],
            'bInstantHeat': instant,
            'HeatRateOverride': 0.0,
        }
        for key, value in values.items():
            asset.set_editor_property(key, value)

        # Placeholder art only. Preserve hand art/transforms tuned on subsequent runs.
        if asset.get_editor_property('HandMesh') is None:
            mesh = unreal.load_asset('/Engine/BasicShapes/' + shape)
            if mesh is None:
                raise RuntimeError('Missing engine placeholder mesh: ' + shape)
            asset.set_editor_property('HandMesh', mesh)

        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('Failed to save ' + path)
        for key, value in values.items():
            actual = asset.get_editor_property(key)
            matches = str(actual) == str(value) if key == 'DisplayName' else actual == value
            assert matches, (path, key, actual)
        unreal.log('STOVE_FUEL_SAVED ' + path)


def configure_stove_blueprint():
    bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    def obj(h):
        return lib.get_object(lib.get_data(h))
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    by_name = {obj(h).get_name().removesuffix('_GEN_VARIABLE'): h for h in handles}
    # Ammo belongs to RMB; the pawn rejects LMB before dispatching stove interactions.
    if any(isinstance(obj(h), unreal.AmmoSupplyComponent) for h in handles):
        raise RuntimeError('STOP: BP_Balloon has AmmoSupplyComponent; move ammo to separate crate actors.')
    mesh_handle = by_name.get('Mesh')
    if mesh_handle is None:
        raise RuntimeError('BP_Balloon Mesh component not found.')
    def ensure(name, cls, parent):
        if name in by_name:
            return obj(by_name[name]), False
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=parent, new_class=cls, blueprint_context=bp))
        if str(reason):
            raise RuntimeError(str(reason))
        assert subsystem.rename_subobject(handle, name)
        by_name[name] = handle
        return obj(handle), True
    stove, _ = ensure('Stove', unreal.StoveComponent, handles[0])
    vfx, vfx_created = ensure('StoveVFX', unreal.StoveVfxComponent, handles[0])
    if vfx_created:
        # Front of the dome: keep the vent smoke clear of the solid stove mesh.
        vfx.set_editor_property('OverheatOffset', unreal.Vector(0, -28, 100))
    box, created = ensure('StoveFirebox', unreal.BoxComponent, mesh_handle)
    box.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    box.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    box.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
    box.set_editor_property('generate_overlap_events', False)
    if created:
        # Prototype only: fit to the imported mouth in the BP viewport before PIE.
        box.set_box_extent(unreal.Vector(8, 1, 7.5))
        mesh = obj(mesh_handle)
        hinge = mesh.get_socket_transform('furnace_door', unreal.RelativeTransformSpace.RTS_COMPONENT)
        box.set_relative_location(hinge.translation + unreal.Vector(9.5, -2.5, 0), False, False)
    stove.set_editor_property('FireboxShape', box)
    # Existing modeled bucket: Balloon_build.py CB and Z0, in mesh centimeters.
    # Coal/crystal buckets still require their own art and placement.
    supply, supply_created = ensure('WoodFuelSupply', unreal.FuelSupplyComponent, mesh_handle)
    supply.set_editor_property('Fuel', unreal.load_asset('/Game/Data/Stove/DA_Fuel_Wood'))
    if supply_created:
        supply.set_editor_property('StartCount', 4)  # TBD prototype stock
        supply.set_editor_property('Capacity', 10)  # TBD prototype capacity
        supply.set_editor_property('relative_location', unreal.Vector(47, 27, 18))
        supply.get_editor_property('InteractShape').set_box_extent(unreal.Vector(17, 17, 17))
    assert supply.get_editor_property('Fuel') is not None
    mesh = obj(mesh_handle)
    mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    mesh.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


def configure_animation():
    L, P = unreal.BlueprintEditorLibrary, unreal.BlueprintGraphPinLibrary
    bp = unreal.load_asset('/Game/Assets/Balloon/ABP_Balloon')
    graph = L.find_event_graph(bp)
    variables = {
        'StoveRef': L.get_object_reference_type(unreal.StoveComponent.static_class()),
        'StoveDoorAngle': L.get_basic_type_by_name('real'),
        'StoveTemp01': L.get_basic_type_by_name('real'),
        'StoveBurn01': L.get_basic_type_by_name('real'),
        'StoveDoorRotation': L.get_struct_type(unreal.load_object(None, '/Script/CoreUObject.Rotator')),
        'StoveGaugeRotation': L.get_struct_type(unreal.load_object(None, '/Script/CoreUObject.Rotator')),
        'StoveBurnTranslation': L.get_struct_type(unreal.load_object(None, '/Script/CoreUObject.Vector')),
    }
    for name, kind in variables.items():
        if not L.get_member_variable_type(bp, name):
            assert L.add_member_variable(bp, name, kind), name
    # Use a node marker rather than presence of variables: partial runs must not be mistaken for success.
    existing = [n for n in unreal.ObjectIterator(unreal.K2Node_IfThenElse)
                if n.get_outer() == graph and L.get_node_comment(n) == 'STOVE_PHASE4_UPDATE']
    if not existing:
        def create(ref, x, y):
            node = L.create_node_from_reference(graph, ref, unreal.Vector2D(x, y))
            assert node, ref
            return node
        def inp(n, name): return L.find_input_pin(n, name)
        def out(n, name='ReturnValue'): return L.find_output_pin(n, name)
        def link(a, b): assert a and b and P.try_create_connection(a, b), 'Stove graph connection failed'
        def get(name, x, y): return out(create('self.' + name, x, y), name)
        owner = create('AnimInstance.GetOwningActor', -4000, 1000)
        find = create('StoveComponent.FindStove', -3700, 1000)
        link(out(owner), inp(find, 'Actor'))
        cache = create('set self.StoveRef', -3400, 700)
        link(out(find), inp(cache, 'StoveRef'))
        valid = create('KismetSystemLibrary.IsValid', -3400, 1100)
        link(get('StoveRef', -3700, 1300), inp(valid, 'Object'))
        branch = create('|IfThenElse', -3100, 700)
        link(out(valid), inp(branch, 'Condition'))
        link(out(cache, 'then'), inp(branch, 'execute'))
        last = branch
        for i, (name, function) in enumerate([
                ('StoveDoorAngle', 'GetDoorAngle'), ('StoveTemp01', 'GetTemperature01'),
                ('StoveBurn01', 'GetBurnTime01')]):
            x = -2800 + i * 400
            call = create('StoveComponent.' + function, x, 1100)
            link(get('StoveRef', x, 1300), inp(call, 'self'))
            store = create('set self.' + name, x, 700)
            link(out(call), inp(store, name))
            link(out(last, 'then'), inp(store, 'execute'))
            last = store
        # Whole struct variables keep the AnimGraph pins simple and inspectable.
        for i, (name, fn, axis, source) in enumerate([
                ('StoveDoorRotation', 'MakeRotator', 'Yaw', 'StoveDoorAngle'),
                ('StoveGaugeRotation', 'MakeRotator', 'Roll', None),
                ('StoveBurnTranslation', 'MakeVector', 'Z', 'StoveBurn01')]):
            x = -1500 + i * 400
            make = create('StoveComponent.GetGaugeRotation' if source is None else 'KismetMathLibrary.' + fn, x, 1100)
            if source:
                scalar = get(source, x, 1500)
            else:
                link(get('StoveRef', x, 1500), inp(make, 'self'))
                scalar = None
            if name == 'StoveBurnTranslation':
                multiply = create('KismetMathLibrary.Multiply_DoubleDouble', x, 1300)
                link(scalar, inp(multiply, 'A'))
                assert P.set_pin_value(inp(multiply, 'B'), '22.0')
                scalar = out(multiply)
            if scalar:
                link(scalar, inp(make, axis))
            store = create('set self.' + name, x, 700)
            link(out(make), inp(store, name))
            link(out(last, 'then'), inp(store, 'execute'))
            last = store
        event = next(n for n in unreal.ObjectIterator(unreal.K2Node_Event)
                     if n.get_outer() == graph and L.get_node_reference(n) == 'entry self.BlueprintUpdateAnimation')
        event_pin = out(event, 'then')
        old = list(P.list_connected_pins(event_pin))
        assert len(old) <= 1, 'Unexpected update fan-out; wire stove update manually.'
        P.break_pin_links(event_pin)
        link(event_pin, inp(cache, 'execute'))
        for pin in old:
            link(out(last, 'then'), pin)
            link(out(branch, 'else'), pin)
        L.set_node_comment(branch, 'STOVE_PHASE4_UPDATE')
    L.compile_blueprint(bp)
    assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    if not unreal.StoveEditorLibrary.configure_animation_and_door_body():
        raise RuntimeError('Stove pose setup needs manual checklist: expected one ComponentToLocalSpace node and the named PhysicsAsset.')
    L.compile_blueprint(bp)
    assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    assert unreal.EditorAssetLibrary.save_asset('/Game/Assets/Balloon/SK_Balloon_PhysicsAsset')
    balloon = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
    L.compile_blueprint(balloon)
    assert balloon.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    assert unreal.EditorAssetLibrary.save_loaded_asset(balloon)
    unreal.log('STOVE_PHASE4_SAVED: fit query boxes and verify gauge axis before PIE.')


def main():
    required = ('StoveComponent', 'StoveEditorLibrary', 'BlueprintGraphPinLibrary')
    if any(not hasattr(unreal, name) for name in required):
        raise RuntimeError('Compile C++ and restart editor. See docs/tech/stove.md for manual setup.')
    configure_fuel_assets()
    with unreal.ScopedEditorTransaction('Configure Phase 4 stove'):
        configure_stove_blueprint()
        configure_animation()


if __name__ == '__main__':
    main()
