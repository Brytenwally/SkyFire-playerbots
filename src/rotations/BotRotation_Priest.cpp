/*
 * Shadow Priest — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum ShadowSpells : uint32
    {
        SHADOWFORM        = 15473,
        INNER_FIRE        = 588,
        VAMPIRIC_TOUCH    = 34914,
        SHADOW_WORD_PAIN  = 589,
        MIND_BLAST        = 8092,
        DEVOURING_PLAGUE  = 2944,
        MIND_FLAY         = 15407,
        MIND_SEAR         = 48045,
        MIND_SPIKE        = 73510,
        SURGE_OF_DARKNESS = 87160,
        SHADOW_WORD_DEATH = 32379,
        SHADOWFIEND       = 34433,
        MINDBENDER        = 123040,
        POWER_INFUSION    = 10060,
        VAMPIRIC_EMBRACE  = 15286,
        HALO              = 120517,
        CASCADE           = 121135,
        DIVINE_STAR       = 110744,
    };
}

uint32 SelectShadow(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const orbs = bot->GetPower(POWER_SHADOW_ORBS);
    float const hpPct = bot->GetMaxHealth()
        ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    float const manaPct = bot->GetMaxPower(POWER_MANA)
        ? (100.0f * float(bot->GetPower(POWER_MANA)) / float(bot->GetMaxPower(POWER_MANA))) : 100.0f;

    if (!HasAuraUp(bot, SHADOWFORM) && CanTryCast(bot, SHADOWFORM))
        return SHADOWFORM;
    if (!HasAuraUp(bot, INNER_FIRE) && CanTryCast(bot, INNER_FIRE))
        return INNER_FIRE;

    if (hpPct <= 40.0f && CanTryCast(bot, VAMPIRIC_EMBRACE))
        return VAMPIRIC_EMBRACE;

    // Mindbender is a 1 min cooldown; Shadowfiend (3 min) waits for a worthy target or low mana.
    if (CanTryCast(bot, MINDBENDER))
        return MINDBENDER;
    if ((WantsCooldowns(ctx) || manaPct < 50.0f) && CanTryCast(bot, SHADOWFIEND))
        return SHADOWFIEND;
    if (WantsCooldowns(ctx) && CanTryCast(bot, POWER_INFUSION))
        return POWER_INFUSION;

    bool const dotsUp = MyAuraRemains(bot, target, SHADOW_WORD_PAIN) > 3.0f
        && MyAuraRemains(bot, target, VAMPIRIC_TOUCH) > 3.0f;
    if (orbs >= 3 && dotsUp && CanTryCast(bot, DEVOURING_PLAGUE))
        return DEVOURING_PLAGUE;

    if (CanTryCast(bot, MIND_BLAST))
        return MIND_BLAST;

    // Only castable below 20% health.
    if (ctx.targetHealthPct < 20.0f && CanTryCast(bot, SHADOW_WORD_DEATH))
        return SHADOW_WORD_DEATH;

    if (NeedsMyAuraRefresh(bot, target, SHADOW_WORD_PAIN, 3.0f) && CanTryCast(bot, SHADOW_WORD_PAIN))
        return SHADOW_WORD_PAIN;
    if (NeedsMyAuraRefresh(bot, target, VAMPIRIC_TOUCH, 3.0f) && CanTryCast(bot, VAMPIRIC_TOUCH))
        return VAMPIRIC_TOUCH;

    if (orbs >= 3 && CanTryCast(bot, DEVOURING_PLAGUE))
        return DEVOURING_PLAGUE;

    if (AuraStacks(bot, SURGE_OF_DARKNESS) >= 2 && CanTryCast(bot, MIND_SPIKE))
        return MIND_SPIKE;

    if (CanTryCast(bot, HALO))
        return HALO;
    if (CanTryCast(bot, CASCADE))
        return CASCADE;
    if (CanTryCast(bot, DIVINE_STAR))
        return DIVINE_STAR;

    if (HasAuraUp(bot, SURGE_OF_DARKNESS) && CanTryCast(bot, MIND_SPIKE))
        return MIND_SPIKE;

    if (ctx.enemies >= 3 && CanTryCast(bot, MIND_SEAR))
        return MIND_SEAR;

    if (CanTryCast(bot, MIND_FLAY))
        return MIND_FLAY;

    return 0;
}

} // namespace BotRotation
