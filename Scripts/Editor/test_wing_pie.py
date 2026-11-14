"""PIE regression: flight/animation transitions and actor/component interactions.
Run with py \"G:/UE/gunsandballoons/Scripts/Editor/test_wing_pie.py\".
Only temporary probe actors are created; no packages are saved.
"""
import unreal
import traceback
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ls=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
us=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not us.get_game_world(), 'Stop PIE before running this test'
base=unreal.Vector(100000,100000,100000)
cls=unreal.load_class(None,'/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C')
probes=[]
def spawn(cls,offset,label):
    a=es.spawn_actor_from_class(cls,base+offset,unreal.Rotator())
    a.set_actor_label(label)
    probes.append(a)
    return a
probe=spawn(cls,unreal.Vector(),'WingRegressionProbe')
f=probe.get_component_by_class(unreal.BalloonFlightComponent)
f.set_editor_property('initial_speed',200)
f.set_editor_property('bAutoTest',False)
f.set_editor_property('bDebugLog',False)
probe.get_component_by_class(unreal.BalloonKeyboardControlComponent).set_editor_property('bEnabled',False)
pawn=spawn(unreal.FirstPersonCameraPawn,unreal.Vector(0,0,100),'WingRegressionPawn')
pawn.set_editor_property('auto_possess_player', unreal.AutoReceiveInput.DISABLED)
crate=spawn(unreal.AmmoCrate,unreal.Vector(3000,0,100),'WingRegressionCrate')
closed=crate.get_editor_property('closed_body')
closed.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
closed.set_collision_profile_name('BlockAll')
swap=spawn(unreal.InteractSwap,unreal.Vector(5000,0,100),'WingRegressionSwap')
before=spawn(unreal.StaticMeshActor,unreal.Vector(5000,0,100),'WingRegressionBefore')
after=spawn(unreal.StaticMeshActor,unreal.Vector(5000,0,100),'WingRegressionAfter')
for a in [before,after]:
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_collision_profile_name('BlockAll')
swap.set_editor_property('before_actors',[before])
swap.set_editor_property('after_actors',[after])
state={'phase':0,'results':[]}
def near(actual,expected,tol=.02):
    assert abs(actual-expected)<tol,(actual,expected)
def passed(message):
    state['results'].append(message)
    print('WING_PIE_PASS',message)
def now(world): return unreal.GameplayStatics.get_time_seconds(world)
def cleanup():
    unreal.unregister_slate_post_tick_callback(handle)
    ls.editor_request_end_play()
    # Editor actors can only be removed after the asynchronous PIE shutdown finishes.
    def remove_editor_probes(dt):
        if us.get_game_world(): return
        try:
            for actor in probes:
                if unreal.SystemLibrary.is_valid(actor):
                    assert es.destroy_actor(actor), actor.get_actor_label()
            print('WING_PIE_CLEANUP_PASS')
        finally:
            unreal.unregister_slate_post_tick_callback(state['cleanup_handle'])
    state['cleanup_handle']=unreal.register_slate_post_tick_callback(remove_editor_probes)
def aimed_click(pawn,origin,target):
    pawn.set_actor_location(origin,False,True)
    rotation=unreal.MathLibrary.find_look_at_rotation(origin,target)
    pawn.set_actor_rotation(rotation,False)
    if pawn.get_controller(): pawn.get_controller().set_control_rotation(rotation)
    return pawn.try_interact()
def interactions(world):
    a=state['a']; p=state['pawn']; mesh=state['mesh']; l=state['l']; r=state['r']
    origin=unreal.MathLibrary.transform_location(mesh.get_world_transform(),unreal.Vector(0,0,110))
    for suffix,lever,other in [('L',l,r),('R',r,l)]:
        found=False
        stats={}
        pivot=mesh.get_socket_location('lever_'+suffix)
        for z in [0,10,20,30,40,-10]:
            for y in [0,-10,10,-20,20,-30,30,-40,40]:
                target=pivot+unreal.Vector(0,y,z)
                hit=unreal.SystemLibrary.line_trace_single(p,origin,target,unreal.TraceTypeQuery.ECC_VISIBILITY,False,[p],unreal.DrawDebugTrace.NONE,False)
                d=hit.to_dict() if hit else {}
                key=(d.get('hit_component').get_name() if d.get('hit_component') else 'None',str(d.get('hit_bone_name')))
                stats[key]=stats.get(key,0)+1
                if hit and str(hit.to_dict().get('hit_bone_name',''))=='lever_'+suffix:
                    old=lever.get_state(); other_old=other.get_state()
                    consumed=aimed_click(p,origin,target)
                    if not consumed:
                        print('CLICK_DIAG',p.get_actor_location(),p.get_actor_rotation(),p.get_controller(),p.get_editor_property('camera').get_world_location(),lever.accepts_hit(hit),lever.can_interact(p),hit.to_dict())
                    assert consumed, suffix+' click not consumed'
                    assert lever.get_state()!=old, suffix+' did not step'
                    assert other.get_state()==other_old, suffix+' changed other lever'
                    found=True
                    passed('real trace and TryInteract '+suffix)
                    break
            if found: break
        assert found, 'No lever_'+suffix+' bone hit from basket center; pivot='+str(pivot)+' origin='+str(origin)+' traces='+str(stats)+' collision='+str(a.get_actor_enable_collision())
    # Aim at the basket floor: should not consume LMB as an interaction.
    floor=unreal.MathLibrary.transform_location(mesh.get_world_transform(),unreal.Vector(30,30,0))
    assert not aimed_click(p,origin,floor), 'Basket wall/floor consumed interaction'
    passed('basket click falls through to firing')
    for label,actor,check in [('crate',state['crate'],lambda:state['crate'].is_open()),('swap',state['before'],lambda:state['swap'].has_swapped())]:
        loc=actor.get_actor_location()
        assert aimed_click(p,loc+unreal.Vector(-200,0,0),loc), label+' interaction not consumed'
        assert check(),label+' actor/owner fallback failed'
        passed(label+' actor/owner interaction regression')

