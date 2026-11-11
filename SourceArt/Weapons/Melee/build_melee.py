"""
Dead Country - melee weapons (cricket bat, machete), built procedurally in Blender at real scale.

Convention for every melee mesh (so the game code needs no per-weapon offsets):
  origin  = centre of the dominant hand's grip
  +X      = along the weapon toward the striking end
  +Z      = the striking face / edge direction ("up" when held for a downward swing)

Run:
  blender -b --factory-startup -P build_melee.py -- <out_dir>
"""

import bpy
import bmesh
import math
import os
import sys
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT_DIR = argv[0] if argv else os.path.dirname(os.path.abspath(__file__))
EXPORT_DIR = os.path.join(OUT_DIR, "Export")
PREVIEW_DIR = os.path.join(OUT_DIR, "Previews")
os.makedirs(EXPORT_DIR, exist_ok=True)
os.makedirs(PREVIEW_DIR, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def material(name, color, metallic, roughness):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = roughness
    m.diffuse_color = (*color, 1.0)
    m.metallic = metallic
    m.roughness = roughness
    return m


M_WILLOW = material("M_Melee_Willow", (0.78, 0.62, 0.40), 0.0, 0.55)
M_GRIP = material("M_Melee_RubberGrip", (0.06, 0.18, 0.09), 0.0, 0.8)
M_STEEL = material("M_Melee_Steel", (0.55, 0.56, 0.58), 1.0, 0.32)
M_HANDLE = material("M_Melee_Polymer", (0.03, 0.03, 0.03), 0.0, 0.6)


def obj_from_bm(name, bm, mat):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    ob.data.materials.append(mat)
    return ob


def prism_xz(name, pts, y0, y1, mat):
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in pts]
    b = [bm.verts.new((x, y1, z)) for x, z in pts]
    bm.faces.new(a)
    bm.faces.new(b[::-1])
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        bm.faces.new((a[i], a[j], b[j], b[i]))
    return obj_from_bm(name, bm, mat)


def prism_yz(name, pts, x0, x1, mat):
    """Extrude a (y, z) cross-section along X."""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in pts]
    b = [bm.verts.new((x1, y, z)) for y, z in pts]
    bm.faces.new(a[::-1])
    bm.faces.new(b)
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        bm.faces.new((a[i], b[i], b[j], a[j]))
    return obj_from_bm(name, bm, mat)


def lathe_x(name, profile, mat, segs=24):
    bm = bmesh.new()
    verts = [bm.verts.new((x, 0.0, r)) for x, r in profile]
    edges = [bm.edges.new((verts[i], verts[i + 1])) for i in range(len(verts) - 1)]
    bmesh.ops.spin(bm, geom=verts + edges, cent=(0, 0, 0), axis=(1, 0, 0), angle=2 * math.pi, steps=segs, use_duplicate=False)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    return obj_from_bm(name, bm, mat)


