"""
Imports the P9 pistol FBX (SourceArt/Weapons/P9/Export/P9_All.fbx) into /Game/Weapons/P9
as separate static meshes, then reports what was created.

Run headless:
  UnrealEditor-Cmd.exe DeadCountry.uproject -ExecutePythonScript="Tools/Import/import_p9.py" -unattended -nullrhi
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX = os.path.join(PROJECT_DIR, "SourceArt", "Weapons", "P9", "Export", "P9_All.fbx")
DEST = "/Game/Weapons/P9"

opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = False
opts.import_materials = True
opts.import_textures = False
opts.import_animations = False
opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
sm = opts.static_mesh_import_data
sm.combine_meshes = False
sm.generate_lightmap_u_vs = True
sm.auto_generate_collision = False
sm.convert_scene = True

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.automated = True
task.replace_existing = True
task.save = True
task.options = opts

unreal.log(f"[P9IMPORT] importing {FBX}")
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

eal = unreal.EditorAssetLibrary
for path in sorted(eal.list_assets(DEST, recursive=True)):
    asset = eal.load_asset(path)
    line = f"[P9IMPORT] {path} ({asset.get_class().get_name()})"
    if isinstance(asset, unreal.StaticMesh):
        b = asset.get_bounding_box()
        size = b.max - b.min
        sockets = []
        for name in ("Muzzle", "Eject", "Grip", "Magazine", "RearSight"):
            if asset.find_socket(name):
                sockets.append(name)
        line += f" size=({size.x:.1f},{size.y:.1f},{size.z:.1f})cm min=({b.min.x:.1f},{b.min.y:.1f},{b.min.z:.1f}) sockets={sockets}"
    unreal.log(line)

eal.save_directory(DEST)
unreal.SystemLibrary.quit_editor()
