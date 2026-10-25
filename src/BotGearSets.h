/*
 * PvE / PvP gear sets for bots.
 *
 * The PvE set is worn; the PvP set lives in a dedicated gear bag where each
 * piece sits at the bag position equal to its equipment slot, so switching
 * sets is a per-slot swap between the bag and the paper doll.
 */

#ifndef MOD_PLAYERBOTS_BOT_GEAR_SETS_H
#define MOD_PLAYERBOTS_BOT_GEAR_SETS_H

#include "Define.h"

class Bag;
class Player;
struct ItemTemplate;

namespace BotGearSets
{
    // Foror's Crate of Endless Resist Gear Storage (36 slots).
    constexpr uint32 GEAR_BAG_ENTRY = 23162;

    // Carries resilience or PvP power.
    bool IsPvpItem(ItemTemplate const* proto);

    Bag* FindGearBag(Player* bot);
    // Finds the gear bag or equips a new one into a free bag slot; nullptr if none is free.
    Bag* EnsureGearBag(Player* bot);

    // Destroys the stored PvP set before a re-roll.
    void ClearStoredSet(Player* bot);
    // Moves every equipped PvP piece into its gear bag position.
    void StoreEquippedAsPvpSet(Player* bot);
    // Drops the stored weapons when one set is two-handed and the other is not.
    void DropMismatchedWeapons(Player* bot);

    // Swaps stored pieces in so the bot wears the requested set.
    void WearSet(Player* bot, bool pvp);
}

#endif
