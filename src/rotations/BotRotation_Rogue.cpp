/*
 * Assassination / Combat / Subtlety — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 *
 * MoP poisons coat all weapons via player auras: one Lethal + one Non-Lethal.
 * Do not treat them like shaman per-weapon imbues.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum RogueSpells : uint32
    {
        // Lethal poisons (only one may be active)
        DEADLY_POISON       = 2823,
        WOUND_POISON        = 8679,
        // Non-lethal poisons (only one may be active)
        CRIPPLING_POISON    = 3408,
        LEECHING_POISON     = 108211,
        PARALYTIC_POISON    = 108215,

        SLICE_AND_DICE      = 5171,
        RUPTURE             = 1943,
        EVISCERATE          = 2098,
        CRIMSON_TEMPEST     = 121411,
        FAN_OF_KNIVES       = 51723,
        SHADOW_BLADES       = 121471,

        // Assassination
        MUTILATE            = 1329,
        ENVENOM             = 32645,
        DISPATCH            = 111240,
        BLINDSIDE           = 121153,
        VENDETTA            = 79140,

        // Combat
        REVEALING_STRIKE    = 84617,
        SINISTER_STRIKE     = 1752,
        BLADE_FLURRY        = 13877,
        ADRENALINE_RUSH     = 13750,
        KILLING_SPREE       = 51690,

        // Subtlety
        HEMORRHAGE          = 16511,
        HEMORRHAGE_DOT      = 89775,
        BACKSTAB            = 53,
        AMBUSH              = 8676,
        SHADOW_DANCE        = 51713,
        PREMEDITATION       = 14183,
    };

    bool HasLethalPoison(Player* bot)
    {
        return HasAuraUp(bot, DEADLY_POISON) || HasAuraUp(bot, WOUND_POISON);
    }

    bool HasNonLethalPoison(Player* bot)
    {
        return HasAuraUp(bot, CRIPPLING_POISON)
            || HasAuraUp(bot, LEECHING_POISON)
            || HasAuraUp(bot, PARALYTIC_POISON);
    }

    // Prefer Deadly (lethal) + Crippling/Leeching (non-lethal). Never apply a
    // second lethal when one is already up.
    uint32 SelectPoisonBuff(Player* bot, bool preferLeeching)
    {
        if (!bot)
            return 0;

        if (!HasLethalPoison(bot) && CanTryCast(bot, DEADLY_POISON))
            return DEADLY_POISON;

        if (!HasNonLethalPoison(bot))
        {
            if (preferLeeching && CanTryCast(bot, LEECHING_POISON))
                return LEECHING_POISON;
            if (CanTryCast(bot, CRIPPLING_POISON))
                return CRIPPLING_POISON;
        }

        return 0;
    }

    bool IsBehind(Player* bot, Unit* target)
    {
        return !target->HasInArc(static_cast<float>(M_PI), bot);
    }

    // Shared pack line: Slice and Dice, Crimson Tempest at 4+, then the spec's finisher / Fan of Knives.
    uint32 SelectRogueAoe(Context const& ctx, uint32 finisher)
    {
        Player* bot = ctx.bot;
        int8 const cp = ctx.comboPoints;
        if (cp >= 2 && MyAuraRemains(bot, bot, SLICE_AND_DICE) < 2.0f && CanTryCast(bot, SLICE_AND_DICE))
            return SLICE_AND_DICE;
        if (cp >= 4 && ctx.enemies >= 4 && !HasMyAura(bot, ctx.target, CRIMSON_TEMPEST)
            && CanTryCast(bot, CRIMSON_TEMPEST))
            return CRIMSON_TEMPEST;
        if (cp >= 5 && CanTryCast(bot, finisher))
            return finisher;
        if (cp < 5 && CanTryCast(bot, FAN_OF_KNIVES))
            return FAN_OF_KNIVES;
        return 0;
    }
}

uint32 SelectAssassination(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;

    if (uint32 const poison = SelectPoisonBuff(bot, true))
        return poison;

    bool const sndUp = HasAuraUp(bot, SLICE_AND_DICE);
    float const rupture = MyAuraRemains(bot, target, RUPTURE);

    if (WantsCooldowns(ctx) && (sndUp || rupture > 0.0f))
    {
        if (CanTryCast(bot, VENDETTA))
            return VENDETTA;
        if (HasMyAura(bot, target, VENDETTA) && CanTryCast(bot, SHADOW_BLADES))
            return SHADOW_BLADES;
    }

    if (ctx.enemies >= 3)
        if (uint32 const aoe = SelectRogueAoe(ctx, ENVENOM))
            return aoe;

    // Finishers. Envenom also refreshes Slice and Dice.
    if (cp >= 2 && MyAuraRemains(bot, bot, SLICE_AND_DICE) < 2.0f && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;
    if (cp >= 1 && rupture <= 0.0f && CanTryCast(bot, RUPTURE))
        return RUPTURE;
    if (cp >= 4 && rupture <= 2.0f && CanTryCast(bot, RUPTURE))
        return RUPTURE;
    if (cp >= 5 && CanTryCast(bot, ENVENOM))
        return ENVENOM;
    if (cp >= 2 && MyAuraRemains(bot, bot, SLICE_AND_DICE) < 3.0f && CanTryCast(bot, ENVENOM))
        return ENVENOM;

    // Generators: Dispatch replaces Mutilate in execute range or on a Blindside proc.
    if ((ctx.targetHealthPct < 35.0f || HasAuraUp(bot, BLINDSIDE)) && CanTryCast(bot, DISPATCH))
        return DISPATCH;
    if (cp < 5 && CanTryCast(bot, MUTILATE))
        return MUTILATE;
    // Low level: Sinister Strike / Eviscerate before Mutilate and Envenom.
    if (cp >= 5 && CanTryCast(bot, EVISCERATE))
        return EVISCERATE;
    if (cp < 5 && CanTryCast(bot, SINISTER_STRIKE))
        return SINISTER_STRIKE;

    return 0;
}

uint32 SelectCombat(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;
    uint32 const energy = bot->GetPower(POWER_ENERGY);

    if (uint32 const poison = SelectPoisonBuff(bot, false))
        return poison;

    // Blade Flurry is a toggle: on for packs, off again for single targets.
    bool const flurry = HasAuraUp(bot, BLADE_FLURRY);
    if ((ctx.enemies >= 2) != flurry && CanTryCast(bot, BLADE_FLURRY))
        return BLADE_FLURRY;

    if (WantsCooldowns(ctx))
    {
        if (CanTryCast(bot, SHADOW_BLADES))
            return SHADOW_BLADES;
        if ((energy < 35 || HasAuraUp(bot, SHADOW_BLADES)) && CanTryCast(bot, ADRENALINE_RUSH))
            return ADRENALINE_RUSH;
        if (energy < 50 && !HasAuraUp(bot, ADRENALINE_RUSH) && CanTryCast(bot, KILLING_SPREE))
            return KILLING_SPREE;
    }

    if (cp >= 1 && MyAuraRemains(bot, bot, SLICE_AND_DICE) < 2.0f && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp < 5)
    {
        if (ctx.enemies >= 4 && CanTryCast(bot, FAN_OF_KNIVES))
            return FAN_OF_KNIVES;
        if (NeedsMyAuraRefresh(bot, target, REVEALING_STRIKE, 3.0f) && CanTryCast(bot, REVEALING_STRIKE))
            return REVEALING_STRIKE;
        if (CanTryCast(bot, SINISTER_STRIKE))
            return SINISTER_STRIKE;
        return 0;
    }

    if (NeedsMyAuraRefresh(bot, target, RUPTURE, 4.0f) && !flurry && ctx.enemies <= 2 && WantsCooldowns(ctx)
        && CanTryCast(bot, RUPTURE))
        return RUPTURE;
    if (ctx.enemies >= 7 && NeedsMyAuraRefresh(bot, target, CRIMSON_TEMPEST, 2.0f) && CanTryCast(bot, CRIMSON_TEMPEST))
        return CRIMSON_TEMPEST;
    if (CanTryCast(bot, EVISCERATE))
        return EVISCERATE;

    return 0;
}

uint32 SelectSubtlety(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;
    uint32 const energy = bot->GetPower(POWER_ENERGY);

    if (uint32 const poison = SelectPoisonBuff(bot, false))
        return poison;

    bool const behind = IsBehind(bot, target);
    bool const dance = HasAuraUp(bot, SHADOW_DANCE);
    bool const sndUp = HasAuraUp(bot, SLICE_AND_DICE);
    bool const bleedUp = HasMyAura(bot, target, RUPTURE) || HasMyAura(bot, target, HEMORRHAGE_DOT);

    if (ctx.enemies >= 3)
        if (uint32 const aoe = SelectRogueAoe(ctx, EVISCERATE))
            return aoe;

    if (!dance && energy >= 75 && sndUp && bleedUp && cp <= 3 && CanTryCast(bot, SHADOW_DANCE))
        return SHADOW_DANCE;
    if (!dance && energy >= 80 && WantsCooldowns(ctx) && CanTryCast(bot, SHADOW_BLADES))
        return SHADOW_BLADES;

    if (dance && cp <= 3 && CanTryCast(bot, PREMEDITATION))
        return PREMEDITATION;

    if (cp >= 1 && MyAuraRemains(bot, bot, SLICE_AND_DICE) <= 3.0f + 3.0f * float(cp)
        && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;
    if (cp >= 5 && NeedsMyAuraRefresh(bot, target, RUPTURE, 4.0f) && CanTryCast(bot, RUPTURE))
        return RUPTURE;
    if (cp >= 5 && CanTryCast(bot, EVISCERATE))
        return EVISCERATE;

    if (cp < 5)
    {
        if (dance && behind && CanTryCast(bot, AMBUSH))
            return AMBUSH;
        if (!bleedUp && CanTryCast(bot, HEMORRHAGE))
            return HEMORRHAGE;
        if (behind && CanTryCast(bot, BACKSTAB))
            return BACKSTAB;
        // Backstab needs us behind the target; Hemorrhage / Sinister Strike work from any side.
        if (!behind && CanTryCast(bot, HEMORRHAGE))
            return HEMORRHAGE;
        if (!behind && CanTryCast(bot, SINISTER_STRIKE))
            return SINISTER_STRIKE;
    }

    return 0;
}

} // namespace BotRotation
