// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Geo/GeoPoint.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Map.hpp"
#include "Profile/WaypointRingProfile.hpp"
#include "Renderer/DistanceRingsRenderer.hpp"
#include "Renderer/WaypointRingSettings.hpp"
#include "TestUtil.hpp"
#include "util/StringAPI.hxx"

#include <cmath>
#include <cstring>
#include <limits>

static constexpr GeoPoint location{Angle::Degrees(8.25), Angle::Degrees(49.5)};

static void
TestList()
{
  WaypointRingSettings s;
  s.SetDefaults();
  ok1(!s.enabled);
  ok1(s.count == 0);
  ok1(s.IsEmpty());
  ok1(!s.IsFull());
  ok1(s.GetRings().empty());

  // Invalid rings
  ok1(!s.Add(GeoPoint::Invalid(), 1000, "x"));
  ok1(!s.Add(location, 0, "x"));
  ok1(!s.Add(location, -5, "x"));
  ok1(!s.Add(location, std::numeric_limits<double>::quiet_NaN(), "x"));
  ok1(!s.Add(location, std::numeric_limits<double>::infinity(), "x"));
  ok1(s.count == 0);

  // Fill list
  bool all_added = true;
  for (unsigned i = 0; i < WaypointRingSettings::MAX_RINGS; ++i)
    all_added &= s.Add(location, 1000 * (i + 1), "A");
  ok1(all_added);
  ok1(s.IsFull());
  ok1(s.count == WaypointRingSettings::MAX_RINGS);

  // At limit
  ok1(!s.Add(location, 1, "x"));
  ok1(s.GetRings().size() == WaypointRingSettings::MAX_RINGS);

  // Index out of bounds
  s.Remove(WaypointRingSettings::MAX_RINGS);
  ok1(s.count == WaypointRingSettings::MAX_RINGS);

  // Ring indices shifted after intermediate ring is removed
  s.Remove(1);
  ok1(s.count == WaypointRingSettings::MAX_RINGS - 1);
  ok1(equals(s.rings[0].radius, 1000));
  ok1(equals(s.rings[1].radius, 3000));
  ok1(equals(s.rings[s.count - 1].radius,
             double(1000 * WaypointRingSettings::MAX_RINGS)));
}

static void
TestName()
{
  WaypointRing ring;

  // Drop invalid characters
  ring.SetName("a\"b\nc\rd");
  ok1(StringIsEqual(ring.name, "abcd"));
  ring.SetName(nullptr);
  ok1(StringIsEqual(ring.name, ""));

  // Name size
  char long_name[81];
  for (unsigned i = 0; i < 40; i++) {
    long_name[2 * i] = static_cast<char>(0xc3);
    long_name[2 * i + 1] = static_cast<char>(0xa4);
  }
  long_name[80] = '\0';

  ring.SetName(long_name);
  const std::size_t length = std::strlen(ring.name);
  ok1(length == 30);
  ok1(std::memcmp(ring.name, long_name, length) == 0);
}

static void
TestProfile()
{
  ProfileMap map;

  WaypointRingSettings in;
  in.SetDefaults();
  in.enabled = true;
  in.Add(location, 12345.678, "Alpha");
  in.Add(location, 5000, "");
  in.Add(GeoPoint(Angle::Degrees(-1.5), Angle::Degrees(-33.25)), 18520,
         "Beta");
  Profile::Save(map, in);

  WaypointRingSettings out;
  out.SetDefaults();
  Profile::Load(map, out);
  ok1(out.enabled);
  ok1(out.count == 3);

  ok1(StringIsEqual(out.rings[0].name, "Alpha"));
  ok1(equals(out.rings[0].radius, 12345.678));
  ok1(std::fabs(out.rings[0].location.longitude.Degrees() - 8.25) < 1e-5);
  ok1(std::fabs(out.rings[0].location.latitude.Degrees() - 49.5) < 1e-5);

  ok1(StringIsEqual(out.rings[1].name, ""));

  ok1(StringIsEqual(out.rings[2].name, "Beta"));
  ok1(equals(out.rings[2].radius, 18520));
  ok1(std::fabs(out.rings[2].location.longitude.Degrees() + 1.5) < 1e-5);
  ok1(std::fabs(out.rings[2].location.latitude.Degrees() + 33.25) < 1e-5);

  // Keys of removed rings are removed
  in.Remove(2);
  in.Remove(1);
  Profile::Save(map, in);
  ok1(map.Exists("WaypointRing0Location"));
  ok1(!map.Exists("WaypointRing1Location"));
  ok1(!map.Exists("WaypointRing2Radius"));
  ok1(!map.Exists("WaypointRing2Name"));

  out.SetDefaults();
  Profile::Load(map, out);
  ok1(out.count == 1);

  // Empty profile has default values
  {
    const ProfileMap empty;
    out.enabled = true;
    out.count = 5;
    out.SetDefaults();
    Profile::Load(empty, out);
    ok1(!out.enabled);
    ok1(out.count == 0);
  }

  // Count is clamped and invalid entries are skipped
  {
    ProfileMap bad;
    bad.Set(ProfileKeys::WaypointRingCount, 100u);

    bad.SetGeoPoint("WaypointRing0Location", location);
    bad.Set("WaypointRing0Radius", 1000.);

    bad.SetGeoPoint("WaypointRing1Location", location);
    bad.Set("WaypointRing2Radius", 1000.);

    bad.SetGeoPoint("WaypointRing3Location", location);
    bad.Set("WaypointRing3Radius", -1.);
    bad.SetGeoPoint("WaypointRing4Location", location);
    bad.Set("WaypointRing4Radius", 0.);
    bad.SetGeoPoint("WaypointRing5Location", location);
    bad.Set("WaypointRing5Radius", 2000.);
    bad.Set("WaypointRing5Name", "Five");

    bad.SetGeoPoint("WaypointRing20Location", location);
    bad.Set("WaypointRing20Radius", 3000.);

    out.SetDefaults();
    Profile::Load(bad, out);
    ok1(out.count == 2);
    ok1(equals(out.rings[0].radius, 1000));
    ok1(equals(out.rings[1].radius, 2000));
    ok1(StringIsEqual(out.rings[1].name, "Five"));
  }
}

static void
TestCulling()
{
  static constexpr PixelRect screen{0, 0, 800, 600};
  static_assert(!CircleOutlineIntersectsRect({400, 300}, 1000, screen));

  ok1(CircleOutlineIntersectsRect({400, 300}, 100, screen));

  ok1(CircleOutlineIntersectsRect({400, 300}, 500, screen));
  ok1(!CircleOutlineIntersectsRect({400, 300}, 501, screen));

  ok1(!CircleOutlineIntersectsRect({-500, 300}, 100, screen));

  ok1(CircleOutlineIntersectsRect({-500, 300}, 600, screen));
  ok1(CircleOutlineIntersectsRect({1000, 300}, 250, screen));

  ok1(!CircleOutlineIntersectsRect({-300, -400}, 499, screen));
  ok1(CircleOutlineIntersectsRect({-300, -400}, 500, screen));

  ok1(!CircleOutlineIntersectsRect({-5000000, 300}, 4999000, screen));
  ok1(CircleOutlineIntersectsRect({-5000000, 300}, 5000100, screen));
}

int
main()
{
  plan_tests(21 + 4 + 22 + 10);

  TestList();
  TestName();
  TestProfile();
  TestCulling();

  return exit_status();
}
