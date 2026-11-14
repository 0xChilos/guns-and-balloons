"""Read-only phase 6 editor inspection; run via Unreal MCP's py command."""
import unreal

bp = unreal.load_asset('/Game/Assets/Balloon/BP_Balloon')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    component = lib.get_object(lib.get_data(handle))
    print('COMPONENT', component.get_name(), component.get_class().get_name())
    if isinstance(component, unreal.BalloonWingLeverComponent):
        for field in ['states', 'detent_break_angle', 'max_slip_chance', 'lever_axis', 'handle_length']:
            print(field, component.get_editor_property(field))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if 'Balloon' in actor.get_class().get_name() or isinstance(actor, unreal.PlayerStart):
        print('ACTOR', actor.get_path_name(), actor.get_actor_transform())
    if 'Balloon' in actor.get_class().get_name():
        for mesh in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            print('MESH', mesh.get_name(), mesh.get_editor_property('skeletal_mesh_asset'))
            if mesh.does_socket_exist('socket_small_weapon'):
                print('SOCKET', mesh.get_socket_transform('socket_small_weapon', unreal.RelativeTransformSpace.RTS_COMPONENT))
                print('SOCKET_WORLD', mesh.get_socket_transform('socket_small_weapon'))
print('PAX_BOUNDS', unreal.load_asset('/Game/Assets/Revolver/SK_Pax').get_bounds())
