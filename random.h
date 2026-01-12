#pragma once
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>

class TRng {
   public:
    using result_type = uint64_t;

    TRng() : Seed(std::random_device()()), Counter(0) {}
    explicit TRng(uint64_t seed, uint64_t counter = 0) : Seed(seed), Counter(counter) {
        if (!Seed) Seed = std::random_device()();
    }
    TRng(std::istream& in) { FromStream(in); }

    static std::string_view GetMagic() { return Magic; }

    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    result_type operator()() {
        uint64_t x = Seed + Counter * 0x9e3779b97f4a7c15ull;
        ++Counter;
        return SplitMix64(x);
    }

    void ToStream(std::ostream& out) const {
        out << Seed << " " << Counter << std::endl;
    }

    void FromStream(std::istream& in) {
        if (!(in >> Seed >> Counter)) {
            std::cerr << "Failed to read header" << std::endl;
            std::exit(1);
        }
    }

   private:
    uint64_t Seed = 0;
    uint64_t Counter = 0;

    static constexpr std::string_view Magic = "[RNG]";

    // https://rosettacode.org/wiki/Pseudo-random_numbers/Splitmix64
    uint64_t SplitMix64(uint64_t z) {
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
};
