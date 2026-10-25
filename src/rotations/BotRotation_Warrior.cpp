/*
 * Arms / Fury / Protection — modules/mod-playerbots/rotation.md
 * Unknown (low-level) spells are skipped via CanTryCast.
 */

#include "BotRotationLists.h"
#include "Player.h"
#include "Unit.h"

namespace BotRotation
{
namespace
{
    enum WarriorSpells : uint32
    {
        BATTLE_SHOUT       = 6673,
        BATTLE_STANCE      = 2457,
        DEFENSIVE_STANCE   = 71,
        BERSERKER_STANCE   = 2458,

        // Shared
        COLOSSUS_SMASH     = 86346,
        EXECUTE            = 5308,
        SUDDEN_DEATH       = 52437,
        HEROIC_STRIKE      = 78,
        CLEAVE             = 845,
        HEROIC_THROW       = 57755,
        THUNDER_CLAP       = 6343,
        WHIRLWIND          = 1680,
        BERSERKER_RAGE     = 18499,
        ENRAGE             = 12880,
        RECKLESSNESS       = 1719,
        VICTORY_RUSH       = 34428,
        VICTORIOUS         = 32216,
        IMPENDING_VICTORY  = 103840,

        // Talents
        STORM_BOLT         = 107570,
        SHOCKWAVE          = 46968,
        DRAGON_ROAR        = 118000,
        AVATAR             = 107574,
        BLOODBATH          = 12292,

        // Arms
        MORTAL_STRIKE      = 12294,
        MORTAL_WOUNDS      = 115804,
        OVERPOWER          = 7384,
        SLAM               = 1464,
        TASTE_FOR_BLOOD    = 60503,
        SWEEPING_STRIKES   = 12328,
        DEEP_WOUNDS        = 115767,

        // Fury
        BLOODTHIRST        = 23881,
        RAGING_BLOW        = 85288,
        RAGING_BLOW_STACKS = 131116,
        WILD_STRIKE        = 100130,
        BLOODSURGE         = 46916,
        MEAT_CLEAVER       = 85739,

        // Protection
        SHIELD_SLAM        = 23922,
        REVENGE            = 6572,
        DEVASTATE          = 20243,
        SUNDER_ARMOR       = 7386,   // low-level until Devastate
        SHIELD_BLOCK       = 2565,   // aura 132404
        SHIELD_BARRIER     = 112048,
        DEMORALIZING_SHOUT = 1160,
        WEAKENED_BLOWS     = 115798,
        WEAKENED_ARMOR     = 113746,
    };

    float HealthPct(Player* bot)
    {
        return bot->GetMaxHealth()
            ? (100.0f * float(bot->GetHealth()) / float(bot->GetMaxHealth())) : 100.0f;
    }

    // Victory Rush only works after a killing blow; Impending Victory replaces it.
    uint32 SelectVictory(Player* bot, float threshold)
    {
        if (HealthPct(bot) >= threshold)
            return 0;
        if (CanTryCast(bot, IMPENDING_VICTORY))
            return IMPENDING_VICTORY;
        if (HasAuraUp(bot, VICTORIOUS) && CanTryCast(bot, VICTORY_RUSH))
            return VICTORY_RUSH;
        return 0;
    }

