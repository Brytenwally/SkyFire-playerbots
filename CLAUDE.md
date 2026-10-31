# CLAUDE.md

Notes for Claude sessions working on SkyFire-playerbots. **Keep this file up to
date**: when a session learns something future sessions need (a new subsystem,
a build gotcha, a ported feature, a changed rule), add it here in the same
commit.

## What this is

A port of the AzerothCore playerbots system (3.3.5 / WotLK) to **SkyFire 5.4.8
(Mists of Pandaria)**. The goal is a test server populated with bot players
(LFG, LFR, battlegrounds, arenas, open world).

## The repositories

| Repo | Role | Rules |
| --- | --- | --- |
| `Brytenwally/SkyFire-playerbots` (this repo) | The 5.4.8 module. Lives in the core at `modules/mod-playerbots`. | Where port work goes. |
| `Brytenwally/SkyFire_548` | The 5.4.8 core, a fork of Project SkyFire. Has the module system and the bot hooks. | Change only for hooks the module cannot do without, or to fix core bugs the owner hits (e.g. database setup). |
| `Brytenwally/SkyFire-build` | GitHub Actions workflow that builds Windows x64 binaries (core + this module, MySQL 8.0.44 client). | Has its own `CLAUDE.md`. |
| `Brytenwally/mod-playerbots` (branch `coa`) | AzerothCore 3.3.5 playerbots, Conquest of Azeroth fork. | **Read-only reference.** Never build it, never change or push to it, never compile it against 5.4.8. Read it to see what a feature does, then rewrite it for 5.4.8 here. |

## Git rules

- Commit and push straight to **`main`** in SkyFire-playerbots, SkyFire_548 and
  SkyFire-build. No feature branches or PRs unless the owner asks for one.
- Commit messages: imperative subject line, body explaining what and why.

## Layout

```
src/
  PlayerbotMgr.*        bot lifecycle, config loading, random pool, auto-create,
                        InitializeBot, gear selection, autogear/maintenance
  PlayerbotAI.*         per-bot AI tick, chat orders (HandleChatCommand), co/nc
  PlayerbotScripts.cpp  ScriptMgr hooks and the `.playerbots` GM commands
  mod_playerbots_loader.cpp  Addmod_playerbotsScripts() (name must match folder)
  engine/               strategy engine, triggers, movement, formation, flee, targets
  rotations/            per-class combat rotations, talents (BotTalents), glyphs
  BotGearSets.*         PvE set worn, PvP set kept in a gear bag (entry 23162),
                        one piece per bag position = equipment slot
  BotTeleportMaps.*, BotVendorHubs.*, BotPreferredMounts.*
conf/playerbots.conf.dist   every config key, documented
sql/characters/             character DB tables the module needs
COMMANDS.md   every GM command and chat order (update when adding one)
PORTING.md    phase plan and what is DONE / not ported (update when porting)
rotation.md   rotation priority lists the rotations/ code follows
```

`PlayerbotMgr.cpp` keeps most helpers (gear, spells, mounts, pets) in an
anonymous namespace; free functions there are file-local.

## Core hooks the module relies on (in SkyFire_548)

- Module system: each `modules/<name>/src` is compiled into the worldserver;
  `AddModulesScripts()` calls `Add<name>Scripts()`. See `modules/README.md`.
- `WorldSession::IsBot()` / socketless bot sessions.
- `PlayerScript::OnUpdate(Player*, uint32)` via `ScriptMgr::OnPlayerUpdate`.
- Bot-aware edits also exist in Map, Group, LFG and Player code (grep `IsBot`).

## Porting a feature from mod-playerbots

1. Find it in the reference: actions in `src/Ai/Base/Actions`, strategies and
   triggers in `src/Ai/Base/Strategy|Trigger`, gear/spell init in
   `src/Bot/Factory/PlayerbotFactory.cpp`, config in `conf/playerbots.conf.dist`
   (`AiPlayerbot.*` keys).
