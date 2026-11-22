// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DistanceRingsRenderer.hpp"
#include "TextInBox.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Projection/WindowProjection.hpp"
#include "Look/MapLook.hpp"
#include "Screen/Layout.hpp"
#include "Formatter/UserUnits.hpp"
#include "Units/Units.hpp"
#include "util/StaticString.hxx"

#include <cmath>

// Ring radius presets in meters

// Kilometers: round values in km
static constexpr double RINGS_KM_VERY_NARROW[] = {
  1000, 2000, 3000, 4000, 5000, 7500, 10000,
};
static constexpr double RINGS_KM_NARROW[] = {
  2500, 5000, 7500, 10000, 15000, 20000, 25000, 30000, 40000, 50000, 100000,
};
static constexpr double RINGS_KM_MEDIUM[] = {
  5000, 10000, 15000, 20000, 25000, 30000, 40000, 50000, 100000,
};
static constexpr double RINGS_KM_WIDE[] = {
  10000, 20000, 30000, 40000, 50000, 75000, 100000,
};
static constexpr double RINGS_KM_VERY_WIDE[] = {
  50000, 100000,
};

// Statute miles: round values in mi (1 mi = 1609.344 m)
static constexpr double SM_TO_M = 1609.344;
static constexpr double RINGS_SM_VERY_NARROW[] = {
  1*SM_TO_M, 2*SM_TO_M, 3*SM_TO_M, 5*SM_TO_M,
};
static constexpr double RINGS_SM_NARROW[] = {
  2*SM_TO_M, 4*SM_TO_M, 6*SM_TO_M, 8*SM_TO_M, 10*SM_TO_M, 15*SM_TO_M,
};
static constexpr double RINGS_SM_MEDIUM[] = {
  5*SM_TO_M, 10*SM_TO_M, 15*SM_TO_M, 20*SM_TO_M, 25*SM_TO_M, 30*SM_TO_M,
};
static constexpr double RINGS_SM_WIDE[] = {
  10*SM_TO_M, 20*SM_TO_M, 30*SM_TO_M, 40*SM_TO_M, 50*SM_TO_M, 75*SM_TO_M,
};
static constexpr double RINGS_SM_VERY_WIDE[] = {
  50*SM_TO_M, 100*SM_TO_M,
};

// Nautical miles: round values in NM (1 NM = 1852 m)
static constexpr double NM_TO_M = 1852.0;
static constexpr double RINGS_NM_VERY_NARROW[] = {
  1*NM_TO_M, 2*NM_TO_M, 3*NM_TO_M, 4*NM_TO_M,
};
static constexpr double RINGS_NM_NARROW[] = {
  2*NM_TO_M, 4*NM_TO_M, 6*NM_TO_M, 8*NM_TO_M, 10*NM_TO_M, 12*NM_TO_M,
};
static constexpr double RINGS_NM_MEDIUM[] = {
  5*NM_TO_M, 10*NM_TO_M, 15*NM_TO_M, 20*NM_TO_M, 25*NM_TO_M,
};
static constexpr double RINGS_NM_WIDE[] = {
  10*NM_TO_M, 20*NM_TO_M, 30*NM_TO_M, 40*NM_TO_M, 50*NM_TO_M, 75*NM_TO_M,
};
static constexpr double RINGS_NM_VERY_WIDE[] = {
  50*NM_TO_M, 100*NM_TO_M,
};

static constexpr unsigned MIN_RING_RADIUS_PX = 20;
static constexpr unsigned MAX_RING_RADIUS_PX = 100000;

static std::span<const double>
SelectRingSet(double screen_distance_m) noexcept
{
  switch (Units::GetUserDistanceUnit()) {
  case Unit::STATUTE_MILES:
    if (screen_distance_m <= 8000.0)
      return RINGS_SM_VERY_NARROW;
    if (screen_distance_m <= 25000.0)
      return RINGS_SM_NARROW;
    if (screen_distance_m <= 50000.0)
      return RINGS_SM_MEDIUM;
    if (screen_distance_m <= 150000.0)
      return RINGS_SM_WIDE;
    return RINGS_SM_VERY_WIDE;

  case Unit::NAUTICAL_MILES:
    if (screen_distance_m <= 8000.0)
      return RINGS_NM_VERY_NARROW;
    if (screen_distance_m <= 25000.0)
      return RINGS_NM_NARROW;
    if (screen_distance_m <= 50000.0)
      return RINGS_NM_MEDIUM;
    if (screen_distance_m <= 150000.0)
      return RINGS_NM_WIDE;
    return RINGS_NM_VERY_WIDE;

  default: // KILOMETER and any other unit
    if (screen_distance_m <= 8000.0)
      return RINGS_KM_VERY_NARROW;
    if (screen_distance_m <= 25000.0)
      return RINGS_KM_NARROW;
    if (screen_distance_m <= 50000.0)
      return RINGS_KM_MEDIUM;
    if (screen_distance_m <= 150000.0)
      return RINGS_KM_WIDE;
    return RINGS_KM_VERY_WIDE;
  }
}

