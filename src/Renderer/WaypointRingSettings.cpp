// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointRingSettings.hpp"

#include "util/TruncateString.hpp"

#include <algorithm>
#include <cmath>

void
WaypointRing::SetName(const char *src) noexcept
{
  if (!src)
    src = "";

  CopyTruncateString(name, sizeof(name), src);

  // KeyValueFileWriter stores an empty value when the text contains a
  // quotation mark or line break
  char *out = name;
  for (const char *in = name; *in != '\0'; ++in) {
    if (*in != '"' && *in != '\n' && *in != '\r')
      *out++ = *in;
  }

  *out = '\0';
}

bool
WaypointRingSettings::Add(const GeoPoint &location, double radius,
                          const char *name) noexcept
{
  if (IsFull() || !location.IsValid() || !std::isfinite(radius) || radius <= 0)
    return false;

  WaypointRing &ring = rings[count++];
  ring.SetName(name);
  ring.location = location;
  ring.radius = radius;

  return true;
}

void
WaypointRingSettings::Remove(unsigned i) noexcept
{
  if (i >= count)
    return;
  std::copy(rings + i + 1, rings + count, rings + i);
  count--;
}
