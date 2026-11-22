// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointRingProfile.hpp"

#include "Keys.hpp"
#include "Map.hpp"
#include "Renderer/WaypointRingSettings.hpp"
#include "util/StaticString.hxx"

#include <algorithm>

static StaticString<48>
MakeRingKey(unsigned i, const char *suffix) noexcept
{
  StaticString<48> key;
  key.Format("WaypointRing%u%s", i, suffix);
  return key;
}

void
Profile::Load(const ProfileMap &map, WaypointRingSettings &settings)
{
  map.Get(ProfileKeys::WaypointRingsEnabled, settings.enabled);
  settings.count = 0;

  unsigned count = 0;
  if (!map.Get(ProfileKeys::WaypointRingCount, count))
    return;
  count = std::min(count, WaypointRingSettings::MAX_RINGS);

  for (unsigned i = 0; i < count; i++) {
    GeoPoint location;
    double radius;

    if (!map.GetGeoPoint(MakeRingKey(i, "Location"), location) ||
        !map.Get(MakeRingKey(i, "Radius"), radius))
      continue;

    const char *name = map.Get(MakeRingKey(i, "Name"));
    settings.Add(location, radius, name);
  }
}

void
Profile::Save(ProfileMap &map, const WaypointRingSettings &settings)
{
  map.Set(ProfileKeys::WaypointRingsEnabled, settings.enabled);
  map.Set(ProfileKeys::WaypointRingCount, settings.count);

  for (unsigned i = 0; i < WaypointRingSettings::MAX_RINGS; i++) {
    auto location_key = MakeRingKey(i, "Location");
    auto radius_key = MakeRingKey(i, "Radius");
    auto name_key = MakeRingKey(i, "Name");

    if (i < settings.count) {
      const WaypointRing &ring = settings.rings[i];
      map.SetGeoPoint(location_key, ring.location);
      map.Set(radius_key, ring.radius);
      map.Set(name_key, ring.name);
    } else {
      map.Remove(location_key);
      map.Remove(radius_key);
      map.Remove(name_key);
    }
  }
}
