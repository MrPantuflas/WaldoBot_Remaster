#include "policarpo/voice_session.hpp"
#include "policarpo/manager.hpp"
#include "policarpo/player.hpp"
#include "policarpo/song_manager.hpp"

void policarpo::Manager::join(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event) {
    dpp::guild* g = dpp::find_guild(guild_id);
    event.thinking();
    auto player = get_player(guild_id);
    if (player == nullptr) {
        switch (policarpo::check_permissions(g, event.command.usr.id, event.from())) {
            case policarpo::join_result_e::VC_DENIED:
                event.reply(dpp::message("❌ No tengo permiso para unirme a tu canal de voz.").set_flags(dpp::m_ephemeral));
                return;
            case policarpo::join_result_e::NO_VC:
                event.reply(dpp::message("❌ No te veo en un canal.").set_flags(dpp::m_ephemeral));
                return;
            case policarpo::join_result_e::ALREADY_IN_VC:
                event.reply(dpp::message("❌ Manito ya estoy en un canal de voz.").set_flags(dpp::m_ephemeral));
                return;
            case policarpo::join_result_e::JOINABLE:
                break;
        }

        player = create_player(*event.from(), guild_id, event.command.channel_id);
        auto join_result = policarpo::join_voice(m_bot, guild_id, event.command.usr.id, event.from()->shard_id);
        std::cout << "[Manager] Join voice result: " << static_cast<int>(join_result) << " for guild " << guild_id << "\n";
        event.edit_response("Aqui toy!");
    } else {
        event.reply(dpp::message("❌ Manito ya estoy en un canal de voz.").set_flags(dpp::m_ephemeral));
    }
}

void policarpo::Manager::play(const dpp::snowflake& guild_id, std::string_view query, const dpp::slashcommand_t& event) {
    
    dpp::guild* g = dpp::find_guild(guild_id);
    event.thinking();
    auto player = get_player(guild_id);
    if (player == nullptr) {
        if (!query.empty()) {
            switch (policarpo::check_permissions(g, event.command.usr.id, event.from())) {
                case policarpo::join_result_e::VC_DENIED:
                    event.edit_response(dpp::message("❌ No tengo permiso para unirme a tu canal de voz.").set_flags(dpp::m_ephemeral));
                    return;
                case policarpo::join_result_e::NO_VC:
                    event.edit_response(dpp::message("❌ No te veo en un canal.").set_flags(dpp::m_ephemeral));
                    return;
                case policarpo::join_result_e::ALREADY_IN_VC:
                    event.edit_response(dpp::message("❌ Manito ya estoy en un canal de voz.").set_flags(dpp::m_ephemeral));
                    return;
                case policarpo::join_result_e::JOINABLE:
                    break;
            }

            player = create_player(*event.from(), guild_id, event.command.channel_id);
            auto join_result = policarpo::join_voice(m_bot, guild_id, event.command.usr.id, event.from()->shard_id);
            std::cout << "[Manager] Join voice result: " << static_cast<int>(join_result) << " for guild " << guild_id << "\n";
            enqueue(query, player, event, [event, guild_id, this](std::optional<policarpo::Song> track) {
                if (track) {
                    std::cout << "[Manager] Playing: " << track.value().title << " on guild " << guild_id << "\n";
                    event.edit_response("🎶 Poniendo " + track.value().title + " " + format_duration(track.value().duration));
                } else {
                    event.edit_response("❌ No pude encontrar la canción.");
                }
            });
        } else {
            event.edit_response(dpp::message("❌ Cual po?").set_flags(dpp::m_ephemeral));
            return;
        }
    } else if (query.empty()) {
        policarpo::current_state_t state = player->get_state();
        switch (state) {
            case policarpo::current_state_t::WAITING_QUEUE_EMPTY: 
            case policarpo::current_state_t::STOPPED_QUEUE_EMPTY: 
            case policarpo::current_state_t::FINISHED_QUEUE_EMPTY:
                std::cout << "[Manager] State: QUEUE_EMPTY for " << guild_id << "\n";
                event.edit_response(dpp::message("❌ Cual po?").set_flags(dpp::m_ephemeral));
                break;
            case policarpo::current_state_t::CURRENT_PAUSED:
                std::cout << "[Manager] State: CAN_UNPAUSE for " << guild_id << "\n";
                if (player->resume()) {
                    event.edit_response(dpp::message("Reanudado."));
                } else {
                    event.edit_response(dpp::message("❌ Nada que reanudar."));
                }
                break;
            case policarpo::current_state_t::STOPPED_QUEUE_NOT_EMPTY:
            case policarpo::current_state_t::FINISHED_QUEUE_NOT_EMPTY:
                std::cout << "[Manager] State: CAN_RESTART for " << guild_id << "\n";
                if (player->restart()) {
                    event.edit_response(dpp::message(std::string("Reiniciando con ") + player->m_current.value().title + ". " + format_duration(player->m_current.value().duration)));
                } else {
                    event.edit_response(dpp::message("❌ Nada que hacer."));
                }
                break;
            default:
                std::cout << "[Manager] State: OTHER for " << guild_id << "\n";
                event.edit_response(dpp::message("❌ Ya estoy reproduciendo algo."));
                break;
        }
    } else {
        enqueue(query, player, event, [event, guild_id, this](std::optional<policarpo::Song> track) {
            if (track) {
                std::cout << "[Manager] Enqueued: " << track.value().title << " on  guild " << guild_id << "\n";
                event.edit_response("🎶 " + track.value().title + " añadida a la cola. " + format_duration(track.value().duration));
            } else {
                event.edit_response("❌ No pude encontrar la canción.");
            }
        });
    }
}

