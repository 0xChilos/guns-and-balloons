import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
for a in actors:
    if 'Balloon' in str(a.get_actor_label()):
        flight = a.get_components_by_class(unreal.BalloonFlightComponent)[0]
        flight.set_turn_input(1.0)
        print("TURN TEST OK turnrate=", flight.get_current_turn_rate())
        break
