/*
 * SkyFire playerbots — level/faction gates and init scatter teleports.
 *
 * Location list follows mod-playerbots RandomPlayerbotMgr::PrepareTeleportCache
 * (innkeeper / flight master spawns, faction mask, zone level brackets) and
 * RandomTeleport's spot checks (map loaded, area exists, not hostile, not
 * underwater, ground height).
 */

#include "BotTeleportMaps.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GridDefines.h"
#include "Log.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
    constexpr uint32 MAP_KALIMDOR = 1;
    constexpr uint32 MAP_OUTLAND = 530;
    constexpr uint32 MAP_NORTHREND = 571;
    constexpr uint32 MAP_GILNEAS = 654;
    constexpr uint32 MAP_GILNEAS_CITY = 655;
    constexpr uint32 MAP_KEZAN = 648;
    constexpr uint32 MAP_LOST_ISLES = 649;
    constexpr uint32 MAP_DEEPHOLM = 646;
    constexpr uint32 MAP_WANDERING_ISLE = 860;
    constexpr uint32 MAP_PANDARIA = 870;

    // UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_FLIGHTMASTER (same mask as upstream).
    constexpr uint32 NPC_FLAG_TRAVEL_HUB = 0x12000;
    constexpr uint8 kScatterMinLevel = 6;
    constexpr uint32 kMaxTeleportTries = 24;

    struct ZoneBracket
    {
        uint32 zoneId;
        uint8 minLevel;
        uint8 maxLevel;
    };

    // Upstream zone2LevelBracket, plus Cataclysm / Pandaria / starter continents
    // so levels that fall between those brackets still have a real tile.
    ZoneBracket const kZoneBrackets[] =
    {
        { 1, 5, 12 }, { 12, 5, 12 }, { 14, 5, 12 }, { 85, 5, 12 },
        { 141, 5, 12 }, { 215, 5, 12 }, { 3430, 5, 12 }, { 3524, 5, 12 },
        { 17, 10, 25 }, { 38, 10, 20 }, { 40, 10, 21 }, { 130, 10, 23 },
        { 148, 10, 21 }, { 3433, 10, 22 }, { 3525, 10, 21 },
        { 10, 19, 33 }, { 11, 21, 30 }, { 44, 16, 28 }, { 267, 20, 34 },
        { 331, 18, 33 }, { 400, 24, 36 }, { 406, 16, 29 },
        { 3, 36, 46 }, { 8, 36, 46 }, { 15, 35, 46 }, { 16, 45, 52 },
        { 33, 32, 47 }, { 45, 30, 42 }, { 47, 42, 51 }, { 51, 45, 51 },
        { 357, 40, 52 }, { 405, 30, 41 }, { 440, 41, 52 },
        { 4, 52, 57 }, { 28, 50, 60 }, { 46, 51, 60 }, { 139, 54, 62 },
        { 361, 47, 57 }, { 490, 49, 56 }, { 618, 54, 61 }, { 1377, 54, 63 },
        { 3483, 58, 66 }, { 3518, 64, 70 }, { 3519, 62, 73 }, { 3520, 66, 73 },
        { 3521, 60, 67 }, { 3522, 64, 73 }, { 3523, 67, 73 }, { 4080, 68, 73 },
        { 65, 71, 77 }, { 66, 74, 80 }, { 67, 77, 80 }, { 210, 77, 80 },
        { 394, 72, 78 }, { 495, 68, 74 }, { 2817, 77, 80 }, { 3537, 68, 75 },
        { 3711, 75, 80 }, { 4197, 79, 80 },
        { 616, 80, 83 }, { 5042, 82, 84 }, { 5034, 83, 85 }, { 4922, 84, 85 },
        { 5144, 80, 82 }, { 5145, 80, 82 }, { 5146, 80, 82 },
        { 5785, 85, 87 }, { 5805, 86, 88 }, { 6134, 86, 88 }, { 5841, 87, 89 },
        { 5842, 88, 90 }, { 6138, 89, 90 }, { 5840, 90, 90 }, { 6757, 90, 90 },
        { 5736, 1, 15 }, { 4714, 1, 15 }, { 4755, 1, 15 },
        { 4737, 1, 12 }, { 4720, 5, 15 },
    };

    struct SafeAnchor
    {
        uint32 mapId;
        float x;
        float y;
        float z;
        float o;
        uint32 zoneId;
        bool forAlliance;
        bool forHorde;
        uint8 minLevel;
        uint8 maxLevel;
    };

    std::vector<SafeAnchor> g_anchors;
    bool g_anchorsReady = false;

    bool InSet(std::initializer_list<uint32> list, uint32 mapId)
    {
        for (uint32 id : list)
            if (id == mapId)
                return true;
        return false;
    }

    bool IsPandarenRace(uint8 race)
    {
        return race == RACE_PANDAREN_NEUTRAL
            || race == RACE_PANDAREN_ALLIANCE
            || race == RACE_PANDAREN_HORDE;
    }

    bool CanRaceUseStartMap(uint8 race, uint32 mapId)
    {
        switch (mapId)
        {
            case MAP_GILNEAS:
            case MAP_GILNEAS_CITY:
                return race == RACE_WORGEN;
            case MAP_KEZAN:
            case MAP_LOST_ISLES:
                return race == RACE_GOBLIN;
            case MAP_WANDERING_ISLE:
                return IsPandarenRace(race);
            default:
                return true;
        }
    }

    // Map::ExistMap logs "Map file ... does not exist!" for every miss, and open
    // ocean / void tiles are simply not shipped, so probe the file quietly.
    bool MapTileExists(uint32 mapId, float x, float y)
    {
        GridCoord p = Skyfire::ComputeGridCoord(x, y);
        int const gx = 63 - int(p.x_coord);
        int const gy = 63 - int(p.y_coord);

        char tile[32];
        snprintf(tile, sizeof(tile), "maps/%04u_%02u_%02u.map", mapId, gx, gy);
        std::string const path = sWorld->GetDataPath() + tile;
        if (FILE* f = fopen(path.c_str(), "rb"))
        {
            fclose(f);
            return true;
        }
        return false;
    }

    bool BracketForZone(uint32 zoneId, uint8& minLevel, uint8& maxLevel)
    {
        for (ZoneBracket const& b : kZoneBrackets)
        {
            if (b.zoneId != zoneId)
                continue;
            minLevel = b.minLevel;
            maxLevel = b.maxLevel;
            return true;
        }
        return false;
    }

    bool FactionAllowsTeam(uint32 factionGroupMask, uint32 team)
    {
        if (factionGroupMask == FACTION_MASK_ALLIANCE && team != ALLIANCE)
            return false;
        if (factionGroupMask == FACTION_MASK_HORDE && team != HORDE)
            return false;
        return true;
    }

    // True when (map, x, y, z) is a real walkable spot on that map.
    // groundZ is snapped onto the terrain. zoneId is the parent zone.
    bool ResolveSpot(uint32 mapId, float x, float y, float z, float& groundZ, uint32& zoneId)
    {
        groundZ = z;
        zoneId = 0;

        if (!MapManager::IsValidMapCoord(mapId, x, y, z))
            return false;
        if (!MapTileExists(mapId, x, y))
            return false;

        Map* map = sMapMgr->CreateBaseMap(mapId);
        if (!map)
            return false;

        float const ground = map->GetHeight(1, x, y, z + 2.0f, true, 50.0f);
        if (ground <= INVALID_HEIGHT)
            return false;
        // Don't snap a bad Z down onto an unrelated cave or the ocean floor.
        if (std::fabs(ground - z) > 25.0f)
            return false;

        uint16 const areaFlag = map->GetAreaFlag(x, y, ground + 2.0f);
        AreaTableEntry const* area = GetAreaEntryByAreaFlagAndMap(areaFlag, mapId);
        if (!area || area->m_ContinentID != mapId)
            return false;

        uint32 const zone = area->m_ParentAreaID ? area->m_ParentAreaID : area->m_ID;
        AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zone);
        if (zoneEntry && zoneEntry->m_ContinentID && zoneEntry->m_ContinentID != mapId)
            return false;

        if (map->IsInWater(x, y, ground + 1.5f))
            return false;

        groundZ = ground + 0.05f;
        zoneId = zone;
        return true;
    }

    bool NpcFactionAllows(uint32 factionId, bool& forAlliance, bool& forHorde)
    {
        forAlliance = true;
        forHorde = true;
        if (!factionId)
            return true;
        FactionTemplateEntry const* entry = sFactionTemplateStore.LookupEntry(factionId);
        if (!entry)
            return true;
        forAlliance = (entry->hostileMask & FACTION_MASK_ALLIANCE) == 0;
        forHorde = (entry->hostileMask & FACTION_MASK_HORDE) == 0;
        return forAlliance || forHorde;
    }

    void AddAnchor(SafeAnchor const& anchor, std::unordered_set<uint64>& seen)
    {
        int32 const cx = int32(std::floor(anchor.x / 45.0f));
        int32 const cy = int32(std::floor(anchor.y / 45.0f));
        uint64 const key = (uint64(anchor.mapId) << 42)
            | (uint64(uint32(cx) & 0x1FFFFF) << 21)
            | uint64(uint32(cy) & 0x1FFFFF);
        if (!seen.insert(key).second)
            return;
        g_anchors.push_back(anchor);
    }

    void AddCreatePositions(std::unordered_set<uint64>& seen)
    {
        for (uint8 race = RACE_HUMAN; race <= RACE_PANDAREN_HORDE; ++race)
        {
            for (uint8 cls = CLASS_WARRIOR; cls <= CLASS_DRUID; ++cls)
            {
                PlayerInfo const* info = sObjectMgr->GetPlayerInfo(race, cls);
                if (!info)
                    continue;

                float ground = info->positionZ;
                uint32 zone = 0;
                if (!ResolveSpot(info->mapId, info->positionX, info->positionY, info->positionZ, ground, zone))
                    continue;

                uint8 minLevel = 1;
                uint8 maxLevel = 10;
                uint8 bracketMin = 0;
                uint8 bracketMax = 0;
                if (BracketForZone(zone, bracketMin, bracketMax))
                {
                    minLevel = 1;
                    maxLevel = bracketMax;
                }

                SafeAnchor anchor;
                anchor.mapId = info->mapId;
                anchor.x = info->positionX;
                anchor.y = info->positionY;
                anchor.z = ground;
                anchor.o = info->orientation;
                anchor.zoneId = zone;
                anchor.forAlliance = (race == RACE_HUMAN || race == RACE_DWARF || race == RACE_NIGHTELF
                    || race == RACE_GNOME || race == RACE_DRAENEI || race == RACE_WORGEN
                    || race == RACE_PANDAREN_ALLIANCE);
                anchor.forHorde = (race == RACE_ORC || race == RACE_UNDEAD_PLAYER || race == RACE_TAUREN
                    || race == RACE_TROLL || race == RACE_BLOODELF || race == RACE_GOBLIN
                    || race == RACE_PANDAREN_HORDE);
                if (race == RACE_PANDAREN_NEUTRAL)
                {
                    anchor.forAlliance = true;
                    anchor.forHorde = true;
                }
                anchor.minLevel = minLevel;
                anchor.maxLevel = maxLevel;
                AddAnchor(anchor, seen);
                break;
            }
        }
    }

    void BuildAnchorCache()
    {
        if (g_anchorsReady)
            return;
        g_anchorsReady = true;
        g_anchors.clear();

        std::unordered_set<uint64> seen;
        QueryResult result = WorldDatabase.Query(
            "SELECT c.map, c.position_x, c.position_y, c.position_z, c.orientation, ct.faction_A, ct.entry "
            "FROM creature c INNER JOIN creature_template ct ON c.id = ct.entry "
            "WHERE ((ct.npcflag | c.npcflag) & 73728) <> 0 "
            "AND c.map IN (0, 1, 530, 571, 646, 648, 649, 654, 655, 860, 870) "
            "AND c.phaseid = 0 AND c.phasegroup = 0 AND (c.spawnMask & 1) <> 0");

        uint32 accepted = 0;
        uint32 skipped = 0;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 const mapId = fields[0].GetUInt16();
                float const x = fields[1].GetFloat();
                float const y = fields[2].GetFloat();
                float const z = fields[3].GetFloat();
                float const o = fields[4].GetFloat();
                uint32 const faction = fields[5].GetUInt32();
                uint32 const entry = fields[6].GetUInt32();

                // Upstream skips these flight masters; their spawn points are bad.
                if (entry == 3838 || entry == 29480)
                    continue;

                bool forAlliance = false;
                bool forHorde = false;
                if (!NpcFactionAllows(faction, forAlliance, forHorde))
                {
                    ++skipped;
                    continue;
                }

                float standX = x + std::cos(o) * 3.0f;
                float standY = y + std::sin(o) * 3.0f;
                float ground = z;
                uint32 zone = 0;
                if (!ResolveSpot(mapId, standX, standY, z, ground, zone))
                {
                    standX = x;
                    standY = y;
                    if (!ResolveSpot(mapId, standX, standY, z, ground, zone))
                    {
                        ++skipped;
                        continue;
                    }
                }

                uint8 minLevel = 0;
                uint8 maxLevel = 0;
                if (!BracketForZone(zone, minLevel, maxLevel))
                {
                    ++skipped;
                    continue;
                }

                SafeAnchor anchor;
                anchor.mapId = mapId;
                anchor.x = standX;
                anchor.y = standY;
                anchor.z = ground;
                anchor.o = o + 3.14159265f;
                anchor.zoneId = zone;
                anchor.forAlliance = forAlliance;
                anchor.forHorde = forHorde;
                anchor.minLevel = minLevel;
                anchor.maxLevel = maxLevel;
                AddAnchor(anchor, seen);
                ++accepted;
            } while (result->NextRow());
        }
        else
            SF_LOG_ERROR("modules", "[mod-playerbots] Init-teleport cache query failed; using race starts only.");

        AddCreatePositions(seen);

        SF_LOG_INFO("modules",
            "[mod-playerbots] Init-teleport cache: %u hubs accepted, %u rejected, %u total spots.",
            accepted, skipped, uint32(g_anchors.size()));
    }

    void IdleBot(Player* bot)
    {
        if (MotionMaster* mm = bot->GetMotionMaster())
        {
            mm->Clear();
            mm->MoveIdle();
        }
    }

    bool FinishTeleport(Player* bot)
    {
        if (WorldSession* session = bot->GetSession())
            if (session->IsBot() && bot->IsBeingTeleported())
                session->FinalizeBotTeleport();
        return bot->IsInWorld() && !bot->IsBeingTeleported();
    }

    // Cross-map TeleportTo can Relocate onto the destination coordinates and then
    // fail to switch maps. Never leave those coordinates on the old map.
    bool CommitTeleport(Player* bot, uint32 mapId, float x, float y, float z, float o)
    {
        if (!bot->IsInWorld())
            return false;

        if (bot->IsBeingTeleported())
            FinishTeleport(bot);

        uint32 const fromMap = bot->GetMapId();
        float const fromX = bot->GetPositionX();
        float const fromY = bot->GetPositionY();
        float const fromZ = bot->GetPositionZ();
        float const fromO = bot->GetOrientation();

        if (!bot->TeleportTo(mapId, x, y, z, o) || !FinishTeleport(bot) || bot->GetMapId() != mapId)
        {
            if (bot->IsInWorld() && (bot->GetMapId() != fromMap
                || std::fabs(bot->GetPositionX() - fromX) > 1.0f
                || std::fabs(bot->GetPositionY() - fromY) > 1.0f))
            {
                SF_LOG_ERROR("modules",
                    "[mod-playerbots] Init-teleport of '%s' did not arrive on map %u (now map %u). Reverting.",
                    bot->GetName().c_str(), mapId, bot->GetMapId());
                if (bot->TeleportTo(fromMap, fromX, fromY, fromZ, fromO))
                    FinishTeleport(bot);
            }
            return false;
        }

        uint32 zone = 0;
        uint32 area = 0;
        bot->GetZoneAndAreaId(zone, area);
        if (!area)
        {
            SF_LOG_ERROR("modules",
                "[mod-playerbots] Init-teleport of '%s' landed on map %u with no area (%.1f, %.1f, %.1f). Reverting.",
                bot->GetName().c_str(), mapId, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            if (bot->TeleportTo(fromMap, fromX, fromY, fromZ, fromO))
                FinishTeleport(bot);
            return false;
        }

        IdleBot(bot);
        return true;
    }

    bool CurrentSpotIsSafe(Player const* bot)
    {
        float ground = 0.0f;
        uint32 zone = 0;
        return ResolveSpot(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
            ground, zone);
    }
}

