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
	X(BotStays, "BOT_STAYS")

enum class Message {
#define X(name, str) name,
	MESSAGE_CODE_LIST(X)
#undef X
};

/** Код сообщения (для JSON/API), не пользовательский текст. */
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

/**
 * Пользовательские строки для stdout/веба — одинаковые на всех платформах.
 * Аргументы (например направление) передаются в том же порядке, что и при logMessage.
 */
inline std::string formatMessage(Message m, std::initializer_list<std::string> args = {}) {
	std::vector<std::string> a(args);
	switch (m) {
		case Message::InvalidTargetPlayer:
			return "Нет такого игрока";
		case Message::NotYourMove:
			return "Сейчас не ваш ход";
		case Message::UnknownItem:
			return "Неизвестный предмет";
		case Message::Moved:
			if (!a.empty()) return std::string("Прошёл ") + a[0];
			return "Прошёл";
		case Message::BotMoved:
			return "Бот походил";
		case Message::KilledByBot:
			// Формат для main.cpp: разбор PLAYER:имя:текст
			if (!a.empty()) return std::string("PLAYER:") + a[0] + ": Вас убил бот. Вы в больнице.";
			return "PLAYER:?";
		case Message::InnerWallCrash:
			if (!a.empty()) return std::string("Врезался в стену (") + a[0] + ")";
			return "Врезался в стену";
		case Message::OuterWallCrash:
			if (!a.empty()) return std::string("Врезался во внешнюю стену (") + a[0] + ")";
			return "Врезался во внешнюю стену";
		case Message::ExitFoundWithTreasure:
			return "Выход найден на внешней стене! Игрок вынес сокровище!";
		case Message::ExitFoundWithoutTreasure:
			return "Выход найден на внешней стене, но без сокровища.";
		case Message::TreasureFound:
			return "Поднято сокровище с земли.";
		case Message::TreasurePicked:
			return "Подобрано сокровище.";
		case Message::FlashLightFound:
			return "Подобран фонарь";
		case Message::RifleFound:
			return "Подобрано ружьё";
		case Message::ShotgunFound:
			return "Подобран дробовик";
		case Message::KnifeFound:
			return "Подобран нож";
		case Message::ArmourFound:
			return "Подобрана броня";
		case Message::ItemFound:
			if (!a.empty()) return std::string("Подобран предмет: ") + a[0];
			return "Подобран предмет";
		case Message::Breathe:
			return "Вы чувствуете чьё-то дыхание поблизости.";
		case Message::BotStays:
			return "Нет свободной клетки — бот остаётся на месте.";
	}
	return "";
}
