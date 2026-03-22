#pragma once
#include <cstddef>
#include <cstdint>

namespace game_rng {

/** SplitMix64 — детерминированное смешивание 64-битного значения. */
inline uint64_t splitmix64(uint64_t x) {
	x += 0x9e3779b97f4a7c15ull;
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
	x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
	return x ^ (x >> 31);
}

/** Начальное состояние PRNG очереди ходов от seed генерации карты. */
inline uint64_t initial_turn_rng_state(unsigned int seed) {
	return splitmix64(static_cast<uint64_t>(seed) ^ 0xC0FFEEA50D5EED5ull);
}

/** Потоковый шаг: обновляет state, возвращает следующее 64-битное значение. */
inline uint64_t next_u64(uint64_t& state) {
	state += 0x9e3779b97f4a7c15ull;
	return splitmix64(state);
}

/** Равномерно в [0, maxExclusive). Требуется maxExclusive > 0. */
inline size_t uniform_exclusive(uint64_t& state, size_t maxExclusive) {
	return static_cast<size_t>(next_u64(state) % static_cast<uint64_t>(maxExclusive));
}

} // namespace game_rng
