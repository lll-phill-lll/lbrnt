#pragma once

#include <cstdint>

struct TPos {
    int32_t X, Y;
};

inline bool operator==(const TPos& a, const TPos& b) {
    return a.X == b.X && a.Y == b.Y;
}