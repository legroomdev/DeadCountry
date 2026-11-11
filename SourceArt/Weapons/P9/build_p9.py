"""
Dead Country - P9 service pistol (prototype sidearm).

Generic polymer-frame, striker-fired 9x19 pistol, built procedurally at real-world scale.
All dimensions below are in millimetres; the scene is converted to metres at the end.

Coordinate system: +X = muzzle direction, +Z = up, +Y = the pistol's LEFT side.
Origin (0,0,0) = rear-bottom of the slide, on the bore's vertical plane.

Groups (collections, exported as separate FBX files):
  Frame    - polymer frame with grip texture, trigger guard, accessory rail
  Slide    - slide with serrations, ejection port, sights
  Barrel   - barrel with chamber hood, cam lug, polygonal bore
  Action   - trigger, safety blade, slide stop, takedown lever, mag catch, pins,
             locking block, recoil spring assembly, striker assembly, extractor
  Magazine - magazine body, baseplate, follower, two top cartridges

Run:
  blender -b --factory-startup -P build_p9.py -- <out_dir> [preview_dir]
"""

import bpy
import bmesh
import math
import os
import sys
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT_DIR = argv[0] if argv else os.path.dirname(os.path.abspath(__file__))
PREVIEW_DIR = argv[1] if len(argv) > 1 else os.path.join(OUT_DIR, "Previews")
EXPORT_DIR = os.path.join(OUT_DIR, "Export")
os.makedirs(EXPORT_DIR, exist_ok=True)
os.makedirs(PREVIEW_DIR, exist_ok=True)

GRIP_ANGLE = math.radians(20.5)   # magazine / grip rake from vertical
BORE_Z = 14.0                      # bore axis height above slide bottom

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

root_coll = scene.collection
COLL = {}
for name in ("Frame", "Slide", "Barrel", "Action", "Magazine", "Effects", "Sockets", "_Cutters"):
    c = bpy.data.collections.new(name)
    root_coll.children.link(c)
    COLL[name] = c

# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------
MATS = {}


def material(name, color, metallic, roughness):
    if name in MATS:
        return MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    m.diffuse_color = (*color, 1.0)
    m.metallic = metallic
    m.roughness = roughness
    MATS[name] = m
    return m


M_POLY = material("M_P9_Polymer", (0.018, 0.018, 0.02), 0.0, 0.62)
M_SLIDE = material("M_P9_SlideNitride", (0.03, 0.031, 0.034), 0.9, 0.38)
M_BARREL = material("M_P9_BarrelNitride", (0.11, 0.11, 0.115), 0.95, 0.3)
M_STEEL = material("M_P9_Steel", (0.07, 0.07, 0.075), 0.9, 0.42)
M_SPRING = material("M_P9_SpringSteel", (0.25, 0.25, 0.26), 1.0, 0.35)
M_WHITE = material("M_P9_SightDot", (0.85, 0.85, 0.82), 0.0, 0.5)
M_BRASS = material("M_P9_Brass", (0.78, 0.55, 0.22), 1.0, 0.25)
M_COPPER = material("M_P9_Copper", (0.72, 0.38, 0.2), 1.0, 0.3)

# ---------------------------------------------------------------------------
# Geometry helpers (all in mm, world space)
# ---------------------------------------------------------------------------


def to_object(name, bm, coll, mat=None):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    if mat:
        ob.data.materials.append(mat)
    return ob


def add_prism(bm, pts, axis, a0, a1):
    """Extrude a 2D polygon along an axis. axis 'Y': pts are (x,z); 'X': (y,z); 'Z': (x,y)."""
    def v(p, a):
        if axis == 'Y':
            return (p[0], a, p[1])
        if axis == 'X':
            return (a, p[0], p[1])
        return (p[0], p[1], a)
    va = [bm.verts.new(v(p, a0)) for p in pts]
    vb = [bm.verts.new(v(p, a1)) for p in pts]
    bm.faces.new(va)
    bm.faces.new(vb[::-1])
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((va[i], va[j], vb[j], vb[i]))


def add_box(bm, x0, x1, y0, y1, z0, z1, matrix=None):
    pts = [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]
    before = set(bm.verts)
    add_prism(bm, pts, 'Y', y0, y1)
    if matrix is not None:
        new = [v for v in bm.verts if v not in before]
        bmesh.ops.transform(bm, matrix=matrix, verts=new)


