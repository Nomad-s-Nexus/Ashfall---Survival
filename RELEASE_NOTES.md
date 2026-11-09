# Rad Overhaul v1.2.8

Bug-fix release of Rad Overhaul (Fallout Anomaly Radiation) for Fallout 4, on top of v1.2.7.
The attached archive is a ready-to-install mod: the complete `deploy/` tree plus a freshly built
`FalloutAnomalyRadiation.dll`.

**Nothing in this release was tested in game by the owner before publishing.** Both fixes come
from the owner's v1.2.7 game logs. They are validated by contract tests and a green build only.

## Upgrade note: install the DLL, scripts and plugin together

`RadNatives.pex` changed together with the DLL, and `RadSurvival.esp` changed. Install all of
them from this archive.

## What is new in v1.2.8

1. **NPCs held by a quest are protected from outbreaks again.** In v1.2.7 the game refused to
   bind `RadNatives.IsQuestHeld` (`Native static function IsQuestHeld does not match existing
   signature` in `Papyrus.0.log`), so every call returned nothing. Quest NPCs could catch an
   illness, be sent to bed and die at the final stage. The native and the script now agree on
   the parameter type, so quest-held NPCs always recover and spread skips them, as v1.2.7
   item 6 describes. Not verified in game.
2. **No more `PendingExpirySeconds` error on load.** `RadSurvival.esp` still set a survival
   quest property that the script had renamed, so `Papyrus.0.log` logged `cannot be initialized
   because the script no longer contains that property` on every load. The stale value is
   removed; the renamed setting keeps the same 2.0 second default, so nothing changes in play.

## Included from v1.2.7

Everything in v1.2.7 is still in this archive. The v1.2.7 notes follow.

# Rad Overhaul v1.2.7

Release of Rad Overhaul (Fallout Anomaly Radiation) for Fallout 4, on top of v1.2.6. The
attached archive is a ready-to-install mod: the complete `deploy/` tree plus a freshly built
`FalloutAnomalyRadiation.dll`.

**Nothing in this release was tested in game by the owner before publishing, and the frame rate
was not measured.** It is validated by contract tests and a green build only. See Known limits.

## Upgrade note: install the plugin, scripts and DLL together

`RadSurvival.esp` has new illness and cold records, and `RadSurvivalDiseases.pex`,
`RadSurvivalQuest.pex`, `RadShowcase.pex` and `RadNatives.pex` were recompiled against the new DLL.
Install all of them from this archive.

`RadOutbreak.esp` is no longer the v1.2.6 probe: it is now the NPC and companion disease system
(item 6). It is still optional and needs `RadSurvival.esp`; enable it in your load order to get
outbreaks. It comes with `RadOutbreakQuest.pex`, `RadOutbreakEffect.pex` and
`RadOutbreakDoctorStation.pex`, the cough, sneeze and groan sounds in `Sound/FX/RadOutbreak/`,
the sick-NPC markers view `hud_outbreak.html` and the companions widget `hud_companions.html`.
Install them together.

## What is new in v1.2.7

1. **Five new illnesses based on real ones.** Dysentery (from dirty water), food poisoning (from
   raw or spoiled food), tetanus (from melee hits), pneumonia (from staying cold) and wound
   infection, which turns into sepsis at stage 3 (from being hit while below half health). Each
   incubates, then goes through three stages with its own penalty shown in the Pip-Boy. Purified
   Water treats dysentery and food poisoning, Med-X treats tetanus and a Stimpak treats wound
   infection, one stage per dose. Rest cures as before, except tetanus past stage 1 and pneumonia
   while you are still cold. Refreshing Beverage and the Rad Cure Compound cure any illness.
   RadAway still cures only Fever and Wasting. Not verified in game.
2. **Hypothermia has effects now.** Each cold stage from Chilled to Critical gives stat and speed
   penalties, slower Action Point regeneration, slower reloads and shakier aim, shown in the Pip-Boy, and from Moderate on the cold costs health, more at each stage.
   No damage while paused or during sleep or wait, god mode is respected, and there is no cold damage at vanilla Survival difficulty. Eating hot cooked
   food warms you for 2 game hours per item. Not verified in game.
