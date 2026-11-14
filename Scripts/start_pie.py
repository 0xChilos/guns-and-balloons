import unreal
unreal.EditorLevelLibrary.load_level('/Game/Scenes/BalloonFlight_Test')
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.editor_request_begin_play()
print("PIE started")
