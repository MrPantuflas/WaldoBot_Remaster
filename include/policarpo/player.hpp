#pragma once
#include <dpp/dpp.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <chrono>
#include <oggz/oggz.h>

namespace policarpo {

struct Song {
  std::string id;
  std::string title;
  std::chrono::milliseconds duration{0};
};

enum loop_mode_t {
  LOOP_OFF,
  LOOP_ONCE,
  LOOP_CURRENT,
  LOOP_ALL
};

enum current_state_t {
  WAITING_QUEUE_EMPTY,
  WAITING_QUEUE_NOT_EMPTY,
  CURRENT_PLAYING,
  CURRENT_PAUSED,
  STOPPED_QUEUE_EMPTY,
  STOPPED_QUEUE_NOT_EMPTY,
  FINISHED_QUEUE_EMPTY,
  FINISHED_QUEUE_NOT_EMPTY,
  OTHER
};

class Player {
public:
  Player(dpp::discord_client& shard, const dpp::snowflake& guild_id, const dpp::snowflake& text_channel_id);

  ~Player();

  void enqueue(Song s);
  bool has_queue() const;

  // playback
  bool play(float seconds = 0.0f);             
  bool skip();
  bool pause();
  bool resume();
  bool restart();
  bool voice_ready();
  std::optional<Song> remove_from_queue(size_t index);
  bool jump_to_queue_index(size_t index);
  void mark_finished();     // called from Manager on marker
  void stop_and_clear();    // stop audio + clear queue

  // state
  current_state_t get_state();
  
  // helpers
  dpp::voiceconn* voice() const;
  void update_text_channel(const dpp::snowflake& text_channel_id);
  void update_loop_mode(loop_mode_t mode);
  dpp::snowflake get_text_channel() const { return m_text_channel_id; }
 
public:
  //std::deque<Song> queue;
  dpp::snowflake m_guild_id;
  std::optional<Song> m_current;
  std::size_t m_current_index{0};
  std::vector<Song> m_queue;
  std::atomic<bool> is_finished{false};
  std::atomic<bool> is_stopped{true};
  std::atomic<bool> is_waiting{true};
  std::atomic<bool> is_paused{false};
  std::atomic<bool> is_playing{false};

private:

  void get_next_track();

  loop_mode_t m_loop_mode{LOOP_OFF};

  dpp::discord_client& m_shard;
  dpp::snowflake m_text_channel_id;


  std::condition_variable m_cv;
  mutable std::mutex m_mu;
};

} // namespace policarpo
