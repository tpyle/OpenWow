#include "openwow/world/presentation/world_presentation_snapshot.h"

#include "openwow/data/wmo/wmo_file.h"
#include "openwow/foundation/diagnostics/logging.h"
#include "openwow/world/streaming/world_map.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <limits>
#include <span>
#include <vector>

namespace openwow::world {

WorldPresentationSnapshot WorldMap::BuildPresentationSnapshot(
    const CameraSnapshot& camera) const {
  WorldPresentationSnapshot snapshot{};
  snapshot.map_generation = world_staging_generation_;
  snapshot.map_id = map_id_;
  snapshot.minimally_valid = !map_name_.empty() && wdt_loaded_ &&
                             camera.map_generation == world_staging_generation_;
  snapshot.camera = camera;
  snapshot.environment = environment_;
  snapshot.environment.weather = weather_.kind;
  snapshot.environment.weather_density = weather_.density;

  (void)ResolveAreaEnvironmentAtPosition(camera.position[0], camera.position[1],
                                         camera.position[2],
                                         AreaEnvironmentProbe::kCameraRoom);
  const AreaEnvironmentQueryCache* const camera_cache_entry =
      last_area_environment_resolution_.valid ? &last_area_environment_resolution_
                                              : nullptr;

  world::Frustum frustum{};
  for (std::size_t plane = 0; plane < frustum.planes.size(); ++plane) {
    std::copy_n(camera.frustum_planes.begin() + plane * 4u, 4u,
                frustum.planes[plane].begin());
  }
  const Matrix4 view_projection = Multiply(camera.view, camera.projection);
  const float near_clip_radius =
      NearClipCornerRadius(camera.projection, camera.near_clip);

  snapshot.world_models.reserve(wmo_instances_.size());
  std::vector<std::uint8_t> camera_lane_walked;
  camera_lane_walked.reserve(wmo_instances_.size());

  for (const auto& [placement, instance] : wmo_instances_) {

    WorldPresentationItem& item = snapshot.world_models.emplace_back();
    item.stable_id = instance.placement_stable_id;
    item.resource_key = instance.wmo_path;
    item.transform = instance.model_matrix;
    item.visible = instance.visible;
    bool walked = false;
    const auto cached = wmo_cache_.find(instance.wmo_path);
    if (cached != wmo_cache_.end() && camera_cache_entry != nullptr) {
      std::array<std::uint16_t, 1u> seed_storage{};
      std::size_t seed_count = 0u;
      const auto append_seed = [&](const std::optional<WmoAreaGroupRef>& ref) {
        if (!ref.has_value() || ref->placement != placement ||
            ref->group_index > std::numeric_limits<std::uint16_t>::max() ||
            ref->group_index >= cached->second.visibility.group_count()) {
          return;
        }
        const auto group = static_cast<std::uint16_t>(ref->group_index);
        // An exterior group is outdoor space (world_map.cpp treats a camera
        // in one as outdoors for sky and environment too). Seeding the
        // camera lane from it would switch the exterior lane to append mode
        // and suppress every other exterior group of this WMO -- e.g. the
        // Stormwind gate's floor and walls vanished while the camera stood
        // under the portcullis -- so leave such placements to the exterior
        // lane.
        if ((cached->second.visibility.groups()[group].flags &
             data::wmo::kMogpExterior) != 0u) {
          return;
        }
        if (std::find(seed_storage.begin(), seed_storage.begin() + seed_count,
                      group) == seed_storage.begin() + seed_count) {
          seed_storage[seed_count++] = group;
        }
      };
      // Only containing_group represents confirmed containment (the
      // corrected floor hit from CorrectWmoRoomHitThroughPortal). The other
      // three AreaEnvironmentQueryCache fields describe threshold-adjacent
      // rooms for other consumers -- e.g. alternate_group exists so
      // ResolveCameraWmoFog (world_map.cpp) can blend fog as the camera
      // crosses a doorway -- not confirmed containment, so they must not be
      // seeded here with a full-viewport (unclipped) camera-lane rect: doing
      // so rendered the room on the other side of any nearby portal across
      // the whole screen whenever it was in the view frustum, regardless of
      // whether the portal aperture actually admitted it, which is visible
      // as the far room's geometry bleeding through solid walls near a
      // doorway. Any room genuinely visible through a portal from
      // containing_group is still found via normal portal traversal below,
      // with a real aperture-clipped rect instead of an unclipped one.
      append_seed(camera_cache_entry->containing_group);
      if (seed_count != 0u) {
        const std::span<const std::uint16_t> seeds(seed_storage.data(),
                                                   seed_count);
        item.wmo_seed_groups.assign(seeds.begin(), seeds.end());
        ComputeVisibleWmoGroups(
            cached->second.visibility, instance.model_matrix, view_projection,
            instance.placement_world_bounds, instance.group_world_bounds,
            frustum, camera.position[0], camera.position[1],
            camera.position[2], camera.forward, seeds,
            instance.visibility_workspace, item.visible_subresources,
            &item.wmo_visible_group_paths, &item.wmo_sky_visibility,
            &item.wmo_portal_fills, WmoTraversalLanes::kCamera,
            nullptr, false, near_clip_radius);
        walked = true;
      }
    }
    camera_lane_walked.push_back(walked ? 1u : 0u);
  }

  float terrain_aperture_depth = 0.0f;
  {
    bool indoors = false;
    for (std::size_t i = 0; i < snapshot.world_models.size(); ++i) {
      if (camera_lane_walked[i] == 0u) continue;
      indoors = true;
      const WmoSkyVisibility& apertures =
          snapshot.world_models[i].wmo_sky_visibility;
      if (apertures.terrain_visible) {
        terrain_aperture_depth =
            std::max(terrain_aperture_depth, apertures.terrain_depth);
      }
    }
    if (indoors && terrain_aperture_depth == 0.0f) {
      terrain_aperture_depth = -1.0f;
    }
  }
  WorldOccluderVolumes occluders;
  occluders.Rebuild(map_id_, camera.position, camera.forward,
                    terrain_aperture_depth, frustum);

  std::size_t item_index = 0;
  for (const auto& [placement, instance] : wmo_instances_) {
    static_cast<void>(placement);
    WorldPresentationItem& item = snapshot.world_models[item_index];
    const auto cached = wmo_cache_.find(instance.wmo_path);
    if (cached != wmo_cache_.end()) {
      ComputeVisibleWmoGroups(
          cached->second.visibility, instance.model_matrix, view_projection,
          instance.placement_world_bounds, instance.group_world_bounds,
          frustum, camera.position[0], camera.position[1], camera.position[2],
          camera.forward, std::span<const std::uint16_t>(),
          instance.visibility_workspace, item.visible_subresources,
          &item.wmo_visible_group_paths, &item.wmo_sky_visibility,
          &item.wmo_portal_fills, WmoTraversalLanes::kExterior, &occluders,
          camera_lane_walked[item_index] != 0u, near_clip_radius);
    }
    ++item_index;
  }

  // Log the camera room's drawn groups with enough state (camera, view
  // projection, placement matrix) to replay the portal traversal offline.
  if (camera_cache_entry != nullptr &&
      camera_cache_entry->containing_group.has_value()) {
    const WmoAreaGroupRef& room = *camera_cache_entry->containing_group;
    std::size_t index = 0;
    for (const auto& [placement, instance] : wmo_instances_) {
      if (placement != room.placement) {
        ++index;
        continue;
      }
      std::vector<std::uint16_t> groups;
      for (const WmoVisibleGroupPath& path :
           snapshot.world_models[index].wmo_visible_group_paths) {
        groups.push_back(path.group_index);
      }
      std::sort(groups.begin(), groups.end());
      groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
      const auto now = std::chrono::steady_clock::now();
      if (groups != camera_visibility_log_groups_ &&
          now - camera_visibility_log_time_ >= std::chrono::milliseconds(250)) {
        camera_visibility_log_groups_ = groups;
        camera_visibility_log_time_ = now;
        const auto floats = [](const float* values, std::size_t count) {
          std::string out;
          char buffer[32];
          for (std::size_t i = 0; i < count; ++i) {
            std::snprintf(buffer, sizeof(buffer), i == 0 ? "%.6g" : ",%.6g",
                          static_cast<double>(values[i]));
            out += buffer;
          }
          return out;
        };
        std::string message =
            "WMO camera visibility: placement=" +
            std::to_string(instance.placement_stable_id) + " room=" +
            std::to_string(room.group_index) + " camera=(" +
            floats(camera.position.data(), 3u) + ") model=[" +
            floats(instance.model_matrix.data(), 16u) + "] viewproj=[" +
            floats(view_projection.data(), 16u) + "] groups=";
        for (std::size_t i = 0; i < groups.size(); ++i) {
          message += (i == 0 ? "" : ",") + std::to_string(groups[i]);
        }
        diagnostics::Log(diagnostics::LogLevel::kInfo, message);
      }
      break;
    }
  }
  return snapshot;
}

}
