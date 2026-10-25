/*
 * Destruction / Demonology — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum WarlockSpells : uint32
    {
        DARK_INTENT      = 109773,
        SUMMON_DOOMGUARD = 18540,
        LIFE_TAP         = 1454,
        SHADOW_BOLT      = 686,    // low-level until Incinerate
        CORRUPTION       = 172,    // aura 146739
    };

    enum DestroSpells : uint32
    {
        IMMOLATE          = 348,
        CONFLAGRATE       = 17962,
        CHAOS_BOLT        = 116858,
        INCINERATE        = 29722,
        SHADOWBURN        = 17877,
        HAVOC             = 80240,
        FIRE_AND_BRIMSTONE = 108683,
        RAIN_OF_FIRE      = 5740,
        DARK_SOUL_INSTABILITY = 113858,
    };

    enum DemoSpells : uint32
    {
        HAND_OF_GULDAN   = 105174,
        SOUL_FIRE        = 6353,
        METAMORPHOSIS    = 103958,
        TOUCH_OF_CHAOS   = 103964,
        DOOM             = 603,
        MOLTEN_CORE      = 122355,
        DARK_SOUL_KNOWLEDGE = 113861,
    };

    float ManaPct(Player* bot)
    {
        return bot->GetMaxPower(POWER_MANA)
            ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(bot->GetMaxPower(POWER_MANA))) : 100.0f;
    }

    uint32 SelectWarlockCooldowns(Context const& ctx, uint32 darkSoul)
    {
        if (!WantsCooldowns(ctx))
            return 0;
        if (CanTryCast(ctx.bot, SUMMON_DOOMGUARD))
            return SUMMON_DOOMGUARD;
        if (CanTryCast(ctx.bot, darkSoul))
            return darkSoul;
        return 0;
    }
}

uint32 SelectDestruction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    // Burning Embers are stored x10 (10 = one full ember).
    uint32 const embers = bot->GetPower(POWER_BURNING_EMBERS) / 10;
    bool const darkSoul = HasAuraUp(bot, DARK_SOUL_INSTABILITY);

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    if (uint32 const cd = SelectWarlockCooldowns(ctx, DARK_SOUL_INSTABILITY))
        return cd;

    // Fire and Brimstone is a toggle: on for packs, off again for single targets.
    bool const brimstone = HasAuraUp(bot, FIRE_AND_BRIMSTONE);
    if (((ctx.enemies >= 3 && embers >= 1) != brimstone) && CanTryCast(bot, FIRE_AND_BRIMSTONE))
        return FIRE_AND_BRIMSTONE;
    if (ctx.enemies >= 3 && !HasGroundEffect(bot, RAIN_OF_FIRE) && CanTryCast(bot, RAIN_OF_FIRE))
        return RAIN_OF_FIRE;

    if (NeedsMyAuraRefresh(bot, target, IMMOLATE, 4.5f) && CanTryCast(bot, IMMOLATE))
        return IMMOLATE;

    if (ctx.enemies == 2 && embers >= 1 && CanTryCast(bot, HAVOC))
        return HAVOC;

    if (ctx.targetHealthPct < 20.0f && embers >= 1 && CanTryCast(bot, SHADOWBURN))
        return SHADOWBURN;

    // Spend embers before they cap, or burst them inside Dark Soul.
    if (!brimstone && (embers >= 3 || (darkSoul && embers >= 1)) && CanTryCast(bot, CHAOS_BOLT))
        return CHAOS_BOLT;

    if (CanTryCast(bot, CONFLAGRATE))
        return CONFLAGRATE;

    if (CanTryCast(bot, INCINERATE))
        return INCINERATE;
    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;
    if (ManaPct(bot) < 30.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    return 0;
}

uint32 SelectDemonology(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const fury = bot->GetPower(POWER_DEMONIC_FURY);
    bool const meta = HasAuraUp(bot, METAMORPHOSIS);
    bool const darkSoul = HasAuraUp(bot, DARK_SOUL_KNOWLEDGE);

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    if (uint32 const cd = SelectWarlockCooldowns(ctx, DARK_SOUL_KNOWLEDGE))
        return cd;

    if (meta)
    {
        // Metamorphosis drains Fury every second; drop out before it runs dry.
        if (fury < 150 && !darkSoul)
        {
            bot->RemoveAurasDueToSpell(METAMORPHOSIS);
            return 0;
        }
        if (NeedsMyAuraRefresh(bot, target, DOOM, 18.0f) && CanTryCast(bot, DOOM))
            return DOOM;
        if (CanTryCast(bot, TOUCH_OF_CHAOS))
            return TOUCH_OF_CHAOS;
        return 0;
    }

    if (NeedsMyAuraRefresh(bot, target, CORRUPTION, 5.0f) && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;

    if ((fury >= 750 || (darkSoul && fury >= 300)) && CanTryCast(bot, METAMORPHOSIS))
        return METAMORPHOSIS;

    if (CanTryCast(bot, HAND_OF_GULDAN))
        return HAND_OF_GULDAN;

    if ((HasAuraUp(bot, MOLTEN_CORE) || ctx.targetHealthPct <= 25.0f) && CanTryCast(bot, SOUL_FIRE))
        return SOUL_FIRE;

    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;
    if (ManaPct(bot) < 30.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    return 0;
}

} // namespace BotRotation
