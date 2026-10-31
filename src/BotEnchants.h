/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Enchants and gems for a bot's equipped gear, chosen by its spec's stat weights.
*/

#ifndef _SF_BOT_ENCHANTS_H
#define _SF_BOT_ENCHANTS_H

#include "Define.h"

class Player;

namespace BotEnchants
{
    struct Result
    {
        uint32 enchants = 0;
        uint32 gems = 0;
    };

    // Enchants first (a belt buckle adds a socket), then gems in empty sockets.
    // replaceEnchants: swap an existing enchant for a better one; when false only
    // items without an enchant get one. Gems only ever go into empty sockets.
    Result Apply(Player* bot, uint32 specId, bool enchants, bool gems, bool replaceEnchants);
}

#endif
