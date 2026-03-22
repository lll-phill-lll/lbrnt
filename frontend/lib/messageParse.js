
// TODO: add multiple concise variants
// TODO: add multiple languages

const makeEnum = (...keys) => Object.fromEntries(keys.map(k => [k, k])); // Creating map from keys to keys for referring as const values
export const MESSAGE_CODES = makeEnum(
    'BREATHE',
    'ARSENAL_ENTER',
    'ARSENAL_EXIT',
    'KNIFE_FIXED',
    'RIFLE_FIXED',
    'LANTERN_FIXED',
    'INVALID_TARGET_PLAYER',
    'NOT_YOUR_MOVE',
    'EXIT_FOUND_WITH_TREASURE',
    'EXIT_FOUND_WITHOUT_TREASURE',
    'MOVED',
    'BOT_MOVED',
    'KILLED_BY_BOT',
    'INNER_WALL_CRASH',
    'OUTER_WALL_CRASH',
    'TREASURE_FOUND',
    'FLASHLIGHT_FOUND',
    'RIFLE_FOUND',
    'SHOTGUN_FOUND',
    'KNIFE_FOUND',
    'ARMOR_FOUND',
    'ITEM_FOUND',
    'UNKNOWN_ITEM',
    'BOT_STAYS',
);

const messageMap = {
    [MESSAGE_CODES.BREATHE]: {
        concise: () => 'Вы чувствуете чье-то дыхание поблизости',
        verbose: () => [
            'Среди вязкой тишины ты улавливаешь чужое дыхание. Оно совсем рядом, почти у самой ' +
                'границы твоего личного пространства: тёплое, живое, настороженное.',
            'Сквозь запах собственной кожи пробивается чужое присутствие. Его нельзя увидеть ' + 
                'но можно почувствовать, кто-то рядом греет воздух своим дыханием.',
            'Ты замираешь и тьма отвечает тебе чужими лёгкими. Очень тихий вдох и едва заметный ' +
                'заметный выдох, как будто кто-то стоит рядом и тоже боится выдать себя лишним звуком.'
        ],
    },
    [MESSAGE_CODES.ARSENAL_ENTER]: {
        concise: () => 'Вы нашли арсенал',
        verbose: () => [
            'Воздух вокруг становится тяжелее и суше. В нем проступает густой запах оружейного масла, ' +
                'металла и старого пороха, а пальцы вдруг ощущают знакомую надежность: ремни подтянуты, ' +
                'механизмы больше не люфтят, сталь снова будто дышит силой.',
            'Под ногами меняется пол. Вместо голого камня, холодные ровные плиты, а в тишине проступает ' +
                'едва слышный звон металла о металл. Все что висело на тебе изношенным грузом, теперь ощущается ' +
                'исправны и готовым к бойне.',
            'Ноздри щиплет запах смазки, угля и железной пыли. Где-то совсем рядом тихо покачивается цепь, и это ' +
                'место встречает тебя как мастерская войны: затвроы больше не заедают, а оружие словно вспомнило ' +
                'свое предназначение',
        ],
    },
    [MESSAGE_CODES.ARSENAL_EXIT]: {
        concise: () => 'Вы покинули арсенал',
        verbose: () => [
            'Запах масла и железа начинает отступать. Воздух снова становится сырым, пустым и чужим',
            'Из-под ног уходят ровные, уверенные плиты оружейни. Мир снова становится шероховатым, сырым и непредсказуемым',
            'Последний след оружейного масла растворяется в воздухе. Теперь рядом нет ничего, кроме темноты и того, что она прячет',
            'Арсенал отпускает тебя неохотно. Его сухой, металлический дух остается позади, а впереди лишь черный коридор неизвестности',
        ],
    },
    [MESSAGE_CODES.KNIFE_FIXED]: {
        concise: () => 'Ваш нож починен',
        verbose: () => [],
    },
    [MESSAGE_CODES.RIFLE_FIXED]: {
        concise: () => 'Ружье перезаряжено',
        verbose: () => [],
    },
    [MESSAGE_CODES.LANTERN_FIXED]: {
        concise: () => 'Фонарь заряжен',
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
    [MESSAGE_CODES.MOVED]: {
        concise: (direction) => `Прошел ${direction}`,
        verbose: (direction) => [],
    },
    [MESSAGE_CODES.INNER_WALL_CRASH]: {
        concise: (direction) => `Врезался в стену (${direction})`,
        verbose: (direction) => [],
    },
    [MESSAGE_CODES.OUTER_WALL_CRASH]: {
        concise: (direction) => `Врезался во внешнюю стену (${direction})`,
        verbose: (direction) => [],
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
        verbose: (item) => [],
    },
    [MESSAGE_CODES.UNKNOWN_ITEM]: {
        concise: () => 'Неизвестный предмет',
        verbose: () => [],
    },
    [MESSAGE_CODES.BOT_STAYS]: {
        concise: () => 'Нет свободной клетки — бот остаётся на месте.',
        verbose: () => [],
    },
};

/**
 *
 * @returns human-readable message with settings
 * @typedef {Object} MessageEntry
 * @property {string} concise
 * @property {string} verbose
 */
export function parseMessage(messageCode) {
    const parts = String(messageCode).split(':'); // we have a contract that message codes with their arguments are separated by a colon
    const code = parts[0];
    const args = parts.slice(1);

    const messageEntry = messageMap[code];
    if (!messageEntry) {
        const supportedCodes = Object.keys(messageMap).join(', ');
        throw `${code} is not supported by messsage map in frontend. Supported message codes: ${supportedCodes}`;
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

