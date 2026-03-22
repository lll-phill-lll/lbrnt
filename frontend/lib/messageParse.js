
// Синхронизировано с message.hpp (MESSAGE_CODE_LIST). Concise — канонический короткий текст; verbose — варианты для UI. Движок отдаёт только wire.

const makeEnum = (...keys) => Object.fromEntries(keys.map(k => [k, k]));

export const MESSAGE_CODES = makeEnum(
    'INVALID_TARGET_PLAYER',
    'NOT_YOUR_MOVE',
    'UNKNOWN_ITEM',
    'MOVED',
    'BOT_MOVED',
    'KILLED_BY_BOT',
    'INNER_WALL_CRASH',
    'OUTER_WALL_CRASH',
    'EXIT_FOUND_WITH_TREASURE',
    'EXIT_FOUND_WITHOUT_TREASURE',
    'TREASURE_FOUND',
    'TREASURE_PICKED',
    'FLASHLIGHT_FOUND',
    'RIFLE_FOUND',
    'SHOTGUN_FOUND',
    'KNIFE_FOUND',
    'ARMOR_FOUND',
    'ITEM_FOUND',
    'BREATHE',
    'BOT_STAYS',
    'HOSPITAL_ENTER',
    'HOSPITAL_EXIT',
    'ARSENAL_ENTER',
    'ARSENAL_EXIT',
    'KNIFE_FIXED',
    'RIFLE_FIXED',
    'SHOTGUN_FIXED',
    'LANTERN_FIXED',
    'ITEM_RECHARGED',
    'BOT_DESTROYED_RELOCATED',
    'ARMOR_ABSORBED_HIT',
    'HOSPITAL_NOT_FOUND',
    'TREASURE_USE_HINT',
    'ARMOR_PASSIVE',
    'EXIT_LOCATION_WITH_TREASURE',
    'EXIT_LOCATION_WITHOUT_TREASURE',
    'TREASURE_SPOT_FOUND',
    'FLASHLIGHT_BLOCKED',
    'FLASHLIGHT_BEAM',
    'FLASHLIGHT_DROPPED',
    'ITEM_BROKEN',
    'ITEM_DEPLETED',
    'RIFLE_MISS',
    'RIFLE_HIT_PLAYER',
    'RIFLE_HIT_BOT',
    'SHOTGUN_WALL',
    'SHOTGUN_MISS',
    'SHOTGUN_HIT_PLAYER',
    'SHOTGUN_HIT_BOT',
    'KNIFE_MISS',
    'KNIFE_HIT_PLAYER',
    'KNIFE_HIT_BOT',
    'KNIFE_SPENT',
);

