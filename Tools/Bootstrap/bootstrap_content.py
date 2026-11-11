"""
Dead Country - content bootstrap.

Creates starter item data assets, region definitions for each planned Australian
environment, and a greybox test map (/Game/Maps/L_TestRange) with loot, zombies,
survivor spawn points and a sun + moon for day/night.
Safe to re-run: existing assets are updated, the test map is rebuilt.

NOTE: rebuilding the map gives placed containers new identities, so an existing world
save (Saved/SaveGames/DCWorld_*.sav) will no longer match it - delete the save after.

Run headless:
  UnrealEditor-Cmd.exe DeadCountry.uproject -ExecutePythonScript="Tools/Bootstrap/bootstrap_content.py" -unattended -nullrhi
Or in the editor: Tools > Execute Python Script.
"""

import math
import unreal

ITEMS_PATH = "/Game/Items"
MAPDEF_PATH = "/Game/Maps/Definitions"
TEST_MAP = "/Game/Maps/L_TestRange"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

Cat = unreal.DCItemCategory


def get_or_create_data_asset(folder, name, cls):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return asset_tools.create_asset(name, folder, cls, factory)


# ---------------------------------------------------------------------------
# Items
# ---------------------------------------------------------------------------
ITEMS = {
    # name: (display, category, (w,h), stack, kg, $, use-effects)
    "DA_WaterBottle": ("Water Bottle", Cat.DRINK, (1, 2), 1, 0.6, 45, dict(hydration_restore=45.0)),
    "DA_Bandage": ("Bandage", Cat.MEDICAL, (1, 1), 1, 0.05, 60, dict(stops_bleeding=True)),
    "DA_Splint": ("Splint", Cat.MEDICAL, (1, 1), 1, 0.2, 90, dict(splints_fracture=True)),
    "DA_FirstAidKit": ("First Aid Kit", Cat.MEDICAL, (2, 2), 1, 0.8, 350, dict(heal_amount=80.0, stops_bleeding=True)),
    "DA_BakedBeans": ("Tin of Baked Beans", Cat.FOOD, (1, 1), 1, 0.4, 40, dict(energy_restore=35.0, hydration_restore=-5.0)),
    "DA_MeatPie": ("Servo Meat Pie", Cat.FOOD, (1, 1), 1, 0.25, 15, dict(energy_restore=25.0)),
    "DA_Ammo308": (".308 Round", Cat.AMMO, (1, 1), 40, 0.025, 8, None),
    "DA_Ammo9mm": ("9x19mm Round", Cat.AMMO, (1, 1), 50, 0.012, 4, None),
    "DA_CarBattery": ("Car Battery", Cat.BUILDING, (2, 2), 1, 13.0, 500, None),
    "DA_JerryCan": ("Jerry Can (Diesel)", Cat.TOOL, (2, 3), 1, 6.0, 300, None),
    "DA_GoldNugget": ("Gold Nugget", Cat.VALUABLE, (1, 1), 1, 0.1, 2500, None),
    "DA_Opal": ("Boulder Opal", Cat.VALUABLE, (1, 1), 1, 0.15, 1800, None),
}

items = {}
for name, (display, category, size, stack, kg, value, use) in ITEMS.items():
    a = get_or_create_data_asset(ITEMS_PATH, name, unreal.DCItemDefinition)
    a.set_editor_property("display_name", display)
    a.set_editor_property("category", category)
    a.set_editor_property("grid_size", unreal.IntPoint(size[0], size[1]))
    a.set_editor_property("max_stack", stack)
    a.set_editor_property("weight_kg", kg)
    a.set_editor_property("base_value", value)
    a.set_editor_property("consumable", use is not None)
    for prop, val in (use or {}).items():
        a.set_editor_property(prop, val)
    items[name] = a
eal.save_directory(ITEMS_PATH)
unreal.log(f"[DC] {len(items)} item definitions ready")


