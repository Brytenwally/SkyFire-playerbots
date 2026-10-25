/*
 * Affliction — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum AffSpells : uint32
    {
        DARK_INTENT         = 109773,
        AGONY               = 980,
        CORRUPTION          = 172,    // aura 146739 via AuraIdForSpell
        UNSTABLE_AFFLICTION = 30108,
        HAUNT               = 48181,
        MALEFIC_GRASP       = 103103,
        DRAIN_SOUL          = 1120,
        SEED_OF_CORRUPTION  = 27243,
        RAIN_OF_FIRE        = 5740,
        DARK_SOUL_MISERY    = 113860,
        SUMMON_DOOMGUARD    = 18540,
        LIFE_TAP            = 1454,
        DRAIN_LIFE          = 689,    // low-level filler if MG unknown
        SHADOW_BOLT         = 686,
    };
}

uint32 SelectAffliction(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const shards = bot->GetPower(POWER_SOUL_SHARDS) / 100;
    float const manaPct = bot->GetMaxPower(POWER_MANA)
        ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(bot->GetMaxPower(POWER_MANA))) : 100.0f;

    if (!HasAuraUp(bot, DARK_INTENT) && CanTryCast(bot, DARK_INTENT))
        return DARK_INTENT;

    if (WantsCooldowns(ctx))
    {
        if (CanTryCast(bot, SUMMON_DOOMGUARD))
            return SUMMON_DOOMGUARD;
        if (CanTryCast(bot, DARK_SOUL_MISERY))
            return DARK_SOUL_MISERY;
    }

    if (manaPct <= 15.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    if (ctx.enemies >= 3 && !HasMyAura(bot, target, SEED_OF_CORRUPTION) && CanTryCast(bot, SEED_OF_CORRUPTION))
        return SEED_OF_CORRUPTION;

    // Refresh inside the pandemic window (last ~30% of each DoT).
    if (NeedsMyAuraRefresh(bot, target, AGONY, 7.0f) && CanTryCast(bot, AGONY))
        return AGONY;
    if (NeedsMyAuraRefresh(bot, target, CORRUPTION, 5.0f) && CanTryCast(bot, CORRUPTION))
        return CORRUPTION;
    if (NeedsMyAuraRefresh(bot, target, UNSTABLE_AFFLICTION, 4.0f) && CanTryCast(bot, UNSTABLE_AFFLICTION))
        return UNSTABLE_AFFLICTION;

    if (ctx.enemies >= 4 && !HasGroundEffect(bot, RAIN_OF_FIRE) && CanTryCast(bot, RAIN_OF_FIRE))
        return RAIN_OF_FIRE;

    // Drain Soul replaces Malefic Grasp in execute range and refunds shards.
    if (ctx.targetHealthPct <= 20.0f && shards < 4 && CanTryCast(bot, DRAIN_SOUL))
        return DRAIN_SOUL;

    if (((shards >= 2 && MyAuraRemains(bot, target, HAUNT) < 3.0f) || shards >= 4) && CanTryCast(bot, HAUNT))
        return HAUNT;

    if (CanTryCast(bot, MALEFIC_GRASP))
        return MALEFIC_GRASP;
    if (CanTryCast(bot, DRAIN_LIFE))
        return DRAIN_LIFE;
    if (CanTryCast(bot, SHADOW_BOLT))
        return SHADOW_BOLT;
    if (manaPct < 50.0f && CanTryCast(bot, LIFE_TAP))
        return LIFE_TAP;

    return 0;
}

} // namespace BotRotation