const messageMap = {
    [MESSAGE_CODES.BREATHE]: {
        concise: () => 'Вы чувствуете чьё-то дыхание поблизости.',
        verbose: () => [
            'Среди вязкой тишины ты улавливаешь чужое дыхание. Оно совсем рядом, почти у самой границы твоего личного пространства: тёплое, живое, настороженное.',
            'Сквозь запах собственной кожи пробивается чужое присутствие. Его нельзя увидеть, но можно почувствовать — кто-то рядом греет воздух своим дыханием.',
            'Ты замираешь, и тьма отвечает тебе чужими лёгкими. Очень тихий вдох и едва заметный выдох, как будто кто-то стоит рядом и тоже боится выдать себя лишним звуком.',
        ],
    },
    [MESSAGE_CODES.HOSPITAL_ENTER]: {
        concise: () => 'Вы нашли больницу.',
        verbose: () => [],
    },
    [MESSAGE_CODES.HOSPITAL_EXIT]: {
        concise: () => 'Вы покинули больницу.',
        verbose: () => [],
    },
    [MESSAGE_CODES.ARSENAL_ENTER]: {
        concise: () => 'Вы нашли арсенал.',
        verbose: () => [
            'Воздух вокруг становится тяжелее и суше. В нём проступает густой запах оружейного масла, ' +
                'металла и старого пороха, а пальцы вдруг ощущают знакомую надёжность: ремни подтянуты, ' +
                'механизмы больше не люфтят, сталь снова будто дышит силой.',
            'Под ногами меняется пол. Вместо голого камня — холодные ровные плиты, а в тишине проступает ' +
                'едва слышный звон металла о металл. Всё, что висело на тебе изношенным грузом, теперь ощущается ' +
                'исправным и готовым к бойне.',
            'Ноздри щиплет запах смазки, угля и железной пыли. Где-то совсем рядом тихо покачивается цепь, и это ' +
                'место встречает тебя как мастерская войны: затворы больше не заедают, а оружие словно вспомнило ' +
                'своё предназначение.',
        ],
    },
    [MESSAGE_CODES.ARSENAL_EXIT]: {
        concise: () => 'Вы покинули арсенал.',
        verbose: () => [
            'Запах масла и железа начинает отступать. Воздух снова становится сырым, пустым и чужим.',
            'Из-под ног уходят ровные, уверенные плиты оружейни. Мир снова становится шероховатым, сырым и непредсказуемым.',
            'Последний след оружейного масла растворяется в воздухе. Теперь рядом нет ничего, кроме темноты и того, что она прячет.',
            'Арсенал отпускает тебя неохотно. Его сухой, металлический дух остаётся позади, а впереди лишь чёрный коридор неизвестности.',
        ],
    },
    [MESSAGE_CODES.KNIFE_FIXED]: {
        concise: () => 'Ваш нож починен.',
        verbose: () => [],
    },
    [MESSAGE_CODES.RIFLE_FIXED]: {
        concise: () => 'Ружьё перезаряжено.',
        verbose: () => [],
    },
    [MESSAGE_CODES.SHOTGUN_FIXED]: {
        concise: () => 'Дробовик перезаряжен.',
        verbose: () => [],
    },
    [MESSAGE_CODES.LANTERN_FIXED]: {
        concise: () => 'Фонарь заряжен.',
        verbose: () => [],
    },
    [MESSAGE_CODES.ITEM_RECHARGED]: {
        concise: (item) => `Предмет перезаряжен: ${item}`,
        verbose: () => [],
    },
    [MESSAGE_CODES.BOT_MOVED]: {
        concise: () => 'Бот походил',
        verbose: () => [],
    },
    [MESSAGE_CODES.KILLED_BY_BOT]: {
        concise: () => 'Вас убил бот. Вы в больнице.',
        verbose: () => [],
    },
    [MESSAGE_CODES.INVALID_TARGET_PLAYER]: {
        concise: () => 'Нет такого игрока',
        verbose: () => [],
    },
    [MESSAGE_CODES.NOT_YOUR_MOVE]: {
        concise: () => 'Сейчас не ваш ход',
        verbose: () => [],
    },
    [MESSAGE_CODES.UNKNOWN_ITEM]: {
        concise: () => 'Неизвестный предмет',
        verbose: () => [],
    },
    [MESSAGE_CODES.MOVED]: {
        concise: (direction) => (direction ? `Прошёл ${direction}` : 'Прошёл'),
        verbose: () => [],
    },
    [MESSAGE_CODES.INNER_WALL_CRASH]: {
        concise: (direction) => (direction ? `Врезался в стену (${direction})` : 'Врезался в стену'),
        verbose: () => [],
    },
    [MESSAGE_CODES.OUTER_WALL_CRASH]: {
        concise: (direction) => (direction ? `Врезался во внешнюю стену (${direction})` : 'Врезался во внешнюю стену'),
        verbose: () => [],
    },
    [MESSAGE_CODES.EXIT_FOUND_WITH_TREASURE]: {
        concise: () => 'Выход найден на внешней стене! Игрок вынес сокровище!',
        verbose: () => [],
    },
    [MESSAGE_CODES.EXIT_FOUND_WITHOUT_TREASURE]: {
        concise: () => 'Выход найден на внешней стене, но без сокровища.',
        verbose: () => [],
    },
    [MESSAGE_CODES.TREASURE_FOUND]: {
        concise: () => 'Поднято сокровище с земли.',
        verbose: () => [],
    },
    [MESSAGE_CODES.TREASURE_PICKED]: {
        concise: () => 'Подобрано сокровище.',
        verbose: () => [],
    },
    [MESSAGE_CODES.FLASHLIGHT_FOUND]: {
        concise: () => 'Подобран фонарь',
        verbose: () => [],
    },
    [MESSAGE_CODES.RIFLE_FOUND]: {
        concise: () => 'Подобрано ружьё',
        verbose: () => [],
    },
    [MESSAGE_CODES.SHOTGUN_FOUND]: {
        concise: () => 'Подобран дробовик',
        verbose: () => [],
    },
    [MESSAGE_CODES.KNIFE_FOUND]: {
        concise: () => 'Подобран нож',
        verbose: () => [],
    },
    [MESSAGE_CODES.ARMOR_FOUND]: {
        concise: () => 'Подобрана броня',
        verbose: () => [],
    },
    [MESSAGE_CODES.ITEM_FOUND]: {
        concise: (item) => `Подобран предмет: ${item}`,
        verbose: () => [],
    },
    [MESSAGE_CODES.BOT_STAYS]: {
        concise: () => 'Нет свободной клетки — бот остаётся на месте.',
        verbose: () => [],
    },
    [MESSAGE_CODES.BOT_DESTROYED_RELOCATED]: {
        concise: () => 'Бот уничтожен и появляется в другом месте.',
        verbose: () => [],
    },
    [MESSAGE_CODES.ARMOR_ABSORBED_HIT]: {
        concise: (victim) => (victim ? `Игрок ${victim} защищён бронёй! Броня уничтожена.` : 'Игрок защищён бронёй! Броня уничтожена.'),
        verbose: () => [],
    },
    [MESSAGE_CODES.HOSPITAL_NOT_FOUND]: {
        concise: () => 'Больница не найдена',
        verbose: () => [],
    },
    [MESSAGE_CODES.TREASURE_USE_HINT]: {
        concise: () => 'Сокровище нельзя «использовать» — дойдите до выхода с внешней стороны лабиринта.',
        verbose: () => [],
    },
    [MESSAGE_CODES.ARMOR_PASSIVE]: {
        concise: () => 'Броня — пассивный предмет. Её нельзя использовать.',
        verbose: () => [],
    },
    [MESSAGE_CODES.EXIT_LOCATION_WITH_TREASURE]: {
        concise: () => 'Выход найден! Игрок вынес сокровище!',
        verbose: () => [],
    },
    [MESSAGE_CODES.EXIT_LOCATION_WITHOUT_TREASURE]: {
        concise: () => 'Выход найден, но без сокровища.',
        verbose: () => [],
    },
    [MESSAGE_CODES.TREASURE_SPOT_FOUND]: {
        concise: () => 'Нашёл сокровище!',
        verbose: () => [],
    },
    [MESSAGE_CODES.FLASHLIGHT_BLOCKED]: {
        concise: (dir) => (dir ? `Фонарь ${dir}: путь закрыт стеной` : 'Фонарь: путь закрыт стеной'),
        verbose: () => [],
    },
    [MESSAGE_CODES.FLASHLIGHT_BEAM]: {
        concise: (dir, step, cell, extra) => {
            let s = `Фонарь ${dir} ${step}: ${cell}`;
            if (extra === 'player') s += ' + игрок';
            return s;
        },
        verbose: () => [],
    },
    [MESSAGE_CODES.FLASHLIGHT_DROPPED]: {
        concise: () => 'Фонарь выпал где-то неподалёку.',
        verbose: () => [],
    },
    [MESSAGE_CODES.ITEM_BROKEN]: {
        concise: () => 'Нож сломан',
        verbose: () => [],
    },
    [MESSAGE_CODES.ITEM_DEPLETED]: {
        concise: () => 'Предмет исчерпан',
        verbose: () => [],
    },
    [MESSAGE_CODES.RIFLE_MISS]: {
        concise: (dir) => (dir ? `Ружьё ${dir}: промах` : 'Ружьё: промах'),
        verbose: () => [],
    },
    [MESSAGE_CODES.RIFLE_HIT_PLAYER]: {
        concise: (dir, pl) => (dir && pl ? `Ружьё ${dir}: игрок ${pl} отправлен в больницу` : 'Ружьё: игрок отправлен в больницу'),
        verbose: () => [],
    },
    [MESSAGE_CODES.RIFLE_HIT_BOT]: {
        concise: (dir) => (dir ? `Ружьё ${dir}: бот уничтожен` : 'Ружьё: бот уничтожен'),
        verbose: () => [],
    },
    [MESSAGE_CODES.SHOTGUN_WALL]: {
        concise: (dir) => (dir ? `Дробовик ${dir}: стена перед вами` : 'Дробовик: стена перед вами'),
        verbose: () => [],
    },
    [MESSAGE_CODES.SHOTGUN_MISS]: {
        concise: (dir) => (dir ? `Дробовик ${dir}: промах` : 'Дробовик: промах'),
        verbose: () => [],
    },
    [MESSAGE_CODES.SHOTGUN_HIT_PLAYER]: {
        concise: (dir, pl) => (dir && pl ? `Дробовик ${dir}: игрок ${pl} отправлен в больницу` : 'Дробовик: игрок отправлен в больницу'),
        verbose: () => [],
    },
    [MESSAGE_CODES.SHOTGUN_HIT_BOT]: {
        concise: (dir) => (dir ? `Дробовик ${dir}: бот уничтожен` : 'Дробовик: бот уничтожен'),
        verbose: () => [],
    },
    [MESSAGE_CODES.KNIFE_MISS]: {
        concise: (dir) => (dir ? `Удар ножом ${dir} — мимо` : 'Удар ножом — мимо'),
        verbose: () => [],
    },
    [MESSAGE_CODES.KNIFE_HIT_PLAYER]: {
        concise: (dir, pl) => (dir && pl ? `Удар ножом ${dir}: игрок ${pl} отправлен в больницу` : 'Удар ножом: игрок отправлен в больницу'),
        verbose: () => [],
    },
    [MESSAGE_CODES.KNIFE_HIT_BOT]: {
        concise: (dir) => (dir ? `Удар ножом ${dir}: бот уничтожен` : 'Удар ножом: бот уничтожен'),
        verbose: () => [],
    },
    [MESSAGE_CODES.KNIFE_SPENT]: {
        concise: () => 'Нож потрачен',
        verbose: () => [],
    },
};

/**
 * @returns {{ concise: string, verbose: string|null }}
 */
export function parseMessage(messageCode) {
    const parts = String(messageCode).split(':');
    const code = parts[0];
    const args = parts.slice(1);

    const messageEntry = messageMap[code];
    if (!messageEntry) {
        const supportedCodes = Object.keys(messageMap).join(', ');
        throw new Error(`${code} is not supported by message map in frontend. Supported: ${supportedCodes}`);
    }

    const concise = messageEntry.concise(...args);
    const verboseOptions = messageEntry.verbose(...args);
    const arr = Array.isArray(verboseOptions) ? verboseOptions : [];
    const randomVerbose =
        arr.length > 0 ? arr[Math.floor(Math.random() * arr.length)] : null;
    return {
        concise,
        verbose: randomVerbose,
    };
}

/** Неизвестный код / сырая строка — показываем как есть (legacy). */
export function parseMessageSafe(line) {
    const s = String(line ?? '').trim();
    if (!s) return { concise: '', verbose: null };
    const parts = s.split(':');
    const code = parts[0];
    if (!messageMap[code]) {
        return { concise: s, verbose: null };
    }
    try {
        return parseMessage(s);
    } catch {
        return { concise: s, verbose: null };
    }
}
