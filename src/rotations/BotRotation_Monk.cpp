/*
 * Brewmaster / Windwalker — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum MonkSpells : uint32
    {
        STANCE_STURDY_OX    = 115069,
        STANCE_FIERCE_TIGER = 103985,
        KEG_SMASH           = 121253,
        BLACKOUT_KICK       = 100784,
        TIGER_PALM          = 100787,
        TIGER_POWER         = 125359,
        JAB                 = 100780,
        PURIFYING_BREW      = 119582,
        SHUFFLE             = 115307,
        RISING_SUN_KICK     = 107428,
        FISTS_OF_FURY       = 113656,

        // Brewmaster
        GUARD               = 115295,
        ELUSIVE_BREW        = 115308,   // consume (dodge buff)
        ELUSIVE_BREW_STACKS = 128939,
        EXPEL_HARM          = 115072,
        BREATH_OF_FIRE      = 115181,
        DIZZYING_HAZE_DEBUFF = 123727,  // applied by Keg Smash
        SPINNING_CRANE_KICK = 101546,
        RUSHING_JADE_WIND   = 116847,
        CHI_WAVE            = 115098,
        CHI_BREW            = 115399,
        FORTIFYING_BREW     = 115203,
        LIGHT_STAGGER       = 124275,
        MODERATE_STAGGER    = 124274,
        HEAVY_STAGGER       = 124273,
    };
}

uint32 SelectBrewmaster(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot)
        return 0;

    uint32 const chi = bot->GetPower(POWER_CHI);
    uint32 const maxChi = bot->GetMaxPower(POWER_CHI);
    uint32 const energy = bot->GetPower(POWER_ENERGY);
    float const hpPct = bot->GetMaxHealth()
        ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    float const shuffle = AuraRemains(bot, SHUFFLE);

    if (!HasAuraUp(bot, STANCE_STURDY_OX) && CanTryCast(bot, STANCE_STURDY_OX))
        return STANCE_STURDY_OX;

    // Fortifying Brew also raises the staggered share, so use it before the shared panic threshold.
    if (hpPct < 55.0f && CanTryCast(bot, FORTIFYING_BREW))
        return FORTIFYING_BREW;

    // Purify Heavy Stagger always; Moderate only while Shuffle is safe.
    if (HasAuraUp(bot, HEAVY_STAGGER) || (HasAuraUp(bot, MODERATE_STAGGER) && shuffle > 2.0f))
        if (CanTryCast(bot, PURIFYING_BREW))
            return PURIFYING_BREW;

    uint32 const brewStacks = AuraStacks(bot, ELUSIVE_BREW_STACKS);
    if (!HasAuraUp(bot, ELUSIVE_BREW) && (brewStacks >= 10 || (brewStacks >= 5 && hpPct < 70.0f))
        && CanTryCast(bot, ELUSIVE_BREW))
        return ELUSIVE_BREW;

    // Shuffle is the main mitigation: never let it drop.
    if (shuffle < 2.0f && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;

    if (CanTryCast(bot, GUARD))
        return GUARD;

    if (chi + 2 <= maxChi && CanTryCast(bot, KEG_SMASH))
        return KEG_SMASH;

    if (ctx.enemies >= 3)
    {
        if (shuffle > 6.0f && target && HasAuraUp(target, DIZZYING_HAZE_DEBUFF)
            && CanTryCast(bot, BREATH_OF_FIRE))
            return BREATH_OF_FIRE;
        if (CanTryCast(bot, RUSHING_JADE_WIND))
            return RUSHING_JADE_WIND;
    }

    if (CanTryCast(bot, CHI_WAVE))
        return CHI_WAVE;

    // Spend Chi before it caps, extending Shuffle.
    if ((chi + 1 >= maxChi || shuffle < 6.0f) && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;

    if (chi <= 1 && CanTryCast(bot, CHI_BREW))
        return CHI_BREW;

    if (NeedsMyAuraRefresh(bot, bot, TIGER_POWER, 3.0f) && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    // Chi builders, pooling energy so Keg Smash is castable when it comes off cooldown.
    bool const poolForKeg = bot->HasSpell(KEG_SMASH)
        && bot->GetSpellCooldownDelay(KEG_SMASH) <= 3 && energy < 80;
    if (chi < maxChi && !poolForKeg)
    {
        if (ctx.enemies >= 4 && !bot->HasSpell(RUSHING_JADE_WIND) && CanTryCast(bot, SPINNING_CRANE_KICK))
            return SPINNING_CRANE_KICK;
        if (hpPct < 90.0f && CanTryCast(bot, EXPEL_HARM))
            return EXPEL_HARM;
        if (CanTryCast(bot, JAB))
            return JAB;
    }

    // Tiger Palm costs no energy for Brewmasters.
    if (CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    return 0;
}

uint32 SelectWindwalker(Context const& ctx)
{
    Player* bot = ctx.bot;
    if (!bot)
        return 0;

    if (!HasAuraUp(bot, STANCE_FIERCE_TIGER) && CanTryCast(bot, STANCE_FIERCE_TIGER))
        return STANCE_FIERCE_TIGER;

    if (CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;

    if (NeedsMyAuraRefresh(bot, bot, TIGER_POWER, 3.0f) && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    if (CanTryCast(bot, FISTS_OF_FURY))
        return FISTS_OF_FURY;

    if (CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;

    if (CanTryCast(bot, JAB))
        return JAB;

    return 0;
}

} // namespace BotRotation
