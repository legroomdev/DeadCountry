# Dead Country — Design Notes

**Pitch:** A first-person, fleshed-out Project Zomboid set in an Australia overrun by the dead.
A persistent world: scavenge, fortify, learn, and survive as many days as you can.
*(Changed 2026-11-11: the extraction-shooter loop was dropped. See `DevelopmentPlan.md`.)*

**Players:** single-player or 1–4 co-op in the host's persistent world (listen server, drop-in/out).
Every system is server-authoritative and replicated from day one.

## Core loop
1. Spawn as a new survivor somewhere in the region.
2. Scavenge houses, shops and homesteads; avoid or fight the dead (melee first, guns are loud).
3. Manage water, food, sleep, injuries and infection; the outback heat makes water critical.
4. Find and fortify a base: barricades, storage, rain collectors, generators.
5. Grow skills over days and weeks; the world changes (power and water go off, hordes migrate).
6. Die → your body and base stay in the world; start again as a new survivor.

## Pillars
| System | Feel |
|---|---|
| Health | Per-limb HP, bleeds, fractures; scratches, lacerations, bites (bite = infection = death sentence) |
| Inventory | Grid inventory with item sizes; weight and encumbrance slow you down |
| Threats | Noise draws hordes; gunshots are a last resort; zombies break through doors |
| Survival | Thirst, hunger, fatigue, illness, mood; heat matters (Red Centre drains water 2x) |
| Persistence | The world remembers: looted houses, barricades, bodies, your base |

## Environments (data in `/Game/Maps/Definitions`; to become regions)
| Map | Biome | Feel / hook |
|---|---|---|
| Coober Creek Roadhouse | Red Centre outback | Roadhouse, airstrip, opal diggings. Water is worth more than gold |
| Kalgarra | Wheatbelt town | Silos, pub, footy oval, rail siding. Open paddocks |
| Shelly Heads | Coastal surf town | Surf club, caravan park, boat ramp extract |
| Barra Crossing | Tropical north | Cane fields, mangroves, flooded causeway, wet-season storms |
| Mt Wallace Ski Village | Snowy Mountains | Lodges, chairlifts, hydro dam. Cold drains energy |
| Ironvale | Pilbara mining town | Haul roads, ore trains, open-cut pit, industrial loot |
| Wattle Vale | Regional city | Hospital, shopping centre, suburbs. Densest horde |
| Huon Hollow | Tasmanian wilderness | Old-growth forest, logging camp, fog |

## Code map (`Source/DeadCountry`)
- `Player/` — `DCCharacter` (first-person survivor: FP arms, equip, shove/stomp, sleep), `DCPlayerController` (Enhanced Input built in code, debug commands)
- `Health/` — `DCHealthComponent` (limbs, bleeds, fractures, infection), `DCSurvivalComponent` (water, energy, fatigue, stamina)
- `Inventory/` — `DCItemDefinition` (data asset per item, incl. melee stats), `DCInventoryComponent` (replicated grid, per-item condition/ammo)
- `Weapons/` — `DCEquippable` (base for held items), `DCWeapon` (P9 firearm), `DCMeleeWeapon` (data-driven melee), `DCShellCasing`
- `AI/` — `DCZombie` (mannequin, knockdown/stagger, ragdoll), `DCZombieAIController` (wander → investigate noise → chase)
- `World/` — `DCWorldItem`, `DCLootContainer`, `DCRegionDefinition`
- `Survival/` — `DCSurvivalGameMode` (world save, sandbox options, respawn), `DCSurvivalGameState` (clock, day/night), `DCPlayerState`, `DCSkillsComponent`, `DCWorldSave`
- `UI/` — `DCHUD` + `DCHUDWidget` (C++ UMG HUD)
- `Core/` — `DCTypes`, `DCAudio`

`Tools/Bootstrap/bootstrap_content.py` rebuilds starter items, map definitions and the greybox test map.

## Controls (prototype)
WASD move · Mouse look · Shift sprint · C/Ctrl crouch · Space jump · F interact ·
LMB fire / swing · RMB aim (guns) or shove (melee, bare hands; stomps a downed zombie) · R reload ·
Tab inventory · 1–9 use or equip an item (while inventory open; equip again to put away) ·
Z sleep / wake · Enter start a new survivor (after death)

## Sandbox options (new worlds only, as URL options)
`?World=Name?ZombieSpeed=Shamblers|FastShamblers|Sprinters?Population=1?Infection=1?Loot=1?DayLength=60?XP=1`

## Roadmap
See [DevelopmentPlan.md](DevelopmentPlan.md) for milestones.