void policarpo::Manager::skip(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Skipping track in guild: " << guild_id << "\n";
    auto player = get_player(guild_id);
    if (!player) {
        event.reply("❌ No hay nada reproduciéndose.");
        return;
    }

    // skip to next marker (end of current track marker)
    if (player->skip()) {
        event.reply("⏭️ Saltando a " + player->m_current.value().title + ". " + format_duration(player->m_current.value().duration));
        //start_next_if_possible(guild_id);
    } else {
        event.reply("❌ Ahora no queda nah.");
    }
}

void policarpo::Manager::pause(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Pausing track in guild: " << guild_id << "\n";
    auto player = get_player(guild_id);
    if (!player) {
        event.reply("❌ Cuando tenga musica que pausar, pausare.");
        return;
    }

    if (player->get_state() != policarpo::current_state_t::CURRENT_PAUSED) {
        event.reply("❌ Ya estoy en pausa.");
        return;
    }

    if (player->pause()) {
        event.reply("Pausado.");
    } else {
        event.reply("❌ Nada que pausar.");
    }
}

void policarpo::Manager::stop(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event) {
    auto player = get_player(guild_id);
    if (player == nullptr || player->m_queue.empty()) {
        event.reply(dpp::message("❌ Ya estoy detenido."));
        return;
    }
    player->stop_and_clear();
    event.reply(dpp::message("Detenido."));
}

void policarpo::Manager::queue(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Showing queue in guild: " << guild_id << "\n";
    auto player = get_player(guild_id);
    if (player == nullptr || player->m_queue.empty()) {
        event.reply(dpp::message("❌ La cola está vacía."));
        return;
    }

    event.thinking();

    dpp::embed queue_embed = dpp::embed().set_color(dpp::colors::sti_blue).set_title("Lista");
    /*Locura total de calculo*/
    if (player->m_current.has_value()) {
        queue_embed.add_field("🎵 **Rola actual:** ", 
            player->m_current.value().title + " " + 
            format_duration(player->m_current.value().duration - std::chrono::milliseconds(static_cast<int64_t>(player->voice()->voiceclient->get_secs_remaining() * 1000))) + " - " + 
            format_duration(player->m_current.value().duration));
    
    }
    
    queue_embed.add_field("📜 **Lista:**","",false);
    for (int i = 0; i < player->m_queue.size(); i++ ) {
        policarpo::Song song = player->m_queue[i];
        queue_embed.add_field(
            std::to_string(i + 1) + ". " + song.title + " " + format_duration(song.duration),
            "",
            false
        );
    }

    event.edit_response(dpp::message().add_embed(queue_embed));
}

void policarpo::Manager::leave(const dpp::snowflake& guild_id, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Leaving voice in guild: " << guild_id << "\n";
    auto player = get_player(guild_id);
    if (player == nullptr) {
        event.reply(dpp::message("❌ No estoy en un canal de voz.").set_flags(dpp::m_ephemeral));
        return;
    }
    
    {
        std::lock_guard lk(m_mu);
        player->stop_and_clear();
        policarpo::leave_voice(*event.from(), guild_id);
        m_players.erase(guild_id);
    }
    event.reply(dpp::message("Noh Vimoh!"));
}

void policarpo::Manager::remove(const dpp::snowflake& guild_id, size_t index, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Removing track from queue in guild: " << guild_id << " at index: " << index << "\n";
    auto player = get_player(guild_id);
    if (player == nullptr || player->m_queue.empty()) {
        event.reply(dpp::message("❌ La cola está vacía."));
        return;
    }
    if (index == 0 || index > player->m_queue.size() + 1) {
        event.reply(dpp::message("❌ Índice inválido."));
        return;
    }
    policarpo::Song removed_song = player->m_queue[index - 1];
    std::optional<Song> removed = player->remove_from_queue(index);
    if (removed.has_value()) {
        event.reply(dpp::message("🗑️ Removida de la cola: " + removed->title));
        return;
    } else {
        event.reply(dpp::message("❌ No puedo remover la actual."));
        return;
    }
}

