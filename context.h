#pragma once

#include <cstdint>

#include "random.h"

class TGameCtx {
   public:
    TRng Rng{std::random_device{}()};

    TGameCtx() = default;
    explicit TGameCtx(uint64_t seed) : Rng(seed) {}
};
