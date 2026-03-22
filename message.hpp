#pragma once

#include <initializer_list>
#include <string>
#include <vector>

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
	X(TreasureFound, "TREASURE_FOUND") \
	X(TreasurePicked, "TREASURE_PICKED") \
	X(FlashLightFound, "FLASHLIGHT_FOUND") \
	X(RifleFound, "RIFLE_FOUND") \
	X(ShotgunFound, "SHOTGUN_FOUND") \
	X(KnifeFound, "KNIFE_FOUND") \
	X(ArmourFound, "ARMOR_FOUND") \
	X(ItemFound, "ITEM_FOUND") \
	X(Breathe, "BREATHE") \
	X(BotStays, "BOT_STAYS") \
	X(HospitalEnter, "HOSPITAL_ENTER") \
	X(HospitalExit, "HOSPITAL_EXIT") \
	X(ArsenalEnter, "ARSENAL_ENTER") \
	X(ArsenalExit, "ARSENAL_EXIT") \
	X(KnifeFixed, "KNIFE_FIXED") \
	X(RifleFixed, "RIFLE_FIXED") \
	X(ShotgunFixed, "SHOTGUN_FIXED") \
	X(LanternFixed, "LANTERN_FIXED") \
	X(ItemRecharged, "ITEM_RECHARGED") \
	X(BotDestroyedRelocated, "BOT_DESTROYED_RELOCATED") \
	X(ArmorAbsorbedHit, "ARMOR_ABSORBED_HIT") \
	X(HospitalNotFound, "HOSPITAL_NOT_FOUND") \
	X(TreasureUseHint, "TREASURE_USE_HINT") \
	X(ArmorPassive, "ARMOR_PASSIVE") \
	X(ExitLocationWithTreasure, "EXIT_LOCATION_WITH_TREASURE") \
	X(ExitLocationWithoutTreasure, "EXIT_LOCATION_WITHOUT_TREASURE") \
	X(TreasureSpotFound, "TREASURE_SPOT_FOUND") \
	X(FlashlightBlocked, "FLASHLIGHT_BLOCKED") \
	X(FlashlightBeam, "FLASHLIGHT_BEAM") \
	X(FlashlightDropped, "FLASHLIGHT_DROPPED") \
	X(ItemBroken, "ITEM_BROKEN") \
	X(ItemDepleted, "ITEM_DEPLETED") \
	X(RifleMiss, "RIFLE_MISS") \
	X(RifleHitPlayer, "RIFLE_HIT_PLAYER") \
	X(RifleHitBot, "RIFLE_HIT_BOT") \
	X(ShotgunWall, "SHOTGUN_WALL") \
	X(ShotgunMiss, "SHOTGUN_MISS") \
	X(ShotgunHitPlayer, "SHOTGUN_HIT_PLAYER") \
	X(ShotgunHitBot, "SHOTGUN_HIT_BOT") \
	X(KnifeMiss, "KNIFE_MISS") \
	X(KnifeHitPlayer, "KNIFE_HIT_PLAYER") \
	X(KnifeHitBot, "KNIFE_HIT_BOT") \
	X(KnifeSpent, "KNIFE_SPENT")

enum class Message {
#define X(name, str) name,
	MESSAGE_CODE_LIST(X)
#undef X
};

/** Строковый код для wire (совпадает с MESSAGE_CODES в messageParse.js). */
inline const char* messageCodeString(Message m) {
	switch (m) {
#define X(name, str) \
	case Message::name: \
		return str;
		MESSAGE_CODE_LIST(X)
#undef X
	}
	return "";
}

/** Сообщение в формате wire: `CODE` или `CODE:arg1:arg2…` — только движок; человекочитаемый текст в JS. */
inline std::string messageWire(Message m, std::initializer_list<std::string> args = {}) {
	std::string s = messageCodeString(m);
	if (args.size() == 0) return s;
	for (const auto& a : args) {
		s += ':';
		s += a;
	}
	return s;
}

inline void appendWire(std::vector<std::string>& messages, Message m, std::initializer_list<std::string> args = {}) {
	messages.push_back(messageWire(m, args));
}