3. **New tuning keys** in `FalloutAnomalyRadiation.ini` and the debug menu: `HypothermiaEffects`
   (on/off), `HypothermiaDamage` (0 to 3), `DiseaseCauseScale` (0 to 5), `DiseaseLethal` (on/off)
   and `DiseaseLethalHours` (1 to 240).
   An illness left at stage 3 for `DiseaseLethalHours` (default 24) starts a 12-hour terminal phase:
   two warnings, a health drain, then death unless cured. Food poisoning never kills.
   God mode (TGM, or the debug menu's God ON) stops the drain and the death.
   The Rad Cure Compound is crafted at the Chemistry Station (1 RadAway, 1 Purified Water,
   2 Bloodleaf; Chemist or Medic rank 1). Not verified in game.
4. **Debug menu scenes** force each new illness and each cold stage. The console takes
   `climate:stage:N` (0 to 5) and `climate:warm`.
5. **With Phosphor HUD installed, the companions widget joins the left column.** On a first
   start the column is companions, toxic, filter, needs and climate (rows at 200, 308, 416, 524
   and 632 of 1080), below Phosphor's message feed and above its companion portraits. A saved
   layout still wins. Not verified in game.
6. **NPCs and companions get sick** (`RadOutbreak.esp`). Human NPCs near you catch your
   contagious illnesses (fever, wasting, dysentery, pneumonia), pass them to each other and
   rarely catch one on their own; sick NPCs near you add to your own illness risk. Children
   and robots never get sick, and essential NPCs only when they are companions. An illness runs
   incubation, early, progress and final stages: sick NPCs cough and sneeze, get weaker, and
   ordinary NPCs go to bed. At the end an ordinary NPC may die; companions, essential, protected
   and quest-held NPCs always recover. Medicine cures them, also when handed over by trade:
   RadAway up to the early stage, Addictol up to progress, the Rad Cure Compound any stage. A
   **Doctor Station** (workshop) with an assigned settler or companion slows the spread; if that
   settler is a doctor or field medic it also treats patients nearby. Carrying the **Pathogen
   Scanner** (Chemistry Station), or a vanilla Biometric Scanner,
   shows a stage ring over each sick NPC in range. The companions widget shows your own illness
   and up to three companions, and sick companions show in the Pip-Boy. The debug menu has an
   `08 OUTBREAK` tab, and the ini has the tuning keys `OutbreakEnabled`, `OutbreakSpread`,
   `OutbreakDeaths` and `OutbreakAmbient`. Not verified in game.
7. **A filter lasts 20 minutes even when the ini line is missing.** Without
   `[Filter] DurationSeconds` in `FalloutAnomalyRadiation.ini` the DLL used 300 seconds; it now
   uses 1200, the same as the shipped ini. `thresholds.json` lost its per-stage fields that the
   DLL never read; no gameplay change.

## Included from v1.2.6

Everything in v1.2.6 is still in this archive. The v1.2.6 notes follow.

# Rad Overhaul v1.2.6

Release of Rad Overhaul (Fallout Anomaly Radiation) for Fallout 4, on top of v1.2.5. The
attached archive is a ready-to-install mod: the complete `deploy/` tree plus a freshly built
`FalloutAnomalyRadiation.dll`.

**Nothing in this release was tested in game by the owner before publishing, and the frame rate
was not measured.** It is validated by contract tests and a green build only. See Known limits.

## What is new in v1.2.6

1. **With Phosphor HUD installed, the Ashfall cores start on the left edge.** On a first start,
   with no saved layout, the climate, needs, filter and toxic cores sat 396 pixels in from the left
   edge. They now start at the left edge, in one column about the vertical middle of the screen
   (rows at 356, 464, 572 and 680 of 1080). A saved layout still wins. With companions in the party
   the bottom row sits just over their portraits. Not verified in game.

## Included from v1.2.5

Everything in v1.2.5 is still in this archive. The v1.2.5 notes follow. Where they say the cores
stack above Phosphor's core row (item 4), v1.2.6 puts them on the left edge instead.

# Rad Overhaul v1.2.5

Release of Rad Overhaul (Fallout Anomaly Radiation) for Fallout 4, on top of v1.2.4. The
attached archive is a ready-to-install mod: the complete `deploy/` tree plus a freshly built
`FalloutAnomalyRadiation.dll`.

**Nothing in this release was tested in game by the owner before publishing, and the frame rate
was not measured.** It is validated by contract tests and a green build only. See Known limits.

## Upgrade note: delete the old gas mask sounds

The gas mask sounds were replaced and renamed, so the old files are no longer in this archive and installing over an older version does not remove them. **If you are upgrading from v1.2.0 to v1.2.4, delete these files from `Data/Sound/FalloutAnomaly/mask/` if they are still there:** `gasmaskdraw.xwm`, `gasmaskdraw_f.xwm`, `gasmaskwithdraw.xwm`, `gasmaskwithdraw_f.xwm`, `gasmaskfilter1.xwm`, `gasmaskfilter2.xwm`, `gasmaskfilterfast1.xwm`, `gasmaskfilterfast2.xwm`, `gasmask_choking_male_nomask.xwm`, `gasmask_choking_male_withmask.xwm`, `gasmask_choking_female_nomask.xwm` and `gasmask_choking_female_withmask.xwm`. They are unused leftovers, not harmful, but they take space. A mod manager that installs each version into a clean folder needs no clean-up.

`RadSurvivalDiseases.pex` was recompiled for the new exposure and illness rings. Install it from this archive together with the rest.

## What is new in v1.2.5

1. **The gas mask sounds are replaced with new sound files.** The twelve sounds (mask on and off, filter breathing, choking and coughing) are new files. The previous gas mask files are removed. Releases v1.2.0 to v1.2.4 contain those old files, which is why the upgrade note asks you to delete them. The new files are named `ashfall_mask_*` and `ashfall_choke_*` in `Sound/FalloutAnomaly/mask/`, in the same stereo xwm format. Not verified in game.
2. **The HUD hides behind menus and loading screens.** The status cores no longer draw over the main menu, loading screens, pause menu, Pip-Boy, container, barter, terminal, examine, lockpicking, level-up, sleep/wait, looks, book, SPECIAL, credits, cooking or power armor menu. They come back when the menu closes. Not verified in game.
3. **Widgets scale with the screen resolution.** At 1440p and 4K the cores kept their 1080p size. They now scale like Phosphor HUD does. Not verified in game.
4. **The cores keep clear of Phosphor HUD.** With Phosphor HUD installed, the climate, needs, filter and toxic cores no longer sit on its WEIGHT and FOOD/WATER/REST cores. They stack at one left edge above its core row and level text, clear of its companions, compass and subtitles. A saved layout still wins. Ashfall does not draw or touch Phosphor's own widgets. Not verified in game.
5. **Ashfall leaves Phosphor HUD's widgets alone.** Its needs cores hide when Phosphor draws FOOD, WATER and REST in vanilla Survival. Phosphor HUD stays optional. Not verified in game.
6. **Exposure and illness are ring cores.** Both used to be small square boxes. The EXPOSURE core is a round ring that fills with the exposure percentage, with the biohazard sign in the middle and `EXPOSURE` and the percent underneath. It is amber from 50% and red from 80%, as before. The ILLNESS core is a round ring with the illness sign in the middle and `ILLNESS` with the name and stage (for example `FEVER 2`) underneath. The ring fills toward one third while the illness is incubating, is one third at stage 1, two thirds at stage 2 and full at stage 3, and never drops back. It is amber until stage 3 and red at stage 3. Exposure and illness are never shown together. For the incubation fill the survival script now writes the onset progress into the fraction of the existing disease value, so `RadSurvivalDiseases.pex` changed (see the upgrade note). On a save made before this, the illness ring may stay empty until the next survival update. Not verified in game.
7. **The HUD layout editor is a small panel on the right.** "Open HUD layout editor" no longer opens inside the big debug console. It hides the console and shows a narrow panel docked to the right edge, so the real HUD stays visible while you adjust X, Y, size, rotation and hide per widget. DONE returns to the console. CLOSE MENU, Esc and Shift+F8 close everything. With PrismaHUD installed its own editor still opens instead. Not verified in game.
8. **Reset in the layout editor uses the real default positions.** With Phosphor HUD installed it used to reset to the old top-left slots instead of the companion slots. Not verified in game.

## Included from v1.2.4

Everything in v1.2.4 is still in this archive, except the old gas mask sounds (item 1). The v1.2.4 notes follow.

# Rad Overhaul v1.2.4

Release of Rad Overhaul (Fallout Anomaly Radiation) for Fallout 4, on top of v1.2.3. The
attached archive is a ready-to-install mod: the complete `deploy/` tree plus a freshly built
`FalloutAnomalyRadiation.dll`.

**Nothing in this release was tested in game by the owner before publishing, and the frame rate
was not measured.** It is validated by contract tests and a green build only. See Known limits.

## Upgrade note: install two plugin files together

**`RadiationOverhaul.esp` and `RadStormFX.pex` changed together, so install both from this archive in the same step.** The nine dome-only records are removed from `RadiationOverhaul.esp`. `RadStormFX` no longer has the dome spawn, the dome timer, or the `EnableCloudDome`, `DomeFollowInterval` and `DomeZOffset` properties. Save compatibility with a save that held a dome reference is **unverified in game**. This archive also no longer ships `RadOverhaul_MaskFX.esp` or `RadMaskViewFX.pex` (the shelved M50 view). **If you are upgrading over v1.2.3, delete `RadOverhaul_MaskFX.esp` and `Scripts/RadMaskViewFX.pex` from the Fallout 4 `Data` folder (or disable the plugin), and delete the leftover M50 files this archive no longer ships:** `Scripts/Source/User/RadMaskViewFX.psc`, `Meshes/RadiationOverhaul/Overlay/RadMaskView.nif`, `materials/RadiationOverhaul/Mask/RadMaskM50*.bgsm` and `textures/RadiationOverhaul/Mask/RadMaskM50*.dds`. Behaviour with these leftovers was not tested: whether an old `RadOverhaul_MaskFX.esp` still loads without its script, and whether saves that reference its records still load, are unverified in game.

## What is new in v1.2.4

1. **The native storm dome is removed.** Storms no longer spawn, follow or animate the dome. Camera shake, dust motes and the storm IMAD fade are kept. The shipped plugin and script changed: the nine dome records are removed from `RadiationOverhaul.esp`, and `RadStormFX` no longer has the dome spawn, the dome timer or the `EnableCloudDome`, `DomeFollowInterval` and `DomeZOffset` properties. v1.2.3 retires dome references before a load; that path was not exercised for this removal. Save compatibility is **unverified in game**. The dome source is archived under `archive/storm-dome/` and is not built. The dome meshes, textures and credit file are no longer shipped (see item 7).
2. **Ash Fallout and Toxic Sulfur storms use the vanilla Glowing Sea radstorm sky.** The sky rolls in only when the storm becomes Active, not at the warning phase, so exposed players should take no vanilla radstorm dose during the warning (not verified in game). Acid Rain, Rad Firestorm and Dust Storm keep their custom weather. Acid Rain is excluded from the vanilla sky because it has real rain and no weather spell.
3. **The storm lead key stays `DomeLeadMinutes`.** The shipped INI key is not renamed (INI keys are append-only), so an existing INI keeps its lead time. An optional `StormLeadMinutes` key, if present, takes precedence over it.
4. **A vanilla-sky storm starts the radstorm cooldown when it ends.** A natural vanilla radstorm can no longer follow our storm straight away; it waits out `VanillaRadstormGapMinutes`. Not verified in game.
5. **A non-numeric lead value falls back to the 3-minute default.** Before, a garbage `StormLeadMinutes` or `DomeLeadMinutes` value read as 0 minutes. Negative and non-finite values also fall back. Shipped INI values are unchanged.
6. **Each storm has its own screen colour.** Ash Fallout is white, Rad Firestorm red-orange, Toxic Sulfur yellow, Acid Rain green and Dust Storm brown. The colour is a click-through overlay in the radiation HUD that follows the storm phase, fades in and out, and is hidden when the player is sheltered. The Ash engine tint (brown-orange) is no longer applied. **Not verified in game;** the alpha values are estimates.
7. **Dead storm dome files are no longer shipped.** Removed from `deploy/`: the KORs dome meshes (`Meshes/KORsMeshes/Anomaly/`), the dome materials (`materials/Knundrum/Anomaly/`), the dome textures (`textures/KORsTextures/Anomalies/`, `textures/FalloutAnomaly/StormNif/`, `textures/FalloutAnomaly/FODomeBlackHole.dds`) and `CREDITS_BlackHole_NASA.txt`. The dome records and the dome Papyrus were removed earlier (see item 1), so nothing references these files. The KORs credit is dropped from `NOTICE.md` with the assets.
8. **The 3D M50 mask view is shelved, and the flat PNG visor is the first-person view again.** The mesh, materials, textures, `RadOverhaul_MaskFX.esp` and the `RadMaskViewFX` script moved to `archive/m50-mask/`, which is not packaged. **Not verified in game.**
9. **A roofed, walled building is an enclosed shelter.** A roof counts when the straight-up ray hits and at least 3 of the 5 roof rays hit. Walls count when at least 5 of the 8 wall rays hit. Inside an enclosed shelter the cold ambient reads as 8 C (`[Climate] EnclosedAmbientFloorC`, range -10 to 20), so the HUD shows COLD rather than FREEZING, and wind is removed from both the cooling and the HUD wind readout. A roofed spot with open sides is partial shelter: thermal shelter is 0.5 (up from 0.25) and wind is cut by 75%. `[Climate] EnclosedShelterEnabled=0` turns the enclosed level off. Shelter rays can no longer be dropped by the engine's pick budget. **Not verified in game.**
10. **God mode stops rads and choking from killing you.** With TGM god mode on, the choking drain and the storm HP loss are clamped to a 1 HP floor while `[Survival] RespectGodMode=1` (the default). Rads and the HUD keep accumulating; with god mode off nothing changes. Detection runs on OG and AE. **Not runtime-verified.**
11. **The debug menu (Shift+F8) runs in its own PrismaUI view and closes reliably.** The view is created when the menu opens and destroyed when it closes. Shift+F8 closes the menu even when the game window is not in the foreground, and the CLOSE button and Escape hide it. Focus is released on close; if focus cannot be taken, the menu gives up after 2 seconds and uses the FocusOverlay fallback. The panel starts closed. Not verified in game.
12. **The debug panel has a redesigned diagnostics page, settings accordions and an apparel console.** Not verified in game.
13. **Pip-Boy debug panel and PrismaUI stack fixes (not verified in game).** The ECG animation runs only while the panel is open. The Geiger click timer stops on close. Dial scrubbers start from the live percentage. The RobCo boot plays only from the replay control and ignores key auto-repeat. Clipboard copy falls back to `execCommand` and never throws. Armor names with 4-byte characters no longer hang the HUD. Queued debug commands survive load (bounded to 32), hotkey edges come from one input read, and widgets send visibility only on change.
14. **Load-time settle.** Prisma view work now waits for the load to settle, and the DOM-ready callback runs on the game thread. Not verified in game.

Internal (no player-visible change): storm sky and phase rules moved into the correctness policy with new tests, a leftover storm trace was removed, the dead dome code is archived, and CI and the release checks were updated to match.

## Included from v1.2.3

Everything in v1.2.3 is still in this archive, except the storm dome (including its portal, cloud decks and lightning), which item 1 removes, and the 3D M50 mask view (item 8).

# Rad Overhaul v1.2.3

Release of Rad Overhaul (Fallout Anomaly Radiation) for Fallout 4, on top of v1.2.2. The
attached archive is a ready-to-install mod: the complete `deploy/` tree plus a freshly built
`FalloutAnomalyRadiation.dll`.

**Nothing in this release was tested in game by the owner before publishing, and the frame rate
was not measured.** It is validated by contract tests and a green build only. See Known limits.

## What is new in v1.2.3
Superseded by v1.2.4: the storm dome described here is removed.

1. **Every status core is one size larger, and the HUD rows line up.** Each status core is now
   64 px in the same 84 x 98 cell, with 8 px gaps, so the rings, icons and the bigger label
   (11 px) and value (12 px) text fit without crowding. The four HUD rows (toxic, needs, climate
   and filter) share one left edge and an even 108 px pitch. The defaults for those four rows
   moved together in each widget, the layout editor, `RadHudLayout.cpp` and the companion table.
   A saved `hud_layout.json` still wins, so an old saved layout keeps its old positions. With
   PhosphorHUD companion mode the rows moved up and right to x=396, y=538-960, because the old
   bottom band could not hold four rows without overlap. Not verified in game.
2. **The storm portal shows again.** v1.2.2 hid all eight vortex and portal shapes. Now only the
   two tube shapes, `CoreBack` and `CoreFront`, stay hidden. The six portal shapes (`PortalA`,
   `PortalB`, `DiskOuter`, `DiskInner`, `DiskHalo`, `PhotonRing`) follow their baked windows,
   about 128 to 320 s into the storm. The cloud decks, lightning and the sky fade are unchanged.
   Not verified in game.

## Included from v1.2.2
Superseded by v1.2.4: the storm dome described here is removed.

Everything below shipped in v1.2.2 and is still in this archive, except the storm portal, which
v1.2.3 restores (see above), and except the storm dome, removed in v1.2.4.

- **The floating glass tube is gone from storms.** The baked dome drew a glassy tube with a
  glowing ring about 4000 units away in the sky. v1.2.2 stopped drawing the tube and, as a side
  effect, the portal too. The tube (`CoreBack`, `CoreFront`) stays hidden.
- **Every status icon is a Phosphor HUD style core.** Hunger, thirst, fatigue, warmth, wetness,
  temperature, wind, breath, filter, mask, auto mask and tissue draw as a dark disc with a
  segmented ring and the icon inside. When PhosphorHUD is not installed the mod draws its own
  health, AP and rads in the same style. The styling follows Phosphor HUD (MIT licence, credited
  in `THIRD_PARTY_NOTICES.md`).
- **There is no classic HUD look.** `[Hud] Mode` accepts only `auto` or `phosphor` and only
  decides whether the mod drops its own vitals when PhosphorHUD is installed. An old
  `Mode=classic` line logs a warning and is treated as `auto`.
- **Visible HUD changes.** The needs cores show a word (SATED, THIRSTY, EXHAUSTED) with the
  percentage in the tooltip. The permanent-scar arc around the tissue core is gone; the scar
  number is in its tooltip.

## Included from v1.2.1
Superseded by v1.2.4: the storm dome described here is removed.

All six changes concern the baked storm dome (`CloudArena01_4.nif`) added in v1.2.0.

1. **Forced storms start the dome at the natural phase.** The baked dome sequence is nearly
   empty for its first 40 s, and a debug forced storm (Shift+F8, SHOWCASE, a storm button) only
   has a 10 s silent lead, so the dome used to start at sequence second 0 and show almost
   nothing. A forced storm now starts the dome at the phase a natural storm would have reached:
   about 170 s for a 10 s lead. Natural storms are unchanged. (The vortex tube is never drawn;
   the portal follows its baked window, see v1.2.3 above.)
2. **The vanilla cloud fade follows the dome.** The vanilla cloud layers used to fade on a 20 s
   wall-clock ramp. They now follow the dome's own sequence clock: zero until the animation
   runs, a smoothstep up over sequence 20 to 70 s, full through the middle, and down over the
   last 30 s. The plugin logs one line 30 s after the dome spawns with the sky-hook counters:
   `[StormFX] sky cloud hook stats at spawn+30s: passes=N scaled=M blend=.. seq=..`.
3. **Dome shapes are culled while invisible.** Every shape in the dome mesh is invisible by its
   static alpha and appears only through baked alpha keys. The plugin now skips drawing a shape
   while its baked alpha is about 0, using a window table generated from the shipped NIF
   (`tools/bake_dome_visibility.py`, checked by `tests/verify_dome_visibility.py`).
4. **Lower per-frame cost.** Dome layers without a controller manager are skipped, node lookups
   are cached per layer, the animation drive runs at about 30 Hz on the real clock (spin speed
   is unchanged), and the follow only moves the dome when it is more than 64 units off
   (was 0.01). These cuts are reasoned from the mesh and the per-frame code, not measured.
5. **Quieter default log.** The default log level is now info, so the `[Shelter]` roof-fan debug
   line no longer prints. The dome, tuning and relief diagnostics below still do.
6. **Correct dome time after a late start or a reload.** The dome sequence seed offset is now
   exactly the seek (the old value left a permanent lag when a layer was first driven after the
   dome clock had started). A layer whose 3D is recreated, for example by a cell reload during
   a storm, gets its sequences reactivated and reseeded and logs
   `[StormFX] dome layer 3D recreated; sequences reactivated and reseeded.`

Baked timing of the vortex layers: they would appear from about storm timeline 132 s to 314 s
of the 360 s sequence. The tube (`CoreBack`, `CoreFront`) is never drawn; the portal shapes are
drawn only inside their baked window, about 128 to 320 s, as described in v1.2.3.

## Included from v1.2.0
Superseded by v1.2.4: the storm dome described here is removed.

Everything below shipped in v1.2.0 and is still in this archive, except the storm dome, removed in v1.2.4.

- **Baked storm dome.** `CloudArena01_4.nif` and eight `textures/FalloutAnomaly/StormNif`
  textures replace the old dome. A plugin-driven animation clock sets the controller manager's
  active bit, the portal and core nodes spin, the Storm sequence loops so the dome does not go
  dark after 360 s, the mesh is collision-free so players are not blocked at the dome edge, and
  the follow moves all layers together (now throttled, see above). The storm shake stays
  outdoors and drops to 35 percent indoors.
- **Showcase tab.** Shift+F8 has a SHOWCASE tab with one button per scene (five storm types,
  rain, acid rain, mask and filter, radiation, and needs and disease scenes) and a
  **PLAY FULL TOUR** button. The needs and disease scenes overwrite the real needs and illness
  state of the loaded save: use a disposable save and resume time before saving.
- **Survival relief tuning.** A normal meal or drink visibly moves hunger and thirst (item
  relief floor 0.8, cap 1.5, scaled by value). Relief is confirmed by pairing the equip and
  item-removal events, so a drop or a failed equip credits nothing. `[Relief]
  ExtraFoodKeywords`, `ExtraThirstKeywords`, `ExtraFoodItems` and `ExtraThirstItems` add other
  mods' food and drink. The SETTINGS tab has live sliders and toggles for survival rates,
  relief, diseases, filters, masks, storms, climate, water and volume, backed by the `[Tuning]`
  INI section.
- **Water bottles.** Sneak-activating a water source with an empty bottle fills it natively
  (Dirty Water everywhere, Purified Water only in the maintained Vault 81 cells), and drinking
  it returns an empty bottle. `[Water]` controls the feature, the SETTINGS tab has a
  sneak-to-fill toggle, and vanilla Survival keeps its own filling.
- **Mask handling.** Auto-don fires only when rad-weather exposure starts, needs 3 s of stable
  exposure and removes only a mask the mod equipped. The filter and repair hotkeys
  (`[Filter] ReplaceKey`, `[Condition] RepairKey`) are validated, re-read when the debug menu
  opens, and fire only while the game window is in the foreground with no blocking menu. A
  power-armor helmet hides the visor. An AUTO chip shows whether auto-equip is OFF, ON or HELD
  for combat. Filter life is 20 minutes, **F6** replaces a filter and **F7** repairs a broken
  mask out of the box.
- **First-person mask view.** The flat PNG gas-mask visor is the first-person visor. The 3D M50
  mask interior is not shipped yet; it is kept in `archive/m50-mask/` in the repository.
- **Phosphor HUD companion mode.** With PhosphorHUD present (`[Hud] Mode=auto`, or force
  `phosphor`) the status HUD drops the duplicate vitals and uses companion default positions.
  Every status widget uses the Phosphor HUD core look; there is no classic look. Status icons are larger and brighter,
  and the debug panel has a SURVIVAL pane.
- **Roofless-interior rain.** Rain can reach the player inside the Concord Museum of Freedom,
  the one approved roofless interior, when the roof probe finds open sky.
- **Carry-weight leak fix.** Applied tissue penalties are saved, so reloads no longer stack
  carry-weight loss.
- **Native Papyrus bridge through `RadNatives`.** `RadNatives.TuningOverride` and
  `RadNatives.GetReliefFlags` are global natives on a `native hidden` script, replacing member
  natives that Bethesda's PapyrusCompiler rejects. A missing DLL keeps the script defaults.
- **Runtime hardening.** A watchdog re-queues the engine tick if it has not run for 10 s, the
  player update hook is published and torn down in order, and the needs widget hides when the
  needs state is unavailable.
- **NPC outbreak probe.** `RadOutbreak.esp` adds a milestone 1 probe (an infected settler sent
  to bed). It is off unless you add the spell by hand and is a test harness only.
- **Native climate model.** Core temperature, wetness, wind, shelter, nearby heat and clothing
  insulation are modelled each frame and shown on the HUD; the Shift+F8 panel lists the full
  values and explains them. Hypothermia penalties stay disabled: the observation model is
  complete, the punishment layer is not.
- **Native needs foundation.** Hunger, thirst and sleep debt are tracked from game time with
  versioned save/load state, saturation and backwards-time protection.
- **Survival quest and disease layer (Papyrus).** `RadSurvival.esp` adds three pools with
  staged abilities (carry weight, AP, heal rate), sleep/wait/eat relief, and two illnesses:
  fever (damage resist) and wasting (speed). Each has incubation, three stages, five exposure
  sources (ambient, wounds, swimming, rad weather, tainted food and water) and two cures: an
  uninterrupted rest, or a cure item. Both grant 24 hours of immunity.
- **Pip-Boy disease perks.** Carrying an illness shows a named perk (`Fever` or `Wasting`,
  stage 1-3) in the PERKS tab, cleared when the illness is cured. The cure item is named
  **Rad Cure Compound**; vanilla **RadAway** also cures.
- **Third-party gas masks.** The `[Masks]` section of `FalloutAnomalyRadiation.ini` takes
  keywords or armor records from other plugins, and by default any mouth-slot armor with
  radiation resistance counts as a mask. Verified against **Gasmask Overhaul** (Nexus 108845)
  and its PPM-88 at the contract level only.
- **Storm weather handoff.** Managed weather returns smoothly without handing control back to a
  captured vanilla radstorm, the storm dome follows the player across cell and worldspace
  transitions, and distant firestorm strikes are placed only in a loaded exterior worldspace,
  on terrain, facing the player.

## Requirements

- Fallout 4 OG `1.10.163` or AE `1.11.240`, with [F4SE](https://f4se.silverlock.org/).
- The native plugin fails closed on unsupported runtimes (NG `1.10.980`-`1.10.984`).
- Optional: PrismaUI F4 2.2.0 or newer for the HUD. Without PrismaHUD the mod supplies its
  own health, stamina, tissue and filter widgets.
- Optional: PhosphorHUD for the companion HUD mode.

## Install

Install the attached `RadiationOverhaul-rad-overhaul-v1.2.8.zip` with Mod Organizer 2 or
Vortex and enable the included ESPs, or extract it into the Fallout 4 `Data` folder. The
archive places `F4SE/`, `Meshes/`, `textures/`, `materials/`, `Sound/`, `Scripts/`,
`PrismaUI_F4/` and the ESPs at its root.

`RadOutbreak.esp` adds the optional NPC and companion disease system (outbreaks, Doctor Station, Pathogen Scanner). `RadOverhaulSurPatch.esp` is an optional patch for **Survival Mode
Overhaul**: leave it disabled unless that mod is installed and enabled in the same profile.

## Known limits

- **Not tested in game by the owner before publishing.** Treat every item above as unverified
  in game.
- **Save compatibility with a save that held a dome reference is unverified in game.** The dome
  records and the `DomeZOffset` property are gone. The path that retires dome references before a
  load was not exercised.
- **Warning-phase dosing is not verified in game.** The vanilla radstorm sky rolls in at Active only.
- **The silent lead before the siren is now dead time.** The storm IMAD layers over the vanilla green
  look. The Papyrus rad-weather list in RadSurvivalDiseases does not include the vanilla sky (form 0x24A3C0).
- **The storm colour tint alpha values are first estimates.** The Ash engine tint is no longer applied.
- **The enclosed shelter values are not verified in game:** the 8 C floor, the 0.5 thermal shelter,
  the 75% wind cut and the ray counts.
- **God mode detection on AE is not checked in game.** It uses the commonlibf4-nomad ID for `PlayerCharacter::IsGodMode`; NG is not supported by the plugin.
- **The HUD cores were not seen in game.** How the Phosphor core rings render in PrismaUI, and whether
  the needs and filter widgets overlap at the PhosphorHUD companion default positions, are unchecked.
  The HUD has only contract tests and CI behind it.
- **The new gas mask sounds were not heard in game.** Old v1.2.0 to v1.2.4 sound files left in `Data` are unused but are not removed by installing.
- **The exposure and illness rings, the right-hand layout editor panel and the menu hiding were not seen in game.**
- **The new illnesses and the hypothermia penalties, health damage and hot-food warmth were not
  tried in game.** Hot-food warmth is not saved, so loading a save drops what is left of it.
- **Frame rate was not measured for this release.**
- The Papyrus bytecode (`.pex`) was compiled with FO4IDE's built-in engine, not the Creation Kit compiler.
- The `RadNatives` static native binding is unverified in game.
- Survival difficulty (`iDifficulty=6`) suspends the Papyrus needs layer, because vanilla survival owns
  the pools there. Use Very Hard or below to play the Rad Overhaul layers.
- **The NPC and companion outbreaks were not tried in game:** spread, deaths, Doctor Station,
  scanner markers and the NPC coughs.
- Companion exhaustion is not implemented, and vanilla doctors do not cure Rad Overhaul illnesses
  (the Doctor Station treats NPCs only). Permanent tissue
  scarring is cured only by the chelation compound.
- Showcase needs and disease scenes reset the touched pools and the illness to zero, not to your real values.
- Not in this release: the Firestorm art previews.

## In-game checks

In `FalloutAnomaly.log` look for:

- `[Tuning] Papyrus bridge registered (RadNatives.TuningOverride, RadNatives.TuningByName, global).` and
  `[Relief] Papyrus bridge registered (GetReliefFlags).`
- No `[Shelter] roof-fan` lines by default.
- `[ClimateFX] Papyrus bridge registered (RadNatives.WarmBody, global).`
- `[Outbreak] Papyrus bridge registered (RadNatives.IsQuestHeld, global).`

`Papyrus.0.log` should have no `Unbound native function`, `could find no matching static
function` or `does not match existing signature` lines for `RadNatives`, and no `cannot be
initialized` line for `RadSurvivalQuest`.

In a forced Ash Fallout or Toxic Sulfur storm (Shift+F8), the vanilla radstorm sky should appear only
once the storm is Active, not during the warning.

## Verification

- The tag workflow runs the Python contract suites (CI deployment isolation, masks, HUD, debug menu
  close, load settle, ghoul contract, storm spawn ownership, serialization, stack integration, survival,
  consumption, roofless climate, diseases, menu, needs globals, AV ownership, dome records removed,
  storm colour tints, Papyrus bytecode) and the C++ correctness-policy and serialization-policy
  executables before the archive is built.
- `verify_papyrus_pex.py` enforces the compiled Papyrus bytecode shipped in the archive.
- `verify_dome_visibility.py` is archived with the dome code and is no longer run by CI or the release workflow.
- The Windows MSVC build produces the DLL packaged here; the archive is assembled from the repository's
  `deploy/` tree plus that DLL, so source and package cannot drift.
- ESP structural validation (`tests/plugin_reader.py`) covers the survival records and the VMAD bindings,
  including the cross-script `Diseases` link the cure depends on.
