/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Enchants and gems for a bot's equipped gear.
*
* Candidates come from the game data, built once: gems are the gem items of
* the item template store, enchants are the enchanting profession's item
* enchant spells plus the item enhancement consumables (scrolls, armor kits,
* leg armor, shoulder inscriptions, belt buckles). Each is scored with the
* bot's spec weights (BotStatWeights): stats of the enchant, and the auras of
* its equip and proc spells (procs count kProcUptime of their value).
*/

#include "BotEnchants.h"
#include "BotStatWeights.h"

#include "DBCStores.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{
    // Share of a proc's stats counted, a rough average uptime.
    constexpr float kProcUptime = 0.35f;
    constexpr uint32 kNormalGemColors = SOCKET_COLOR_RED | SOCKET_COLOR_YELLOW | SOCKET_COLOR_BLUE;

    struct GemCandidate
    {
        ItemTemplate const* proto;
        SpellItemEnchantmentEntry const* enchant;
        uint32 color;
    };

    struct EnchantCandidate
    {
        SpellInfo const* spell;
        SpellItemEnchantmentEntry const* enchant;
        uint32 level;      // bot level needed
        uint32 skill;      // profession the bot itself needs (item RequiredSkill), 0 = none
        uint32 skillRank;
        bool prismatic;    // adds a socket (belt buckle) instead of a permanent enchant
    };

    bool IsJunkName(char const* raw)
    {
        if (!raw)
            return false;
        std::string name(raw);
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });
        // "test" as a word only: "Greatest" is a real enchant name.
        for (size_t at = name.find("test"); at != std::string::npos; at = name.find("test", at + 1))
            if (at == 0 || !std::isalpha(static_cast<unsigned char>(name[at - 1])))
                return true;
        return name.rfind("qa", 0) == 0 || name.find("deprecated") != std::string::npos
            || name.find("unused") != std::string::npos || name.rfind("monster -", 0) == 0;
    }

    // Enchanting recipe rank to the level of the content it belongs to.
    uint32 SkillRankToLevel(uint32 rank)
    {
        if (rank <= 300)
            return std::max<uint32>(1, rank / 5);
        if (rank <= 375)
            return 60 + (rank - 300) * 10 / 75;
        if (rank <= 450)
            return 70 + (rank - 375) * 10 / 75;
        if (rank <= 525)
            return 80 + (rank - 450) * 5 / 75;
        return std::min<uint32>(90, 85 + (rank - 525) * 5 / 75);
    }

    std::vector<GemCandidate> const& GemCandidates()
    {
        static std::vector<GemCandidate> gems;
        static bool built = false;
        if (built)
            return gems;
        built = true;

        for (auto const& pair : *sObjectMgr->GetItemTemplateStore())
        {
            ItemTemplate const* proto = &pair.second;
            if (proto->Class != ITEM_CLASS_GEM || !proto->GemProperties || IsJunkName(proto->Name1.c_str()))
                continue;
            GemPropertiesEntry const* props = sGemPropertiesStore.LookupEntry(proto->GemProperties);
            if (!props || !props->spellitemenchantement || !props->color)
                continue;
            // Sha-touched and cogwheel gems only fit their own sockets, which bots skip.
            if (props->color & (SOCKET_COLOR_HYDRAULIC | SOCKET_COLOR_COGWHEEL))
                continue;
            bool const meta = props->color == SOCKET_COLOR_META;
            // Unique gems other than metas are profession or special gems.
            if (!meta && ((proto->Flags & ITEM_PROTO_FLAG_UNIQUE_EQUIPPED) || proto->ItemLimitCategory))
                continue;
            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(props->spellitemenchantement);
            if (!enchant)
                continue;
            gems.push_back({ proto, enchant, props->color });
        }
        SF_LOG_INFO("modules", "[mod-playerbots] Enchants: %u gem candidate(s).", uint32(gems.size()));
        return gems;
    }

    bool FindEnchantEffect(SpellInfo const* spell, SpellItemEnchantmentEntry const*& enchant, bool& prismatic)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            SpellEffectInfo const& effect = spell->Effects[i];
            if (effect.Effect != SPELL_EFFECT_ENCHANT_ITEM && effect.Effect != SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC)
                continue;
            if (effect.MiscValue <= 0)
                continue;
            enchant = sSpellItemEnchantmentStore.LookupEntry(uint32(effect.MiscValue));
            if (!enchant)
                continue;
            prismatic = effect.Effect == SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC;
            if (prismatic)
            {
                bool socket = false;
                for (uint8 j = 0; j < MAX_ITEM_ENCHANTMENT_EFFECTS; ++j)
                    socket = socket || enchant->type[j] == ITEM_ENCHANTMENT_TYPE_PRISMATIC_SOCKET;
                if (!socket)
                    continue;
            }
            return true;
        }
        return false;
    }

    bool TargetsGear(SpellInfo const* spell)
    {
        return spell->EquippedItemClass == ITEM_CLASS_ARMOR || spell->EquippedItemClass == ITEM_CLASS_WEAPON;
    }

    std::vector<EnchantCandidate> const& EnchantCandidates()
    {
        static std::vector<EnchantCandidate> enchants;
        static bool built = false;
        if (built)
            return enchants;
        built = true;

        // Scrolls, armor kits, leg armor, inscriptions, buckles.
        for (auto const& pair : *sObjectMgr->GetItemTemplateStore())
        {
            ItemTemplate const* proto = &pair.second;
            if (proto->Class != ITEM_CLASS_CONSUMABLE || proto->SubClass != ITEM_SUBCLASS_ITEM_ENHANCEMENT)
                continue;
            if (IsJunkName(proto->Name1.c_str()))
                continue;
            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (proto->Spells[i].SpellId <= 0)
                    continue;
                SpellInfo const* spell = sSpellMgr->GetSpellInfo(uint32(proto->Spells[i].SpellId));
                SpellItemEnchantmentEntry const* enchant = nullptr;
                bool prismatic = false;
                if (!spell || !TargetsGear(spell) || !FindEnchantEffect(spell, enchant, prismatic))
                    continue;
                uint32 const level = proto->RequiredLevel ? proto->RequiredLevel
                    : std::clamp<uint32>(proto->ItemLevel, 1, 90);
                enchants.push_back({ spell, enchant, level, proto->RequiredSkill, proto->RequiredSkillRank, prismatic });
            }
        }

        // Enchanting recipes: the bot does not need the profession, someone enchanted it.
        for (uint32 id = 1; id < sSpellMgr->GetSpellInfoStoreSize(); ++id)
        {
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
            if (!spell || !TargetsGear(spell) || IsJunkName(spell->SpellName))
                continue;
            uint32 rank = 0;
            bool enchanting = false;
            SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(id);
            for (auto it = bounds.first; it != bounds.second; ++it)
            {
                if (it->second->skillId != SKILL_ENCHANTING)
                    continue;
                enchanting = true;
                rank = std::max(rank, it->second->req_skill_value ? it->second->req_skill_value : it->second->min_value);
            }
            if (!enchanting)
                continue;
            SpellItemEnchantmentEntry const* enchant = nullptr;
            bool prismatic = false;
            if (!FindEnchantEffect(spell, enchant, prismatic))
                continue;
            enchants.push_back({ spell, enchant, SkillRankToLevel(rank), 0, 0, prismatic });
        }
        SF_LOG_INFO("modules", "[mod-playerbots] Enchants: %u enchant candidate(s).", uint32(enchants.size()));
        return enchants;
    }

    BotStat StatFromCombatRating(uint32 rating)
    {
        switch (CombatRating(rating))
        {
            case CombatRating::CR_DODGE:       return BOT_STAT_DODGE;
            case CombatRating::CR_PARRY:       return BOT_STAT_PARRY;
            case CombatRating::CR_HIT_MELEE:
            case CombatRating::CR_HIT_RANGED:
            case CombatRating::CR_HIT_SPELL:   return BOT_STAT_HIT;
            case CombatRating::CR_CRIT_MELEE:
            case CombatRating::CR_CRIT_RANGED:
            case CombatRating::CR_CRIT_SPELL:  return BOT_STAT_CRIT;
            case CombatRating::CR_HASTE_MELEE:
            case CombatRating::CR_HASTE_RANGED:
            case CombatRating::CR_HASTE_SPELL: return BOT_STAT_HASTE;
            case CombatRating::CR_EXPERTISE:   return BOT_STAT_EXPERTISE;
            case CombatRating::CR_MASTERY:     return BOT_STAT_MASTERY;
            default:             return BOT_STAT_MAX;
        }
    }

    BotStat StatFromUnitStat(int32 stat)
    {
        switch (stat)
        {
            case STAT_STRENGTH:  return BOT_STAT_STRENGTH;
            case STAT_AGILITY:   return BOT_STAT_AGILITY;
            case STAT_STAMINA:   return BOT_STAT_STAMINA;
            case STAT_INTELLECT: return BOT_STAT_INTELLECT;
            case STAT_SPIRIT:    return BOT_STAT_SPIRIT;
            default:             return BOT_STAT_MAX;
        }
    }

    // Stats an aura spell gives, weighted; proc triggers count kProcUptime.
    float ScoreSpell(uint32 spellId, Player* bot, BotStatWeights const& weights, float factor, uint32 depth)
    {
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
        if (!spell || depth > 2)
            return 0.0f;

        float direct = 0.0f;
        float procs = 0.0f;
        bool attackPower = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            SpellEffectInfo const& effect = spell->Effects[i];
            if (!effect.IsAura())
                continue;
            float const amount = float(effect.CalcValue(bot));
            switch (effect.ApplyAuraName)
            {
                case SPELL_AURA_MOD_STAT:
                    if (effect.MiscValue < 0)
                    {
                        for (int32 stat = STAT_STRENGTH; stat <= STAT_SPIRIT; ++stat)
                            direct += weights.Get(StatFromUnitStat(stat)) * amount;
                    }
                    else if (BotStat stat = StatFromUnitStat(effect.MiscValue); stat != BOT_STAT_MAX)
                        direct += weights.Get(stat) * amount;
                    break;
                case SPELL_AURA_MOD_RATING:
                {
                    bool seen[BOT_STAT_MAX] = {};
                    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
                    {
                        if (!(uint32(effect.MiscValue) & (1u << rating)))
                            continue;
                        BotStat stat = StatFromCombatRating(rating);
                        if (stat == BOT_STAT_MAX || seen[stat])
                            continue;
                        seen[stat] = true;
                        direct += weights.Get(stat) * amount;
                    }
                    break;
                }
                case SPELL_AURA_MOD_DAMAGE_DONE:
                    if (effect.MiscValue & SPELL_SCHOOL_MASK_MAGIC)
                        direct += weights.Get(BOT_STAT_SPELL_POWER) * amount;
                    break;
                case SPELL_AURA_MOD_ATTACK_POWER:
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
                    if (!attackPower)
                        direct += weights.Get(BOT_STAT_ATTACK_POWER) * amount;
                    attackPower = true;
                    break;
                case SPELL_AURA_PROC_TRIGGER_SPELL:
                    if (effect.TriggerSpell && effect.TriggerSpell != spellId)
                        procs += ScoreSpell(effect.TriggerSpell, bot, weights, factor * kProcUptime, depth + 1);
                    break;
                default:
                    break;
            }
        }
        return direct * factor + procs;
    }

    float ScoreEnchant(SpellItemEnchantmentEntry const* enchant, Player* bot, BotStatWeights const& weights)
    {
        if (!enchant)
            return 0.0f;
        float score = 0.0f;
        for (uint8 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
        {
            switch (enchant->type[i])
            {
                case ITEM_ENCHANTMENT_TYPE_STAT:
                    score += weights.ForItemMod(enchant->spellid[i]) * float(enchant->amount[i]);
                    break;
                case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
                    score += ScoreSpell(enchant->spellid[i], bot, weights, 1.0f, 0);
                    break;
                case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:
                    score += ScoreSpell(enchant->spellid[i], bot, weights, kProcUptime, 0);
                    break;
                default:
                    break;
            }
        }
        return score;
    }

    bool HasSkill(Player* bot, uint32 skill, uint32 rank)
    {
        return !skill || (bot->HasSkill(skill) && bot->GetSkillValue(skill) >= rank);
    }

    // Level, profession and target checks shared by every candidate enchant.
    bool EnchantUsable(EnchantCandidate const& c, Player* bot, Item* item)
    {
        uint32 const level = bot->getLevel();
        SpellItemEnchantmentEntry const* enchant = c.enchant;
        if (c.level > level || !HasSkill(bot, c.skill, c.skillRank))
            return false;
        if (enchant->requiredMinLevel > level || (enchant->requiredMaxLevel && enchant->requiredMaxLevel < level))
            return false;
        // Ring enchants and other profession perks.
        if (!HasSkill(bot, enchant->requiredSkill, enchant->requiredSkillValue))
            return false;
        if (item->GetTemplate()->ItemLevel < c.spell->BaseLevel)
            return false;
        return item->IsFitToSpellRequirements(c.spell);
    }

    uint32 GemColorOfEnchant(uint32 enchantId)
    {
        SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!enchant || !enchant->GemID)
            return 0;
        ItemTemplate const* gem = sObjectMgr->GetItemTemplate(enchant->GemID);
        if (!gem)
            return 0;
        GemPropertiesEntry const* props = sGemPropertiesStore.LookupEntry(gem->GemProperties);
        return props ? props->color : 0;
    }

    struct Socket
    {
        uint8 index;
        uint32 color;   // socket color; kNormalGemColors for a buckle's prismatic socket
        bool prismatic;
    };

    struct GemPick
    {
        GemCandidate const* gem = nullptr;
        float score = 0.0f;
    };

    GemPick BestGem(Socket const& socket, bool matchColor, Player* bot, Item* item, std::vector<float> const& gemScores)
    {
        std::vector<GemCandidate> const& gems = GemCandidates();
        uint32 const level = bot->getLevel();
        uint32 const itemLevel = item->GetTemplate()->ItemLevel;
        bool const metaSocket = socket.color == SOCKET_COLOR_META;
        GemPick best;
        for (size_t i = 0; i < gems.size(); ++i)
        {
            GemCandidate const& gem = gems[i];
            bool const meta = gem.color == SOCKET_COLOR_META;
            if (meta != metaSocket)
                continue;
            if (!meta && matchColor && !socket.prismatic && !(gem.color & socket.color))
                continue;
            if (gemScores[i] <= best.score)
                continue;
            ItemTemplate const* proto = gem.proto;
            if (proto->RequiredLevel > level || proto->ItemLevel > itemLevel)
                continue;
            if (!HasSkill(bot, proto->RequiredSkill, proto->RequiredSkillRank))
                continue;
            if (meta && bot->CanEquipUniqueItem(proto, item->GetSlot(), 1) != EQUIP_ERR_OK)
                continue;
            best.gem = &gem;
            best.score = gemScores[i];
        }
        return best;
    }

    // Same order as WorldSession::HandleSocketOpcode.
    void SocketGems(Player* bot, Item* item, std::vector<std::pair<uint8, uint32>> const& picks)
    {
        uint8 const slot = item->GetSlot();
        bool const bonusBefore = item->GemsFitSockets();
        bot->ToggleMetaGemsActive(slot, false);

        for (uint32 enchantSlot = SOCK_ENCHANTMENT_SLOT; enchantSlot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchantSlot)
            bot->ApplyEnchantment(item, EnchantmentSlot(enchantSlot), false);
        for (auto const& pick : picks)
            item->SetEnchantment(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + pick.first), pick.second, 0, 0, bot->GetGUID());
        for (uint32 enchantSlot = SOCK_ENCHANTMENT_SLOT; enchantSlot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchantSlot)
            bot->ApplyEnchantment(item, EnchantmentSlot(enchantSlot), true);

        bool const bonusAfter = item->GemsFitSockets();
        if (bonusBefore != bonusAfter)
        {
            bot->ApplyEnchantment(item, BONUS_ENCHANTMENT_SLOT, false);
            item->SetEnchantment(BONUS_ENCHANTMENT_SLOT, bonusAfter ? item->GetTemplate()->socketBonus : 0, 0, 0, bot->GetGUID());
            bot->ApplyEnchantment(item, BONUS_ENCHANTMENT_SLOT, true);
        }

        bot->ToggleMetaGemsActive(slot, true);
        item->ClearSoulboundTradeable(bot);
    }

    uint32 GemItem(Player* bot, Item* item, BotStatWeights const& weights, std::vector<float> const& gemScores)
    {
        ItemTemplate const* proto = item->GetTemplate();
        std::vector<Socket> empty;
        bool bonusReachable = proto->socketBonus != 0;
        bool prismaticTaken = !item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT);
        for (uint8 i = 0; i < MAX_GEM_SOCKETS; ++i)
        {
            uint32 color = proto->Socket[i].Color;
            bool prismatic = false;
            if (!color)
            {
                // A buckle's socket is the first one the item does not have.
                if (prismaticTaken)
                    break;
                prismaticTaken = true;
                prismatic = true;
                color = kNormalGemColors;
            }
            if (color & (SOCKET_COLOR_HYDRAULIC | SOCKET_COLOR_COGWHEEL))
                continue;

            if (uint32 current = item->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i)))
            {
                if (!prismatic && !(GemColorOfEnchant(current) & color))
                    bonusReachable = false;
                continue;
            }
            empty.push_back({ i, color, prismatic });
        }
        if (empty.empty())
            return 0;

        // Best gems whatever their color, or matching colors plus the socket bonus.
        std::vector<std::pair<uint8, uint32>> anyColor, matched;
        float anyScore = 0.0f, matchedScore = 0.0f;
        bool matchedComplete = bonusReachable;
        for (Socket const& socket : empty)
        {
            GemPick pick = BestGem(socket, false, bot, item, gemScores);
            if (pick.gem)
            {
                anyColor.emplace_back(socket.index, pick.gem->enchant->ID);
                anyScore += pick.score;
            }
            if (!bonusReachable)
                continue;
            GemPick match = BestGem(socket, true, bot, item, gemScores);
            if (match.gem)
            {
                matched.emplace_back(socket.index, match.gem->enchant->ID);
                matchedScore += match.score;
            }
            else if (!socket.prismatic)
                matchedComplete = false;
        }
        if (matchedComplete)
            matchedScore += ScoreEnchant(sSpellItemEnchantmentStore.LookupEntry(proto->socketBonus), bot, weights);

        std::vector<std::pair<uint8, uint32>> const& picks =
            (bonusReachable && matchedScore >= anyScore) ? matched : anyColor;
        if (picks.empty())
            return 0;
        SocketGems(bot, item, picks);
        return uint32(picks.size());
    }

    void EnchantItem(Player* bot, Item* item, EnchantmentSlot slot, uint32 enchantId)
    {
        bot->ApplyEnchantment(item, slot, false);
        item->SetEnchantment(slot, enchantId, 0, 0, bot->GetGUID());
        bot->ApplyEnchantment(item, slot, true);
        item->ClearSoulboundTradeable(bot);
    }

    bool HasFreeSocketPosition(ItemTemplate const* proto)
    {
        for (uint8 i = 0; i < MAX_GEM_SOCKETS; ++i)
            if (!proto->Socket[i].Color)
                return true;
        return false;
    }

    uint32 EnchantGear(Player* bot, Item* item, BotStatWeights const& weights, bool replace)
    {
        uint32 applied = 0;
        EnchantCandidate const* best = nullptr;
        float bestScore = 0.0f;
        EnchantCandidate const* buckle = nullptr;
        bool const wantsBuckle = !item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT)
            && HasFreeSocketPosition(item->GetTemplate());
        for (EnchantCandidate const& candidate : EnchantCandidates())
        {
            if (candidate.prismatic)
            {
                if (wantsBuckle && !buckle && EnchantUsable(candidate, bot, item))
                    buckle = &candidate;
                continue;
            }
            float const score = ScoreEnchant(candidate.enchant, bot, weights);
            if (score <= bestScore || !EnchantUsable(candidate, bot, item))
                continue;
            best = &candidate;
            bestScore = score;
        }

        if (best)
        {
            uint32 const current = item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT);
            bool better = !current;
            if (current && replace && current != best->enchant->ID)
                better = bestScore > ScoreEnchant(sSpellItemEnchantmentStore.LookupEntry(current), bot, weights) + 0.5f;
            if (better)
            {
                EnchantItem(bot, item, PERM_ENCHANTMENT_SLOT, best->enchant->ID);
                ++applied;
            }
        }
        if (buckle)
        {
            EnchantItem(bot, item, PRISMATIC_ENCHANTMENT_SLOT, buckle->enchant->ID);
            ++applied;
        }
        return applied;
    }
}

BotEnchants::Result BotEnchants::Apply(Player* bot, uint32 specId, bool enchants, bool gems, bool replaceEnchants)
{
    Result result;
    if (!bot || (!enchants && !gems))
        return result;

    BotStatWeights const& weights = BotStatWeightTable::For(specId, bot->getClass());

    std::vector<float> gemScores;
    if (gems)
    {
        std::vector<GemCandidate> const& candidates = GemCandidates();
        gemScores.reserve(candidates.size());
        for (GemCandidate const& gem : candidates)
            gemScores.push_back(ScoreEnchant(gem.enchant, bot, weights));
    }

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item || !item->GetTemplate())
            continue;
        if (enchants)
            result.enchants += EnchantGear(bot, item, weights, replaceEnchants);
        if (gems)
            result.gems += GemItem(bot, item, weights, gemScores);
    }
    return result;
}
