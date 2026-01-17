#include "policarpo/manager.hpp"
#include "policarpo/player.hpp"
#include "policarpo/voice_session.hpp"
#include "policarpo/song_manager.hpp"
#include "policarpo/track_index.hpp"
#include <chrono>
#include <filesystem>
#include <array>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <optional>
#include <sys/stat.h>
#include "yt-search/encode.h"
#include "yt-search/yt-playlist.h"
#include "yt-search/yt-search.h"
#include "yt-search/yt-track-info.h"
#include "nlohmann/json.hpp"

//static policarpo::TrackIndex g_index{"songs/index.json"};

namespace policarpo {

bool is_link(std::string_view query) {
    return query.starts_with("http://") || query.starts_with("https://");
}

bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

std::string format_duration(std::chrono::milliseconds ms) {
    using namespace std::chrono;
    auto secs = duration_cast<seconds>(ms);
    ms -= duration_cast<milliseconds>(secs);
    auto mins = duration_cast<minutes>(secs);
    secs -= duration_cast<seconds>(mins);
    auto hrs = duration_cast<hours>(mins);
    

    std::stringstream ss;

    if (hrs.count() > 0) {
        mins -= duration_cast<minutes>(hrs);
        ss << hrs.count() << ":";
        if (mins.count() < 10)
            ss << "0";
    }

    ss << mins.count() << ":";
    if (secs.count() < 10)
        ss << "0";
    ss << secs.count();
    return ss.str();
}

std::string get_file_extension(std::string_view filename) {
    return std::filesystem::path(filename).extension().string();
}

std::string run_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

bool reencode_to_opus(std::string_view input_file, std::string& output_file) {
    output_file = std::string(input_file) + ".converted.opus";
    std::string command = "ffmpeg -i \"" + std::string(input_file) + "\" -c:a libopus \"" + output_file + "\" -y";
    int result = std::system(command.c_str());
    return result == 0;
}

std::string download_opus_track(std::string_view url) {
  std::string current_dir = std::filesystem::current_path().string();
  std::string download_dir = current_dir + "/songs";
  
  // Check if it's a YouTube URL
  std::string url_str(url);
  if (url_str.find("youtube.com") == std::string::npos && 
      url_str.find("youtu.be") == std::string::npos) {
      std::cerr << "[Song Manager] Error: Only YouTube URLs are supported." << std::endl;
      return "";
  }
  
  // First, get video info to check duration and if it's a livestream
  std::string info_command = "yt-dlp --print duration --print is_live --no-warnings " + url_str + " 2>/dev/null";
  std::string info_output = run_command(info_command);
  
  std::istringstream info_stream(info_output);
  std::string line, duration_line, is_live_line;
  bool found_duration = false, found_is_live = false;
  
  // Skip warning lines and find the actual data
  while (std::getline(info_stream, line)) {
    if (!found_duration && !line.empty() && line.find("WARNING") == std::string::npos && line.find("ERROR") == std::string::npos) {
      duration_line = line;
      found_duration = true;
    } else if (found_duration && !found_is_live && !line.empty() && line.find("WARNING") == std::string::npos && line.find("ERROR") == std::string::npos) {
      is_live_line = line;
      found_is_live = true;
      break;
    }
  }
  
  // Check if it's a livestream
  if (is_live_line == "True" || is_live_line == "true") {
      std::cerr << "[Song Manager] Error: Livestreams are not supported." << std::endl;
      return "";
  }
  
  // Check duration (yt-dlp returns duration in seconds)
  try {
      if (!duration_line.empty() && duration_line != "NA") {
          double duration_seconds = std::stod(duration_line);
          if (duration_seconds > 9000) { // 2.5 hours = 9000 seconds
              std::cerr << "[Song Manager] Error: Track is longer than 2.5 hours (" << format_duration(std::chrono::milliseconds(static_cast<long long>(duration_seconds * 1000))) << ")." << std::endl;
              return "";
          }
      }
  } catch (const std::exception& e) {
      std::cerr << "[Song Manager] Warning: Could not parse duration: " << duration_line << std::endl;
  }
  
  std::string command = "yt-dlp -f bestaudio --extract-audio --audio-format opus "
                        "--no-playlist --print after_move:filename "
                        "--output \"" + download_dir + "/%(title)s.%(ext)s\""
                        " " + std::string(url) + " 2>&1";

  std::string output = run_command(command);
  // Check if the command was successful
  if (output.empty()) {
      std::cerr << "[Song Manager] Error: Command failed or returned no output." << std::endl;
      return "";
  }
  // Check if the output contains an error message
  if (output.find("ERROR") != std::string::npos) {
      std::cerr << "[Song Manager] Error in download opus track: " << output << std::endl;
      //return "";
  }

  // Clean trailing whitespace (important!)
  // Remove all lines except the last non-empty one (assumed to be the filename)
  std::istringstream iss(output);
  std::string lines, last_nonempty;
  while (std::getline(iss, lines)) {
      if (!lines.empty() && lines.find_first_not_of(" \n\r\t") != std::string::npos) {
          last_nonempty = lines;
      }
  }
  output = last_nonempty;
  output.erase(output.find_last_not_of(" \n\r\t") + 1);

  return output;  // This is the downloaded .opus filename
}

std::chrono::milliseconds get_audio_duration_ms(std::string_view filepath) {
    std::string cmd = "ffprobe -v error -show_entries format=duration "
                      "-of default=noprint_wrappers=1:nokey=1 \"" + std::string(filepath) + "\"";
    std::string output = run_command(cmd);
    double seconds = std::stod(output);
    return std::chrono::milliseconds(static_cast<long long>(seconds * 1000));
}

bool is_track_available(const policarpo::Song& track) {
    std::string filepath = "songs/" + track.id + ".opus";
    return file_exists(filepath);
}

bool is_track_downloaded(std::string_view url) {
  std::string id = extract_youtube_id_from_watch_url(url);
  if (id.empty()) return false;

  std::string filepath = "songs/" + id + ".opus";
  return file_exists(filepath);
}

std::optional<Song> download_url_track(std::string_view url) {
    std::string filename = download_opus_track(url);
    if (filename.empty()) {
        std::cerr << "[Song Manager] Error: Failed to download track." << std::endl;
        return {};
    }

    std::cout << "[Song Manager] Downloaded file: " << filename << " ]" << "\n";
    std::string id = std::string(url).substr(std::string(url).find("?v=") + 3, std::string(url).find_first_of("&", std::string(url).find("?v=") + 3) - (std::string(url).find("?v=") + 3));

    std::filesystem::path opus_file(filename);
    opus_file.replace_extension(".opus");

    if (!file_exists(opus_file)) {
        std::cerr << "[Song Manager] Error: .opus file not found: " << opus_file << std::endl;
        return {};
    }

    // Rename file to id.opus
    std::filesystem::path new_filename = opus_file.parent_path() / (id + ".opus");
    try {
        std::filesystem::rename(opus_file, new_filename);
        std::cout << "[Song Manager] Renamed file to: " << new_filename << " ]" << "\n";
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[Song Manager] Error renaming file: " << e.what() << std::endl;
        return {};
    }

    std::chrono::milliseconds duration = get_audio_duration_ms(new_filename.string());
    
    // Keep original filename as title (without extension)
    std::string title = std::filesystem::path(filename).stem().string();
    
    return Song { id, title, duration };
}

nlohmann::json get_youtube_track_info(const std::string_view query) {
    std::vector<yt_search::YTrack> res;

    yt_search::YSearchResult data = yt_search::search(std::string(query));
    res = data.trackResults();
    if (res.empty()) {
        std::cerr << "[Song Manager] No results found for the query: " << query << std::endl;
        return nlohmann::json();
    }

    nlohmann::json result = nlohmann::json::object({
        {"title", res[0].title()},
        {"url", res[0].url()},
        {"length", res[0].length()}
    });

    return result;

}

// Minimal ID extractor. not perfect for youtu.be/shorts
std::string extract_youtube_id_from_watch_url(std::string_view url) {
  std::string s(url);

  auto vpos = s.find("?v=");
  if (vpos == std::string::npos) return {};

  vpos += 3;
  auto amp = s.find_first_of("&", vpos);
  if (amp == std::string::npos) return s.substr(vpos);
  return s.substr(vpos, amp - vpos);
}

// Cached load using index (title+duration)
std::optional<policarpo::Song> load_cached_song_by_id(const std::string& id) {
  const std::filesystem::path opus_file = "songs/" + id + ".opus";
  if (!file_exists(opus_file)) return std::nullopt;

  auto meta = policarpo::track_cache_get(id);

  std::string title;
  std::chrono::milliseconds duration{0};

  if (meta) {
    title = meta->title;
    duration = meta->duration;
  }

  // Refresh missing fields if needed
  if (title.empty()) title = id;
  if (duration.count() == 0) duration = get_audio_duration_ms(opus_file.string());

  // If index was missing or incomplete, persist what we now know
  if (!meta || meta->title.empty() || meta->duration.count() == 0) {
    policarpo::track_cache_upsert(id, {title, duration});
  }

  return policarpo::Song{id, title, duration};
}

void get_track(const std::string_view search_query, std::function<void(std::optional<policarpo::Song>)> callback) {

  std::optional<Song> track;

  if (is_link(search_query)) {
    std::cout << "[Song Manager] Skipping for link: " << search_query << "\n";

    if (is_track_downloaded(search_query)) {
      std::cout << "[Song Manager] Track already downloaded.\n";

      std::string id = extract_youtube_id_from_watch_url(search_query);
      if (!id.empty()) {
        track = load_cached_song_by_id(id);
        if (track) {
          std::cout << "[Song Manager] Adding " << track->title << " ]\n";
        } else {
          std::cerr << "[Song Manager] Error: cached .opus exists check failed for id=" << id << "\n";
        }
      }
    } else {
      std::cout << "[Song Manager] Downloading track from URL.\n";
      track = download_url_track(search_query);

      if (track) {
        // Make sure it’s indexed for future cached loads
        policarpo::track_cache_upsert(track->id, {track->title, track->duration});
        std::cout << "[Song Manager] Adding " << track->title << " ]\n";
      } else {
        std::cerr << "[Song Manager] Error: Failed to download track from URL.\n";
      }
    }

  } else {
    std::cout << "[Song Manager] Searching for query: " << search_query << "\n";

    std::string safe_query = std::string(search_query);
    safe_query.erase(std::remove_if(safe_query.begin(), safe_query.end(),
      [](char c) {
        return !(isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' || c == '.' || c == '/');
      }), safe_query.end());

    nlohmann::json track_info = get_youtube_track_info(safe_query);
    if (track_info.empty()) {
      std::cerr << "[Song Manager] Error: Failed to retrieve track info.\n";
      if (callback) callback(std::nullopt);
      return; // IMPORTANT: prevent double-callback
    }

    std::string title = track_info["title"].get<std::string>();
    std::string url   = track_info["url"].get<std::string>();

    std::cout << "[Song Manager] Track info retrieved successfully.\n";
    std::cout << "[Song Manager] Title: " << title << "\n";
    std::cout << "[Song Manager] URL: " << url << "\n";

    std::string id = extract_youtube_id_from_watch_url(url);
    if (id.empty()) {
      std::cerr << "[Song Manager] Error: could not extract id from url.\n";
      if (callback) callback(std::nullopt);
      return;
    }

    if (is_track_downloaded(url)) {
      std::cout << "[Song Manager] Track already downloaded.\n";

      track = load_cached_song_by_id(id);
      if (track) {
        // Optional: if index had id-title placeholder, upgrade it using fresh title from search
        if (!track->title.empty() && track->title == id && !title.empty()) {
          track->title = title;
          policarpo::track_cache_upsert(id, {track->title, track->duration});
        }
        std::cout << "[Song Manager] Adding " << track->title << " ]\n";
      } else {
        std::cerr << "[Song Manager] Error: cached file missing for id=" << id << "\n";
      }

    } else {
      std::cout << "[Song Manager] Downloading track.\n";
      std::string filename = download_opus_track(url);

      if (filename.empty()) {
        std::cerr << "[Song Manager] Error: Failed to download track.\n";
      } else {
        std::cout << "[Song Manager] Downloaded file: " << filename << " ]\n";

        std::filesystem::path opus_file(filename);
        opus_file.replace_extension(".opus");

        if (!file_exists(opus_file)) {
          std::cerr << "[Song Manager] Error: .opus file not found: " << opus_file << "\n";
        } else {
          std::filesystem::path new_filename = opus_file.parent_path() / (id + ".opus");
          try {
            std::filesystem::rename(opus_file, new_filename);
            std::cout << "[Song Manager] Renamed file to: " << new_filename << " ]\n";

            std::chrono::milliseconds duration = get_audio_duration_ms(new_filename.string());

            // Use the title from track_info (more reliable than filename stem)
            track = Song{id, title, duration};

            // Index it so cached loads show the correct title
            policarpo::track_cache_upsert(id, {title, duration});

            std::cout << "[Song Manager] Adding " << track->title << " ]\n";
          } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[Song Manager] Error renaming file: " << e.what() << "\n";
          }
        }
      }
    }
  }

  if (callback) callback(track);
}

} // namespace policarpo