def bake(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    old = ob.data
    ob.modifiers.clear()
    ob.data = me
    bpy.data.meshes.remove(old)


def bevel(ob, width, segs=3, angle=35):
    m = ob.modifiers.new("Bevel", 'BEVEL')
    m.width = width
    m.segments = segs
    m.limit_method = 'ANGLE'
    m.angle_limit = math.radians(angle)
    m.use_clamp_overlap = True
    bake(ob)


def smooth(ob, angle=40):
    for p in ob.data.polygons:
        p.use_smooth = True
    ob.data.use_auto_smooth = True
    ob.data.auto_smooth_angle = math.radians(angle)


def join(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name
    ob.data.name = name
    return ob


# ---------------------------------------------------------------------------
# Cricket bat (mm). Handle 300mm, blade 560mm, blade width 108mm, max depth 65mm.
# Origin at the middle of the handle; +X toward the toe; +Z = hitting face side.
# ---------------------------------------------------------------------------
HANDLE_START, HANDLE_END = -150.0, 150.0
handle = lathe_x("BatHandle", [(HANDLE_START - 4, 0.0), (HANDLE_START - 4, 15.0), (HANDLE_START, 17.5),
                               (HANDLE_END - 20, 16.5), (HANDLE_END, 18.0), (HANDLE_END + 1, 0.0)], M_GRIP, 28)
smooth(handle, 60)
# Blade cross-section: flat face (+Z), a pronounced spine ridge on the back (-Z), chamfered edges.
blade_xsec = [(-54, 12), (-48, 20), (48, 20), (54, 12), (54, -2), (30, -24), (8, -42), (-8, -42), (-30, -24), (-54, -2)]
blade = prism_yz("BatBlade", blade_xsec, HANDLE_END - 10, HANDLE_END + 560, M_WILLOW)
bevel(blade, 4.0, 3, 30)
# Shoulders (where handle meets blade) and a rounded toe.
shoulder = prism_xz("BatShoulder", [(HANDLE_END - 40, 18), (HANDLE_END + 10, 20), (HANDLE_END + 10, -42), (HANDLE_END - 40, -14)], -54, 54, M_WILLOW)
bevel(shoulder, 6.0, 4, 30)
toe_cut = prism_xz("ToeCut", [(HANDLE_END + 540, 40), (HANDLE_END + 600, 40), (HANDLE_END + 600, -60), (HANDLE_END + 560, -60)], -70, 70, M_WILLOW)
mod = blade.modifiers.new("Toe", 'BOOLEAN')
mod.operation = 'DIFFERENCE'
mod.solver = 'EXACT'
mod.object = toe_cut
bake(blade)
bpy.data.objects.remove(toe_cut)
smooth(blade, 35)
smooth(shoulder, 35)
bat = join([blade, shoulder, handle], "SM_Melee_CricketBat")

# ---------------------------------------------------------------------------
# Machete (mm). Handle 130mm, blade 450mm, blade 50mm tall at the belly, 2.5mm thick.
# Origin at the handle centre; +X toward the tip; +Z = the cutting edge.
# ---------------------------------------------------------------------------
mhandle = prism_yz("MacheteHandle", [(-11, -14), (11, -14), (13, 10), (8, 16), (-8, 16), (-13, 10)], -65, 70, M_HANDLE)
bevel(mhandle, 3.0, 3, 30)
smooth(mhandle, 40)
guard = prism_yz("MacheteGuard", [(-14, -18), (14, -18), (14, 22), (-14, 22)], 66, 74, M_HANDLE)
bevel(guard, 1.5, 2, 30)
# Blade outline in X/Z: spine along the bottom (-Z), edge sweeping up to a belly then a clipped tip.
outline = [(70, -14), (500, -6), (522, 4), (518, 30), (430, 40), (250, 34), (70, 22)]
mblade = prism_xz("MacheteBlade", outline, -1.25, 1.25, M_STEEL)
# Bevelled edge: thin the top (cutting) side
edge_cut = prism_yz("EdgeCut", [(-4, 26), (4, 26), (4, 44), (1.25, 44), (0.15, 30), (-0.15, 30), (-1.25, 44), (-4, 44)], 60, 540, M_STEEL)
mod = mblade.modifiers.new("Edge", 'BOOLEAN')
mod.operation = 'DIFFERENCE'
mod.solver = 'EXACT'
mod.object = edge_cut
bake(mblade)
bpy.data.objects.remove(edge_cut)
smooth(mblade, 30)
machete = join([mblade, guard, mhandle], "SM_Melee_Machete")

# ---------------------------------------------------------------------------
# mm -> m, UVs, export
# ---------------------------------------------------------------------------
S = Matrix.Scale(0.001, 4)
for ob in (bat, machete):
    ob.data.transform(S)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004)
    bpy.ops.object.mode_set(mode='OBJECT')

bpy.context.view_layer.update()
for ob in (bat, machete):
    ob.data.calc_loop_triangles()
    d = ob.dimensions * 1000
    print(f"[MELEE] {ob.name}: {len(ob.data.loop_triangles)} tris, {d.x:.0f} x {d.y:.0f} x {d.z:.0f} mm")
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(EXPORT_DIR, ob.name + ".fbx"), use_selection=True,
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', object_types={'MESH'},
                             mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='STRIP')

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "Melee.blend"))

# Preview
machete.location = (0.0, 0.0, -0.18)
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_cavity = True
scene.render.resolution_x = 1600
scene.render.resolution_y = 700
w = bpy.data.worlds.new("W")
w.color = (0.55, 0.55, 0.57)
scene.world = w
cam_data = bpy.data.cameras.new("Cam")
cam_data.type = 'ORTHO'
cam_data.ortho_scale = 0.95
cam = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
target = Vector((0.33, 0.0, -0.08))
d = Vector((-0.35, 1.0, 0.45)).normalized()
cam.location = target + d * 1.5
cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
scene.render.filepath = os.path.join(PREVIEW_DIR, "melee.png")
bpy.ops.render.render(write_still=True)
print("[MELEE] done")
