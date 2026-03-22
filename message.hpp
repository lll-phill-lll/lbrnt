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

inline bool messageFromCodeString(const std::string& code, Message& out) {
#define X(name, str) \
	if (code == str) { \
		out = Message::name; \
		return true; \
	}
	MESSAGE_CODE_LIST(X)
#undef X
	(void)out;
	return false;
}

/** Сообщение в формате wire: `CODE` или `CODE:arg1:arg2…` */
inline std::string messageWire(Message m, std::initializer_list<std::string> args = {}) {
	std::string s = messageCodeString(m);
	if (args.size() == 0) return s;
	for (const auto& a : args) {
		s += ':';
		s += a;
	}
	return s;
}

inline void splitWireLine(const std::string& line, std::string& code, std::vector<std::string>& args) {
	size_t p = line.find(':');
	if (p == std::string::npos) {
		code = line;
		args.clear();
		return;
	}
	code = line.substr(0, p);
	args.clear();
	size_t start = p + 1;
	while (start < line.size()) {
		size_t q = line.find(':', start);
		if (q == std::string::npos) {
			args.push_back(line.substr(start));
			break;
		}
		args.push_back(line.substr(start, q - start));
		start = q + 1;
	}
}

inline std::string formatMessageVec(Message m, const std::vector<std::string>& a) {
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
			return "Вас убил бот. Вы в больнице.";
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
		case Message::HospitalEnter:
			return "Вы нашли больницу.";
		case Message::HospitalExit:
			return "Вы покинули больницу.";
		case Message::ArsenalEnter:
			return "Вы нашли арсенал.";
		case Message::ArsenalExit:
			return "Вы покинули арсенал.";
		case Message::KnifeFixed:
			return "Ваш нож починен.";
		case Message::RifleFixed:
			return "Ружьё перезаряжено.";
		case Message::ShotgunFixed:
			return "Дробовик перезаряжен.";
		case Message::LanternFixed:
			return "Фонарь заряжен.";
		case Message::ItemRecharged:
			if (!a.empty()) return std::string("Предмет перезаряжен: ") + a[0];
			return "Предмет перезаряжен";
		case Message::BotDestroyedRelocated:
			return "Бот уничтожен и появляется в другом месте.";
		case Message::ArmorAbsorbedHit:
			if (!a.empty()) return std::string("Игрок ") + a[0] + " защищён бронёй! Броня уничтожена.";
			return "Игрок защищён бронёй! Броня уничтожена.";
		case Message::HospitalNotFound:
			return "Больница не найдена";
		case Message::TreasureUseHint:
			return "Сокровище нельзя «использовать» — дойдите до выхода с внешней стороны лабиринта.";
		case Message::ArmorPassive:
			return "Броня — пассивный предмет. Её нельзя использовать.";
		case Message::ExitLocationWithTreasure:
			return "Выход найден! Игрок вынес сокровище!";
		case Message::ExitLocationWithoutTreasure:
			return "Выход найден, но без сокровища.";
		case Message::TreasureSpotFound:
			return "Нашёл сокровище!";
		case Message::FlashlightBlocked:
			if (!a.empty()) return std::string("Фонарь ") + a[0] + ": путь закрыт стеной";
			return "Фонарь: путь закрыт стеной";
		case Message::FlashlightBeam: {
			if (a.size() < 3) return "Фонарь";
			std::string s = std::string("Фонарь ") + a[0] + " " + a[1] + ": " + a[2];
			if (a.size() >= 4 && a[3] == "player") s += " + игрок";
			return s;
		}
		case Message::FlashlightDropped:
			return "Фонарь выпал где-то неподалёку.";
		case Message::ItemBroken:
			return "Нож сломан";
		case Message::ItemDepleted:
			return "Предмет исчерпан";
		case Message::RifleMiss:
			if (!a.empty()) return std::string("Ружьё ") + a[0] + ": промах";
			return "Ружьё: промах";
		case Message::RifleHitPlayer:
			if (a.size() >= 2) return std::string("Ружьё ") + a[0] + ": игрок " + a[1] + " отправлен в больницу";
			return "Ружьё: игрок отправлен в больницу";
		case Message::RifleHitBot:
			if (!a.empty()) return std::string("Ружьё ") + a[0] + ": бот уничтожен";
			return "Ружьё: бот уничтожен";
		case Message::ShotgunWall:
			if (!a.empty()) return std::string("Дробовик ") + a[0] + ": стена перед вами";
			return "Дробовик: стена перед вами";
		case Message::ShotgunMiss:
			if (!a.empty()) return std::string("Дробовик ") + a[0] + ": промах";
			return "Дробовик: промах";
		case Message::ShotgunHitPlayer:
			if (a.size() >= 2) return std::string("Дробовик ") + a[0] + ": игрок " + a[1] + " отправлен в больницу";
			return "Дробовик: игрок отправлен в больницу";
		case Message::ShotgunHitBot:
			if (!a.empty()) return std::string("Дробовик ") + a[0] + ": бот уничтожен";
			return "Дробовик: бот уничтожен";
		case Message::KnifeMiss:
			if (!a.empty()) return std::string("Удар ножом ") + a[0] + " — мимо";
			return "Удар ножом — мимо";
		case Message::KnifeHitPlayer:
			if (a.size() >= 2) return std::string("Удар ножом ") + a[0] + ": игрок " + a[1] + " отправлен в больницу";
			return "Удар ножом: игрок отправлен в больницу";
		case Message::KnifeHitBot:
			if (!a.empty()) return std::string("Удар ножом ") + a[0] + ": бот уничтожен";
			return "Удар ножом: бот уничтожен";
		case Message::KnifeSpent:
			return "Нож потрачен";
	}
	return "";
}

inline std::string formatMessage(Message m, std::initializer_list<std::string> args = {}) {
	return formatMessageVec(m, std::vector<std::string>(args.begin(), args.end()));
}

/** Декодирует одну строку wire в русский текст. Неизвестный код — возврат строки как есть (legacy). */
inline std::string wireToDisplayRuInner(const std::string& line) {
	std::string code;
	std::vector<std::string> args;
	splitWireLine(line, code, args);
	Message m;
	if (!messageFromCodeString(code, m)) return line;
	return formatMessageVec(m, args);
}

inline std::string wireToDisplayRu(const std::string& line) {
	if (line.rfind("PLAYER:", 0) == 0) {
		size_t p = line.find(':', 7);
		if (p == std::string::npos) return line;
		std::string victim = line.substr(7, p - 7);
		std::string rest = line.substr(p + 1);
		return std::string("PLAYER:") + victim + ":" + wireToDisplayRuInner(rest);
	}
	return wireToDisplayRuInner(line);
}

inline void appendWire(std::vector<std::string>& messages, Message m, std::initializer_list<std::string> args = {}) {
	messages.push_back(messageWire(m, args));
}
