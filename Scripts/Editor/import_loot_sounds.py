"""Import the generated loot sounds (Content/Audio/Sounds/*.wav, made by Scripts/Audio/generate_loot_sounds.py)
as SoundWaves into /Game/Audio/Sounds. Re-running re-imports them in place.
Run in the editor: py "<this file>".
"""
import glob
import os
import unreal

DEST = '/Game/Audio/Sounds'
SRC_DIR = os.path.join(unreal.Paths.project_content_dir(), 'Audio', 'Sounds')

tasks = []
for path in sorted(glob.glob(os.path.join(SRC_DIR, '*.wav'))):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(path)
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    unreal.log('LOOT_SOUNDS imported %s -> %s' % (os.path.basename(task.filename), list(task.imported_object_paths)))
