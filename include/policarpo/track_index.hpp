#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace policarpo {

struct TrackMeta {
  std::string title;
  std::chrono::milliseconds duration{0};
};

// call once at startup
void track_cache_init();

// lookup/update
std::optional<TrackMeta> track_cache_get(const std::string& id);
void track_cache_upsert(const std::string& id, const TrackMeta& meta);

class TrackIndex {
public:
  explicit TrackIndex(std::filesystem::path json_path);

  // Load from disk once at startup
  void load();

  // Persist current map to disk (atomic write)
  void save();

  // Lookup by id
  std::optional<TrackMeta> get(const std::string& id) const;

  // Insert/update
  void upsert(const std::string& id, TrackMeta meta);

private:
  std::filesystem::path m_path;
  mutable std::mutex m_mu;
  std::unordered_map<std::string, TrackMeta> m_by_id;
};

} // namespace policarpo