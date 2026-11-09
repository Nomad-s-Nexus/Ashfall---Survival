# Fallout Anomaly - Radiation Overhaul

A radiation and environmental survival overhaul for Fallout 4 supporting OG 1.10.163 and AE 1.11.240; NG 1.10.980-1.10.984 and other unverified runtimes are intentionally unsupported. The package includes an F4SE DLL +
`RadiationOverhaul.esp` + optional PrismaUI HUD. The core ESP references `Fallout4.esm`
(plus `DLCCoast` / `DLCNukaWorld` for merged overrides). The storm sky no longer uses a native dome, so the dome lights are no longer used and need no other plugin. The dome meshes, textures and credit file are no longer shipped, and the dome records were removed from the ESP.

Author: NomadsReach

## What it does

- **Twofold radiation.** Particulate rads (vanilla Rads AV, cleared by RadAway) plus a
  persistent **tissue damage** pool that survives saves, builds from sustained exposure,
  and is NOT cleared by RadAway. **Permanent (scarred) tissue** floors how far tissue can
  heal. Item-triggered tissue cures are currently disabled fail-closed until a
  source-identifying item-use adapter is verified.
- **10-stage severity** (100 to 1000) drives SPECIAL penalties (END/STR) and Pip-Boy stage perks.
- **STALKER-style storms.** Ash Fallout and Rad Firestorm (lethal, shelter-or-die) plus Toxic
  Sulfur Storm, Acid Rain, and Dust Storm, with APPROACH / WARNING / ACTIVE / AFTERMATH phases,
  siren audio, shelter detection, storm atmosphere, and native storm FX.
- **Environmental survival expansion.** The native cold-survival policy core models body
  temperature, wetness, insulation, shelter, heat, activity, and reversible hypothermia stages.
  Runtime acquisition of weather, water exposure, armor insulation, and heat sources is being
  added behind verified engine inputs rather than guessed native calls.
- **Cures.** Sleep/wait/fast-travel can regress reducible tissue over elapsed game time. RadAway
  and the craftable **Radiation Chelation Compound** do not currently change tissue damage: their
  item-triggered effects are disabled fail-closed pending a verified source-identifying item-use adapter.
- **PrismaUI HUD** (optional, degrades to no-op without the framework): gas-mask visor, storm
  banners, debug controls, and Rad-owned toxicity/filter widgets. The Rad debug menu includes a
  temporary built-in layout editor for those widgets. When PrismaHUD_F4 is detected, PrismaHUD
  automatically becomes the layout owner and its shared editor takes over.
- **Needs (vanilla Papyrus quest).** Hunger, thirst and sleep debt advance on a game-time timer,
  persist with the quest in the save, and are relieved by real food/drink use and real sleep.
  Consumption is confirmed by the engine's item-removal event paired with the equip, not by a count
  change, so dropping, storing, selling and crafting cannot feed the player. **This layer now needs
  the F4SE plugin (RadiationOverhaul) loaded**: the quest calls its native relief bridge for the
  `[Relief]` INI extras, and without the DLL loaded those extras stay off and each ingestible equip
  logs one unbound-native error (the vanilla-keyword pools still work). The quest script owns the
  rates and mirrors every change into
  `RadSurvival_Hunger`/`_Thirst`/`_Sleep` plus `RadSurvival_Risk`/`_Disease`; the native HUD reads
  those five `GLOB` records through `TESGlobal`, so there is no native clock and no needs co-save
  record. `HC_Manager` and every `HC_*` form are untouched, and the layer suspends itself while
  vanilla Survival is active. No script extender is required. Staged hunger/thirst/exhaustion
  abilities and the disease layer are live.
- **Companion patches.** `RadOverhaul_GearPatch.esp` buffs the hazmat suit's radiation resistance;
  `RadOverhaulSurPatch.esp` is also included in the deployment package.
- **Third-party gas masks.** Masks from other mods are declared in
  `F4SE/Plugins/FalloutAnomalyRadiation.ini` under `[Masks]` (`ExtraKeywords` / `ExtraMasks`), so their
  wearers get the same filter, condition and choking protection without editing those records.
  Gasmask Overhaul (Nexus 108845) and its PPM-88 are configured there by default.

## Layout

```
├── src/              C++ F4SE plugin source
├── data/             runtime configuration loaded by the DLL
├── deploy/           game-ready package
│   ├── F4SE/         plugin config/runtime files
│   ├── PrismaUI_F4/  HUD views and assets
│   ├── Scripts/      deployed Papyrus sources/compiled scripts
│   ├── Sound/        runtime audio
│   ├── Meshes/ materials/ textures/  runtime visual assets
│   ├── RadiationOverhaul.esp
│   ├── RadOverhaul_GearPatch.esp
│   └── RadOverhaulSurPatch.esp
├── docs/             changelog, proof records and captured logs
├── NOTICE.md         third-party notices/credits
└── xmake.lua         DLL build definition
```

## Build the DLL

```powershell
cd path\to\Ashfall
xmake
# output: build\release\FalloutAnomalyRadiation.dll
```

Close Fallout 4 before redeploying the DLL because a loaded DLL is file-locked.

`xmake.lua` can auto-copy a successful build to a live mod directory when `XSE_FO4_MODS_PATH` is set.
The local development fallback also covers `C:\Modlists\TMR\mods\Fallout Anomaly - Radiation Overhaul`.

## Build the ESP

The ESP/Spriggit build pipeline notes are no longer kept in this repository.
The historical `tooling/` tree referenced by older notes is not part of the current repository root.

## CI

Pull requests stay on this private repository. CI runs on the public repository `Nomad-s-Nexus/Ashfall---Survival`:
a workflow exports an allowlisted snapshot to its `ci/pr-N` branch (`ci/main` for pushes to `main`) and the public run
posts a `public-ci` status back on the PR commit. This needs the `PUBLIC_CI_TOKEN` secret on this repository and on the
public repository's `ashfall-ci` environment. `tests/verify_av_ownership.py` is private-only and runs in `private-checks.yml`.

## Docs

- [docs/CHANGELOG.md](docs/CHANGELOG.md) - dated change history
- [docs/guide/illness.md](docs/guide/illness.md) - player guide to illness, cold, radiation sickness and NPC outbreaks
