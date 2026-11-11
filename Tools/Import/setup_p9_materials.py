"""
Builds a simple PBR master material (/Game/Weapons/Materials/M_WeaponMaster) and points the
P9's imported material instances at it with the colours/metal/roughness from build_p9.py.
Re-run after any re-import of the P9 FBX.
"""

import unreal

MAT_DIR = "/Game/Weapons/Materials"
P9_DIR = "/Game/Weapons/P9"
MASTER = f"{MAT_DIR}/M_WeaponMaster"

# name: (base colour, metallic, roughness) - matches build_p9.py
P9_MATS = {
    "M_P9_Polymer": ((0.018, 0.018, 0.02), 0.0, 0.62),
    "M_P9_SlideNitride": ((0.03, 0.031, 0.034), 0.9, 0.38),
    "M_P9_BarrelNitride": ((0.11, 0.11, 0.115), 0.95, 0.3),
    "M_P9_Steel": ((0.07, 0.07, 0.075), 0.9, 0.42),
    "M_P9_SpringSteel": ((0.25, 0.25, 0.26), 1.0, 0.35),
    "M_P9_SightDot": ((0.85, 0.85, 0.82), 0.0, 0.5),
    "M_P9_Brass": ((0.78, 0.55, 0.22), 1.0, 0.25),
    "M_P9_Copper": ((0.72, 0.38, 0.2), 1.0, 0.3),
}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

if eal.does_asset_exist(MASTER):
    master = eal.load_asset(MASTER)
else:
    master = tools.create_asset("M_WeaponMaster", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    bc = mel.create_material_expression(master, unreal.MaterialExpressionVectorParameter, -500, -100)
    bc.set_editor_property("parameter_name", "BaseColor")
    bc.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    mel.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for i, (pname, default, prop) in enumerate((("Metallic", 0.0, unreal.MaterialProperty.MP_METALLIC),
                                                ("Roughness", 0.5, unreal.MaterialProperty.MP_ROUGHNESS))):
        s = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -500, 120 + i * 120)
        s.set_editor_property("parameter_name", pname)
        s.set_editor_property("default_value", default)
        mel.connect_material_property(s, "", prop)
    mel.recompile_material(master)
    eal.save_loaded_asset(master)
unreal.log(f"[P9MAT] master: {MASTER}")

for name, (col, metal, rough) in P9_MATS.items():
    path = f"{P9_DIR}/{name}"
    if not eal.does_asset_exist(path):
        unreal.log_warning(f"[P9MAT] missing {path}")
        continue
    mi = eal.load_asset(path)
    old_parent = mi.get_editor_property("parent")
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(col[0], col[1], col[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    unreal.log(f"[P9MAT] {name}: parent {old_parent.get_name() if old_parent else None} -> M_WeaponMaster")

unreal.SystemLibrary.quit_editor()
