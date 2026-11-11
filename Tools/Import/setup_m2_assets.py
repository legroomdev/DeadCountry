"""
Milestone 2 asset setup for Dead Country:
  - imports SourceArt/Audio/*.wav into /Game/Audio (re-run after replacing a WAV)
  - imports the spent casing mesh into /Game/Weapons/P9
  - builds simple VFX materials in /Game/VFX (muzzle flash, bullet hole + blood decals)
  - makes a zombie skin material instance from the Epic mannequin material

Run headless:
  UnrealEditor-Cmd.exe DeadCountry.uproject -ExecutePythonScript="Tools/Import/setup_m2_assets.py" -unattended -nullrhi
"""

import glob
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[DCM2] " + msg)


# ---------------------------------------------------------------------------
# Audio
# ---------------------------------------------------------------------------
wavs = sorted(glob.glob(os.path.join(PROJECT_DIR, "SourceArt", "Audio", "*.wav")))
tasks = []
for wav in wavs:
    t = unreal.AssetImportTask()
    t.filename = wav
    t.destination_path = "/Game/Audio"
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
log(f"imported {len(wavs)} sounds into /Game/Audio")

# ---------------------------------------------------------------------------
# Casing mesh
# ---------------------------------------------------------------------------
opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = False
opts.import_materials = False
opts.import_textures = False
opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
opts.static_mesh_import_data.combine_meshes = True
opts.static_mesh_import_data.auto_generate_collision = True
opts.static_mesh_import_data.generate_lightmap_u_vs = False
opts.static_mesh_import_data.convert_scene = True
t = unreal.AssetImportTask()
t.filename = os.path.join(PROJECT_DIR, "SourceArt", "Weapons", "P9", "Export", "SM_P9_Casing.fbx")
t.destination_path = "/Game/Weapons/P9"
t.destination_name = "SM_P9_Casing"
t.automated = True
t.replace_existing = True
t.save = True
t.options = opts
tools.import_asset_tasks([t])
casing = eal.load_asset("/Game/Weapons/P9/SM_P9_Casing")
brass = eal.load_asset("/Game/Weapons/P9/M_P9_Brass")
if casing and brass:
    casing.set_material(0, brass)
    eal.save_loaded_asset(casing)
    b = casing.get_bounding_box()
    log(f"casing imported, size {(b.max - b.min).x:.2f} x {(b.max - b.min).y:.2f} cm")
else:
    unreal.log_warning("[DCM2] casing import failed")


# ---------------------------------------------------------------------------
# VFX materials
# ---------------------------------------------------------------------------
def new_material(name, folder="/Game/VFX"):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def radial_mask(mat, power, x=-900, y=0):
    """1 at the UV centre fading to 0 at the edge, raised to `power`."""
    tc = expr(mat, unreal.MaterialExpressionTextureCoordinate, x, y)
    half = expr(mat, unreal.MaterialExpressionConstant2Vector, x, y + 120)
    half.set_editor_property("r", 0.5)
    half.set_editor_property("g", 0.5)
    sub = expr(mat, unreal.MaterialExpressionSubtract, x + 180, y)
    mel.connect_material_expressions(tc, "", sub, "A")
    mel.connect_material_expressions(half, "", sub, "B")
    length = expr(mat, unreal.MaterialExpressionLength, x + 340, y)
    mel.connect_material_expressions(sub, "", length, "")
    mul = expr(mat, unreal.MaterialExpressionMultiply, x + 480, y)
    mel.connect_material_expressions(length, "", mul, "A")
    mul.set_editor_property("const_b", 2.0)
    one_minus = expr(mat, unreal.MaterialExpressionOneMinus, x + 620, y)
    mel.connect_material_expressions(mul, "", one_minus, "")
    sat = expr(mat, unreal.MaterialExpressionSaturate, x + 760, y)
    mel.connect_material_expressions(one_minus, "", sat, "")
    pw = expr(mat, unreal.MaterialExpressionPower, x + 900, y)
    mel.connect_material_expressions(sat, "", pw, "Base")
    pw.set_editor_property("const_exponent", power)
    return pw


def colour(mat, rgb, x, y):
    c = expr(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return c


# Muzzle flash: unlit additive hot core
m = new_material("M_MuzzleFlash")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property("two_sided", True)
mask = radial_mask(m, 2.5)
hot = colour(m, (9.0, 4.2, 1.3), -200, 250)
mul = expr(m, unreal.MaterialExpressionMultiply, 100, 100)
mel.connect_material_expressions(hot, "", mul, "A")
mel.connect_material_expressions(mask, "", mul, "B")
mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m)


def decal_material(name, rgb, roughness, opacity_power, opacity_scale):
    d = new_material(name)
    d.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    d.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    base = colour(d, rgb, -200, 250)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = expr(d, unreal.MaterialExpressionConstant, -200, 380)
    r.set_editor_property("r", roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    msk = radial_mask(d, opacity_power)
    scale = expr(d, unreal.MaterialExpressionMultiply, 100, 0)
    mel.connect_material_expressions(msk, "", scale, "A")
    scale.set_editor_property("const_b", opacity_scale)
    mel.connect_material_property(scale, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(d)
    eal.save_loaded_asset(d)


decal_material("M_Decal_BulletHole", (0.012, 0.010, 0.008), 0.95, 1.6, 0.95)
decal_material("M_Decal_Blood", (0.11, 0.006, 0.004), 0.25, 0.7, 0.9)
log("VFX materials built in /Game/VFX")

# ---------------------------------------------------------------------------
# Zombie skin: a sickly tint on the Epic mannequin material
# ---------------------------------------------------------------------------
base_mi = eal.load_asset("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New")
if base_mi:
    vec_names = [str(n) for n in mel.get_vector_parameter_names(base_mi)]
    scal_names = [str(n) for n in mel.get_scalar_parameter_names(base_mi)]
    log(f"mannequin vector params: {vec_names}")
    log(f"mannequin scalar params: {scal_names}")
    path = "/Game/Characters/Zombie/MI_Zombie_Skin"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    zmi = tools.create_asset("MI_Zombie_Skin", "/Game/Characters/Zombie", unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(zmi, base_mi)
    for n in vec_names:
        low = n.lower()
        if "tint" in low or "color" in low or "colour" in low:
            mel.set_material_instance_vector_parameter_value(zmi, n, unreal.LinearColor(0.32, 0.36, 0.26, 1.0))
            log(f"zombie tint applied to '{n}'")
    mel.update_material_instance(zmi)
    eal.save_loaded_asset(zmi)
else:
    unreal.log_warning("[DCM2] mannequin material not found")

for folder in ("/Game/Audio", "/Game/VFX", "/Game/Weapons/P9", "/Game/Characters/Zombie"):
    eal.save_directory(folder)
log("done")
unreal.SystemLibrary.quit_editor()
