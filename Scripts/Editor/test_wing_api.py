"""Transient API regression checks; does not modify assets or start PIE."""
import unreal

def near(actual, expected):
    assert abs(actual-expected)<.001, (actual, expected)

f=unreal.BalloonFlightComponent()
f.set_target_speed(200)
near(f.get_effective_target_speed(),200)
near(f.get_effective_turn_input(),0)
f.set_flight_modifier('Left',-1,.8)
near(f.get_effective_turn_input(),-1)
near(f.get_effective_target_speed(),160)
f.set_turn_input(0)
near(f.get_effective_turn_input(),-1)
f.set_flight_modifier('Right',1,.8)
near(f.get_effective_turn_input(),0)
near(f.get_effective_target_speed(),128)
near(f.get_target_speed(),200)
f.set_turn_input(.5)
near(f.get_effective_turn_input(),.5)
f.set_flight_modifier('Right',9,-3)
near(f.get_effective_target_speed(),50)
near(f.get_effective_turn_input(),.5)
f.clear_flight_modifier('Left')
near(f.get_effective_turn_input(),1)
f.clear_flight_modifier('Right')
near(f.get_effective_target_speed(),200)
f.set_turn_input(0)
near(f.get_effective_turn_input(),0)

l=unreal.BalloonWingLeverComponent()
assert l.get_num_states()==4
for n in [2,6,4]:
    l.set_editor_property('num_states',n)
    l.regenerate_states()
    assert l.get_num_states()==n
    states=l.get_editor_property('states')
    near(states[0].wing_angle,0)
    near(states[-1].wing_angle,90)
    near(states[0].lever_angle,-20)
    near(states[-1].lever_angle,70)
    near(states[0].turn_input,0)
    near(states[-1].turn_input,1)
    near(states[0].speed_multiplier,1)
    near(states[-1].speed_multiplier,.8)
    l.set_state(0)
    sequence=[l.get_state()]
    for _ in range(2*(n-1)):
        l.step_state()
        sequence.append(l.get_state())
    assert sequence==list(range(n))+list(range(n-2,-1,-1)), sequence
l.set_editor_property('step_mode',unreal.WingLeverStepMode.CYCLE)
l.set_state(3)
l.step_state()
assert l.get_state()==0
l.set_state(1000)
assert l.get_state()==3
l.set_state(-1000)
assert l.get_state()==0
print('WING_API_TEST_PASS: modifiers, keyboard coexistence, source replacement/cleanup, clamping, 2/4/6 states, PingPong, Cycle')