void policarpo::Manager::jump(const dpp::snowflake& guild_id, size_t index, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Jumping to track in guild: " << guild_id << " at index: " << index << "\n";
    auto player = get_player(guild_id);
    if (player == nullptr || player->m_queue.empty()) {
        event.reply(dpp::message("❌ La cola está vacía."));
        return;
    }
    if (index == 0 || index > player->m_queue.size() || index == player->m_current_index + 1) {
        event.reply(dpp::message("❌ Índice inválido."));
        return;
    }
    if (player->jump_to_queue_index(index)) {
        event.reply(dpp::message("⏩ Saltando a la pista " + std::to_string(index) + " - " + player->m_current->title + ". " + format_duration(player->m_current->duration)));
        //start_next_if_possible(guild_id);
    } else {
        event.reply(dpp::message("❌ No pude saltar a esa pista."));
    }
}

void policarpo::Manager::enqueue(std::string_view query, std::shared_ptr<policarpo::Player> player, const dpp::slashcommand_t& event, std::function<void(std::optional<policarpo::Song>)> callback) {
    // Heavy work off-thread:
    std::async(std::launch::async, [this, query, player, callback, guild_id = player->m_guild_id]() {
        policarpo::get_track(query, [this, player, guild_id, callback](std::optional<policarpo::Song> track) {
            if (track) {
                auto track_copy = *track;
                player->enqueue(std::move(*track));
                start_next_if_possible(guild_id);
                // Use the copy for callback
                if (callback) {
                    callback(track_copy);
                    return; // Early return to avoid duplicate callback
                }
            } else if (callback) {
                callback(track); // nullptr
            }
        });
    });
}

void policarpo::Manager::set_loop_mode(const dpp::snowflake& guild_id, const std::string& mode, const dpp::slashcommand_t& event) {
    std::cout << "[Manager] Setting loop mode in guild: " << guild_id << " to mode: " << mode << "\n";
    auto player = get_player(guild_id);
    if (player == nullptr) {
        event.reply(dpp::message("❌ No hay nada reproduciéndose."));
        return;
    }

    if (mode == "off") {
        player->update_loop_mode(LOOP_OFF);
        event.reply(dpp::message("Loop desactivado."));
    } else if (mode == "once") {
        player->update_loop_mode(LOOP_ONCE);
        event.reply(dpp::message("Loop una vez activado."));
    } else if (mode == "current") {
        player->update_loop_mode(LOOP_CURRENT);
        event.reply(dpp::message("Loop pista actual activado."));
    } else if (mode == "all") {
        player->update_loop_mode(LOOP_ALL);
        event.reply(dpp::message("Loop toda la lista activado."));
    } else {
        event.reply(dpp::message("❌ Modo de loop inválido."));
    }
}

std::shared_ptr<policarpo::Player> policarpo::Manager::create_player(dpp::discord_client& shard, const dpp::snowflake& guild_id, const dpp::snowflake& text_channel_id) {
    if (!m_players.contains(guild_id)) {
        auto player = std::make_shared<policarpo::Player>(shard, guild_id, text_channel_id);
        m_players[guild_id] = player;
        return player;
    }
    return m_players[guild_id];
}

std::shared_ptr<policarpo::Player> policarpo::Manager::get_player(const dpp::snowflake& guild_id) {
    if (m_players.contains(guild_id)) {
        return m_players[guild_id];
    }
    return nullptr;
}

void policarpo::Manager::on_voice_track_marker(const dpp::voice_track_marker_t& event) {
    if (!event.voice_client) return;

    std::cout << "[Manager] Track marker reached for guild: " << event.voice_client->server_id << "\n";

    const dpp::snowflake guild_id = event.voice_client->server_id;
    auto player = get_player(guild_id);
    if (!player) return;

    player->mark_finished();
    if (player->m_current) {
        if (player->play()) {
            post_update(*player, "🎶 Poniendo: " + player->m_current->title + " " + format_duration(player->m_current->duration));
        } else {
            std::cout << "[Manager] Could not start playback for guild: " << guild_id << "On voice track marker\n";
        }
    } else {
        std::cout << "[Manager] No more tracks in guild: " << guild_id << "On voice track marker\n";
        post_update(*player, "No hay mah!.");
    }
    //start_next_if_possible(guild_id);
}

