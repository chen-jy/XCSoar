// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"

#include <cstddef>
#include <span>
#include <type_traits>

/**
 * A distance ring drawn around a waypoint. Its location is copied from that
 * waypoint when it is created and does not automatically move if the waypoint
 * is moved.
 */
struct WaypointRing {
  // Names are truncated and stripped of invalid characters
  void SetName(const char *src) noexcept;

  static constexpr std::size_t NAME_SIZE = 32;
  char name[NAME_SIZE];
  GeoPoint location;
  double radius;
};

static_assert(std::is_trivial_v<WaypointRing>, "type is not trivial");

/**
 * The list of waypoint rings, stored in MapSettings and copied to the draw
 * thread. Its capacity is fixed to keep MapSettings trivial and to bound the
 * rendering cost.
 */
struct WaypointRingSettings {
  void SetDefaults() noexcept {
    enabled = false;
    count = 0;
  }

  bool IsEmpty() const noexcept { return count == 0; }
  bool IsFull() const noexcept { return count >= MAX_RINGS; }

  std::span<const WaypointRing> GetRings() const noexcept {
    return {rings, count};
  }

  bool Add(const GeoPoint &location, double radius, const char *name) noexcept;
  void Remove(unsigned i) noexcept;

  bool enabled;
  unsigned count;
  static constexpr unsigned MAX_RINGS = 12;
  WaypointRing rings[MAX_RINGS];
};

static_assert(std::is_trivial_v<WaypointRingSettings>, "type is not trivial");