namespace BotTeleportMaps
{
    bool IsMapAllowedForLevel(uint32 mapId, uint32 level)
    {
        if (InSet({ MAP_EASTERN_KINGDOMS, MAP_KALIMDOR, MAP_OUTLAND,
                MAP_GILNEAS, MAP_GILNEAS_CITY, MAP_KEZAN, MAP_LOST_ISLES,
                MAP_WANDERING_ISLE }, mapId))
            return true;

        if (mapId == MAP_NORTHREND)
            return level >= 68;
        if (mapId == MAP_DEEPHOLM)
            return level >= 82;
        if (mapId == MAP_PANDARIA)
            return level >= 85;
        return false;
    }

    bool IsHostileCapitalZone(uint32 zoneId, uint32 team)
    {
        if (!zoneId)
            return false;

        static uint32 const hordeCaps[] = {
            1637, 1638, 1497, 3487, 14, 215, 85, 3430,
        };
        static uint32 const allianceCaps[] = {
            1519, 1537, 1657, 3557, 12, 1, 141, 3524, 4714,
        };

        auto has = [](uint32 const* arr, size_t n, uint32 z) -> bool
        {
            for (size_t i = 0; i < n; ++i)
                if (arr[i] == z)
                    return true;
            return false;
        };

        if (team == ALLIANCE)
            return has(hordeCaps, sizeof(hordeCaps) / sizeof(hordeCaps[0]), zoneId);
        if (team == HORDE)
            return has(allianceCaps, sizeof(allianceCaps) / sizeof(allianceCaps[0]), zoneId);
        return false;
    }

