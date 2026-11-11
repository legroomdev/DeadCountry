# Dead Country — Development Plan

**What it is:** a first-person, fleshed-out take on Project Zomboid set in Australia. A persistent world after the outbreak: scavenge, fortify, learn skills, and survive as many days as you can. Death is permanent for your survivor, but the world (and your base) carries on.

**How it's made:** hobby project, no deadline. Solo developer + Claude. Unreal 5.8, C++ core, Blueprint/data for tuning.
**Art:** licensed characters/arms/animations (Fab, Mixamo) + weapons and props modelled by us in Blender.
**Vehicles:** worked on when the game needs them (likely once the world is big enough to need driving).

Milestones are a guide, not a schedule. Each ends with something you can **play and judge** before moving on.

---

## Decisions locked in (2026-11-11)

| Topic | Decision |
|---|---|
| Genre | Persistent survival sandbox (no raids, no extraction, no stash) |
| Death | Permadeath for the survivor; the world persists; respawn as a new survivor |
| Co-op | Host's save owns the world and every player's character (Zomboid model), drop-in/drop-out |
| Art | Licence characters/animations, make weapons/props ourselves |
| Target | Hobby, no deadline |
| Vehicles | When relevant |

## Open questions (decide before the milestone that needs them)

1. **World shape** (before M5): one connected map of a region with several environments, or separate standalone worlds per environment you pick at the start? *Recommendation: one region first (Red Centre), designed so more regions can join later.*
2. **Zombie rules** (before M3): Zomboid-style shamblers by default with **sandbox options** (speed, population, infection rules, loot rarity) chosen per world. *Recommendation: yes.*
3. **Melee vs guns balance** (M3): guns are loud and rare, melee is the everyday tool — like Zomboid. *Recommendation: yes.*

---

## Milestone 0 — Foundation ✅ (done)

| Keep | Rework / remove in M1 |
|---|---|
| First-person movement, sprint/crouch, stamina, footstep noise | Raid timer, extraction zones, KIA/MIA/Extracted |
| Per-limb health, bleeds, fractures, infection, hydration/energy | Raid game mode/state/player state → survival versions |
| Replicated grid inventory, item data assets, using items | "Loot value $" focus → item usefulness/condition |
| P9 pistol: model, animated action, ADS, recoil, 9mm ammo | Map definitions → world/region definitions |
| Zombie AI: wander → investigate noise → chase → bite | — |
| Loot containers, lootable bodies, greybox test map, tooling scripts | — |

---

## Milestone 1 — Pivot to survival + safety net (≈ 2–3 weeks) — *in progress*

**Status (2026-11-11):** survival game mode, world clock + day/night, death → new survivor, world save/load (autosave, backup, save on quit/logout/death), fatigue + sleep with time skip, raid code removed, regions in place. Tested: new world → reload restores survivor and clock; death leaves a saved body and the next session starts a new survivor.
**Source control:** git + LFS, backed up to the private repo github.com/legroomdev/DeadCountry (first push 2026-11-11).
**Still to do:** playtest sleep (Z) in-game; multiplayer PIE check of save/respawn.

Debug console commands: `DCSetHour <h>`, `DCSuicide`, `DCSaveWorld`, `DCForceAim 1`, `DCShotIn <s>`, `DCSpawnZombie [cm]`, `DCTestFire <shots> [delay]`, `DCArmsDebug [delay]`. (`DCShotIn` uses `shot showui` so the HUD is captured.)


1. **Source control** — git + Git LFS, private GitHub repo as backup. *(Do this first.)*
2. **Survival game mode** — replace the raid mode: world clock with day counter and day/night, "Days survived" on the HUD, no raid timer or extracts.
3. **Death & respawn** — survivor dies → corpse stays with their gear → player creates a new survivor and spawns elsewhere in the same world.
4. **World save system** (the core of the game, built early): save/load the world — containers' contents, dropped items, killed zombies, doors/barricades (later), each player's character (stats, inventory, health). Autosave + save on quit.
5. **Sleep & fatigue** — add fatigue to survival; sleeping passes time (single-player) or needs everyone in bed (co-op).
6. **Clean up the code** — delete extraction/raid code, rename map definitions to region definitions.