# ---------------------------------------------------------------------------
# Region definitions - one per planned environment (levels to be built later)
# ---------------------------------------------------------------------------
B = unreal.DCBiome
MAPS = {
    "DA_Map_CooberCreek": ("Coober Creek Roadhouse", B.RED_CENTRE, 2.0, 1800,
                           "A lonely roadhouse, airstrip and opal diggings on the Stuart Hwy. Water is worth more than gold."),
    "DA_Map_Kalgarra": ("Kalgarra", B.WHEATBELT_TOWN, 1.3, 2100,
                        "Grain silos, a pub, a footy oval and a rail siding. Open paddocks, nowhere to hide."),
    "DA_Map_ShellyHeads": ("Shelly Heads", B.COASTAL_TOWN, 1.0, 2100,
                           "Surf club, caravan park and a fishing co-op. A boat ramp, and the open sea."),
    "DA_Map_BarraCrossing": ("Barra Crossing", B.TROPICAL_NORTH, 1.6, 1800,
                             "Cane fields, mangroves and a flooded causeway. Wet season storms, crocs in the creek."),
    "DA_Map_MtWallace": ("Mt Wallace Ski Village", B.SNOWY_MOUNTAINS, 0.5, 2400,
                         "Alpine lodges, chairlifts and a hydro dam. Cold drains energy instead of water."),
    "DA_Map_Ironvale": ("Ironvale", B.MINING_TOWN, 1.8, 2400,
                        "Company town, haul roads, ore trains and an open-cut pit. High-tier industrial loot."),
    "DA_Map_WattleVale": ("Wattle Vale", B.REGIONAL_CITY, 1.0, 2700,
                          "Hospital, shopping centre and suburbs. Densest horde, best medical loot."),
    "DA_Map_HuonHollow": ("Huon Hollow", B.TASMANIAN_WILDERNESS, 0.7, 2400,
                          "Old-growth forest, a logging camp and an apple orchard. Fog, rain, long sightlines lost."),
}
for name, (display, biome, heat, _unused, desc) in MAPS.items():
    m = get_or_create_data_asset(MAPDEF_PATH, name, unreal.DCRegionDefinition)
    m.set_editor_property("display_name", display)
    m.set_editor_property("description", desc)
    m.set_editor_property("biome", biome)
    m.set_editor_property("heat_stress_multiplier", heat)
    # Resave every asset so the old map-definition class is written out as a region.
    eal.save_loaded_asset(m, False)
eal.save_directory(MAPDEF_PATH)
unreal.log(f"[DC] {len(MAPS)} region definitions ready")


# ---------------------------------------------------------------------------
# Greybox test map: a roadhouse in the desert
# ---------------------------------------------------------------------------
if eal.does_asset_exist(TEST_MAP):
    # Reuse the existing map: open it and clear every actor, then repopulate below.
    if not level_editor.load_level(TEST_MAP):
        raise RuntimeError("Could not open " + TEST_MAP)
    for old in actors.get_all_level_actors():
        if not isinstance(old, (unreal.WorldSettings, unreal.Brush)):
            actors.destroy_actor(old)
elif not level_editor.new_level(TEST_MAP):
    raise RuntimeError("Could not create " + TEST_MAP)

cube = eal.load_asset("/Engine/BasicShapes/Cube")
V = unreal.Vector
R = unreal.Rotator


def block(loc, scale, label):
    """Spawns a cube StaticMeshActor. Cube is 100cm, so scale is in metres."""
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, V(*loc), R(0, 0, 0))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(V(*scale))
    a.set_actor_label(label)
    return a


def spawn(cls, loc, label, yaw=0.0):
    a = actors.spawn_actor_from_class(cls, V(*loc), R(0, 0, yaw))
    a.set_actor_label(label)
    return a


# Ground: 200m x 200m of red dirt (grey for now)
block((0, 0, -50), (200, 200, 1), "Ground")

# Roadhouse: 20m x 14m shell with a doorway on the west wall
block((0, 700, 200), (20, 0.3, 4), "Roadhouse_WallN")
block((0, -700, 200), (20, 0.3, 4), "Roadhouse_WallS")
block((1000, 0, 200), (0.3, 14, 4), "Roadhouse_WallE")
block((-1000, 450, 200), (0.3, 5, 4), "Roadhouse_WallW1")
block((-1000, -450, 200), (0.3, 5, 4), "Roadhouse_WallW2")
block((0, 0, 410), (20.3, 14.3, 0.2), "Roadhouse_Roof")
block((300, 300, 50), (6, 0.8, 1), "Roadhouse_Counter")

# Bowser (fuel pumps) and a couple of wrecked cars out front for cover
block((-1800, 300, 90), (0.6, 1.2, 1.8), "Bowser_1")
block((-1800, -300, 90), (0.6, 1.2, 1.8), "Bowser_2")
block((-2600, 1200, 70), (4.5, 1.8, 1.4), "Wreck_Ute")
block((-600, -2200, 70), (4.5, 1.8, 1.4), "Wreck_Sedan")
block((2500, 2500, 150), (8, 6, 3), "Shed")
block((-4000, -3000, 100), (1, 30, 2), "Fence_Line")

# Lighting: harsh outback sun
sun = spawn(unreal.DirectionalLight, (0, 0, 2000), "Sun")
sun.set_actor_rotation(R(0, -50, 30), False)
sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
sun_comp.set_editor_property("intensity", 10.0)
sun_comp.set_editor_property("atmosphere_sun_light", True)
sun_comp.set_editor_property("light_color", unreal.Color(255, 236, 210, 255))