void policarpo::Manager::on_voice_client_disconnect(const dpp::voice_client_disconnect_t& event) {
    if (!event.voice_client) return;

    const dpp::snowflake guild_id = event.voice_client->server_id;
    std::cout << "[Manager] Voice client disconnected for guild: " << guild_id << ", user: " << event.user_id << "\n";

    // Check if the disconnected user is our bot
    if (event.user_id == m_bot.me.id) {
        std::cout << "[Manager] Bot was disconnected/kicked from guild: " << guild_id << "\n";
        
        auto player = get_player(guild_id);
        if (player) {
            // Send notification message
            post_update(*player, "❌ Me echaron del canal de voz. Na que hacerle.");
            
            // Safely destroy the player
            {
                std::lock_guard<std::mutex> lock(m_mu);
                player->stop_and_clear();  // Stop playback and clear queue
                m_players.erase(guild_id); // Remove from active players
            }
            
            std::cout << "[Manager] Player destroyed for guild: " << guild_id << "\n";
        }
    }
}

void policarpo::Manager::on_voice_ready(const dpp::voice_ready_t& event) {
    if (!event.voice_client) return;

    std::cout << "[Manager] Voice ready for guild: " << event.voice_client->server_id << "\n";

    const dpp::snowflake guild_id = event.voice_client->server_id;
    auto player = get_player(guild_id);
    if (!player) return;

    if (player->has_queue() && player->is_waiting) {
        std::cout << "[Manager] Starting playback after voice ready in guild: " << guild_id << "\n";
        if(!player->play()) {
            std::cout << "[Manager] Could not start playback for guild: " << guild_id << "On voice ready\n";
            return;
        }
     //   post_update(*player, "🎶 Poniendo: " + player->m_current->title + " " + format_duration(player->m_current->duration));
    }
}

void policarpo::Manager::on_voice_state_update(const dpp::voice_state_update_t& event) {
    // Check if this is our bot's voice state update
    if (event.state.user_id == m_bot.me.id) {
        const dpp::snowflake guild_id = event.state.guild_id;
        
        std::cout << "[Manager] Bot voice state update in guild: " << guild_id << ", channel: " << event.state.channel_id << "\n";
        
        // If channel_id is 0, the bot has left the voice channel
        if (event.state.channel_id == 0) {
            std::cout << "[Manager] Bot disconnected from voice channel in guild: " << guild_id << "\n";
            
            auto player = get_player(guild_id);
            if (player) {
                // Send notification message
                post_update(*player, "❌ Me echaron del canal de voz. Na que hacerle.");
                
                // Safely destroy the player
                {
                    std::lock_guard<std::mutex> lock(m_mu);
                    player->stop_and_clear();  // Stop playback and clear queue
                    m_players.erase(guild_id); // Remove from active players
                }
                
                std::cout << "[Manager] Player destroyed for guild: " << guild_id << "\n";
            } else {
                std::cout << "[Manager] No active player found for guild: " << guild_id << " on bot disconnect, probably manually disconnected.\n";
            }
        }
    }
}

void policarpo::Manager::start_next_if_possible(const dpp::snowflake& guild_id) {
    std::cout << "[Manager] Attempting to start next track in guild: " << guild_id << "\n";
    auto player = get_player(guild_id);
    if (!player) return;
    if (player->get_state() == policarpo::current_state_t::CURRENT_PLAYING) return;

    // If there's something queued, start it
    if (player->m_current.has_value()) {
        if (player->play()) {
            std::cout << "[Manager] Started playback for guild: " << guild_id << " - " << player->m_current->title << "In start_next_if_possible\n";
        // post_update(*player, "🎶 Poniendo: " + player->m_current->title + " " + format_duration(player->m_current->duration));
        } else {
            std::cout << "[Manager] Could not start playback for guild: " << guild_id << "In start_next_if_possible\n";
        }
    } else {
        policarpo::current_state_t state = player->get_state();
        switch (state) {
            case policarpo::current_state_t::STOPPED_QUEUE_NOT_EMPTY: 
            case policarpo::current_state_t::FINISHED_QUEUE_NOT_EMPTY:
                // There's something in the queue, try to play it
                if (player->resume()) {
                    std::cout << "[Manager] Resumed playback for guild: " << guild_id << " - " << player->m_current->title << "In start_next_if_possible (resume)\n";
                }
                break;
            default:
                std::cout << "[Manager] No tracks to play in guild: " << guild_id << "In start_next_if_possible\n";
                break;
        }
    }
}

void policarpo::Manager::post_update(policarpo::Player const& player, std::string_view content) {
    m_bot.message_create(dpp::message(player.get_text_channel(), content));
}

void policarpo::Manager::post_embeded_update(policarpo::Player const& player, const dpp::embed& embed) {
    m_bot.message_create(dpp::message(player.get_text_channel(), embed));
}