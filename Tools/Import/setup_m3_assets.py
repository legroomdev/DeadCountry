"""
Milestone 3 asset setup for Dead Country:
  - imports melee weapon meshes (SourceArt/Weapons/Melee/Export) into /Game/Weapons/Melee
  - (re)imports every WAV in SourceArt/Audio into /Game/Audio

Run headless:
  UnrealEditor-Cmd.exe DeadCountry.uproject -ExecutePythonScript="Tools/Import/setup_m3_assets.py" -unattended -nullrhi
"""

import glob
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Melee meshes
tasks = []
for fbx in sorted(glob.glob(os.path.join(PROJECT_DIR, "SourceArt", "Weapons", "Melee", "Export", "*.fbx"))):
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = True
    opts.import_textures = False
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    opts.static_mesh_import_data.combine_meshes = True
    opts.static_mesh_import_data.auto_generate_collision = True
    opts.static_mesh_import_data.convert_scene = True
    t = unreal.AssetImportTask()
    t.filename = fbx
    t.destination_path = "/Game/Weapons/Melee"
    t.destination_name = os.path.splitext(os.path.basename(fbx))[0]
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = opts
    tasks.append(t)
tools.import_asset_tasks(tasks)
for t in tasks:
    mesh = eal.load_asset(f"/Game/Weapons/Melee/{t.destination_name}")
    if mesh:
        b = mesh.get_bounding_box()
        s = b.max - b.min
        unreal.log(f"[DCM3] {t.destination_name}: {s.x:.1f} x {s.y:.1f} x {s.z:.1f} cm, min x {b.min.x:.1f}")

# Give the imported legacy-phong materials real PBR values via the weapon master material.
mel = unreal.MaterialEditingLibrary
master = eal.load_asset("/Game/Weapons/Materials/M_WeaponMaster")
MATS = {
    "M_Melee_Willow": ((0.78, 0.62, 0.40), 0.0, 0.55),
    "M_Melee_RubberGrip": ((0.06, 0.18, 0.09), 0.0, 0.8),
    "M_Melee_Steel": ((0.55, 0.56, 0.58), 1.0, 0.32),
    "M_Melee_Polymer": ((0.03, 0.03, 0.03), 0.0, 0.6),
}
for name, (col, metal, rough) in MATS.items():
    path = f"/Game/Weapons/Melee/{name}"
    if master and eal.does_asset_exist(path):
        mi = eal.load_asset(path)
        if isinstance(mi, unreal.MaterialInstanceConstant):
            mel.set_material_instance_parent(mi, master)
            mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(col[0], col[1], col[2], 1.0))
            mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
            mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi)
            unreal.log(f"[DCM3] material {name} -> M_WeaponMaster")

# Audio
wav_tasks = []
for wav in sorted(glob.glob(os.path.join(PROJECT_DIR, "SourceArt", "Audio", "*.wav"))):
    t = unreal.AssetImportTask()
    t.filename = wav
    t.destination_path = "/Game/Audio"
    t.automated = True
    t.replace_existing = True
    t.save = True
    wav_tasks.append(t)
tools.import_asset_tasks(wav_tasks)
unreal.log(f"[DCM3] imported {len(wav_tasks)} sounds")

eal.save_directory("/Game/Weapons/Melee")
eal.save_directory("/Game/Audio")
unreal.SystemLibrary.quit_editor()
