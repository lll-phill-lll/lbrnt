#pragma once

#include <vector>

#include "../common.h"
#include "../random.h"

using TShape = std::vector<TPos>;

inline std::vector<TShape> DefaultShapes = {
    // 2-cells
    {{0, 0}, {1, 0}},
    {{0, 0}, {0, 1}},
    // 3-cells lines
    {{0, 0}, {1, 0}, {2, 0}},
    {{0, 0}, {0, 1}, {0, 2}},
    // 3-cells L
    {{0, 0}, {1, 0}, {0, 1}},
    {{0, 0}, {-1, 0}, {0, 1}},
    {{0, 0}, {1, 0}, {0, -1}},
    {{0, 0}, {-1, 0}, {0, -1}},
    // 4-cells square
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
    // 4-cells line
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}},
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
    // 4-cells T (upright)
    {{0, 0}, {-1, 0}, {1, 0}, {0, 1}},
    {{0, 0}, {-1, 0}, {1, 0}, {0, -1}},
};

inline const TShape& PickRandomShape(TRng& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, DefaultShapes.size() - 1);
    return DefaultShapes[dist(rng)];
}