def tick(dt):
    try:
        world=us.get_game_world()
        if not world: return
        t=now(world)
        if state['phase']==0:
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            def find(label): return next(a for a in actors if a.get_actor_label()==label)
            a=state['a']=find('WingRegressionProbe')
            state['pawn']=find('WingRegressionPawn')
            state['crate']=find('WingRegressionCrate')
            state['swap']=find('WingRegressionSwap')
            state['before']=find('WingRegressionBefore')
            state['f']=a.get_component_by_class(unreal.BalloonFlightComponent)
            state['mesh']=a.get_component_by_class(unreal.SkeletalMeshComponent)
            state['l']=unreal.BalloonWingLeverComponent.find_wing_lever(a,unreal.BalloonSide.LEFT)
            state['r']=unreal.BalloonWingLeverComponent.find_wing_lever(a,unreal.BalloonSide.RIGHT)
            assert state['l'] and state['r']
            near(state['f'].get_current_speed(),200)
            state['last_location']=a.get_actor_location()
            state['distance']=0.0
            state['l'].set_state(1)
            state.update(phase=1,start=t)
            return
        f=state['f']; l=state['l']; r=state['r']; mesh=state['mesh']
        location=state['a'].get_actor_location()
        state['distance']+=(location-state['last_location']).length()
        state['last_location']=location
        elapsed=t-state['start']
        if state['phase']==1 and elapsed>1:
            near(l.get_wing_angle(),30)
            near(l.get_lever_angle(),10)
            near(f.get_effective_turn_input(),-1/3)
            near(f.get_current_turn_rate(),-10)
            near(f.get_current_speed(),200*(1-(.2/3)))
            anim=mesh.get_anim_instance()
            assert anim.get_editor_property('WingLeverLRef')==l
            near(anim.get_editor_property('WingLeverAngleL'),10)
            near(anim.get_editor_property('RotLeverL').roll,-10)
            near(anim.get_editor_property('RotStopperL').pitch,30)
            passed('left intermediate detent: steady flight and ABP degrees')
            l.set_state(3)
            state.update(phase=2,start=t)
        elif state['phase']==2 and elapsed>2:
            near(l.get_wing_angle(),90); near(f.get_current_speed(),160); near(f.get_current_turn_rate(),-30)
            f.set_turn_input(1); near(f.get_effective_turn_input(),0)
            f.set_turn_input(0); near(f.get_effective_turn_input(),-1)
            near(f.get_target_speed(),200)
            passed('left full wing and independent keyboard base input')
            l.set_state(0)
            state.update(phase=3,start=t)
        elif state['phase']==3 and elapsed>.2:
            assert 0<l.get_wing_angle()<90
            before=f.get_effective_turn_input()
            l.set_state(3)
            near(f.get_effective_turn_input(),before)
            r.set_state(3)
            state.update(phase=4,start=t)
            passed('rapid retarget keeps current flight effect continuous')
        elif state['phase']==4 and elapsed>2:
            near(f.get_current_speed(),128); near(f.get_effective_turn_input(),0); near(f.get_current_turn_rate(),0)
            near(r.get_wing_angle(),90)
            anim=mesh.get_anim_instance()
            near(anim.get_editor_property('RotStopperR').pitch,-90)
            near(anim.get_editor_property('RotLeverR').roll,-70)
            passed('both wings cancel turning and multiply braking to 0.64')
            f.set_target_speed(50)
            state.update(phase=5,start=t)
        elif state['phase']==5 and elapsed>2.1:
            near(f.get_effective_target_speed(),50); near(f.get_current_speed(),50)
            passed('minimum speed clamp')
            l.set_state(0); r.set_state(0); f.set_target_speed(200)
            state.update(phase=6,start=t)
        elif state['phase']==6 and elapsed>4:
            near(f.get_effective_turn_input(),0); near(f.get_current_speed(),200)
            near(l.get_wing_angle(),0); near(r.get_wing_angle(),0)
            assert state['distance']>500, ('Flight blocked in empty space',state['distance'])
            passed('closing wings restores panel request; unobstructed travel '+str(round(state['distance']))+' cm')
            interactions(world)
            state.update(phase=7,start=t)
        elif state['phase']==7 and elapsed>2:
            l.set_state(3)
            state.update(phase=8,start=t)
        elif state['phase']==8 and elapsed>2:
            # Isolate left removal from any right-wing state changed by interaction tests.
            expected=r.get_editor_property('states')[r.get_state()].turn_input
            l.destroy_component(state["a"])
            near(f.get_effective_turn_input(),expected)
            passed('EndPlay clears the removed component modifier')
            print('WING_PIE_TEST_PASS',len(state['results']),'checks')
            cleanup()
    except Exception:
        unreal.log_error('WING_PIE_TEST_FAILED '+traceback.format_exc())
        cleanup()
handle=unreal.register_slate_post_tick_callback(tick)
ls.editor_request_begin_play()



