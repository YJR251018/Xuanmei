// Geometry entity shapes and their closed-form lengths.
//
// Lengths are computed in closed form, never by discretisation, so they carry
// no sampling error. All results are millimetres.
//
// This translation unit belongs to the Qt-free core layer (ADR-001).

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/geometry.hpp"

namespace xm::core {

struct Line {
    Point2d from;
    Point2d to;
};

struct Circle {
    Point2d center;
    double radius = 0.0;
};

// A circular arc from startDegrees to endDegrees, counter-clockwise.
//
// The sweep is normalizeDegrees(endDegrees - startDegrees), which lies in
// [0, 360). A full circle MUST be represented by Circle, never by an Arc with
// startDegrees == endDegrees: that form is indistinguishable from a zero-length
// arc and would silently produce an empty entity on export.
struct Arc {
    Point2d center;
    double radius = 0.0;
    double startDegrees = 0.0;
    double endDegrees = 0.0;
};

struct Polyline {
    std::vector<Point2d> points;
    bool closed = false;
};

using Shape = std::variant<Line, Circle, Arc, Polyline>;

// A shape together with its document-level identity and provenance.
//
// `id` is assigned by GeometryDocument::add() and increases from 1; `layer`
// defaults to "0"; `transform` defaults to the identity and records the
// original object transform on import (evaluation of non-identity transforms
// belongs to the CDR import stage, see docs/MVP_DESIGN.md section 6.3).
struct GeometryEntity {
    std::uint64_t   id = 0;          // 文档内原始对象编号
    std::string     layer = "0";
    Shape           shape;
    Transform2d     transform;       // 默认恒等；导入时记录原始变换
    std::vector<std::string> warnings;  // 该实体的转换警告
};

// ---------------------------------------------------------------------------
// Closed-form lengths
//
// A negative radius cannot occur in a well-formed document; it is treated as a
// non-positive radius and yields length 0 rather than a negative length, so a
// malformed file cannot silently cancel out other entities in a total.
// ---------------------------------------------------------------------------

double length(const Line& line) noexcept;
double length(const Circle& circle) noexcept;
double length(const Arc& arc) noexcept;
double length(const Polyline& polyline) noexcept;

// Dispatches to the overload above for the active alternative.
double length(const Shape& shape) noexcept;

// The arc's counter-clockwise sweep in degrees, in [0, 360).
inline double arcSweepDegrees(const Arc& arc) noexcept
{
    return normalizeDegrees(arc.endDegrees - arc.startDegrees);
}

// DXF entity name for the active alternative.
//   Line -> "LINE", Circle -> "CIRCLE", Arc -> "ARC", Polyline -> "LWPOLYLINE"
// Returns a pointer to a string literal; never null.
const char* kindName(const Shape& shape) noexcept;

}  // namespace xm::core
