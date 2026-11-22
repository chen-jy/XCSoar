// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class ProfileMap;
struct WaypointRingSettings;

namespace Profile {

/**
 * Loads the list of waypoint rings, up to the maximum number allowed. Rings
 * with a missing location or invalid radius are skipped.
 */
void Load(const ProfileMap &map, WaypointRingSettings &settings);

/**
 * Stores the list of waypoint rings and removes the keys of rings that no
 * longer exist.
 */
void Save(ProfileMap &map, const WaypointRingSettings &settings);

} // namespace Profile
