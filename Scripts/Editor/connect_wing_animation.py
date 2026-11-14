"""Connect cached wing components to existing ABP rotations (Unreal Editor)."""
import unreal

def main():
    asset = unreal.load_asset('/Game/Assets/Balloon/ABP_Balloon')
    if unreal.BlueprintEditorLibrary.get_member_variable_type(asset, 'WingLeverLRef'):
        print('WING_ANIMATION_ALREADY_CONNECTED')
        return
    L=unreal.BlueprintEditorLibrary
    P=unreal.BlueprintGraphPinLibrary
    bp=unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
    b=unreal.load_asset('/Game/Assets/Balloon/ABP_Balloon')
    g=L.find_event_graph(b)
    already_connected = False
    for suffix in ['L','R']:
        if not already_connected:
            assert L.add_member_variable(b,'WingLever'+suffix+'Ref',L.get_object_reference_type(unreal.BalloonWingLeverComponent.static_class()))
        for name in ['WingLeverAngle'+suffix,'WingAngle'+suffix]:
            if L.get_member_variable_type(b,name):
                L.change_member_variable_type(b,name,L.get_basic_type_by_name('real'))
            else:
                assert L.add_member_variable(b,name,L.get_basic_type_by_name('real'))

    def create(ref,x,y):
        n=L.create_node_from_reference(g,ref,unreal.Vector2D(x,y))
        assert n, ref
        return n

    def inp(n,name): return L.find_input_pin(n,name)
    def out(n,name='ReturnValue'): return L.find_output_pin(n,name)
    def link(a,z): assert P.try_create_connection(a,z), 'Cannot connect pins'
    def value(p,v): assert P.set_pin_value(p,v), str(v)
    def get(name,x,y):
        n=create('self.'+name,x,y)
        return out(n,name)
    def setter(name,x,y): return create('set self.'+name,x,y)
    def valid_pair(x,y):
        both=create('KismetMathLibrary.BooleanAND',x+350,y)
        for i,suffix in enumerate(['L','R']):
            valid=create('KismetSystemLibrary.IsValid',x,y+i*130)
            link(get('WingLever'+suffix+'Ref',x-300,y+i*130),inp(valid,'Object'))
            link(out(valid),inp(both,'A' if i==0 else 'B'))
        return out(both)

    with unreal.ScopedEditorTransaction('Connect cached wing components to balloon animation'):
        if not already_connected:
            branch=create('|IfThenElse',-2700,-1600)
            link(valid_pair(-3400,-1300),inp(branch,'Condition'))
            owner=create('AnimInstance.GetOwningActor',-3400,-850)
            last=None
            for i,suffix in enumerate(['L','R']):
                find=create('BalloonWingLeverComponent.FindWingLever',-2700+i*450,-950)
                link(out(owner),inp(find,'Actor'))
                value(inp(find,'InSide'),'Left' if suffix=='L' else 'Right')
                cache=setter('WingLever'+suffix+'Ref',-2700+i*450,-600)
                link(out(find),inp(cache,'WingLever'+suffix+'Ref'))
                link(out(branch,'else') if last is None else out(last,'then'),inp(cache,'execute'))
                last=cache
            resolved=create('|IfThenElse',-1750,-600)
            link(out(last,'then'),inp(resolved,'execute'))
            link(valid_pair(-2400,-300),inp(resolved,'Condition'))
            angle_first=None
            angle_last=None
            for i,(suffix,kind) in enumerate([('L','Lever'),('R','Lever'),('L','Wing'),('R','Wing')]):
                name=('WingLeverAngle' if kind=='Lever' else 'WingAngle')+suffix
                fn=create('BalloonWingLeverComponent.Get'+kind+'Angle',-1400+i*420,-1150)
                link(get('WingLever'+suffix+'Ref',-1400+i*420,-1300),inp(fn,'self'))
                store=setter(name,-1400+i*420,-1600)
                link(out(fn),inp(store,name))
                if angle_first is None: angle_first=store
                if angle_last: link(out(angle_last,'then'),inp(store,'execute'))
                angle_last=store
            link(out(branch,'then'),inp(angle_first,'execute'))
            link(out(resolved,'then'),inp(angle_first,'execute'))
            event=next(n for n in unreal.ObjectIterator(unreal.K2Node_Event) if n.get_outer()==g and L.get_node_reference(n)=='entry self.BlueprintUpdateAnimation')
            event_then=out(event,'then')
            old=list(P.list_connected_pins(event_then))
            assert len(old)==1
            P.break_pin_links(event_then)
            link(event_then,inp(branch,'execute'))
            link(out(angle_last,'then'),old[0])
            link(out(resolved,'else'),old[0])
            L.set_node_comment(branch,'Cache by Side; reacquire only when invalid. Component angles are the source of truth.')
        # Preserve imported bone axes/signs, replacing the old normalized scalar calculations.
        for i,(name,axis,angle,sign) in enumerate([
            ('RotLeverL','Roll','WingLeverAngleL',-1),('RotLeverR','Roll','WingLeverAngleR',-1),
            ('RotStopperL','Pitch','WingAngleL',1),('RotStopperR','Pitch','WingAngleR',-1)]):
            store=next(n for n in unreal.ObjectIterator(unreal.K2Node_VariableSet) if n.get_outer()==g and L.get_node_reference(n)=='set self.'+name)
            rotation_source=P.list_connected_pins(inp(store,name))
            assert len(rotation_source)==1
            make=P.get_owning_node(rotation_source[0])
            p=inp(make,axis)
            P.break_pin_links(p)
            source=get(angle,600+i*350,-1100)
            if sign<0:
                neg=create('KismetMathLibrary.Multiply_DoubleDouble',600+i*350,-900)
                link(source,inp(neg,'A'))
                value(inp(neg,'B'),'-1.0')
                source=out(neg)
            link(source,p)
            L.set_node_comment(store,'Degrees from wing component, imported local '+axis+' axis.')
        # Disconnect only BP writers to the four legacy ABP scalar variables; preserve all other execution.
        for n in list(unreal.ObjectIterator(unreal.K2Node_VariableSet)):
            if not n.get_path_name().startswith(bp.get_path_name()): continue
            ref=L.get_node_reference(n)
            if '/ABP_Balloon.' not in ref or not any(ref.endswith('.'+x) for x in ['LeverL','LeverR','StopperL','StopperR']): continue
            incoming=list(P.list_connected_pins(inp(n,'execute')))
            outgoing=list(P.list_connected_pins(out(n,'then')))
            P.break_pin_links(inp(n,'execute'))
            P.break_pin_links(out(n,'then'))
            for a in incoming:
                for z in outgoing: link(a,z)
            L.set_node_comment(n,'Disconnected: wing component owns angles; ABP reads them directly.')
    for asset in [b,bp]:
        L.compile_blueprint(asset)
        assert asset.get_editor_property('status')==unreal.BlueprintStatus.BS_UP_TO_DATE, asset.get_editor_property('status')
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print('WING_ANIMATION_SAVED')


main()