def add_cyl(bm, center, radius, length, axis='X', segs=24, radius2=None):
    before = set(bm.verts)
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs,
                          radius1=radius, radius2=radius if radius2 is None else radius2, depth=length)
    new = [v for v in bm.verts if v not in before]
    rot = Matrix.Identity(4)
    if axis == 'X':
        rot = Matrix.Rotation(math.pi / 2, 4, 'Y')
    elif axis == 'Y':
        rot = Matrix.Rotation(-math.pi / 2, 4, 'X')
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(center)) @ rot, verts=new)


def prism(name, pts, axis, a0, a1, coll, mat=None):
    bm = bmesh.new()
    add_prism(bm, pts, axis, a0, a1)
    return to_object(name, bm, coll, mat)


def box(name, x0, x1, y0, y1, z0, z1, coll, mat=None, matrix=None):
    bm = bmesh.new()
    add_box(bm, x0, x1, y0, y1, z0, z1, matrix)
    return to_object(name, bm, coll, mat)


def cyl(name, center, radius, length, coll, mat=None, axis='X', segs=24, radius2=None):
    bm = bmesh.new()
    add_cyl(bm, center, radius, length, axis, segs, radius2)
    return to_object(name, bm, coll, mat)


def cutter(name, build):
    """build(bm) fills a bmesh with any number of closed solids; returns the cutter object."""
    bm = bmesh.new()
    build(bm)
    return to_object(name, bm, COLL["_Cutters"])


