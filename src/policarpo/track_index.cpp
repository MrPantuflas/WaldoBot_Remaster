#include "policarpo/track_index.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace policarpo {

namespace {
  TrackIndex g_index{"songs/index.json"};
  std::once_flag g_once;
}

void track_cache_init() {
  std::call_once(g_once, [] { g_index.load(); });
}

std::optional<TrackMeta> track_cache_get(const std::string& id) {
  track_cache_init();
  return g_index.get(id);
}

void track_cache_upsert(const std::string& id, const TrackMeta& meta) {
  track_cache_init();
  g_index.upsert(id, meta);
}

TrackIndex::TrackIndex(std::filesystem::path json_path)
  : m_path(std::move(json_path)) {}

void TrackIndex::load() {
  std::lock_guard lk(m_mu);
  m_by_id.clear();

  if (!std::filesystem::exists(m_path)) return;

  std::ifstream in(m_path);
  if (!in) return;

  nlohmann::json j;
  try {
    in >> j;
  } catch (...) {
    return; // corrupt index? ignore for now
  }

  if (!j.is_object()) return;

  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string id = it.key();
    const auto& val = it.value();
    if (!val.is_object()) continue;

    TrackMeta meta;
    if (val.contains("title") && val["title"].is_string())
      meta.title = val["title"].get<std::string>();
    if (val.contains("duration_ms") && val["duration_ms"].is_number_integer())
      meta.duration = std::chrono::milliseconds(val["duration_ms"].get<long long>());

    if (!meta.title.empty())
      m_by_id.emplace(id, std::move(meta));
  }
}

void TrackIndex::save() {
  std::lock_guard lk(m_mu);

  nlohmann::json j = nlohmann::json::object();
  for (const auto& [id, meta] : m_by_id) {
    j[id] = {
      {"title", meta.title},
      {"duration_ms", meta.duration.count()}
    };
  }

  std::filesystem::create_directories(m_path.parent_path());

  // Atomic-ish write: write temp then rename
  auto tmp = m_path;
  tmp += ".tmp";

  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return;
    out << j.dump(2);
  }

  std::error_code ec;
  std::filesystem::rename(tmp, m_path, ec);
  if (ec) {
    // Windows rename behavior can be picky; fall back:
    std::filesystem::remove(m_path, ec);
    std::filesystem::rename(tmp, m_path, ec);
  }
}

std::optional<TrackMeta> TrackIndex::get(const std::string& id) const {
  std::lock_guard lk(m_mu);
  auto it = m_by_id.find(id);
  if (it == m_by_id.end()) return std::nullopt;
  return it->second;
}

void TrackIndex::upsert(const std::string& id, TrackMeta meta) {
  {
    std::lock_guard lk(m_mu);
    m_by_id[id] = std::move(meta);
  }
  save(); // simple + safe; you can batch later if you want
}

} // namespace policarpo
