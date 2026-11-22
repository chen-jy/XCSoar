// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Math/Util.hpp"
#include "Renderer/WaypointRingSettings.hpp"
#include "ui/dim/Rect.hpp"

#include <algorithm>
#include <span>

class Canvas;
class WindowProjection;
struct GeoPoint;
struct MapLook;

/**
 * Formats the given radius in the user's distance unit. The output buffer must
 * be at least 32 bytes.
 */
void
FormatRingDistance(double radius, char *buffer) noexcept;

void
DrawDistanceRings(Canvas &canvas,
                  const WindowProjection &projection,
                  const GeoPoint &aircraft_pos,
                  const MapLook &look) noexcept;

/**
 * Draws the given waypoint rings.
 */
void
DrawWaypointRings(Canvas &canvas, const WindowProjection &projection,
                  std::span<const WaypointRing> rings,
                  const MapLook &look) noexcept;

/**
 * Determines if the outline of a circle will be visible within the given
 * rectangle.
 */
constexpr bool
CircleOutlineIntersectsRect(PixelPoint center, double radius_px,
                            PixelRect rect) noexcept
{
  double left = static_cast<double>(rect.left) - center.x;
  double right = static_cast<double>(rect.right) - center.x;
  double top = static_cast<double>(rect.top) - center.y;
  double bottom = static_cast<double>(rect.bottom) - center.y;

  // Calculate the distance to the nearest point of the rectangle
  double near_dx = left > 0 ? left : (right < 0 ? -right : 0);
  double near_dy = top > 0 ? top : (bottom < 0 ? -bottom : 0);

  // Calculate the squared distance to the furthest corner. This avoids fabs(),
  // which is not constexpr in clang
  double far_dx2 = std::max(Square(left), Square(right));
  double far_dy2 = std::max(Square(top), Square(bottom));

  double r2 = radius_px * radius_px;
  return (near_dx * near_dx + near_dy * near_dy <= r2 &&
          far_dx2 + far_dy2 >= r2);
}