def bake(ob):
    """Apply all modifiers (works headless, no operator context needed)."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev)
    old = ob.data
    ob.modifiers.clear()
    ob.data = me
    bpy.data.meshes.remove(old)


def boolean(ob, cut, op='DIFFERENCE'):
    # New faces inherit the cutter's material - give it the target's so cuts match the surface.
    cut.data.materials.clear()
    if ob.data.materials:
        cut.data.materials.append(ob.data.materials[0])
    mod = ob.modifiers.new("Bool", 'BOOLEAN')
    mod.operation = op
    mod.solver = 'EXACT'
    mod.use_self = True            # cutters are built from overlapping solids
    mod.use_hole_tolerant = True
    mod.object = cut
    bake(ob)
    bpy.data.objects.remove(cut, do_unlink=True)


def bevel(ob, width, segments=3, angle=35.0):
    mod = ob.modifiers.new("Bevel", 'BEVEL')
    mod.width = width
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(angle)
    mod.use_clamp_overlap = True
    bake(ob)


def smooth(ob, angle=35.0):
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


def helix_spring(name, x0, x1, cy, cz, radius, wire, pitch, coll, mat):
    """Coil spring along +X."""
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_depth = wire
    cu.bevel_resolution = 2
    cu.use_fill_caps = True
    sp = cu.splines.new('POLY')
    turns = (x1 - x0) / pitch
    steps = max(8, int(turns * 16))
    sp.points.add(steps)
    for i in range(steps + 1):
        t = i / steps
        a = t * turns * 2 * math.pi
        sp.points[i].co = (x0 + t * (x1 - x0), cy + radius * math.sin(a), cz + radius * math.cos(a), 1.0)
    tmp = bpy.data.objects.new(name + "_crv", cu)
    coll.objects.link(tmp)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(tmp.evaluated_get(dg))
    bpy.data.objects.remove(tmp, do_unlink=True)
    bpy.data.curves.remove(cu)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    ob.data.materials.append(mat)
    smooth(ob, 60)
    return ob


def lathe(name, profile, coll, mat, segs=32):
    """Revolve (x, r) profile around the X axis."""
    bm = bmesh.new()
    verts = [bm.verts.new((x, 0.0, r)) for x, r in profile]
    edges = [bm.edges.new((verts[i], verts[i + 1])) for i in range(len(verts) - 1)]
    bmesh.ops.spin(bm, geom=verts + edges, cent=(0, 0, 0), axis=(1, 0, 0),
                   angle=2 * math.pi, steps=segs, use_duplicate=False)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    ob = to_object(name, bm, coll, mat)
    smooth(ob, 50)
    return ob


def point_in_poly(x, y, poly):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def grip_frame_matrix(pivot_x, pivot_z):
    """Local space that follows the grip rake: local Z runs up the magazine axis."""
    return Matrix.Translation((pivot_x, 0, pivot_z)) @ Matrix.Rotation(GRIP_ANGLE, 4, 'Y')


MAG_PIVOT = (29.0, -106.0)   # bottom centre of the magazine body / magwell
MAG_M = grip_frame_matrix(*MAG_PIVOT)

# ===========================================================================
# FRAME
# ===========================================================================
FRAME_OUTLINE = [
    (-1.0, 0.0), (168.0, 0.0), (169.5, -2.0), (169.5, -13.0), (167.5, -14.5),
    (123.0, -14.5), (120.5, -16.5), (119.5, -42.0), (120.6, -45.0), (118.5, -48.5),
    (88.0, -48.5), (81.0, -46.5), (77.5, -44.0),
    (57.0, -101.0), (55.5, -104.5), (52.0, -106.0), (6.0, -106.0), (2.5, -104.0), (4.0, -96.0),
    (24.5, -44.0), (27.5, -34.0), (26.5, -26.0), (21.0, -20.0), (10.0, -15.0), (2.0, -12.0),
    (-2.5, -9.5), (-4.0, -6.5), (-3.6, -3.0), (-2.0, -0.5),
]
GRIP_OUTLINE = [
    (22.5, -23.0), (80.5, -36.5), (78.0, -46.0), (57.0, -101.0), (55.5, -104.5), (52.0, -106.0),
    (6.0, -106.0), (2.5, -104.0), (4.0, -96.0), (24.5, -44.0), (27.0, -33.5), (25.5, -25.5),
]
GUARD_HOLE = [
    (115.5, -17.5), (115.0, -42.5), (113.5, -44.0), (89.0, -44.0), (84.5, -41.0),
    (82.5, -36.0), (83.0, -20.0), (85.5, -17.5),
]
GRIP_PANEL = [(27.5, -48.0), (70.5, -50.0), (54.5, -95.0), (9.5, -95.0)]
FRAME_HALF = 12.9
GRIP_HALF = 14.6

frame = prism("SM_P9_Frame", FRAME_OUTLINE, 'Y', -FRAME_HALF, FRAME_HALF, COLL["Frame"], M_POLY)
bevel(frame, 1.6, 3, 40)

grip = prism("GripBulk", GRIP_OUTLINE, 'Y', -GRIP_HALF, GRIP_HALF, COLL["Frame"], M_POLY)
bevel(grip, 4.5, 5, 40)
boolean(frame, grip, 'UNION')

# Trigger guard opening
boolean(frame, cutter("c_guard", lambda bm: add_prism(bm, GUARD_HOLE, 'Y', -20, 20)))

# Accessory rail under the dust cover (one cross slot, side grooves)
rail = box("Rail", 128.0, 167.0, -9.5, 9.5, -18.5, -14.0, COLL["Frame"], M_POLY)
bevel(rail, 0.6, 2, 40)
boolean(frame, rail, 'UNION')


def rail_cuts(bm):
    for s in (-1, 1):
        add_box(bm, 126.0, 169.0, s * 8.0, s * 11.0, -17.2, -15.6)
    add_box(bm, 150.0, 154.0, -12, 12, -19.0, -16.4)


boolean(frame, cutter("c_rail", rail_cuts))


def frame_internal_cuts(bm):
    add_box(bm, 95.5, 107.5, -3.6, 3.6, -19.0, 0.5)      # trigger slot
    add_box(bm, 64.0, 108.5, -5.2, 5.2, -8.0, 0.5)       # barrel lug / locking block pocket
    add_box(bm, 104.0, 170.0, -5.6, 5.6, -6.0, 0.5)      # recoil spring channel
    add_box(bm, 49.0, 58.5, -8.0, 8.0, -108.0, -100.5)   # magwell front scallop
    # Magwell: follows the grip rake, open at the bottom
    add_box(bm, -16.6, 16.4, -11.4, 11.4, -6.0, 93.0, MAG_M)
    # Lanyard notch at the back strap base
    add_box(bm, 1.0, 7.5, -4.0, 4.0, -108.0, -101.0)


boolean(frame, cutter("c_frame_int", frame_internal_cuts))


def grip_panels(bm):
    for s in (-1, 1):
        y0, y1 = (GRIP_HALF - 0.8, GRIP_HALF + 3) if s > 0 else (-GRIP_HALF - 3, -GRIP_HALF + 0.8)
        add_prism(bm, GRIP_PANEL, 'Y', y0, y1)


boolean(frame, cutter("c_panels", grip_panels))
smooth(frame, 40)

# Stippling: raised micro-pyramids inside each recessed panel
stip = bmesh.new()
xs = [p[0] for p in GRIP_PANEL]
zs = [p[1] for p in GRIP_PANEL]
row = 0
z = min(zs) + 1.4
while z < max(zs) - 1.0:
    x = min(xs) + 1.2 + (0.95 if row % 2 else 0.0)
    while x < max(xs) - 1.0:
        # keep 1.2mm clear of the panel edge
        if all(point_in_poly(x + dx, z + dz, GRIP_PANEL) for dx, dz in ((1.2, 0), (-1.2, 0), (0, 1.2), (0, -1.2))):
            for s in (-1, 1):
                base_y = s * (GRIP_HALF - 0.8)
                before = set(stip.verts)
                bmesh.ops.create_cone(stip, cap_ends=True, cap_tris=False, segments=6,
                                      radius1=0.6, radius2=0.05, depth=0.55)
                new = [v for v in stip.verts if v not in before]
                rot = Matrix.Rotation(-math.pi / 2 * s, 4, 'X')
                bmesh.ops.transform(stip, matrix=Matrix.Translation((x, base_y + s * 0.2, z)) @ rot, verts=new)
        x += 1.9
    z += 1.65
    row += 1
stipple = to_object("FrameStipple", stip, COLL["Frame"], M_POLY)
frame = join([frame, stipple], "SM_P9_Frame")

# ===========================================================================
# SLIDE
# ===========================================================================
SLIDE_LEN = 186.0
SLIDE_HALF = 12.75
SLIDE_TOP = 28.0
slide_xsec = [(-SLIDE_HALF, 0.0), (SLIDE_HALF, 0.0), (SLIDE_HALF, 24.5), (10.5, SLIDE_TOP),
              (-10.5, SLIDE_TOP), (-SLIDE_HALF, 24.5)]
slide = prism("SM_P9_Slide", slide_xsec, 'X', 0.0, SLIDE_LEN, COLL["Slide"], M_SLIDE)
bevel(slide, 0.7, 2, 30)


def slide_cuts(bm):
    # Nose: lower front chamfer and small top chamfer
    add_prism(bm, [(186.5, 10.5), (186.5, -1.0), (173.5, -1.0)], 'Y', -20, 20)
    add_prism(bm, [(186.5, 29.0), (183.5, 29.0), (186.5, 26.5)], 'Y', -20, 20)
    # Rear cocking serrations (9) and front serrations (6), both sides
    for i in range(9):
        x = 7.0 + i * 3.6
        for s in (-1, 1):
            add_box(bm, x, x + 1.7, s * (SLIDE_HALF - 1.0), s * (SLIDE_HALF + 2), 3.0, 24.0)
    for i in range(6):
        x = 152.0 + i * 3.6
        for s in (-1, 1):
            add_box(bm, x, x + 1.7, s * (SLIDE_HALF - 1.0), s * (SLIDE_HALF + 2), 4.0, 22.0)
    # Ejection port (top window + right-side window)
    add_box(bm, 72.0, 112.0, -7.4, 6.3, 19.0, 31.0)
    add_box(bm, 76.0, 110.0, -16.0, -6.0, 14.5, 31.0)
    # Internal cavity for barrel + recoil spring, breech face at X=71.5
    add_box(bm, 71.5, 171.0, -10.6, 10.6, -1.0, 21.5)     # ends before the nose chamfer
    add_box(bm, -1.0, 71.6, -8.0, 8.0, -1.0, 6.0)          # frame rail / trigger housing channel
    add_cyl(bm, (178.0, 0, BORE_Z), 7.1, 20.0, 'X', 32)   # barrel bore through the nose
    add_cyl(bm, (176.0, 0, 5.0), 4.9, 12.0, 'X', 24)      # recoil spring pocket in the nose
    add_cyl(bm, (183.0, 0, 5.0), 2.5, 9.0, 'X', 20)       # guide rod tip hole
    add_cyl(bm, (35.0, 0, BORE_Z), 3.4, 74.0, 'X', 20)    # striker channel
    add_cyl(bm, (71.0, 0, BORE_Z), 1.1, 3.0, 'X', 12)     # firing pin hole in breech face
    # Rear cover-plate recess
    add_box(bm, -1.0, 0.5, -7.0, 7.0, 7.0, 22.0)
    # Slide-stop notch (left side, lower edge)
    add_box(bm, 74.0, 84.0, 10.8, 14.0, -1.0, 3.2)
    # Extractor pocket (right side)
    add_box(bm, 62.0, 78.0, -14.0, -11.6, 14.0, 21.5)
    # Sight dovetails
    add_box(bm, 8.5, 18.0, -9.0, 9.0, 26.5, 29.0)
    add_box(bm, 172.5, 179.5, -1.5, 1.5, 27.0, 29.0)


boolean(slide, cutter("c_slide", slide_cuts))
smooth(slide, 30)

rear_sight = prism("RearSight", [(8.0, 26.5), (18.5, 26.5), (18.5, 31.5), (16.0, 34.5), (8.0, 34.5)],
                   'Y', -9.5, 9.5, COLL["Slide"], M_SLIDE)
bevel(rear_sight, 0.5, 2, 40)
boolean(rear_sight, cutter("c_notch", lambda bm: add_box(bm, 7.0, 19.0, -1.75, 1.75, 31.2, 36.0)))
front_sight = prism("FrontSight", [(172.0, 27.0), (180.0, 27.0), (180.0, 30.0), (177.0, 33.6), (172.0, 33.6)],
                    'Y', -1.7, 1.7, COLL["Slide"], M_SLIDE)
bevel(front_sight, 0.3, 2, 40)
cover_plate = box("CoverPlate", -0.6, 0.5, -6.6, 6.6, 7.4, 21.6, COLL["Slide"], M_STEEL)
bevel(cover_plate, 0.4, 2, 40)
dots = bmesh.new()
for y in (-5.0, 5.0):
    add_cyl(dots, (7.85, y, 32.6), 0.9, 0.5, 'X', 16)
add_cyl(dots, (171.85, 0.0, 31.7), 0.85, 0.5, 'X', 16)
dot_obj = to_object("SightDots", dots, COLL["Slide"], M_WHITE)
for o in (rear_sight, front_sight, cover_plate, dot_obj):
    smooth(o, 40)
slide = join([slide, rear_sight, front_sight, cover_plate, dot_obj], "SM_P9_Slide")

# ===========================================================================
# BARREL
# ===========================================================================
MUZZLE_X = 185.3
barrel = cyl("SM_P9_Barrel", ((108.0 + MUZZLE_X) / 2, 0, BORE_Z), 6.75, MUZZLE_X - 108.0, COLL["Barrel"], M_BARREL, 'X', 40)
chamber = cyl("Chamber", (90.0, 0, BORE_Z), 7.6, 36.0, COLL["Barrel"], M_BARREL, 'X', 40)
boolean(barrel, chamber, 'UNION')
hood = box("Hood", 72.2, 111.5, -7.15, 6.05, BORE_Z, SLIDE_TOP - 0.1, COLL["Barrel"], M_BARREL)
bevel(hood, 0.8, 2, 40)
boolean(barrel, hood, 'UNION')
lug = prism("CamLug", [(74.0, 8.5), (100.0, 8.5), (100.0, 1.5), (90.0, 1.5), (82.0, -4.0), (70.0, -4.0), (66.5, 1.5), (68.0, 8.5)],
            'Y', -4.2, 4.2, COLL["Barrel"], M_BARREL)
bevel(lug, 0.6, 2, 40)
boolean(barrel, lug, 'UNION')


def barrel_cuts(bm):
    add_cyl(bm, (130.0, 0, BORE_Z), 4.5, 130.0, 'X', 6)    # hexagonal (polygonal-rifled) bore
    add_cyl(bm, (80.0, 0, BORE_Z), 4.95, 17.0, 'X', 32)    # chamber
    # Feed ramp
    add_box(bm, -4.0, 8.0, -3.8, 3.8, -6.0, 4.0,
            Matrix.Translation((72.0, 0, BORE_Z - 3.0)) @ Matrix.Rotation(math.radians(35), 4, 'Y'))
    add_box(bm, 82.0, 98.0, -5.0, 5.0, -1.0, 1.0)           # locking-block engagement slot
    # Muzzle crown
    add_cyl(bm, (MUZZLE_X, 0, BORE_Z), 6.0, 1.2, 'X', 40, 4.6)


boolean(barrel, cutter("c_barrel", barrel_cuts))
smooth(barrel, 30)

# ===========================================================================
# ACTION (separate moving / small parts)
# ===========================================================================
A = COLL["Action"]

trigger = prism("SM_P9_Trigger", [(95.5, -9.0), (103.0, -9.0), (103.6, -14.0), (105.6, -22.0), (106.6, -30.0),
                                  (106.2, -36.0), (104.8, -40.3), (101.0, -40.8), (101.4, -36.0), (101.3, -30.0),
                                  (100.2, -23.0), (97.2, -15.0)],
                'Y', -3.0, 3.0, A, M_POLY)
bevel(trigger, 0.6, 2, 40)
boolean(trigger, cutter("c_trigger_slot", lambda bm: add_box(bm, 95.0, 108.0, -1.1, 1.1, -38.0, -21.0)))
smooth(trigger, 40)
safety = prism("SM_P9_TriggerSafety", [(99.6, -23.5), (100.6, -30.0), (100.6, -36.0), (102.4, -36.0),
                                       (102.4, -30.0), (101.4, -23.0)],
               'Y', -1.0, 1.0, A, M_POLY)
bevel(safety, 0.3, 2, 40)
smooth(safety, 40)

trigger_bar = box("SM_P9_TriggerBar", 28.0, 100.0, -9.2, -8.2, -7.0, -3.5, A, M_STEEL)

slide_stop = prism("SM_P9_SlideStop", [(57.0, -1.4), (77.5, -1.4), (80.0, -3.8), (78.0, -6.6), (66.0, -6.6), (57.0, -4.4)],
                   'Y', FRAME_HALF - 0.6, FRAME_HALF + 1.3, A, M_STEEL)
bevel(slide_stop, 0.4, 2, 40)
boolean(slide_stop, cutter("c_stop_ribs", lambda bm: [add_box(bm, 70.0 + i * 1.8, 70.9 + i * 1.8, FRAME_HALF + 0.9, FRAME_HALF + 2, -7, 0) for i in range(4)]))
smooth(slide_stop, 40)

takedown = box("SM_P9_TakedownLever", 110.0, 115.0, -FRAME_HALF - 0.9, FRAME_HALF + 0.9, -6.5, -2.5, A, M_STEEL)
bevel(takedown, 0.4, 2, 40)
boolean(takedown, cutter("c_td_ribs", lambda bm: [add_box(bm, 111.0 + i * 1.4, 111.6 + i * 1.4, -20, 20, -7, -6.2) for i in range(3)]))
smooth(takedown, 40)

mag_catch = box("SM_P9_MagCatch", 74.5, 82.5, 10.0, GRIP_HALF + 1.1, -40.0, -31.5, A, M_POLY)
bevel(mag_catch, 1.0, 3, 40)
boolean(mag_catch, cutter("c_mc_ribs", lambda bm: [add_box(bm, 75.5, 81.5, GRIP_HALF + 0.7, 20, -38.8 + i * 1.8, -38.0 + i * 1.8) for i in range(4)]))
smooth(mag_catch, 40)

pins = bmesh.new()
for (x, z) in ((104.0, -5.0), (84.0, -9.0), (22.0, -13.0)):
    add_cyl(pins, (x, 0, z), 1.5, 2 * FRAME_HALF + 0.6, 'Y', 16)
pin_obj = to_object("SM_P9_Pins", pins, A, M_STEEL)
smooth(pin_obj, 40)

locking_block = box("SM_P9_LockingBlock", 100.0, 108.0, -5.0, 5.0, -7.5, 1.0, A, M_STEEL)
bevel(locking_block, 0.5, 2, 40)
smooth(locking_block, 40)

guide = cyl("SM_P9_GuideRod", (143.0, 0, 5.0), 2.2, 80.0, A, M_STEEL, 'X', 16)
flange = cyl("GuideFlange", (102.0, 0, 5.0), 4.8, 2.4, A, M_STEEL, 'X', 24)
guide = join([guide, flange], "SM_P9_GuideRod")
smooth(guide, 40)
recoil_spring = helix_spring("SM_P9_RecoilSpring", 103.5, 179.0, 0.0, 5.0, 3.9, 0.55, 4.6, A, M_SPRING)

striker = cyl("SM_P9_Striker", (52.0, 0, BORE_Z), 2.2, 38.0, A, M_STEEL, 'X', 16)
striker_tip = cyl("StrikerTip", (71.0, 0, BORE_Z), 1.0, 1.6, A, M_STEEL, 'X', 12)
striker_lug = box("StrikerLug", 33.0, 37.0, -1.4, 1.4, BORE_Z - 6.0, BORE_Z, A, M_STEEL)
striker = join([striker, striker_tip, striker_lug], "SM_P9_Striker")
smooth(striker, 40)
striker_spring = helix_spring("SM_P9_StrikerSpring", 4.0, 34.0, 0.0, BORE_Z, 2.75, 0.35, 2.2, A, M_SPRING)

extractor = box("SM_P9_Extractor", 62.4, 77.6, -SLIDE_HALF - 0.45, -11.7, 14.4, 21.1, A, M_STEEL)
bevel(extractor, 0.4, 2, 40)
smooth(extractor, 40)

# ===========================================================================
# MAGAZINE (built in grip-raked local space, then transformed)
# ===========================================================================
MAG_LEN = 100.0
mag_body = prism("SM_P9_Magazine", [(-15.5, -10.75), (15.5, -10.75), (15.5, 10.75), (-15.5, 10.75)], 'Z', 0.0, MAG_LEN,
                 COLL["Magazine"], M_POLY)
bevel(mag_body, 2.4, 3, 40)


def mag_cuts(bm):
    add_box(bm, -14.0, 14.0, -9.3, 9.3, 3.0, 96.0)        # hollow interior
    add_box(bm, -11.5, 17.0, -4.3, 4.3, 95.0, 102.0)      # feed lip opening
    add_box(bm, -17.0, -13.0, -4.3, 4.3, 92.0, 102.0)     # rear relief
    for i in range(6):                                     # witness holes on the back
        add_cyl(bm, (-15.5, 0.0, 16.0 + i * 12.5), 1.6, 4.0, 'X', 12)


boolean(mag_body, cutter("c_mag", mag_cuts))
base = prism("Baseplate", [(-18.0, -12.6), (17.0, -12.6), (19.5, -9.0), (19.5, 9.0), (17.0, 12.6), (-18.0, 12.6)],
             'Z', -7.0, 0.2, COLL["Magazine"], M_POLY)
bevel(base, 1.4, 3, 40)
boolean(base, cutter("c_base", lambda bm: add_box(bm, 16.0, 21.0, -6.0, 6.0, -8.0, -4.6)))
follower = box("Follower", -13.6, 13.6, -8.9, 8.9, 82.0, 85.0, COLL["Magazine"], M_POLY)
bevel(follower, 0.6, 2, 40)
mag = join([mag_body, base, follower], "SM_P9_Magazine")
smooth(mag, 40)
mag.data.transform(MAG_M)

# Two visible 9x19 cartridges (double stack) under the feed lips, lying level with the bore
CASE_PROFILE = [(0.0, 0.0), (0.0, 4.9), (0.9, 4.9), (1.15, 4.2), (2.0, 4.2), (2.35, 4.96), (19.15, 4.82), (19.15, 4.5), (18.0, 0.0)]
BULLET_PROFILE = [(18.4, 0.0), (18.4, 4.5), (21.0, 4.5), (24.0, 4.0), (26.8, 2.9), (28.9, 1.5), (29.7, 0.0)]
for idx, (lz, ly) in enumerate(((90.0, 2.3), (85.6, -2.3))):
    case = lathe(f"Case{idx}", CASE_PROFILE, COLL["Magazine"], M_BRASS)
    bullet = lathe(f"Bullet{idx}", BULLET_PROFILE, COLL["Magazine"], M_COPPER)
    rnd = join([case, bullet], f"SM_P9_Round{idx}")
    # Centre the round on its length, place it in magazine space, then undo the grip rake so it lies level
    place = MAG_M @ Matrix.Translation((0.5, ly, lz)) @ Matrix.Rotation(-GRIP_ANGLE, 4, 'Y') @ Matrix.Translation((-14.0, 0, 0))
    rnd.data.transform(place)

# Spent casing for shell ejection (separate asset, origin at the case head, lies along +X)
EMPTY_CASE_PROFILE = [(0.0, 0.0), (0.0, 4.9), (0.9, 4.9), (1.15, 4.2), (2.0, 4.2), (2.35, 4.96), (19.15, 4.82),
                      (19.15, 4.55), (2.8, 4.4), (2.8, 0.0)]
lathe("SM_P9_Casing", EMPTY_CASE_PROFILE, COLL["Effects"], M_BRASS, segs=20)

# ===========================================================================
# Sockets (UE imports empties named SOCKET_* as static-mesh sockets)
# ===========================================================================
SOCKETS = {
    "SOCKET_Muzzle": (MUZZLE_X + 1.0, 0.0, BORE_Z),
    "SOCKET_Eject": (92.0, -14.0, 24.0),
    "SOCKET_Grip": (40.0, 0.0, -55.0),
    "SOCKET_Magazine": (MAG_PIVOT[0], 0.0, MAG_PIVOT[1]),
    "SOCKET_RearSight": (13.0, 0.0, 34.5),
}
for name, loc in SOCKETS.items():
    e = bpy.data.objects.new(name, None)
    e.empty_display_type = 'ARROWS'
    e.empty_display_size = 6.0
    e.location = loc
    COLL["Sockets"].objects.link(e)

# ===========================================================================
# Clean-up, mm -> m, report
# ===========================================================================
bpy.data.collections.remove(COLL["_Cutters"])
S = Matrix.Scale(0.001, 4)
for ob in scene.objects:
    if ob.type == 'MESH':
        ob.data.transform(S)
        ob.location = ob.location * 0.001
    elif ob.type == 'EMPTY':
        ob.location = ob.location * 0.001
        ob.empty_display_size *= 0.001

frame_ob = bpy.data.objects["SM_P9_Frame"]
for name in SOCKETS:
    s = bpy.data.objects[name]
    s.parent = frame_ob

# UVs for texturing (Smart UV Project per part; Unreal generates its own lightmap UVs)
for ob in [o for o in scene.objects if o.type == 'MESH']:
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')

bpy.context.view_layer.update()
total_tris = 0
print("[P9] parts:")
for cname in ("Frame", "Slide", "Barrel", "Action", "Magazine"):
    for ob in COLL[cname].objects:
        if ob.type != 'MESH':
            continue
        ob.data.calc_loop_triangles()
        tris = len(ob.data.loop_triangles)
        total_tris += tris
        d = ob.dimensions * 1000
        uv = "UV" if ob.data.uv_layers else "noUV"
        print(f"[P9]   {cname:9s} {ob.name:24s} {tris:7d} tris  {d.x:6.1f} x {d.y:5.1f} x {d.z:6.1f} mm  {uv}")
print(f"[P9] total triangles: {total_tris}")

blend_path = os.path.join(OUT_DIR, "P9.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)
print("[P9] saved " + blend_path)

# ===========================================================================
# FBX export: one file per group, plus everything together
# ===========================================================================


def export(objs, path):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_UNITS', object_types={'MESH', 'EMPTY'},
                             mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False,
                             bake_anim=False, path_mode='STRIP')


for cname in ("Frame", "Slide", "Barrel", "Action", "Magazine"):
    objs = list(COLL[cname].objects)
    if cname == "Frame":
        objs += list(COLL["Sockets"].objects)
    export(objs, os.path.join(EXPORT_DIR, f"SM_P9_{cname}.fbx"))
effects = set(COLL["Effects"].objects)
export([o for o in scene.objects if o not in effects], os.path.join(EXPORT_DIR, "P9_All.fbx"))
export(list(effects), os.path.join(EXPORT_DIR, "SM_P9_Casing.fbx"))
print("[P9] exported FBX to " + EXPORT_DIR)

# ===========================================================================
# Preview renders (Workbench, cavity shading)
# ===========================================================================
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = 'BOTH'
scene.display.shading.show_specular_highlight = True
scene.display.shading.show_object_outline = False
scene.render.resolution_x = 1800
scene.render.resolution_y = 1100
scene.render.film_transparent = False
world = bpy.data.worlds.new("W")
scene.world = world
world.color = (0.55, 0.55, 0.57)
scene.display.shading.background_type = 'WORLD'
COLL["Sockets"].hide_render = True
COLL["Effects"].hide_render = True

cam_data = bpy.data.cameras.new("Cam")
cam = bpy.data.objects.new("Cam", cam_data)
root_coll.objects.link(cam)
scene.camera = cam
CENTER = Vector((0.0895, 0.0, -0.039))


def render(name, direction, ortho=True, scale=0.235, lens=85, dist=0.6, target=CENTER):
    d = Vector(direction).normalized()
    cam.location = target + d * dist
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    cam_data.type = 'ORTHO' if ortho else 'PERSP'
    cam_data.ortho_scale = scale
    cam_data.lens = lens
    cam_data.clip_end = 10
    scene.render.filepath = os.path.join(PREVIEW_DIR, name + ".png")
    bpy.ops.render.render(write_still=True)


render("p9_left", (0, 1, 0))
render("p9_right", (0, -1, 0))
render("p9_threequarter", (-0.6, 1.0, 0.45), ortho=False, lens=70, dist=0.55)
render("p9_top", (0, 0.001, 1), scale=0.215)
render("p9_front", (1, 0, 0), scale=0.16)

# Exploded view (not saved to the .blend)
offsets = {"Slide": (0, 0, 0.055), "Barrel": (0, 0, 0.11), "Magazine": (-0.03, 0, -0.06), "Action": (0, 0.0, 0.0)}
for cname, off in offsets.items():
    for ob in COLL[cname].objects:
        ob.location += Vector(off)
for ob in COLL["Action"].objects:
    if "Spring" in ob.name or "Guide" in ob.name or "Striker" in ob.name:
        ob.location += Vector((0, 0, 0.085))
render("p9_exploded", (-0.35, 1.0, 0.25), ortho=False, lens=50, dist=0.75, target=Vector((0.09, 0, -0.01)))
print("[P9] previews in " + PREVIEW_DIR)
