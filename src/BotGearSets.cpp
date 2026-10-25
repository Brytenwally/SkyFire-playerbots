#include "BotGearSets.h"
#include "Bag.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "Player.h"

namespace BotGearSets
{
namespace
{
    uint16 Pos(uint8 bag, uint8 slot)
    {
        return uint16(bag) << 8 | slot;
    }

    bool IsTwoHand(Item* item)
    {
        ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
        return proto && proto->InventoryType == INVTYPE_2HWEAPON;
    }
}

bool IsPvpItem(ItemTemplate const* proto)
{
    if (!proto)
        return false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 const stat = proto->ItemStat[i].ItemStatType;
        if (stat == ITEM_MOD_RESILIENCE_RATING || stat == ITEM_MOD_PVP_POWER)
            return true;
    }
    return false;
}

Bag* FindGearBag(Player* bot)
{
    if (!bot)
        return nullptr;
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        if (Bag* bag = bot->GetBagByPos(slot))
            if (bag->GetEntry() == GEAR_BAG_ENTRY)
                return bag;
    return nullptr;
}

Bag* EnsureGearBag(Player* bot)
{
    if (Bag* bag = FindGearBag(bot))
        return bag;
    if (!bot)
        return nullptr;

    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            continue;
        uint16 dest = 0;
        if (bot->CanEquipNewItem(slot, dest, GEAR_BAG_ENTRY, false) != EQUIP_ERR_OK)
            continue;
        if (bot->EquipNewItem(dest, GEAR_BAG_ENTRY, true))
            return bot->GetBagByPos(slot);
    }
    return nullptr;
}

void ClearStoredSet(Player* bot)
{
    Bag* bag = FindGearBag(bot);
    if (!bag)
        return;
    uint8 const bagSlot = bag->GetSlot();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(bagSlot, slot);
        if (!item)
            continue;
        ItemTemplate const* proto = item->GetTemplate();
        if (proto && (proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON))
            bot->DestroyItem(bagSlot, slot, true);
    }
}

void StoreEquippedAsPvpSet(Player* bot)
{
    Bag* bag = FindGearBag(bot);
    if (!bag)
        return;
    uint8 const bagSlot = bag->GetSlot();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* worn = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!worn || !IsPvpItem(worn->GetTemplate()) || bot->GetItemByPos(bagSlot, slot))
            continue;
        bot->SwapItem(Pos(INVENTORY_SLOT_BAG_0, slot), Pos(bagSlot, slot));
    }
}

void DropMismatchedWeapons(Player* bot)
{
    Bag* bag = FindGearBag(bot);
    if (!bag)
        return;
    uint8 const bagSlot = bag->GetSlot();
    Item* storedMain = bot->GetItemByPos(bagSlot, EQUIPMENT_SLOT_MAINHAND);
    Item* wornMain = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!storedMain || !wornMain || IsTwoHand(storedMain) == IsTwoHand(wornMain))
        return;

    bot->DestroyItem(bagSlot, EQUIPMENT_SLOT_MAINHAND, true);
    if (bot->GetItemByPos(bagSlot, EQUIPMENT_SLOT_OFFHAND))
        bot->DestroyItem(bagSlot, EQUIPMENT_SLOT_OFFHAND, true);
}

void WearSet(Player* bot, bool pvp)
{
    Bag* bag = FindGearBag(bot);
    if (!bag)
        return;

    uint8 const bagSlot = bag->GetSlot();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* stored = bot->GetItemByPos(bagSlot, slot);
        if (!stored || IsPvpItem(stored->GetTemplate()) != pvp)
            continue;

        Item* worn = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        bool const wornPvp = worn && IsPvpItem(worn->GetTemplate());
        // Back in PvE only a worn PvP piece goes out; anything else stored here is loot.
        if (pvp ? wornPvp : !wornPvp)
            continue;

        bot->SwapItem(Pos(bagSlot, slot), Pos(INVENTORY_SLOT_BAG_0, slot));
    }
}

} // namespace BotGearSets
