#pragma once
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <utility>

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

/**
 * Равномерное целое в [0, span) только из вызовов g() и деления — без
 * std::uniform_int_distribution (у него потребление бит от URBG зависит от реализации
 * libstdc++/libc++, из‑за чего один seed давал разные карты на Linux и macOS).
 */
template <typename UniformRandomBitGenerator>
inline uint32_t uniform_u32_below(UniformRandomBitGenerator& g, uint32_t span) {
	if (span <= 1u) return 0u;
	const uint32_t limit = (0xFFFFFFFFu / span) * span;
	uint32_t r;
	do {
		r = static_cast<uint32_t>(g());
	} while (r >= limit);
	return r % span;
}

/**
 * Fisher–Yates shuffle. Переносимо между libstdc++ и libc++ — в отличие от std::shuffle,
 * у которого алгоритм зависит от реализации стандартной библиотеки.
 */
template <typename RandomIt, typename UniformRandomBitGenerator>
void shuffle_portable(RandomIt first, RandomIt last, UniformRandomBitGenerator& g) {
	using diff_t = typename std::iterator_traits<RandomIt>::difference_type;
	diff_t n = last - first;
	for (diff_t i = n - 1; i > 0; --i) {
		const uint32_t span = static_cast<uint32_t>(i) + 1u;
		const uint32_t ju = uniform_u32_below(g, span);
		const diff_t j = static_cast<diff_t>(ju);
		using std::swap;
		swap(first[j], first[i]);
	}
}

} // namespace game_rng