    bool CanBotTeleportTo(Player const* bot, uint32 mapId, uint32 zoneId)
    {
        if (!bot)
            return false;
        if (!IsMapAllowedForLevel(mapId, bot->getLevel()))
            return false;
        if (!CanRaceUseStartMap(bot->getRace(), mapId))
            return false;
        if (zoneId && IsHostileCapitalZone(zoneId, bot->GetTeam()))
            return false;
        return true;
    }

    bool TeleportToHomebind(Player* bot)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return false;

        uint32 mapId = bot->m_homebindMapId;
        float x = bot->m_homebindX;
        float y = bot->m_homebindY;
        float z = bot->m_homebindZ;
        float o = bot->GetOrientation();

        float ground = z;
        uint32 zone = 0;
        bool const homeOk = ResolveSpot(mapId, x, y, z, ground, zone)
            && CanBotTeleportTo(bot, mapId, zone)
            && FactionAllowsTeam(
                GetAreaEntryByAreaID(zone) ? GetAreaEntryByAreaID(zone)->m_FactionGroupMask : 0,
                bot->GetTeam());

        if (!homeOk)
        {
            PlayerInfo const* info = sObjectMgr->GetPlayerInfo(bot->getRace(), bot->getClass());
            if (!info || !ResolveSpot(info->mapId, info->positionX, info->positionY, info->positionZ, ground, zone))
                return false;
            if (!CanBotTeleportTo(bot, info->mapId, zone))
                return false;
            mapId = info->mapId;
            x = info->positionX;
            y = info->positionY;
            o = info->orientation;
        }