**Exit test:** play a few in-game days, quit, reload: the world is exactly as you left it. Die and come back as a new survivor and find your old body.

---

## Milestone 2 — "It feels good" (≈ 3–4 weeks) — *in progress*

**Round 1 done (2026-11-11):** Epic mannequin content in `/Game/Characters` (free, engine-licensed) as placeholder art.
Zombies use Manny/Quinn with a zombie skin, shamble/lurch/grab animations, per-bone hits (real headshots), ragdoll deaths.
Synthesized placeholder audio (`Tools/Audio/synth_sounds.py` → `SourceArt/Audio`, imported by `Tools/Import/setup_m2_assets.py`):
gunshot, dry fire, mag out/in, slide release, casing tinks, footsteps, impacts, zombie groan/alert/attack.
VFX: muzzle flash cards + light, physics shell casings, bullet-hole and blood decals; debug tracers removed.
**Round 2 done (2026-11-11):** first-person mannequin arms locked to the P9 grip (two-handed pistol pose, reload clip);
third-person mannequin body + pistol locomotion for other players; C++ UMG HUD (moodle-style status icons, body
silhouette, ammo panel, stamina bar, clock, prompts, sleep/death/inventory overlays).
**Still open for M2:** licensed sounds, zombie art and arms to replace placeholders; crawler state; ambience; exit-test playtest.

1. **First-person arms** (licensed) holding the P9: fire, reload, ADS, slide lock, interact/pick-up animations.
2. **Audio** — gunshots with outdoor tails, footsteps per surface, zombie groans/alerts, doors, ambience (cicadas by day, wind at night). MetaSounds.
3. **VFX** — muzzle flash, shell ejection from `SOCKET_Eject`, dust/blood impacts. Remove debug tracers.
4. **Zombie art** (licensed) — mesh, walk/lurch/attack/hit/death anims, ragdoll, crawler state when legs are destroyed.
5. **UMG HUD** — replace the debug HUD with a moodle-style status column (thirst, hunger, tired, bleeding, injured, panic…) and a body silhouette.

**Exit test:** 15 minutes in the test map feels like a game, not a prototype.

---

## Milestone 3 — Survival depth (≈ 6–8 weeks) — *in progress*

**Round 1 done (2026-11-11):** melee combat (cricket bat, machete: data-driven swings, multi-target arcs, crits,
knockdowns, durability that breaks weapons), shove + stomp, zombie stagger/knockdown ragdoll with get-up, equipping
from the inventory (items carry their own condition and loaded ammo through drops, bodies and saves), skills & XP
(Aiming, Reloading, Long Blunt/Blade, Fitness, Strength, Sneaking, Maintenance, ... with real effects),
sandbox world options (zombie speed, population, infection, loot, day length, XP).
**Round 2 next:** drag-and-drop inventory screen + equipment slots. **Round 3:** injuries, nutrition/cooking, mood.

1. **Melee combat** — bats, crowbars, machetes, axes, shovels; swing stamina, weapon durability, shove/knock-down, stomp; melee becomes the default and guns the emergency.
2. **Inventory UI** — UMG grid with drag/drop and rotate, equipment slots (hands, back, belt, clothing), containers in containers, a nearby-floor panel, a loot panel, weight and encumbrance.
3. **Injuries, Zomboid-style** — scratches, lacerations and bites (bite = infection), deep wounds, glass shards, bandage dirtiness, disinfecting, stitches, splints, illness and food poisoning.
4. **Nutrition** — calories, food types, cooking, spoilage (heat speeds it up!), hydration from tanks and creeks (boil it or get sick).
5. **Skills & XP** — Aiming, Reloading, Short/Long Blunt, Blade, Fitness, Strength, Sneaking, Carpentry, Cooking, First Aid, Mechanics (later). Learn by doing + skill books.
6. **Mood** — panic (affects aim), stress, boredom, depression, kept simple at first.
7. **Sandbox options** — zombie speed/population/infection, loot rarity, day length, set when creating a world.

