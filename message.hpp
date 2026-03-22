#pragma once

#include <string>

#define MESSAGE_CODE_LIST(X) \
    X(InvalidTargetPlayer, "INVALID_TARGET_PLAYER") \
    X(NotYourMove, "NOT_YOUR_MOVE") \
    X(UnknownItem, "UNKNOWN_ITEM") \
    X(Moved, "MOVED") \
    X(BotMoved, "BOT_MOVED") \
    X(KilledByBot, "KILLED_BY_BOT") \
    X(InnerWallCrash, "INNER_WALL_CRASH") \
    X(OuterWallCrash, "OUTER_WALL_CRASH") \
    X(ExitFoundWithTreasure, "EXIT_FOUND_WITH_TREASURE") \
    X(ExitFoundWithoutTreasure, "EXIT_FOUND_WITHOUT_TREASURE") \
    X(TreasureFound, "TREASURE_FOUND") /* Item commands */ \
    X(FlashLightFound, "FLASHLIGHT_FOUND") \
    X(RifleFound, "RIFLE_FOUND") \
    X(ShotgunFound, "SHOTGUN_FOUND") \
    X(KnifeFound, "KNIFE_FOUND") \
    X(ArmourFound, "ARMOR_FOUND") \
    X(ItemFound, "ITEM_FOUND") \
    X(Breathe, "BREATHE") \
    X(BotStays, "BOT_STAYS") \

enum class Message {
#define X(name, str) name,
    MESSAGE_CODE_LIST(X)
#undef X
};

std::string toString(Message messageCode) {
    switch (messageCode) {
#define X(name, str) case Message::name: return str;
        MESSAGE_CODE_LIST(X)
#undef X
    }
}

const std::string ArgsSeparator = ":";
