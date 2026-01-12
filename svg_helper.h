#pragma once

#include <format>

#include "common.h"

namespace SVG {

struct TSvgCfg {
    int Cell = 24;
    int Margin = 10;
    int Stroke = 2;
};

struct TRect {
    int x0, y0, x1, y1;

    int GetW() const { return x1 - x0; }
    int GetH() const { return y1 - y0; }
    int GetCX() const { return (x0 + x1) / 2; }
    int GetCY() const { return (y0 + y1) / 2; }
};

inline TRect ToRect(TPos pos, int32_t marginpx, int32_t cellpx) {
    int x0 = marginpx + pos.X * cellpx;
    int y0 = marginpx + pos.Y * cellpx;
    return {x0, y0, x0 + cellpx, y0 + cellpx};
}

inline std::string GetHeader(int w, int h) {
    return std::format(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"{}\" height=\"{}\" viewBox=\"0 0 {} {}\">\n",
        w, h, w, h);
}

inline std::string GetFooter() { return "</svg>\n"; }

inline std::string GroupBegin(std::string_view id = "", std::string_view extra = "") {
    if (!id.empty()) {
        return std::format("<g id=\"{}\" {}>\n", id, extra);
    } else {
        return std::format("<g {}>\n", extra);
    }
}

inline std::string GroupEnd() { return "</g>\n"; }

inline std::string DrawLine(int x0, int y0, int x1, int y1) {
    return std::format("<line x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\"/>\n", x0, y0, x1, y1);
}

inline std::string DrawRect(const TRect& r, std::string_view fill, std::string_view stroke = "",
                            std::string_view extra = "") {
    if (stroke.empty()) {
        return std::format("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" fill=\"{}\" {}/>\n", r.x0, r.y0,
                           r.GetW(), r.GetH(), fill, extra);
    }
    return std::format("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" fill=\"{}\" stroke=\"{}\" {}/>\n", r.x0,
                       r.y0, r.GetW(), r.GetH(), fill, stroke, extra);
}

inline std::string svgCircle(int cx, int cy, int r, std::string_view fill, std::string_view stroke = "black") {
    return std::format("<circle cx=\"{}\" cy=\"{}\" r=\"{}\" fill=\"{}\" stroke=\"{}\"/>\n", cx, cy, r, fill, stroke);
}

}  // namespace SVG
