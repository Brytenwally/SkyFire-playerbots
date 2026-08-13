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

        MUTILATE            = 1329,
        RUPTURE             = 1943,
        ENVENOM             = 32645,
        DISPATCH            = 111240,
        BLINDSIDE           = 121153,
        REVEALING_STRIKE    = 84617,
        SINISTER_STRIKE     = 1752,
        SLICE_AND_DICE      = 5171,
        EVISCERATE          = 2098,
        HEMORRHAGE          = 16511,
        BACKSTAB            = 53,
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

    if (CanTryCast(bot, MUTILATE))
        return MUTILATE;

    if (NeedsMyAuraRefresh(bot, target, RUPTURE, 3.0f) && cp >= 1 && CanTryCast(bot, RUPTURE))
        return RUPTURE;

    if (cp >= 4 && CanTryCast(bot, ENVENOM))
        return ENVENOM;

    if ((ctx.targetHealthPct <= 30.0f || HasAuraUp(bot, BLINDSIDE)) && CanTryCast(bot, DISPATCH))
        return DISPATCH;

    return 0;
}

uint32 SelectCombat(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    int8 const cp = ctx.comboPoints;

    if (uint32 const poison = SelectPoisonBuff(bot, false))
        return poison;

    if (NeedsMyAuraRefresh(bot, target, REVEALING_STRIKE, 3.0f) && CanTryCast(bot, REVEALING_STRIKE))
        return REVEALING_STRIKE;

    if (CanTryCast(bot, SINISTER_STRIKE))
        return SINISTER_STRIKE;

    if (NeedsMyAuraRefresh(bot, bot, SLICE_AND_DICE, 3.0f) && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 5 && CanTryCast(bot, EVISCERATE))
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

    if (uint32 const poison = SelectPoisonBuff(bot, false))
        return poison;

    if (NeedsMyAuraRefresh(bot, target, HEMORRHAGE, 3.0f) && CanTryCast(bot, HEMORRHAGE))
        return HEMORRHAGE;

    if (CanTryCast(bot, BACKSTAB))
        return BACKSTAB;

    if (NeedsMyAuraRefresh(bot, bot, SLICE_AND_DICE, 3.0f) && cp >= 1 && CanTryCast(bot, SLICE_AND_DICE))
        return SLICE_AND_DICE;

    if (cp >= 1 && CanTryCast(bot, RUPTURE))
        return RUPTURE;

    if (cp >= 5 && CanTryCast(bot, EVISCERATE))
        return EVISCERATE;

    return 0;
}

} // namespace BotRotation
