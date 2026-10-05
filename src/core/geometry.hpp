// Core 2D value types and numeric helpers.
//
// This translation unit belongs to the Qt-free core layer: no third-party
// headers may be included here (see docs/DECISIONS.md ADR-001).

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace xm::core {

// All coordinates and lengths in this project are millimetres, stored as
// double. See docs/MVP_DESIGN.md section 7.
struct Point2d {
    double x = 0.0;
    double y = 0.0;

    friend constexpr bool operator==(const Point2d&, const Point2d&) = default;
};

// ---------------------------------------------------------------------------
// Vector helpers
// ---------------------------------------------------------------------------

constexpr double dot(const Point2d& a, const Point2d& b) noexcept
{
    return a.x * b.x + a.y * b.y;
}

// z component of the 3D cross product of two planar vectors.
constexpr double cross(const Point2d& a, const Point2d& b) noexcept
{
    return a.x * b.y - a.y * b.x;
}

inline double magnitude(const Point2d& v) noexcept
{
    return std::hypot(v.x, v.y);
}

inline double distance(const Point2d& a, const Point2d& b) noexcept
{
    return std::hypot(b.x - a.x, b.y - a.y);
}

// ---------------------------------------------------------------------------
// Angle helpers
//
// Angle convention, frozen for the whole project (docs/MVP_DESIGN.md 5.2):
//   * degrees, counter-clockwise positive, 0 degrees pointing along +X;
//   * this matches DXF ARC group codes 50/51 exactly, so no sign flipping is
//     needed on either the writer or the reader side.
// ---------------------------------------------------------------------------

constexpr double toRadians(double degrees) noexcept
{
    return degrees * std::numbers::pi / 180.0;
}

constexpr double toDegrees(double radians) noexcept
{
    return radians * 180.0 / std::numbers::pi;
}

// Normalises an angle in degrees into [0, 360).
//
// std::fmod keeps the sign of its first argument, so fmod(-90, 360) is -90;
// adding one full turn before the second fmod maps that into [0, 360).
inline double normalizeDegrees(double degrees) noexcept
{
    if (!std::isfinite(degrees)) {
        return degrees;
    }
    return std::fmod(std::fmod(degrees, 360.0) + 360.0, 360.0);
}

// ---------------------------------------------------------------------------
// Affine transform
//
// Row-major 2x3 matrix stored as {a, b, c, d, e, f}:
//   x' = a*x + c*y + e
//   y' = b*x + d*y + f
//
// Slice 1 provides construction and point mapping only. Transforming whole
// entities (curves under rotation/scale) and non-identity evaluation belong to
// the CDR import stage, see docs/MVP_DESIGN.md section 6.3.
// ---------------------------------------------------------------------------

class Transform2d {
public:
    constexpr Transform2d() noexcept = default;

    constexpr Transform2d(double a, double b, double c,
                          double d, double e, double f) noexcept
        : values_{a, b, c, d, e, f}
    {
    }

    static constexpr Transform2d identity() noexcept
    {
        return Transform2d{};
    }

    static constexpr Transform2d translation(double dx, double dy) noexcept
    {
        return Transform2d{1.0, 0.0, 0.0, 1.0, dx, dy};
    }

    static constexpr Transform2d scaling(double sx, double sy) noexcept
    {
        return Transform2d{sx, 0.0, 0.0, sy, 0.0, 0.0};
    }

    static Transform2d rotation(double radians) noexcept
    {
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        return Transform2d{c, s, -s, c, 0.0, 0.0};
    }

    Point2d apply(const Point2d& p) const noexcept
    {
        return Point2d{
            values_[0] * p.x + values_[2] * p.y + values_[4],
            values_[1] * p.x + values_[3] * p.y + values_[5],
        };
    }

    // Composition: this transform is applied first, then `next`.
    Transform2d then(const Transform2d& next) const noexcept
    {
        return Transform2d{
            next.values_[0] * values_[0] + next.values_[2] * values_[1],
            next.values_[1] * values_[0] + next.values_[3] * values_[1],
            next.values_[0] * values_[2] + next.values_[2] * values_[3],
            next.values_[1] * values_[2] + next.values_[3] * values_[3],
            next.values_[0] * values_[4] + next.values_[2] * values_[5] + next.values_[4],
            next.values_[1] * values_[4] + next.values_[3] * values_[5] + next.values_[5],
        };
    }

    // A matrix is the identity only when every coefficient is a real finite
    // number equal to its identity value within `tolerance`.
    //
    // The finiteness test is explicit and must not be folded into the
    // comparison: with NaN coefficients, `std::abs(NaN - 1.0) > tolerance` is
    // FALSE for every entry (all comparisons against NaN are false), so the
    // loop would fall through and report a NaN matrix as the identity.
    //
    // A `tolerance` that is negative, NaN or infinite is rejected: a negative
    // bound can never be satisfied, and a NaN or infinite bound would make the
    // predicate meaningless instead of reporting the bad argument.
    bool isIdentity(double tolerance = 1e-12) const noexcept
    {
        if (!std::isfinite(tolerance) || tolerance < 0.0) {
            return false;
        }

        static constexpr std::array<double, 6> kIdentity{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
        for (std::size_t i = 0; i < kIdentity.size(); ++i) {
            if (!std::isfinite(values_[i])) {
                return false;
            }
            if (std::abs(values_[i] - kIdentity[i]) > tolerance) {
                return false;
            }
        }
        return true;
    }

    const std::array<double, 6>& coefficients() const noexcept { return values_; }

private:
    // Default-initialised to the identity matrix.
    std::array<double, 6> values_{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
};

}  // namespace xm::core