    // Recklessness / Avatar / Bloodbath, stacked inside a fresh Colossus Smash.
    uint32 SelectBurst(Context const& ctx, bool smashUp)
    {
        if (!smashUp || !WantsCooldowns(ctx))
            return 0;
        if (CanTryCast(ctx.bot, RECKLESSNESS))
            return RECKLESSNESS;
        if (CanTryCast(ctx.bot, AVATAR))
            return AVATAR;
        if (CanTryCast(ctx.bot, BLOODBATH))
            return BLOODBATH;
        return 0;
    }
}

uint32 SelectArms(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const rage = bot->GetPower(POWER_RAGE) / 10;

    if (bot->GetShapeshiftForm() != FORM_BATTLESTANCE
        && !HasAuraUp(bot, BATTLE_STANCE)
        && CanTryCast(bot, BATTLE_STANCE))
        return BATTLE_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (uint32 const heal = SelectVictory(bot, 70.0f))
        return heal;

    float const smash = MyAuraRemains(bot, target, COLOSSUS_SMASH);
    bool const smashUp = smash > 0.0f;
    bool const suddenDeath = HasAuraUp(bot, SUDDEN_DEATH);
    uint32 const tasteStacks = AuraStacks(bot, TASTE_FOR_BLOOD);

    if (smash < 1.5f && ctx.enemies <= 6 && CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;
    if (uint32 const burst = SelectBurst(ctx, smashUp))
        return burst;
    if (!HasAuraUp(bot, ENRAGE) && smashUp && CanTryCast(bot, BERSERKER_RAGE))
        return BERSERKER_RAGE;

    // Execute phase
    if (ctx.targetHealthPct < 20.0f)
    {
        if (NeedsMyAuraRefresh(bot, target, MORTAL_WOUNDS, 3.0f) && CanTryCast(bot, MORTAL_STRIKE))
            return MORTAL_STRIKE;
        if ((smashUp && rage >= 30) || suddenDeath || rage >= 100)
            if (CanTryCast(bot, EXECUTE))
                return EXECUTE;
        if (smashUp && rage >= 25 && rage < 100 && CanTryCast(bot, SLAM))
            return SLAM;
        if ((tasteStacks >= 2 || rage < 80) && CanTryCast(bot, OVERPOWER))
            return OVERPOWER;
        if (rage >= 30 && CanTryCast(bot, EXECUTE))
            return EXECUTE;
        if (rage >= 85 && CanTryCast(bot, HEROIC_STRIKE))
            return HEROIC_STRIKE;
        return 0;
    }

    if (ctx.enemies >= 2)
    {
        bool const sweeping = HasAuraUp(bot, SWEEPING_STRIKES);
        if (!sweeping && CanTryCast(bot, SWEEPING_STRIKES))
            return SWEEPING_STRIKES;
        if (ctx.enemies >= 3 && CanTryCast(bot, DRAGON_ROAR))
            return DRAGON_ROAR;
        if (ctx.enemies >= 3 && !HasMyAura(bot, target, DEEP_WOUNDS) && CanTryCast(bot, THUNDER_CLAP))
            return THUNDER_CLAP;
        if (sweeping && ctx.enemies <= 4 && CanTryCast(bot, MORTAL_STRIKE))
            return MORTAL_STRIKE;
        if (ctx.enemies >= 4 && MyAuraRemains(bot, target, DEEP_WOUNDS) < 4.0f && CanTryCast(bot, THUNDER_CLAP))
            return THUNDER_CLAP;
        if (ctx.enemies >= 5 && rage >= 30 && CanTryCast(bot, WHIRLWIND))
            return WHIRLWIND;
        if (sweeping && rage >= 25 && CanTryCast(bot, SLAM))
            return SLAM;
        if (sweeping && rage >= 85 && CanTryCast(bot, CLEAVE))
            return CLEAVE;
        if (suddenDeath && CanTryCast(bot, EXECUTE))
            return EXECUTE;
        if (rage < 80 && CanTryCast(bot, OVERPOWER))
            return OVERPOWER;
    }

    if (NeedsMyAuraRefresh(bot, target, MORTAL_WOUNDS, 4.0f) && CanTryCast(bot, MORTAL_STRIKE))
        return MORTAL_STRIKE;
    if (smashUp && tasteStacks >= 2 && CanTryCast(bot, OVERPOWER))
        return OVERPOWER;
    if (smashUp && CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (smashUp && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;
    bool const smashSoon = bot->HasSpell(COLOSSUS_SMASH) && bot->GetSpellCooldownDelay(COLOSSUS_SMASH) <= 1;
    if (((smashUp && rage >= 25) || (rage >= 60 && !smashSoon)) && CanTryCast(bot, SLAM))
        return SLAM;
    if (suddenDeath && CanTryCast(bot, EXECUTE))
        return EXECUTE;
    if (tasteStacks >= 3 && CanTryCast(bot, OVERPOWER))
        return OVERPOWER;
    if (CanTryCast(bot, MORTAL_STRIKE))
        return MORTAL_STRIKE;
    if (rage < 70 && CanTryCast(bot, OVERPOWER))
        return OVERPOWER;
    if (rage >= 50 && CanTryCast(bot, SLAM))
        return SLAM;
    if (rage >= 85 && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;
    if (!bot->IsWithinMeleeRange(target) && CanTryCast(bot, HEROIC_THROW))
        return HEROIC_THROW;

    return 0;
}

uint32 SelectFury(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot || !target)
        return 0;

    uint32 const rage = bot->GetPower(POWER_RAGE) / 10;

    if (!HasAuraUp(bot, BERSERKER_STANCE) && CanTryCast(bot, BERSERKER_STANCE))
        return BERSERKER_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    if (uint32 const heal = SelectVictory(bot, 70.0f))
        return heal;

    float const smash = MyAuraRemains(bot, target, COLOSSUS_SMASH);
    bool const smashUp = smash > 0.0f;
    float const enrage = AuraRemains(bot, ENRAGE);
    uint32 const blowStacks = AuraStacks(bot, RAGING_BLOW_STACKS);
    bool const bloodsurge = HasAuraUp(bot, BLOODSURGE);

    if (smash < 1.5f && CanTryCast(bot, COLOSSUS_SMASH))
        return COLOSSUS_SMASH;
    if (uint32 const burst = SelectBurst(ctx, smashUp))
        return burst;
    bool const smashSoon = bot->HasSpell(COLOSSUS_SMASH) && bot->GetSpellCooldownDelay(COLOSSUS_SMASH) < 3;
    if (enrage <= 0.0f && (smashUp || smashSoon) && CanTryCast(bot, BERSERKER_RAGE))
        return BERSERKER_RAGE;

    if (ctx.enemies >= 2)
    {
        bool const cleaver = HasAuraUp(bot, MEAT_CLEAVER);
        if (smashUp && CanTryCast(bot, DRAGON_ROAR))
            return DRAGON_ROAR;
        if (AuraRemains(bot, MEAT_CLEAVER) < 1.5f && CanTryCast(bot, WHIRLWIND))
            return WHIRLWIND;
        if ((enrage < 1.5f || smashUp) && CanTryCast(bot, BLOODTHIRST))
            return BLOODTHIRST;
        if (cleaver && blowStacks >= 1 && CanTryCast(bot, RAGING_BLOW))
            return RAGING_BLOW;
        if (cleaver && bloodsurge && CanTryCast(bot, WILD_STRIKE))
            return WILD_STRIKE;
        if (CanTryCast(bot, WHIRLWIND))
            return WHIRLWIND;
        if (rage >= 70 && ctx.enemies <= 4 && cleaver && CanTryCast(bot, HEROIC_STRIKE))
            return HEROIC_STRIKE;
        return 0;
    }

    if ((enrage < 1.5f || smashUp) && CanTryCast(bot, BLOODTHIRST))
        return BLOODTHIRST;
    if (ctx.targetHealthPct < 20.0f && (smashUp || rage >= 90) && CanTryCast(bot, EXECUTE))
        return EXECUTE;
    if (smashUp && blowStacks >= 2 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;
    if (smashUp && CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (smashUp && CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;
    if (HasAuraUp(bot, SUDDEN_DEATH) && CanTryCast(bot, EXECUTE))
        return EXECUTE;
    if (bloodsurge && CanTryCast(bot, WILD_STRIKE))
        return WILD_STRIKE;
    if (blowStacks >= 1 && (blowStacks >= 2 || smashUp) && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;
    if (CanTryCast(bot, BLOODTHIRST))
        return BLOODTHIRST;
    if (ctx.targetHealthPct < 20.0f && CanTryCast(bot, EXECUTE))
        return EXECUTE;
    if (blowStacks >= 1 && CanTryCast(bot, RAGING_BLOW))
        return RAGING_BLOW;
    if (rage >= 70 && CanTryCast(bot, WILD_STRIKE))
        return WILD_STRIKE;
    if (rage >= 90 && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;
    if (!bot->IsWithinMeleeRange(target) && CanTryCast(bot, HEROIC_THROW))
        return HEROIC_THROW;

    return 0;
}

uint32 SelectProtectionWarrior(Context const& ctx)
{
    Player* bot = ctx.bot;
    Unit* target = ctx.target;
    if (!bot)
        return 0;

    uint32 const rage = bot->GetPower(POWER_RAGE) / 10;
    float const hpPct = HealthPct(bot);
    bool const hasShield = HasShieldEquipped(bot);

    bool const inDefensive = bot->GetShapeshiftForm() == FORM_DEFENSIVESTANCE
        || HasAuraUp(bot, DEFENSIVE_STANCE);
    if (!inDefensive && CanTryCast(bot, DEFENSIVE_STANCE))
        return DEFENSIVE_STANCE;
    if (!HasAuraUp(bot, BATTLE_SHOUT) && CanTryCast(bot, BATTLE_SHOUT))
        return BATTLE_SHOUT;

    // Mitigation: Shield Block first, Shield Barrier when Block is down or we're taking a beating.
    bool const blockUp = AuraRemains(bot, SHIELD_BLOCK) > 0.0f;
    if (hasShield && inDefensive)
    {
        if (!blockUp && rage >= 60 && CanTryCast(bot, SHIELD_BLOCK))
            return SHIELD_BLOCK;
        if (AuraRemains(bot, SHIELD_BARRIER) <= 0.0f
            && ((rage >= 60 && !blockUp && !CanTryCast(bot, SHIELD_BLOCK)) || (hpPct < 50.0f && rage >= 20))
            && CanTryCast(bot, SHIELD_BARRIER))
            return SHIELD_BARRIER;
    }
    if (hpPct < 60.0f && CanTryCast(bot, DEMORALIZING_SHOUT))
        return DEMORALIZING_SHOUT;
    if (uint32 const heal = SelectVictory(bot, 60.0f))
        return heal;

    if (blockUp && WantsCooldowns(ctx))
    {
        if (CanTryCast(bot, AVATAR))
            return AVATAR;
        if (CanTryCast(bot, BLOODBATH))
            return BLOODBATH;
    }

    if (!target)
        return 0;

    if ((!HasAuraUp(target, WEAKENED_BLOWS) || ctx.enemies >= 3) && CanTryCast(bot, THUNDER_CLAP))
        return THUNDER_CLAP;
    if (hasShield && CanTryCast(bot, SHIELD_SLAM))
        return SHIELD_SLAM;
    if (CanTryCast(bot, REVENGE))
        return REVENGE;
    if (CanTryCast(bot, STORM_BOLT))
        return STORM_BOLT;
    if (ctx.enemies >= 3 && CanTryCast(bot, SHOCKWAVE))
        return SHOCKWAVE;
    if (CanTryCast(bot, DRAGON_ROAR))
        return DRAGON_ROAR;

    if (AuraStacks(target, WEAKENED_ARMOR) < 3)
    {
        if (CanTryCast(bot, DEVASTATE))
            return DEVASTATE;
        if (CanTryCast(bot, SUNDER_ARMOR))
            return SUNDER_ARMOR;
    }

    if (ctx.targetHealthPct < 20.0f && rage >= 30 && CanTryCast(bot, EXECUTE))
        return EXECUTE;

    // Rage dumps once mitigation is covered.
    if (ctx.enemies >= 3 && rage >= 60 && (blockUp || !hasShield) && CanTryCast(bot, CLEAVE))
        return CLEAVE;
    if (rage >= 80 && (blockUp || !hasShield) && CanTryCast(bot, HEROIC_STRIKE))
        return HEROIC_STRIKE;

    if (CanTryCast(bot, DEVASTATE))
        return DEVASTATE;
    if (CanTryCast(bot, SUNDER_ARMOR))
        return SUNDER_ARMOR;

    if (!bot->IsWithinMeleeRange(target) && CanTryCast(bot, HEROIC_THROW))
        return HEROIC_THROW;

    return 0;
}

} // namespace BotRotation
