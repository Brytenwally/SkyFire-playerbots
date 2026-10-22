/*
 * SkyFire playerbots — level/faction gates and init scatter teleports.
 *
 * Summon / follow-to-master is unrestricted (the master chose the place).
 * Random / init scatter teleports must call CanBotTeleportTo() / TeleportForLevel().
 *
 * Destinations come from creature spawns (innkeepers and flight masters), the
 * same source mod-playerbots uses for RandomTeleportForLevel, and each spot is
 * rejected unless the map tile exists, ground height resolves, and the area
 * belongs to that map. A teleport that does not actually change map is aborted
 * so foreign coordinates are not left on the bot's old continent.
 */

#ifndef _SF_BOT_TELEPORT_MAPS_H
#define _SF_BOT_TELEPORT_MAPS_H

#include "Define.h"

class Player;

namespace BotTeleportMaps
{
    // Expansion / starter continents by bot level:
    //   always : Eastern Kingdoms, Kalimdor, Outland (incl. Draenei/BE starts),
    //            worgen/goblin/pandaren starts (race-gated in CanBotTeleportTo)
    //   68+    : Northrend
    //   82+    : Deepholm
    //   85+    : Pandaria
    bool IsMapAllowedForLevel(uint32 mapId, uint32 level);

    // Hostile capital zones (Orgrimmar for Alliance, Stormwind for Horde, etc.).
    bool IsHostileCapitalZone(uint32 zoneId, uint32 team /* ALLIANCE or HORDE */);

    // True when map is in the level band, the zone is not an enemy capital, and
    // race-exclusive starts (Gilneas / Kezan / Lost Isles / Wandering Isle)
    // match the bot's race.
    bool CanBotTeleportTo(Player const* bot, uint32 mapId, uint32 zoneId = 0);

    // Pick a safe open-world anchor for the bot's *current* level and faction,
    // then TeleportTo + FinalizeBotTeleport. Call after relevel/init so the
    // destination matches the post-init level.
    //
    // Levels 1–5 stay at race spawn when that tile is real. A stuck position
    // (missing map tile, no ground, unknown area) is moved. Explicit player
    // `.playerbots init … tele` sends levels 1–5 to homebind / race create.
    bool TeleportForLevel(Player* bot, bool fromPlayerCommand = false);

    // Hearth / homebind location (falls back to race create coords).
    // Refuses a homebind that is not on a real tile for its map.
    bool TeleportToHomebind(Player* bot);
}

#endif
