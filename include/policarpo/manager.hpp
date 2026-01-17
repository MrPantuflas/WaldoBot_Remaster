#pragma once
#include <dpp/dpp.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include "policarpo/player.hpp"
#include "policarpo/voice_session.hpp"

namespace policarpo {

class Player;

class Manager {
public:
  explicit Manager(dpp::cluster& bot) : m_bot(bot) {}

  // Commands
  void join(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event);
  void play(const dpp::snowflake& guild_id, std::string_view query, const dpp::slashcommand_t& event);
  void skip(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event);
  void pause(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event);
  void stop(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event);
  void queue(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event);
  void leave(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event);
  void remove(const dpp::snowflake& guild_id, size_t index, const dpp::slashcommand_t& event);
  void jump(const dpp::snowflake& guild_id, size_t index, const dpp::slashcommand_t& event);
  void set_loop_mode(const dpp::snowflake& guild_id, const std::string& mode, const dpp::slashcommand_t& event);

  // Events
  void on_voice_track_marker(const dpp::voice_track_marker_t& event);
  void on_voice_client_disconnect(const dpp::voice_client_disconnect_t& event);
  void on_voice_ready(const dpp::voice_ready_t& event);
  void on_voice_state_update(const dpp::voice_state_update_t& event);

private:
  struct track_request_t {
    std::string query;
    std::shared_ptr<policarpo::Player> player;
    std::function<void(std::optional<policarpo::Song>)> callback;
  };

  dpp::cluster& m_bot;
  std::mutex m_mu;
  std::unordered_map<dpp::snowflake, std::shared_ptr<Player>> m_players;

  std::shared_ptr<Player> get_player(const dpp::snowflake& guild_id);
  std::shared_ptr<Player> create_player(dpp::discord_client& shard, const dpp::snowflake& guild_id, const dpp::snowflake& text_channel_id);
  void enqueue(const std::string_view query, std::shared_ptr<policarpo::Player> player, const dpp::slashcommand_t& event, std::function<void(std::optional<policarpo::Song>)> callback);
  void start_next_if_possible(const dpp::snowflake& guild_id);
  void post_update(policarpo::Player const& player, std::string_view content);
  void post_embeded_update(policarpo::Player const& player, const dpp::embed& embed);
};

} // namespace policarpo