**Exit test:** a week-long in-game survival with real choices about food, injuries and when to fight.

---

## Milestone 4 — Building & crafting (≈ 4–6 weeks)

1. **Doors & windows** — open/close/lock, smash, climb through; zombies thump and break them.
2. **Barricading** — planks, nails, hammer; Carpentry skill sets strength.
3. **Crafting** — data-driven recipes (items + tools + skill), crafting menu.
4. **Base building** — walls, floors, gates, storage, rain collectors (outback water!), sheet-metal walls.
5. **Utilities shut-off** — power and water go off after a sandbox-set number of days; generators and fuel.

**Exit test:** fortify a farmhouse, survive a horde attack, and still have water after the mains go off.

---

## Milestone 5 — The world: Red Centre region (≈ 2–3 months)

1. **Region layout** (World Partition) — Coober Creek town with roadhouse, pub, police station, clinic, school, houses; outlying homesteads, opal diggings, airstrip, Stuart Hwy.
2. **Enterable buildings** — modular kit (corrugated iron, fibro, besser block), room types driving loot (kitchen, garage, bathroom, shed…).
3. **Landscape + PCG** — red sand, gibber plains, spinifex, mulga, creek gums, termite mounds.
4. **Zombie population** — a virtual population that persists and migrates across the region, hordes, meta events (gunshot echoes, a helicopter that draws zombies).
5. **Weather & seasons** — heat waves (water drain), storms, cold desert nights.

**Exit test:** a 30-day survival run where the town feels lived-in, dangerous, and different every time.

---

## Milestone 6 — Co-op (≈ 4 weeks)

1. **Host/join** through Steam or EOS, invite friends, drop-in/drop-out into the host's world.
2. **Characters stored in the host's world save**; each player keeps their survivor in that world.
3. **Netcode hardening** — packet loss/latency tests, client-side hit prediction, zombie relevancy and network budget for big hordes, shared sleep and time skips.

*Multiplayer is tested in PIE with 2 clients from Milestone 1 onward so this milestone is hardening, not a rewrite.*

**Exit test:** 4 players on separate PCs survive a week together without desyncs.

---

## Milestone 7+ — Growing the world (ongoing)

- **Weapons:** shotgun, .308 bolt-action, .22, revolver; attachments (suppressors matter for noise).
- **Clothing & protection:** layers, bite protection, heat vs carrying capacity.
- **Vehicles (when relevant):** utes and cars, fuel, damage, noise; keys and hotwiring. Can learn from the Beyond the Bitumen vehicle work but stays a separate copy.
- **More regions:** Wheatbelt town, coastal town, tropical north, Snowy Mountains, mining town, regional city, Tasmania.
- **Farming, trapping, fishing, hunting** (kangaroos, feral pigs).
- **NPC survivors** (later, if ever — Zomboid shipped without them for years).

---

## How we work

- One milestone at a time, each broken into small tasks you can playtest.
- Batch C++ header changes so the editor restarts once per batch; Live Coding for .cpp-only changes.
- Scripts over manual steps for repeatable content (bootstrap, import, materials).
- Keep `Docs/GDD.md` for vision and this file for what's next.

## Risks

| Risk | Mitigation |
|---|---|
| Survival sandboxes are deep and endless | Ship each system "minimum fun" first; depth later |
| Save system bugs corrupt long runs | Build saving in M1, version the save format, keep autosave backups |
| Hordes are expensive (CPU + network) | Virtual population off-screen, LODs/animation budgets, relevancy |
| Art volume solo | Licence characters/animations; model weapons/props ourselves |
| No backup today | Git + LFS + GitHub first thing in M1 |