# Moon: a faint cool light the game's day/night cycle turns on when the sun is down
moon = spawn(unreal.DirectionalLight, (0, 0, 2100), "Moon")
moon.set_editor_property("tags", [unreal.Name("Moon")])
moon_comp = moon.get_component_by_class(unreal.DirectionalLightComponent)
moon_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
moon_comp.set_editor_property("intensity", 0.4)
moon_comp.set_editor_property("light_color", unreal.Color(170, 190, 255, 255))
moon_comp.set_editor_property("atmosphere_sun_light", True)
moon_comp.set_editor_property("atmosphere_sun_light_index", 1)

spawn(unreal.SkyAtmosphere, (0, 0, 0), "SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 500), "SkyLight")
sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
sky_comp.set_editor_property("real_time_capture", True)
fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0), "HeightFog")
fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_comp.set_editor_property("fog_density", 0.006)
fog_comp.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.9, 0.6, 0.4, 1.0))

# Exposure limits: stop auto-exposure brightening moonlit nights into daytime
ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), "PostProcess_Exposure")
ppv.set_editor_property("unbound", True)
pp = ppv.get_editor_property("settings")
pp.set_editor_property("override_auto_exposure_min_brightness", True)
pp.set_editor_property("auto_exposure_min_brightness", 1.0)   # EV100 floor: nights stay dark
pp.set_editor_property("override_auto_exposure_max_brightness", True)
pp.set_editor_property("auto_exposure_max_brightness", 14.0)
ppv.set_editor_property("settings", pp)

# Navmesh for zombies
nav = spawn(unreal.NavMeshBoundsVolume, (0, 0, 0), "NavBounds")
nav.set_actor_scale3d(V(100, 100, 10))

# Survivor spawn points scattered around the edge; the game picks one at random
SPAWNS = [(-5000, 0), (-5000, 3500), (-4000, -4500), (4000, -5500), (6500, 4000), (-1500, 7000)]
for i, (x, y) in enumerate(SPAWNS):
    yaw = math.degrees(math.atan2(-y, -x))  # face the roadhouse
    spawn(unreal.PlayerStart, (x, y, 100), f"SurvivorSpawn_{i}", yaw=yaw)


def roll(item, chance, lo=1, hi=1):
    return unreal.DCLootRoll(item=items[item], chance=chance, min_count=lo, max_count=hi)


def container(loc, name, table, yaw=0.0):
    c = spawn(unreal.DCLootContainer, loc, f"Loot_{name.replace(' ', '')}", yaw)
    c.set_editor_property("container_name", name)
    c.set_editor_property("loot_table", table)
    return c


container((600, 500, 25), "Esky", [
    roll("DA_WaterBottle", 0.9, 1, 2), roll("DA_MeatPie", 0.6, 1, 3), roll("DA_BakedBeans", 0.4)])
container((-200, 500, 25), "Servo Shelf", [
    roll("DA_MeatPie", 0.8, 1, 2), roll("DA_BakedBeans", 0.7, 1, 2), roll("DA_WaterBottle", 0.5), roll("DA_Bandage", 0.4, 1, 2)])
container((800, -500, 25), "First Aid Cabinet", [
    roll("DA_Bandage", 0.9, 1, 3), roll("DA_Splint", 0.6), roll("DA_FirstAidKit", 0.3)])
container((300, -300, 25), "Till Safe", [
    roll("DA_Opal", 0.5), roll("DA_GoldNugget", 0.25)])
container((-2600, 1200, 160), "Ute Tray", [
    roll("DA_CarBattery", 0.5), roll("DA_JerryCan", 0.6), roll("DA_Ammo308", 0.5, 5, 15), roll("DA_Ammo9mm", 0.7, 10, 20)])
container((2500, 2000, 25), "Shed Toolbox", [
    roll("DA_Ammo9mm", 0.8, 15, 30), roll("DA_Splint", 0.4), roll("DA_GoldNugget", 0.15)])


def loose(item, count, loc):
    w = spawn(unreal.DCWorldItem, loc, f"Item_{item[3:]}")
    w.set_editor_property("item", items[item])
    w.set_editor_property("count", count)


loose("DA_WaterBottle", 1, (-3500, 200, 20))
loose("DA_Ammo9mm", 17, (-1800, 0, 20))
loose("DA_Bandage", 1, (-600, -2000, 20))
loose("DA_GoldNugget", 1, (2600, 2600, 20))


# Zombies: a few around the roadhouse, a cluster in the shed, stragglers on the road
zombie_spots = [
    (-1300, 1500), (-1500, -1200), (500, 0), (-400, -300), (1600, 900),
    (2300, 2300), (2700, 2300), (2500, 2900), (4500, -500), (5500, 600), (0, 5000), (300, 6000),
]
for i, (x, y) in enumerate(zombie_spots):
    spawn(unreal.DCZombie, (x, y, 100), f"Zombie_{i:02d}", yaw=float(i * 37 % 360))

level_editor.save_current_level()
unreal.log("[DC] Test map built and saved: " + TEST_MAP)
unreal.SystemLibrary.quit_editor()
