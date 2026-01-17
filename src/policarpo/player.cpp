#include "policarpo/player.hpp"

policarpo::Player::Player(dpp::discord_client& shard, const dpp::snowflake& guild_id, const dpp::snowflake& text_channel_id)
    : m_shard(shard), m_guild_id(guild_id), m_text_channel_id(text_channel_id) {
      std::cout << "[Player] Created for guild " << m_guild_id << "\n";
      m_queue.reserve(4);
    }

policarpo::Player::~Player() {}

dpp::voiceconn* policarpo::Player::voice() const {
  dpp::voiceconn* v = m_shard.get_voice(m_guild_id);
  if (!v || !v->voiceclient || !v->voiceclient->is_ready()) return nullptr;
  return v;
}

void policarpo::Player::enqueue(Song s) {
  std::unique_lock lk(m_mu);
  std::cout << "[Player] Enqueue called for guild " << m_guild_id << " - " << s.title << "\n";
  m_queue.push_back(std::move(s));
  if (m_queue.size() == 1 && !m_current.has_value()) {
    lk.unlock();
    get_next_track();
  }
}

bool policarpo::Player::skip() {
  std::unique_lock lk(m_mu);
  std::cout << "[Player] Skip called for guild " << m_guild_id << "\n";
  if (m_queue.empty()) {
    return false;
  }
  if (dpp::voiceconn* v = voice()) {
    if (m_queue.size() > m_current_index) {
      if (m_loop_mode == LOOP_ONCE || m_loop_mode == LOOP_CURRENT) {
        m_loop_mode = LOOP_OFF;
      }
  
      is_playing = false;

      v->voiceclient->skip_to_next_marker();
      lk.unlock();
      get_next_track();
      return play();
    } else {
      // It technically doesn't get here, but oh well...
      std::cout << "[Player] No more tracks to skip to on guild " << m_guild_id << "\n";
      return false;
    }
  } else {
    std::cout << "[Player] Voice connection not ready on skip on guild " << m_guild_id << "\n";
    return false;
  }    
}

bool policarpo::Player::pause() {
  std::lock_guard lk(m_mu);
  std::cout << "[Player] Pause called for guild " << m_guild_id << "\n";
  if (is_paused || is_stopped || is_finished) return false;
  
  if (dpp::voiceconn* v = voice(); v && v->voiceclient && v->voiceclient->is_ready()) {
    v->voiceclient->pause_audio(true);   // :contentReference[oaicite:0]{index=0}
    is_paused = true;
    is_playing = false;
    #ifdef DEBUG_MODE 
      std::cout
      << "e2ee=" << v->voiceclient->is_end_to_end_encrypted()
      << " connected=" << v->voiceclient->is_connected()
      << " paused=" << v->voiceclient->is_paused()
      << " playing=" << v->voiceclient->is_playing()
      << "\n";
    #endif
    return true;
  }
  return false;
}

#ifdef ENABLE_DAVE

bool policarpo::Player::resume() {
  std::lock_guard lk(m_mu);

  if (!is_paused) return false;

  if (dpp::voiceconn* v = voice(); v && v->voiceclient && v->voiceclient->is_ready()) {
    std::cout << "e2ee=" << v->voiceclient->is_end_to_end_encrypted()
    << " connected=" << v->voiceclient->is_connected()
    << " paused=" << v->voiceclient->is_paused()
    << " playing=" << v->voiceclient->is_playing()
    << "\n";
    float seconds = v->voiceclient->get_secs_remaining();
    v->voiceclient->stop_audio();
    is_paused = false;
    play(seconds);
    return true;
  }
  return false;
}

#else 

bool policarpo::Player::resume() {
  std::unique_lock lk(m_mu);

  std::cout << "[Player] Resume called for guild " << m_guild_id << "\n";
  if (!is_paused && !is_stopped && !is_finished) {
    std::cout << "[Player] Nothing to resume for guild " << m_guild_id << "\n";
    return false;
  }
  dpp::voiceconn* v = voice();
  if (v && v->voiceclient && v->voiceclient->is_ready()) {
    if (is_finished) {
      is_finished = false;
      is_stopped = true;
      is_waiting = false;
      lk.unlock();
      get_next_track();
      if (v->voiceclient->is_paused()) {
        v->voiceclient->pause_audio(false);   // :contentReference[oaicite:0]{index=0}
        is_paused = false;
      }
      #ifdef DEBUG_MODE 
        std::cout
        << "e2ee=" << v->voiceclient->is_end_to_end_encrypted()
        << " connected=" << v->voiceclient->is_connected()
        << " paused=" << v->voiceclient->is_paused()
        << " playing=" << v->voiceclient->is_playing()
        << "\n";
      #endif
      return play();
    } else {
      v->voiceclient->pause_audio(false);   // :contentReference[oaicite:0]{index=0}
      is_paused = false;
      is_playing = true;
      #ifdef DEBUG_MODE 
        std::cout
        << "e2ee=" << v->voiceclient->is_end_to_end_encrypted()
        << " connected=" << v->voiceclient->is_connected()
        << " paused=" << v->voiceclient->is_paused()
        << " playing=" << v->voiceclient->is_playing()
        << "\n";
      #endif

      return true;
    }
  } else {
    std::cout << "[Player] Voice connection not ready on resume on guild " << m_guild_id << "\n";
  }
  return false;
}