        z = ground;
        if (!CommitTeleport(bot, mapId, x, y, z, o))
            return false;

        SF_LOG_INFO("modules",
            "[mod-playerbots] Init-teleport '%s' -> homebind map %u (%.1f, %.1f, %.1f) level %u.",
            bot->GetName().c_str(), mapId, x, y, z, uint32(bot->getLevel()));
        return true;
    }

    bool TeleportForLevel(Player* bot, bool fromPlayerCommand)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return false;

        uint8 const level = bot->getLevel();
        if (level < kScatterMinLevel)
        {
            if (!fromPlayerCommand && CurrentSpotIsSafe(bot))
                return false;
            return TeleportToHomebind(bot);
        }

        BuildAnchorCache();

        uint32 const team = bot->GetTeam();
        uint8 const race = bot->getRace();

        std::vector<SafeAnchor const*> picks;
        picks.reserve(32);
        for (SafeAnchor const& a : g_anchors)
        {
            if (level < a.minLevel || level > a.maxLevel)
                continue;
            if (team == ALLIANCE && !a.forAlliance)
                continue;
            if (team == HORDE && !a.forHorde)
                continue;
            if (!CanBotTeleportTo(bot, a.mapId, a.zoneId))
                continue;
            if (!CanRaceUseStartMap(race, a.mapId))
                continue;
            if (AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(a.zoneId))
                if (!FactionAllowsTeam(zoneEntry->m_FactionGroupMask, team))
                    continue;
            picks.push_back(&a);
        }

        if (picks.empty())
        {
            SF_LOG_WARN("modules",
                "[mod-playerbots] No init-teleport anchor for '%s' (level %u, team %u).",
                bot->GetName().c_str(), uint32(level), team);
            if (!CurrentSpotIsSafe(bot))
                return TeleportToHomebind(bot);
            return false;
        }

        std::mt19937 rng{ std::random_device{}() };
        std::shuffle(picks.begin(), picks.end(), rng);

        uint32 const tries = std::min<uint32>(kMaxTeleportTries, uint32(picks.size()));
        for (uint32 i = 0; i < tries; ++i)
        {
            SafeAnchor const* dest = picks[i];
            if (!CommitTeleport(bot, dest->mapId, dest->x, dest->y, dest->z, dest->o))
                continue;

            SF_LOG_INFO("modules",
                "[mod-playerbots] Init-teleport '%s' -> map %u zone %u (%.1f, %.1f, %.1f) level %u.",
                bot->GetName().c_str(), dest->mapId, dest->zoneId, dest->x, dest->y, dest->z, uint32(level));
            return true;
        }

        SF_LOG_WARN("modules",
            "[mod-playerbots] Init-teleport gave up for '%s' after %u spots (level %u).",
            bot->GetName().c_str(), tries, uint32(level));
        if (!CurrentSpotIsSafe(bot))
            return TeleportToHomebind(bot);
        return false;
    }

    bool IsOnKnownGround(Player const* bot)
    {
        if (!bot || !bot->IsInWorld() || bot->IsBeingTeleported() || bot->IsInFlight() || bot->GetTransport())
            return true;

        Map const* map = bot->GetMap();
        if (!map || map->Instanceable())
            return true;

        uint32 const mapId = bot->GetMapId();
        if (!MapTileExists(mapId, bot->GetPositionX(), bot->GetPositionY()))
            return false;

        AreaTableEntry const* area = GetAreaEntryByAreaID(bot->GetAreaId());
        return area && area->m_ContinentID == mapId;
    }

    bool RescueStrandedBot(Player* bot)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return false;

        SF_LOG_ERROR("modules",
            "[mod-playerbots] Bot '%s' (GUID %u, level %u) stranded on map %u at (%.1f, %.1f, %.1f) area %u; "
            "homebind map %u (%.1f, %.1f). Sending home.",
            bot->GetName().c_str(), bot->GetGUIDLow(), uint32(bot->getLevel()), bot->GetMapId(),
            bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetAreaId(),
            bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY);

        if (!TeleportToHomebind(bot))
            return false;
        bot->SaveToDB();
        return true;
    }
}