void
FormatRingDistance(double radius, char *buffer) noexcept
{
  double user_val = Units::ToUserUnit(radius, Units::GetUserDistanceUnit());
  FormatUserDistance(radius, buffer, true,
                     (user_val - static_cast<int>(user_val)) < 0.01 ? 0 : 1);
}

static void
DrawRingLabel(Canvas &canvas, const PixelRect &screen_rect, PixelPoint anchor,
              const char *text) noexcept
{
  PixelSize text_size = canvas.CalcTextSize(text);
  PixelPoint label_pos{
    anchor.x - static_cast<int>(text_size.width) / 2,
    anchor.y - static_cast<int>(text_size.height) / 2,
  };

  // Only draw a label if it's within screen bounds
  if (label_pos.x >= screen_rect.left &&
      label_pos.x + static_cast<int>(text_size.width) <= screen_rect.right &&
      label_pos.y >= screen_rect.top &&
      label_pos.y + static_cast<int>(text_size.height) <= screen_rect.bottom)
    RenderShadowedText(canvas, text, label_pos, false);
}

void
DrawDistanceRings(Canvas &canvas,
                  const WindowProjection &projection,
                  const GeoPoint &aircraft_pos,
                  const MapLook &look) noexcept
{
  if (!projection.IsValid() || !aircraft_pos.IsValid())
    return;

  /* 8 * GetMapScale() is the short-edge span; GetScreenDistanceMeters()
     follows the long edge and changes with InfoBox layout aspect. */
  const auto rings = SelectRingSet(8 * projection.GetMapScale());
  const PixelPoint aircraft_px = projection.GeoToScreen(aircraft_pos);
  const PixelRect screen_rect = projection.GetScreenRect();

  canvas.Select(look.distance_rings_pen);
  canvas.SelectHollowBrush();

  const Font &font = *look.overlay.overlay_font;
  canvas.Select(font);

  for (double radius_m : rings) {
    const unsigned radius_px = projection.GeoToScreenDistance(radius_m);

    if (radius_px < Layout::Scale(MIN_RING_RADIUS_PX))
      continue;

    canvas.DrawCircle(aircraft_px, radius_px);

    // Label at topmost point of the ring on screen
    char buf[32];
    FormatRingDistance(radius_m, buf);
    DrawRingLabel(canvas, screen_rect,
                  {aircraft_px.x, aircraft_px.y - (int)radius_px}, buf);
  }
}

void
DrawWaypointRings(Canvas &canvas, const WindowProjection &projection,
                  std::span<const WaypointRing> rings,
                  const MapLook &look) noexcept
{
  if (!projection.IsValid() || rings.empty())
    return;

  canvas.Select(look.waypoint_rings_pen);
  canvas.SelectHollowBrush();
  canvas.Select(*look.overlay.overlay_font);

  PixelRect screen_rect = projection.GetScreenRect();
  PixelPoint screen_center = screen_rect.GetCenter();
  unsigned min_radius_px = Layout::Scale(MIN_RING_RADIUS_PX);

  for (const WaypointRing &ring : rings) {
    if (!ring.location.IsValid())
      continue;

    unsigned radius_px = projection.GeoToScreenDistance(ring.radius);
    if (radius_px < min_radius_px || radius_px > MAX_RING_RADIUS_PX)
      continue;

    PixelPoint center_px = projection.GeoToScreen(ring.location);
    if (!CircleOutlineIntersectsRect(center_px, radius_px, screen_rect))
      continue;

    canvas.DrawCircle(center_px, radius_px);

    // Put the label on the point of the ring that is closest to the center of
    // the screen
    double dx = screen_center.x - center_px.x;
    double dy = screen_center.y - center_px.y;
    double distance = std::hypot(dx, dy);

    PixelPoint anchor = distance >= 1
      ? PixelPoint{center_px.x + static_cast<int>(dx / distance * radius_px),
                   center_px.y + static_cast<int>(dy / distance * radius_px)}
      : PixelPoint{center_px.x, center_px.y - static_cast<int>(radius_px)};

    char distance_text[32];
    FormatRingDistance(ring.radius, distance_text);

    StaticString<80> text;
    if (ring.name[0] != '\0')
      text.Format("%s %s", ring.name, distance_text);
    else
      text = distance_text;

    DrawRingLabel(canvas, screen_rect, anchor, text);
  }
}
