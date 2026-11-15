"""Export the enemy models to FBX for Unreal, one FBX per part (hit zone or moving part).
Run headless, once per file:  Blender -b <Model>.blend --python export_enemies.py
Parts in model space keep the model origin; moving parts (props, gun) get their pivot as origin."""
import os
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(bpy.data.filepath)
MODEL = os.path.splitext(os.path.basename(bpy.data.filepath))[0]

# part name -> (source objects, pivot object or None = model origin)
PARTS = {
    "CropDuster": {
        "SM_CropDuster_Body": (["SM_CropDuster_Fuselage", "SM_CropDuster_Wings", "SM_CropDuster_Trim", "SM_CropDuster_Struts",
                                "SM_CropDuster_Wheels", "SM_CropDuster_Details", "SM_CropDuster_Hatch", "SM_CropDuster_TurretBase"], None),
        "SM_CropDuster_Engine": (["SM_CropDuster_Engine"], None),
        "SM_CropDuster_Prop": (["SM_CropDuster_Prop"], "SM_CropDuster_Prop"),
        "SM_CropDuster_Gun": (["SM_CropDuster_Gun", "SM_CropDuster_GunLens"], "SM_CropDuster_Gun"),
    },
    "Bomber": {
        "SM_Bomber_Body": (["SM_Bomber_Fuselage", "SM_Bomber_Trim", "SM_Bomber_Wings", "SM_Bomber_TrimRed", "SM_Bomber_Racks"], None),
        "SM_Bomber_EngineNose": (["SM_Bomber_EngineNose"], None),
        "SM_Bomber_EngineL": (["SM_Bomber_EngineL", "SM_Bomber_EngineL_Exhaust"], None),
        "SM_Bomber_EngineR": (["SM_Bomber_EngineR", "SM_Bomber_EngineR_Exhaust"], None),
        "SM_Bomber_PropNose": (["SM_Bomber_Prop_Nose"], "SM_Bomber_Prop_Nose"),
        "SM_Bomber_PropWing": (["SM_Bomber_Prop_L"], "SM_Bomber_Prop_L"),
    },
    "Mine": {
        "SM_Mine_Body": (["SM_Mine_Body", "SM_Mine_Band", "SM_Mine_Horns"], None),
        "SM_Mine_Parachute": (["SM_Mine_Canopy", "SM_Mine_CanopyRed", "SM_Mine_Lines"], None),
    },
    "Rocket": {
        "SM_Rocket_Body": (["SM_Rocket_Body", "SM_Rocket_Nose", "SM_Rocket_Brass", "SM_Rocket_Fins", "SM_Rocket_Nozzle"], None),
        "SM_Rocket_Flame": (["SM_Rocket_Flame"], None),
    },
}[MODEL]

scene = bpy.context.scene
out_dir = os.path.join(HERE, "FBX")
os.makedirs(out_dir, exist_ok=True)
pivots = {}
for part, (names, pivot_name) in PARTS.items():
    pivot = bpy.data.objects[pivot_name].matrix_world.translation.copy() if pivot_name else Vector((0, 0, 0))
    pivots[part] = pivot
    # merge copies of the source meshes, baked to world space, then moved so the pivot is the origin
    me = bpy.data.meshes.new(part)
    import bmesh
    bm = bmesh.new()
    for n in names:
        o = bpy.data.objects[n]
        tmp = o.data.copy()
        tmp.transform(Matrix.Translation(-pivot) @ o.matrix_world)
        # keep material slots: remap face material index into the merged mesh's slots
        for m in tmp.materials:
            if m.name not in me.materials:
                me.materials.append(m)
        idx = {i: list(me.materials).index(m) for i, m in enumerate(tmp.materials)}
        for p in tmp.polygons:
            p.material_index = idx.get(p.material_index, 0)
        bm.from_mesh(tmp)
        bpy.data.meshes.remove(tmp)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(part, me)
    scene.collection.objects.link(obj)
    for o in scene.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, part + ".fbx"), use_selection=True, object_types={'MESH'},
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', bake_anim=False, apply_unit_scale=True,
                             path_mode='STRIP')
    bpy.data.objects.remove(obj)
    bpy.data.meshes.remove(me)
print("EXPORTED", MODEL, {k: tuple(round(c, 3) for c in v) for k, v in pivots.items()})
