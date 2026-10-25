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

        // Windwalker
        RISING_SUN_KICK_DEBUFF = 130320,
        TIGEREYE_BREW_STACKS = 125195,
        TIGEREYE_BREW_USE   = 116740,
        ENERGIZING_BREW     = 115288,
        TOUCH_OF_DEATH      = 115080,
        DEATH_NOTE          = 121125,
        COMBO_BREAKER_BOK   = 116768,
        COMBO_BREAKER_TP    = 118864,
        INVOKE_XUEN         = 123904,
        CHI_BURST           = 123986,
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

    Unit* target = ctx.target;
    if (!HasAuraUp(bot, STANCE_FIERCE_TIGER) && CanTryCast(bot, STANCE_FIERCE_TIGER))
        return STANCE_FIERCE_TIGER;
    if (!target)
        return 0;

    uint32 const chi = bot->GetPower(POWER_CHI);
    uint32 const maxChi = bot->GetMaxPower(POWER_CHI);
    uint32 const energy = bot->GetPower(POWER_ENERGY);
    // Energy regen is ~10/s, so "seconds until capped" maps onto an energy threshold.
    bool const notCappingSoon = energy < 60;
    float const tigerPower = AuraRemains(bot, TIGER_POWER);
    float const sunKick = MyAuraRemains(bot, target, RISING_SUN_KICK_DEBUFF);
    bool const brewing = HasAuraUp(bot, TIGEREYE_BREW_USE);

    if (chi <= 2 && CanTryCast(bot, CHI_BREW))
        return CHI_BREW;
    if (tigerPower <= 3.0f && chi >= 1 && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;

    uint32 const brewStacks = AuraStacks(bot, TIGEREYE_BREW_STACKS);
    if (!brewing && (brewStacks >= 20 || (chi >= 2 && brewStacks >= 10 && sunKick > 0.0f && tigerPower > 0.0f))
        && CanTryCast(bot, TIGEREYE_BREW_USE))
        return TIGEREYE_BREW_USE;
    if (energy < 50 && CanTryCast(bot, ENERGIZING_BREW))
        return ENERGIZING_BREW;

    if (sunKick <= 0.0f && chi >= 2 && CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;
    if (brewing && WantsCooldowns(ctx) && CanTryCast(bot, INVOKE_XUEN))
        return INVOKE_XUEN;

    // Death Note: Touch of Death is usable on this target.
    if (HasAuraUp(bot, DEATH_NOTE) && chi >= 3 && CanTryCast(bot, TOUCH_OF_DEATH))
        return TOUCH_OF_DEATH;

    if (ctx.enemies >= 3)
    {
        if (CanTryCast(bot, RUSHING_JADE_WIND))
            return RUSHING_JADE_WIND;
        if (CanTryCast(bot, CHI_WAVE))
            return CHI_WAVE;
        if (CanTryCast(bot, CHI_BURST))
            return CHI_BURST;
        if (chi >= maxChi && CanTryCast(bot, RISING_SUN_KICK))
            return RISING_SUN_KICK;
        if (!bot->HasSpell(RUSHING_JADE_WIND) && energy < 60 && CanTryCast(bot, SPINNING_CRANE_KICK))
            return SPINNING_CRANE_KICK;
    }

    if (chi >= 3 && notCappingSoon && tigerPower > 4.0f && !HasAuraUp(bot, ENERGIZING_BREW)
        && CanTryCast(bot, FISTS_OF_FURY))
        return FISTS_OF_FURY;
    if (chi >= 2 && CanTryCast(bot, RISING_SUN_KICK))
        return RISING_SUN_KICK;

    if (energy < 80)
    {
        if (CanTryCast(bot, CHI_WAVE))
            return CHI_WAVE;
        if (CanTryCast(bot, CHI_BURST))
            return CHI_BURST;
    }

    if (HasAuraUp(bot, COMBO_BREAKER_BOK) && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;
    if (HasAuraUp(bot, COMBO_BREAKER_TP) && AuraRemains(bot, COMBO_BREAKER_TP) <= 2.0f
        && CanTryCast(bot, TIGER_PALM))
        return TIGER_PALM;
    if (chi + 2 <= maxChi && CanTryCast(bot, JAB))
        return JAB;
    if (chi >= 2 && CanTryCast(bot, BLACKOUT_KICK))
        return BLACKOUT_KICK;
    // Low level: Jab even at near-full Chi rather than idling.
    if (chi < maxChi && CanTryCast(bot, JAB))
        return JAB;

    return 0;
}

} // namespace BotRotation