2. Rewrite it for 5.4.8 APIs here. Do not copy WotLK assumptions: no ranged
   slot (bows are main hand), no ammo/keyring, specializations
   (`ChrSpecialization`) and tiered talents instead of talent trees, MoP item
   levels (~460+ at 90).
3. Config keys use the `Playerbots.*` prefix, loaded in
   `PlayerbotMgr::LoadConfig`, documented in `conf/playerbots.conf.dist`.
4. Update `COMMANDS.md`, `PORTING.md` (and README if user-facing).

### Already ported

- `autogear [reset] [match|<quality>|<ilvl>]`: `PlayerbotMgr::AutoGearBot`.
  Incremental slot-by-slot upgrade scored by item level (half for another
  spec's primary stat); old piece to bags, never the gear bag. Config
  `Playerbots.AutoGear.*`. `autogear bis` not ported (needs BiS tables).
- `maintenance`: `PlayerbotMgr::MaintainBot`. Spells, proficiencies, talents,
  glyphs, mounts, hunter pets, repair; keeps spec and gear. Config
  `Playerbots.Maintenance.Command`, `Playerbots.AltMaintenance.*`.
  Not ported: enchants/gems, consumables, bags, reputation, pet talents.
- `quests all|completed|incompleted|summary`: `PlayerbotAI::HandleQuestListCommand`
  (AC ListQuestsAction, plus objective progress). Bare `quests` stays
  SkyFire's accept / turn in. AC's `travel` variants need TravelMgr (absent).
- `.playerbots add` is async (`BeginSpawnBot` + `ProcessPendingLogins`, paced
  by `RandomBots.LoginsPerTick`); `Bot 'X' logged in.` is sent to the issuer
  by `NotifyAddResult` when the character is loaded. SquidBots Lite parses that
  exact text: keep the wording.
- Alt bot = not in the random pool and not on an AccountPrefix account
  (`PlayerbotMgr::IsAltBot`, account name checked by `IsBotAccount`, cached).
  **Rule from the owner: nothing changes an alt unless the player orders it.**
  No automatic init/gear/spec/teleport (`BotNeedsGearInit` returns false for
  alts) and idle vendoring sells only greys (more only on `sell all`). Keep
  new automatic behaviour off for alts too.
- Automatic init (ungeared sweep / first login) runs at most once per bot per
  session (`_autoInited`): low-level bots can stay in whites after an init.

## Core fixes made in SkyFire_548

- Database setup (`src/server/shared/Database/DatabaseSetup/`): updates the
  base SQL already records in `skyfire_db_updates` are skipped instead of
  re-run (fresh characters DB failed with "Duplicate entry
  'characters-2026_08_03_characters_00.sql'").

## Building and checking

- Core requires **g++ 14+** (C++23), **Boost 1.91**, OpenSSL, MySQL client.
  Ubuntu 24.04 apt only has Boost 1.83, so a full CMake configure fails there.
- Quick check in a Linux container: syntax-only compile of changed files
  against the core headers (works with apt's Boost 1.83):

  ```sh
  apt-get install -y g++-14 libboost-all-dev libssl-dev libmysqlclient-dev
  INC=$(find /path/SkyFire_548/src/server /path/SkyFire_548/src/common /path/SkyFire_548/dep -type d | sed 's|^|-I|')
  mkdir -p /tmp/gen && touch /tmp/gen/revision.h
  g++-14 -std=c++23 -fsyntax-only -Wall -Wextra -I/tmp/gen -Isrc $INC -I/usr/include/mysql src/PlayerbotMgr.cpp
  ```

- Real Windows builds: run the SkyFire-build workflow (Actions tab, or push a
  `v*` tag there).
- There is no server or database in the cloud container: runtime behaviour
  cannot be tested there. Say so when reporting.

## Code style

Match the surrounding code: 4-space indent, Allman braces, `SF_LOG_INFO("modules",
"[mod-playerbots] ...")` for logs, short comments that explain why, core types
(`uint32`, `Player*`, `sObjectMgr`, `WorldDatabase.PQuery`).