#endif

bool policarpo::Player::restart() {
  std::unique_lock lk(m_mu);

  std::cout << "[Player] Restart called for guild " << m_guild_id << "\n";

  if (m_queue.empty()) {
    std::cout << "[Player] No tracks to restart for guild " << m_guild_id << "\n";
    return false;
  }

  if (dpp::voiceconn* v = voice(); v && v->voiceclient && v->voiceclient->is_ready()) {
      is_finished = false;
      is_stopped = true;
      is_waiting = false;
      m_current_index = 0;
      lk.unlock();
      get_next_track();
      return play();
  }
  std::cout << "[Player] Voice connection not ready on restart for guild " << m_guild_id << "\n";
  return false;
}

std::optional<policarpo::Song> policarpo::Player::remove_from_queue(size_t index) {
  std::lock_guard lk(m_mu);
  std::cout << "[Player] Remove from queue called for guild " << m_guild_id << " index " << index << "\n";

  index--; // to zero-based

  if (index == m_current_index) {
    return std::nullopt;
  }

  Song s = m_queue[index];
  m_queue.erase(m_queue.begin() + index);
  if (index < m_current_index && m_current_index > 0) {
    m_current_index--;
    std::cout << "[Player] Adjusted current index to " << m_current_index << " for guild " << m_guild_id << "\n";
  }
  return s;
}

bool policarpo::Player::jump_to_queue_index(size_t index) {
  std::unique_lock lk(m_mu);
  std::cout << "[Player] Jump to queue index called for guild " << m_guild_id << " index " << index << "\n";
  if (dpp::voiceconn* v = voice()) {
    m_current_index = index - 1;
    is_waiting = false;
    is_stopped = true;
    is_playing = false;
    v->voiceclient->skip_to_next_marker();
    lk.unlock();
    get_next_track();
    return play();
  } else {
    std::cout << "[Player] Voice connection not ready on jump for guild " << m_guild_id << "\n";
    return false;
  }    
}

bool policarpo::Player::has_queue() const {
  std::lock_guard lk(m_mu);
  return !m_queue.empty();
}

void policarpo::Player::mark_finished() {
  std::unique_lock lk(m_mu);
  std::cout << "[Player] Mark finished called for guild " << m_guild_id << "\n";
  is_playing = false;
  m_current.reset();
  lk.unlock();
  get_next_track();
}

void policarpo::Player::stop_and_clear() {
  std::cout << "[Player] Stop and clear called for guild " << m_guild_id << "\n";
  is_paused = false;
  is_playing = false;
  is_waiting = false;
  is_stopped = true;
  is_finished = false;
  if (dpp::voiceconn* v = voice()) {
    v->voiceclient->stop_audio(); // hard stop
  }
  std::lock_guard lk(m_mu);
  m_queue.clear();
  m_current.reset();
  m_current_index = 0;
}

void policarpo::Player::update_loop_mode(loop_mode_t mode) {
  std::cout << "[Player] Update loop mode called for guild " << m_guild_id << " mode " << static_cast<int>(mode) << "\n";
  std::lock_guard lk(m_mu);
  m_loop_mode = mode;
}

bool policarpo::Player::play(float seconds = 0.0f) {
  std::cout << "[Player] Play called for guild " << m_guild_id << " seconds " << seconds << "\n";
  if (is_playing) return false;

  // Take next song
  //Song song;
  //if (seconds < 0.5f && !is_waiting) {
  //  get_next_track();
  //}

  dpp::voiceconn* v = voice();
  if (!v || !v->voiceclient || !v->voiceclient->is_ready()) {
    std::cout << "[Player] Voice connection not ready on play for guild " << m_guild_id << "\n";
    is_waiting = true;
    is_playing = false;
    return false;
  }

  if (!m_current) {
    std::cout << "[Player] No current track to play for guild " << m_guild_id << "\n";
    is_stopped = true;
    is_playing = false;
    return false;
  }
  #ifdef DEBUG_MODE 
    std::cout
    << "e2ee=" << v->voiceclient->is_end_to_end_encrypted()
    << " connected=" << v->voiceclient->is_connected()
    << " paused=" << v->voiceclient->is_paused()
    << " playing=" << v->voiceclient->is_playing()
    << "\n";
  #endif

  is_playing = true;
  is_waiting = false;
  is_stopped = false;
  is_finished = false;

  OGGZ* og = oggz_open(("songs/" + m_current->id + ".opus").c_str(), OGGZ_READ);
  if (!og) {
    std::cerr << "Error opening: " << m_current->id << "\n";
    is_playing = false;
    std::cout << "[Player] Error opening file for guild " << m_guild_id << " track " << m_current->id << "\n";
  }

  oggz_seek_units(og, seconds * 1000, SEEK_SET);

  oggz_set_read_callback(
    og, -1,
    [](OGGZ*, oggz_packet* packet, long, void* user_data) -> int {
      auto* self = static_cast<Player*>(user_data);

      // Snapshot vc pointer each packet (no global lock held during send)
      dpp::voiceconn* v = self->voice();
      if (v && v->voiceclient) {
        v->voiceclient->send_audio_opus(packet->op.packet, packet->op.bytes);
      }
      return 0;
    },
    this
  );

  // Feed packets into DPP output buffer quickly
  while (v && v->voiceclient && !v->voiceclient->terminating) {
    static constexpr long CHUNK_READ = BUFSIZ * 2;
    long read_bytes = oggz_read(og, CHUNK_READ);
    if (!read_bytes) break; // EOF
  }

  oggz_close(og);

  // Marker = “track boundary”
  // Put something useful in marker metadata (id is best)

  if (v && v->voiceclient) {
    std::cout << "[Player] Inserting marker for guild " << m_guild_id << " track " << m_current->id << "\n";
    v->voiceclient->insert_marker(m_current.value().id);
  }

  std::cout << "[Player] Finished play call for guild " << m_guild_id << " track " << m_current->id << "\n";

  return true;
}

