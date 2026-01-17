#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <string_view>
#include <functional>
#include "policarpo/player.hpp"

namespace policarpo {
    
bool is_link(std::string_view query);

bool file_exists(const std::string& name);

std::string format_duration(std::chrono::milliseconds ms);

std::string get_file_extension(const std::string& filename);

std::string run_command(const std::string& cmd);

bool reencode_to_opus(const std::string& input_file, std::string& output_file);

bool is_track_downloaded(std::string_view url);

bool is_track_available(const policarpo::Song& track);

std::string extract_youtube_id_from_watch_url(std::string_view url);

std::optional<policarpo::Song> load_cached_song_by_id(const std::string& id);

std::string download_opus_track(std::string_view url);

std::optional<Song> download_url_track(std::string_view url);

std::chrono::milliseconds get_audio_duration_ms(std::string_view filepath);

void get_track(const std::string_view search_query, std::function<void(std::optional<policarpo::Song>)> callback);

// void search_track(std::string_view query, std::function<void(std::optional<policarpo::Song>)> callback);

}