void policarpo::Player::update_text_channel(const dpp::snowflake& text_channel_id) {
  m_text_channel_id = text_channel_id;
}

void policarpo::Player::get_next_track() {
  std::lock_guard lk(m_mu);
  std::cout << "[Player] Get next track called for guild " << m_guild_id << "\n";
  switch (m_loop_mode) {
    case LOOP_OFF:
      if (!is_stopped) {
        m_current_index++;
      }
      if (m_current_index < m_queue.size()) {
        m_current = m_queue[m_current_index];
        std::cout << "[Player] Next track is " << m_current->title << " for guild " << m_guild_id << "\n";
        break;
      } else {
        std::cout << "[Player] No more tracks in queue for guild " << m_guild_id << "\n";
        m_current.reset();
        //m_current_index = 0;
        is_finished = true;
        break;
      }
    case LOOP_ONCE:
      if (!m_current) {
        std::cout << "[Player] LOOP_ONCE but no current track, resetting to current index for guild " << m_guild_id << "\n";
        m_current = m_queue[m_current_index];
      } 
      m_loop_mode = LOOP_OFF;
      std::cout << "[Player] LOOP_ONCE played for guild " << m_guild_id << ", switching to LOOP_OFF\n";
      break;
    case LOOP_CURRENT:
      if (!m_current) {
        std::cout << "[Player] LOOP_CURRENT but no current track, resetting to current index for guild " << m_guild_id << "\n";
        m_current = m_queue[m_current_index];
      }
      break;
    case LOOP_ALL:
      if (m_queue.empty()) {
        std::cout << "[Player] LOOP_ALL but queue is empty for guild " << m_guild_id << "\n";
        m_current.reset();
        is_finished = true;
        //m_current_index = 0;
        break;
      }
      if (!is_stopped) {
        m_current_index++;
      }
      if (m_current_index == m_queue.size()) {
        std::cout << "[Player] LOOP_ALL wrapping to start for guild " << m_guild_id << "\n";
        m_current_index = 0;
        m_current = m_queue[0];
        break;
      } else {
        m_current = m_queue[m_current_index];
        std::cout << "[Player] LOOP_ALL next track is " << m_current->title << " for guild " << m_guild_id << "\n";
        break;
      }
  }
}

policarpo::current_state_t policarpo::Player::get_state() {
  if (m_queue.empty()) {
    if (is_waiting) {
      return current_state_t::WAITING_QUEUE_EMPTY;
    } else if (is_stopped) {
      return current_state_t::STOPPED_QUEUE_EMPTY;
    } else if (is_finished) {
      return current_state_t::FINISHED_QUEUE_EMPTY;
    }
  } else {
    if (is_waiting) {
      return current_state_t::WAITING_QUEUE_NOT_EMPTY;
    } else if (is_playing) {
      return current_state_t::CURRENT_PLAYING;
    } else if (is_paused) {
      return current_state_t::CURRENT_PAUSED;
    } else if (is_stopped) {
      return current_state_t::STOPPED_QUEUE_NOT_EMPTY;
    } else if (is_finished) {
      return current_state_t::FINISHED_QUEUE_NOT_EMPTY;
    }
  }

  return current_state_t::OTHER;
}

bool policarpo::Player::voice_ready() {
  std::unique_lock lk(m_mu);
  std::cout << "[Player] Voice ready check for guild " << m_guild_id << "\n";
  if (is_waiting)  {
    is_waiting = false;
    std::cout << "[Player] Voice is ready, starting play for guild " << m_guild_id << "\n";
    lk.unlock();
    return play();
  } else {
    std::cout << "[Player] Voice is not waiting, no action taken for guild " << m_guild_id << "\n";
    return true;
  }
}
